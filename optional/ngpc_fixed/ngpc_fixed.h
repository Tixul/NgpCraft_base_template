#ifndef NGPC_FIXED_H
#define NGPC_FIXED_H

/*
 * ngpc_fixed -- Math fixe-point 8.4 (header-only, zéro RAM)
 * ==========================================================
 * Format : s16, 4 bits fractionnels (partie entière sur 11 bits + signe)
 *   Plage     : -2048.0 .. +2047.9375 pixels
 *   Précision : 1/16 = 0.0625 pixel
 *
 * Adapté à l'écran NGPC (160x152 px), CPU 6 MHz sans FPU.
 * Toutes les opérations = arithmétique entière pure.
 * Header-only : pas de .c à ajouter au makefile.
 *
 * Usage : #include "ngpc_fixed/ngpc_fixed.h"
 */

#include "ngpc_hw.h"   /* u8, u16, s8, s16, s32 */

/* ── Type de base ───────────────────────────────────── */

typedef s16 fx16;   /* fixe-point 8.4 : les 4 bits bas = fraction */

/* ── Constantes ─────────────────────────────────────── */

#define FX_SHIFT     4
#define FX_ONE       ((fx16)(1 << FX_SHIFT))   /* 1.0  = 0x0010 = 16  */
#define FX_HALF      ((fx16)(FX_ONE >> 1))     /* 0.5  = 0x0008 =  8  */
#define FX_QUARTER   ((fx16)(FX_ONE >> 2))     /* 0.25 = 0x0004 =  4  */
#define FX_MASK      (FX_ONE - 1)              /* masque fractionnel   */
#define FX_MAXVAL    ((fx16)0x7FFF)            /* +2047.9375 (valeur max) */
#define FX_MINVAL    ((fx16)((s16)0x8000))     /* -2048.0    (valeur min) */

/* ── Conversions ────────────────────────────────────── */

/* Entier → fx16  (ex: INT_TO_FX(3) = 48 = 3.0) */
#define INT_TO_FX(x)     ((fx16)((s16)(x) << FX_SHIFT))

/* fx16 → entier, tronqué vers zéro */
#define FX_TO_INT(x)     ((s16)((x) >> FX_SHIFT))

/* fx16 → entier, arrondi au plus proche */
#define FX_ROUND(x)      ((s16)(((x) + FX_HALF) >> FX_SHIFT))

/* Partie fractionnelle (toujours 0..15) */
#define FX_FRAC(x)       ((x) & FX_MASK)

/* Constante littérale flottante — compile-time uniquement ! */
#define FX_LIT(f)        ((fx16)((f) >= 0 ? (f) * FX_ONE + 0.5f : (f) * FX_ONE - 0.5f))

/* ── Arithmétique ───────────────────────────────────── */

#define FX_ADD(a, b)     ((fx16)((a) + (b)))
#define FX_SUB(a, b)     ((fx16)((a) - (b)))
#define FX_NEG(a)        ((fx16)(-(a)))
#define FX_ABS(a)        ((fx16)((a) < 0 ? -(a) : (a)))
#define FX_SIGN(a)       ((fx16)((a) > 0 ? FX_ONE : ((a) < 0 ? -FX_ONE : 0)))

/* Multiplication fx16 × fx16 → fx16  (utilise s32 en interne) */
#define FX_MUL(a, b)     ((fx16)(((s32)(a) * (s32)(b)) >> FX_SHIFT))

/* Division fx16 / fx16 → fx16  (utilise s32 en interne) */
#define FX_DIV(a, b)     ((fx16)(((s32)(a) << FX_SHIFT) / (s32)(b)))

/* Mise à l'échelle : entier × facteur fx16 → entier
 * Ex: FX_SCALE(100, FX_LIT(0.5)) → 50  */
#define FX_SCALE(i, f)   ((s16)(((s32)(i) * (s32)(f)) >> FX_SHIFT))

/* ── Clamp / Min / Max ──────────────────────────────── */

#define FX_MIN(a, b)     ((a) < (b) ? (a) : (b))
#define FX_MAX(a, b)     ((a) > (b) ? (a) : (b))
#define FX_CLAMP(x, lo, hi) FX_MIN(FX_MAX((x), (lo)), (hi))

/* Lerp linéaire : FX_LERP(a, b, t) avec t en fx16 [0..FX_ONE]
 * t = FX_ONE → résultat = b, t = 0 → résultat = a
 * Utile pour transitions de caméra, fondu de valeurs. */
#define FX_LERP(a, b, t) FX_ADD((a), FX_MUL(FX_SUB((b), (a)), (t)))

/* ── Vecteur 2D ─────────────────────────────────────── */

/* Paire de fx16 pour position ou vélocité.
 * Ex: FxVec2 vel = { INT_TO_FX(2), FX_LIT(-0.5) }; */
typedef struct { fx16 x, y; } FxVec2;

/* Opérations vecteur = macros-INSTRUCTIONS avec destination explicite.
 * (corrigé campagne 2026-11-08 : les anciennes FXVEC2/FXVEC2_ZERO/FXVEC2_ADD/
 *  FXVEC2_SCALE étaient des littéraux composés C99 « (FxVec2){...} » que cc900
 *  refuse : THC1-Error-216. Initialiser avec un agrégat C89 : FxVec2 v = {0, 0};)
 * dst peut être a ou v lui-même (les deux composantes sont lues avant écriture
 * composante par composante, sans dépendance croisée). */
#define FXVEC2_SET(dst, px, py)   do { (dst).x = (fx16)(px); (dst).y = (fx16)(py); } while (0)
#define FXVEC2_ZERO(dst)          FXVEC2_SET((dst), 0, 0)
#define FXVEC2_ADD(dst, a, b)     do { (dst).x = FX_ADD((a).x, (b).x); \
                                       (dst).y = FX_ADD((a).y, (b).y); } while (0)
#define FXVEC2_SCALE(dst, v, f)   do { (dst).x = FX_MUL((v).x, (f)); \
                                       (dst).y = FX_MUL((v).y, (f)); } while (0)

/* ── Physique de base ───────────────────────────────── */
/*
 * Mouvement subpixel typique chaque frame :
 *
 *   vel.y = FX_ADD(vel.y, GRAVITY);          // appliquer gravité
 *   vel.y = FX_MIN(vel.y, MAX_FALL_SPEED);   // clamper chute
 *   pos.x = FX_ADD(pos.x, vel.x);
 *   pos.y = FX_ADD(pos.y, vel.y);
 *   sprite_x = FX_TO_INT(pos.x);            // position pixel pour sprite
 *   sprite_y = FX_TO_INT(pos.y);
 *
 * Valeurs courantes :
 *   GRAVITY       = FX_LIT(0.25)   → 0.25 px/frame² = 15 px/s² à 60fps
 *   MAX_FALL_SPEED = INT_TO_FX(4)  → 4 px/frame max
 *   JUMP_VEL      = FX_LIT(-3.5)  → saut de 3.5 px/frame initial
 *
 * Intégration avec ngpc_sin/cos (s8 [-127..127]) :
 *   fx16 vx = (fx16)((s16)ngpc_cos(angle) >> 3); // 127>>3 ≈ 15 ≈ FX_ONE
 *   fx16 vy = (fx16)((s16)ngpc_sin(angle) >> 3);
 */

#endif /* NGPC_FIXED_H */
