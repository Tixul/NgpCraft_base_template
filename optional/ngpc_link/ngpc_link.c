/*
 * Two-console session over the link cable.
 *
 * Wire format (both consoles run the same code):
 *
 *     0xA5 | type | seq | body[len(type)] | checksum
 *
 *   type 0x01 HELLO: body = { version, payload size, token hi, token lo,
 *                             flags (bit 0: "I already have a session"),
 *                             echo hi, echo lo -- the last token we heard from
 *                             the peer, 0 if none yet (see link_on_hello) }
 *   type 0x02 DATA : body = ngpc_link_out[NGPC_LINK_PAYLOAD]
 *   type 0x03 BYE  : empty body
 *   checksum = (type + seq + body) XOR 0x5A
 *
 * The 0xA5 magic is how the parser latches back on after noise or a mid-session
 * restart: it goes back to hunting for the magic byte as soon as a checksum
 * fails. One lost byte costs one packet, not the session.
 *
 * Nothing here waits for the peer: ngpc_link_update() reads what arrived,
 * writes what fits in the TX ring, and returns within the frame.
 */

#include "ngpc_link/ngpc_link.h"

/* Compile-time guard: past 32 bytes a packet no longer fits the wire budget
 * of ~32 bytes per frame. */
typedef char ngpc_link_payload_range_check[
    (NGPC_LINK_PAYLOAD >= 1 && NGPC_LINK_PAYLOAD <= 32) ? 1 : -1];

/* Largest body to store: the payload, or the announcement (7 bytes) when the
 * payload is smaller. */
#if NGPC_LINK_PAYLOAD > 7
#define NGPC_LINK_BODY_MAX NGPC_LINK_PAYLOAD
#else
#define NGPC_LINK_BODY_MAX 7
#endif

#define LINK_MAGIC     0xA5u
#define LINK_SUM_XOR   0x5Au

#define LINK_TYPE_HELLO 0x01u
#define LINK_TYPE_DATA  0x02u
#define LINK_TYPE_BYE   0x03u

#define LINK_HELLO_BODY 7u
#define LINK_HEADER     3u   /* magique + type + seq */
#define LINK_FRAME_MAX  (LINK_HEADER + NGPC_LINK_BODY_MAX + 1u)

/* Receive parser states. */
#define RXS_MAGIC 0u
#define RXS_TYPE  1u
#define RXS_SEQ   2u
#define RXS_BODY  3u
#define RXS_SUM   4u

/* Announcements sent when a session opens or changes. Several are needed: the
 * peer must receive OURS, not just us receiving theirs, and it may still be
 * booting. */
#define LINK_HELLO_BURST 8u

/* Drain cap per frame: the BIOS ring holds 64 bytes, and we do not want to
 * spin on a counter that has gone wild. */
#define LINK_DRAIN_MAX 64u

/* ---- Public state ---- */

u8 ngpc_link_state = NGPC_LINK_OFF;
u8 ngpc_link_host = 0;
u8 ngpc_link_fresh = 0;
u8 ngpc_link_peer_seq = 0;
u8 ngpc_link_out[NGPC_LINK_PAYLOAD];
u8 ngpc_link_in[NGPC_LINK_PAYLOAD];
NgpcLinkStats ngpc_link_stats;

/* ---- Private state ---- */

static u16 s_token;        /* our role token */
static u16 s_peer_token;   /* the peer's */
static u8  s_tx_seq;
static u8  s_roll;         /* token draws since init */
static u16 s_search;       /* frames spent searching, feeds the announced token */
static u8  s_search_frozen; /* stop the clock at first contact -- see link_on_hello */
static u8  s_hello_burst;
static u8  s_send_timer;
static u8  s_hello_timer;

static u8  s_rx_state;
static u8  s_rx_type;
static u8  s_rx_seq;
static u8  s_rx_len;
static u8  s_rx_idx;
static u8  s_rx_sum;
static u8  s_rx_body[NGPC_LINK_BODY_MAX];

