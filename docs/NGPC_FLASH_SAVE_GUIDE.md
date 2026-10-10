# NGPC Flash Save — Complete Guide

**Template 2026 — two-bank journal (2026-10-08), ported from OVER REV.**
Emulator-validated (`tools/save_check/save_check.py`); **not yet validated on
hardware in this generic form.** The research below (§3, §11) is what made the
first hardware saves work; §4 and §10 are what OVER REV then learned the hard way.

---

## 1. Quick start

```c
// Build: make NGP_ENABLE_FLASH_SAVE=1     (links ngpc_flash_asm.rel, no system.lib)

typedef struct {
    u8  magic[4];                      /* MUST be { 0xCA, 0xFE, 0x20, 0x26 } */
    u8  version;                       /* your layout version, after the magic */
    u8  level;
    u16 score;
    u8  pad[NGPC_FLASH_PAYLOAD - 8];   /* 8 = the fields above */
    u8  journal[NGPC_FLASH_TRAILER];   /* last 8 bytes: stamped by the driver */
} MySave;                              /* sizeof(MySave) == SAVE_SIZE */

static MySave s;                       /* static: SAVE_SIZE bytes, not on the stack */
static u8 dirty;

// ── at startup, after ngpc_init() ───────────────────────────────────
ngpc_flash_init();
if (ngpc_flash_exists()) ngpc_flash_load(&s);
else { /* defaults, then s.magic = CA FE 20 26 */ }

// ── during play: change RAM only ────────────────────────────────────
s.score = g_score; dirty = 1u;

// ── when LEAVING a screen (never per button press, never in an ISR) ─
if (dirty && ngpc_flash_save(&s)) dirty = 0u;   /* 0 = keep dirty, retry later */
```

`ngpc_flash_ready()` returns 0 when the cartridge size is unknown or the image
reaches the save banks: every call is then refused. Show it somewhere — a
refused save looks exactly like a working one until the console is switched off.

---

## 2. Hardware context

### Memory map

| Address (CPU) | Region | Note |
|---|---|---|
| `0x200000` | Cart ROM base (CS0) | 16Mbit = 2 MB |
| `0x3F8000` | **Block 32 (F16_B32, 8 KB)** | **Bank B of the journal** |
| `0x3FA000` | **Block 33 (F16_B33, 8 KB)** | **Bank A of the journal** |
| `0x3FC000` | Block 34 (F16_B34, 16 KB) | Reserved for BIOS — DO NOT USE |

### Block sizes (16Mbit cart)

| Blocks | Size each | Erase time | Safe? |
|---|---|---|---|
| 0–30 | 64 KB | 50–200 ms | ❌ Watchdog fires (~100 ms limit) |
| 31 | 32 KB | ~20 ms | Untested |
| **32, 33** | **8 KB** | **~5–15 ms** | **✓ Safe** |
| 34 | 16 KB | — | Reserved — never use |

**Blocks 32 and 33 are used** because they are 8 KB (fast erase, safe under the
watchdog) and SNK games save there on 16 Mbit carts.

### The address depends on the cartridge size

The save blocks sit **0x6000 below the top of the chip**, so each size puts them
at a different address. The BIOS stores the size at **`0x6C58`** at power-on
(`0` none, `1` = 4 Mbit, `2` = 8 Mbit, `3` = 16 Mbit); its own flash routine
reads the same byte.

| Cart | Bank A (block) | Bank B | CPU address of A |
|---|---|---|---|
| 4 Mbit | `0x07A000` (`0x09`) | `0x078000` | `0x27A000` |
| 8 Mbit | `0x0FA000` (`0x11`) | `0x0F8000` | `0x2FA000` |
| 16 Mbit | `0x1FA000` (`0x21`) | `0x1F8000` | `0x3FA000` |

A hard-coded `0x1FA000` does not "fail" on a smaller chip: it **erases 8 KB
elsewhere** — inside the image, i.e. the running code. A write only touches its
own slot, so the bug stays harmless until the first erase, then the console
dies: the worst profile, it shows up for the player who played longest.
**Unknown value = do not save**, never "take the default".

