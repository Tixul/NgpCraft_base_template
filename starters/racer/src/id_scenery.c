/* Original first-track roadside projection, phase and sprite-size ladder. */

#include "ngpc_hw.h"
#include "ngpc_gfx.h"
#include "id_scenery.h"
#include "../GraphX/id_borne.h"
#include "id_road.h"
#include "id_player.h"
#include "../GraphX/id_race.h"
#define SCN_COUNT       6u
#define SCN_VIEW     1100u
#define SCN_SPACING  (SCN_VIEW / SCN_COUNT)
#define SCN_Z_MAX      40u
#define SCN_Z_STEP     (SCN_VIEW / SCN_Z_MAX)
#define SCN_OFF_NEAR   (u16)(id_road_bottom() - id_road_top())
#define SCN_NEAR        3u
#define SCN_OFF_MIN     2u
#define SCN_RATE      230u
#define SCN_MARGIN(half)  (u8)(3u + ((half) >> 2))
#define SCN_PAL         1u

static u8 scn_style;
static u8 scn_width[5];
static u16 scn_tile_base;
static u8  scn_line[SCN_Z_MAX];
static u8  scn_rung[SCN_Z_MAX];
static u16 scn_lead;
static u8  scn_phase;   
static u16 scn_frac;            
static u8  scn_side;
static u8  scn_passed;
#if ID_PROBE
static u8  scn_hidden;
static u8  scn_highest;
u8 id_scenery_hidden(void)
{
    return scn_hidden;
}
u8 id_scenery_highest(void)
{
    return scn_highest;
}
#endif
u16 id_scenery_char_count(void)
{
    return (u16)ID_BORNE_CHARS;
}
static void scn_load_chars(u16 base)
{
    u16 tile;
    for (tile = 0u; tile < (u16)ID_BORNE_CHARS; tile++) {
        volatile u8 *dst = (volatile u8 *)(0xA000u + (u32)(base + tile) * 16u);
        u16 w;
        for (w = 0u; w < 8u; w++) {
            u16 v = id_borne_tiles[(u16)((u16)scn_style * 64u + tile * 8u + w)];
            dst[(u16)(w * 2u) + 0u] = (u8)(v & 0xFFu);
            dst[(u16)(w * 2u) + 1u] = (u8)(v >> 8);
        }
    }
}
static void scn_build_tables(void)
{
    u8 z;
    for (z = 0u; z < SCN_Z_MAX; z++) {
        u16 off = (u16)((u16)(SCN_OFF_NEAR * SCN_NEAR) / (u16)(z + SCN_NEAR));
        if (off > (u16)SCN_OFF_NEAR) {
            off = (u16)SCN_OFF_NEAR;
        }
        scn_line[z] = (u8)(id_road_top() + off);
        if (off < 4u) {
            scn_rung[z] = 0u;
        } else if (off < 8u) {
            scn_rung[z] = 1u;
        } else if (off < 14u) {
            scn_rung[z] = 2u;
        } else {
            scn_rung[z] = 3u;
        }
    }
}
void id_scenery_init(u16 tile_base, u8 course)
{
    u8 i;
    u8 pal;
    (void)course; scn_style=0u;
    pal = (u8)(scn_style * 4u);
    for (i = 0u; i < 5u; i++) {
        scn_width[i] = id_borne_px_w[i];
    }
    scn_tile_base = tile_base;
    scn_load_chars(tile_base);
    ngpc_gfx_set_palette((u8)GFX_SPR, (u8)SCN_PAL,
                         id_borne_palette[(u8)(pal + 0u)], id_borne_palette[(u8)(pal + 1u)],
                         id_borne_palette[(u8)(pal + 2u)], id_borne_palette[(u8)(pal + 3u)]);
    scn_build_tables();
    scn_lead = (u16)(SCN_SPACING / 2u);
    scn_phase = 0u;
    scn_frac = 0u;
    scn_side = 0u;
    scn_passed = 0u;
    id_spr_hide_range((u8)ID_SCN_SPR_0, (u8)ID_SCN_SPR_MAX);
}
u8 id_scenery_passed(void)
{
    return scn_passed;
}
void id_scenery_frame(u8 speed)
{
    u8 i;
    u8 spr = ID_SCN_SPR_0;
    u16 step;
    u8 r_top = id_road_top();
    s16 r_bottom = (s16)id_road_bottom();
    s16 haut = (s16)id_road_visible_top();
    u16 off_near = (u16)(r_bottom - (s16)r_top);   
    step = (u16)((u16)scn_frac + (u16)speed * (u16)SCN_RATE);
    scn_frac = (u16)(step & 4095u);
    step >>= 12;
    {
        u8 avant = scn_phase;
        u8 k;
        scn_phase = (u8)(scn_phase + (u8)step);
        scn_passed = 0u;
        for (k = 0u; k < SCN_COUNT; k++) {
            u8 o = (u8)((u16)k * (256u / SCN_COUNT));
            if ((u8)(scn_phase + o) < (u8)(avant + o)) {
                scn_passed |= (u8)((((scn_side + k) & 1u) != 0u) ? 2u : 1u);
            }
        }
    }
#if ID_PROBE
    scn_hidden = 0u;
    scn_highest = 255u;
#endif
    for (i = 0u; i < SCN_COUNT; i++) {
        u8 ph = (u8)(scn_phase + (u8)((u16)i * (256u / SCN_COUNT)));
        u16 q = (u16)(((u16)ph * (u16)ph) >> 8);
        u16 off = (u16)(SCN_OFF_MIN
                        + (u16)((q * (u16)(off_near - SCN_OFF_MIN)) / 255u));
        u8 line, draw, rung, cells, c;
        s16 x;
        IdRoadSpriteSample geometry;
        if (off > off_near) {
            off = off_near;
        }
        line = (u8)(r_top + off);
        if (off < 6u) {
            rung = 4u;                  
        } else if (off < 10u) {
            rung = 3u;
        } else if (off < 15u) {
            rung = 2u;
        } else if (off < 22u) {
            rung = 1u;
        } else {
            rung = 0u;                  
        }
        {
            s16 sh;
            id_road_sprite_sample(line, &geometry);
            sh = geometry.foot;
            if (sh < haut) {
#if ID_PROBE
                scn_hidden++;
#endif
                continue;
            }
            if (sh > r_bottom) {
                sh = r_bottom;
            }
            draw = (u8)sh;
        }
        {
            u8 half = geometry.half;
            s16 centre = geometry.centre;
            s16 bord = (s16)half + (s16)SCN_MARGIN(half);

            if (((scn_side + i) & 1u) != 0u) {
                x = (s16)(centre + bord);
            } else {
                x = (s16)(centre - bord - (s16)scn_width[rung] + 1);
            }
        }
        if (x < -16 || x > 168) {
            continue;
        }
        {
            u8 bw = id_borne_w[rung];
            u8 bh = id_borne_h[rung];
            u16 ch = (u16)(scn_tile_base + id_borne_first[rung]);
            u8 cy;
            u8 stop = 0u;
            cells = (u8)(bw * bh);
            c = 0u;
            for (cy = 0u; cy < bh && !stop; cy++) {
                u8 y = (u8)((u16)draw - (u16)((bh - cy) * 8u));
                u8 cx;
                for (cx = 0u; cx < bw; cx++) {
                    if (spr >= ID_SCN_SPR_MAX) {
                        stop = 1u;
                        break;
                    }
#if ID_PROBE
                    if (y < scn_highest) {
                        scn_highest = y;
                    }
#endif
                    id_spr_put(spr, (u8)(x + (s16)(cx * 8u)), y,
                               (u16)(ch + c), (u8)SCN_PAL, 0u);
                    spr++;
                    c++;
                }
            }
        }
    }
    id_spr_hide_range(spr, (u8)ID_SCN_SPR_MAX);
}
