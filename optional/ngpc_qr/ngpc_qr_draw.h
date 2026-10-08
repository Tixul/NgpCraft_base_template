#ifndef NGPC_QR_DRAW_H
#define NGPC_QR_DRAW_H
#include "ngpc_qr.h"
/* Draw at tile coordinates x/y on GFX_SCR1 or GFX_SCR2. Four pixels/module.
 * Writes exactly 16 character slots starting at tile_base (0..496), one
 * palette (0..15), and MAP_SIZE squared map words including quiet zones.
 * Returns 0 BEFORE any VRAM writes for invalid arguments/context.
 * Caller owns sprites, raster/DMA, scroll, viewport, visibility and restoration.
 * No clears, input loop, palette animation, vsync or game state changes.
 * Uses opaque white/black. Select an otherwise unused palette and tile range.
 * Entire rectangle must fit the visible 20x19 tile screen, no wrapping.
 */
u8 ngpc_qr_draw(const NgpcQr *qr, u8 plane, u16 tile_base, u8 palette,
                u8 x, u8 y);
#endif
