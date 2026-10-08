/* Scanline road renderer extracted from OVER REV. */

#include "ngpc_hw.h"
#include "ngpc_gfx.h"
#include "ngpc_sys.h"
#include "ngpc_dma.h"
#include "ngpc_dma_raster.h"
#include "id_road.h"
#include "id_panel.h"
#include "id_sky.h"
#include "../GraphX/id_race.h"
#include "../GraphX/id_route.h"
#define RALLY_ROW_STEP    2u    
#define RALLY_BASE_X     48u    
#define RALLY_SHEAR_MAX  48     
#define RALLY_PLANE0     56u
#define RALLY_TOP        56u
#define RALLY_LINES      (ID_PANEL_TOP - RALLY_TOP)
#define RALLY_SKY_SPLIT  40u
#define ROAD_TIP_HIDE     3u
typedef struct {
    const u16 *tiles;
    const u16 *map;
    const u8  *map_pal;
    const u16 *palettes;
    const u8  *road_half;
    u8 w;                       
    u8 road_top;                
    u8 road_bottom;             
} CourseArt;
static void course_art(u8 course, CourseArt *out)
{
    (void)course;
    out->tiles = id_route_tiles;
    out->map = id_route_map;
    out->map_pal = id_route_map_pal;
    out->palettes = id_route_palettes;
    out->road_half = id_route_road_half;
    out->w = (u8)ID_ROUTE_W;
    out->road_top = (u8)ID_ROUTE_ROAD_TOP;
    out->road_bottom = (u8)ID_ROUTE_ROAD_BOTTOM;
}
static CourseArt art;
static u8 road_course;
static u8 uses_akina(void)
{
    return 0u;
}
static s8 road_grade;
static s8 band_pitch[2];        
static const u8 hill_rate[41] = {
     64u,  65u,  67u,  69u,  71u,  73u,  75u,  77u,
     80u,  82u,  85u,  88u,  91u,  94u,  98u, 102u,
    106u, 111u, 116u, 121u, 128u, 134u, 140u, 147u,
    153u, 160u, 166u, 172u, 179u, 185u, 192u, 198u,
    204u, 211u, 217u, 224u, 230u, 236u, 243u, 249u,
    255u
};
#define CAR_STAND_DIV   6u
static u8 car_stand = (u8)(ID_ROAD_LINES / CAR_STAND_DIV);
static s8 grade_hi = 6;         
static s8 grade_lo = -6;        
static u8 climb_room;
static u8 descent_room;
static s16 band_shift;
static u8  band_first = (u8)ID_ROAD_TOP;
static u8  band_count = (u8)ID_ROAD_LINES;
static u8  band_bg_end[2] = { (u8)ID_ROAD_TOP, (u8)ID_ROAD_TOP };
static u8 road_top = (u8)ID_ROAD_TOP;
static u8 road_plane0 = (u8)ID_ROAD_TOP;
static u8 road_bottom = (u8)ID_ROAD_BOTTOM;
static u8 road_lines = (u8)ID_ROAD_LINES;
#define SOL_DE_LA_PLANCHE  0xFFFFu
typedef struct {
    u16 pal[4];
    u16 verge;
    u16 bande[4];
} CourseSol;
static const CourseSol course_sol[1] = {
    { { 0x0052u, 0x0333u, 0x0555u, 0x0FFFu }, SOL_DE_LA_PLANCHE,
      { 0x0333u, 0x0333u, 0x0666u, 0x0999u } }
};

