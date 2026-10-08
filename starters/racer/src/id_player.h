
#ifndef ID_PLAYER_H
#define ID_PLAYER_H
#include "ngpc_types.h"
#define ID_CAR_SPR_0    0u
#define ID_CAR_SPR_MAX  12u
#define ID_CAR_SPR2_0   52u
#define ID_CAR_SPR2_MAX 64u
#define ID_CAR_SPR_NEAR_0 8u
#define ID_CAR_WHEEL    103u
#define ID_CAR_PORTRAIT_0    32u
#define ID_CAR_PORTRAIT_MAX  64u
extern u8 id_spr_priority; 
void id_spr_put(u8 id, u8 x, u8 y, u16 tile, u8 pal, u8 flip);
#define ID_SPR_PUT_PRIORITY(id, x, y, tile, pal, flip, priority) do {                                  u8  spr_i_ = (u8)(id);                                                      u8  spr_x_ = (u8)(x);                                                       u8  spr_y_ = (u8)(y);                                                       u16 spr_t_ = (u16)(tile);                                                   u8  spr_p_ = (u8)(pal);                                                     u8  spr_f_ = (u8)(flip);                                                    volatile u8 *spr_s_ = (volatile u8 *)(0x8800u + ((u16)spr_i_ << 2));         spr_s_[0] = (u8)(spr_t_ & 0x00FFu);                                         spr_s_[1] = (u8)(spr_f_ | (u8)(priority) | (u8)((spr_t_ >> 8) & 1u));         spr_s_[2] = spr_x_;                                                         spr_s_[3] = spr_y_;                                                         *(volatile u8 *)(0x8C00u + (u16)spr_i_) = (u8)(spr_p_ & 0x0Fu);         } while (0)
#define ID_SPR_PUT(id, x, y, tile, pal, flip) \
    ID_SPR_PUT_PRIORITY(id, x, y, tile, pal, flip, id_spr_priority)
#define ID_SPR_PUT_MIDDLE(id, x, y, tile, pal, flip) \
    ID_SPR_PUT_PRIORITY(id, x, y, tile, pal, flip, (2u << 3))
void id_spr_hide_range(u8 from, u8 to);
void id_car_init(u16 tile_base, u8 all);
void id_car_select(u8 variant);
void id_car_portrait(u8 px, u8 py, u8 spr0);
void id_car_portrait_angle(u8 cxm, u8 py, u8 spr0, u8 angle);
u16 id_car_char_count(u8 all);
u8 id_car_half_width(void);
s8 id_car_lean_share(void);
u8 id_car_height(void);
u8 id_car_wheel_dx(void);
void id_car_draw(s16 lat, s8 lean, u8 rough);
#endif 

