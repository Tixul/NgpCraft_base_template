# role_sim — proving the host/client election, off the console

Two consoles running the **real** `ngpc_link.c`, on a cable whose byte rate and
announcement lag this harness controls. It exists to sweep the two things that
decide the role election and that two real consoles cannot be made to vary on
demand:

- **skew** — how many frames apart the players opened the link screen;
- **lag** — how stale a HELLO is by the time the peer's game code reads it
  (measured at two frames on hardware, more once a burst is queued behind it).

Build and run (any host compiler; nothing NGPC-specific is compiled):

```
gcc -std=c99 -O1 -I. -I.. -I../.. role_sim.c inst_a.c inst_b.c -o role_sim
./role_sim
```

Exit code is non-zero if any run ends with the two consoles disagreeing.

## How it makes two consoles out of one module

`inst_a.c` and `inst_b.c` each `#define` the module's public names to a prefix and
then `#include "ngpc_link.c"`. Two translation units means two independent copies
of its file-scope statics — which is exactly what two consoles are. The BIOS COM
layer is replaced by `ngpc_link/ngpc_link_com.h` here: same names and shapes, but
the rings are plain arrays and the wire is the harness. The wire is modelled as
the real one is — **32 bytes per frame** (19200 bps 8N1 = one byte per 3200 cycles)
and in order.

## What it measured

`. = the two consoles agreed on one host`, `X = they did not`.

**Before the fix** (`host = (link_token() > tok)`, our token read live against the
peer's snapshot):

```
      lag: 1 2 3 4 5 6 7 8 9 . . .
skew  0:   X X X X X X X X X X X X
skew  1:   X X X X X X X X X X X X
skew  2:   . X X X X X X X X X X X
skew  3:   . . X X X X X X X X X X
...
skew  7:   . . . . . . X X X X X X
skew  8:   . . . . . . . . . . . .
624 runs, 300 disagreements, slowest agreement 25 frames
```

The triangle is the whole diagnosis, and it is not a heuristic: the failure is
exactly **`lag > skew`**, which is what falls out of the arithmetic — each console
weighed its own present against the other's past, so the one that started second
still looked bigger whenever the announcement was older than the gap between the
two starts. It stops at skew 8 because that is `NGPC_LINK_HELLO_INTERVAL`: past it
the first announcement is always old enough for the gap to show through.

**After** (freeze the counter at first contact, decide only when the peer echoes
our own token back):

```
624 runs, 0 disagreements, slowest agreement 51 frames
wire: deepest TX ring 19/64 bytes, tx_skipped 0
```

⚖️ **The cost is in that last number and it is the honest trade**: agreement now
takes a round trip instead of a one-way announcement, so the worst case went from
25 frames to 51 — about 0.85 s on a link screen. Both-host was a broken session;
half a second before "PLAYER 1 / PLAYER 2" appears is a playable one.

⚠️ **The other cost is wire, and it was measured rather than assumed.** A pending
burst now announces every frame instead of once per interval, which is what keeps
the extra round trip from doubling the wait. That took the deepest the 64-byte BIOS
TX ring ever got from **17 bytes to 19**, with `tx_skipped` still **0** — the
module never once had to refuse a send. The last line of the run reports both, and
a non-zero `tx_skipped` fails the bench.

⚠️ **What this bench does NOT cover.** It exercises the module above the BIOS, not
the BIOS serial path, the CTS/RTS handshake or the 64-byte rings. For that, build
the demo ROM and run `../validate_link_emulator.py`, which drives two emulated
consoles through the real BIOS. The two are complements: this one sweeps a
parameter, that one proves the stack.
