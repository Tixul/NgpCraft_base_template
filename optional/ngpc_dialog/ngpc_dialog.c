#include "ngpc_dialog.h"
#include "ngpc_gfx.h"    /* ngpc_gfx_put_tile, ngpc_gfx_fill */
#include "ngpc_text.h"   /* ngpc_text_print */
#include "ngpc_input.h"  /* ngpc_pad_pressed, PAD_A, PAD_UP, PAD_DOWN */

/* Caractères de cadre (ASCII) */
#define _FRAME_H  '-'
#define _FRAME_V  '|'
#define _FRAME_TL '+'
#define _FRAME_TR '+'
#define _FRAME_BL '+'
#define _FRAME_BR '+'
#define _SPACE    ' '

/* ── Helpers ─────────────────────────────────────────────── */

/* Écrit un char unique à (tx, ty) sur le plan donné. */
static void _put_char(u8 plane, u8 pal, u8 tx, u8 ty, char c)
{
    char s[2];
    s[0] = c;
    s[1] = '\0';
    ngpc_text_print(plane, pal, tx, ty, s);
}

/* Dessine le cadre de la boîte. */
static void _draw_frame(const NgpcDialog *d, u8 pal)
{
    u8 x, y;
    char line[21];
    u8 inner_w = d->bw - 2;
    u8 inner_h = d->bh - 2;

    /* Ligne horizontale (intérieur) */
    {
        u8 i;
        for (i = 0; i < inner_w; i++) line[i] = _FRAME_H;
        line[inner_w] = '\0';
    }

    /* Haut */
    _put_char(d->plane, pal, d->bx,              d->by, _FRAME_TL);
    ngpc_text_print(d->plane, pal, (u8)(d->bx + 1), d->by, line);
    _put_char(d->plane, pal, (u8)(d->bx + d->bw - 1), d->by, _FRAME_TR);

    /* Milieu */
    {
        u8 i;
        for (i = 0; i < inner_w; i++) line[i] = _SPACE;
        line[inner_w] = '\0';
    }
    for (y = 1; y <= inner_h; y++) {
        _put_char(d->plane, pal, d->bx, (u8)(d->by + y), _FRAME_V);
        ngpc_text_print(d->plane, pal, (u8)(d->bx + 1), (u8)(d->by + y), line);
        _put_char(d->plane, pal, (u8)(d->bx + d->bw - 1),
                  (u8)(d->by + y), _FRAME_V);
        (void)x;
    }

    /* Bas */
    {
        u8 i;
        for (i = 0; i < inner_w; i++) line[i] = _FRAME_H;
        line[inner_w] = '\0';
    }
    _put_char(d->plane, pal, d->bx,
              (u8)(d->by + d->bh - 1), _FRAME_BL);
    ngpc_text_print(d->plane, pal, (u8)(d->bx + 1),
                    (u8)(d->by + d->bh - 1), line);
    _put_char(d->plane, pal, (u8)(d->bx + d->bw - 1),
              (u8)(d->by + d->bh - 1), _FRAME_BR);
}

/* Efface l'intérieur de la boîte. */
static void _clear_inner(const NgpcDialog *d, u8 pal)
{
    char line[21];
    u8 i, y;
    u8 inner_w = d->bw - 2;
    u8 inner_h = d->bh - 2;

    for (i = 0; i < inner_w && i < 20; i++) line[i] = _SPACE;
    line[(inner_w < 20) ? inner_w : 20] = '\0';

    for (y = 0; y < inner_h; y++) {
        ngpc_text_print(d->plane, pal, (u8)(d->bx + 1),
                        (u8)(d->by + 1 + y), line);
    }
}

/* Dessine l'indicateur ▶ ou l'efface. */
static void _draw_arrow(const NgpcDialog *d, u8 pal, u8 visible)
{
    char c = visible ? DIALOG_ARROW_CHAR : _SPACE;
    _put_char(d->plane, pal,
              (u8)(d->bx + d->bw - 2),
              (u8)(d->by + d->bh - 2), c);
}

/* Redessine le texte déjà défilé (après efface). */
static void _redraw_text(const NgpcDialog *d, u8 pal)
{
    u8 col = d->bx + 1;
    u8 row = d->by + 1;
    u8 max_col = d->bx + d->bw - 2;
    u8 max_row = d->by + d->bh - 2;
    u8 i;
    char ch[2];
    ch[1] = '\0';

    for (i = 0; i < d->char_idx && d->text[i] != '\0'; i++) {
        if (d->text[i] == '\n') {
            col = d->bx + 1;
            row++;
            if (row > max_row) break;
        } else {
            if (col <= max_col && row <= max_row) {
                ch[0] = d->text[i];
                ngpc_text_print(d->plane, pal, col, row, ch);
            }
            col++;
            if (col > max_col) {
                col = d->bx + 1;
                row++;
            }
        }
    }
}

/* Dessine les choix disponibles. */
static void _draw_choices(const NgpcDialog *d, u8 pal)
{
    u8 i;
    u8 ty = d->by + d->bh - 1 - d->n_choices;

    for (i = 0; i < d->n_choices; i++) {
        char prefix[2];
        prefix[0] = (i == d->cursor) ? '>' : ' ';
        prefix[1] = '\0';
        ngpc_text_print(d->plane, pal, (u8)(d->bx + 1), (u8)(ty + i), prefix);
        ngpc_text_print(d->plane, pal, (u8)(d->bx + 2), (u8)(ty + i), d->choices[i]);
    }
}

