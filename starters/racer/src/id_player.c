#include "ngpc_hw.h"
#include "ngpc_gfx.h"
#include "id_player.h"
#include "id_road.h"
#include "id_fleet.h"
#define SPR_TABLE  0x8800u      
#define SPR_PALS   0x8C00u      
#define CAR_PAL     0u
#define CAR_PAL2    5u
static IdBody body;
static u8 car_variant;
static u8 car_wheel_dx;
static u16 car_tile_base;
u8 id_spr_priority = SPR_FRONT;
void id_spr_put(u8 id, u8 x, u8 y, u16 tile, u8 pal, u8 flip)
{
    volatile u8 *s = (volatile u8 *)(SPR_TABLE + ((u16)id << 2));
    s[0] = (u8)(tile & 0x00FFu);
    s[1] = (u8)(flip | id_spr_priority | (u8)((tile >> 8) & 1u));
    s[2] = x;
    s[3] = y;
    *(volatile u8 *)(SPR_PALS + (u16)id) = (u8)(pal & 0x0Fu);
}
void id_spr_hide_range(u8 from, u8 to)
{
    volatile u8 *s;
    u8 n;
    if (from >= to) {
        return;
    }
    s = (volatile u8 *)(SPR_TABLE + 1u + ((u16)from << 2));
    n = (u8)(to - from);
    do {
        *s = (u8)(*s & (u8)~(3u << 3));          
        s += 4;
    } while (--n != 0u);
}
static void mesure_roues(void)
{
    u8 layer, i;
    u8 bas = 0u;            
    u8 droite = 0u;         
    u8 gauche = 255u;
    u8 bord = 0u;           
    for (layer = 0u; layer < body.layers; layer++) {
        u8 p = (u8)(layer * body.poses);
        u8 first = body.pose_first[p];
        u8 n = body.pose_cells[p];
        for (i = 0u; i < n; i++) {
            u8 cx = body.cells[(u8)(first + i)].cx;
            u8 cy = body.cells[(u8)(first + i)].cy;
            u16 ch = body.cells[(u8)(first + i)].ch;
            u8 y;
            for (y = 0u; y < 8u; y++) {
                u16 mot = body.tiles[(u16)(ch * 8u + y)];
                u8 x;
                for (x = 0u; x < 8u; x++) {
                    if (((mot >> (14u - 2u * x)) & 3u) == 0u) {
                        continue;
                    }
                    if ((u8)(cy * 8u + y) > bas) {
                        bas = (u8)(cy * 8u + y);
                        bord = 0u;      
                    }
                    if ((u8)(cy * 8u + y) == bas
                        && (u8)(cx * 8u + x) > bord) {
                        bord = (u8)(cx * 8u + x);
                    }
                    if ((u8)(cx * 8u + x) > droite) {
                        droite = (u8)(cx * 8u + x);
                    }
                    if ((u8)(cx * 8u + x) < gauche) {
                        gauche = (u8)(cx * 8u + x);
                    }
                }
            }
            
        }
    }
    if (gauche > droite) {              
        car_wheel_dx = (u8)(body.pose_w[0] * 4u);
        return;
    }
    {
        u16 deux_pneu = (u16)(2u * (u16)(bord - 1u));
        u16 deux_mil = (u16)((u16)gauche + (u16)droite);
        car_wheel_dx = (deux_pneu > deux_mil)
                           ? (u8)((deux_pneu - deux_mil) / 2u) : 0u;
    }
}
void id_car_select(u8 variant)
{
    car_variant = (variant < (u8)ID_BODY_COUNT) ? variant : (u8)ID_BODY_ORIG;
    id_fleet_body(car_variant, &body);
    mesure_roues();
}
u16 id_car_char_count(u8 all)
{
    return (u16)(((all != 0u) ? body.tiles_count : body.race_tiles) / 8u);
}
u8 id_car_half_width(void)
{
    return (u8)(body.pose_w[0] * 4u);
}
u8 id_car_wheel_dx(void)
{
    return car_wheel_dx;
}
u8 id_car_height(void)
{
    return body.height;
}
void id_car_init(u16 tile_base, u8 all)
{
    u16 tile;
    u16 count = id_car_char_count(all);
    const u16 *src = body.tiles;
    car_tile_base = tile_base;
    for (tile = 0u; tile < count; tile++) {
        volatile u8 *dst = (volatile u8 *)(0xA000u + (u32)(tile_base + tile) * 16u);
        u16 w;
        for (w = 0u; w < 8u; w++) {
            u16 v = src[(u16)(tile * 8u + w)];
            dst[(u16)(w * 2u) + 0u] = (u8)(v & 0xFFu);
            dst[(u16)(w * 2u) + 1u] = (u8)(v >> 8);
        }
        
    }
    ngpc_gfx_set_palette((u8)GFX_SPR, (u8)CAR_PAL,
                         body.palette[0], body.palette[1],
                         body.palette[2], body.palette[3]);
    if (body.layers > 1u) {
        ngpc_gfx_set_palette((u8)GFX_SPR, (u8)CAR_PAL2,
                             body.palette[4], body.palette[5],
                             body.palette[6], body.palette[7]);
    } else {
        ngpc_gfx_set_palette((u8)GFX_SPR, (u8)CAR_PAL2,
                             body.palette[0], body.palette[1],
                             body.palette[2], body.palette[3]);
    }
    id_spr_hide_range((u8)ID_CAR_SPR2_0, (u8)ID_CAR_SPR2_MAX);
}
void id_car_portrait_angle(u8 cxm, u8 py, u8 spr0, u8 angle)
{
    u8 i, layer;
    u8 spr = spr0;
    u8 n_ang = (u8)(2u * body.poses - 2u);
    u8 pose, mirror, lo, hi, px;
    if (body.poses < 2u) {
        id_car_portrait((u8)(cxm - id_car_half_width()), py, spr0);
        return;
    }
    angle = (u8)(angle % n_ang);
    if (angle < body.poses) {
        pose = angle;
        mirror = 0u;
    } else {
        pose = (u8)(n_ang - angle);
        mirror = 1u;
    }
    {
        u8 first = body.pose_first[pose];
        u8 n = body.pose_cells[pose];
        u8 w = body.pose_w[pose];
        lo = 255u;
        hi = 0u;
        for (i = 0u; i < n; i++) {
            u8 c = body.cells[(u8)(first + i)].cx;
            u8 x = mirror ? (u8)(w - 1u - c) : c;
            if (x < lo) {
                lo = x;
            }
            if (x > hi) {
                hi = x;
            }
        }
        if (lo > hi) {
            lo = 0u;
            hi = 0u;
        }
        px = (u8)(cxm - (u8)(((u16)(hi - lo + 1u) * 8u) / 2u) - (u8)(lo * 8u));
    }
    for (layer = 0u; layer < body.layers; layer++) {
        u8 l = (u8)(body.layers - 1u - layer);
        u8 first = body.pose_first[(u8)(l * body.poses + pose)];
        u8 n = body.pose_cells[(u8)(l * body.poses + pose)];
        u8 w = body.pose_w[(u8)(l * body.poses + pose)];
        u8 pal = (u8)((l == 0u) ? CAR_PAL : CAR_PAL2);
        for (i = 0u; i < n && spr < (u8)ID_CAR_PORTRAIT_MAX; i++) {
            const IdCarCell *c = &body.cells[(u8)(first + i)];
            u8 cx = mirror ? (u8)(w - 1u - c->cx) : c->cx;
            ID_SPR_PUT(spr, (u8)(px + cx * 8u), (u8)(py + c->cy * 8u),
                       (u16)(car_tile_base + (u16)c->ch), pal,
                       (u8)(mirror ? SPR_HFLIP : 0u));
            spr++;
        }
    }
    id_spr_hide_range(spr, (u8)ID_CAR_PORTRAIT_MAX);
}
void id_car_portrait(u8 px, u8 py, u8 spr0)
{
    u8 i, layer;
    u8 spr = spr0;
    for (layer = 0u; layer < body.layers; layer++) {
        u8 l = (u8)(body.layers - 1u - layer);
        u8 first = body.pose_first[l * body.poses];
        u8 n = body.pose_cells[l * body.poses];
        u8 pal = (u8)((l == 0u) ? CAR_PAL : CAR_PAL2);
        for (i = 0u; i < n && spr < (u8)ID_CAR_PORTRAIT_MAX; i++) {
            const IdCarCell *c = &body.cells[(u8)(first + i)];
            ID_SPR_PUT(spr, (u8)(px + c->cx * 8u), (u8)(py + c->cy * 8u),
                       (u16)(car_tile_base + (u16)c->ch), pal, 0u);
            spr++;
        }
    }
    id_spr_hide_range(spr, (u8)ID_CAR_PORTRAIT_MAX);
}
static void car_layer(const IdCarCell *cells, u8 first, u8 n, u8 w,
                      u8 mirror, s16 lat, u8 pal, u8 spr, u8 last, u8 top,
                      u8 wheel, u8 lift)
{
    u8 i;
    s16 x = (s16)(id_road_centre_at(wheel) - (s16)(w * 4u) + lat);
    const IdCarCell *cell = &cells[first];
    for (i = 0u; i < n; i++, cell++) {
        u8 cx = cell->cx;
        s16 sx;
        if (mirror) {
            cx = (u8)(w - 1u - cx);
        }
        sx = (s16)(x + (s16)(cx * 8u));
        if (sx < -8 || sx > 168 || spr >= last) {
            continue;
        }
        ID_SPR_PUT(spr, (u8)sx,
                   (u8)(top - lift + cell->cy * 8u),
                   (u16)(car_tile_base + (u16)cell->ch),
                   pal, (u8)(mirror ? SPR_HFLIP : 0u));
        spr++;
    }
    id_spr_hide_range(spr, last);
}
#define LEAN_WIND   8u
#define LEAN_WIND_2  5u
#define LEAN_WIND_3 12u
#define LEAN_UNWIND  2u
#define CAR_WHEEL   ID_CAR_WHEEL     
#define car_top()   ((u8)((u16)CAR_WHEEL + 1u - (u16)body.height))
static u8 lean_hold;
static s8 lean_side;            
static u8 lean_wait;            
static u8 lean_last(void)
{
    u8 last = (u8)((body.poses > 0u) ? (body.poses - 1u) : 0u);
    if (last > 2u) {
        last = 2u;              
    }
    return last;
}
static u8 lean_pose(void)
{
    u8 last = lean_last();
    u8 pose;
    if (lean_hold == 0u) {
        return 0u;
    }
    pose = (u8)((lean_hold >= (u8)LEAN_WIND_3) ? 3u
                : ((lean_hold >= (u8)LEAN_WIND_2) ? 2u : 1u));
    return (u8)((pose > last) ? last : pose);
}
s8 id_car_lean_share(void)
{
    u8 last = lean_last();
    u8 pose = lean_pose();
    s16 part;
    if (pose == 0u || last == 0u) {
        return 0;
    }
    part = (s16)(((s16)pose * 16) / (s16)last);
    if (part > 16) {
        part = 16;
    }
    return (s8)((lean_side < 0) ? -part : part);
}
static u8 bump_phase;
void id_car_draw(s16 lat, s8 lean, u8 rough)
{
    u8 pose, mirror;
    u8 lift;
    u8 top;                     
    u8 wheel;                   
    if (lean != 0) {
        if (lean_side != lean) {
            lean_side = lean;
            lean_hold = 0u;
        }
        if (lean_hold < 255u) {
            lean_hold++;
        }
        lean_wait = (u8)LEAN_UNWIND;    
    } else if (lean_hold != 0u) {
        if (lean_wait != 0u) {
            lean_wait--;
        } else {
            lean_wait = (u8)LEAN_UNWIND;
            if (lean_hold >= (u8)LEAN_WIND_3) {
                lean_hold = (u8)(LEAN_WIND_3 - 1u);
            } else if (lean_hold >= (u8)LEAN_WIND_2) {
                lean_hold = (u8)(LEAN_WIND_2 - 1u);
            } else {
                lean_hold = 0u;
            }
        }
    }
    if (rough == 0u) {
        bump_phase = 0u;
        lift = 0u;
    } else {
        bump_phase++;
        lift = (u8)((bump_phase & 2u) ? rough : 0u);
    }
    wheel = (u8)(car_top() + body.height - 1u);
    {
        s16 t = (s16)car_top() - (s16)id_road_car_lift();
        top = (u8)((t < 0) ? 0 : t);
    }
    if (body.layers > 1u) {
        pose = lean_pose();
        mirror = (u8)((lean_side < 0) ? 1u : 0u);
        car_layer(body.cells, body.pose_first[body.poses + pose],
                  body.pose_cells[body.poses + pose],
                  body.pose_w[body.poses + pose],
                  mirror, lat, (u8)CAR_PAL2,
                  (u8)(0u ? ID_CAR_SPR_NEAR_0 : ID_CAR_SPR_0),
                  (u8)(0u ? ID_CAR_SPR_NEAR_0 + 12u : ID_CAR_SPR_MAX),
                  top, wheel, lift);
        car_layer(body.cells, body.pose_first[pose],
                  body.pose_cells[pose], body.pose_w[pose],
                  mirror, lat, (u8)CAR_PAL,
                  (u8)ID_CAR_SPR2_0, (u8)ID_CAR_SPR2_MAX, top,
                  wheel, lift);
        return;
    }
    pose = (lean_hold == 0u) ? 0u : 1u;
    mirror = (u8)((lean_side > 0) ? 1u : 0u);
    car_layer(body.cells, body.pose_first[pose], body.pose_cells[pose],
              body.pose_w[pose], mirror, lat, (u8)CAR_PAL,
              (u8)(0u ? ID_CAR_SPR_NEAR_0 : ID_CAR_SPR_0),
              (u8)(0u ? ID_CAR_SPR_NEAR_0 + 12u : ID_CAR_SPR_MAX),
              top, wheel, lift);
    id_spr_hide_range((u8)ID_CAR_SPR2_0, (u8)ID_CAR_SPR2_MAX);
}

