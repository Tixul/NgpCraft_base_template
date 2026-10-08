/*
 * road3d_example.c - forward-view pseudo-3D scaling road (racer)
 *
 * Part of NgpCraft_base_template (MIT License)
 *
 * The mechanism is the one Densha de Go! 2 uses, recovered by RUNTIME
 * measurement of the retail cartridge rather than from a listing (see
 * measurement notes kept outside this repository). Every constant below
 * traces to something that was measured on hardware-accurate emulation:
 *
 *   - ONE tiled plane is sheared per scanline (SCR2). The second plane is NOT
 *     a parallax layer -- Densha never fills its per-line table in 600 frames
 *     of driving. Keep SCR1 for a fixed HUD in front (P.F bit7 = SCR1 first).
 *   - The road is NOT the whole screen: a constant "horizon strip" then N
 *     bands, one band per scanline, and untouched lines above and below.
 *   - The fan comes from sampling a signed profile LUT at a QUADRATIC index,
 *     idx(k) = k*k / DIV -- measured as the HIGH word of (32k)^2, i.e. k*k/64
 *     for Densha's 48 bands. Not a per-line double integration.
 *   - Curvature and forward motion are the SAME control: a base pointer that
 *     SLIDES through the profile. Densha's road is dead straight while the
 *     window sits in the profile's zero head, and bends as the window reaches
 *     the ramp.
 *
 * WHAT A RACER NEEDS THAT A TRAIN DOES NOT: free steering. Densha's driver
 * cannot aim, so the game only slides the window. Here `steer` biases the
 * whole road sideways on top of the track's own curvature -- which is what
 * moves the vanishing point when you turn in, and what makes a racer feel
 * like one.
 *
 * Usage:
 *   road_init();
 *   for (;;) { ngpc_vsync(); road_frame(throttle, ngpc_input_x()); }
 */

#include "ngpc_types.h"
#include "ngpc_gfx.h"        /* GFX_SCR1, GFX_SCR2, ngpc_gfx_scroll */
#include "fx/ngpc_raster.h"  /* ngpc_raster_init, ngpc_raster_set_scroll_table */

/* ---- Geometry ----
 *
 * Densha: horizon strip on lines 56..79, 48 bands on lines 80..127, nothing
 * elsewhere -- so its road stops 24 lines short of the screen bottom (the HUD
 * sits there). A racer wants the road under the player's nose, so the bands
 * run all the way down.
 *
 * ROAD_DIV keeps Densha's exact foreshortening SHAPE at any band count: the
 * profile window must stay 37 entries wide, so DIV = BANDS*BANDS / 36.
 *   48 bands -> DIV  64   (Densha, verbatim)
 *   96 bands -> DIV 256   (this file: same curve, twice the vertical detail)
 */
#define ROAD_LINES      152u
#define ROAD_HORIZON     56u              /* first line the road touches      */
#define ROAD_BANDS       96u              /* one band = one scanline          */
#define ROAD_DIV        256u              /* idx(k) = k*k / ROAD_DIV          */
#define ROAD_WINDOW      37u              /* profile entries the bands read   */
#define ROAD_STRIP_TOP   40u              /* constant strip: lines 40..55     */
#define ROAD_CX          80u              /* screen centre X (160/2)          */

/* ---- The track ----
 *
 * A signed profile, in the same shape Densha ships: a long flat head (straight
 * track) followed by ramps (bends). The window slides through it as you drive,
 * so ONE array is both the track layout and the curve generator. Left and right
 * are simply the sign of the bytes.
 *
 * Length must cover slide_max + ROAD_WINDOW.
 */
#define PROFILE_LEN     256u