static u8  s_txbuf[LINK_FRAME_MAX + LINK_HEADER + LINK_HELLO_BODY + 1u];

#if NGPC_LINK_RX_QUEUE > 0
static u8  s_rxq[NGPC_LINK_RX_QUEUE][NGPC_LINK_PAYLOAD];
static u8  s_rxq_head;     /* write index */
static u8  s_rxq_tail;     /* read index */
static u8  s_rxq_count;
#endif

/* ---- Helpers ---- */

static void link_reset_parser(void)
{
    s_rx_state = RXS_MAGIC;
    s_rx_idx = 0;
    s_rx_sum = 0;
}

static void link_roll_token(void)
{
    /* Both consoles run the SAME code, so telling them apart needs a real
     * difference: the seed, the controller state, the traffic already seen and
     * the draw counter. If everything matches, no algorithm can decide -- that
     * is the limit of the problem, not of this code; use ngpc_link_set_role(). */
    s_roll++;
    s_token += (u16)NGPC_COM_JOYPAD * 257u;
    s_token += ngpc_link_stats.rx_bytes;
    s_token += (u16)s_roll * 7u;
    s_token += 1u;
    if (s_token == 0) {
        s_token = 0x1234u;
    }
}

/* The token a console announces.
 *
 * Top 12 bits: FRAMES spent searching, capped at 4095 (about 68 seconds).
 * Bottom 4 bits: the random draw.
 *
 * This is what decides who plays host, and it is deliberately NOT a coin toss:
 * the console that has been looking for a peer LONGEST wins, so opening the
 * link screen first makes you player one -- the rule cable games have always
 * used, and the only one a player can predict.
 *
 * The resolution is one frame on purpose. Counting in seconds looked tidier but
 * left half a second of ambiguity, during which the winner was random again;
 * two players never press start on the same frame, so frames settle it. The
 * four random bits only separate consoles that entered on the very same frame,
 * and if those collide too the draw is repeated. */
static u16 link_token(void)
{
    u16 frames = s_search;

    if (frames > 0x0FFFu) {
        frames = 0x0FFFu;
    }
    return (u16)((frames << 4) | (u16)(s_token & 0x000Fu));
}

/* Build a frame into dst, return its size. */
static u8 link_build(u8 *dst, u8 type, const u8 *body, u8 len)
{
    u8 sum;
    u8 i;

    dst[0] = LINK_MAGIC;
    dst[1] = type;
    dst[2] = s_tx_seq;
    sum = (u8)(type + s_tx_seq);
    s_tx_seq++;

    for (i = 0; i < len; i++) {
        dst[LINK_HEADER + i] = body[i];
        sum = (u8)(sum + body[i]);
    }
    dst[LINK_HEADER + len] = (u8)(sum ^ LINK_SUM_XOR);
    return (u8)(LINK_HEADER + len + 1u);
}

static u8 link_build_hello(u8 *dst)
{
    u8 body[LINK_HELLO_BODY];

    body[0] = NGPC_LINK_PROTO;
    body[1] = NGPC_LINK_PAYLOAD;
    {
        u16 tok = link_token();

        body[2] = (u8)(tok >> 8);
        body[3] = (u8)(tok & 0x00FFu);
    }
    /* Say whether we already have a peer: without this, two established
     * consoles keep answering each other and waste half the wire. */
    body[4] = (u8)((ngpc_link_state == NGPC_LINK_READY) ? 1 : 0);
    /* Echo the last token we heard. This is what tells the peer that we are
     * looking at the same PAIR of numbers it is -- see link_on_hello. */
    body[5] = (u8)(s_peer_token >> 8);
    body[6] = (u8)(s_peer_token & 0x00FFu);
    return link_build(dst, LINK_TYPE_HELLO, body, LINK_HELLO_BODY);
}

/* ---- Receive ---- */