static const u8 course_horizon[1] = { ID_SKY_CYPRES };
u8 id_road_horizon(void)
{
    return course_horizon[road_course];
}
static u8 sky_is_tall_course(void)
{
    return (u8)ID_SKY_IS_TALL(course_horizon[road_course]);
}
static u8 road_is_tarmac(void)
{
    return 1u;
}
#define RALLY_PLANE0_TALL  88u
static u8 road_base_x = (u8)ID_BASE_X;
#define ID_TILE_BASE   128u
#define SKY_SHIFT        2u
#define MOUNTAIN_SHIFT   1u
#define TREE_SHIFT       0u
#define TREE_LATERAL_SHIFT  3u
#define AKINA_TREE_SPLIT   48u
static u16 id_xy_a[152];
static u16 id_xy_b[152];
static u16 *id_xy = id_xy_a;            
static const u16 *id_xy_live = id_xy_b; 
static u16 *id_xy_done;                 
static u16 *id_xy_last;                 
static NgpcDmaRasterXY id_raster;
static s16 id_shear[ID_ROAD_LINES_MAX]; 
typedef struct {
    u8 line_of[ID_ROAD_LINES_MAX];  
    s8 grade[ID_ROAD_LINES_MAX];    
    u8 bandi[ID_ROAD_LINES_MAX];    
} RoadSlices;
typedef char road_lines_max_is_64[(ID_ROAD_LINES_MAX == 64u) ? 1 : -1];
typedef char road_slices_is_192[(sizeof(RoadSlices) == 192u) ? 1 : -1];
static RoadSlices id_sl;
#define id_line_of (id_sl.line_of)
#define id_grade   (id_sl.grade)
#define id_bandi   (id_sl.bandi)
static const u8 band_lut[16][48] = {
    3u, 2u, 1u, 1u, 2u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 1u,
    2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 2u, 1u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u,
    1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 2u, 1u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u,
    0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 2u, 1u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u,
    0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    0u, 3u, 3u, 2u, 1u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u,
    0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u,
    0u, 3u, 3u, 2u, 1u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u,
    0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u,
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u,
    0u, 3u, 3u, 2u, 1u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 2u, 1u,
    0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u,
    0u, 3u, 3u, 2u, 1u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 2u,
    1u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u,
    1u, 2u, 3u, 3u, 2u, 1u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u,
    2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    1u, 2u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u,
    3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 1u, 2u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u,
    3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u,
    3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    3u, 0u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u, 3u,
    3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u,
    3u, 0u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u,
    3u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 2u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u,
    2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u,
    3u, 0u, 1u, 2u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 2u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    1u, 2u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 2u, 1u,
};
static u16 id_bandcol_a[152];
static u16 id_bandcol_b[152];
static u16 bc_rest[2] = { 0xFFFFu, 0xFFFFu };
static u8  bc_top[2];
static u16 *id_bandcol = id_bandcol_a;
static u16 *id_bandcol_done;
static NgpcDmaU16Stream id_bandstream;
static u16 band_col[4];
static u16 band_phase;              
#define BAND_RATE   2u
static u8  road_band_pal = 2u;      
typedef struct {
    s16 min[ID_ROAD_LINES_MAX];
    s16 max[ID_ROAD_LINES_MAX];
} RoadClamp;
typedef char road_clamp_is_256[(sizeof(RoadClamp) == 256u) ? 1 : -1];
static RoadClamp id_dxc;
#define id_dx_min (id_dxc.min)
#define id_dx_max (id_dxc.max)
#define RALLY_SRC_LINES (ID_RALLY_ROAD_BOTTOM - ID_RALLY_ROAD_TOP + 1)
#define ROW_0           (u8)(art.road_top / 8u)
#define ROW_N           (u8)(art.road_bottom / 8u)
#define LAST_ROW ROW_N
#define RALLY_ROW_0     (ID_RALLY_ROAD_TOP / 8)
#define RALLY_ROW_N     (ID_RALLY_ROAD_BOTTOM / 8)
#define RALLY_LAST_ROW  (RALLY_ROW_N - 1)
static u8 id_w_lat[ID_ROAD_LINES_MAX];      
static u8 id_w_curve[ID_ROAD_LINES_MAX];    
static u8 id_half[ID_ROAD_LINES_MAX];
static u8 id_src[ID_ROAD_LINES_MAX];
static u16 band_top[2], band_mid[2], band_tree[2];
static u8  band_bg_first[2];
static u8 id_buf;               
static s16 pan_head, pan_lat;
static u8  pan_fine;
#if ID_PROBE
static u8 pan_seen, pan_mode;
u8 id_road_pan_seen(void)
{
    return pan_seen;
}
u8 id_road_pan_mode(void)
{
    return pan_mode;
}
#endif
#define BAND_FADE  24u   
#define BAND_HOLD  16u          
static u8  band_held;           
static u8  band_wait;           
#if ID_PROBE
static u16 rate_sum;            
#endif
static void id_fill_hud_band(void);
#define ROAD_LOOK_NEAR   60u
#define ROAD_LOOK_MAX  1600u    
static u16 id_ahead[ID_ROAD_LINES_MAX];
static u16 id_curve_cm;
static void id_build_weights(void)
{
    u8 k;
    u16 den = (u16)(road_lines - 1u);
    {
        u16 lines2 = (u16)((u16)road_lines * (u16)(road_lines + 1u));
        id_curve_cm = (u16)((u16)((32768u + (lines2 >> 1)) / lines2) * 4u);
    }
    for (k = 0u; k < road_lines; k++) {
        u16 depth;      
        if (!uses_akina()) {
            u16 span = (u16)(((u16)(LAST_ROW + 1u) * 8u) - 1u
                             - (u16)art.road_top);
            u16 j = (u16)(((u16)span * (u16)k) / (u16)den);
            if (j > span) {
                j = span;
            }
            id_src[k] = (u8)j;
            id_half[k] = art.road_half[j];
            depth = (u16)(((u16)j * 255u) / span);
        } else {
            id_src[k] = k;
            id_half[k] = art.road_half[k];
            depth = (u16)(((u16)k * 255u) / den);
        }
        {
            u16 far = (u16)(255u - depth);
            id_line_of[k] = (u8)(road_top + k);
            id_w_lat[k] = (u8)depth;
            id_w_curve[k] = (u8)(((u16)(far * far)) >> 8);
            if (depth == 0u) {
                id_ahead[k] = (u16)ROAD_LOOK_MAX;
            } else {
                u16 d = (u16)(((u16)ROAD_LOOK_NEAR * far) / depth);
                id_ahead[k] = (d > (u16)ROAD_LOOK_MAX) ? (u16)ROAD_LOOK_MAX : d;
            }
            {
                u16 a = (u16)(((u16)k * 47u) / (u16)(road_lines - 1u));
                u16 b = (u16)(47u - (u16)(((u32)id_ahead[k] * 47u)
                                          / (u32)ROAD_LOOK_MAX));
                u16 j = (u16)((a + b) >> 1);
                j = (u16)((j * 2u) % 48u);
                id_bandi[k] = (u8)((j > 47u) ? 47u : j);
            }
        }
    }
    {
        s16 g;
        climb_room = (u8)(road_top - 8u);
        if (uses_akina()) {
            descent_room = (u8)(road_lines / 10u);
        } else {
            descent_room = (u8)(road_lines / 2u);
        }
        grade_hi = 0;
        grade_lo = 0;
        for (g = 1; g <= 20; g++) {
            u16 up = (u16)(((u16)road_lines * (u16)hill_rate[20 + g]) >> 7);
            if (up <= (u16)(road_lines + climb_room)) {
                grade_hi = (s8)g;
            }
        }
        for (g = 1; g <= 20; g++) {
            u16 dn = (u16)(((u16)road_lines * (u16)hill_rate[20 - g]) >> 7);
            if (dn + (u16)descent_room >= (u16)road_lines) {
                grade_lo = (s8)(-g);
            }
        }
    }
}
#define RALLY_SRC_MAX   512u
#define RALLY_SLOTS     255u
static u8  rally_of[RALLY_SRC_MAX];     
static u16 rally_src[RALLY_SLOTS];      
static u8  rally_used;
static u8  rally_scanned;
static void rally_scan(void)
{
    u16 i;
    u8 x, y;
    if (rally_scanned != 0u) {
        return;
    }
    for (i = 0u; i < RALLY_SRC_MAX; i++) {
        rally_of[i] = 255u;
    }
    rally_used = 0u;
    for (y = ROW_0; y <= LAST_ROW; y++) {
        for (x = 0u; x < art.w; x++) {
            u16 ch = art.map[(u16)y * (u16)art.w + (u16)x];
            if (ch >= RALLY_SRC_MAX || rally_of[ch] != 255u) {
                continue;
            }
            if (rally_used >= (u8)RALLY_SLOTS) {
                continue;
            }
            rally_of[ch] = rally_used;
            rally_src[rally_used] = ch;
            rally_used++;
        }
    }
    rally_scanned = 1u;
}
u16 id_road_char_count(void) { rally_scan(); return (u16)rally_used; }
static void id_load_chars(void)
{
    u16 tile;
    u16 count = id_road_char_count();
    const u16 *src = art.tiles;
    for (tile = 0u; tile < count; tile++) {
        volatile u8 *dst =
            (volatile u8 *)(0xA000u + (u32)(ID_TILE_BASE + tile) * 16u);
        u16 from = (!uses_akina()) ? rally_src[tile] : tile;
        u16 w;
        for (w = 0u; w < 8u; w++) {
            u16 v = src[(u16)(from * 8u + w)];
            dst[(u16)(w * 2u) + 0u] = (u8)(v & 0xFFu);
            dst[(u16)(w * 2u) + 1u] = (u8)(v >> 8);
        }
    }
}
static void id_load_palettes(void) {
    u8 i;
    for (i=0u;i<8u;i++) ngpc_gfx_set_palette(GFX_SCR2,i,
        art.palettes[i*4u],art.palettes[i*4u+1u],art.palettes[i*4u+2u],art.palettes[i*4u+3u]);
    ngpc_gfx_set_palette(GFX_SCR2,0u,0x0052u,0x0333u,0x0555u,0x0FFFu);
    ngpc_gfx_set_bg_color(art.palettes[0]);
}
void id_road_reload_backdrop(void) { ngpc_gfx_set_bg_color(art.palettes[0]); }
#define RALLY_MAP_X     6u      
#define RALLY_MAP_Y     ((s8)((s8)(RALLY_PLANE0 / 8) - (s8)ROW_0))
static void id_load_maps_rally(void)
{
    u8 x, y;
    {
    s8 map_y = (s8)((s8)(road_plane0 / 8u) - (s8)ROW_0);
    rally_scan();
    for (y = (u8)(road_plane0 / 8u); y < 32u; y++) {
        u8 sy = (u8)((s16)y - (s16)map_y);
        if (sy > LAST_ROW) {
            sy = LAST_ROW;
        }
        for (x = 0u; x < 32u; x++) {
            u8 sx = 0u;
            u8 row = sy;
            u16 k, ch;
            u8 slot;
            if (x < RALLY_MAP_X) {
                row = ROW_0;
            } else if (x >= (u8)(RALLY_MAP_X + art.w)) {
                sx = (u8)(art.w - 1u);
                row = ROW_0;
            } else {
                sx = (u8)(x - RALLY_MAP_X);
            }
            k = (u16)row * (u16)art.w + (u16)sx;
            ch = art.map[k];
            slot = (ch < RALLY_SRC_MAX) ? rally_of[ch] : 255u;
            if (slot == 255u) {
                slot = 0u;      
            }
            HW_SCR2_MAP[(u16)y * 32u + (u16)x] =
                (u16)(ID_TILE_BASE + (u16)slot +
                      ((u16)(art.map_pal[k] & 0x07u) << 9));
        }
        
    }
    }
}
static void id_load_maps(void) { id_load_maps_rally(); }
static void id_build_shear_limits(void)
{
    u8 k;
    if (!uses_akina()) {
        for (k = 0u; k < road_lines; k++) {
            id_dx_min[k] = (s16)-ID_ROAD_SWEEP_MAX;
            id_dx_max[k] = (s16)ID_ROAD_SWEEP_MAX;
        }
        return;
    }
    for (k = 0u; k < road_lines; k++) {
        u16 y = (u16)(road_top + k);
        u8 col;
        u16 lo = 999u, hi = 0u;
        for (col = 0u; col < 32u; col++) {
            u16 ch = 0u;
            u16 row = 0u;
            u8 p;
            if (row == 0u) {
                continue;
            }
            for (p = 0u; p < 8u; p++) {
                if ((row >> (14u - 2u * p)) & 3u) {
                    u16 x = (u16)(col * 8u + p);
                    if (x < lo) {
                        lo = x;
                    }
                    if (x > hi) {
                        hi = x;
                    }
                }
            }
        }
        if (lo > hi) {
            id_dx_min[k] = (s16)-127;
            id_dx_max[k] = (s16)127;
        } else {
            id_dx_min[k] = (s16)((s16)hi - 255 - ID_BASE_X);
            id_dx_max[k] = (s16)((s16)lo + 97 - ID_BASE_X);
        }
        if (id_dx_max[k] > (s16)ID_ROAD_SWEEP_MAX) {
            id_dx_max[k] = (s16)ID_ROAD_SWEEP_MAX;
        }
        if (id_dx_min[k] < (s16)-ID_ROAD_SWEEP_MAX) {
            id_dx_min[k] = (s16)-ID_ROAD_SWEEP_MAX;
        }
    }
}
u8 id_road_course(void)
{
    return road_course;
}
u8 id_road_top(void)
{
    return road_top;
}
u8 id_road_bottom(void)
{
    return road_bottom;
}
void id_road_select(u8 course)
{
    u8 k;
    road_course = (course < (u8)ID_ROAD_COURSES) ? course : 0u;
    ngpc_gfx_set_bg_color(0x0000u);
    if (!uses_akina()) {
        course_art(road_course, &art);
        rally_scanned = 0u;     
    }
    if (!uses_akina()) {
        road_base_x = (u8)RALLY_BASE_X;
        road_top = (u8)RALLY_TOP;
        road_plane0 = sky_is_tall_course() ? (u8)RALLY_PLANE0_TALL
                                          : (u8)RALLY_PLANE0;
        road_lines = (u8)RALLY_LINES;
    } else {
        road_base_x = (u8)ID_BASE_X;
        road_top = (u8)ID_ROAD_TOP;
        road_plane0 = (u8)ID_ROAD_TOP;
        road_lines = (u8)ID_ROAD_LINES;
    }
    road_bottom = (u8)(road_top + road_lines - 1u);
    car_stand = (u8)(road_lines / CAR_STAND_DIV);
    if (car_stand >= road_lines) {
        car_stand = (u8)(road_lines - 1u);
    }
    id_load_chars();
    id_load_palettes();
    if (!uses_akina()) {
        id_load_maps_rally();
    } else {
        id_load_maps();
    }
    for (k = 0u; k < road_lines; k++) {
        id_shear[k] = 0;
    }
    band_top[0] = 0xFFFFu; band_top[1] = 0xFFFFu;
    band_mid[0] = 0xFFFFu; band_mid[1] = 0xFFFFu;
    band_tree[0] = 0xFFFFu; band_tree[1] = 0xFFFFu;
    band_bg_first[0] = 0xFFu; band_bg_first[1] = 0xFFu;
    road_band_pal = uses_akina() ? 3u : 0u;
    {
        u16 base = HW_PAL_SCR2[(u16)road_band_pal * 4u + 2u];
        u8 n;
        for (n = 0u; n < 4u; n++) {
            u16 d = (u16)(3u - n);
            u16 r = (u16)((base >> 8) & 0x0Fu);
            u16 g = (u16)((base >> 4) & 0x0Fu);
            u16 b = (u16)(base & 0x0Fu);
            r = (r > d) ? (u16)(r - d) : 0u;
            g = (g > d) ? (u16)(g - d) : 0u;
            b = (b > d) ? (u16)(b - d) : 0u;
            band_col[n] = (u16)((r << 8) | (g << 4) | b);
        }
        if (road_is_tarmac()) {
            for (n = 0u; n < 4u; n++) {
                band_col[n] = course_sol[road_course].bande[n];
            }
        } else if (!uses_akina()) {
            for (n = 0u; n < 4u; n++) {
                band_col[n] = base;
            }
        }
    }
    id_build_weights();
    id_build_shear_limits();
    id_fill_hud_band();
    id_road_frame(0, 0, 0);     
}
static u8 road_dma_live;
static u8 road_dma_depth;
void id_road_dma_pause(void)
{
    if (road_dma_depth < 255u) {
        road_dma_depth++;
    }
    if (road_dma_live != 0u) {
        ngpc_set_vblank_hook((NgpcVblankFn)0);
        ngpc_dma_raster_xy_disable(&id_raster);
        ngpc_dma_stream_end_u16(&id_bandstream);
        road_dma_live = 0u;
    }
}
void id_road_dma_resume(void)
{
    if (road_dma_depth != 0u) {
        road_dma_depth--;
    }
    if (road_dma_depth != 0u) {
        return;             
    }
    if (road_dma_live == 0u) {
        ngpc_dma_raster_xy_begin(&id_raster, GFX_SCR2, id_xy_live);
        ngpc_dma_raster_xy_enable(&id_raster);
        if (!uses_akina() && !road_is_tarmac()) {
            id_bandstream.count = 0u;
            road_dma_live = 1u;
            ngpc_set_vblank_hook(id_road_rearm);
            return;
        }
        ngpc_dma_timer01_hblank_enable();
        ngpc_dma_stream_begin_u16(&id_bandstream, NGPC_DMA_CH1,
                                  (volatile u8 NGP_FAR *)&HW_PAL_SCR2[
                                      (u16)road_band_pal * 4u + 2u],
                                  id_bandcol, 152u, NGPC_DMA_VEC_TIMER1);
        ngpc_dma_stream_rearm_u16(&id_bandstream);
        road_dma_live = 1u;
        ngpc_set_vblank_hook(id_road_rearm);
    }
}
void id_road_init(void)
{
    id_road_select(0u);
    ngpc_dma_init();
    id_road_dma_resume();
}
void id_road_flat(void)
{
    u16 line;
    for (line = 0u; line < 152u; line++) {
        id_xy_a[line] = 0u;
        id_xy_b[line] = 0u;
        id_bandcol_a[line] = band_col[3];
        id_bandcol_b[line] = band_col[3];
    }
    bc_rest[0] = 0xFFFFu;       
    bc_rest[1] = 0xFFFFu;
    id_xy_done = 0;
    id_bandcol_done = 0;
    band_top[0] = 0xFFFFu; band_top[1] = 0xFFFFu;
    band_mid[0] = 0xFFFFu; band_mid[1] = 0xFFFFu;
    band_tree[0] = 0xFFFFu; band_tree[1] = 0xFFFFu;
    HW_SCR2_OFS_X = 0u;
    HW_SCR2_OFS_Y = 0u;
}
void id_road_rearm(void)
{
    if (id_xy_done != 0) {
        id_xy_live = id_xy_done;
        id_raster.table_xy = id_xy_done;
        id_raster.stream_xy.src_table = id_xy_done;
        id_bandstream.src_table = id_bandcol_done;
        id_xy_done = 0;
        id_bandcol_done = 0;
    }
    ngpc_dma_raster_xy_rearm(&id_raster);
    if (id_bandstream.count != 0u) {
        ngpc_dma_stream_rearm_u16(&id_bandstream);
    }
}
u8 id_road_dma_state(void)
{
    return (u8)(id_raster.enabled | (id_raster.table_xy ? 2u : 0u));
}
u16 id_road_xy_at(u8 line)
{
    return id_xy[line];
}
void id_road_restore_map(void)
{
    if (!uses_akina()) id_load_maps_rally();
    else id_load_maps();
}
void id_road_reload(void)
{
    id_fill_hud_band();
    ngpc_gfx_set_bg_color(0x0000u);
    id_load_palettes();
    if (!uses_akina()) {
        id_load_maps_rally();
    } else {
        id_load_maps();
    }
}
static s16 id_curve_dx[ID_ROAD_LINES_MAX];
#ifndef ID_ROAD_WALK_C
#define ID_ROAD_WALK_C 0        
#endif
typedef struct {
    u16       *xy;          
    u16       *band;        
    u8        *sl;          
    const s16 *sh;          
    const u8  *rtab;        
    const u8  *lut;         
    const u16 *bcol;        
    u16 row;                
    u16 step;               
    u16 half;               
    u8  top;                
    u8  plane0;             
    u8  base_x;             
    u8  n;                  
    u8  line;               
    u8  pad;                
} RoadWalk;
typedef char road_walk_ptr_is_4[(sizeof(const u8 *) == 4u) ? 1 : -1];
typedef char road_walk_is_40[(sizeof(RoadWalk) == 40u) ? 1 : -1];
extern void id_road_walk_asm(RoadWalk *w);
#ifndef ID_ROAD_SHEAR_C
#define ID_ROAD_SHEAR_C 0       
#endif
typedef struct {
    const u8  *w;           
    const s16 *cv;          
    const s16 *mx;          
    s16       *sh;          
    s16 lateral;            
    u16 n;                  
} RoadShearWalk;
typedef char road_shear_walk_is_20[(sizeof(RoadShearWalk) == 20u) ? 1 : -1];
extern void id_road_shear_asm(RoadShearWalk *s);
static void id_road_write(s16 lateral, s16 heading)
{
    u16 line;
    s16 sky, mountain, tree;
    u16 bg_xy;
    s16 bg_drop;
    {
        u8  acc = 0u;
        u16 bandc = band_col[3]; 
#if ID_PROBE
        u16 somme = 0u;         
#endif
        u16 row_fx;             
        s16 step_fx;            
        s16 lineno = (s16)road_bottom;
        s16 top_limit = (s16)road_top - (s16)climb_room;
        s16 k;
        u8  ran_out;
        const u8 *lut = band_lut[(band_phase >> BAND_RATE) & 15u];
        u16 *xy = id_xy;
        u16 *bcol = id_bandcol;
        u8  base_x = road_base_x;
        u8  plane0 = road_plane0;
        row_fx = (u16)((u16)id_src[road_lines - 1u] << 8);
        step_fx = (s16)((((s16)id_src[road_lines - 1u] - (s16)id_src[0]) * 256)
                        / (s16)(road_lines - 1u));
#if !ID_ROAD_SHEAR_C
        if (road_lines != 0u) {
            RoadShearWalk s;
            s.w = id_w_lat;
            s.cv = id_curve_dx;
            s.mx = id_dx_max;
            s.sh = id_shear;
            s.lateral = lateral;
            s.n = (u16)road_lines;
            id_road_shear_asm(&s);
        }
#else
        {
            const u8  *w = id_w_lat;
            const s16 *cv = id_curve_dx;
            const s16 *mx = id_dx_max;
            const s16 *mn = id_dx_min;
            s16 *sh = id_shear;
            u8 n = road_lines;
            while (n != 0u) {
                s16 dx = (s16)((s16)((lateral * (s16)*w) >> 8) + *cv);
                if (dx > *mx) {
                    dx = *mx;
                } else if (dx < *mn) {
                    dx = *mn;
                }
                *sh = dx;
                w++; cv++; mx++; mn++; sh++;
                n--;
            }
        }
#endif
#if !ID_ROAD_WALK_C
        if (road_lines != 0u && top_limit >= 2 && top_limit <= 255) {
            RoadWalk w;
            k = (s16)road_lines - 1;
            if (lineno >= top_limit) {
                w.xy = &xy[lineno];
                w.band = &bcol[lineno];
                w.sl = id_line_of + road_lines;
                w.sh = id_shear + road_lines;
                w.rtab = &hill_rate[20];
                w.lut = lut;
                w.bcol = band_col;
                w.row = row_fx;
                w.step = (u16)step_fx;
                w.half = (u16)((u16)step_fx >> 1);
                w.top = (u8)top_limit;
                w.plane0 = plane0;
                w.base_x = base_x;
                w.n = road_lines;
                w.line = (u8)lineno;
                id_road_walk_asm(&w);
                k = (s16)w.n - 1;
                lineno = (s16)w.line;
            }
#if ID_PROBE
            {
                s16 j;
                for (j = (s16)road_lines - 1; j >= ((k < 0) ? 0 : k); j--) {
                    somme = (u16)(somme + (u16)hill_rate[20 + (s16)id_grade[j]]);
                }
            }
#endif
            (void)acc;
        } else
#endif
        for (k = (s16)road_lines - 1; k >= 0; k--) {
            u8  rate = hill_rate[20 + (s16)id_grade[k]];
            u8  c;
            u16 t;
            s16 dx;
#if ID_PROBE
            somme = (u16)(somme + (u16)rate);   
#endif
            t = (u16)((u16)acc + (u16)rate + (u16)rate);
            c = (u8)(t >> 8);
            acc = (u8)t;
            dx = id_shear[k];       
            id_line_of[k] = (u8)((lineno < top_limit) ? top_limit : lineno);
            bandc = band_col[lut[id_bandi[k]]];   
            if (c >= 1u && lineno >= top_limit) {
                u8 scr = (u8)lineno;
                xy[scr] = (u16)(((u16)(u8)((u8)(row_fx >> 8)
                                           - (u8)(scr - plane0)) << 8)
                                | (u16)(u8)(base_x + dx));
                bcol[scr] = bandc;
                lineno--;
            }
            if (c == 2u && lineno >= top_limit) {
                u8 scr = (u8)lineno;
                u16 half = (u16)((u16)step_fx >> 1);
                u16 mid = (row_fx > half) ? (u16)(row_fx - half) : row_fx;
                xy[scr] = (u16)(((u16)(u8)((u8)(mid >> 8)
                                           - (u8)(scr - plane0)) << 8)
                                | (u16)(u8)(base_x + dx));
                bcol[scr] = bandc;
                lineno--;
            }
            row_fx = (u16)(row_fx - (u16)step_fx);
            if (lineno < top_limit) {
                break;
            }
        }
        ran_out = (u8)((k < 0) ? 1u : 0u);
        while (k >= 0) {
            id_line_of[k] = (u8)top_limit;
            k--;
        }
#if ID_PROBE
        rate_sum = somme;
#endif
        band_first = (u8)(lineno + 1);
        if (band_first > band_held) {
            if ((u8)(band_first - band_held) >= 2u || band_wait >= BAND_HOLD) {
                band_wait = 0u;
            } else {
                band_wait++;
                while (band_first > band_held) {
                    band_first--;
                    id_xy[band_first] = id_xy[band_first + 1u];
                    id_bandcol[band_first] = id_bandcol[band_first + 1u];
                }
            }
        } else {
            band_wait = 0u;
        }
        band_held = band_first;
        {
            s16 mont = (s16)((s16)road_top - (s16)band_first);
            u16 fin;
            u16 rest = band_col[3];
            u16 *p;
            u16 *end;
            if (road_is_tarmac() && !0u) {
                u16 gap = band_col[0];
                u8 phase = (u8)(0u - (band_phase >> 5));
                fin = road_top;
                if (fin > (u16)road_bottom) fin = road_bottom;
                p = id_bandcol + band_first;
                end = id_bandcol + fin;
                while (p < end) {
                    *p++ = (phase++ & 2u) ? rest : gap;
                }
            } else {
                if (mont > (s16)BAND_FADE) mont = (s16)BAND_FADE;
                fin = (u16)((s16)band_first + ((mont > 0) ? mont : 0));
                if (fin > (u16)road_bottom) fin = road_bottom;
                p = id_bandcol + band_first;
                end = id_bandcol + fin;
                while (p < end) *p++ = rest;
            }
        }
        band_shift = (s16)((s16)band_first - (s16)road_top);
        band_count = (u8)((s16)road_bottom - (s16)band_first + 1);
        (void)ran_out;
    }
    {
        u16 rest = band_col[3];
        u8 b = id_buf;
        u16 *p;
        u16 *end;
        if (bc_rest[b] != rest) {
            bc_rest[b] = rest;
            bc_top[b] = 0u;
            p = id_bandcol + ID_PANEL_TOP + 1u;
            end = id_bandcol + 152u;
            while (p < end) {
                *p++ = rest;
            }
        }
        if (bc_top[b] < band_first) {
            p = id_bandcol + bc_top[b];
            end = id_bandcol + band_first;
            while (p < end) {
                *p++ = rest;
            }
        }
        bc_top[b] = band_first;
    }
    id_xy[ID_PANEL_TOP] = id_xy[road_bottom];
    id_bandcol[ID_PANEL_TOP] = id_bandcol[road_bottom];
    bg_drop = (band_shift > 0) ? band_shift : 0;
    bg_xy = 0u;
    sky = (s16)(-(heading >> SKY_SHIFT));
    mountain = (s16)(-(heading >> MOUNTAIN_SHIFT));
    tree = (s16)(-(heading >> TREE_SHIFT) + (lateral >> TREE_LATERAL_SHIFT));
    if (!uses_akina()) {
        u16 v_far = (u16)((u8)(road_base_x
                               + ((pan_fine != 0)
                                  ? (s16)((-(pan_head / 2)) / 16)
                                  : (s16)(-(heading >> 1) + (lateral >> 4)))));
        u16 v_near = (u16)((u8)(road_base_x
                                + ((pan_fine != 0)
                                   ? (s16)((-(pan_head * 2)) / 16)
                                   : (s16)(-(heading * 2) + (lateral >> 2)))));
        u8 bg_top = (u8)((band_first < 244u) ? (band_first + (u8)ROAD_TIP_HIDE)
                                              : band_first);
        u8 bg_end = (bg_top > (u8)RALLY_SKY_SPLIT)
                    ? bg_top : (u8)RALLY_SKY_SPLIT;
        u8 bg_cache = (band_first < bg_end) ? band_first : bg_end;
        u16 debut = (u16)bg_cache;
        if (v_far != band_top[id_buf] || v_near != band_mid[id_buf]
            || bg_end != band_bg_end[id_buf] || bg_drop != band_pitch[id_buf]
            || band_first != band_bg_first[id_buf]) {
            band_top[id_buf] = v_far;
            band_mid[id_buf] = v_near;
            band_bg_end[id_buf] = bg_end;
            band_bg_first[id_buf] = band_first;
            debut = 0u;
        }
        {
            u8 tall = sky_is_tall_course();
            {
                u16 *p = id_xy + debut;
                u16 *end = id_xy + bg_end;
                if (tall) {
                    while (p < end) {
                        *p++ = v_near;
                    }
                } else {
                    u16 *cut;
                    u16 yb = (u16)((u16)(u8)(-bg_drop) << 8);
                    line = debut;
                    while (p < end && (s16)line < bg_drop) {
                        *p++ = (u16)(((line < (u16)RALLY_SKY_SPLIT) ? v_far : v_near)
                                     | ((u16)(u8)(-(s16)line) << 8));
                        line++;
                    }
                    cut = id_xy + (u16)RALLY_SKY_SPLIT;
                    if (cut > end) {
                        cut = end;
                    }
                    while (p < cut) {
                        *p++ = (u16)(v_far | yb);
                    }
                    while (p < end) {
                        *p++ = (u16)(v_near | yb);
                    }
                }
            }
        }
#if ID_PROBE
        pan_seen = (u8)v_near;
        pan_mode = pan_fine;
#endif
        band_pitch[id_buf] = (s8)bg_drop;
        bg_xy = v_near;
    } else {
        u16 a = (u16)((u8)(road_base_x + sky));
        u16 b = (u16)((u8)(road_base_x + mountain));
        u16 c = (u16)((u8)(road_base_x + tree));
        (void)a;
        (void)b;
        if (c != band_tree[id_buf] || bg_drop != band_pitch[id_buf]
            || band_first != band_bg_end[id_buf]) {
            band_pitch[id_buf] = (s8)bg_drop;
            band_bg_end[id_buf] = band_first;
            band_mid[id_buf] = c;
            band_tree[id_buf] = c;
            for (line = 0u; line < band_first; line++) {
                s16 y;
                y = sky_is_tall_course() ? 0 : -bg_drop;
                if ((s16)line + y < 0) {
                    y = -(s16)line;      
                }
                id_xy[line] = (u16)(c | ((u16)(u8)y << 8));
            }
        }
        bg_xy = c;
    }
}
void id_road_ready(void)
{
    id_xy_done = id_xy;
    id_bandcol_done = id_bandcol;
}
void id_road_pan(s16 heading16, s16 lateral16)
{
    pan_head = heading16;
    pan_lat = lateral16;
    pan_fine = 1u;
}
void id_road_begin(void)
{
    if (id_xy_live == id_xy_a) {
        id_xy = id_xy_b;
        id_bandcol = id_bandcol_b;
    } else {
        id_xy = id_xy_a;
        id_bandcol = id_bandcol_a;
    }
    id_buf = (u8)((id_xy == id_xy_a) ? 0u : 1u);
}
void id_road_frame(s16 lateral, s16 curve, s16 heading)
{
    u8 k;
    id_road_begin();
    for (k = 0u; k < road_lines; k++) {
        id_curve_dx[k] = (s16)((curve * (s16)id_w_curve[k]) >> 8);
    }
    id_road_write(lateral, heading);
    id_road_ready();
}
const u16 *id_road_ahead(void)
{
    return id_ahead;
}
u8 id_road_lines(void)
{
    return road_lines;
}
#ifndef ID_ROAD_CURVE_C
#define ID_ROAD_CURVE_C 0
#endif
typedef struct {
    const s16 *bend_end;    
    s16       *out_end;     
    u16 cm;                 
    u16 n;                  
} RoadCurve;
typedef char road_curve_is_12[(sizeof(RoadCurve) == 12u) ? 1 : -1];
extern void id_road_curve_asm(RoadCurve *c);
void id_road_frame_ahead(s16 lateral, const s16 *bend, s16 heading)
{
    u8 k = road_lines;
    s16 slope = 0;              
    s16 acc = 0;                
    id_road_begin();
#if !ID_ROAD_CURVE_C
    if (k != 0u) {
        RoadCurve rc;
        rc.bend_end = bend + k;
        rc.out_end = id_curve_dx + k;
        rc.cm = id_curve_cm;
        rc.n = (u16)k;
        id_road_curve_asm(&rc);
    }
    (void)slope;
    (void)acc;
#else
    {
        const s16 *bp = bend + k;
        s16 *out = id_curve_dx + k;
        u16 cm = id_curve_cm;
        while (k > 0u) {
            s16 b = *--bp;
            s16 mag = (b < 0) ? (s16)-b : b;
            s16 c = (s16)(((u16)mag * cm + 128u) >> 8);
            slope = (s16)(slope + ((b < 0) ? (s16)-c : c));
            acc = (s16)(acc + slope);
            *--out = (s16)(acc >> 8);
            k--;
        }
    }
#endif
    id_road_write(lateral, heading);
    id_road_ready();
}
static void id_fill_hud_band(void)
{
    u16 line;
    for (line = ID_PANEL_TOP; line < 152u; line++) {
        id_xy_a[line] = (u16)(((u16)(u8)(ID_BACK_Y - ID_PANEL_TOP) << 8)
                              | (u16)road_base_x);
        id_xy_b[line] = id_xy_a[line];
    }
}
void id_road_sprite_sample(u8 line, IdRoadSpriteSample *out)
{
    u8 k;
    if (line < road_top) line = road_top;
    else if (line > road_bottom) line = road_bottom;
    k = (u8)(line - road_top);
    out->centre = (s16)((s16)(ID_ROAD_CENTRE - ID_BASE_X) - id_shear[k]);
    out->foot = (s16)((s16)line + (s8)((s16)id_line_of[k] - (s16)line));
    out->half = id_half[k];
}
s16 id_road_centre_at(u8 line)
{
    u8 k;
    if (line < road_top) {
        line = road_top;
    } else if (line > road_bottom) {
        line = road_bottom;
    }
    k = (u8)(line - road_top);
    return (s16)((s16)(ID_ROAD_CENTRE - ID_BASE_X) - id_shear[k]);
}
void id_road_band_phase(u16 travelled)
{
    band_phase = travelled;
}
void id_road_join(u8 course)
{
    road_course = course < ID_ROAD_COURSES ? course : 0u;
    id_load_palettes();
    id_road_join_white(0u);
}
void id_road_join_white(u8 white)
{
    u8 n;
    for (n = 0u; n < 4u; n++) {
        u16 c = course_sol[road_course].bande[n];
        u16 r = (u16)((c >> 8) & 15u);
        u16 g = (u16)((c >> 4) & 15u);
        u16 b = (u16)(c & 15u);
        r = (u16)(r + ((15u - r) * white + 7u) / 15u);
        g = (u16)(g + ((15u - g) * white + 7u) / 15u);
        b = (u16)(b + ((15u - b) * white + 7u) / 15u);
        band_col[n] = (u16)((r << 8) | (g << 4) | b);
    }
    band_top[0] = band_top[1] = 0xFFFFu;
    band_mid[0] = band_mid[1] = 0xFFFFu;
    band_tree[0] = band_tree[1] = 0xFFFFu;
}
void id_road_hill_ahead(const s8 *grade)
{
    const s8 *src = grade;
    s8 *dst = id_grade;
    s8 hi = grade_hi;
    s8 lo = grade_lo;
    u8 n = road_lines;
    if (src == dst) {
        n = 0u;
    }
    while (n != 0u) {
        s8 g = *src++;
        if (g > hi) {
            g = hi;
        } else if (g < lo) {
            g = lo;
        }
        *dst++ = g;
        n--;
    }
    road_grade = id_grade[road_lines - 1u];
}
s8 *id_road_grade_buf(void)
{
    return id_grade;
}
u8 id_road_band_first(void)
{
    return band_first;
}
u8 id_road_visible_top(void)
{
    u8 top = (band_first < 244u) ? (u8)(band_first + (u8)ROAD_TIP_HIDE)
                                 : band_first;
    if (top < (u8)RALLY_SKY_SPLIT) {
        top = (u8)RALLY_SKY_SPLIT;
    }
    return (top > road_bottom) ? road_bottom : top;
}
#if ID_PROBE
u16 id_road_rate_sum(void)
{
    return rate_sum;
}
#endif
s8 id_road_grade_hi(void)
{
    return grade_hi;
}
s8 id_road_grade_lo(void)
{
    return grade_lo;
}
void id_road_set_hill(s8 grade)
{
    if (grade > grade_hi) {
        grade = grade_hi;
    } else if (grade < grade_lo) {
        grade = grade_lo;
    }
    road_grade = grade;
    {
        u8 k;
        for (k = 0u; k < road_lines; k++) {
            id_grade[k] = grade;
        }
    }
}
s8 id_road_line_shift(u8 line)
{
    s16 k, j;
    if (line < road_top || line > road_bottom) {
        return 0;
    }
    k = (s16)((s16)line - (s16)road_top);
    if (k >= (s16)road_lines) {
        k = (s16)road_lines - 1;
    }
    j = (s16)id_line_of[k];
    return (s8)(j - (s16)line);
}
#define CAR_LIFT_UP     8       
#define CAR_LIFT_DOWN   5       
s8 id_road_car_lift(void)
{
    s16 lift = (s16)(-(s16)id_road_line_shift((u8)(road_bottom - car_stand)));
    if (lift > (s16)CAR_LIFT_UP) {
        lift = (s16)CAR_LIFT_UP;
    } else if (lift < (s16)(-CAR_LIFT_DOWN)) {
        lift = (s16)(-CAR_LIFT_DOWN);
    }
    return (s8)lift;
}
u8 id_road_half_at(u8 line)
{
    if (line < road_top) {
        line = road_top;
    } else if (line > road_bottom) {
        line = road_bottom;
    }
    return id_half[(u8)(line - road_top)];
}

