#ifndef NGPC_WAVE_H
#define NGPC_WAVE_H

/*
 * ngpc_wave -- Enemy wave sequencer (shmup / arcade)
 * ====================================================
 * Reads a ROM wave table and spawns entities at the right frame.
 * The table must be sorted by ascending delay (REQUIRED).
 * A sentinel { 0, 0, 0, 0, WAVE_END } terminates the table.
 *
 * Depends on: ngpc_hw.h only
 *
 * RAM: 9 bytes per NgpcWaveSeq (ptr 4 + timer 2 + next/count/flags 3 ; sizeof 10 under cc900).
 *
 * Usage:
 *   Copy ngpc_wave/ into src/
 *   OBJS += src/ngpc_wave/ngpc_wave.rel
 *   #include "ngpc_wave/ngpc_wave.h"
 *
 * Define the ROM table (const):
 *   static const NgpcWaveEntry wave1[] = {
 *       { ENT_FIGHTER,  19, 4, 0,   0 },   // spawn at frame 0
 *       { ENT_FIGHTER,  19, 8, 0,  60 },   // spawn at frame 60
 *       { ENT_BOMBER,   19, 6, 1, 120 },   // spawn at frame 120
 *       WAVE_SENTINEL                       // end of table
 *   };
 *
 * Use:
 *   static NgpcWaveSeq seq;
 *   ngpc_wave_start(&seq, wave1, WAVE_COUNT(wave1) - 1); // -1: exclude sentinel
 *
 *   // Each frame:
 *   const NgpcWaveEntry *e = ngpc_wave_update(&seq);
 *   while (e) {
 *       spawn_enemy(e->type, e->x, e->y, e->data);
 *       e = ngpc_wave_poll(&seq);      // simultaneous spawns: poll, do NOT call update again
 *   }
 *   if (ngpc_wave_done(&seq)) load_next_wave();
 *
 * Note: ngpc_wave_update() returns a pointer to the entry to spawn, or NULL
 * if nothing is due this frame. For simultaneous spawns (same delay), loop on
 * ngpc_wave_poll() until NULL: each extra update() call would advance the timer.
 */

#include "ngpc_hw.h"

/* ── Wave entry (6 bytes ROM: 4 x u8 + u16) ──────────────────────────── */
typedef struct {
    u8  type;     /* entity type to spawn              */
    u8  x;        /* X position in pixels (or tiles)   */
    u8  y;        /* Y position in pixels (or tiles)   */
    u8  data;     /* behavior / variant (game-defined) */
    u16 delay;    /* spawn frame from wave start       */
} NgpcWaveEntry;

/* End-of-table sentinel */
#define WAVE_END      0xFFFFu
#define WAVE_SENTINEL { 0, 0, 0, 0, WAVE_END }

/* Number of entries INCLUDING the sentinel (pass WAVE_COUNT(arr) - 1 to start) */
#define WAVE_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ── Sequencer (9 bytes, sizeof 10 under cc900) ──────────────────────── */
typedef struct {
    const NgpcWaveEntry *entries; /* ROM table                        */
    u16 timer;                   /* frames elapsed since wave start   */
    u8  next;                    /* index of the next entry           */
    u8  count;                   /* number of entries (no sentinel)   */
    u8  flags;                   /* WAVE_FLAG_*                       */
} NgpcWaveSeq;

#define WAVE_FLAG_ACTIVE  0x01
#define WAVE_FLAG_DONE    0x02

/* ── API ─────────────────────────────────────────────────────────────── */

/*
 * Start reading a wave table.
 *   entries : ROM array, sorted by delay ascending
 *   count   : number of entries WITHOUT the sentinel (use WAVE_COUNT-1)
 */
void ngpc_wave_start(NgpcWaveSeq *seq,
                     const NgpcWaveEntry *entries, u8 count);

/*
 * Stop and reset the sequencer.
 */
void ngpc_wave_stop(NgpcWaveSeq *seq);

/*
 * Update the sequencer -- call ONCE per frame.
 * Returns a pointer to the entry to spawn if a spawn is due,
 * NULL otherwise. Advances the timer by 1 on every call (except the very first,
 * which is frame 0): call it exactly once per frame, then loop on
 * ngpc_wave_poll() for simultaneous spawns (same delay).
 */
const NgpcWaveEntry *ngpc_wave_update(NgpcWaveSeq *seq);

/*
 * Separate tick/poll variant for games that advance the timer elsewhere.
 * ngpc_wave_tick() increments the timer by 1, ngpc_wave_poll() returns
 * spawns without advancing.
 * Do not mix with ngpc_wave_update() on the same sequencer.
 */
void ngpc_wave_tick(NgpcWaveSeq *seq);
const NgpcWaveEntry *ngpc_wave_poll(NgpcWaveSeq *seq);

/* 1 if the wave is complete (all entries have been spawned). */
#define ngpc_wave_done(seq)    ((seq)->flags & WAVE_FLAG_DONE)

/* 1 if the wave is in progress. */
#define ngpc_wave_active(seq)  ((seq)->flags & WAVE_FLAG_ACTIVE)

/* Current frame within the wave. */
#define ngpc_wave_timer(seq)   ((seq)->timer)

#endif /* NGPC_WAVE_H */