static const s8 track_profile[PROFILE_LEN] = {
    /* +0   : 48 entries of straight */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* +48  : opening right-hander */
     0,-1,-1,-2,-2,-3,-4,-4,-5,-6,-7,-8,-9,-10,-11,-12,
   -13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,
    /* +80  : holding the turn */
   -28,-28,-28,-28,-28,-28,-28,-28,-27,-26,-25,-24,-22,-20,-18,-16,
    /* +96  : back to straight */
   -14,-12,-10, -8, -6, -4, -2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* +128 : hard left */
     0, 1, 2, 3, 5, 7, 9, 11, 13, 16, 19, 22, 25, 28, 31, 34,
    36,38,40,41,42,42,42,42,42,42,41,40,38,36,33,30,
    /* +160 : unwind */
    27,24,21,18,15,12,10, 8, 6, 4, 2, 1, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* +192 : chicane */
     0,-2,-4,-6,-8,-9,-10,-10, -9, -7, -4,  0,  4,  7,  9, 10,
    10, 9, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* +224 : run-in to the finish */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define SLIDE_MAX  ((u16)(PROFILE_LEN - ROAD_WINDOW - 1u))

/* ---- Per-line scroll tables ----
 *
 * Double buffered, because the ISR consumes a table line by line WHILE this
 * code builds the next one. Densha ping-pongs correctly for its bands but
 * picks the buffer for its horizon strip from a DIFFERENT counter, so one
 * frame in three that strip lands in the buffer being displayed. Harmless
 * there (the strip is a constant) but it is a bug -- one parity only.
 */
static u8 road_tbl[2][ROAD_LINES];
static u8 road_page;                  /* the buffer being BUILT              */

/* ---- Camera ---- */
static u16 slide;                     /* window position in the profile      */
static u16 slide_frac;                /* sub-step accumulator                */
static u8  road_y;                    /* vertical scroll = forward motion    */
static s8  steer;                     /* player's lateral aim                */

/* idx(k) = k*k / ROAD_DIV, the measured law. k*k fits u16 for k <= 255. */
static u8 road_index(u8 k)
{
    u16 sq;

    sq = (u16)k * (u16)k;
    return (u8)(sq / ROAD_DIV);
}

/* Build one table. Band k counts UPWARD from the bottom of the road, so band 1
 * is the line under the player and band ROAD_BANDS is at the horizon -- that
 * is why Densha writes its table with a pre-decrementing pointer. The deepest
 * bands move most; the line at your wheels never moves. */
static void road_build(u8 *tbl)
{
    u8  k;
    u8  line;
    s16 x;
    s8  ref;

    ref = track_profile[slide];

    /* Above the road: sky / fixed. */
    for (line = 0u; line < ROAD_STRIP_TOP; ++line) {
        tbl[line] = (u8)ROAD_CX;
    }
    /* The constant strip between sky and road (Densha's 0x449B band). It
     * carries the steering alone: distance beyond the horizon is meaningless,
     * so there is no curve left to apply, only where you are pointing. */
    for (line = ROAD_STRIP_TOP; line < ROAD_HORIZON; ++line) {
        tbl[line] = (u8)((s16)ROAD_CX + (s16)steer);
    }
    /* The bands. */
    for (k = 1u; k <= ROAD_BANDS; ++k) {
        line = (u8)(ROAD_LINES - k);          /* band 1 -> line 151 */
        if (line < ROAD_HORIZON) {
            break;
        }
        x = (s16)ROAD_CX
          + (s16)track_profile[slide + road_index(k)] - (s16)ref
          + (s16)steer;
        tbl[line] = (u8)x;
    }
}

/* ---- Public ---- */

void road_init(void)
{
    slide = 0u;
    slide_frac = 0u;
    road_page = 0u;
    road_y = 0u;
    steer = 0;

    ngpc_raster_init();
    road_build(road_tbl[0]);
    road_build(road_tbl[1]);
    /* Only SCR2 is sheared. SCR1 keeps a flat table (pass NULL) and carries
     * the HUD in front of the road. */
    ngpc_raster_set_scroll_table(GFX_SCR2, road_tbl[0], (const u8 *)0);
}

/*
 * One frame. Call after ngpc_vsync(), never from an interrupt.
 *
 *   speed:  0..255, how fast the window slides = how fast you are going
 *   aim:    -64..63, steering input
 *
 * Densha slides ~1 profile entry every 6 frames at 28 km/h. `speed` here is a
 * 1/64th-of-an-entry-per-frame rate, so speed=11 reproduces that, and a racer
 * at full tilt (speed=255) covers 4 entries a frame.
 */
void road_frame(u8 speed, s8 aim)
{
    u8 *next;

    steer = aim;

    slide_frac = (u16)(slide_frac + (u16)speed);
    slide = (u16)(slide + (slide_frac >> 6));
    slide_frac &= 0x3Fu;
    if (slide > SLIDE_MAX) {
        slide = 0u;                    /* lap: wrap to the start line */
    }

    /* Forward motion is the vertical scroll of the road tiles. Densha instead
     * streams fresh tile rows and only toggles this between two values -- do
     * that once the road art is longer than one map. */
    road_y = (u8)(road_y + (u8)(speed >> 3));

    next = road_tbl[road_page];
    road_build(next);
    ngpc_raster_set_scroll_table(GFX_SCR2, next, (const u8 *)0);
    ngpc_gfx_scroll(GFX_SCR2, 0u, road_y);
    road_page ^= 1u;                   /* ONE parity, for both regions */
}

/*
 * Relief (signs, rivals, trees) is NOT part of the shear: the K2GE has no
 * sprite scaling, so pick a pre-baked metasprite by distance and place it on
 * the road's centreline for that depth -- which the table you just built
 * already holds.
 *
 *   depth: 0 = at the player, ROAD_BANDS = at the horizon
 *   side:  lateral offset in road units
 */
u8 road_screen_x(u8 depth, s8 side)
{
    u8 line;

    if (depth > ROAD_BANDS) {
        depth = (u8)ROAD_BANDS;
    }
    line = (u8)(ROAD_LINES - 1u - depth);
    if (line < ROAD_HORIZON) {
        line = (u8)ROAD_HORIZON;
    }
    /* The centreline for that depth, plus the object's own offset, shrunk with
     * distance the same way the road is. */
    return (u8)((s16)road_tbl[road_page ^ 1u][line]
                + (((s16)side * (s16)(ROAD_BANDS - depth)) / (s16)ROAD_BANDS));
}

u8 road_screen_y(u8 depth)
{
    if (depth > ROAD_BANDS) {
        depth = (u8)ROAD_BANDS;
    }
    return (u8)(ROAD_LINES - 1u - depth);
}