⚠️ An emulator cannot catch this: it presents the capacity your address makes
correct, and derives `0x6C58` from it. Open case: on a Flash Masta 32 Mbit,
games writing block `0x11` corrupt their own space — read `0x6C58` on the target
cart before the first save.

### Why the BIOS makes flash write hard

During any flash operation the BIOS executes **DI**, and so must your own code:
a chip being programmed or erased answers reads with **status bits**, not data.
The erase stub runs from RAM for that reason, but the interrupt handlers are
still in the cartridge — an interrupt taken mid-operation fetches its handler
from status bits. Toshiba: *"all interrupts are prohibited during system calls
related to flash memory management"* (SysCall.txt). Hence `di` … `ei 0` in both
entry points of `ngpc_flash_asm.asm` (bytes `06 07` / `06 00`).

**An emulator whose flash keeps serving ROM during the operation never shows
this.** It cost OVER REV a hard crash "at the end of a race, only on the console".
The NgpCraft emulator now models the busy window and counts a
`flash-busy-fetch` fault; the save bench removes `di` as a negative control and
the ROM is lost at the first real erase.

With interrupts off, the VBL ISR cannot clear the watchdog: a 64 KB erase
(50–200 ms) outlasts it (~100 ms) → reset. 8 KB blocks are safe.

---

## 3. BIOS bugs and how they are worked around

### Bug 1 — VECT_FLASHERS broken for blocks 32/33/34

`SysCall.txt p.299`:
> *System call "VECT_FLASHERS" cannot operate on blocks 32, 33, 34 (F16_B32, F16_B33, F16_B34). When these areas need to be operated on, please use the system library routine "CLR_FLASH_RAM".*

**Workaround (original):** use `CLR_FLASH_RAM` from `system.lib` (bank-3 registers).

**Workaround (standalone — used by this template):** `ngpc_flash_asm.asm` embeds
the AMD sector-erase sequence executed from RAM at `0x6E00` — no `system.lib` required.

### Bug 2 — CLR_FLASH_RAM silently fails on its 2nd call in the same session

`CLR_FLASH_RAM` erases block 33 successfully on the **first** call after power-on. On any subsequent call within the same session it exits silently without erasing. No source is available for `system.lib`; root cause unknown.

Diagnostic codes used during research:
```
BERA:00CA  → chip read 0xCA before erase  = data present, chip in Read Array mode ✓
FERA:00CA  → chip still 0xCA after erase  = erase silently failed
FERA:FF    → chip reads 0xFF after erase  = erase OK ✓
FVFY:0000  → flash matches data exactly   = write OK ✓
FVFY:0002  → 2 bytes wrong after write    = erase failed, NOR AND of old+new data
```

**Workaround:** the append-only slot design (see §4) ensures `CLR_FLASH_RAM` is called at most once per session.

### Bug 3 — Direct cart ROM writes are no-ops from user code

Attempting to send Sharp flash commands manually (e.g. `ldb (xde),0x20` with `xde=0x3FA000`) has no effect. The cart bus `/WE` line is only asserted by the BIOS and `system.lib`; user code read/write cycles to ROM space are decoded as read-only by the address decoder.

**Evidence (confirmed across 3 hardware attempts):**
```asm
ld   xde,0x3fa000
ldb  (xde),0x50      ; Clear SR command — silently ignored
ldb  (xde),0x20      ; Block Erase Setup — silently ignored
ldb  (xde),0xd0      ; Block Erase Confirm — silently ignored
; Poll:
bit  7,(xde)         ; reads 0xCA (real flash data, chip in Read Array mode)
                     ; 0xCA = 0b11001010 → bit 7 = 1 → poll exits immediately
                     ; Chip was never put in erase mode.
```

**Consequence:** `ldb (xde),imm8` from ROM-resident code still fails (executing from
the chip being programmed is impossible). The fix is a three-step workaround:

```
(0x6E) = 0x14    ; assert /WE on cart bus — user code CAN write this register
(0x6F) = 0xB1    ; watchdog extended mode
copy stub → 0x6E00 + call 0x6E00   ; execute AMD sequence from RAM, not from flash
```

This is exactly what `ngpc_flash_asm.asm` does.

### Bug 4 — asm900 Error-230 with `ld xde3,(xsp+N)`

