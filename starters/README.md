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

Output `bin/raceengine.ngc` (about 32 KB). `bin/` is a build output and is not
versioned here; a prebuilt ROM ships with the OVER REV release
(`overrev-engine-template.zip`).

Its core already includes the fixes later ported to the base template
(language constants, `RGB()` widening, no joypad "hold POWER" fallback,
VBlank hook). The VBlank hook is what re-arms the road's one-shot raster DMA
on every displayed frame: keep that relationship if you change the frame loop.

To add a save, take `src/core/ngpc_flash*` from the base template (two-bank
journal) and build with `-DNGP_ENABLE_FLASH_SAVE=1`; the sample is built
without it and is not padded to 2 MB.

Checks, 2026-10-08: rebuilt from this folder, the ROM is byte-identical to the
one validated on 2026-10-04 (SHA-256 in `racer/VALIDATION.json`); 600 frames
with the throttle held on the native emulator: road and HUD update, no
hardware fault. No physical-console validation is claimed for this extraction.
