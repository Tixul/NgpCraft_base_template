#include "ngpc_kinematic.h"

/* v * f arrondi vers 0 (symétrique en signe). */
static fx16 kin_damp(fx16 v, fx16 f)
{
    return (v < 0) ? (fx16)-FX_MUL((fx16)-v, f) : FX_MUL(v, f);
}

void ngpc_kinematic_init(NgpcKinematic *k, fx16 x, fx16 y,
                         fx16 friction, fx16 bounce)
{
    k->pos.x   = x;
    k->pos.y   = y;
    k->vel.x   = 0;
    k->vel.y   = 0;
    k->friction = friction;
    k->bounce  = bounce;
    k->flags   = 0;
}

void ngpc_kinematic_apply_gravity(NgpcKinematic *k,
                                  fx16 gravity, fx16 max_fall)
{
    k->vel.y = FX_ADD(k->vel.y, gravity);
    if (k->vel.y > max_fall) k->vel.y = max_fall;
}

void ngpc_kinematic_move(NgpcKinematic *k,
                         const NgpcTileCol *col, u8 w, u8 h)
{
    s16 opx, opy, npx, npy, dx, dy;
    NgpcMoveResult res;

    /* Friction multiplicative sur les deux axes, symétrique.
     * corrigé campagne 2026-11-08 : FX_MUL arrondit vers -inf, une vitesse
     * négative restait bloquée à -1..-7 (objet qui glisse à gauche à vie). */
    k->vel.x = kin_damp(k->vel.x, k->friction);
    k->vel.y = kin_damp(k->vel.y, k->friction);

    /* Position entière avant intégration */
    opx = (s16)FX_TO_INT(k->pos.x);
    opy = (s16)FX_TO_INT(k->pos.y);

    /* Intégration en fx16 (préserve le sous-pixel) */
    k->pos.x = FX_ADD(k->pos.x, k->vel.x);
    k->pos.y = FX_ADD(k->pos.y, k->vel.y);

    /* Déplacement en pixels entiers */
    npx = (s16)FX_TO_INT(k->pos.x);
    npy = (s16)FX_TO_INT(k->pos.y);
    dx  = npx - opx;
    dy  = npy - opy;

    /* Résolution de collision depuis l'ancienne position entière */
    ngpc_tilecol_move(col, &opx, &opy, w, h, dx, dy, &res);

    /* Reconstruire pos fx16 : nouvelle position entière + fraction conservée */
    {
        fx16 fx = (fx16)((u16)(k->pos.x) & 0x0F);
        fx16 fy = (fx16)((u16)(k->pos.y) & 0x0F);
        k->pos.x = (fx16)(((s16)opx << FX_SHIFT) | (s16)fx);
        k->pos.y = (fx16)(((s16)opy << FX_SHIFT) | (s16)fy);
    }

    /* Flags et rebond */
    k->flags = 0;

    if (res.sides & COL_BOTTOM) {
        k->pos.y  = (fx16)((s16)opy << FX_SHIFT);   /* posé au pixel, sans fraction */
        k->flags |= KIN_ON_GROUND;
        k->vel.y = (k->bounce > 0) ? -FX_MUL(k->vel.y, k->bounce) : 0;
    }
    if (res.sides & COL_TOP) {
        k->flags |= KIN_ON_CEIL;
        k->vel.y = (k->bounce > 0) ? -FX_MUL(k->vel.y, k->bounce) : 0;
    }
    if (res.sides & (COL_LEFT | COL_RIGHT)) {
        k->flags |= KIN_ON_WALL;
        k->vel.x = (k->bounce > 0) ? -FX_MUL(k->vel.x, k->bounce) : 0;
    }

    /* corrigé campagne 2026-11-08 : au repos, la gravité sous-pixel donne des
     * frames sans déplacement (dy = 0) -> ON_GROUND clignotait (1 frame sur 3).
     * Sonde 1 px sous les pieds quand on ne monte pas. */
    if (!(k->flags & KIN_ON_GROUND) && k->vel.y >= 0 &&
        ngpc_tilecol_on_ground(col, opx, opy, w, h))
        k->flags |= KIN_ON_GROUND;
}

void ngpc_kinematic_impulse(NgpcKinematic *k, fx16 ix, fx16 iy)
{
    k->vel.x = FX_ADD(k->vel.x, ix);
    k->vel.y = FX_ADD(k->vel.y, iy);
}

void ngpc_kinematic_stop(NgpcKinematic *k)
{
    k->vel.x = 0;
    k->vel.y = 0;
}
