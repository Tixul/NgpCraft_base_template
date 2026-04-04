#include "ngpc_dialog.h"
#include "ngpc_gfx.h"
#include "ngpc_text.h"
#include "ngpc_input.h"

#define _FRAME_H  '-'
#define _FRAME_V  '|'
#define _FRAME_TL '+'
#define _FRAME_TR '+'
#define _FRAME_BL '+'
#define _FRAME_BR '+'
#define _SPACE    ' '

/*
 * NGPCRAFT ENGINE COMPATIBILITY NOTE
 * ----------------------------------
 * The large block marked below adds:
 *   - encoded speaker parsing ("\001Speaker\002Text")
 *   - portrait-aware text layout
 *   - incremental word-wrapped typewriter rendering
 *
 * If you only want the smallest plain-text-only dialogue box, you can
 * comment out or delete the marked block together with the matching
 * declarations in ngpc_dialog.h.
 */

typedef struct {
    u8 speaker_x;
    u8 speaker_y;
    u8 text_x;
    u8 text_y;
    u8 max_col;
    u8 max_row;
} NgpcDialogLayout;

typedef struct {
    u8 row;
    u8 col;
    u8 need_word_check;
} NgpcDialogCursor;

static void _put_char(u8 plane, u8 pal, u8 tx, u8 ty, char c)
{
    char s[2];
    s[0] = c;
    s[1] = '\0';
    ngpc_text_print(plane, pal, tx, ty, s);
}

static void _draw_frame(const NgpcDialog *d)
{
    u8 y;
    char line[21];
    u8 pal;
    u8 plane;
    u8 inner_w;
    u8 inner_h;

    pal = ngpc_dialog_palette(d);
    plane = ngpc_dialog_plane(d);
    inner_w = (u8)(d->bw - 2u);
    inner_h = (u8)(d->bh - 2u);

    {
        u8 i;
        for (i = 0u; i < inner_w; i++) line[i] = _FRAME_H;
        line[inner_w] = '\0';
    }

    _put_char(plane, pal, d->bx, d->by, _FRAME_TL);
    ngpc_text_print(plane, pal, (u8)(d->bx + 1u), d->by, line);
    _put_char(plane, pal, (u8)(d->bx + d->bw - 1u), d->by, _FRAME_TR);

    {
        u8 i;
        for (i = 0u; i < inner_w; i++) line[i] = _SPACE;
        line[inner_w] = '\0';
    }
    for (y = 1u; y <= inner_h; y++) {
        _put_char(plane, pal, d->bx, (u8)(d->by + y), _FRAME_V);
        ngpc_text_print(plane, pal, (u8)(d->bx + 1u), (u8)(d->by + y), line);
        _put_char(plane, pal, (u8)(d->bx + d->bw - 1u), (u8)(d->by + y), _FRAME_V);
    }

    {
        u8 i;
        for (i = 0u; i < inner_w; i++) line[i] = _FRAME_H;
        line[inner_w] = '\0';
    }
    _put_char(plane, pal, d->bx, (u8)(d->by + d->bh - 1u), _FRAME_BL);
    ngpc_text_print(plane, pal, (u8)(d->bx + 1u), (u8)(d->by + d->bh - 1u), line);
    _put_char(plane, pal, (u8)(d->bx + d->bw - 1u), (u8)(d->by + d->bh - 1u), _FRAME_BR);
}

static void _clear_inner(const NgpcDialog *d)
{
    char line[21];
    u8 i;
    u8 y;
    u8 pal;
    u8 plane;
    u8 inner_w;
    u8 inner_h;

    pal = ngpc_dialog_palette(d);
    plane = ngpc_dialog_plane(d);
    inner_w = (u8)(d->bw - 2u);
    inner_h = (u8)(d->bh - 2u);

    for (i = 0u; i < inner_w && i < 20u; i++) line[i] = _SPACE;
    line[(inner_w < 20u) ? inner_w : 20u] = '\0';

    for (y = 0u; y < inner_h; y++) {
        ngpc_text_print(plane, pal, (u8)(d->bx + 1u), (u8)(d->by + 1u + y), line);
    }
}

static void _draw_arrow(const NgpcDialog *d, u8 visible)
{
    char c = visible ? DIALOG_ARROW_CHAR : _SPACE;
    _put_char(ngpc_dialog_plane(d), ngpc_dialog_palette(d),
              (u8)(d->bx + d->bw - 2u),
              (u8)(d->by + d->bh - 2u), c);
}