/* ⛔ THE BUG THIS ENDS: "I quite often end up with both sides claiming to be the
 * host (and sometimes both the client)."
 *
 * The old rule was `host = (link_token() > tok)`. It reads OUR token live -- the
 * search counter is still running -- while `tok` is a SNAPSHOT the peer took when
 * it built its HELLO, up to HELLO_INTERVAL + 2 frames earlier. So the two consoles
 * never compared the same pair of numbers: each weighed its own present against
 * the other's past. Write A's start as 0 and B's as delta, with d frames of
 * announcement lag: A concludes `t > t - delta - d`, which is ALWAYS true, and B
 * concludes `t - delta > t - d`, which is true as soon as `d > delta`. Two hosts,
 * and both of them right from where they stood. With d around 10 frames, any two
 * players who opened the link screen within ~170 ms of each other could hit it.
 *
 * Worse, the verdict LATCHED: once READY with an unchanged peer token nothing was
 * recomputed, so the later exchange that would have agreed never ran. And the
 * tie branch fired on consoles that were not tied at all -- a live counter sweeps
 * PAST the peer's stale value on its way up, and each crossing threw away a
 * perfectly good ordering and re-drew.
 *
 * 🔑 THE FIX IS NOT MORE ENTROPY IN THE TOKEN, IT IS COMPARING THE SAME PAIR.
 * Two rules do it:
 *
 *   1. FREEZE the search counter at first contact. From then on our announced
 *      token is a constant, so "what we announced" and "what we compare" cannot
 *      drift apart.
 *   2. Each HELLO ECHOES the last token heard from the peer, and we only decide
 *      when the echo coming back IS our own frozen token. At that instant both
 *      consoles are provably holding the same (mine, theirs), and `>` on the same
 *      pair cannot disagree.
 *
 * ⚖️ WHAT THIS DOES NOT FIX, deliberately. The two freezes do not land at the same
 * millisecond, so a start-time difference under one hello interval can still elect
 * the console that opened the screen second. That is a fair trade: the roles are
 * AGREED, which is what a session needs, and "who was first" was never resolvable
 * to better than the announcement rate anyway. Both-host is a broken session; a
 * role decided eight frames coarsely is a playable one. */
static void link_on_hello(void)
{
    u16 tok;
    u16 echo;
    u16 mine;
    u8 peer_ready;

    if (s_rx_body[0] != NGPC_LINK_PROTO || s_rx_body[1] != NGPC_LINK_PAYLOAD) {
        /* The peer speaks a different dialect: say so rather than quietly
         * shuffling bytes. */
        ngpc_link_state = NGPC_LINK_MISMATCH;
        return;
    }
    if (ngpc_link_state == NGPC_LINK_MISMATCH) {
        ngpc_link_state = NGPC_LINK_SEARCHING;
    }

    tok = (u16)(((u16)s_rx_body[2] << 8) | (u16)s_rx_body[3]);
    peer_ready = (u8)(s_rx_body[4] & 1u);
    echo = (u16)(((u16)s_rx_body[5] << 8) | (u16)s_rx_body[6]);

    /* Rule 1: somebody is out there, so stop the clock. */
    if (!s_search_frozen) {
        s_search_frozen = 1;
        s_hello_burst = LINK_HELLO_BURST;
    }

    if (ngpc_link_state == NGPC_LINK_READY) {
        if (tok != s_peer_token) {
            /* A different console, or the same one restarted. The verdict we
             * hold was about somebody else: go and elect again rather than keep
             * it. */
            ngpc_link_stats.resyncs++;
            s_peer_token = tok;
            ngpc_link_state = NGPC_LINK_SEARCHING;
            s_hello_burst = LINK_HELLO_BURST;
        } else if (!peer_ready && s_hello_burst == 0) {
            /* The peer greets us while the session is already running: it lost
             * track of us, not the other way round. Answer at least once, or a
             * console that went LOST could never come back -- it only sends
             * announcements, we only send data, and neither restarts the other. */
            s_hello_burst = 1;
        }
        return;
    }

    s_peer_token = tok;
    mine = link_token();

    /* Rule 2: decide only on a pair we know they are holding too. */
    if (echo != mine) {
        /* They have not heard our current token yet. Keep announcing -- now with
         * THEIR token echoed back, which is what closes the loop. */
        s_hello_burst = LINK_HELLO_BURST;
        return;
    }

    if (tok == mine) {
        /* A real tie this time, not an artefact of a moving counter: same frozen
         * search time AND same draw. Nothing left to separate them, so re-draw
         * the low nibble and go round again. */
        link_roll_token();
        s_hello_burst = LINK_HELLO_BURST;
        return;
    }

    ngpc_link_host = (u8)((mine > tok) ? 1 : 0);
    ngpc_link_state = NGPC_LINK_READY;
    /* Answer, so the peer reaches the same instant and the same conclusion. */
    s_hello_burst = LINK_HELLO_BURST;
}

