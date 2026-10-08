#include "id_fleet.h"
#include "../GraphX/id_carog.h"
void id_fleet_body(u8 index,IdBody *out) {
(void)index;

        out->tiles = id_carog_tiles;
        out->tiles_count = id_carog_tiles_count;
        out->race_tiles = (u16)(ID_CAROG_RACE_CHARS * 8);
        out->palette = id_carog_palette;
        out->pose_cells = id_carog_pose_cells;
        out->pose_first = id_carog_pose_first;
        out->pose_w = id_carog_pose_w;
        out->cells = id_carog_cells;
        out->layers = (u8)ID_CAROG_LAYERS;
        out->poses = (u8)ID_CAROG_POSES;
        out->leans_right = 1u;
        out->height = (u8)ID_CAROG_HEIGHT;

}
