

#ifndef ID_SCENERY_H
#define ID_SCENERY_H

#include "ngpc_types.h"


#define ID_SCN_SPR_0   20u

#define ID_SCN_SPR_MAX 32u


void id_scenery_init(u16 tile_base, u8 course);


u16 id_scenery_char_count(void);


void id_scenery_frame(u8 speed);


u8 id_scenery_passed(void);

#if ID_PROBE

u8 id_scenery_hidden(void);


u8 id_scenery_highest(void);
#endif

#endif 


