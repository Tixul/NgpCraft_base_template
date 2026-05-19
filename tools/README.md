# tools/

Python scripts for the asset pipeline and template tooling.

Requirements: Python 3.10+, Pillow (`pip install pillow`).

---

## Graphics pipeline

### `ngpc_tilemap.py`
**PNG → C/H tilemap for scroll planes (SCR1/SCR2).**

Takes a PNG whose every 8×8 tile has ≤ 3 visible colors and produces the
`tiles[]`, `map_tiles[]` and `palettes[]` arrays ready to be loaded into VRAM.
If a tile exceeds 3 colors, the pipeline automatically switches to dual-layer
mode (two separate C files).

```sh
python tools/ngpc_tilemap.py GraphX/bg.png -o GraphX/bg.c --header
```

---

### `ngpc_sprite_export.py`
**Spritesheet PNG → metasprite C/H (one asset at a time).**

Handles tile deduplication, palette assignment, and emits `NgpcMetasprite`
structures + animation table. Also writes `name_tile_base` and `name_pal_base`
into the generated files for deterministic VRAM placement.

```sh
python tools/ngpc_sprite_export.py GraphX/player.png \
    -o GraphX/player_mspr.c \
    --frame-w 16 --frame-h 16 \
    --tile-base 256 --pal-base 0 \
    --header
```

Useful options:
- `--tile-base N` — starting tile slot (default 0)
- `--pal-base N` — starting palette slot, 0–15 (default 0)
- `--fixed-palette A,B,C,D` — force an external RGB444 palette, useful for
  sharing a palette with another previously-exported sprite
- `--frame-count N` — number of frames to export (default = all)
- `--anim-duration N` — duration per frame in the animation table

---

### `ngpc_sprite_bundle.py`
**Generic infrastructure for exporting multiple sprites in sequence.**

Provides the `SpriteBundle` class that automatically tracks `tile_base` /
`pal_base` and checks for overflows (512 tiles max, 16 palettes max). Meant
to be imported from a game-specific export script.

Utility functions:
- `load_rgba(path)` — open a PNG in RGBA
- `make_sheet(frames, w, h, out)` — assemble a horizontal sprite sheet
- `make_sheet_from_files(paths, w, h, out)` — same, from a list of files
- `split_two_layers(frames, w, h)` — split a 6-color sprite into two 3-color layers
- `read_palette(mspr_c, symbol)` — read 4 RGB444 words from a generated `*_mspr.c`

```python
# Example: game-specific export script
from pathlib import Path
from ngpc_sprite_bundle import SpriteBundle, load_rgba, make_sheet, split_two_layers

project_root = Path(__file__).resolve().parent.parent
bundle = SpriteBundle(
    project_root=project_root,
    out_dir=project_root / "GraphX",
    gen_dir=project_root / "GraphX" / "_gen",
    tile_base=256,
    pal_base=0,
)

# Standard export (advances both tile_base and pal_base)
sheet = bundle.gen_dir / "enemy_sheet.png"
make_sheet([load_rgba(f) for f in sorted((project_root / "art").glob("enemy*.png"))], 8, 8, sheet)
bundle.export("enemy", sheet, 8, 8, anim_duration=4)

# Export with shared palette (advances tile_base only)
saved_pal = bundle.pal_base - 1  # palette allocated by previous export
bundle.export_reuse_palette("enemy_b", sheet_b, 8, 8, shared_pal_base=saved_pal)

print(f"Done. tile_base={bundle.tile_base}, pal_base={bundle.pal_base}")
```

---

### `ngpc_palette_viewer.py`
**Visualize sprite palettes from every `*_mspr.c` in a GraphX folder.**

Generates three files:
- `ngpc_palettes.gpl` — palette directly loadable in Aseprite
- `ngpc_palettes.png` — visual swatch (4 columns × 16 rows)
- `ngpc_palettes.txt` — text report: slot / sprite / hex / RGB

Handy for sanity-checking palette allocations and avoiding collisions.

```sh
python tools/ngpc_palette_viewer.py
# or
python tools/ngpc_palette_viewer.py GraphX --out GraphX
```

---

### `ngpc_font_export.py`
**8×8 font PNG → C/H compatible with `ngpc_text_*`.**

Converts a glyph sheet into NGPC 2bpp tiles to replace the system font
without changing any call site (`ngpc_text_print`, `ngpc_text_print_dec`,
`ngpc_text_print_hex`).

```sh
python tools/ngpc_font_export.py font.png -o GraphX/ngpc_custom_font
python tools/ngpc_font_export.py font.png -o GraphX/ngpc_custom_font -n myfont
```

Supported PNG layouts:
- `128x48` — ASCII 32–127, default `tile_base` = 32
- `256x24` — ASCII 32–127, default `tile_base` = 32
- `256x32` — ASCII 0–127, default `tile_base` = 0

---

## Compression

