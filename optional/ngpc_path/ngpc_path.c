#include "ngpc_path.h"

/*
 * BFS de la CIBLE vers la SOURCE (flood-fill depuis la cible).
 * Remplit dist[] : dist[y*map_w+x] = distance depuis la cible (0 = cible).
 * 0xFF = non-visité ou solide.
 *
 * Avantage : avec dist[], on peut trouver le premier pas depuis n'importe
 * quelle source sans refaire le BFS.
 */

#define _DIST_INF  0xFF
/* corrigé campagne 2026-11-08 : file de 256 (chaque case n'y entre qu'une fois -> jamais pleine),
 * plus de coupure à 63 pas, ré-init correcte d'une grille 16×16 (n = 256), voisins par index. */
#define _QUEUE_CAP (PATH_MAX_W * PATH_MAX_H)

/* Encode une position (x, y) en 1 octet : (y << 4) | x (x,y < 16) */
#define _ENC(x, y)   ((u8)(((u8)(y) << 4) | (u8)(x)))
#define _DEC_X(v)    ((u8)((v) & 0x0F))
#define _DEC_Y(v)    ((u8)((v) >> 4))

/* Directions 4-connexité */
static const s8 _dx[4] = { 0,  0, -1, 1 };
static const s8 _dy[4] = {-1,  1,  0, 0 };

/* File BFS statique (index u8 : rebouclage naturel à 256 = capacité). */
static u8 s_queue[_QUEUE_CAP];
/* Table de distances partagée par step / dist / build (aucune n'est réentrante). */
static u8 s_dist[PATH_MAX_W * PATH_MAX_H];

/* Visite le voisin d'index k / position (x, y) : distance nd s'il est libre et non vu. */
#define _VISIT(k, x, y)     do { if (map[(k)] == 0 && dist_buf[(k)] == _DIST_INF) {         dist_buf[(k)] = nd; s_queue[qtail++] = _ENC((x), (y)); qsize++; } } while (0)

/*
 * Effectue le BFS depuis (tx, ty).
 * Remplit dist_buf[0..map_w*map_h-1] avec les distances (0xFF si non-atteint).
 */
static void _bfs(const u8 *map, u8 map_w, u8 map_h,
                 u8 tx, u8 ty,
                 u8 *dist_buf)
{
    u8  qhead = 0, qtail = 0;
    u16 qsize, k, ci;
    u16 n = (u16)map_w * map_h;
    u8  cur, cx, cy, nd;

    /* Init : tout à infini (u16 : n vaut 256 pour une grille 16×16) */
    for (k = 0; k < n; k++) dist_buf[k] = _DIST_INF;

    if (map_w > PATH_MAX_W || map_h > PATH_MAX_H) return;
    if (tx >= map_w || ty >= map_h) return;
    ci = (u16)ty * map_w + tx;
    if (map[ci] != 0) return; /* cible solide */

    dist_buf[ci] = 0;
    s_queue[qtail++] = _ENC(tx, ty);
    qsize = 1;

    while (qsize > 0) {
        cur = s_queue[qhead++];
        qsize--;
        cx  = _DEC_X(cur);
        cy  = _DEC_Y(cur);
        ci  = (u16)cy * map_w + cx;
        nd  = (u8)(dist_buf[ci] + 1u);

        if (cy > 0)                  { k = (u16)(ci - map_w); _VISIT(k, cx, cy - 1u); }
        if ((u8)(cy + 1u) < map_h)   { k = (u16)(ci + map_w); _VISIT(k, cx, cy + 1u); }
        if (cx > 0)                  { k = (u16)(ci - 1u);    _VISIT(k, cx - 1u, cy); }
        if ((u8)(cx + 1u) < map_w)   { k = (u16)(ci + 1u);    _VISIT(k, cx + 1u, cy); }
    }
}

