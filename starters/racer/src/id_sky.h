#ifndef ID_SKY_H
#define ID_SKY_H
#include "ngpc_types.h"
#include "../GraphX/id_cypres83.h"
#define ID_SKY_CYPRES 0u
#define ID_SKY_NONE 255u
#define ID_SKY_IS_TALL(b) ((b)==0u)
#define ID_SKY_TALL_ROWS ID_CYPRES83_ROWS
u16 id_sky_char_count(u8 band);
void id_sky_load(u8 band,u16 base);
void id_sky_draw(u8 band);
u16 id_sky_backdrop(u8 band);
#endif
