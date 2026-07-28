r"""End-to-end proof of the ngpc_link module: TWO emulated consoles, wired
through the real BIOS serial path (nothing above it is simulated).

This script is not part of the ROM. It drives an NGPC emulator from Python.

Requirements
------------
* an NGPC emulator exposing `core.native_session.NativeSession` and
  `core.link.InProcessLink`, plus a real `bios.bin`;
* the demo ROM, built from example_link_main.c (see README);
* its .map, which gives the addresses of the module globals (they move on
  every build).

Point NGPC_EMU at the emulator root (and NGPC_BIOS at the BIOS if it does not
sit next to it):

    set NGPC_EMU=D:\path\to\emulator
    py -3 validate_link_emulator.py <rom.ngc> <rom.map> [<rom16.ngc> <rom16.map>]

The last two arguments are a second ROM built with a DIFFERENT
NGPC_LINK_PAYLOAD: they enable gates 5 and 6 (a larger payload, then two
consoles that disagree on the packet size).
"""
import os
import pathlib
import sys

EMU = pathlib.Path(os.environ.get("NGPC_EMU", "."))
BIOS = pathlib.Path(os.environ.get("NGPC_BIOS", EMU / "bios.bin"))

if not (EMU / "core" / "link.py").is_file():
    sys.exit("NGPC_EMU does not point at an emulator exposing core/link.py")

sys.path.insert(0, str(EMU))
from core.native_session import NativeSession      # noqa: E402
from core.link import InProcessLink                # noqa: E402

NAMES = {0: "OFF", 1: "SEARCHING", 2: "READY", 3: "LOST", 4: "MISMATCH"}
PAD_A, PAD_B = 0x08, 0x01          # A holds RIGHT, B holds UP

ok = True


def check(cond, msg):
    global ok
    print(("  OK    " if cond else "  FAIL  ") + msg)
    ok = ok and cond


def syms(mapfile):
    """Module globals, read from the Toshiba linker .map."""
    lines = pathlib.Path(mapfile).read_text(errors="ignore").splitlines()
    out = {}
    for i, ln in enumerate(lines):
        s = ln.strip()
        if s.startswith("_ngpc_link_") or s == "_g_vb_counter":
            try:
                out[s[1:]] = int(lines[i + 1].split()[0], 16)
            except (ValueError, IndexError):
                pass
    return out


def u16(m, a):
    d = m.read(a, 2)
    return d[0] | (d[1] << 8)


def stats(m, s):
    b = s["ngpc_link_stats"]
    return dict(tx_bytes=u16(m, b), rx_bytes=u16(m, b + 2), tx_pkts=u16(m, b + 4),
                rx_pkts=u16(m, b + 6), bad_sum=u16(m, b + 8),
                tx_skipped=m.read(b + 10, 1)[0], resyncs=m.read(b + 11, 1)[0],
                gap=m.read(b + 12, 1)[0])


def state(m, s):
    return m.read(s["ngpc_link_state"], 1)[0]


def report(tag, m, s, payload=4):
    st = state(m, s)
    print(f"  {tag}: {NAMES.get(st, st)} host={m.read(s['ngpc_link_host'], 1)[0]} "
          f"out={m.read(s['ngpc_link_out'], payload).hex()} "
          f"in={m.read(s['ngpc_link_in'], payload).hex()} {stats(m, s)}")
    return st


def pair(rom_a, map_a, rom_b, map_b, frames):
    sa, sb = syms(map_a), syms(map_b)
    a = NativeSession(rom_a, bios_path=BIOS, autosave=False)
    b = NativeSession(rom_b, bios_path=BIOS, autosave=False)
    link = InProcessLink(a.machine, b.machine)
    ready_at = None
    for i in range(frames):
        a.machine.write(0x00B0, bytes([PAD_A]))
        b.machine.write(0x00B0, bytes([PAD_B]))
        a.run_frames(1)
        b.run_frames(1)
        link.pump()
        if ready_at is None and state(a.machine, sa) == 2 and state(b.machine, sb) == 2:
            ready_at = i
    return a, sa, b, sb, link, ready_at