Stack-relative addressing `(xsp+disp)` is not encodable with bank-3 extended registers as destination. The assembler reports:

```
ASM900-Error-230 : Operand type mismatch
```

**Workaround:** two-step load through the primary bank — the same pattern already used for `XHL3`:

```asm
; Passing a pointer (1st param):
ld   xhl,(xsp+4)     ; stack-rel → primary XHL  ✓
ld   xhl3,xhl        ; primary   → bank-3        ✓

; Passing a u32 offset (2nd param):
ld   xde,(xsp+8)     ; stack-rel → primary XDE  ✓
ld   xde3,xde        ; primary   → bank-3        ✓
```

---

## 4. Two-bank journal (current design)

### Layout

Each bank is 8 KB = `8192 / SAVE_SIZE` slots (32 at 256 bytes, 16 at 512).
Slot `i` of bank A is at `A + i*SAVE_SIZE`; bank B is `A - 0x2000`.

```
offset      field
0..3        magic CA FE 20 26                         (game)
4..N-9      game payload                              (game)
N-8..N-5    sequence number, u32 little-endian        (driver)
N-4..N-3    CRC16-CCITT (poly 0x1021, init 0xFFFF,
            no reflection) of bytes 0..N-5, LE        (driver)
N-2         journal tag 0x4A                          (driver)
N-1         commit byte 0x00 — the LAST byte programmed
```

### Read (ngpc_flash_init)

Scan both banks; a record counts if magic, tag and commit byte match and its
CRC is right. The newest sequence wins, compared modulo 2^32 (so `0xFFFFFFFF`
→ `0` keeps working). The CRC is computed only for a record that could become
current, so a full journal does not slow the boot.

### Write (ngpc_flash_save)

1. Refuse if not ready or if the buffer does not start with the magic (it would
   be written and never found again).
2. Next slot after the current one, in the current bank, **whose every byte
   reads 0xFF**. One byte is not enough: a slot cut mid-write cannot be
   reprogrammed (flash only turns 1 into 0) and the old driver retried it for
   ~18 s.
3. Bank full → switch to the other bank. **The bank holding the newest valid
   record is never erased.** The other one is erased only if it is not blank
   already, then every byte is checked before anything is programmed.
4. Stamp sequence + CRC + tag + commit in the caller's buffer, write through
   `VECT_FLASHWRITE`, read the slot back. Only an identical readback makes it
   current and returns 1.

A power loss at any point leaves the previous record valid: an unfinished
write has no commit byte, an unfinished erase is in the other bank.

### Cost

Write: one BIOS call, interrupts off for the programming time of `SAVE_SIZE`
bytes. Rollover: one 8 KB erase every `8192/SAVE_SIZE` saves, ~60 ms with
interrupts off (emulator model: 57.5 ms) — music and VBlank stall for that
long, so save on a screen change, never mid-action.

### History: the append-only design it replaces

Until 2026-10 the template wrote block 33 only, slot after slot, and erased it
when full. It was built around `CLR_FLASH_RAM` failing on its 2nd call per
session (§3, Bug 2), which the RAM erase stub (essai 18) no longer uses.
Its defects, each found on a real console by OVER REV: no `di`; a hard-coded
16 Mbit address; "empty slot" decided on one byte; the only copy of the save
erased before the new one was written; no integrity check. The OVER REV v31
journal fixed them; this driver is its game-independent version.
Records written by the old driver are recovered with `ngpc_flash_load_legacy()`
(newest old slot of bank A, copied as is: validate it with the game's own
checksum). The old slots stay untouched until a journal record exists; keep
your checksum away from the last 8 bytes, which old records left as padding.

---

## 5. system.lib symbols (reference — no longer a dependency)

> **The template does NOT require `system.lib`.** `ngpc_flash_asm.asm` embeds
> standalone AMD stubs (erase + write) extracted by disassembly of the
> hardware-validated StarGunner ROM. `system.lib` remains supported via
> `SYSTEM_LIB=<path>` if you prefer the original BIOS path.

For reference, the relevant symbols and their register interface:

**TULINK is case-sensitive. All symbols must be UPPER-CASE.**