u8 ngpc_path_step(const u8 *map, u8 map_w, u8 map_h,
                  u8 sx, u8 sy, u8 tx, u8 ty,
                  s8 *out_dx, s8 *out_dy)
{
    u8 *dist = s_dist;   /* buffer statique partagé (pas de RTOS, pas de réentrance) */
    u8 i, best_dir;
    u8 best_dist;

    *out_dx = 0;
    *out_dy = 0;

    if (sx == tx && sy == ty) return 0;  /* déjà arrivé */
    if (sx >= map_w || sy >= map_h) return 0;  /* source hors grille */

    /* BFS depuis la cible */
    _bfs(map, map_w, map_h, tx, ty, dist);

    /* Source inatteignable ? */
    if (dist[(u16)sy * map_w + sx] == _DIST_INF) return 0;

    /* Choisir le voisin de (sx, sy) avec la distance minimale */
    best_dir  = 4;
    best_dist = _DIST_INF;

    for (i = 0; i < 4; i++) {
        s8 nx = (s8)((s8)sx + _dx[i]);
        s8 ny = (s8)((s8)sy + _dy[i]);
        u16 idx;
        u8  nd;

        if (nx < 0 || ny < 0 || (u8)nx >= map_w || (u8)ny >= map_h) continue;
        idx = (u16)(u8)ny * map_w + (u8)nx;
        if (map[idx] != 0) continue;   /* solide */

        nd = dist[idx];
        if (nd < best_dist) {
            best_dist = nd;
            best_dir  = i;
        }
    }

    if (best_dir == 4) return 0;  /* aucun voisin accessible */

    *out_dx = _dx[best_dir];
    *out_dy = _dy[best_dir];
    return 1;
}

u8 ngpc_path_dist(const u8 *map, u8 map_w, u8 map_h,
                  u8 sx, u8 sy, u8 tx, u8 ty)
{
    if (sx == tx && sy == ty) return 0;
    if (sx >= map_w || sy >= map_h) return PATH_NO_PATH;

    _bfs(map, map_w, map_h, tx, ty, s_dist);

    return s_dist[(u16)sy * map_w + sx];
}

/* ── API « calculer une fois, suivre pas à pas » (ajout campagne 2026-11-08) ──
 * Le chemin est stocké sur 2 bits par pas (0=haut 1=bas 2=gauche 3=droite, même
 * ordre de préférence que ngpc_path_step) dans NgpcPath.steps. */

u8 ngpc_path_build(NgpcPath *p, const u8 *map, u8 map_w, u8 map_h,
                   u8 sx, u8 sy, u8 tx, u8 ty)
{
    u8 k, i, len, cx, cy;
    u16 ci;

    p->len = 0;
    p->pos = 0;
    if (sx == tx && sy == ty) return 0;
    if (sx >= map_w || sy >= map_h) return PATH_NO_PATH;

    _bfs(map, map_w, map_h, tx, ty, s_dist);
    len = s_dist[(u16)sy * map_w + sx];
    if (len == _DIST_INF) return PATH_NO_PATH;

    /* Descente de gradient : chaque pas va vers un voisin à distance d-1. */
    cx = sx; cy = sy;
    for (k = 0; k < len; k++) {
        ci = (u16)cy * map_w + cx;
        for (i = 0; i < 4; i++) {
            s8 nx = (s8)((s8)cx + _dx[i]);
            s8 ny = (s8)((s8)cy + _dy[i]);
            if (nx < 0 || ny < 0 || (u8)nx >= map_w || (u8)ny >= map_h) continue;
            if (s_dist[(u16)(u8)ny * map_w + (u8)nx] == (u8)(s_dist[ci] - 1u)) break;
        }
        /* i < 4 garanti : toute case à distance d > 0 a un voisin à d-1 (propriété du BFS) */
        p->steps[k >> 2] = (u8)((p->steps[k >> 2] & ~(3u << ((k & 3u) << 1)))
                                | (u8)(i << ((k & 3u) << 1)));
        cx = (u8)((s8)cx + _dx[i]);
        cy = (u8)((s8)cy + _dy[i]);
    }
    p->len = len;
    return len;
}

u8 ngpc_path_next(NgpcPath *p, s8 *out_dx, s8 *out_dy)
{
    u8 d;
    if (p->pos >= p->len) { *out_dx = 0; *out_dy = 0; return 0; }
    d = (u8)((p->steps[p->pos >> 2] >> ((p->pos & 3u) << 1)) & 3u);
    p->pos++;
    *out_dx = _dx[d];
    *out_dy = _dy[d];
    return 1;
}