### `ngpc_compress.py`
**Compress binary data (tiles, maps) with RLE or LZ77/LZSS.**

The output matches the decompressor embedded in `src/ngpc_lz.c`. Produces a
`.c` file with a `const u8` array.

```sh
python tools/ngpc_compress.py tiles.bin -o tiles_lz.c -m lz77 -n level1_tiles
python tools/ngpc_compress.py tiles.bin -o tiles_both.c -m both -n my_tiles
```

Modes: `rle`, `lz77`, `both` (emits both in the same file).

---

## Project tooling

### `ngpc_project_init.py`
**Create a new project from this template.**

Copies the template into a target folder, skipping generated artifacts
(`bin/`, `build/`, `__pycache__/`, etc.), and renames the project identifiers
(`NAME` in the makefile, `CartTitle` in `carthdr.h`).

```sh
python tools/ngpc_project_init.py C:/dev/MyGame --name "My NGPC Game" --rom-name mygame
python tools/ngpc_project_init.py C:/dev/MyGame --dry-run   # preview without copying
```

---

### `apply_live_zip.py`
**Apply a ZIP exported from the NgpCraft Live Editor to this template, and
patch the makefile so any extra user sources get built.**

Typical workflow: the player prototypes their game in the NgpCraft Live Editor
app (browser or Android), clicks "Export ZIP", drops the `.zip` onto their
build machine, then runs this script to merge the sources in and produce the
ROM.

#### Detailed behavior

1. **ZIP validation.** Lists the entries, rejects any path that doesn't match
   `src/<name>.[ch]` (regex `^src/[A-Za-z_][\w-]*\.[ch]$`). Any entry
   containing `..`, an absolute path, or a backslash is rejected — nothing is
   ever extracted outside `src/`.
2. **Extraction.** Each valid entry is written to its target path (by default
   `<project>/src/<name>.c|h`), silently overwriting existing files. The
   `src/` directory is created if missing.
3. **Makefile patch.** The script then scans `<project>/src/*.c` (top-level
   only — never the `src/core/`, `src/gfx/`, etc. subdirectories), excludes
   `main.c` (already covered by the base OBJS list), and regenerates a
   marker-delimited block:

   ```makefile
   # >>> NGPC_LIVE_EDITOR_OBJS auto (do not edit by hand)
   OBJS += $(OBJ_DIR)/src/<user_source>.rel
   ...
   # <<< NGPC_LIVE_EDITOR_OBJS auto
   ```

   - **First run:** if the block doesn't exist yet, it is injected right
     after the last line of the base OBJS list, located via the anchor line
     `$(OBJ_DIR)/src/core/ngpc_syspatch.rel` (if the anchor can't be found —
     e.g. a modified makefile — the script aborts cleanly with an explicit
     message).
   - **Subsequent runs:** the existing block is replaced in place. Idempotent:
     if nothing changes, the makefile isn't even rewritten (`makefile already
     up to date — no changes.`).
   - If there are no user sources beyond `main.c`, the block is still emitted
     with a `# (no extra user sources)` placeholder so re-patching works
     later.

4. **Backup.** Before any write, the previous makefile is copied to
   `makefile.bak` (only if a patch is actually applied).

#### Arguments

| Argument | Type | Default | Purpose |
|---|---|---|---|
| `zip` (positional, required) | path | — | ZIP exported from the live editor (`ngpcraft-project-*.zip`). |
| `--project-root` | path | parent of `tools/` (auto-detected) | Root of the project to patch. Override to target a different checkout. |
| `--dry-run` | flag | off | Print the full plan (ZIP entries, files to overwrite/create, sources detected, makefile state) without writing anything to disk. |

#### Examples

```sh
# Standard case: from the base template root
py -3 tools/apply_live_zip.py path/to/ngpcraft-project-2026-05-17.zip

# Preview without touching disk
py -3 tools/apply_live_zip.py path/to/zip --dry-run

# Target a different checkout (useful if tools/ is shared across projects)
py -3 tools/apply_live_zip.py path/to/zip --project-root C:/dev/MyGame

# Final build
make
```

#### Limitations

- Only top-level `src/*.c` files are auto-added to the makefile. Hand-written
  sources under `src/<subdir>/` (e.g. `src/gameplay/`) still have to be added
  to the OBJS list manually.
- The marker block uses `OBJS += …` and therefore cannot sit inside a
  continued `OBJS = \` block — which is why it always comes after the anchor
  line.

---

### `build_utils.py`
**Cross-platform helpers called from the Makefile.**

Not meant to be invoked directly. Used internally by `make clean` and `make`
(final `.ngc` ROM move, internal `.s24 → .ngp` conversion).

---

### `ngpc-aseprite-color-tools.zip`
**Aseprite extension for working in the NGPC palette.**

Contains Lua scripts to install in Aseprite to constrain colors to the NGPC
RGB444 gamut and make hardware-correct asset creation easier. See the README
inside the archive.
