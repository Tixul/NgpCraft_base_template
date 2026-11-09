#ifndef NGPC_PATH_H
#define NGPC_PATH_H

/*
 * ngpc_path -- Pathfinding BFS sur petite grille
 * ================================================
 * Trouve le chemin le plus court entre deux cellules sur une grille
 * max 16×16 (limitée par la capacité des coordonnées en u8 4 bits).
 * Retourne le PREMIER PAS (direction) et la longueur totale du chemin.
 *
 * Utilisation principale : IA ennemis (se diriger vers le joueur).
 *
 * Coût RAM (buffers statiques internes) :
 *   - dist[16×16] = 256 octets    (partagé par step / dist / build)
 *   - queue[256]  = 256 octets    (file BFS, 4 bits x + 4 bits y par case)
 * Total : 512 octets statiques (corrigé campagne 2026-11-08 : file 256, plus de limite à 63 pas)
 * + 66 octets par NgpcPath si l'API build/next est utilisée.
 * Coût : un BFS complet par appel (~quelques images CPU sur 15×15) -> ne pas appeler
 * chaque image pour chaque ennemi : recalculer quand la case de la cible change.
 *
 * Contrainte : grille max 16×16 (tx, ty ∈ [0..15]).
 *
 * Dépend de : ngpc_hw.h uniquement
 *
 * Usage :
 *   Copier ngpc_path/ dans src/
 *   OBJS += src/ngpc_path/ngpc_path.rel
 *   #include "ngpc_path/ngpc_path.h"
 *
 * Exemple — ennemi qui suit le joueur :
 *   // La map de collision est partagée avec ngpc_tilecol
 *   // (0 = passable, 1 = solide)
 *   s8 dx, dy;
 *   if (ngpc_path_step(col_map, MAP_W, MAP_H,
 *                      enemy_tx, enemy_ty,
 *                      player_tx, player_ty,
 *                      &dx, &dy)) {
 *       enemy_tx += dx;
 *       enemy_ty += dy;
 *   }
 *
 * Exemple — longueur du chemin :
 *   u8 dist = ngpc_path_dist(col_map, MAP_W, MAP_H,
 *                            enemy_tx, enemy_ty,
 *                            player_tx, player_ty);
 *   if (dist == PATH_NO_PATH) { wander(); }
 *   else if (dist <= 2)       { attack(); }
 *   else                      { follow(); }
 *
 * Connectivité : 4 directions (haut, bas, gauche, droite).
 * Tiles passables : valeur 0. Solides : toute valeur != 0.
 */

#include "ngpc_hw.h"

/* Valeur retournée par ngpc_path_dist() si aucun chemin. */
#define PATH_NO_PATH  0xFF

/* Taille max de la grille supportée. */
#define PATH_MAX_W  16
#define PATH_MAX_H  16

/*
 * Trouve le premier pas du chemin le plus court de (sx,sy) vers (tx,ty).
 *
 * map     : map de collision (flat array, 0 = passable, != 0 = solide)
 * map_w   : largeur en tiles (max 16)
 * map_h   : hauteur en tiles (max 16)
 * sx, sy  : position source (tile)
 * tx, ty  : position cible (tile)
 * *out_dx : direction horizontale du premier pas (-1, 0, +1)
 * *out_dy : direction verticale du premier pas (-1, 0, +1)
 *
 * Retourne 1 si un chemin existe, 0 sinon (cible inaccessible ou déjà arrivé).
 * Si retour 0 : *out_dx = *out_dy = 0.
 */
u8 ngpc_path_step(const u8 *map, u8 map_w, u8 map_h,
                  u8 sx, u8 sy, u8 tx, u8 ty,
                  s8 *out_dx, s8 *out_dy);

/*
 * Calcule la longueur du chemin le plus court de (sx,sy) vers (tx,ty).
 * Retourne le nombre de pas, ou PATH_NO_PATH si inaccessible.
 * 0 si source == cible.
 */
u8 ngpc_path_dist(const u8 *map, u8 map_w, u8 map_h,
                  u8 sx, u8 sy, u8 tx, u8 ty);

/* ── Calculer une fois, suivre pas à pas (ajout campagne 2026-11-08) ──────
 * Un seul BFS par chemin au lieu d'un par image : appeler ngpc_path_build()
 * quand la cible change de case (ou toutes les N images), puis consommer
 * un pas par déplacement avec ngpc_path_next().
 *
 * RAM : 66 octets par NgpcPath (2 bits par pas, 255 pas max = toute grille 16×16).
 *
 *   static NgpcPath ep;
 *   if (player_changed_tile)
 *       ngpc_path_build(&ep, col_map, MAP_W, MAP_H, etx, ety, ptx, pty);
 *   if (enemy_ready_to_move && ngpc_path_next(&ep, &dx, &dy)) { etx += dx; ety += dy; }
 *
 * NB : le chemin est figé au moment du build ; si la carte change (porte, bloc
 * poussé), refaire un build.
 */
typedef struct {
    u8 steps[64];  /* 2 bits par pas : 0=haut 1=bas 2=gauche 3=droite */
    u8 len;        /* nombre total de pas */
    u8 pos;        /* prochain pas à consommer */
} NgpcPath;

/*
 * Calcule le chemin le plus court de (sx,sy) vers (tx,ty) et le mémorise dans *p.
 * Retourne sa longueur (= ngpc_path_dist), 0 si source == cible,
 * PATH_NO_PATH si inaccessible (p->len = 0 dans ces deux derniers cas).
 * Partage les buffers statiques de ngpc_path_step/dist (non réentrant).
 */
u8 ngpc_path_build(NgpcPath *p, const u8 *map, u8 map_w, u8 map_h,
                   u8 sx, u8 sy, u8 tx, u8 ty);

/*
 * Donne le pas suivant (*dx, *dy ∈ {-1,0,+1}, un seul axe) et avance.
 * Retourne 1, ou 0 (dx = dy = 0) quand le chemin est entièrement parcouru.
 */
u8 ngpc_path_next(NgpcPath *p, s8 *out_dx, s8 *out_dy);

/* Pas restants à parcourir. */
#define ngpc_path_remaining(p)  ((u8)((p)->len - (p)->pos))

/* Repartir du début du même chemin (source d'origine). */
#define ngpc_path_rewind(p)     ((p)->pos = 0)

#endif /* NGPC_PATH_H */
