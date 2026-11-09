#include "ngpc_tilecol.h"

/* ══════════════════════════════════════════════════════
 * LECTURE DE MAP
 * ══════════════════════════════════════════════════════ */

u8 ngpc_tilecol_type(const NgpcTileCol *col, s16 tx, s16 ty)
{
    if (tx < 0 || ty < 0 || tx >= (s16)col->map_w || ty >= (s16)col->map_h)
        return TILE_SOLID;   /* hors bornes = solide par convention */
    return col->map[(u16)ty * col->map_w + (u16)tx];
}

u8 ngpc_tilecol_type_at(const NgpcTileCol *col, s16 wx, s16 wy)
{
    /* corrigé campagne 2026-11-08 : >> 3 = division par défaut (aussi < 0) */
    return ngpc_tilecol_type(col, (s16)(wx >> 3), (s16)(wy >> 3));
}

u8 ngpc_tilecol_is_solid(const NgpcTileCol *col, s16 tx, s16 ty)
{
    return ngpc_tilecol_type(col, tx, ty) == TILE_SOLID;
}

/* ══════════════════════════════════════════════════════
 * TESTS DE ZONE
 * ══════════════════════════════════════════════════════ */

u8 ngpc_tilecol_rect_solid(const NgpcTileCol *col,
                            s16 wx, s16 wy, u8 rw, u8 rh)
{
    /* corrigé campagne 2026-11-08 : division par défaut (>> 3) et plus de bornage :
     * hors de la map = TILE_SOLID (convention de ngpc_tilecol_type), sur les 4 bords.
     * Avant : wx/8 arrondi vers 0 + bornes ramenées dans la map -> bords passables. */
    s16 tx0 = (s16)(wx >> 3);
    s16 ty0 = (s16)(wy >> 3);
    s16 tx1 = (s16)((s16)(wx + (s16)rw - 1) >> 3);
    s16 ty1 = (s16)((s16)(wy + (s16)rh - 1) >> 3);
    s16 tx, ty;


    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            if (ngpc_tilecol_type(col, tx, ty) == TILE_SOLID)
                return 1;
        }
    }
    return 0;
}

u8 ngpc_tilecol_rect_ground(const NgpcTileCol *col,
                             s16 wx, s16 wy, u8 rw, u8 rh,
                             s16 prev_bottom)
{
    /* corrigé campagne 2026-11-08 : division par défaut (>> 3) et plus de bornage :
     * hors de la map = TILE_SOLID (convention de ngpc_tilecol_type), sur les 4 bords.
     * Avant : wx/8 arrondi vers 0 + bornes ramenées dans la map -> bords passables. */
    s16 tx0 = (s16)(wx >> 3);
    s16 ty0 = (s16)(wy >> 3);
    s16 tx1 = (s16)((s16)(wx + (s16)rw - 1) >> 3);
    s16 ty1 = (s16)((s16)(wy + (s16)rh - 1) >> 3);
    s16 tx, ty;


    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            {
                u8 t = ngpc_tilecol_type(col, tx, ty);
                if (t == TILE_SOLID) return 1;
                if (t == TILE_ONE_WAY) {
                    /* Collide seulement si on venait d'au-dessus */
                    s16 tile_top = ty * NGPC_TILE_SIZE;
                    if (prev_bottom < tile_top) return 1;
                }
            }
        }
    }
    return 0;
}

/* ══════════════════════════════════════════════════════
 * DÉTECTIONS DIRECTIONNELLES
 * ══════════════════════════════════════════════════════ */

u8 ngpc_tilecol_on_ground(const NgpcTileCol *col,
                           s16 rx, s16 ry, u8 rw, u8 rh)
{
    /* Tester la ligne de 1px directement sous les pieds */
    return ngpc_tilecol_rect_ground(col, rx, ry + (s16)rh, rw, 1,
                                    ry + (s16)rh - 1);
}

u8 ngpc_tilecol_on_ceiling(const NgpcTileCol *col,
                            s16 rx, s16 ry, u8 rw, u8 rh)
{
    return ngpc_tilecol_rect_solid(col, rx, ry - 1, rw, 1);
}

u8 ngpc_tilecol_on_wall_left(const NgpcTileCol *col,
                              s16 rx, s16 ry, u8 rw, u8 rh)
{
    (void)rw;
    return ngpc_tilecol_rect_solid(col, rx - 1, ry, 1, rh);
}

u8 ngpc_tilecol_on_wall_right(const NgpcTileCol *col,
                               s16 rx, s16 ry, u8 rw, u8 rh)
{
    return ngpc_tilecol_rect_solid(col, rx + (s16)rw, ry, 1, rh);
}

s16 ngpc_tilecol_ground_dist(const NgpcTileCol *col,
                              s16 rx, s16 ry, u8 rw, u8 rh,
                              u8 max_dist)
{
    s16 feet = ry + (s16)rh;
    s16 prev_bottom = feet - 1;
    u8  d;
    for (d = 0; d < max_dist; d++) {
        if (ngpc_tilecol_rect_ground(col, rx, feet + (s16)d, rw, 1, prev_bottom))
            return (s16)d;
    }
    return (s16)max_dist;
}

