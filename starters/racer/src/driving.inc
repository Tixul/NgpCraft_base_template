/* Acceleration, transmission, steering, hills and off-road recovery. */
#define SPEED_MAX      240u
#define DOWNHILL_MARGIN 16u
#define SPEED_DRAG       1u
#define GEARS            ID_GEARS
#define GEAR_LIMIT_DRAG  2u
#define HILL_DEAD        8

#define HILL_TOP         3u     
#define HILL_PULL        6      

#define HILL_TOP_FLOOR  40u
static const u8 torque_curve[8] = { 12u, 40u, 96u, 176u, 240u, 255u, 230u, 170u };
static const u8 gear_pull[ID_GEARS] = { 255u, 200u, 160u, 130u, 108u };
#define POWER_BAND      96u     
#define CLUTCH_SPEED    20u
#define CLUTCH_REVS    176u
#define STEER_LOAD      53
#define SLIP_FRAMES     20u
            
static s16 road_edge;           
static s16 lat_limit;           
#define LAT_MARGIN     12       
#define OFFROAD_DRAG_MIN  0u
#define HEADING_SHIFT     7
#define HIT_DRAG_SIDE     8u
#define HIT_REAR_MIN      6u
#define VERGE_HIT        24u
#define VERGE_TOLERANCE_PX 5
#define VERGE_SOFT_PX      5  
#define POST_HIT         48u