static void link_on_data(void)
{
    u8 i;

    if (ngpc_link_state != NGPC_LINK_READY) {
        /* Data before the announcement: the peer is there but roles are not
         * settled. Deliver nothing to the game until they are. */
        return;
    }
    for (i = 0; i < NGPC_LINK_PAYLOAD; i++) {
        ngpc_link_in[i] = s_rx_body[i];
    }
    ngpc_link_peer_seq = s_rx_seq;
    ngpc_link_fresh = 1;
    ngpc_link_stats.rx_packets++;

#if NGPC_LINK_RX_QUEUE > 0
    if (s_rxq_count >= NGPC_LINK_RX_QUEUE) {
        /* Full: the game is not consuming fast enough. Dropping the NEWEST
         * keeps the rest in order, and the counter tells the game it has a hole. */
        ngpc_link_stats.rx_lost++;
    } else {
        for (i = 0; i < NGPC_LINK_PAYLOAD; i++) {
            s_rxq[s_rxq_head][i] = s_rx_body[i];
        }
        s_rxq_head++;
        if (s_rxq_head >= NGPC_LINK_RX_QUEUE) {
            s_rxq_head = 0;
        }
        s_rxq_count++;
    }
#endif
}

static void link_accept(void)
{
    ngpc_link_stats.gap = 0;

    if (s_rx_type == LINK_TYPE_HELLO) {
        link_on_hello();
    } else if (s_rx_type == LINK_TYPE_DATA) {
        link_on_data();
    } else {
        /* BYE: the peer unplugged. */
        s_peer_token = 0;
        ngpc_link_state = NGPC_LINK_LOST;
    }
}

static void link_feed(u8 b)
{
    switch (s_rx_state) {
    case RXS_MAGIC:
        if (b == LINK_MAGIC) {
            s_rx_state = RXS_TYPE;
        }
        break;

    case RXS_TYPE:
        s_rx_type = b;
        s_rx_sum = b;
        if (b == LINK_TYPE_HELLO) {
            s_rx_len = LINK_HELLO_BODY;
        } else if (b == LINK_TYPE_DATA) {
            s_rx_len = NGPC_LINK_PAYLOAD;
        } else if (b == LINK_TYPE_BYE) {
            s_rx_len = 0;
        } else {
            /* Unknown type: that 0xA5 was data, not a frame start. */
            s_rx_state = RXS_MAGIC;
            break;
        }
        s_rx_state = RXS_SEQ;
        break;

    case RXS_SEQ:
        s_rx_seq = b;
        s_rx_sum = (u8)(s_rx_sum + b);
        s_rx_idx = 0;
        s_rx_state = (u8)(s_rx_len ? RXS_BODY : RXS_SUM);
        break;

    case RXS_BODY:
        s_rx_body[s_rx_idx] = b;
        s_rx_idx++;
        s_rx_sum = (u8)(s_rx_sum + b);
        if (s_rx_idx >= s_rx_len) {
            s_rx_state = RXS_SUM;
        }
        break;

    default:
        if (b == (u8)(s_rx_sum ^ LINK_SUM_XOR)) {
            link_accept();
        } else {
            ngpc_link_stats.bad_sum++;
        }
        s_rx_state = RXS_MAGIC;
        break;
    }
}

