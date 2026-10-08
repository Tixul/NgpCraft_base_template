/* Two consoles running the real ngpc_link.c, wired by a cable whose byte rate
 * and announcement lag the harness controls.
 *
 * The point of measurement: sweep the two things that made the old rule fail --
 * how far apart the players opened the link screen (skew) and how stale a HELLO
 * is by the time it is read (lag) -- and assert that the two consoles always
 * agree on exactly one host. Two real consoles cannot be made to sweep that.
 *
 * The wire is modelled as it really is: 19200 bps 8N1 is one byte per 3200 CPU
 * cycles, i.e. ~32 bytes per 60 Hz frame, and bytes arrive in order.
 */
#include <stdio.h>
#include <string.h>

typedef unsigned char  u8;
typedef unsigned short u16;

#define BYTES_PER_FRAME 32
#define PIPE_MAX 4096

/* ---- the two consoles' COM state ------------------------------------------ */
typedef struct {
    u8  out[PIPE_MAX]; int out_n;      /* handed to the BIOS, not yet on the wire */
    u8  in[PIPE_MAX];  int in_head, in_n;
    u8  pad;
} Com;

static Com com[2];
static int cur;                         /* which console is running right now */

u8   sim_joypad(void)    { return com[cur].pad; }
u8   sim_tx_count(void)  { int n = com[cur].out_n; return (u8)(n > 64 ? 64 : n); }
u8   sim_rx_count(void)  { int n = com[cur].in_n;  return (u8)(n > 255 ? 255 : n); }

void sim_send_block(const u8 *p, u8 n)
{
    Com *c = &com[cur];
    int i;
    for (i = 0; i < n && c->out_n < PIPE_MAX; i++) c->out[c->out_n++] = p[i];
}

void sim_get_data(u8 *out)
{
    Com *c = &com[cur];
    if (c->in_n <= 0) { *out = 0; return; }
    *out = c->in[c->in_head++];
    c->in_n--;
    if (c->in_head > PIPE_MAX / 2) {    /* compact */
        memmove(c->in, c->in + c->in_head, (size_t)c->in_n);
        c->in_head = 0;
    }
}

/* ---- the cable: bytes in flight, each with the frame it lands on ---------- */
typedef struct { int at; u8 b; int to; } Inflight;
static Inflight fly[PIPE_MAX];
static int fly_n;

/* ---- the two module instances -------------------------------------------- */
extern u8 A_link_state, A_link_host, A_link_out[], A_link_in[];
extern u8 B_link_state, B_link_host, B_link_out[], B_link_in[];
void A_link_init(u16 seed); void A_link_update(void);
void B_link_init(u16 seed); void B_link_update(void);

/* Only what this harness reads. Layout from NgpcLinkStats in ngpc_link.h. */
extern struct { u16 tx_bytes, rx_bytes; u8 tx_packets, rx_packets, bad_sum,
                tx_skipped, resyncs, gap, rx_lost; } A_link_stats, B_link_stats;

/* ⚠️ A burst announces every frame now, so the wire budget is worth watching:
 * `tx_skipped` is the module refusing to send because the 64-byte BIOS ring has
 * not drained. Zero of those is the claim; the deepest the ring ever got is the
 * margin behind it. */
static int worst_ring;
static int worst_skipped;

#define ST_SEARCHING 1
#define ST_READY     2

static void reset_wire(void)
{
    memset(com, 0, sizeof com);
    fly_n = 0;
}

static void pump(int frame, int lag)
{
    int side, i, k;
    for (side = 0; side < 2; side++) {           /* wire takes what it can */
        Com *c = &com[side];
        int take = c->out_n < BYTES_PER_FRAME ? c->out_n : BYTES_PER_FRAME;
        if (c->out_n > worst_ring) worst_ring = c->out_n;
        for (i = 0; i < take; i++) {
            if (fly_n < PIPE_MAX) {
                fly[fly_n].at = frame + lag;
                fly[fly_n].b  = c->out[i];
                fly[fly_n].to = 1 - side;
                fly_n++;
            }
        }
        memmove(c->out, c->out + take, (size_t)(c->out_n - take));
        c->out_n -= take;
    }
    for (i = 0, k = 0; i < fly_n; i++) {         /* deliver what has landed */
        if (fly[i].at <= frame) {
            Com *d = &com[fly[i].to];
            if (d->in_head + d->in_n < PIPE_MAX) d->in[d->in_head + d->in_n++] = fly[i].b;
        } else {
            fly[k++] = fly[i];
        }
    }
    fly_n = k;
}

/* One run. Returns 0 = agreed on one host, 1 = both host, 2 = both client,
 * 3 = never settled. `settled_in` gets the frame both reached READY. */
static int run(int skew, int lag, u16 seed_a, u16 seed_b, int *settled_in)
{
    int f;
    reset_wire();
    com[0].pad = 0x00;
    com[1].pad = 0x00;
    cur = 0; A_link_init(seed_a);

    for (f = 0; f < 600; f++) {
        if (f == skew) { cur = 1; B_link_init(seed_b); }
        cur = 0; A_link_update();
        if (f >= skew) { cur = 1; B_link_update(); }
        pump(f, lag);
        if (A_link_stats.tx_skipped > worst_skipped)
            worst_skipped = A_link_stats.tx_skipped;
        if (B_link_stats.tx_skipped > worst_skipped)
            worst_skipped = B_link_stats.tx_skipped;
        if (A_link_state == ST_READY && B_link_state == ST_READY) {
            *settled_in = f;
            if (A_link_host && B_link_host) return 1;
            if (!A_link_host && !B_link_host) return 2;
            return 0;
        }
    }
    *settled_in = -1;
    return 3;
}

int main(void)
{
    int skew, lag, bad = 0, worst = 0, runs = 0;
    const char *why[] = {"ok", "BOTH HOST", "BOTH CLIENT", "NEVER SETTLED"};

    printf("      lag: 1 2 3 4 5 6 7 8 9 . . .   (. = agreed, X = disagreed)\n");
    for (skew = 0; skew <= 12; skew++) {
        printf("skew %2d:   ", skew);
        for (lag = 1; lag <= 12; lag++) {
            int in = -1, seed, here = 0;
            for (seed = 0; seed < 4; seed++) {
                int r = run(skew, lag, (u16)(0x1111 * (seed + 1)),
                            (u16)(0x2222 * (seed + 1) + 7), &in);
                runs++;
                if (r != 0) {
                    if (bad < 4)
                        printf("\n  skew=%d lag=%d seed=%d -> %s\n  ",
                               skew, lag, seed, why[r]);
                    bad++; here++;
                } else if (in > worst) {
                    worst = in;
                }
            }
            printf("%c ", here ? 'X' : '.');
        }
        printf("\n");
    }
    printf("%d runs, %d disagreements, slowest agreement %d frames\n",
           runs, bad, worst);
    printf("wire: deepest TX ring %d/64 bytes, tx_skipped %d\n",
           worst_ring, worst_skipped);
    if (worst_skipped) {
        printf("  ERROR: the announcements do not fit the wire budget\n");
        bad++;
    }
    return bad ? 1 : 0;
}