#define BEND_HALF 128u
#define RECOVERY_SPEED 8u
#define RECOVERY_FRAMES 180u
#define RECOVERY_PROGRESS (2*16)
static const Segment *track_seg=descent_track;
static IdCar car_tuned={{46u,92u,119u,159u,224u},2u,0u,6u,80u,130u,0u,0u};
static u8 speed,gear,slip,auto_box=1u,on_throttle,steer_hold,offroad,racing,finished;
static u16 speed_acc,seg_pos,course_pos,recovery_frames;
static s16 hill_acc,car_lat,bend_now,heading_acc,recovery_lat;
static s8 car_lean;
static u8 seg_index,recovery_vblank,oil_hold;
static s16 bend_blended(u8 cursor, u16 into)
{
    s16 b = track_seg[cursor].bend;
    u16 rem = (u16)(track_seg[cursor].span - into);
    u8 other;
    s16 towards;
    u16 w;
    if (rem <= into && rem < (u16)BEND_HALF) {
        other = (u8)(cursor + 1u);
        if (other >= (u8)TRACK_LEN) {
            other = 0u;
        }
        w = (u16)((u16)BEND_HALF - rem);
    } else if (into < (u16)BEND_HALF) {
        other = (u8)((cursor != 0u) ? (cursor - 1u) : (u8)(TRACK_LEN - 1u));
        w = (u16)((u16)BEND_HALF - into);
    } else {
        return b;
    }
    towards = (s16)(track_seg[other].bend - b);
    return (s16)(b + (s16)((towards * (s16)w) >> 8));
}
static s8 slope_blended(u8 cursor, u16 into)
{
    s16 b = (s16)track_seg[cursor].slope;
    u16 rem = (u16)(track_seg[cursor].span - into);
    u8 other;
    s16 towards;
    u16 w;
    if (rem <= into && rem < (u16)BEND_HALF) {
        other = (u8)(cursor + 1u);
        if (other >= (u8)TRACK_LEN) {
            other = 0u;
        }
        w = (u16)((u16)BEND_HALF - rem);
    } else if (into < (u16)BEND_HALF) {
        other = (u8)((cursor != 0u) ? (cursor - 1u) : (u8)(TRACK_LEN - 1u));
        w = (u16)((u16)BEND_HALF - into);
    } else {
        return (s8)b;
    }
    towards = (s16)((s16)track_seg[other].slope - b);
    return (s8)(b + (s16)((towards * (s16)w) >> 8));
}
static u8 engine_revs(void)
{
    const IdCar *car = &car_tuned;
    u16 top = (u16)car->gear_top[gear];
    if (slip > 0u) {
        return 255u;            
    }
    if (top == 0u) {
        return 0u;
    }
    if (speed >= (u8)top) {
        return 255u;
    }
    {
        u8 r = (u8)(((u16)speed * 255u) / top);
        if (on_throttle && gear == 0u && speed < (u8)CLUTCH_SPEED) {
            u8 lift = (u8)((u16)CLUTCH_REVS
                           * (u16)((u8)CLUTCH_SPEED - speed)
                           / (u16)CLUTCH_SPEED);
            if (lift > r) {
                r = lift;
            }
        }
        return r;
    }
}
static void recover_stranded(s16 tyre_edge)
{
    s16 distance = car_lat < 0 ? (s16)-car_lat : car_lat;
    s16 previous = recovery_lat < 0 ? (s16)-recovery_lat : recovery_lat;
    u8 now = g_vb_counter;
    if (!racing || finished || !offroad
        || speed > (u8)RECOVERY_SPEED
        || distance <= tyre_edge + VERGE_SOFT_PX * 16) {
        recovery_frames = 0u;
        return;
    }
    if (recovery_frames == 0u
        || (car_lat < 0) != (recovery_lat < 0)
        || distance + RECOVERY_PROGRESS <= previous) {
        recovery_frames = 1u;
        recovery_lat = car_lat;
        recovery_vblank = now;
        return;
    }
    recovery_frames = (u16)(recovery_frames + (u8)(now - recovery_vblank));
    recovery_vblank = now;
    if (recovery_frames < (u16)RECOVERY_FRAMES) return;
    car_lat = 0;
    speed = 0u;
    speed_acc = 0u;
    hill_acc = 0;
    gear = 0u;
    slip = 0u;
    car_lean = 0;
    steer_hold = 0u;
    offroad = 0u;
    recovery_frames = 0u;
}
static void drive(void)
{
    s16 drift;
    u8 ceiling;
    s8 grade;                           
    u8 v16;                            
    const IdCar *car = &car_tuned;
    u8 revs;
    u8 grip = car->grip;
    if (oil_hold != 0u) {
        grip = (u8)(grip - (u8)(grip >> 2));
        if (grip == 0u) {
            grip = 1u;
        }
    }
    on_throttle = (u8)((ngpc_pad_held & PAD_A) ? 1u : 0u);
    if ((ngpc_pad_pressed & (PAD_LEFT | PAD_RIGHT))
        || !(ngpc_pad_held & (PAD_LEFT | PAD_RIGHT))) {
        steer_hold = 0u;
    } else if (steer_hold < 255u) {
        steer_hold++;
    }
    revs = engine_revs();
    grade = slope_blended(seg_index, seg_pos);
    {
        s16 top = (s16)((s16)car->gear_top[gear] - (s16)grade * (s16)HILL_TOP);
        s16 cap = (s16)((s16)car->gear_top[(u8)(GEARS - 1u)]
                        + (s16)DOWNHILL_MARGIN);
        if (cap > 255) {
            cap = 255;
        }
        if (top > cap) {
            top = cap;
        } else if (top < (s16)HILL_TOP_FLOOR) {
            top = (s16)HILL_TOP_FLOOR;
        }
        ceiling = (u8)top;
    }
    if (auto_box) {
        u16 sous_plafond = (car->gear_top[gear] < ceiling)
                           ? (u16)car->gear_top[gear] : (u16)ceiling;
        u16 monte = (u16)((sous_plafond * 230u) >> 8);
        if (gear < (u8)(GEARS - 1u) && (u16)speed >= monte) {
            gear++;
        } else if (gear > 0u
                   && (u16)speed < (u16)(((u16)car->gear_top[gear - 1u]
                                          * 166u) >> 8)) {
            gear--;
        }
    }
    if (!auto_box && (ngpc_pad_pressed & PAD_DOWN) && gear < (u8)(GEARS - 1u)) {
        if (revs < (u8)POWER_BAND) {
            slip = (u8)SLIP_FRAMES;
        }
        gear++;
    } else if (!auto_box && (ngpc_pad_pressed & PAD_UP) && gear > 0u) {
        gear--;
        if (speed > car->gear_top[gear]) {
            slip = (u8)SLIP_FRAMES;
        }
    }
    if (slip > 0u) {
        slip--;
        speed = (speed > SPEED_DRAG) ? (u8)(speed - SPEED_DRAG) : 0u;
    } else if ((ngpc_pad_held & PAD_A)) {
        if (speed > ceiling) {
            speed = (u8)((speed - ceiling > GEAR_LIMIT_DRAG)
                         ? speed - GEAR_LIMIT_DRAG : ceiling);
        } else {
            u16 f = (u16)(((u16)torque_curve[revs >> 5] *
                           (u16)gear_pull[gear]) >> 8);
            u16 add = (u16)(((u16)car->accel * f *
                             (u16)(8u + car->accel8)) >> 3);
            speed_acc = (u16)(speed_acc + add);
            if (speed_acc >= 256u) {
                u8 whole = (u8)(speed_acc >> 8);
                speed_acc = (u16)(speed_acc & 255u);
                if (speed <= (u8)(ceiling - whole)) {
                    speed = (u8)(speed + whole);
                } else {
                    speed = ceiling;
                }
            }
        }
    } else if ((ngpc_pad_held & PAD_B)) {
        speed = (speed > car->brake) ? (u8)(speed - car->brake) : 0u;
    } else if (speed > SPEED_DRAG) {
        speed = (u8)(speed - SPEED_DRAG);
    } else {
        speed = 0u;
    }
    hill_acc = (s16)(hill_acc - (s16)grade * (s16)HILL_PULL);
    while (hill_acc >= 256) {
        hill_acc = (s16)(hill_acc - 256);
        if ((s16)speed < (s16)((s16)car->gear_top[(u8)(GEARS - 1u)]
                               + (s16)DOWNHILL_MARGIN)
            && speed < 255u) {
            speed++;
        }
    }
    while (hill_acc <= -256) {
        hill_acc = (s16)(hill_acc + 256);
        if (speed > 0u) {
            speed--;
        }
    }
    v16 = (u8)(speed >> 4);
    {
        s16 wheel = (s16)id_cars_steering(speed, car->steer);
        if (oil_hold != 0u) {
            wheel = (s16)(wheel - (s16)(wheel >> 3));
        }
        if (offroad) {
            wheel = (s16)(wheel - (s16)(wheel >> 2));       
        }
        if ((ngpc_pad_held & PAD_LEFT)) {
            car_lat = (s16)(car_lat - wheel);
        } else if ((ngpc_pad_held & PAD_RIGHT)) {
            car_lat = (s16)(car_lat + wheel);
        } else {
            s8 reste = id_car_lean_share();
            if (reste != 0) {
                car_lat = (s16)(car_lat + (s16)(((s16)wheel * (s16)reste) >> 4));
            }
        }
    }
    drift = (s16)(((s16)bend_now * (s16)v16 * (s16)v16) / (s16)grip);
    car_lat = (s16)(car_lat + drift);
    heading_acc = (s16)(heading_acc +
                        (s16)((bend_now * (s16)(speed >> 4)) >> HEADING_SHIFT));
    if ((ngpc_pad_held & PAD_LEFT)) {
        car_lean = -1;
    } else if ((ngpc_pad_held & PAD_RIGHT)) {
        car_lean = 1;
    } else {
        car_lean = 0;
    }
    {
        u8 was = offroad;
        s16 tyre_edge = (s16)(road_edge
            - ((s16)id_car_wheel_dx() + 2 - VERGE_TOLERANCE_PX) * 16);
        u8 shallow;
        offroad = (u8)((car_lat >= tyre_edge || car_lat <= -tyre_edge) ? 1u : 0u);
        shallow = (u8)(car_lat <= tyre_edge + VERGE_SOFT_PX * 16
                    && car_lat >= -tyre_edge - VERGE_SOFT_PX * 16);
        if (offroad && !was) {
            u8 hit = shallow ? (u8)(VERGE_HIT / 2u) : (u8)VERGE_HIT;
            speed = (speed > hit) ? (u8)(speed - hit) : 0u;
        }
        if (offroad) {
            u8 d = (u8)(speed >> 4);
            if (shallow) d >>= 1;
            if (d < (u8)OFFROAD_DRAG_MIN) {
                d = (u8)OFFROAD_DRAG_MIN;
            }
            speed = (speed > d) ? (u8)(speed - d) : 0u;
        }
        recover_stranded(tyre_edge);
    }
    if (car_lat > lat_limit) {
        car_lat = lat_limit;
    } else if (car_lat < -lat_limit) {
        car_lat = -lat_limit;
    }
}