/*
 * ==================== NGPCRAFT ENGINE COMPATIBILITY ====================
 * If you do not use NgpCraft-generated speaker/portrait data, you may
 * comment out or delete this extended layout block.
 * =======================================================================
 */

static u8 _speaker_len(const NgpcDialog *d)
{
    u8 len;
    const char *p;

    if (!d->speaker) return 0u;

    len = 0u;
    p = d->speaker;
    while (p[len] != '\0' && p[len] != DIALOG_SPEAKER_END_CHAR && len < 20u) len++;
    return len;
}

static u8 _next_word_len(const char *p)
{
    u8 len;

    len = 0u;
    while (p[len] != '\0' && p[len] != ' ' && p[len] != '\n' && len < 20u) len++;
    return len;
}

static void _calc_layout(const NgpcDialog *d, NgpcDialogLayout *lay)
{
    u8 inner_x;
    u8 inner_y;
    u8 inner_w;
    u8 inner_h;
    u8 portrait_cols;
    u8 speaker_rows;
    u8 choice_rows;
    u8 text_w;
    u8 text_h;

    inner_x = (u8)(d->bx + 1u);
    inner_y = (u8)(d->by + 1u);
    inner_w = (d->bw > 2u) ? (u8)(d->bw - 2u) : 1u;
    inner_h = (d->bh > 2u) ? (u8)(d->bh - 2u) : 1u;

    portrait_cols = 0u;
    if ((d->layout_flags & DIALOG_LAYOUT_PORTRAIT) && inner_w > 6u && inner_h > 4u) {
        portrait_cols = 4u;
    }

    speaker_rows = 0u;
    if (_speaker_len(d) > 0u && inner_h > 2u) {
        speaker_rows = 2u;
    }

    choice_rows = 0u;
    if (d->n_choices > 0u) {
        choice_rows = d->n_choices;
        if (choice_rows >= inner_h) choice_rows = (u8)(inner_h - 1u);
    }

    lay->speaker_x = (u8)(inner_x + portrait_cols);
    lay->speaker_y = inner_y;
    lay->text_x = lay->speaker_x;
    lay->text_y = (u8)(inner_y + speaker_rows);

    text_w = (inner_w > portrait_cols) ? (u8)(inner_w - portrait_cols) : 1u;
    text_h = (inner_h > speaker_rows) ? (u8)(inner_h - speaker_rows) : 1u;
    if (choice_rows < text_h) text_h = (u8)(text_h - choice_rows);
    else text_h = 1u;

    lay->max_col = (u8)(lay->text_x + text_w - 1u);
    lay->max_row = (u8)(lay->text_y + text_h - 1u);
}

static void _draw_speaker(const NgpcDialog *d, const NgpcDialogLayout *lay)
{
    char line[21];
    u8 i;
    u8 len;
    u8 width;

    len = _speaker_len(d);
    if (len == 0u) return;

    width = (u8)(lay->max_col - lay->speaker_x + 1u);
    if (len > width) len = width;

    for (i = 0u; i < len; i++) line[i] = d->speaker[i];
    line[len] = '\0';

    ngpc_text_print(ngpc_dialog_plane(d), ngpc_dialog_palette(d),
                    lay->speaker_x, lay->speaker_y, line);
}

static void _cursor_begin(const NgpcDialogLayout *lay, NgpcDialogCursor *cur)
{
    cur->row = lay->text_y;
    cur->col = lay->text_x;
    cur->need_word_check = 1u;
}

