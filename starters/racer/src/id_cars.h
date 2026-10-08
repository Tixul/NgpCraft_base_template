#ifndef ID_CARS_H
#define ID_CARS_H
#include "ngpc_types.h"
#define ID_GEARS 5u
typedef struct {
    u8 gear_top[ID_GEARS];  
    u8 accel;               
    u8 accel8;              
    u8 brake;               
    u8 steer;               
    u8 grip;                
    u16 price;              
    u8 drive;               
} IdCar;
u8 id_cars_steering(u8 speed,u8 steer);
#endif
