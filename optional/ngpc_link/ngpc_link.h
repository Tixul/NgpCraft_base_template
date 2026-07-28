#ifndef NGPC_LINK_H
#define NGPC_LINK_H

/*
 * Two-console session over the link cable.
 *
 * Sits on top of the BIOS COM calls (ngpc_link_com.h) and adds: finding the
 * peer, deciding who is host, one packet per frame, checksums, loss detection.
 *
 * THE MODEL: the cable is a byte pipe between two independent consoles, not a
 * shared simulation. Each console runs its own copy of the game and they share
 * nothing but the bytes on the wire. This layer hands you "what the peer sent
 * on its last frame"; what the game does with that is its own business.
 *
 * Never block waiting for the peer. The wire is 19200 bps (~32 bytes per 60 Hz
 * frame) and the other console may be paused, in a menu, or unplugged.
 * ngpc_link_update() never costs more than the frame it is called in.
 *
 * Typical loop:
 *
 *   ngpc_link_init(seed);
 *   while (1) {
 *       vsync();
 *       ngpc_link_out[0] = pad;      * what I send -- fill in BEFORE update
 *       ngpc_link_update();
 *       if (ngpc_link_state == NGPC_LINK_READY) {
 *           u8 peer_pad = ngpc_link_in[0];
 *       }
 *   }
 *
 * ngpc_link_in[] keeps the last value received until another one arrives;
 * ngpc_link_fresh is 1 only on the frame a packet actually landed.
 *
 * WHO IS HOST: not a coin toss. Each console announces how long it has been
 * searching, and the one that has been looking LONGEST plays host -- so opening
 * the link screen first makes you player one, which is what every cable game
 * does and the only rule a player can predict. Resolution is one frame; four
 * random bits (fed by the seed you pass) separate consoles that started within
 * a few frames of each other, and identical draws are simply repeated. No
 * question is ever put to the player.
 *
 * ngpc_link_set_role() is still there for games whose menu already asked
 * ("create" / "join"): it claims the longest or the shortest search time.
 *
 * Settings, defined before the include or with -D:
 *   NGPC_LINK_PAYLOAD        bytes exchanged per frame (1..32, default 4)
 *   NGPC_LINK_SEND_INTERVAL  frames between sends (1 = every frame)
 *   NGPC_LINK_TIMEOUT        silent frames before NGPC_LINK_LOST
 *   NGPC_LINK_HELLO_INTERVAL frames between announcements while searching
 *   NGPC_LINK_RX_QUEUE       receive queue depth in packets (0 = none)
 *
 * A DATA packet costs NGPC_LINK_PAYLOAD + 4 bytes on a wire that carries ~32
 * per frame, so stay under 24 bytes of payload. Both consoles must agree on
 * NGPC_LINK_PAYLOAD, or the session reports NGPC_LINK_MISMATCH instead of
 * quietly shuffling bytes.
 */

#include "ngpc_link/ngpc_link_com.h"

#ifndef NGPC_LINK_PAYLOAD
#define NGPC_LINK_PAYLOAD 4
#endif

#ifndef NGPC_LINK_SEND_INTERVAL
#define NGPC_LINK_SEND_INTERVAL 1
#endif

/* ~2 seconds. */
#ifndef NGPC_LINK_TIMEOUT
#define NGPC_LINK_TIMEOUT 120
#endif

#ifndef NGPC_LINK_HELLO_INTERVAL
#define NGPC_LINK_HELLO_INTERVAL 8
#endif

/* Receive queue depth, in packets.
 *
 * 0 (default) keeps only the LAST packet, in ngpc_link_in[]. That is enough
 * when you exchange state ("here is where I am"), where missing an
 * intermediate packet costs nothing.
 *
 * Use 4 or 8 as soon as the game consumes one packet per simulation step
 * (input lockstep): without a queue, two packets arriving in the same frame
 * overwrite each other and the lost step desynchronises the match silently. */
#ifndef NGPC_LINK_RX_QUEUE
#define NGPC_LINK_RX_QUEUE 0
#endif

/* Wire protocol version. Bump it if the packet format changes. */
#define NGPC_LINK_PROTO 1

/* ---- Session states (ngpc_link_state) ---- */
#define NGPC_LINK_OFF        0  /* ngpc_link_init() not called yet */
#define NGPC_LINK_SEARCHING  1  /* announcing, nobody has answered */
#define NGPC_LINK_READY      2  /* peer found, roles fixed, data flowing */
#define NGPC_LINK_LOST       3  /* nothing received for NGPC_LINK_TIMEOUT frames */
#define NGPC_LINK_MISMATCH   4  /* peer found but different protocol or payload size */

/* ---- Diagnostic counters ---- */
typedef struct {
    u16 tx_bytes;
    u16 rx_bytes;
    u16 tx_packets;
    u16 rx_packets;   /* valid DATA packets received */
    u16 bad_sum;      /* dropped on checksum: noise or desync */
    u8  tx_skipped;   /* sends skipped, TX ring full: the peer is not draining */
    u8  resyncs;      /* number of times a new peer appeared */
    u8  gap;          /* frames since the last valid packet */
    u8  rx_lost;      /* packets dropped, receive queue full */
} NgpcLinkStats;

/* ---- Public state ---- */

extern u8 ngpc_link_state;

/* 1 on exactly one of the two consoles. Meaningful once READY. */
extern u8 ngpc_link_host;

/* 1 during the frame a DATA packet landed. */
extern u8 ngpc_link_fresh;

/* Sequence number of the last packet received; gaps mean lost packets. */
extern u8 ngpc_link_peer_seq;

/* Sent on the next update. Fill it in before calling. */
extern u8 ngpc_link_out[NGPC_LINK_PAYLOAD];

/* Last packet received from the peer. */
extern u8 ngpc_link_in[NGPC_LINK_PAYLOAD];

extern NgpcLinkStats ngpc_link_stats;

/* ---- API ---- */

/* Open the serial channel and start looking for a peer. The seed feeds the
 * role draw: pass something that varies (frame counter, PRNG, clock). */
void ngpc_link_init(u16 seed);

/* Call once per frame after filling ngpc_link_out[]. Drains reception, keeps
 * the session alive, sends one packet. Never blocks. */
void ngpc_link_update(void);

/* Tell the peer we are leaving and go back to NGPC_LINK_OFF. */
void ngpc_link_close(void);

/* Search again without closing the channel. */
void ngpc_link_resync(void);

/* Force the role instead of drawing for it, when the menu already asked. Both
 * consoles must pick opposite roles; if they pick the same one, the draw takes
 * over again. Call after ngpc_link_init(). */
void ngpc_link_set_role(u8 want_host);

#if NGPC_LINK_RX_QUEUE > 0
/* Pop the oldest queued packet into dst (NGPC_LINK_PAYLOAD bytes).
 * Returns 1 if a packet was popped, 0 if the queue is empty. */
u8 ngpc_link_recv(u8 *dst);

u8 ngpc_link_rx_waiting(void);
#endif

/* Cable detected. Advisory: use it to say "plug the cable in", never to gate
 * the exchange. */
#define ngpc_link_cable()  ngpc_com_cable_present()

#define ngpc_link_ready()  (ngpc_link_state == NGPC_LINK_READY)

#endif /* NGPC_LINK_H */
