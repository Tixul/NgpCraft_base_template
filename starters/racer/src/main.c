/* Minimal driving sample: first track, one car, no front end. */
#include "ngpc_hw.h"
#include "carthdr.h"
#include "ngpc_sys.h"
#include "ngpc_timing.h"
#include "ngpc_gfx.h"
#include "ngpc_sprite.h"
#include "ngpc_input.h"
#include "ngpc_text.h"
#include "id_road.h"
#include "id_player.h"
#include "id_fleet.h"
#include "id_sky.h"
#include "id_scenery.h"
#include "id_cars.h"
#include "track.h"
#include "driving.inc"

/* Change to zero to use manual transmission. */
#define TEMPLATE_AUTO 1u
static s16 road_bend[ID_ROAD_LINES_MAX];
static u16 course_len,track_frac,post_base;
static u16 elapsed_ticks;
static u8 clock_prev,pace_prev,paused;


/* Same bounded projection sampler as the full engine; no per-line division. */
static s8 grade_tab[13];
typedef struct {
    const u16     *ahead_end;   
    s16           *bend_end;    
    s8            *gl_end;      
    const Segment *seg;         
    const s8      *gtab;        
    const Segment *sc;          
    u16 into;                   
    u16 w;                      
    u8  k;                      
    u8  cursor;                 
    u8  len;                    
    
    u8  fkey;                   
    s8  fg;                     
    u8  nkey;                   
    u8  pkey;                   
    u8  pad;                    
    s16 ndb, ndp;               
    s16 pdb, pdp;               
} RoadAhead;

typedef char road_ahead_is_44[(sizeof(RoadAhead) == 44u) ? 1 : -1];
typedef char segment_is_6[(sizeof(Segment) == 6u) ? 1 : -1];

extern void id_road_ahead_asm(RoadAhead *a);

static void profile_ahead(void)
{
    RoadAhead a;
    u8 lines = id_road_lines();

    if (lines == 0u) {
        return;
    }
    a.ahead_end = id_road_ahead() + lines;
    a.bend_end = road_bend + lines;
    a.gl_end = id_road_grade_buf() + lines;
    a.seg = track_seg;
    a.gtab = &grade_tab[6u];
    a.sc = &track_seg[seg_index];
    a.into = seg_pos;
    a.k = lines;
    a.cursor = seg_index;
    a.len = (u8)TRACK_LEN;
    a.fkey = 0xFFu;
    a.nkey = 0xFFu;
    a.pkey = 0xFFu;
    id_road_ahead_asm(&a);
}

static void panel(void)
{
    u8 x,y; u16 i;
    for(i=0u;i<8u;i++) ((volatile u16 *)0xA010u)[i]=0x5555u;
    ngpc_gfx_set_palette(GFX_SCR2,7u,0u,0u,0u,0u);
    ngpc_gfx_set_palette(GFX_SCR1,0u,0u,0x0FFFu,0x0FFFu,0x0FFFu);
    /* Road uses palettes 0-1; sky owns 8-15. Palette 7 is reserved for the HUD.
       The raster displays source rows 20..23 behind the stationary HUD. */
    for(y=20u;y<24u;y++) for(x=0u;x<32u;x++) HW_SCR2_MAP[y*32u+x]=1u+(7u<<9);
}

/* Static labels stay in VRAM; only changed numeric fields are rewritten. */
static u8 hud_state=255u,hud_speed=255u,hud_gear=255u;
static u16 hud_seconds=65535u,hud_hundredths=65535u;
static void hud(void)
{
    u16 seconds=(u16)(elapsed_ticks/60u);
    u16 hundredths=(u16)((elapsed_ticks%60u)*100u/60u);
    u8 state=finished?2u:paused?1u:0u;
    u8 mph=(u8)((u16)speed*2u/5u);
    if(hud_state==255u) {
        ngpc_text_print(GFX_SCR1,0u,0u,16u,"TIME 00:00.00      ");
        ngpc_text_print(GFX_SCR1,0u,0u,17u,"MPH 000  GEAR 0    ");
        ngpc_text_print(GFX_SCR1,0u,0u,18u,"A:GAS B:BRAKE OPT:P ");
    }
    if(state!=hud_state) {
        ngpc_text_print(GFX_SCR1,0u,0u,15u,finished?"FINISH! A:RESTART   ":paused?"PAUSED A:GO B:RESET ":"RACING ENGINE      ");
        hud_state=state;
    }
    if(seconds!=hud_seconds) {
        ngpc_text_print_dec(GFX_SCR1,0u,5u,16u,(u16)(seconds/60u),2u);
        ngpc_text_print_dec(GFX_SCR1,0u,8u,16u,(u16)(seconds%60u),2u);
        hud_seconds=seconds;
    }
    if(hundredths!=hud_hundredths) {
        ngpc_text_print_dec(GFX_SCR1,0u,11u,16u,hundredths,2u);
        hud_hundredths=hundredths;
    }
    if(mph!=hud_speed) {
        ngpc_text_print_dec(GFX_SCR1,0u,4u,17u,mph,3u);hud_speed=mph;
    }
    if(gear!=hud_gear) {
        ngpc_text_print_dec(GFX_SCR1,0u,14u,17u,(u16)(gear+1u),1u);hud_gear=gear;
    }
}

