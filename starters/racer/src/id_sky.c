#include "ngpc_hw.h"
#include "ngpc_gfx.h"
#include "id_sky.h"
static u16 sky_base;
u16 id_sky_char_count(u8 band) { (void)band; return ID_CYPRES83_CHAR_COUNT; }
u16 id_sky_backdrop(u8 band) { (void)band; return id_cypres83_palettes[0]; }
void id_sky_load(u8 band,u16 base) {
    u16 i; u8 p; (void)band; sky_base=base;
    for(i=0u;i<ID_CYPRES83_CHAR_COUNT*8u;i++)
        ((volatile u16 *)(0xA000u+(u32)base*16u))[i]=id_cypres83_tiles[i];
    for(p=0u;p<8u;p++) ngpc_gfx_set_palette(GFX_SCR2,(u8)(8u+p),
        id_cypres83_palettes[p*4u],id_cypres83_palettes[p*4u+1u],
        id_cypres83_palettes[p*4u+2u],id_cypres83_palettes[p*4u+3u]);
}
void id_sky_draw(u8 band) {
    u8 x,y; (void)band;
    for(y=0u;y<ID_CYPRES83_ROWS;y++) for(x=0u;x<32u;x++) {
        u16 i=(u16)y*32u+x;
        HW_SCR2_MAP[i]=(u16)(sky_base+id_cypres83_map[i]
            +((u16)(8u+id_cypres83_map_pal[i])<<9));
    }
}
