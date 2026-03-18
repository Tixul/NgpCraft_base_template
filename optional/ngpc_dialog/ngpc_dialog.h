#ifndef NGPC_DIALOG_H
#define NGPC_DIALOG_H

/*
 * ngpc_dialog -- Boîte de dialogue textuelle
 * ============================================
 * Affiche une boîte de dialogue sur la tilemap avec défilement lettre
 * par lettre. Supporte un portrait sprite optionnel et jusqu'à 2 choix.
 *
 * La boîte occupe une zone fixe définie à l'init (ex: 3 lignes en bas).
 * L'indicateur de suite (▶) clignote quand le texte est complet.
 *
 * Dépend de : ngpc_gfx.h, ngpc_text.h, ngpc_input.h (template de base)
 *
 * Usage :
 *   Copier ngpc_dialog/ dans src/
 *   OBJS += src/ngpc_dialog/ngpc_dialog.rel
 *   #include "ngpc_dialog/ngpc_dialog.h"
 *
 * Exemple — dialogue simple :
 *   static NgpcDialog dlg;
 *   static const char *msg = "Bonjour !\nBienvenue sur NGPC.";
 *   ngpc_dialog_open(&dlg, GFX_SCR1, 0, 16, 20, 3, 0);
 *   ngpc_dialog_set_text(&dlg, msg);
 *
 *   // Chaque frame (retourne DIALOG_DONE quand fermé) :
 *   u8 r = ngpc_dialog_update(&dlg);
 *   if (r == DIALOG_DONE) game_resume();
 *
 * Exemple — dialogue avec 2 choix :
 *   static const char *choices[] = { "OUI", "NON" };
 *   ngpc_dialog_set_text(&dlg, "Continuer ?");
 *   ngpc_dialog_set_choices(&dlg, choices, 2);
 *
 *   u8 r = ngpc_dialog_update(&dlg);
 *   if (r == DIALOG_CHOICE_0) { ... oui ... }
 *   if (r == DIALOG_CHOICE_1) { ... non ... }
 *
 * Configuration des tiles de la boîte :
 *   Surcharger DIALOG_TILE_* avant d'inclure le header.
 *   Par défaut, la boîte est dessinée avec des caractères ASCII.
 */

#include "ngpc_hw.h"

/* ── Retours de ngpc_dialog_update() ────────────────────── */
#define DIALOG_RUNNING   0   /* en cours */
#define DIALOG_DONE      1   /* texte fini, aucun choix, A pressé */
#define DIALOG_CHOICE_0  2   /* choix 0 sélectionné */
#define DIALOG_CHOICE_1  3   /* choix 1 sélectionné */

/* ── Paramètres ajustables ───────────────────────────────── */
#ifndef DIALOG_TEXT_SPEED
#define DIALOG_TEXT_SPEED   2   /* frames entre chaque lettre */
#endif

#ifndef DIALOG_BLINK_PERIOD
#define DIALOG_BLINK_PERIOD 30  /* frames pour le clignotement de ▶ */
#endif

#ifndef DIALOG_MAX_LINES
#define DIALOG_MAX_LINES    3   /* lignes de texte dans la boîte */
#endif

#ifndef DIALOG_MAX_CHOICES
#define DIALOG_MAX_CHOICES  2
#endif

/* Tile indicateur de suite (caractère ASCII ▶ approx). */
#ifndef DIALOG_ARROW_CHAR
#define DIALOG_ARROW_CHAR   '>'
#endif

/* ── Flags internes ──────────────────────────────────────── */
#define _DLG_HAS_CHOICES  0x01
#define _DLG_TEXT_DONE    0x02
#define _DLG_OPEN         0x04

/* ── Struct (14 octets RAM) ──────────────────────────────── */
typedef struct {
    const char *text;          /* texte courant (ROM)              */
    const char **choices;      /* tableau de choix (NULL si aucun) */
    u8  plane;                 /* plan tilemap                     */
    u8  bx, by;                /* position tile de la boîte        */
    u8  bw, bh;                /* largeur/hauteur en tiles         */
    u8  char_idx;              /* position courante dans le texte  */
    u8  tick;                  /* compteur de frame                */
    u8  blink;                 /* compteur clignotement            */
    u8  cursor;                /* curseur de choix (0/1)           */
    u8  n_choices;             /* nombre de choix (0/1/2)          */
    u8  flags;                 /* _DLG_*                           */
} NgpcDialog;

/* ── API ─────────────────────────────────────────────────── */

/*
 * Ouvre la boîte de dialogue et dessine le cadre.
 *   plane     : GFX_SCR1 ou GFX_SCR2
 *   bx, by    : position tile du coin haut-gauche de la boîte
 *   bw, bh    : largeur/hauteur en tiles (bw max 20)
 *   pal       : palette texte
 */
void ngpc_dialog_open(NgpcDialog *d,
                      u8 plane, u8 bx, u8 by, u8 bw, u8 bh, u8 pal);

/*
 * Ferme la boîte (efface le cadre et le texte).
 */
void ngpc_dialog_close(NgpcDialog *d);

/*
 * Définit le texte à afficher. Peut utiliser '\n' pour les sauts de ligne.
 * Appeler après ngpc_dialog_open().
 */
void ngpc_dialog_set_text(NgpcDialog *d, const char *text);

/*
 * Ajoute des choix (max 2) affichés quand le texte est complet.
 * Appeler après ngpc_dialog_set_text().
 */
void ngpc_dialog_set_choices(NgpcDialog *d, const char **choices, u8 count);

/*
 * Met à jour la boîte — appeler UNE FOIS par frame.
 * Gère : défilement lettre par lettre, A pour skip/confirmer, D-pad pour choix.
 * Retourne DIALOG_RUNNING / DIALOG_DONE / DIALOG_CHOICE_0 / DIALOG_CHOICE_1.
 */
u8 ngpc_dialog_update(NgpcDialog *d);

/*
 * 1 si la boîte est ouverte et en cours.
 * Utiliser pour bloquer le gameplay pendant le dialogue.
 */
#define ngpc_dialog_is_open(d)  ((d)->flags & _DLG_OPEN)

#endif /* NGPC_DIALOG_H */
