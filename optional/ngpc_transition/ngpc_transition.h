#ifndef NGPC_TRANSITION_H
#define NGPC_TRANSITION_H

/*
 * ngpc_transition -- Screen transitions (fade, flash, instant)
 * =============================================================
 * Sequence: fade-out -> load -> fade-in.
 * Manages timing AND the fade-out: the module itself darkens the 48 palettes
 * (SCR1 + SCR2 + SPR, 16 each) one 4-bit step at a time, 15 steps spread over
 * the OUT phase (corrigé campagne 2026-11-08 : avant, palfx recevait pal_id 0xFF
 * = "palette 255", hors RAM palette, et rien ne s'assombrissait).
 *
 * Depends on:
 *   ngpc_room/ngpc_room.h    (timing -- copy into src/)
 *   src/fx/ngpc_palfx.h     (only for the GAME's fade-in, see below)
 *
 * RAM: sizeof(NgpcRoom) + 3 = 7 bytes per NgpcTransition.
 *
 * Usage:
 *   Copy ngpc_transition/ and ngpc_room/ into src/
 *   OBJS += src/ngpc_transition/ngpc_transition.rel
 *   OBJS += src/ngpc_room/ngpc_room.rel
 *   #include "ngpc_transition/ngpc_transition.h"
 *
 * Example -- scene change with fade:
 *   static NgpcTransition tr;
 *   ngpc_transition_init(&tr, 24);          // 24 frames per phase
 *
 *   // Trigger (player touches a door):
 *   ngpc_transition_start(&tr, TRANS_FADE);
 *
 *   // Each frame:
 *   u8 r = ngpc_transition_update(&tr);
 *   if (r == TRANS_LOAD) {
 *       load_scene(next_scene);             // load assets, palettes
 *       ngpc_transition_loaded(&tr);        // start fade-in
 *   }
 *   // During TRANS_IN and TRANS_LOAD: block gameplay
 *   if (ngpc_transition_active(&tr)) { ... }
 *   if (r == TRANS_DONE) gameplay_active = 1;
 *
 * Note on palettes:
 *   TRANS_FADE darkens SCR1 + SCR2 + SPR to black on fade-out.
 *   During TRANS_LOAD, the game loads the new scene (tiles, palettes).
 *   ngpc_transition_loaded() only advances the timing: for the fade-in,
 *   call ngpc_palfx_fade() toward the target palettes AFTER loaded():
 *
 *     if (r == TRANS_LOAD) {
 *         load_scene();
 *         ngpc_transition_loaded(&tr);
 *         // one palfx slot per palette (4 slots max), e.g. palette 0 of SCR1:
 *         ngpc_palfx_fade(GFX_SCR1, 0, scene_pal, _TRANS_SPEED(24)); // fade-in
 *     }
 *
 *   TRANS_FLASH sets the 48 palettes to white at start(), then fades them to
 *   black during OUT (same stepping as TRANS_FADE).
 *   TRANS_INSTANT skips the visual effect (useful for hard cuts).
 */

#include "ngpc_hw.h"
#include "../ngpc_room/ngpc_room.h"
/* No palfx slot is used by the module. */

/* ── Transition types ──────────────────────────────────────────────── */
#define TRANS_FADE    0   /* cross-fade to black              */
#define TRANS_FLASH   1   /* white flash then fade            */
#define TRANS_INSTANT 2   /* no visual effect, timing only    */

/* ── Return values of ngpc_transition_update() ─────────────────────── */
/* Reuses ROOM_* codes for compatibility */
#define TRANS_IDLE  ROOM_IDLE     /* no transition active          */
#define TRANS_OUT   ROOM_FADE_OUT /* exit phase (effect active)    */
#define TRANS_LOAD  ROOM_LOAD     /* load the new scene            */
#define TRANS_IN    ROOM_FADE_IN  /* enter phase (effect active)   */
#define TRANS_DONE  ROOM_DONE     /* complete (1 frame)            */

/* ── Fade speed from phase_frames ──────────────────────────────────── */
/* palfx speed so that a palfx fade (15 steps per 4-bit channel) fits in   */
/* one phase: speed = phase_frames / 15, min 1 (corrigé campagne         */
/* 2026-11-08 : /4 supposait 4 niveaux, un fondu de 24 im. en durait 85).  */
#define _TRANS_SPEED(pf)  ((u8)(((pf) < 15 ? 1 : (pf) / 15)))

/* ── Struct (7 bytes RAM) ───────────────────────────────────────────── */
typedef struct {
    NgpcRoom room;        /* timing state machine (4 bytes)          */
    u8 type;             /* TRANS_FADE / TRANS_FLASH / TRANS_INSTANT */
    u8 phase_frames;     /* duration of each phase                   */
    u8 fade_step;        /* darkening steps already applied (0..15)  */
} NgpcTransition;

/* ── API ────────────────────────────────────────────────────────────── */

/*
 * Initialize the structure.
 *   phase_frames: duration of each phase in frames (e.g. 20).
 *   Recommended value: 16-30.
 */
void ngpc_transition_init(NgpcTransition *tr, u8 phase_frames);

/*
 * Start a transition.
 *   type: TRANS_FADE | TRANS_FLASH | TRANS_INSTANT
 * No effect if a transition is already in progress.
 * FADE: the 48 palettes are darkened by ngpc_transition_update() during OUT.
 * FLASH: the 48 palettes turn white now, then darken the same way.
 */
void ngpc_transition_start(NgpcTransition *tr, u8 type);

/*
 * Signal that loading is complete -- call during TRANS_LOAD.
 * Advances timing to the IN phase.
 * For TRANS_FADE: trigger ngpc_palfx_fade() toward the new scene's
 * palettes AFTER this call (the module does not know them).
 */
void ngpc_transition_loaded(NgpcTransition *tr);

/*
 * Update the transition -- call ONCE per frame.
 * Returns TRANS_IDLE / TRANS_OUT / TRANS_LOAD / TRANS_IN / TRANS_DONE.
 */
u8 ngpc_transition_update(NgpcTransition *tr);

/*
 * 1 if a transition is in progress (OUT, LOAD, or IN).
 * Use to block gameplay + input.
 */
#define ngpc_transition_active(tr) ngpc_room_in_transition(&(tr)->room)

/*
 * Progress of the current phase [0..255].
 * 0 = start of phase, 255 = end.
 * Useful for custom effects without ngpc_palfx:
 *   TRANS_OUT: brightness = 255 - ngpc_transition_progress(tr)
 *   TRANS_IN:  brightness = ngpc_transition_progress(tr)
 */
#define ngpc_transition_progress(tr) ngpc_room_progress(&(tr)->room)

#endif /* NGPC_TRANSITION_H */