| Symbol | Function | Parameters |
|---|---|---|
| `CLR_FLASH_RAM` | Erase blocks 32/33/34 | `RA3`=cart(0), `RB3`=block(0x21) |
| `WRITE_FLASH_RAM` | Write flash | `RA3`=cart, `RBC3`=count×256, `XHL3`=src, `XDE3`=offset |
| `FLASH_M_READ` | Read chip capacity (debug only) | `RA3`=cart → returns capacity in `RA3` |

`FLASH_M_READ` is documented as **debug-only** (`SysLib.txt`): *"This subroutine should be used during debug. When creating master program, the code calling this subroutine should be invalid."*

Declare in ASM with `extern large SYMBOL_NAME` and call with `calr SYMBOL_NAME`.

---

## 6. NGP_FAR requirement

The save area is at `0x200000 + 0x1FA000 = 0x3FA000`. This is beyond the 16-bit near address range (`0x0000..0xFFFF`).

`cc900` uses **near (16-bit) pointers by default**. Without `NGP_FAR` (`__far`), the compiler silently truncates `0x3FA000` to `0xA000`, which is ROM code space. Reads return random ROM bytes instead of flash data.

```c
/* CORRECT — all internal flash pointers use NGP_FAR */
#define SAVE_ADDR  ((volatile u8 NGP_FAR *)(CART_ROM_BASE + SAVE_OFFSET))
```

User-facing API functions (`ngpc_flash_save`, `ngpc_flash_load`) take plain `void *` pointers to **RAM** buffers. You do not need `NGP_FAR` in your own code.

---

## 7. Configuring SAVE_SIZE

A power of two from 256 to 8192, default 256 in `ngpc_flash.h`. Override it
**globally** (`-DSAVE_SIZE=512` for every file that includes `ngpc_flash.h`),
nothing to change in the assembly: the page count is passed at run time.

| SAVE_SIZE | Game bytes (`NGPC_FLASH_PAYLOAD`, magic included) | Slots per bank |
|---|---|---|
| 256 (default) | 248 | 32 |
| 512 | 504 | 16 |
| 1024 | 1016 | 8 |

---

## 8. Files involved

| File | Role |
|---|---|
| `src/core/ngpc_flash.h` | Public API and full documentation |
| `src/core/ngpc_flash.c` | Journal: bank selection, scan, CRC, rollover, readback |
| `src/core/ngpc_flash_asm.asm` | `di`-protected RAM erase stub + `VECT_FLASHWRITE` call (no `system.lib`) |
| `tools/save_check/` | Emulator bench: test ROM + scenarios (needs `NGPC_EMU`, `NGPC_BIOS`) |
| `system.lib` | Toshiba library — **optional**, only for `SYSTEM_LIB=<path>` compatibility path |
| `SysCall.txt` (Toshiba SDK) | VECT_FLASHERS bug (p.299), VECT_FLASHWRITE params |
| `SysLib.txt` (Toshiba SDK) | CLR_FLASH_RAM, WRITE_FLASH_RAM, FLASH_M_READ specs |
| `FlashMem.txt` (Toshiba SDK) | Block layout for 4/8/16 Mbit carts |

---

## 9. Diagnostics

| Call | Tells you |
|---|---|
| `ngpc_flash_ready()` | 0 = unknown cart size (`0x6C58`) or image reaching bank B: everything refused |
| `ngpc_flash_cart_size()` | raw `0x6C58`: 1/2/3 = 4/8/16 Mbit |
| `ngpc_flash_slots_used()` | slots taken in the current bank |
| `ngpc_flash_verify(buf)` | bytes differing in the slot of the last attempt (0xFFFF = none) |

Show a failed save on screen (e.g. "NOT SAVED - RETRY") by re-reading this
state, not by remembering a flag: a console switched back on has no flag.

### "The console switches itself off" is not always a crash

`User_Shutdown` (`0x6F85`) is a bit field (SysWork.txt): bit 7 power switch,
bit 6 ten minutes of inactivity, **bit 5 main battery too low**. A flash
operation is the biggest current peak a cartridge makes, so tired batteries
cross the threshold right then, and a game that obeys switches off — at the end
of a race, late in a session, never in an emulator. Before hunting a bug: read
the battery level (`0x6F80`, 0..0x3FF), show which bit fired before shutting
down, try fresh batteries.

