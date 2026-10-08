
#ifndef ID_ROAD_H
#define ID_ROAD_H
#include "ngpc_types.h"
#define ID_TILE_BASE   128u
#define ID_TILE_BLANK  ID_TILE_BASE
void id_road_init(void);
#define ID_ROAD_COURSES  1u
void id_road_select(u8 course);
void id_road_join(u8 course);
void id_road_join_white(u8 white);
u8   id_road_course(void);
u16  id_road_char_count(void);
u8   id_road_horizon(void);
u8   id_road_top(void);
u8   id_road_bottom(void);
void id_road_flat(void);
u8 id_road_dma_state(void);
u16 id_road_xy_at(u8 line);
void id_road_dma_pause(void);
void id_road_dma_resume(void);
void id_road_rearm(void);
void id_road_reload(void);
void id_road_restore_map(void);
void id_road_reload_backdrop(void);
void id_road_frame(s16 lateral, s16 curve, s16 heading);
#define ID_ROAD_LINES_MAX  64u
const u16 *id_road_ahead(void);
u8 id_road_lines(void);
void id_road_frame_ahead(s16 lateral, const s16 *bend, s16 heading);
s16 id_road_centre_at(u8 line);
u8 id_road_half_at(u8 line);
void id_road_set_hill(s8 grade);
void id_road_hill_ahead(const s8 *grade);
s8 *id_road_grade_buf(void);
void id_road_band_phase(u16 travelled);
void id_road_pan(s16 heading16, s16 lateral16);  
#if ID_PROBE
u8 id_road_pan_seen(void);
u8 id_road_pan_mode(void);
#endif
void id_road_begin(void);       
u8 id_road_band_first(void);
#if ID_PROBE
u16 id_road_rate_sum(void);     
#endif
s8 id_road_grade_hi(void);
s8 id_road_grade_lo(void);
s8 id_road_line_shift(u8 line);
typedef struct {
    s16 centre;
    s16 foot;
    u8 half;
} IdRoadSpriteSample;
void id_road_sprite_sample(u8 line, IdRoadSpriteSample *out);
u8 id_road_visible_top(void);
s8 id_road_car_lift(void);
#define ID_ROAD_SWEEP_MAX  88
#endif 