/* ══════════════════════════════════════════════════════
 * HELPER INTERNE : tiles passifs présents dans une zone
 * ══════════════════════════════════════════════════════
 * corrigé campagne 2026-11-08 : renvoie un MASQUE (ZONE_DAMAGE | ZONE_LADDER)
 * au lieu du type de plus grande valeur (DAMAGE + LADDER -> in_damage perdu,
 * type projet 5-15 masquait les deux). */
#define ZONE_DAMAGE 0x01u
#define ZONE_LADDER 0x02u

static u8 zone_passive_mask(const NgpcTileCol *col,
                           s16 wx, s16 wy, u8 rw, u8 rh)
{
    /* corrigé campagne 2026-11-08 : division par défaut (>> 3) et plus de bornage :
     * hors de la map = TILE_SOLID (convention de ngpc_tilecol_type), sur les 4 bords.
     * Avant : wx/8 arrondi vers 0 + bornes ramenées dans la map -> bords passables. */
    s16 tx0 = (s16)(wx >> 3);
    s16 ty0 = (s16)(wy >> 3);
    s16 tx1 = (s16)((s16)(wx + (s16)rw - 1) >> 3);
    s16 ty1 = (s16)((s16)(wy + (s16)rh - 1) >> 3);
    s16 tx, ty;
    u8 m = 0u;


    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            {
                u8 t = ngpc_tilecol_type(col, tx, ty);
                if (t == TILE_DAMAGE)      m |= ZONE_DAMAGE;
                else if (t == TILE_LADDER) m |= ZONE_LADDER;
            }
        }
    }
    return m;
}

/* ══════════════════════════════════════════════════════
 * MOUVEMENT + RÉSOLUTION
 * ══════════════════════════════════════════════════════ */

void ngpc_tilecol_move(const NgpcTileCol *col,
                       s16 *rx, s16 *ry, u8 rw, u8 rh,
                       s16 dx, s16 dy,
                       NgpcMoveResult *result)
{
    s16 x = *rx;
    s16 y = *ry;
    u8  sides  = COL_NONE;
    u8  tile_x = TILE_PASS;
    u8  tile_y = TILE_PASS;

    /* ── Phase 1 : mouvement horizontal ── */
    if (dx != 0) {
        s16 orig_x = x;
        x += dx;

        if (ngpc_tilecol_rect_solid(col, x, y, rw, rh)) {
            /* Recul pixel par pixel jusqu'à position libre */
            if (dx > 0) {
                while (x > orig_x && ngpc_tilecol_rect_solid(col, x, y, rw, rh))
                    x--;
                /* corrigé campagne 2026-11-08 : seul TILE_SOLID bloque en X
                 * (avant : lu au coin haut seulement -> 0 si le bas heurtait). */
                tile_x = TILE_SOLID;
                sides |= COL_RIGHT;
            } else {
                while (x < orig_x && ngpc_tilecol_rect_solid(col, x, y, rw, rh))
                    x++;
                tile_x = TILE_SOLID;
                sides |= COL_LEFT;
            }
        }
    }

    /* ── Phase 2 : mouvement vertical ── */
    if (dy != 0) {
        s16 orig_y = y;
        s16 prev_bottom = orig_y + (s16)rh - 1;
        y += dy;

        /* Choisir la fonction de test selon la direction */
        {
            u8 col_y = (dy > 0)
                ? ngpc_tilecol_rect_ground(col, x, y, rw, rh, prev_bottom)
                : ngpc_tilecol_rect_solid (col, x, y, rw, rh);

            if (col_y) {
                if (dy > 0) {
                    /* Recul vers le haut */
                    while (y > orig_y &&
                           ngpc_tilecol_rect_ground(col, x, y, rw, rh, prev_bottom))
                        y--;
                    /* corrigé campagne 2026-11-08 : toute la largeur, pas le coin gauche */
                    tile_y = ngpc_tilecol_rect_solid(col, x, y + (s16)rh, rw, 1)
                             ? TILE_SOLID : TILE_ONE_WAY;
                    sides |= COL_BOTTOM;
                } else {
                    /* Recul vers le bas (tête dans le plafond) */
                    while (y < orig_y &&
                           ngpc_tilecol_rect_solid(col, x, y, rw, rh))
                        y++;
                    tile_y = TILE_SOLID;   /* seul SOLID bloque vers le haut */
                    sides |= COL_TOP;
                }
            }
        }
    }

    /* ── Phase 3 : tiles passifs (damage, ladder) dans la zone finale ── */
    {
        u8 passive = zone_passive_mask(col, x, y, rw, rh);

        *rx = x;
        *ry = y;

        if (result) {
            result->sides     = sides;
            result->tile_x    = tile_x;
            result->tile_y    = tile_y;
            result->in_ladder = (passive & ZONE_LADDER) ? 1 : 0;
            result->in_damage = (passive & ZONE_DAMAGE) ? 1 : 0;
        }
    }
}