static void _consume_text_char(
        const NgpcDialog *d,
        const NgpcDialogLayout *lay,
        NgpcDialogCursor *cur,
        const char *p,
        u8 draw)
{
    u8 plane;
    u8 pal;
    char ch[2];

    plane = ngpc_dialog_plane(d);
    pal = ngpc_dialog_palette(d);
    ch[1] = '\0';

    if (*p == '\n') {
        cur->row++;
        cur->col = lay->text_x;
        cur->need_word_check = 1u;
        return;
    }

    if (*p == ' ') {
        u8 next_len;

        next_len = _next_word_len(p + 1);
        if (cur->col == lay->text_x) {
            cur->need_word_check = 1u;
            return;
        }
        if (next_len > 0u && (u16)cur->col + 1u + next_len - 1u > (u16)lay->max_col) {
            cur->row++;
            cur->col = lay->text_x;
            cur->need_word_check = 1u;
            return;
        }
        if (draw && cur->row <= lay->max_row && cur->col <= lay->max_col) {
            ch[0] = ' ';
            ngpc_text_print(plane, pal, cur->col, cur->row, ch);
        }
        cur->col++;
        if (cur->col > lay->max_col) {
            cur->col = lay->text_x;
            cur->row++;
        }
        cur->need_word_check = 1u;
        return;
    }

    if (cur->need_word_check && cur->col != lay->text_x) {
        u8 word_len;

        word_len = _next_word_len(p);
        if ((u16)cur->col + word_len - 1u > (u16)lay->max_col) {
            cur->row++;
            cur->col = lay->text_x;
        }
    }

    if (draw && cur->row <= lay->max_row && cur->col <= lay->max_col) {
        ch[0] = *p;
        ngpc_text_print(plane, pal, cur->col, cur->row, ch);
    }
    cur->col++;
    if (cur->col > lay->max_col) {
        cur->col = lay->text_x;
        cur->row++;
    }
    cur->need_word_check = 0u;
}

static void _draw_next_char(const NgpcDialog *d)
{
    NgpcDialogLayout lay;
    NgpcDialogCursor cur;
    const char *p;
    u8 i;

    if (!d->text || d->text[d->char_idx] == '\0') return;

    _calc_layout(d, &lay);
    _cursor_begin(&lay, &cur);

    p = d->text;
    for (i = 0u; i < d->char_idx && p[i] != '\0'; i++) {
        _consume_text_char(d, &lay, &cur, &p[i], 0u);
    }
    _consume_text_char(d, &lay, &cur, &p[d->char_idx], 1u);
}

static void _redraw_text(const NgpcDialog *d)
{
    NgpcDialogLayout lay;
    NgpcDialogCursor cur;
    const char *p;
    u8 i;

    _calc_layout(d, &lay);
    _draw_speaker(d, &lay);

    if (!d->text) return;

    _cursor_begin(&lay, &cur);
    p = d->text;
    for (i = 0u; i < d->char_idx && p[i] != '\0'; i++) {
        _consume_text_char(d, &lay, &cur, &p[i], 1u);
    }
}

static void _draw_choices(const NgpcDialog *d)
{
    NgpcDialogLayout lay;
    u8 pal;
    u8 plane;
    u8 i;
    u8 ty;

    _calc_layout(d, &lay);
    pal = ngpc_dialog_palette(d);
    plane = ngpc_dialog_plane(d);
    ty = (u8)(lay.max_row + 1u);

    for (i = 0u; i < d->n_choices; i++) {
        char prefix[2];
        prefix[0] = (i == d->cursor) ? '>' : ' ';
        prefix[1] = '\0';
        ngpc_text_print(plane, pal, lay.text_x, (u8)(ty + i), prefix);
        ngpc_text_print(plane, pal, (u8)(lay.text_x + 1u), (u8)(ty + i), d->choices[i]);
    }
}

void ngpc_dialog_open(NgpcDialog *d,
                      u8 plane, u8 bx, u8 by, u8 bw, u8 bh, u8 pal)
{
    d->text = 0;
    d->speaker = 0;
    d->choices = 0;
    d->plane = (u8)((plane & 0x03u) | ((pal & 0x0Fu) << 2u));
    d->bx = bx;
    d->by = by;
    d->bw = bw;
    d->bh = bh;
    d->char_idx = 0u;
    d->tick = 0u;
    d->blink = 0u;
    d->cursor = 0u;
    d->n_choices = 0u;
    d->layout_flags = 0u;
    d->flags = _DLG_OPEN;

#ifndef DIALOG_NO_FRAME
    _draw_frame(d);
#endif
}

void ngpc_dialog_close(NgpcDialog *d)
{
    char line[21];
    u8 i;
    u8 y;
    u8 pal;
    u8 plane;

    pal = ngpc_dialog_palette(d);
    plane = ngpc_dialog_plane(d);
    for (i = 0u; i < d->bw && i < 20u; i++) line[i] = _SPACE;
    line[(d->bw < 20u) ? d->bw : 20u] = '\0';
    for (y = 0u; y < d->bh; y++) {
        ngpc_text_print(plane, pal, d->bx, (u8)(d->by + y), line);
    }
    d->text = 0;
    d->speaker = 0;
    d->layout_flags = 0u;
    d->flags = 0u;
}

