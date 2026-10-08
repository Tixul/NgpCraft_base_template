"""What ACTUALLY happens when a v1 build meets a v2 build.

⛔ THE CLAIM THIS REFUTED. The first draft of the module README and the devlog
said the two would meet as `NGPC_LINK_MISMATCH`. Measured here: they do not.
They both sit in SEARCHING for ever, with `bad_sum` climbing.

The reason is structural and worth knowing: `NGPC_LINK_PROTO` lives INSIDE the
HELLO body, and the body LENGTH is what changed (5 bytes to 7). A v1 parser
reads five body bytes and then takes v2's sixth as the checksum, which fails; a
v2 parser waits for seven and eats v1's checksum as body. Neither frame ever
validates, so the version byte is never reached. The version check protects
against a changed MEANING of the same-sized packet -- it cannot protect against
a changed size.

What matters is that this is still SAFE: no session, no garbage election, no
data delivered. And it has a signature a game can show -- bytes crossing while
`bad_sum` climbs and the state never leaves SEARCHING is "the other console runs
a different build", not "no cable".

    python check_v1_v2.py <v2.ngc> <v2.map> <v1.ngc> <v1.map>
"""
import os
import pathlib
import sys

EMU = pathlib.Path(os.environ["NGPC_EMU"])
BIOS = pathlib.Path(os.environ.get("NGPC_BIOS", EMU / "bios.bin"))
sys.path.insert(0, str(EMU))
from core.native_session import NativeSession      # noqa: E402
from core.link import InProcessLink                # noqa: E402

NAMES = {0: "OFF", 1: "SEARCHING", 2: "READY", 3: "LOST", 4: "MISMATCH"}


def symbols(map_path):
    """Module globals from the Toshiba linker .map -- the address sits on the
    line AFTER the name. Same parser as validate_link_emulator.py, deliberately:
    two readings of one format is how they drift apart."""
    lines = pathlib.Path(map_path).read_text(errors="ignore").splitlines()
    out = {}
    for i, ln in enumerate(lines):
        s = ln.strip()
        if s.startswith("_ngpc_link_"):
            try:
                out[s[1:]] = int(lines[i + 1].split()[0], 16)
            except (ValueError, IndexError):
                pass
    return out


def u16(m, a):
    d = m.read(a, 2)
    return d[0] | (d[1] << 8)


def main(argv):
    v2_rom, v2_map, v1_rom, v1_map = argv[1:5]
    sa, sb = symbols(v2_map), symbols(v1_map)
    for need in ("ngpc_link_state", "ngpc_link_host"):
        if need not in sa or need not in sb:
            sys.exit(f"{need} not found in the maps: {sorted(sa)} / {sorted(sb)}")

    a = NativeSession(pathlib.Path(v2_rom), bios_path=BIOS, autosave=False)
    b = NativeSession(pathlib.Path(v1_rom), bios_path=BIOS, autosave=False)
    link = InProcessLink(a.machine, b.machine)
    try:
        for _ in range(400):
            a.run_frames(1)
            b.run_frames(1)
            link.pump()
        st_a = a.machine.read(sa["ngpc_link_state"], 1)[0]
        st_b = b.machine.read(sb["ngpc_link_state"], 1)[0]
        ho_a = a.machine.read(sa["ngpc_link_host"], 1)[0]
        ho_b = b.machine.read(sb["ngpc_link_host"], 1)[0]
        print(f"  v2 console: {NAMES.get(st_a, st_a)} host={ho_a}")
        print(f"  v1 console: {NAMES.get(st_b, st_b)} host={ho_b}")
        print(f"  bytes crossed: {link.bytes_ab} / {link.bytes_ba}")

        bad_a = u16(a.machine, sa["ngpc_link_stats"] + 8)
        bad_b = u16(b.machine, sb["ngpc_link_stats"] + 8)
        print(f"  bad_sum: v2={bad_a}  v1={bad_b}")

        ok = True
        if link.bytes_ab < 50 or link.bytes_ba < 50:
            print("  FAIL  the two never even talked -- this proves nothing")
            ok = False
        for who, st in (("v2", st_a), ("v1", st_b)):
            if st == 2:
                print(f"  FAIL  the {who} console went READY against an incompatible peer")
                ok = False
            else:
                print(f"  OK    the {who} console never reached READY "
                      f"({NAMES.get(st, st)})")
        if ho_a or ho_b:
            print("  FAIL  a role was elected across incompatible builds")
            ok = False
        else:
            print("  OK    neither console claimed a role")
        if bad_a and bad_b:
            print("  OK    both count rejected frames -- the signature a game can show")
        else:
            print("  FAIL  no bad_sum: the frames were not even seen, so this is "
                  "indistinguishable from an unplugged cable")
            ok = False
        print("RESULT:", "ALL GREEN" if ok else "FAILED")
        return 0 if ok else 1
    finally:
        link.disconnect()
        a.close()
        b.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
