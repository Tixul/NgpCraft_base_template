#include "ngpc_transition.h"

/* corrigé campagne 2026-11-08 : le fondu est fait ici, sur les 48 palettes
 * (SCR1, SCR2, SPR), au lieu de ngpc_palfx_*(plane, 0xFF, ...) qui visait la
 * "palette 255" (adresse hors RAM palette) et n'assombrissait rien. */

#define _TR_FADE_STEPS 15u

static void tr_fill_white(void)
{
    volatile u16 *p = HW_PAL_SPR;            /* 0x8200..0x837F : SPR, SCR1, SCR2 */
    u8 i;
    for (i = 0; i < 192u; i++) p[i] = 0x0FFFu;
}

/* One step toward black on every channel of the 192 colours. */
static void tr_darken_step(void)
{
    volatile u16 *p = HW_PAL_SPR;
    u8 i;
    u16 c, r, g, b;
    for (i = 0; i < 192u; i++) {
        c = p[i];
        r = (u16)(c & 0x00Fu);
        g = (u16)(c & 0x0F0u);
        b = (u16)(c & 0xF00u);
        if (r) r = (u16)(r - 0x001u);
        if (g) g = (u16)(g - 0x010u);
        if (b) b = (u16)(b - 0x100u);
        p[i] = (u16)(r | g | b);
    }
}

void ngpc_transition_init(NgpcTransition *tr, u8 phase_frames)
{
    ngpc_room_init(&tr->room, phase_frames ? phase_frames : 1);
    tr->type         = TRANS_FADE;
    tr->phase_frames = phase_frames ? phase_frames : 1;
    tr->fade_step    = _TR_FADE_STEPS;
}

void ngpc_transition_start(NgpcTransition *tr, u8 type)
{
    if (ngpc_room_in_transition(&tr->room)) return;
    tr->type = type;
    tr->fade_step = (type == TRANS_INSTANT) ? (u8)_TR_FADE_STEPS : 0u;
    if (type == TRANS_FLASH) tr_fill_white();
    ngpc_room_go(&tr->room, 0);  /* next_room ignoré — géré par le jeu */
}

void ngpc_transition_loaded(NgpcTransition *tr)
{
    /*
     * À ce stade le jeu a chargé ses nouvelles palettes. Le fondu d'entrée est
     * à la charge du jeu APRÈS cet appel (ngpc_palfx_fade, un slot par palette,
     * vitesse _TRANS_SPEED(phase)), ou via ngpc_transition_progress().
     */
    ngpc_room_loaded(&tr->room);
}

u8 ngpc_transition_update(NgpcTransition *tr)
{
    u8 r = ngpc_room_update(&tr->room);
    u8 target;

    if ((r == TRANS_OUT || r == TRANS_LOAD) && tr->fade_step < _TR_FADE_STEPS) {
        if (r == TRANS_LOAD) {
            target = _TR_FADE_STEPS;
        } else {
            target = (u8)(((u16)(u8)(tr->room.duration - tr->room.timer) * _TR_FADE_STEPS)
                          / (u16)tr->room.duration);
        }
        while (tr->fade_step < target) {
            tr_darken_step();
            tr->fade_step++;
        }
    }
    return r;
}