/*
 * ==================== NGPCRAFT ENGINE COMPATIBILITY ====================
 * This parser understands encoded speaker strings:
 *   "\001Speaker\002Text"
 *
 * If your standalone project only uses plain text, you may comment out or
 * delete this parsing block and keep text = body directly.
 * =======================================================================
 */
void ngpc_dialog_set_text(NgpcDialog *d, const char *text)
{
    const char *body;
    const char *sep;

    body = text;
    d->speaker = 0;

    if (text && text[0] == DIALOG_SPEAKER_BEGIN_CHAR) {
        sep = text + 1;
        while (*sep != '\0' && *sep != DIALOG_SPEAKER_END_CHAR) sep++;
        if (*sep == DIALOG_SPEAKER_END_CHAR) {
            d->speaker = text + 1;
            body = sep + 1;
        }
    }

    d->text = body;
    d->char_idx = 0u;
    d->tick = 0u;
    d->blink = 0u;
    d->flags &= (u8)~(_DLG_TEXT_DONE | _DLG_HAS_CHOICES);
    d->n_choices = 0u;
    _clear_inner(d);
    _redraw_text(d);
}

void ngpc_dialog_set_layout(NgpcDialog *d, u8 flags)
{
    d->layout_flags = flags;
}

void ngpc_dialog_set_choices(NgpcDialog *d, const char **choices, u8 count)
{
    if (count > DIALOG_MAX_CHOICES) count = DIALOG_MAX_CHOICES;
    d->choices = choices;
    d->n_choices = count;
    d->cursor = 0u;
    if (count > 0u) d->flags |= _DLG_HAS_CHOICES;
}

u8 ngpc_dialog_update(NgpcDialog *d)
{
    if (!(d->flags & _DLG_OPEN)) return DIALOG_DONE;

    if (!(d->flags & _DLG_TEXT_DONE)) {
        if (d->text == 0 || d->text[d->char_idx] == '\0') {
            d->flags |= _DLG_TEXT_DONE;
            if (d->flags & _DLG_HAS_CHOICES) _draw_choices(d);
        } else {
            if (ngpc_pad_pressed & PAD_A) {
                while (d->text[d->char_idx] != '\0') d->char_idx++;
                d->flags |= _DLG_TEXT_DONE;
                _clear_inner(d);
                _redraw_text(d);
                if (d->flags & _DLG_HAS_CHOICES) _draw_choices(d);
                return DIALOG_RUNNING;
            }

            d->tick++;
            if (d->tick >= DIALOG_TEXT_SPEED) {
                d->tick = 0u;
                _draw_next_char(d);
                d->char_idx++;
                if (d->text[d->char_idx] == '\0') {
                    d->flags |= _DLG_TEXT_DONE;
                    if (d->flags & _DLG_HAS_CHOICES) _draw_choices(d);
                }
            }
        }
        return DIALOG_RUNNING;
    }

    if (d->flags & _DLG_HAS_CHOICES) {
        u8 changed;

        changed = 0u;
        if ((ngpc_pad_pressed & PAD_UP) && d->cursor > 0u) {
            d->cursor--;
            changed = 1u;
        }
        if ((ngpc_pad_pressed & PAD_DOWN) && d->cursor < (u8)(d->n_choices - 1u)) {
            d->cursor++;
            changed = 1u;
        }
        if (changed) _draw_choices(d);

        if (ngpc_pad_pressed & PAD_A) {
            u8 sel;
            sel = d->cursor;
            ngpc_dialog_close(d);
            return (u8)(DIALOG_CHOICE_0 + sel);
        }
        return DIALOG_RUNNING;
    }

    d->blink++;
    if (d->blink >= DIALOG_BLINK_PERIOD) d->blink = 0u;
    _draw_arrow(d, (u8)(d->blink < (DIALOG_BLINK_PERIOD / 2u)));

    if (ngpc_pad_pressed & PAD_A) {
        ngpc_dialog_close(d);
        return DIALOG_DONE;
    }
    return DIALOG_RUNNING;
}