static void link_drain(void)
{
    u8 n;
    u8 b;

    n = ngpc_com_rx_pending();
    if (n > LINK_DRAIN_MAX) {
        n = LINK_DRAIN_MAX;
    }
    while (n != 0) {
        b = 0;
        ngpc_com_get_data(&b);
        ngpc_link_stats.rx_bytes++;
        link_feed(b);
        n--;
    }
}

/* ---- Send ---- */

static void link_send(void)
{
    u8 n = 0;
    u8 want_data = 0;
    u8 want_hello = 0;

    if (ngpc_link_state == NGPC_LINK_READY) {
        s_send_timer--;
        if (s_send_timer == 0) {
            s_send_timer = NGPC_LINK_SEND_INTERVAL;
            want_data = 1;
            want_hello = (u8)(s_hello_burst ? 1 : 0);
        }
    } else {
        s_hello_timer--;
        /* ⚡ A PENDING BURST ANNOUNCES EVERY FRAME, not once per interval. The
         * rendezvous is now a round trip -- our token out, their echo back -- so
         * at one hello per 8 frames the roles took ~16 frames to settle. Bursting
         * closes it in about 4, and a burst is bounded (8 packets of 11 bytes)
         * so it cannot run away with the wire budget. */
        if (s_hello_timer == 0 || s_hello_burst) {
            s_hello_timer = NGPC_LINK_HELLO_INTERVAL;
            want_hello = 1;
        }
    }

    if (want_hello) {
        n = link_build_hello(s_txbuf);
    }
    if (want_data) {
        n = (u8)(n + link_build(s_txbuf + n, LINK_TYPE_DATA,
                                ngpc_link_out, NGPC_LINK_PAYLOAD));
    }
    if (n == 0) {
        return;
    }

    /* If the TX ring is not draining, the peer is not keeping up: skip this
     * turn instead of trampling the queue. Visible in stats.tx_skipped. */
    if (ngpc_com_tx_free() < n) {
        ngpc_link_stats.tx_skipped++;
        return;
    }

    ngpc_com_send_block(s_txbuf, n);
    ngpc_com_send_start();

    if (want_hello && s_hello_burst) {
        s_hello_burst--;
    }
    ngpc_link_stats.tx_bytes += n;
    ngpc_link_stats.tx_packets++;
}

/* ---- API ---- */

void ngpc_link_init(u16 seed)
{
    u8 i;

    for (i = 0; i < NGPC_LINK_PAYLOAD; i++) {
        ngpc_link_out[i] = 0;
        ngpc_link_in[i] = 0;
    }
    ngpc_link_stats.tx_bytes = 0;
    ngpc_link_stats.rx_bytes = 0;
    ngpc_link_stats.tx_packets = 0;
    ngpc_link_stats.rx_packets = 0;
    ngpc_link_stats.bad_sum = 0;
    ngpc_link_stats.tx_skipped = 0;
    ngpc_link_stats.resyncs = 0;
    ngpc_link_stats.gap = 0;
    ngpc_link_stats.rx_lost = 0;

    ngpc_link_host = 0;
    ngpc_link_fresh = 0;
    ngpc_link_peer_seq = 0;
    s_peer_token = 0;
    s_tx_seq = 0;
    s_roll = 0;
    s_hello_burst = LINK_HELLO_BURST;
    s_send_timer = 1;
    s_hello_timer = 1;
    s_search = 0;
    s_search_frozen = 0;
    link_reset_parser();

#if NGPC_LINK_RX_QUEUE > 0
    s_rxq_head = 0;
    s_rxq_tail = 0;
    s_rxq_count = 0;
#endif

    s_token = (u16)(seed ^ 0xA53Cu);
    link_roll_token();

    /* COMINIT arms both serial vectors, COMRECIVESTART lowers RTS. Both
     * consoles must have called RECIVESTART to hear each other. */
    ngpc_com_init();
    ngpc_com_recv_start();

    ngpc_link_state = NGPC_LINK_SEARCHING;
}

