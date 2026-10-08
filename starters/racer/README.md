# OVER REV - racing engine template

A small, standalone Neo Geo Pocket Color driving template extracted from
OVER REV. It starts directly on the first track with one car. Use it as a
starting point for your own racing homebrew.

## Included

- Scanline road rendering, moving road markings and a single background.
- A segmented track with smoothly blended bends and hills.
- Acceleration, braking, automatic/manual transmission and steering.
- Off-road slowdown and recovery, roadside posts and an animated car.
- A simple speed/gear/time display, pause and restart.
- The C/assembly sources, graphics data, build scripts and a playable ROM.

This template is silent and has no persistent save. It includes no menus,
story campaigns, extra vehicles/tracks, unlock system, secret-code detector,
online-score export or multiplayer. Those game modules and their data are
absent from the source package, rather than hidden behind a build flag.

## Play

Load [bin/raceengine.ngc](bin/raceengine.ngc) in a compatible NGPC emulator.
The race is ready to drive immediately; there is no title screen or countdown.

| Console control | Action |
|---|---|
| A | Accelerate |
| B | Brake |
| Left / Right | Steer |
| Down / Up | Shift up / down when manual transmission is enabled |
| OPTION | Pause / resume |
| A while paused | Resume |
| B while paused | Restart the track |
| A after finishing | Start a new run |

Keyboard mappings depend on your emulator. The sample clock saturates at
65535 ticks (18:12.25), and a finished run stays on screen until restarted.

## Build

Requires Python 3 and the Toshiba TLCS-900/H toolchain, including `cc900`,
`thc1`, `thc2`, `asm900`, `tulink`, `tuconv` and `s242ngp`. The SDK is not
bundled. The build has been tested on Windows; GNU Make is not required.

From this folder:

```powershell
python build.py --thome C:/path/to/t900
```

Alternatively, set `THOME` to the SDK root (the directory containing `BIN`,
`INCLUDE` and `LIB`) and run `python build.py` or `build.bat`.

Output: `bin/raceengine.ngc`. Every build recompiles all modules, including
header changes. There is no flash-save padding: the sample ROM is about 31 KiB.
The build does not modify your OVER REV game or require its source directory.

## Make it your own

| File | What to change |
|---|---|
| `src/track.h` | Track segments: positive span, bend strength and slope. Keep the total length within 65535 distance units. |
| `src/main.c` | Direct-start loop, simple HUD, pause/restart; set `TEMPLATE_AUTO` to `0` for manual transmission. |
| `src/driving.inc` | Car parameters, acceleration, grip, hill effects and off-road behaviour. |
| `src/id_road.c` / `src/id_road_asm.asm` | Road projection, scanline tables and raster rendering. |
| `src/id_player.c` / `src/id_fleet.c` | Car sprite loading and drawing. |
| `src/id_scenery.c` / `GraphX/id_borne.c` | Original first-track roadside posts, perspective, road-edge placement and sprite sizes. |
| `src/id_sky.c` / `GraphX/` | The sample's single background, road image and car graphics. |
| `src/core/carthdr.h` | Cartridge title: exactly 12 ASCII characters. |

The renderer uses hardware raster DMA and double-buffered scanline tables.
The VBlank hook rearms DMA independently of the driving loop. Keep that
relationship intact when changing the frame loop. The sample targets one
update per two VBlanks. It waits only when that interval has not yet elapsed;
it does not promise a fixed frame rate under every load.

Font tiles occupy 32-127; road and graphics allocation begins at 128. The
sample checks that the selected road/background/car/post tiles fit below 512.
The fixed HUD reads its background from source map rows 20-23; those rows
must not overwrite the road image. Road graphics use SCR2 palettes 0-1,
the HUD reserves palette 7, and the panorama owns palettes 8-15. Preserve
these allocations when replacing graphics.

## Validation

The supplied ROM has been tested in the native NGPC emulator with a BIOS:
direct boot, acceleration/shifts, steering, braking, pause/resume, restarting
from pause and finish, clock saturation and a complete first-track run using
ordinary controller input. Validation also checked a stack canary and that
the symbol-instrumented ROM was byte-identical to the supplied ROM.

See [VALIDATION.json](VALIDATION.json) for the ROM hash and measured results.
The archive was rebuilt in a separate directory to check that it is standalone.
No physical-console validation is claimed for this extraction.

MIT license; preserve the notices in [LICENSE](LICENSE) and the source files.

## Revision - 2026-10-04

- Fixed background colour corruption: the HUD no longer overwrites the
  panorama's final palette.
- Restored the engine's assembly look-ahead sampler and precomputed hill
  scaling, replacing repeated per-scanline traversal and division.
- Removed the mandatory extra VBlank wait after a completed render.
- Cached HUD text and values; unchanged fields stay in VRAM.
- Restored the original COMET acceleration, braking, steering, grip and
  transmission ratios instead of the earlier modified sample tuning.
- Replaced the simplified posts with the game's first-track scenery code
  and graphics: five sprite sizes, perspective and margins sampled from
  the road geometry, including bends and hills.
- Corrected off-road detection to sample the road width at the wheels.
  Passing a roadside post off-road applies the original speed penalty.
- Restored the game's gradual bend easing and driving update order.
- Preserved the game's two-VBlank pacing through small rendering overruns,
  instead of resetting the schedule after every update.

See `VALIDATION.json` for the current ROM's release checks.
