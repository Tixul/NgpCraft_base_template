/* Optional screen example; call after ngpc_init() from a stable menu.
 * Add build/obj/examples/qr_example.rel plus both QR module objects.
 * Caller MUST stop its raster/DMA and restore its own screen when we return.
 * No game-specific URL, record format, save or server is embedded here. */
#include "../optional/ngpc_qr/ngpc_qr_draw.h"
#include "ngpc_hw.h"
#include "ngpc_gfx.h"
#include "ngpc_sprite.h"
#include "ngpc_sys.h"
#include "ngpc_timing.h"
#include "ngpc_input.h"

static NgpcQr example_qr;

u8 qr_example_show(const char *text)
{
    u8 armed;
    if (!ngpc_qr_encode(&example_qr, text)) return 0u;
    ngpc_sprite_hide_all();
    ngpc_gfx_clear(GFX_SCR1);
    ngpc_gfx_clear(GFX_SCR2);
    ngpc_gfx_scroll(GFX_SCR1, 0u, 0u);
    ngpc_gfx_scroll(GFX_SCR2, 0u, 0u);
    ngpc_gfx_set_viewport(0u, 0u, 160u, 152u);
    ngpc_gfx_set_bg_color(0x0FFFu);
    if (!ngpc_qr_draw(&example_qr, GFX_SCR1, 128u, 0u,
                      (u8)((20u - NGPC_QR_MAP_SIZE) / 2u),
                      (u8)((19u - NGPC_QR_MAP_SIZE) / 2u))) return 0u;
    armed = 0u;
    for (;;) {
        ngpc_vsync();
        ngpc_input_update();
        /* Add your sound/link pumping here if your game needs it. */
        if (!(ngpc_pad_held & (PAD_A | PAD_B | PAD_OPTION))) armed = 1u;
        if (armed && (ngpc_pad_pressed & (PAD_A | PAD_B | PAD_OPTION))) break;
    }
    return 1u;
}