def main(argv):
    global ok
    if len(argv) < 3:
        print(__doc__)
        return 2
    rom, mp = argv[1], argv[2]
    rom16, map16 = (argv[3], argv[4]) if len(argv) >= 5 else (None, None)

    print("=== 1. A lone console, no peer: must neither freeze nor crash ===")
    s = syms(mp)
    solo = NativeSession(rom, bios_path=BIOS, autosave=False)
    solo.run_frames(120)
    vb1 = solo.machine.read(s["g_vb_counter"], 1)[0]
    solo.run_frames(120)
    vb2 = solo.machine.read(s["g_vb_counter"], 1)[0]
    st = report("solo", solo.machine, s)
    check(vb1 != vb2, f"the frame counter advances ({vb1} -> {vb2}): no ei 6 trap")
    check(st == 1, "stays in SEARCHING with no peer")

    print("\n=== 2. Two consoles wired together ===")
    a, sa, b, sb, link, ready_at = pair(rom, mp, rom, mp, 600)
    print(f"  session established at frame {ready_at}")
    st_a, st_b = report("A", a.machine, sa), report("B", b.machine, sb)
    out_a = a.machine.read(sa["ngpc_link_out"], 4)
    out_b = b.machine.read(sb["ngpc_link_out"], 4)
    check(st_a == 2 and st_b == 2, "both consoles are READY")
    check(ready_at is not None and ready_at < 60, "established in under a second")
    check(b.machine.read(sb["ngpc_link_in"], 4) == out_a, "B receives what A sends")
    check(a.machine.read(sa["ngpc_link_in"], 4) == out_b, "A receives what B sends")
    check(out_a != out_b, "the two consoles send different things")
    check(a.machine.read(sa["ngpc_link_host"], 1)[0]
          + b.machine.read(sb["ngpc_link_host"], 1)[0] == 1, "exactly one host")
    check(stats(a.machine, sa)["bad_sum"] == 0 and stats(b.machine, sb)["bad_sum"] == 0,
          "no broken checksum")
    check(stats(a.machine, sa)["rx_pkts"] > 400 and stats(b.machine, sb)["rx_pkts"] > 400,
          "one packet per frame received on both sides")
    check(stats(a.machine, sa)["tx_skipped"] == 0 and stats(b.machine, sb)["tx_skipped"] == 0,
          "no send skipped: the BIOS ring keeps up")
    check(link.bytes_ab > 3000 and link.bytes_ba > 3000,
          f"real traffic on the wire ({link.bytes_ab}/{link.bytes_ba} bytes)")

    print("\n=== 3. Cable pulled mid-session ===")
    for _ in range(200):
        a.machine.write(0x00B0, bytes([PAD_A]))
        a.run_frames(1)                      # no pump: the peer is gone
    check(report("A", a.machine, sa) == 3, "A goes LOST after the timeout")

    print("\n=== 4. Cable plugged back in ===")
    for _ in range(200):
        a.machine.write(0x00B0, bytes([0x20]))
        b.machine.write(0x00B0, bytes([PAD_B]))
        a.run_frames(1)
        b.run_frames(1)
        link.pump()
    st_a, st_b = report("A", a.machine, sa), report("B", b.machine, sb)
    check(st_a == 2 and st_b == 2, "the session rebuilds itself")
    check(b.machine.read(sb["ngpc_link_in"], 4) == a.machine.read(sa["ngpc_link_out"], 4),
          "and data flows again")

    if rom16:
        print("\n=== 5. Larger payload, on both sides ===")
        a, sa, b, sb, link, _ = pair(rom16, map16, rom16, map16, 400)
        st_a, st_b = state(a.machine, sa), state(b.machine, sb)
        out_a = a.machine.read(sa["ngpc_link_out"], 16)
        print(f"  A={NAMES[st_a]} B={NAMES[st_b]} out_A={out_a.hex()}")
        check(st_a == 2 and st_b == 2, "READY with a 16-byte payload")
        check(b.machine.read(sb["ngpc_link_in"], 16) == out_a,
              "the 16 bytes cross unchanged")

        print("\n=== 6. Two consoles that disagree on the packet size ===")
        a, sa, b, sb, link, _ = pair(rom, mp, rom16, map16, 400)
        st_a, st_b = state(a.machine, sa), state(b.machine, sb)
        print(f"  A={NAMES[st_a]}  B={NAMES[st_b]}")
        check(st_a == 4 and st_b == 4, "MISMATCH on both sides, not gibberish")
        check(a.machine.read(sa["ngpc_link_in"], 4) == b"\x00\x00\x00\x00",
              "no data delivered to the game")

    print("\nRESULT:", "ALL GREEN" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
