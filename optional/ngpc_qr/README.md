# ngpc_qr — Text/URL QR codes for Neo Geo Pocket Color

Optional C89 module, extracted from the approach used in Over Rev and made
independent of the game. No required score, player name, save system, website,
or packet format. MIT license; the Nayuki reference implementation and its
license are preserved in `tools/vendor/qrcodegen.py`. Not included in the base
build by default.

## How it works and available profiles

At four pixels per module, an 8 x 8 tile represents four modules: all 16
combinations are enough to draw the entire QR code. No framebuffer, no malloc.
A compact matrix is computed once; the tilemap uses these 16 patterns.

| NGPC_QR_PROFILE | Format | Alphanumeric capacity | Including quiet zone | Tilemap | Matrix |
|---|---|---:|---:|---:|---:|
| 2 | Version 2, error correction M | 38 characters | 136 x 136 px | 17 x 17 | 79 bytes |
| 3 (default) | Version 3, error correction L | 77 characters | 152 x 152 px | 19 x 19 | 106 bytes |

The quiet zone contains four white modules at the top/left and five at the
bottom/right to round up to whole tiles. Profile 3 occupies the full screen
height: do not add text in its quiet zone. Profile 2 leaves two tile rows outside.

The mask is fixed (0), as in Over Rev: decodable, but without selecting the
mask with the lowest penalty. This module is not a general-purpose library
supporting all QR versions and error correction levels. Both profiles require
testing on the actual screen and target phones before release.

## Adding it to a project

Keep the folder in `optional/ngpc_qr/` and add the following to the makefile
**before the compilation/link rules**:

```makefile
CDEFS += -Ioptional/ngpc_qr
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr.rel
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_draw.rel
# Only if you share results in the OVER REV format (see "Result codes" below):
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_result.rel

$(OBJ_DIR)/optional/ngpc_qr/ngpc_qr.rel: optional/ngpc_qr/ngpc_qr.h optional/ngpc_qr/ngpc_qr_config.h optional/ngpc_qr/ngpc_qr_tables.h
$(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_draw.rel: optional/ngpc_qr/ngpc_qr.h optional/ngpc_qr/ngpc_qr_config.h optional/ngpc_qr/ngpc_qr_draw.h
$(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_result.rel: optional/ngpc_qr/ngpc_qr_result.h optional/ngpc_qr/ngpc_qr_config.h
```

The Toshiba compiler requires the explicit `-I`, especially for nested includes.
You can also copy the folder into `src/ngpc_qr/`: adjust the `-I` paths, object
paths, and example includes accordingly.

Select the profile in `ngpc_qr_config.h`, or add this **globally**:

```makefile
CDEFS += -DNGPC_QR_PROFILE=2
```

The definition must be identical for the module, renderer, and every file using
`NgpcQr`: the structure size depends on the profile. Rebuild all these objects
after a change; do not define the profile in just one .c file. This old make
does not automatically detect changes to compiler flags.

## Minimal API

```c
#include "ngpc_qr_draw.h"
#include "ngpc_gfx.h"

static NgpcQr qr; /* Avoid placing this structure on the NGPC stack. */

/* After ngpc_init(), on a stable screen with raster/scroll/sprites controlled. */
if (ngpc_qr_encode(&qr, "HTTPS://EXAMPLE.COM/RESULT/ABC123")) {
    /* SCR1, character tiles 128..143, palette 0, cell (0,0). */
    if (!ngpc_qr_draw(&qr, GFX_SCR1, 128u, 0u, 0u, 0u)) {
        /* Invalid display parameters. */
    }
} else {
    /* Text is too long, empty, or contains unsupported characters. */
}
```

`ngpc_qr_encode(qr,text)` returns 1 on success. Failure invalidates the previous
matrix without truncating or modifying the text. NULL is rejected. Contexts
are independent; do not use the same context concurrently from an interrupt
and the main loop. The text must not overlap the context memory and must be
NUL-terminated.