---

## 10. Known-bad patterns (do not use)

```c
// ❌ VECT_FLASHERS for block 33 — broken in BIOS (SysCall.txt p.299)
__ASM("ld  ra3,0");
__ASM("ld  rb3,0x21");       // block 33
__ASM("ld  rw3,8");          // VECT_FLASHERS = 8
__ASM("swi 1");              // silently does nothing for this block

// ❌ Direct cart ROM writes — no-ops (cart /WE not asserted by user code)
ld   xde,0x3fa000
ldb  (xde),0x20              // Block Erase Setup — never reaches chip
ldb  (xde),0xd0              // Confirm — never reaches chip

// ❌ ld xde3,(xsp+N) — asm900 Error-230
ld   xde3,(xsp+8)            // stack-rel to bank-3 = invalid encoding
                             // use two-step: ld xde,(xsp+8) / ld xde3,xde

// ❌ FLASH_M_READ in production builds — debug-only per SysLib.txt
calr FLASH_M_READ            // remove before shipping

// ❌ Flash erase/write with interrupts enabled — crash on hardware only
ld   (0x6E),0x14             // ... no `di` first

// ❌ A hard-coded block address — erases the game on a smaller chip
ld   xde,0x3FA000            // read 0x6C58 instead

// ❌ "Empty slot" = first byte 0xFF — a torn slot cannot be reprogrammed
if (SAVE_ADDR[slot * 256] == 0xFF) ...

// ❌ Erasing the bank that holds the only valid copy, then writing
```

```c
// ❌ A magic of your own: the driver only recognises CA FE 20 26.
//    Put YOUR layout version in a field after the magic.
static const u8 my_magic[4] = { 'I', 'D', 'G', 'C' };

// ❌ One save per button press: 32 slots go fast and every rollover is an erase
if (pad_pressed & PAD_LEFT) { opt ^= 1; ngpc_flash_save(&s); }
// ✓ mark dirty in RAM, save once when leaving the screen

// ❌ Clearing the dirty flag without proof
ngpc_flash_save(&s); dirty = 0;
// ✓ if (ngpc_flash_save(&s)) dirty = 0;   (1 = read back identical)

// ❌ A "hold POWER" fallback on Sys_Lever bit 7 (0x6F82): no NGPC button is
//    wired there and it is NOT the power switch. Only User_Shutdown (0x6F85).
```

---

## 11. Research log

Summary of key findings:

| Essai | Approach | Result | Root cause |
|---|---|---|---|
| 1–5 | VECT_FLASHERS / block 30/33 | Crash or no effect | Bug in BIOS for blk 33; watchdog for 64 KB |
| 6 | CLR_FLASH_RAM + swi 1 write | **1st save works ✓** | Valid path |
| 7–13 | Various resets before 2nd CLR call | FERA:00CA always | CLR_FLASH_RAM session bug, chip state irrelevant |
| 14 | Split erase/write for logging | Cleaner diagnostics | — |
| 15 | Manual Sharp erase, bank-3 regs | FERA:00CA | `ldb (xde3),imm8` invalid encoding (Error-230 path) |
| 16 | Manual Sharp erase, primary XDE | FERA:00CA | Writes silently ignored — user code can't drive /WE |
| **17** | **Append-only slots** | **✓ SOLVED** | Avoids 2nd CLR_FLASH_RAM call entirely |
| **18** | **Standalone AMD stubs (no system.lib)** | **✓ SOLVED** | Register 0x6E=0x14 enables /WE; stubs copied to RAM 0x6E00 and called from there |
| 19 | OVER REV: 16 Mbit address hard-coded | Console off at the first erase | Erase landed in the image on another chip size → read `0x6C58` |
| 20 | OVER REV: write/erase without `di` | Hard crash on console only | Interrupt fetched from a busy chip → `di` … `ei 0` |
| 21 | OVER REV: append-only bank full | Progress silently stops being saved / only copy erased | → two-bank journal, never erase the current bank (v31, 2026-09-27) |
| **22** | **Generic journal in the template** | **✓ emulator** (2026-10-08) | `tools/save_check/save_check.py`; hardware test pending |
