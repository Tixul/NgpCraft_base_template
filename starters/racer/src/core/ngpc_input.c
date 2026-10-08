/*
 * ngpc_input.c - Joypad input with edge detection
 *
 * Part of NgpCraft_base_template (MIT License)
 * Written from hardware specification (ngpcspec.txt).
 *
 * The template uses joypad bits as read from HW_JOYPAD, with the
 * convention 1 = pressed for public PAD_* masks and edge detection.
 */

#include "ngpc_hw.h"
#include "ngpc_input.h"

/* ---- State ---- */

u8 ngpc_pad_held     = 0;
u8 ngpc_pad_pressed  = 0;
u8 ngpc_pad_released = 0;
u8 ngpc_pad_repeat   = 0;

static u8 s_pad_prev = 0;
static u8 s_repeat_delay = 15;
static u8 s_repeat_rate  = 4;
static u8 s_repeat_timer[8];

/* Ce que le V-blank a vu s'enfoncer depuis que la boucle a regarde, et la
 * reference contre laquelle il compare. Voir ngpc_input_vblank(). */
static volatile u8 s_vbl_down = 0;
static volatile u8 s_vbl_prev = 0;

/* A PRESS MADE WHILE THE GAME IS BUSY IS STILL LOST, and the obvious fix is
 * wrong. The pad is read once per turn of the main loop, which is fine while a
 * turn is a frame -- and a menu that loads a page is not: a couple of hundred
 * characters take several frames. A button pressed and released inside one of
 * those turns never existed as far as edge detection is concerned. Measured: a
 * direction pressed within five frames of the course page coming up was lost
 * every time.
 *
 * Sampling the pad from the V-blank interrupt and keeping the presses for the
 * loop to collect was tried and REVERTED. It manufactures presses: with it in,
 * the course page started the race on its own, with nothing held at all. The
 * pad this module reads is 0x6F82, which is the BIOS's copy rather than the
 * port itself, and reading it from our own handler does not see it at a point
 * where it is guaranteed settled. Anything done here has to sample the same
 * source the loop does, at a point the BIOS has finished with -- which is a
 * piece of hardware work, not a two-line patch.
 *
 * ⚡ 2026-09-18 -- REPRIS, ET LA CAUSE N'ETAIT PROBABLEMENT PAS LA SOURCE.
 *
 * Un appui fantome sur la page circuit, « sans rien de presse », c'est ce que
 * donne un appui COMPTE DEUX FOIS : le A qui ouvre la page est vu une fois par
 * la boucle (front sur 0x6F82) et une fois par le V-blank (son propre front),
 * et la seconde livraison tombe sur la page suivante, qui lance la course.
 * Ca arrive des que les deux ne voient pas 0x6F82 changer au meme moment --
 * par exemple si le BIOS la met a jour APRES avoir appele notre V-blank :
 * la boucle voit l'appui une trame avant le V-blank, puis le V-blank le voit
 * « nouveau » a son tour.
 *
 * Ce qui est fait ici ne depend pas de cet ordre : a chaque lecture, la
 * boucle PREND les appuis que le V-blank a memorises ET recale la reference
 * du V-blank sur ce qu'elle vient de voir, interruptions coupees. Un appui
 * deja vu par la boucle ne peut donc plus etre « nouveau » pour le V-blank, et
 * un appui vu par le V-blank est livre une fois puis efface. Au pire, un appui
 * arrive une trame plus tard ; il n'est jamais double.
 * ⚠️ Mesure en emulation (tools/appui_bref.py). Pas encore sur console. */

/* Appele par isr_vblank() (ngpc_sys.c), a chaque blanc. */
void ngpc_input_vblank(void)
{
    u8 raw = HW_JOYPAD;

    s_vbl_down = (u8)(s_vbl_down | (u8)(raw & (u8)~s_vbl_prev));
    s_vbl_prev = raw;
}

void ngpc_input_set_repeat(u8 delay, u8 rate)
{
    s_repeat_delay = delay;
    s_repeat_rate  = rate;
}

/* ---- Public API ---- */

void ngpc_input_update(void)
{
    u8 i;
    u8 mask;
    u8 raw;

    /* Read joypad register.
     * ngpcspec.txt: 0x6F82 "Sys Lever" - button state.
     * Bits: 7=POWER 6=OPTION 5=B 4=A 3=RIGHT 2=LEFT 1=DOWN 0=UP */
    u8 latched;

    /* PRENDRE ET RECALER D'UN SEUL COUP, interruptions coupees : un V-blank
     * glisse entre les deux perdrait un appui ou en referait un. Le jeu tourne
     * au niveau 0 partout (le module link l'y remet apres chaque appel BIOS). */
    __asm(" di");
    raw = HW_JOYPAD;
    latched = s_vbl_down;
    s_vbl_down = 0;
    s_vbl_prev = raw;
    __asm(" ei 0");

    ngpc_pad_held     = raw;
    /* newly pressed: ce que la boucle voit changer, PLUS ce que le V-blank a
     * vu passer pendant un tour trop long (appui enfonce ET relache dedans) */
    ngpc_pad_pressed  = (u8)((raw & ~s_pad_prev) | latched);
    ngpc_pad_released = ~raw & s_pad_prev;   /* newly released */
    ngpc_pad_repeat   = 0;

    mask = 1;
    for (i = 0; i < 8; ++i) {
        if (raw & mask) {
            if (ngpc_pad_pressed & mask) {
                s_repeat_timer[i] = s_repeat_delay;
            } else {
                if (s_repeat_timer[i] > 0) {
                    --s_repeat_timer[i];
                } else {
                    ngpc_pad_repeat |= mask;
                    s_repeat_timer[i] = s_repeat_rate;
                }
            }
        } else {
            s_repeat_timer[i] = 0;
        }
        mask <<= 1;
    }

    s_pad_prev = raw;
}
