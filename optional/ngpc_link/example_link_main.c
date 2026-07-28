/*
 * example_link_main.c -- Demo du module ngpc_link (2 consoles, meme ROM)
 *
 * Module optionnel de NgpCraft_base_template (MIT License)
 *
 * CE FICHIER N'EST PAS COMPILE PAR LE TEMPLATE. Pour l'essayer, recopie son
 * contenu dans src/main.c (ou ajoute-le a OBJS a la place de src/main.rel).
 *
 * Ce qu'il fait : chaque console envoie l'etat de sa croix + boutons, et
 * affiche DEUX curseurs -- le sien (plein) et celui du pair (creux). L'etat de
 * la liaison est ecrit en clair en haut de l'ecran. Le curseur de l'hote est
 * celui de gauche : c'est ngpc_link_host qui tranche, pas l'ordre de demarrage.
 *
 * A tester :
 *  - NgpCraft Emulator, bouton 🔗 -> "2 joueurs - ce PC" (deux fenetres)
 *  - ou "Heberger" / "Rejoindre" en LAN, ou le salon en ligne
 *  - ou deux vraies consoles + cable link
 * Les trois passent par le meme chemin BIOS : le jeu ne fait pas la difference.
 */

#include "ngpc_hw.h"
#include "carthdr.h"
#include "ngpc_sys.h"
#include "ngpc_gfx.h"
#include "ngpc_sprite.h"
#include "ngpc_text.h"
#include "ngpc_input.h"
#include "ngpc_timing.h"

#include "ngpc_link/ngpc_link.h"

/* Tuiles du curseur : deux glyphes de la police systeme suffisent pour la demo. */
#define TILE_ME    '#'
#define TILE_PEER  'o'

static u8 s_my_x = 60, s_my_y = 80;
static u8 s_peer_x = 90, s_peer_y = 80;

static void move_by_pad(u8 pad, u8 *x, u8 *y)
{
    if ((pad & PAD_LEFT) && *x > 8)    { *x -= 2; }
    if ((pad & PAD_RIGHT) && *x < 150) { *x += 2; }
    if ((pad & PAD_UP) && *y > 16)     { *y -= 2; }
    if ((pad & PAD_DOWN) && *y < 144)  { *y += 2; }
}

static void draw_status(void)
{
    ngpc_text_print(GFX_SCR1, 0, 1, 1, "LINK:            ");

    switch (ngpc_link_state) {
    case NGPC_LINK_SEARCHING:
        /* Distinguer "personne ne repond" de "pas de cable" aide le joueur,
         * mais la recherche continue dans les deux cas : le bit de detection
         * est indicatif, il ne commande rien. */
        if (ngpc_link_cable()) {
            ngpc_text_print(GFX_SCR1, 0, 7, 1, "RECHERCHE");
        } else {
            ngpc_text_print(GFX_SCR1, 0, 7, 1, "PAS DE CABLE");
        }
        break;
    case NGPC_LINK_READY:
        ngpc_text_print(GFX_SCR1, 0, 7, 1,
                        ngpc_link_host ? "OK - HOTE" : "OK - INVITE");
        break;
    case NGPC_LINK_LOST:
        ngpc_text_print(GFX_SCR1, 0, 7, 1, "PERDU");
        break;
    case NGPC_LINK_MISMATCH:
        ngpc_text_print(GFX_SCR1, 0, 7, 1, "VERSION !=");
        break;
    default:
        ngpc_text_print(GFX_SCR1, 0, 7, 1, "OFF");
        break;
    }

    /* Compteurs : les memes chiffres que l'onglet Link du debugger. */
    ngpc_text_print(GFX_SCR1, 0, 1, 3, "TX");
    ngpc_text_print_dec(GFX_SCR1, 0, 4, 3, ngpc_link_stats.tx_packets, 5);
    ngpc_text_print(GFX_SCR1, 0, 10, 3, "RX");
    ngpc_text_print_dec(GFX_SCR1, 0, 13, 3, ngpc_link_stats.rx_packets, 5);
    ngpc_text_print(GFX_SCR1, 0, 1, 4, "ERR");
    ngpc_text_print_dec(GFX_SCR1, 0, 5, 4, ngpc_link_stats.bad_sum, 4);
    ngpc_text_print(GFX_SCR1, 0, 10, 4, "GAP");
    ngpc_text_print_dec(GFX_SCR1, 0, 14, 4, ngpc_link_stats.gap, 3);
}

void main(void)
{
    u8 peer_pad;

    ngpc_sys_patch();
    ngpc_init();
    ngpc_load_sysfont();
    ngpc_gfx_clear(GFX_SCR1);
    ngpc_gfx_clear(GFX_SCR2);
    ngpc_gfx_set_bg_color(RGB(0, 0, 0));
    ngpc_sprite_hide_all();

    /* Une seule fois, apres ngpc_init() : le BIOS COM installe ses ISR serie. */
    ngpc_link_init(0);

    while (1) {
        ngpc_vsync();
        ngpc_input_update();

        /* 1. Ce que J'envoie cette frame -- rempli AVANT ngpc_link_update(). */
        ngpc_link_out[0] = ngpc_pad_held;

        /* 2. L'echange. Ne bloque jamais, meme sans pair. */
        ngpc_link_update();

        /* 3. Ce que le pair m'a envoye. En cas de trou, on garde la derniere
         *    valeur connue : c'est ce qu'un jeu 2 joueurs veut (le personnage
         *    continue sa course au lieu de se figer). ngpc_link_fresh dit si
         *    c'est nouveau. */
        peer_pad = ngpc_link_ready() ? ngpc_link_in[0] : 0;

        move_by_pad(ngpc_pad_held, &s_my_x, &s_my_y);
        move_by_pad(peer_pad, &s_peer_x, &s_peer_y);

        ngpc_sprite_set(0, s_my_x, s_my_y, TILE_ME, 0, 0);
        ngpc_sprite_set(1, s_peer_x, s_peer_y, TILE_PEER, 0, 0);

        draw_status();
    }
}
