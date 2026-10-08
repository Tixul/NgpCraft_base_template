
#ifndef ID_FLEET_H
#define ID_FLEET_H
#include "ngpc_types.h"
#ifndef ID_CAR_CELL_DEFINED
#define ID_CAR_CELL_DEFINED
typedef struct { u8 cx, cy, ch; } IdCarCell;
#endif
#define ID_BODY_COUNT 1u
#define ID_BODY_ORIG 0u
#define ID_FLEET_POSES   3u
typedef struct {
    const u16 *tiles;
    u16 tiles_count;            
    const u16 *palette;         
    const u8  *pose_cells;      
    const u8  *pose_first;
    const u8  *pose_w;
    const IdCarCell *cells;
    u16 race_tiles;
    u8 layers;
    u8 poses;
    u8 height;
    u8 leans_right;
} IdBody;
void id_fleet_body(u8 index, IdBody *out);
#endif 

