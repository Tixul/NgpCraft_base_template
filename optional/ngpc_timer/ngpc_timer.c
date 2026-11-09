#include "ngpc_timer.h"

void ngpc_timer_start(NgpcTimer *t, u8 frames) {
    t->period = frames;
    t->count  = frames;
    t->flags  = TIMER_FLAG_ACTIVE;
    if (frames == 0) {
        t->flags = TIMER_FLAG_DONE;
    }
}

void ngpc_timer_start_repeat(NgpcTimer *t, u8 period) {
    /* campagne 2026-11-08 : période 0 = 1 (tire à chaque image) — explicite au lieu
     * d'un compteur à 0 qui ne décomptait pas. */
    if (period == 0) period = 1;
    t->period = period;
    t->count  = period;
    t->flags  = TIMER_FLAG_ACTIVE | TIMER_FLAG_REPEAT;
}

void ngpc_timer_stop(NgpcTimer *t) {
    t->flags &= ~(TIMER_FLAG_ACTIVE | TIMER_FLAG_DONE);
}

void ngpc_timer_restart(NgpcTimer *t) {
    t->count  = t->period;
    t->flags |= TIMER_FLAG_ACTIVE;
    t->flags &= ~TIMER_FLAG_DONE;
}

u8 ngpc_timer_update(NgpcTimer *t) {
    /* Efface le flag DONE du frame précédent */
    t->flags &= ~TIMER_FLAG_DONE;

    if (!(t->flags & TIMER_FLAG_ACTIVE)) return 0;

    if (t->count > 0) t->count--;

    if (t->count == 0) {
        if (t->flags & TIMER_FLAG_REPEAT) {
            t->count = t->period;   /* relance automatique */
        } else {
            t->flags &= ~TIMER_FLAG_ACTIVE;
            t->flags |= TIMER_FLAG_DONE;
        }
        return 1;
    }
    return 0;
}
