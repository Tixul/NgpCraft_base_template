# Starters

Complete, standalone game skeletons extracted from finished NgpCraft games.
Unlike `optional/` modules, a starter is a **whole project**: it carries its
own copy of the core (`src/core`, `src/gfx`, `src/fx`), its own linker file and
its own build script. Copy the folder out and make it your game.

| Starter | From | What you get |
|---|---|---|
| [`racer/`](racer/README.md) | OVER REV (2026) | Raster-DMA scanline road, segmented track with bends and hills, COMET driving physics (automatic/manual gearbox, grip, off-road), first-track roadside scenery, HUD, pause/restart. One track, one car, no menus/save/sound. |

## racer

```powershell
cd starters/racer
python build.py --thome C:/t900        # or set THOME, or run build.bat
```

Output `bin/raceengine.ngc` (about 32 KB). The folder ships that ROM, listed in
`racer/SHA256.json`; rebuilding must give the same bytes. A prebuilt ROM also
ships with the OVER REV release (`overrev-engine-template.zip`).

Its core already includes the fixes later ported to the base template
(language constants, `RGB()` widening, no joypad "hold POWER" fallback,
VBlank hook). The VBlank hook is what re-arms the road's one-shot raster DMA
on every displayed frame: keep that relationship if you change the frame loop.

To add a save, take `src/core/ngpc_flash*` from the base template (two-bank
journal) and build with `-DNGP_ENABLE_FLASH_SAVE=1`; the sample is built
without it and is not padded to 2 MB.

Checks, 2026-11-08: the sources were fixed on 2026-11-03 (32-bit `C9H_*`
runtime bridges in `src/core/ngpc_runtime_alias.asm`, `src/fx/ngpc_dma.c`), then
on 2026-11-08 (`ngpc_cpu_speed` and the VRAM-queue range check, taken from the
base template), so the ROM was rebuilt: `python build.py` from this folder gives
exactly `bin/raceengine.ngc` (32 620 bytes, SHA-256 in `racer/VALIDATION.json` and
`racer/SHA256.json`). Native emulator: direct boot, acceleration and automatic
shifts, pause/resume, no hardware fault. The full check list in VALIDATION.json
dates from 2026-10-04 (previous ROM). No physical-console validation is claimed.

Known difference, kept on purpose: the base template's `src/fx/ngpc_dma.c` now drains a
pending Timer0 request before arming (fixes a one-line shift: line N showed table[N+1], seen
in emulation on 2026-11-08). The racer keeps its own `ngpc_dma.c` without that change because
its road was tuned on the current behaviour. Port it only once the raster test ROM confirms the
fix on a real console, then re-check the road (a one-line shift of the whole road table).
