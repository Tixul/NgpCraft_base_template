# `ngpc_link` — link cable for NGPC games

Two consoles, one cable. This directory is self-contained and does not depend on
anything else in this game: drop it into your own project.

| File | Role |
|---|---|
| `ngpc_link_com.c/.h` | the 11 BIOS COM calls (vectors `0x10`–`0x1A`), made safe |
| `ngpc_link.c/.h` | the session: find the peer, pick roles, one packet per frame, checksums, timeouts |

## Dropping it into a project

1. Copy the `ngpc_link/` directory to your project root (keep the directory
   name: the headers include each other through it).
2. Compile both `.c` files **with the project root on the include path**
   (`-I.`), and link them in:

   ```makefile
   OBJS += ngpc_link/ngpc_link.rel ngpc_link/ngpc_link_com.rel

   ngpc_link/%.rel: ngpc_link/%.c
           $(CC) -c -O3 -I. $< -o $@
   ```

3. Give the module your integer types. It includes `NGPC_LINK_TYPES_HEADER`,
   which defaults to `"ngpc_types.h"`, so either create that file or override
   the macro:

   ```c
   /* ngpc_types.h */
   #include "ngpc.h"          /* whatever defines u8 / u16 in your project */
   #define NGPC_LINK_PAYLOAD  4
   #define NGPC_LINK_RX_QUEUE 0
   ```

   Nothing else is required: no `ngpc_hw.h`, no framework, no globals.

## Using it

```c
#include "ngpc_link/ngpc_link.h"

ngpc_link_init(seed);              /* once; seed must vary, see below */

while (1) {
    vsync();
    ngpc_link_out[0] = pad;        /* what I send -- fill in BEFORE update */
    ngpc_link_update();            /* the exchange; never blocks */

    if (ngpc_link_ready()) {
        u8 peer_pad = ngpc_link_in[0];
        if (ngpc_link_host) { /* decisions only one console may take */ }
    }
}
```

| Symbol | Meaning |
|---|---|
| `ngpc_link_state` | `OFF` / `SEARCHING` / `READY` / `LOST` / `MISMATCH` |
| `ngpc_link_host` | 1 on exactly one of the two consoles |
| `ngpc_link_out[]` / `ngpc_link_in[]` | payload sent / last one received |
| `ngpc_link_fresh` | 1 only on the frame a packet landed |
| `ngpc_link_peer_seq` | peer sequence number; gaps mean lost packets |
| `ngpc_link_stats` | bytes, packets, bad checksums, skipped sends, silence |
| `ngpc_link_cable()` | cable detected — advisory, never gate on it |
| `ngpc_link_set_role(h)` | force host/guest when your menu already asked |
| `ngpc_link_resync()` / `ngpc_link_close()` | search again / say goodbye |

Settings (before the include, or with `-D`): `NGPC_LINK_PAYLOAD` (1..32,
default 4), `NGPC_LINK_SEND_INTERVAL`, `NGPC_LINK_TIMEOUT`,
`NGPC_LINK_HELLO_INTERVAL`, `NGPC_LINK_RX_QUEUE`. **Both consoles must use the
same `NGPC_LINK_PAYLOAD`**, or the session reports `MISMATCH` instead of quietly
shuffling bytes.

Cost: ~1.5 KB of ROM, and about 30 bytes of RAM plus twice the payload.

## What you need to understand before using it

**The cable is a byte pipe between two independent consoles, not a shared
simulation.** Each console runs its own copy of the game; they share only the
bytes on the wire. So there is no lockstep, no rollback and no determinism
requirement built in — and **no waiting**: the wire is 19200 bps (~32 bytes per
60 Hz frame) and the peer may be paused, in a menu or unplugged.
`ngpc_link_update()` never costs more than the frame it is called in.

**One packet per frame, or one packet per simulation step?** By default the
module keeps only the LAST packet received, in `ngpc_link_in[]` — right when you
exchange state ("here is where I am"), because missing an intermediate packet
costs nothing. If instead your game consumes one packet per simulation step
(input lockstep), set `NGPC_LINK_RX_QUEUE` to 4 or 8 and read with
`ngpc_link_recv()`: without a queue, two packets arriving in the same frame
overwrite each other and the lost step desynchronises the game silently.

**Who is host?** Whoever opened the link screen first. Each console announces
how long it has been searching and the one looking longest becomes host, at one
frame of resolution; a few random bits seeded from your `ngpc_link_init()` seed
separate consoles that started within a few frames, and identical draws are
simply repeated. No question is ever put to the player. If your game already
asked ("create" / "join"), call `ngpc_link_set_role()` instead.

## Three traps, all measured

1. **No BIOS wrapper returns a value.** At `-O3`, a cc900 function whose body is
   inline asm without a C return yields garbage, which is why every result comes
   out through a pointer and buffer levels are read from the BIOS counters at
   `0x6D00` / `0x6D01`.
2. **`COMOFFRTS` starts with `ei 6`**, masking VBlank: the classic
   `com_rts_off(); WaitVsync();` idiom hangs, and on real hardware the watchdog
   powers the console off after about a second. Every wrapper therefore ends with
   `ei 0`, so do not call them from an ISR.
3. **`COMINIT` installs the BIOS serial handlers** into `0x6FE4` / `0x6FE8`.
   Never reinstall the user interrupt vectors after `ngpc_link_init()`, and do
   not try to "restore" copies of those two pointers taken at boot: that
   overwrites the working handlers and not one byte leaves the console.

## Diagnostics

`ngpc_link_stats` reports `tx_bytes`, `rx_bytes`, `tx_packets`, `rx_packets`,
`bad_sum` (checksum failures: noise or desync), `tx_skipped` (the TX ring was
full, the peer is not draining), `resyncs`, `gap` (frames since the last valid
packet) and `rx_lost` (dropped because the receive queue was full).

With no peer connected, `tx_skipped` climbs and `tx_packets` freezes: that is
normal. Without CTS the transmitter stalls and the ring fills; those pending
announcements leave in one go when a cable is plugged in, which speeds the
handshake up rather than breaking it.

## Examples and proof

- `example_link_main.c` — a runnable demo: one cursor per console, session state on
  screen. Copy it over `src/main.c` to try it.
- `validate_link_emulator.py` — the harness that proves the module on two emulated
  consoles: a lone console does not freeze, two find each other, a cut cable is
  reported, the session rebuilds itself, a 16-byte payload crosses intact, and two
  builds with different payload sizes report `MISMATCH` instead of mixing bytes.
A complete game built on this module — input lockstep, settings sent by the host,
disconnection handling — is published separately as an example project; its
`LINK_2P.md` is the design write-up.
