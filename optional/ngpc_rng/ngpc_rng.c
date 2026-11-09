/*
 * ngpc_rng.c -- Implémentation xorshift32 pour NGPC
 *
 * Algorithme : xorshift32 (Marsaglia 2003)
 *   x ^= x << 13;
 *   x ^= x >> 17;
 *   x ^= x << 5;
 * Période 2^32 - 1, aucune multiplication, rapide sur TLCS-900/H.
 */

#include "ngpc_rng/ngpc_rng.h"

/* ── Algorithme xorshift32 (interne) ───────────────────────────────────── */

static u32 xorshift32(u32 x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/* ── Initialisation ─────────────────────────────────────────────────────── */

void ngpc_rng_init(NgpcRng *rng, u16 seed)
{
    u32 s;
    /* Étendre le seed 16-bit en u32 et le mixer pour éviter les mauvais
       états initiaux (xorshift32 dégénère si state = 0). */
    s = (u32)seed;
    if (s == 0u) s = 1u;
    /* Constante de Knuth (entier doré 32-bit) pour mélanger le seed. */
    s ^= (u32)0x9E3779B9uL;
    s = xorshift32(s);
    rng->state = s;
}

/* ngpc_rng_init_vbl : macro dans ngpc_rng.h (corrigé campagne 2026-11-08 :
   l'ancienne version lisait ADMOD 0x006D, constant -> graine toujours identique). */

/* ── Génération de nombres ──────────────────────────────────────────────── */

u16 ngpc_rng_next(NgpcRng *rng)
{
    rng->state = xorshift32(rng->state);
    return (u16)(rng->state >> 16);
}

u8 ngpc_rng_u8(NgpcRng *rng)
{
    /* Utiliser les bits hauts (meilleure distribution que les bits bas) */
    return (u8)(ngpc_rng_next(rng) >> 8);
}

u8 ngpc_rng_range(NgpcRng *rng, u8 min, u8 max)
{
    u8 span;
    if (max <= min) return min;
    span = (u8)(max - min + 1u);
    return (u8)(min + (u8)(ngpc_rng_u8(rng) % span));
}

u8 ngpc_rng_chance(NgpcRng *rng, u8 pct)
{
    if (pct == 0u)   return 0u;
    if (pct >= 100u) return 1u;
    /* corrigé campagne 2026-11-08 : u8 % 100 biaisait (restes 0..55 3 fois sur
     * 256 -> chance(50) = 58,6 %). u8 * 100 / 256 : exact pour 25/50/75 %,
     * biais <= 1/256 sinon. */
    return (u8)(((u16)((u16)ngpc_rng_u8(rng) * 100u) >> 8) < pct ? 1u : 0u);
}

s8 ngpc_rng_signed(NgpcRng *rng, u8 range)
{
    u8 span;
    u8 raw;
    if (range == 0u) return 0;
    /* Span = 2*range+1 valeurs : [-range .. 0 .. +range] */
    span = (u8)((u8)(range * 2u) + 1u);
    raw  = (u8)(ngpc_rng_u8(rng) % span);
    /* Décaler pour centrer en 0 */
    return (s8)((int)raw - (int)range);
}

/* ── Utilitaires ────────────────────────────────────────────────────────── */

void ngpc_rng_shuffle(NgpcRng *rng, u8 *arr, u8 n)
{
    u8 i, j, tmp;
    /* Fisher-Yates : O(n), distribution uniforme sur toutes les permutations */
    if (n <= 1u) return;
    /* C89 : déclarer i avant la boucle */
    i = (u8)(n - 1u);
    for (; i > 0u; i--) {
        j       = (u8)(ngpc_rng_u8(rng) % (u8)(i + 1u));
        tmp     = arr[i];
        arr[i]  = arr[j];
        arr[j]  = tmp;
    }
}

u8 ngpc_rng_pick_mask(NgpcRng *rng, u16 mask)
{
    u8 bits[16];
    u8 count;
    u8 i;
    if (mask == 0u) return 0u;
    /* Collecter les indices des bits actifs */
    count = 0u;
    for (i = 0u; i < 16u; i++) {
        if ((mask >> i) & 1u) {
            bits[count] = i;
            count++;
        }
    }
    /* Choisir au hasard parmi les bits actifs */
    return bits[ngpc_rng_u8(rng) % count];
}