/* ── API ─────────────────────────────────────────────────── */

void ngpc_dialog_open(NgpcDialog *d,
                      u8 plane, u8 bx, u8 by, u8 bw, u8 bh, u8 pal)
{
    d->text      = 0;
    d->choices   = 0;
    d->plane     = plane;
    d->bx        = bx;
    d->by        = by;
    d->bw        = bw;
    d->bh        = bh;
    d->char_idx  = 0;
    d->tick      = 0;
    d->blink     = 0;
    d->cursor    = 0;
    d->n_choices = 0;
    d->flags     = _DLG_OPEN;

    _draw_frame(d, pal);
}

void ngpc_dialog_close(NgpcDialog *d)
{
    /* Efface la zone complète (cadre + intérieur) */
    char line[21];
    u8 i, y;
    for (i = 0; i < d->bw && i < 20; i++) line[i] = _SPACE;
    line[(d->bw < 20) ? d->bw : 20] = '\0';
    for (y = 0; y < d->bh; y++) {
        ngpc_text_print(d->plane, 0, d->bx, (u8)(d->by + y), line);
    }
    d->flags = 0;
}

void ngpc_dialog_set_text(NgpcDialog *d, const char *text)
{
    d->text     = text;
    d->char_idx = 0;
    d->tick     = 0;
    d->blink    = 0;
    d->flags   &= ~(_DLG_TEXT_DONE | _DLG_HAS_CHOICES);
    d->n_choices = 0;
    _clear_inner(d, 0);
}

void ngpc_dialog_set_choices(NgpcDialog *d,
                             const char **choices, u8 count)
{
    if (count > DIALOG_MAX_CHOICES) count = DIALOG_MAX_CHOICES;
    d->choices   = choices;
    d->n_choices = count;
    d->cursor    = 0;
    if (count > 0) d->flags |= _DLG_HAS_CHOICES;
}

u8 ngpc_dialog_update(NgpcDialog *d)
{
    if (!(d->flags & _DLG_OPEN)) return DIALOG_DONE;

    /* ── Texte encore en cours ── */
    if (!(d->flags & _DLG_TEXT_DONE)) {
        if (d->text == 0 || d->text[d->char_idx] == '\0') {
            /* Texte terminé */
            d->flags |= _DLG_TEXT_DONE;

            if (d->flags & _DLG_HAS_CHOICES) {
                _draw_choices(d, 0);
            }
        } else {
            /* PAD_A : affiche tout immédiatement */
            if (ngpc_pad_pressed & PAD_A) {
                while (d->text[d->char_idx] != '\0') d->char_idx++;
                d->flags |= _DLG_TEXT_DONE;
                _clear_inner(d, 0);
                _redraw_text(d, 0);
                if (d->flags & _DLG_HAS_CHOICES) {
                    _draw_choices(d, 0);
                }
                return DIALOG_RUNNING;
            }

            /* Défilement lettre par lettre */
            d->tick++;
            if (d->tick >= DIALOG_TEXT_SPEED) {
                u8 col = d->bx + 1;
                u8 row = d->by + 1;
                u8 max_col = d->bx + d->bw - 2;
                u8 max_row = d->by + d->bh - 2;
                u8 i;
                char ch[2];
                ch[1] = '\0';

                d->tick = 0;

                /* Calculer la position de la prochaine lettre */
                for (i = 0; i < d->char_idx; i++) {
                    if (d->text[i] == '\n') {
                        col = d->bx + 1; row++;
                    } else {
                        col++;
                        if (col > max_col) { col = d->bx + 1; row++; }
                    }
                }

                if (row <= max_row && col <= max_col
                    && d->text[d->char_idx] != '\n') {
                    ch[0] = d->text[d->char_idx];
                    ngpc_text_print(d->plane, 0, col, row, ch);
                }
                d->char_idx++;
            }
        }
        return DIALOG_RUNNING;
    }

    /* ── Texte terminé — choix ── */
    if (d->flags & _DLG_HAS_CHOICES) {
        u8 changed = 0;

        if ((ngpc_pad_pressed & PAD_UP) && d->cursor > 0) {
            d->cursor--;
            changed = 1;
        }
        if ((ngpc_pad_pressed & PAD_DOWN)
            && d->cursor < d->n_choices - 1) {
            d->cursor++;
            changed = 1;
        }
        if (changed) _draw_choices(d, 0);

        if (ngpc_pad_pressed & PAD_A) {
            u8 sel = d->cursor;
            ngpc_dialog_close(d);
            return (u8)(DIALOG_CHOICE_0 + sel);
        }
        return DIALOG_RUNNING;
    }

    /* ── Texte terminé — clignotement ▶ + A pour fermer ── */
    d->blink++;
    if (d->blink >= DIALOG_BLINK_PERIOD) d->blink = 0;
    _draw_arrow(d, 0, (u8)(d->blink < (DIALOG_BLINK_PERIOD / 2)));

    if (ngpc_pad_pressed & PAD_A) {
        ngpc_dialog_close(d);
        return DIALOG_DONE;
    }
    return DIALOG_RUNNING;
}
