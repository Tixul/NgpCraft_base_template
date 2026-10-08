/* MIT. 2x2 modules per tile -> 16 combinations, no framebuffer. */
#include "ngpc_qr_draw.h"
#include "ngpc_gfx.h"

static u8 module(const NgpcQr *qr, u8 x, u8 y)
{
    if (x < 4u || y < 4u) return 0u;
    return ngpc_qr_get_module(qr, (u8)(x - 4u), (u8)(y - 4u));
}

u8 ngpc_qr_draw(const NgpcQr *qr, u8 plane, u16 tile_base, u8 palette,
                u8 x, u8 y)
{
    u8 pattern, row, tx, ty, top, bottom;
    u16 word;
    volatile u16 *tile;
    if (!qr || !qr->valid || plane > (u8)GFX_SCR2 || tile_base > 496u ||
        palette > 15u || x > 20u - NGPC_QR_MAP_SIZE ||
        y > 19u - NGPC_QR_MAP_SIZE) return 0u;
    for (pattern = 0u; pattern < 16u; pattern++) {
        tile = (volatile u16 *)(0xA000u + (u32)(tile_base + pattern) * 16u);
        for (row = 0u; row < 8u; row++) {
            top = (u8)((row < 4u) ? pattern : pattern >> 2);
            word = (u16)(((top & 1u) ? 0xAA00u : 0x5500u) |
                         ((top & 2u) ? 0x00AAu : 0x0055u));
            tile[row] = word;
        }
    }
    ngpc_gfx_set_palette(plane, palette, 0x0FFFu, 0x0FFFu, 0x0000u, 0x0000u);
    for (ty = 0u; ty < NGPC_QR_MAP_SIZE; ty++) {
        for (tx = 0u; tx < NGPC_QR_MAP_SIZE; tx++) {
            top = (u8)(module(qr, (u8)(tx * 2u), (u8)(ty * 2u)) |
                       (module(qr, (u8)(tx * 2u + 1u), (u8)(ty * 2u)) << 1));
            bottom = (u8)(module(qr, (u8)(tx * 2u), (u8)(ty * 2u + 1u)) |
                          (module(qr, (u8)(tx * 2u + 1u), (u8)(ty * 2u + 1u)) << 1));
            ngpc_gfx_put_tile(plane, (u8)(x + tx), (u8)(y + ty),
                             (u16)(tile_base + top + (bottom << 2)), palette);
        }
    }
    return 1u;
}
