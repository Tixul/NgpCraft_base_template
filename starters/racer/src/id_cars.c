#include "id_cars.h"
u8 id_cars_steering(u8 speed,u8 steer) {
    u16 scale= speed<96u ? (u16)(((u16)speed*7u)/6u) : (u16)(112u-((speed-96u)>>3));
    return (u8)(((u16)steer*scale)>>7);
}