void ngpc_link_update(void)
{
    ngpc_link_fresh = 0;

    if (ngpc_link_state == NGPC_LINK_OFF) {
        return;
    }

    link_drain();

    /* Time spent searching is the primary role criterion, so it only runs while
     * we are actually looking for a peer -- and it STOPS at first contact
     * (s_search_frozen), because a criterion that keeps moving cannot be
     * compared against a value the peer sent a few frames ago. See
     * link_on_hello. */
    if (!s_search_frozen && ngpc_link_state != NGPC_LINK_READY &&
        s_search < 0xFF00u) {
        s_search++;
    }

    if (ngpc_link_stats.gap < 255) {
        ngpc_link_stats.gap++;
    }
    if (ngpc_link_state == NGPC_LINK_READY &&
        ngpc_link_stats.gap > NGPC_LINK_TIMEOUT) {
        /* Silent for too long. Tell the game, but keep announcing: if the peer
         * comes back, the session rebuilds itself. */
        ngpc_link_state = NGPC_LINK_LOST;
        s_peer_token = 0;
        s_hello_timer = 1;
    }

    link_send();
}

void ngpc_link_close(void)
{
    u8 n;

    if (ngpc_link_state == NGPC_LINK_OFF) {
        return;
    }
    n = link_build(s_txbuf, LINK_TYPE_BYE, s_txbuf, 0);
    if (ngpc_com_tx_free() >= n) {
        ngpc_com_send_block(s_txbuf, n);
        ngpc_com_send_start();
    }
    ngpc_link_state = NGPC_LINK_OFF;
}

void ngpc_link_set_role(u8 want_host)
{
    /* Forcing a role is expressed in the same currency as the automatic rule:
     * claim the longest possible search time, or the shortest. If both consoles
     * ask for the same side they fall back to the random byte instead of
     * deadlocking. */
    s_search = want_host ? 0xFF00u : 0u;
    /* A forced role is a FINAL value, so it is frozen like one: nothing must
     * creep it back towards the middle afterwards. Two consoles that both force
     * the same side land on the same search time and are separated by the random
     * nibble instead of deadlocking -- and the echo handshake still runs, so they
     * still agree on which of them got it. */
    s_search_frozen = 1;
    s_token = (u16)(s_token + NGPC_COM_JOYPAD + 1u);
    s_hello_burst = LINK_HELLO_BURST;
    s_hello_timer = 1;
    if (ngpc_link_state == NGPC_LINK_READY) {
        ngpc_link_host = want_host ? 1 : 0;
    }
}

void ngpc_link_resync(void)
{
    if (ngpc_link_state == NGPC_LINK_OFF) {
        return;
    }
    ngpc_link_state = NGPC_LINK_SEARCHING;
    ngpc_link_host = 0;
    ngpc_link_fresh = 0;
    s_peer_token = 0;
    s_search_frozen = 0;      /* looking again, so the clock runs again */
    s_hello_burst = LINK_HELLO_BURST;
    s_hello_timer = 1;
    ngpc_link_stats.gap = 0;
    link_roll_token();
    link_reset_parser();

#if NGPC_LINK_RX_QUEUE > 0
    s_rxq_head = 0;
    s_rxq_tail = 0;
    s_rxq_count = 0;
#endif
}

#if NGPC_LINK_RX_QUEUE > 0

u8 ngpc_link_recv(u8 *dst)
{
    u8 i;

    if (s_rxq_count == 0) {
        return 0;
    }
    for (i = 0; i < NGPC_LINK_PAYLOAD; i++) {
        dst[i] = s_rxq[s_rxq_tail][i];
    }
    s_rxq_tail++;
    if (s_rxq_tail >= NGPC_LINK_RX_QUEUE) {
        s_rxq_tail = 0;
    }
    s_rxq_count--;
    return 1;
}

u8 ngpc_link_rx_waiting(void)
{
    return s_rxq_count;
}

#endif /* NGPC_LINK_RX_QUEUE */