`ngpc_qr_get_module(qr,x,y)` returns 0/1. Out-of-bounds coordinates, an invalid
context, or NULL return zero. This allows alternative rendering if needed.

`ngpc_qr_draw(qr,plane,tile_base,palette,x,y)` returns 1/0. `x/y` are **tile**
coordinates, not pixels. Supports SCR1 and SCR2, palettes 0–15, and character
tile bases 0–496. The entire rectangle must fit within the visible 20 x 19 tiles.
Errors are detected before any VRAM writes. Validate coordinates before casting
them to u8 if they come from a wider type.

The renderer writes exactly 16 tiles, the tilemap rectangle, and one opaque
white/black palette. Choose free slots; it does not save/restore the previous
graphics. It does not clear planes, modify scroll, raster/DMA, or sprites, or
block in an input loop. The game must prevent any animation, palette changes,
or scrolling that would alter the QR code.

The `examples/qr_example.c` example exposes `qr_example_show(const char *text)`:
a white page, centering, and a wait for a button press after release. Add its
`.rel` if used. **Before** calling it, suspend your engine's raster/DMA effects;
**afterwards**, rebuild the menu and restore those effects. The loop includes
a place to service an audio/link driver if needed.

## Text and URLs

Strict alphabet: `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:`.
No lowercase letters, accents, underscores, `?`, `=`, `&`, `#`, UTF-8, or binary
mode. Unsupported characters cause encoding to fail; there is no silent conversion.

To open a website from a phone camera, use a full uppercase HTTPS URL with data
in the **path**. Example: `HTTPS://EXAMPLE.COM/GAME/ABC123`. The domain is
case-insensitive, but the path may be case-sensitive: provide the uppercase
route or a redirect on the website. For arbitrary bytes, the game can produce
Base32 using A-Z/2-7, counting the prefix and URL toward the total capacity.
Base32 and CRC are provided by `ngpc_qr_result` (below); authentication and
the server remain the game's responsibility.
Do not copy the Over Rev URL into another game.

## Result codes (the OVER REV format)

`ngpc_qr_result.h` builds the text OVER REV puts in its Time Attack QR, with
your own URL, tag and data:

```
<URL><TAG>:<NAME>:<BASE32(data + CRC16)>
HTTPS://WWW.NGPC-DEV.COM/OVERREV/OV1:PLAYER01:AAAQAAAACEABDFAAAAAADYBN
```

```c
#include "ngpc_qr_result.h"
#include "ngpc_qr_draw.h"

static char text[NGPC_QR_TEXT_MAX + 1];
static NgpcQr qr;
u8 data[6];

/* Serialise field by field, big-endian: never copy a C struct. */
data[0] = 1u;                    /* YOUR rules revision: bump when scoring changes */
data[1] = track_id;              /* stable ids, not menu positions */
data[2] = (u8)(ticks >> 8); data[3] = (u8)ticks;
data[4] = (u8)(seq >> 8);   data[5] = (u8)seq;

if (ngpc_qr_result_build(text, sizeof(text), "HTTPS://EXAMPLE.COM/MYGAME/",
                         "MG1", player_name, data, 6u)
    && ngpc_qr_encode(&qr, text)) {
    /* draw it, e.g. qr_example_show(text) */
}
```

- **TAG** `[A-Z0-9]{1,8}` names the format and its version (`OV1` for OVER
  REV). Change it when the meaning of the bytes changes.
- **NAME** `[A-Z0-9]{1,16}`; **data** 1–32 bytes, serialised by the game.
- **CRC-16/CCITT-FALSE** (poly `0x1021`, init `0xFFFF`, no reflection, no final
  xor; check value of `123456789` = `0x29B1`) over ASCII `<TAG>:<NAME>:` then
  the data, appended high byte first. The URL is **not** hashed.
- **Base32** RFC 4648 (`A-Z2-7`), no `=` padding; a last partial group is padded
  with zero bits. `n` data bytes give `ceil((n+2)*8/5)` characters: 13 bytes → 24,
  exactly OVER REV's packet.
