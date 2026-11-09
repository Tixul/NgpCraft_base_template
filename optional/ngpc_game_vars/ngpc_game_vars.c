/*
 * ngpc_game_vars.c -- Persistent flags & variables
 * See ngpc_game_vars.h for documentation.
 */

#include "ngpc_game_vars.h"

/*
 * corrigé campagne 2026-11-08 : ce .c n'incluait jamais l'en-tête généré (même nom de
 * fichier que celui du module, tableau 'static'), la branche #ifdef était morte et les
 * valeurs initiales n'étaient jamais appliquées. Le tableau est maintenant passé par le jeu
 * (ngpc_gv_init_from, ou la macro ngpc_gv_init() du .h) et mémorisé pour l'action 74.
 */
static const u8 *s_gv_inits;

/* ---- State ------------------------------------------------------------ */

u8 ngpc_gv_flags[NGPC_GV_FLAG_COUNT];
u8 ngpc_gv_vars[NGPC_GV_VAR_COUNT];

/* ---- Init ------------------------------------------------------------- */

void ngpc_gv_init_from(const u8 *inits)
{
    u8 i;
    s_gv_inits = inits;
    for (i = 0u; i < NGPC_GV_FLAG_COUNT; ++i) ngpc_gv_flags[i] = 0u;
    for (i = 0u; i < NGPC_GV_VAR_COUNT; ++i)  ngpc_gv_vars[i]  = inits ? inits[i] : 0u;
}

/* ---- Flags ------------------------------------------------------------ */

void ngpc_gv_set_flag(u8 idx)
{
    if (idx < NGPC_GV_FLAG_COUNT) ngpc_gv_flags[idx] = 1u;
}

void ngpc_gv_clear_flag(u8 idx)
{
    if (idx < NGPC_GV_FLAG_COUNT) ngpc_gv_flags[idx] = 0u;
}

void ngpc_gv_toggle_flag(u8 idx)
{
    if (idx < NGPC_GV_FLAG_COUNT) ngpc_gv_flags[idx] ^= 1u;
}

u8 ngpc_gv_get_flag(u8 idx)
{
    return (idx < NGPC_GV_FLAG_COUNT) ? ngpc_gv_flags[idx] : 0u;
}

/* ---- Variables -------------------------------------------------------- */

void ngpc_gv_set_var(u8 idx, u8 val)
{
    if (idx < NGPC_GV_VAR_COUNT) ngpc_gv_vars[idx] = val;
}

void ngpc_gv_inc_var(u8 idx, u8 cap)
{
    if (idx < NGPC_GV_VAR_COUNT) {
        if (cap == 0u || ngpc_gv_vars[idx] < cap) ngpc_gv_vars[idx]++;
    }
}

void ngpc_gv_dec_var(u8 idx)
{
    if (idx < NGPC_GV_VAR_COUNT && ngpc_gv_vars[idx] > 0u) ngpc_gv_vars[idx]--;
}

u8 ngpc_gv_get_var(u8 idx)
{
    return (idx < NGPC_GV_VAR_COUNT) ? ngpc_gv_vars[idx] : 0u;
}

/* ---- Trigger dispatch ------------------------------------------------- */

#define _GV_ACT_SET_FLAG       28u
#define _GV_ACT_CLEAR_FLAG     29u
#define _GV_ACT_SET_VARIABLE   30u
#define _GV_ACT_INC_VARIABLE   31u
#define _GV_ACT_DEC_VARIABLE   51u
#define _GV_ACT_TOGGLE_FLAG    58u
#define _GV_ACT_INIT_GAME_VARS 74u

u8 ngpc_gv_dispatch(u8 action, u8 a0, u8 a1)
{
    switch (action) {
    case _GV_ACT_SET_FLAG:       ngpc_gv_set_flag(a0);     return 1u;
    case _GV_ACT_CLEAR_FLAG:     ngpc_gv_clear_flag(a0);   return 1u;
    case _GV_ACT_TOGGLE_FLAG:    ngpc_gv_toggle_flag(a0);  return 1u;
    case _GV_ACT_SET_VARIABLE:   ngpc_gv_set_var(a0, a1);  return 1u;
    case _GV_ACT_INC_VARIABLE:   ngpc_gv_inc_var(a0, a1);  return 1u;
    case _GV_ACT_DEC_VARIABLE:   ngpc_gv_dec_var(a0);      return 1u;
    case _GV_ACT_INIT_GAME_VARS: ngpc_gv_init_from(s_gv_inits); return 1u;
    default:                                                 return 0u;
    }
}
