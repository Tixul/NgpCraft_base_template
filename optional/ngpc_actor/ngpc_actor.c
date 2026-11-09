#include "ngpc_actor.h"

/* Bit interne : ngpc_actor_move() appelé ce frame (décide de la friction). */
#define _ACTOR_INPUT  0x02

/* corrigé campagne 2026-11-08 : FX_MUL arrondit vers -inf -> une vitesse négative ne
 * retombait jamais à 0 (bloquée à -3/16) et l'accel diagonale était asymétrique.
 * On multiplie la valeur absolue puis on remet le signe. */
static fx16 _actor_mul_sym(fx16 v, fx16 k) {
    return (v < 0) ? (fx16)(-FX_MUL((fx16)(-v), k)) : FX_MUL(v, k);
}

void ngpc_actor_init(NgpcActor *a, fx16 x, fx16 y,
                     fx16 speed, fx16 accel, fx16 friction) {
    a->pos.x    = x;
    a->pos.y    = y;
    a->vel.x    = 0;
    a->vel.y    = 0;
    a->speed    = speed;
    a->accel    = accel;
    a->friction = friction;
    a->dir_x    = 0;
    a->dir_y    = 0;
    a->flags    = 0;
}

void ngpc_actor_move(NgpcActor *a, s8 dx, s8 dy) {
    fx16 ax, ay, lim;

    if (!dx && !dy) return;

    /* Accélération dans la direction demandée */
    ax = dx ? (dx > 0 ? a->accel : -a->accel) : 0;
    ay = dy ? (dy > 0 ? a->accel : -a->accel) : 0;

    /* Normalisation diagonale : ×0.6875 ≈ 1/√2 pour éviter le boost diagonal */
    if (dx && dy) {
        ax = _actor_mul_sym(ax, _ACTOR_DIAG_NORM);
        ay = _actor_mul_sym(ay, _ACTOR_DIAG_NORM);
    }

    a->vel.x = FX_ADD(a->vel.x, ax);
    a->vel.y = FX_ADD(a->vel.y, ay);

    /* Clamp vitesse max — corrigé campagne 2026-11-08 : en diagonale chaque axe est
     * plafonné à speed×0.6875 (sinon vitesse diagonale = speed×1.41). */
    lim = (dx && dy) ? _actor_mul_sym(a->speed, _ACTOR_DIAG_NORM) : a->speed;
    if (a->vel.x >  lim) a->vel.x =  lim;
    if (a->vel.x < -lim) a->vel.x = -lim;
    if (a->vel.y >  lim) a->vel.y =  lim;
    if (a->vel.y < -lim) a->vel.y = -lim;

    /* Mémoriser la direction pour les sprites/animations */
    if (dx) a->dir_x = dx;
    if (dy) a->dir_y = dy;

    a->flags |= (u8)(_ACTOR_INPUT | ACTOR_MOVING);
}

void ngpc_actor_update(NgpcActor *a) {
    /* Friction quand aucune direction n'est appliquée ce frame */
    if (!(a->flags & _ACTOR_INPUT)) {
        a->vel.x = _actor_mul_sym(a->vel.x, a->friction);
        a->vel.y = _actor_mul_sym(a->vel.y, a->friction);
    }
    /* corrigé campagne 2026-11-08 : ACTOR_MOVING reste lisible APRÈS update (= move() appelé
     * ce frame) ; le bit interne est remis à 0 pour le frame suivant. */
    a->flags = (u8)((a->flags & ~(ACTOR_MOVING | _ACTOR_INPUT))
                    | ((a->flags & _ACTOR_INPUT) ? ACTOR_MOVING : 0));

    /* Intégration position */
    a->pos.x = FX_ADD(a->pos.x, a->vel.x);
    a->pos.y = FX_ADD(a->pos.y, a->vel.y);
}

void ngpc_actor_stop(NgpcActor *a) {
    a->vel.x = 0;
    a->vel.y = 0;
    a->flags &= ~(ACTOR_MOVING | _ACTOR_INPUT);
}