- Returns 0 (and an empty string) on any invalid input or if the whole text
  exceeds `NGPC_QR_TEXT_MAX` (77 in profile 3, 38 in profile 2) — profile 2 is
  usually too small for a URL.

Length budget, profile 3 (77): `len(URL) + len(TAG) + len(NAME) + 2 + base32`.
OVER REV: 33 + 3 + 8 + 2 + 24 = 70.

**The CRC detects a damaged scan, it does not authenticate anything.** Anyone
can build a valid code; a result is a claim. Server side: check the URL prefix
yourself, never upper-case a scanned payload to "repair" it (it is covered by
the CRC), reject unknown tags/revisions, show a preview on GET and publish only
on an explicit POST. `tools/result_codec.py` is the reference encoder/decoder
(`python result_codec.py --self-test` checks an OV1 vector).

### Opening the page from a phone camera

The QR alphanumeric mode has no lowercase, `?`, `=` or `#`, so the data goes in
an **upper-case path**: `HTTPS://EXAMPLE.COM/MYGAME/MG1:...`. A static host
(GitHub Pages and similar) has no such page, so the request lands on the
site's `404.html`. OVER REV's site uses that: its `404.html` matches the
upper-case path and redirects to the real lower-case page with the payload in
the hash, e.g. `/mygame/#MG1:ACE:...`, where the page decodes it locally.
That route becomes a **permanent contract** of every ROM you ship: pick a
domain and path you will keep.

## Memory and computation time

Context: matrix + codewords + bit counter + valid flag, aligned by cc900.
With this compiler: 128 bytes for profile 2, 180 for profile 3. Use
`sizeof(NgpcQr)` as the source of truth with another compiler.
Codewords: 44 (28+16) for profile 2, 70 (55+15) for profile 3.
Add the text size if the game constructs it in RAM.

Rendering: 256 bytes of character memory and 578/722 bytes in the existing
tilemap, with no bitmap buffer in RAM. Constant tables do not use mutable RAM;
only one profile is compiled into the ROM. The module has no mutable global
variables and imposes no memory cost when unused.

Encode when opening a page, not on every racing frame. The generic context
does not have the same cost as Over Rev's specialized encoder: do not reuse
its timing estimate without measuring your own build.

## Verification and maintenance

Tables are included: Python is not required to compile the game.
Regeneration requires no network access:

```
python optional/ngpc_qr/tools/generate_tables.py --check
python optional/ngpc_qr/tools/generate_tables.py
```

The `tools/check_rom.py --help` test uses the actual cc900 ROM, its map file,
the native NgpCraft emulator, a user-provided BIOS, OpenCV, and NumPy.
Create the test ROM in a **working copy** of the template: keep the test main
object first in OBJS so the cartridge header comes first, call ngpc_init(),
then call the example with `HTTPS://EXAMPLE.COM/QR/TEST123`.
The regular main and base ROM have not been replaced by this demonstration.

Validation on September 18, 2026: both profiles compiled with cc900;
40 V2-M and 79 V3-L matrices compared module by module against Nayuki;
two independent contexts; NULL, invalid text, and oversized text rejected;
memory sentinels and text preserved; invalid placements caused no VRAM writes;
SCR2 rendering at the last character tile range/palette; screenshots of both
profiles decoded with OpenCV. These are emulator tests, not physical hardware
validation of this generic module.

Result codes, October 8, 2026 (`tools/check_result.py`, needs `NGPC_EMU` and
`NGPC_BIOS`, no OpenCV): the cc900 `ngpc_qr_result_build` reproduces an OV1
result text (OVER REV layout and bytes) byte for byte (70 characters with its URL) and two other
vectors (1 byte; 16-character name + 32 bytes) against `result_codec.py`;
9 invalid inputs refused with an empty string; the QR of the full URL shown by
`qr_example_show` matches Nayuki V3-L mask 0 on all 841 modules, quiet zone
white. Not yet read by a phone from a real LCD.