static void reset_run(void)
{
    speed=gear=slip=offroad=finished=paused=seg_index=0u;
    speed_acc=seg_pos=course_pos=track_frac=recovery_frames=0u;
    car_lat=hill_acc=heading_acc=bend_now=0; car_lean=0;
    on_throttle=steer_hold=oil_hold=0u;
    auto_box=TEMPLATE_AUTO; racing=1u; elapsed_ticks=0u;
    id_scenery_init(post_base,0u);
    clock_prev=pace_prev=g_vb_counter;
}

static u16 advance(void)
{
    u16 base=speed<16u?(u16)(((u16)speed*3502u)>>4):3502u;
    u16 acc=(u16)(track_frac+(u16)speed*123u+base);
    u16 step=(u16)(acc>>12);
    track_frac=(u16)(acc&0x0FFFu);
    if(step >= (u16)(course_len-course_pos)) {
        course_pos=course_len;finished=1u;racing=0u;speed=0u;
        return 0u;
    }
    course_pos=(u16)(course_pos+step);seg_pos=(u16)(seg_pos+step);
    while(seg_pos>=track_seg[seg_index].span) {
        seg_pos=(u16)(seg_pos-track_seg[seg_index].span);
        seg_index++;if(seg_index>=TRACK_LEN) seg_index=0u;
    }
    {
        s16 target=bend_blended(seg_index,seg_pos);
        /* Use the game's six-unit easing; drive consumes the previous step. */
        if(bend_now<target-6) bend_now+=6;
        else if(bend_now>target+6) bend_now-=6;
        else bend_now=target;
    }
    return step;
}

void main(void)
{
    u16 next; u8 i; s16 g;
    ngpc_init();ngpc_load_sysfont();
    ngpc_gfx_set_viewport(0u,0u,160u,152u);
    ngpc_gfx_clear(GFX_SCR1);ngpc_gfx_clear(GFX_SCR2);ngpc_sprite_hide_all();
    id_road_init();id_road_dma_pause();
    next=(u16)(ID_TILE_BASE+id_road_char_count());
    id_sky_load(0u,next);id_sky_draw(0u);next=(u16)(next+id_sky_char_count(0u));
    id_car_select(0u);id_car_init(next,0u);next=(u16)(next+id_car_char_count(0u));
    post_base=next;
    if(next+id_scenery_char_count()>512u) for(;;) ngpc_vsync();
    panel();
    road_edge=(s16)((s16)id_road_half_at(ID_CAR_WHEEL)*16);
    lat_limit=(s16)((s16)id_road_half_at(id_road_bottom())*16+LAT_MARGIN*16);
    if(lat_limit>ID_ROAD_SWEEP_MAX*16) lat_limit=ID_ROAD_SWEEP_MAX*16;
    course_len=0u;for(i=0u;i<TRACK_LEN;i++) course_len=(u16)(course_len+descent_track[i].span);
    for(g=-6;g<=6;g++) {
        s16 room=g>0?id_road_grade_hi():-id_road_grade_lo();
        grade_tab[(u8)(g+6)]=(s8)(g*room/6);
    }
    reset_run();id_road_dma_resume();
    for(;;) {
        u16 step=0u;u8 now;
        while((u8)(g_vb_counter-pace_prev)<2u) ngpc_vsync();
        /* Preserve the game's two-VBlank schedule through a small overrun. */
        pace_prev=(u8)(pace_prev+2u);
        if((u8)(g_vb_counter-pace_prev)>=2u) pace_prev=g_vb_counter;
        ngpc_input_update();now=g_vb_counter;
        if(ngpc_pad_pressed&PAD_OPTION) paused=(u8)!paused;
        if(finished && (ngpc_pad_pressed&PAD_A)) reset_run();
        if(paused) {
            if(ngpc_pad_pressed&PAD_B) reset_run();
            else if(ngpc_pad_pressed&PAD_A) paused=0u;
        } else if(!finished) {
            u8 delta=(u8)(now-clock_prev);
            elapsed_ticks=elapsed_ticks>(u16)(65535u-delta)?65535u:(u16)(elapsed_ticks+delta);
            drive();step=advance();
        }
        clock_prev=now;
        profile_ahead();
        id_road_band_phase(course_pos);
        id_road_hill_ahead(id_road_grade_buf());
        id_road_pan(heading_acc,car_lat);
        id_road_frame_ahead((s16)(car_lat>>4),road_bend,(s16)(heading_acc>>4));
        id_scenery_frame((u8)((paused||finished)?0u:speed));
        if(!paused && !finished && (car_lat>road_edge || car_lat<-road_edge)) {
            u8 mine=(u8)(car_lat>0?2u:1u);
            if(id_scenery_passed()&mine) {
                u8 cost=POST_HIT,cap=(u8)(speed>>1);
                if(cost>cap) cost=cap;
                speed=(speed>cost)?(u8)(speed-cost):0u;
            }
        }
        id_car_draw((s16)(car_lat>>4),car_lean,0u);hud();
    }
}
