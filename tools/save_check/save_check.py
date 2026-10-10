"""Emulator bench for src/core/ngpc_flash.c (two-bank journal).

Builds a throw-away copy of the template with save_test_main.c as main, then
drives that ROM's own main loop through RAM commands, so every flash call runs
like a game makes it (main loop, interrupts on, VBlank live). Emulator only:
this is NOT a hardware validation.

    py -3 tools/save_check/save_check.py [--keep]

Required environment:
    NGPC_EMU   NgpCraft emulator checkout (the folder containing core/native.py)
    NGPC_BIOS  a bios.bin dumped from your own console
Optional:
    THOME           Toshiba t900 root (default C:/t900)
    NGPC_SAVE_WORK  build folder (default: <system temp>/ngpc_save_test)
"""
from __future__ import annotations
import binascii, os, pathlib, re, shutil, struct, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
TPL = HERE.parents[1]
WORK = pathlib.Path(os.environ.get("NGPC_SAVE_WORK",
                                   pathlib.Path(tempfile.gettempdir()) / "ngpc_save_test"))
THOME = os.environ.get("THOME", "C:/t900")
for _var in ("NGPC_EMU", "NGPC_BIOS"):
    if not os.environ.get(_var) or not pathlib.Path(os.environ[_var]).exists():
        sys.exit("set %s (see the docstring)" % _var)
EMU = pathlib.Path(os.environ["NGPC_EMU"])
BIOS = pathlib.Path(os.environ["NGPC_BIOS"])
MAGIC = bytes.fromhex("cafe2026")
SIZES = {1: 0x80000, 2: 0x100000, 3: 0x200000}
BANK_A = {1: 0x07A000, 2: 0x0FA000, 3: 0x1FA000}

sys.path.insert(0, str(EMU))
from core import native  # noqa: E402


def rmtree(path: pathlib.Path) -> None:
    """A file named "nul" (left by older makefiles) needs the \\\\?\\ prefix."""
    def retry(func, p, _exc):
        if os.name != "nt":
            return func(p)
        # abspath() would turn "...\nul" into the device "\\.\nul"
        full = os.path.join(os.path.abspath(os.path.dirname(p)), os.path.basename(p))
        os.remove("\\\\?\\" + full)
    if path.exists():
        shutil.rmtree(path, onexc=retry) if sys.version_info >= (3, 12) \
            else shutil.rmtree(path, onerror=retry)


def build(save_size: int) -> tuple[bytes, dict]:
    out = WORK / f"s{save_size}"
    rmtree(out)
    out.mkdir(parents=True)
    for name in ("src", "GraphX", "sound", "tools"):
        shutil.copytree(TPL / name, out / name, ignore=shutil.ignore_patterns("__pycache__"))
    for name in ("makefile", "ngpc.lcf", "asm900.exe", "thc1.exe", "thc2.exe"):
        shutil.copy2(TPL / name, out / name)
    shutil.copy2(HERE / "save_test_main.c", out / "src" / "main.c")
    h = out / "src/core/ngpc_flash.h"
    h.write_text(h.read_text(encoding="utf-8").replace(
        "#define SAVE_SIZE   256", f"#define SAVE_SIZE   {save_size}"), encoding="utf-8")
    env = dict(os.environ, THOME=THOME, PATH=THOME + r"\BIN;" + os.environ["PATH"])
    r = subprocess.run(["make", "PYTHON=py -3", "NGP_ENABLE_FLASH_SAVE=1", "NGP_ENABLE_SOUND=0"],
                       cwd=out, env=env, capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stdout[-3000:] + r.stderr[-3000:])
    txt = (out / "main.map").read_text(encoding="latin-1")
    txt = txt[txt.index("Symbol table"):] if "Symbol table" in txt else txt
    syms = {}
    for m in re.finditer(r"^\s+(_\w+)\s*\n?\s+([0-9A-F]{4,6})\s", txt, re.M):
        syms.setdefault(m.group(1), int(m.group(2), 16))
    return (out / "bin/main.ngc").read_bytes(), syms


class Rig:
    def __init__(self, rom: bytes, syms: dict, size: int, save_size: int,
                 flash: bytes | None = None, pre_init=None):
        self.syms, self.size, self.n = syms, size, save_size
        img = bytearray(flash if flash is not None else rom + b"\xFF" * (size - len(rom)))
        self.m = native.NativeMachine(bytes(img), bios=BIOS.read_bytes())
        self.m.set_flash_size(size)
        self.m.set_cart_wait(3); self.m.set_cart_data_wait(0); self.m.set_ldir_cost(14)
        self.m.reset(bios_handoff=True)
        if hasattr(self.m, "set_hw_guard"):
            self.m.set_hw_guard(0)
        self.wait(0)

    def peek(self, name, n=1):
        return self.m.read(self.syms["_" + name], n)

    def wait(self, before):
        for _ in range(900):
            self.m.run_frames(1)
            if self.peek("g_done")[0] != before:
                return
        raise AssertionError("command never completed")

    def cmd(self, c, arg=0):
        before = self.peek("g_done")[0]
        self.m.write(self.syms["_g_arg"], struct.pack("<I", arg))
        self.m.write(self.syms["_g_cmd"], bytes([c]))
        self.wait(before)
        return struct.unpack("<H", self.peek("g_res", 2))[0]

    def info(self):
        return tuple(self.peek("g_info", 4))

    def loaded_seed(self):
        d = self.peek("g_load", 6)
        return None if d[:4] != MAGIC else d[4] | d[5] << 8

    def flash(self):
        return self.m.read(0x200000, self.size)

    def busy_faults(self):
        return self.m.hw_violations(native.HW_FLASH_BUSY_FETCH)


def records(img: bytes, cart: int, n: int):
    out = []
    a = BANK_A[cart]
    for bank in (a, a - 0x2000):
        for off in range(bank, bank + 0x2000, n):
            d = img[off:off + n]
            if d[:4] != MAGIC or d[n - 2] != 0x4A or d[n - 1] != 0:
                continue
            if struct.unpack("<H", d[n - 4:n - 2])[0] != binascii.crc_hqx(d[:n - 4], 0xFFFF):
                continue
            out.append((struct.unpack("<I", d[n - 8:n - 4])[0], off, d))
    return out


def seal(n, seed, seq):
    d = bytearray(MAGIC + bytes(((seed + i) & 255) for i in range(4, n - 8)) + bytes(8))
    d[4], d[5] = seed & 255, seed >> 8 & 255
    d[n - 8:n - 4] = struct.pack("<I", seq)
    d[n - 4:n - 2] = struct.pack("<H", binascii.crc_hqx(bytes(d[:n - 4]), 0xFFFF))
    d[n - 2], d[n - 1] = 0x4A, 0
    return bytes(d)


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def suite(rom, syms, n):
    per_bank = 0x2000 // n
    total = per_bank * 5 + 3
    # -- 16 Mbit: long run, rollovers, reboots ---------------------------------
    r = Rig(rom, syms, 0x200000, n)
    check(r.info()[:3] == (1, 3, 0), ("fresh 16M", r.info()))
    base = r.flash()
    snaps = {}
    for i in range(1, total + 1):
        check(r.cmd(1, i) == 1, ("save refused", i))
        rec = max(records(r.flash(), 3, n))
        check(rec[0] == i and rec[2][4] | rec[2][5] << 8 == i, ("newest", i, rec[:2]))
        img = r.flash()
        check(img[:0x1F8000] == base[:0x1F8000], ("ROM below bank B touched", i))
        check(img[0x1FC000:] == base[0x1FC000:], ("BIOS block 34 touched", i))
        if i in (1, per_bank, per_bank + 1, 2 * per_bank, 2 * per_bank + 1, total):
            snaps[i] = img
    faults = r.busy_faults()
    check(faults == 0, ("flash-busy-fetch", faults))
    print(f"  PASS {total} saves, {total // per_bank} bank switches, readback exact, "
          f"code/BIOS block intact, 0 flash-busy-fetch")
    for i, img in snaps.items():
        b = Rig(rom, syms, 0x200000, n, flash=img)
        check(b.info()[2] == 1 and b.loaded_seed() == i, ("reboot", i, b.info(), b.loaded_seed()))
    print(f"  PASS reboot after {sorted(snaps)} saves loads the newest record")

    # -- torn writes: the previous record survives -----------------------------
    k = per_bank + 3
    prior = Rig(rom, syms, 0x200000, n)
    for i in range(1, k + 1):
        prior.cmd(1, i)
    prior_img = prior.flash()
    _, last_off, _ = max(records(prior_img, 3, n))
    nxt = last_off + n
    new = seal(n, k + 1, k + 1)
    for cut in (0, 1, 4, n // 2, n - 8, n - 4, n - 2, n - 1):
        img = bytearray(prior_img)
        img[nxt:nxt + n] = new[:cut] + b"\xFF" * (n - cut)
        b = Rig(rom, syms, 0x200000, n, flash=bytes(img))
        check(b.loaded_seed() == k, ("torn", cut, b.loaded_seed()))
        check(b.cmd(1, 500) == 1, ("save after torn", cut))
        rec = max(records(b.flash(), 3, n))
        check(rec[0] == k + 1 and rec[1] != nxt or cut == 0, ("torn slot reused", cut, hex(rec[1])))
        c = Rig(rom, syms, 0x200000, n, flash=b.flash())
        check(c.loaded_seed() == 500, ("reboot after torn", cut, c.loaded_seed()))
    print("  PASS 8 cut points: previous record kept, partly written slot skipped")

    # -- corruption: newest record rejected, previous one loads ----------------
    for pos in (0, 4, n // 2, n - 8, n - 4, n - 3, n - 2, n - 1):
        img = bytearray(prior_img)
        img[last_off + pos] ^= 0x01 if pos != n - 1 else 0xFF
        b = Rig(rom, syms, 0x200000, n, flash=bytes(img))
        check(b.loaded_seed() == k - 1, ("corrupt", pos, b.loaded_seed()))
    print("  PASS 8 corruptions: CRC/tag/commit reject the record, previous one loads")

    # -- inactive bank partly erased / garbage, then rollover ------------------
    r = Rig(rom, syms, 0x200000, n)
    for i in range(1, per_bank + 1):
        r.cmd(1, i)                         # bank A full, bank B blank
    img = bytearray(r.flash())
    b_off = BANK_A[3] - 0x2000
    img[b_off + 100:b_off + 3000] = bytes(range(256)) * 11 + bytes(2900 - 2816)
    b = Rig(rom, syms, 0x200000, n, flash=bytes(img))
    check(b.loaded_seed() == per_bank, "garbage in B hides A")
    check(b.cmd(1, 777) == 1, "rollover over garbage")
    recs = records(b.flash(), 3, n)
    check(max(recs)[1] == b_off and max(recs)[0] == per_bank + 1, ("rollover", hex(max(recs)[1])))
    check(b.flash()[b_off + n:b_off + 0x2000] == b"\xFF" * (0x2000 - n), "B not erased")
    check(b.flash()[BANK_A[3]:BANK_A[3] + 0x2000] == bytes(img[BANK_A[3]:BANK_A[3] + 0x2000]),
          "current bank A was touched")
    print("  PASS rollover erases the dirty inactive bank, never the current one")

    # -- sequence wrap ---------------------------------------------------------
    img = bytearray(rom + b"\xFF" * (0x200000 - len(rom)))
    img[BANK_A[3]:BANK_A[3] + n] = seal(n, 9, 0xFFFFFFFF)
    b = Rig(rom, syms, 0x200000, n, flash=bytes(img))
    check(b.loaded_seed() == 9, "wrap load")
    check(b.cmd(1, 10) == 1, "wrap save")
    c = Rig(rom, syms, 0x200000, n, flash=b.flash())
    check(c.loaded_seed() == 10, ("wrap newest", c.loaded_seed()))
    print("  PASS sequence 0xFFFFFFFF -> 0 stays newest")

    # -- refusals --------------------------------------------------------------
    r = Rig(rom, syms, 0x200000, n)
    check(r.cmd(4) == 0, "bad magic accepted")
    before = r.flash()
    r.m.write(0x6C58, b"\x00")              # cartridge size unknown
    r.cmd(2)
    check(r.info()[0] == 0, ("unknown cart not refused", r.info()))
    check(r.cmd(1, 5) == 0 and r.cmd(3) == 0, "refused cart still writes")
    check(r.flash() == before, "flash changed while refused")
    print("  PASS bad magic refused; unknown cart size refuses save and erase, flash untouched")

    # -- migration from the old append-only driver -----------------------------
    def old_record(seed):           # pre-2026-10 layout: magic + data, zero padding
        d = bytearray(n)
        d[:4] = MAGIC
        d[4], d[5] = seed & 255, seed >> 8 & 255
        for j in range(6, 80):
            d[j] = (seed + j) & 255
        return bytes(d)
    for count in (5, per_bank):     # bank A partly / completely filled by the old driver
        img = bytearray(rom + b"\xFF" * (0x200000 - len(rom)))
        for s in range(count):
            img[BANK_A[3] + s * n:BANK_A[3] + (s + 1) * n] = old_record(300 + s)
        old_a = bytes(img[BANK_A[3]:BANK_A[3] + 0x2000])
        b = Rig(rom, syms, 0x200000, n, flash=bytes(img))
        check(b.info()[2] == 0, "legacy record taken as journal")
        check(b.cmd(5) == 1 and b.loaded_seed() == 300 + count - 1, ("legacy load", count, b.loaded_seed()))
        check(b.cmd(1, 900) == 1, ("save after legacy", count))
        rec = max(records(b.flash(), 3, n))
        where = BANK_A[3] + count * n if count < per_bank else BANK_A[3] - 0x2000
        check(rec[1] == where, ("journal slot after legacy", count, hex(rec[1])))
        check(b.flash()[BANK_A[3]:BANK_A[3] + count * n] == old_a[:count * n], "legacy slots touched")
        c = Rig(rom, syms, 0x200000, n, flash=b.flash())
        check(c.info()[2] == 1 and c.loaded_seed() == 900, ("reboot after migration", count))
    blank = Rig(rom, syms, 0x200000, n)
    check(blank.cmd(5) == 0, "legacy found on a blank cart")
    print("  PASS migration: newest old record recovered, kept intact, first save goes after it "
          "(or to bank B when A is full)")

    # -- explicit erase --------------------------------------------------------
    r = Rig(rom, syms, 0x200000, n)
    for i in range(1, 4):
        r.cmd(1, i)
    check(r.cmd(3) == 1, "erase failed")
    r.cmd(2)
    check(r.info()[2] == 0, "record survived erase")
    check(r.flash()[BANK_A[3] - 0x2000:BANK_A[3] + 0x2000] == b"\xFF" * 0x4000, "banks not blank")
    print("  PASS ngpc_flash_erase blanks both banks")

    # -- 4 and 8 Mbit ----------------------------------------------------------
    for cart in (1, 2):
        size = SIZES[cart]
        r = Rig(rom, syms, size, n)
        check(r.info()[:3] == (1, cart, 0), ("cart", cart, r.info()))
        base = r.flash()
        for i in range(1, per_bank + 3):
            check(r.cmd(1, i) == 1, ("save", cart, i))
        img = r.flash()
        recs = records(img, cart, n)
        check(max(recs)[0] == per_bank + 2, ("records", cart))
        check(img[:BANK_A[cart] - 0x2000] == base[:BANK_A[cart] - 0x2000], ("code touched", cart))
        check(img[BANK_A[cart] + 0x2000:] == base[BANK_A[cart] + 0x2000:], ("above bank A", cart))
        b = Rig(rom, syms, size, n, flash=img)
        check(b.loaded_seed() == per_bank + 2, ("reboot", cart))
        check(r.busy_faults() == 0, "busy fetch")
    print("  PASS 4 Mbit (0x07A000) and 8 Mbit (0x0FA000): own banks, nothing else touched")


def negative_control(rom, syms, n):
    """Same ROM with the erase stub's `di` turned into two NOPs: the emulator
    must now catch an interrupt fetched from a busy chip. Without this, a 0 above
    could mean the emulator just does not look."""
    erase = syms["_ngpc_flash_erase_asm"] - 0x200000
    check(rom[erase:erase + 2] == b"\x06\x07", "erase does not start with di")
    bad = bytearray(rom); bad[erase:erase + 2] = b"\x00\x00"
    r = Rig(bytes(bad), syms, 0x200000, n)
    hung = None
    for i in range(1, 2 * (0x2000 // n) + 2):   # 2nd switch = first real erase
        try:
            r.cmd(1, i)
        except AssertionError:
            hung = i                            # the machine never came back
            break
    f = r.busy_faults()
    check(f > 0, "negative control: no fault without di - the bench proves nothing")
    print(f"  PASS negative control: without `di` the erase triggers {f} flash-busy-fetch"
          + (f", ROM lost at save {i}" if hung else ""))


def main():
    for n in (256, 512):
        print(f"SAVE_SIZE={n}")
        rom, syms = build(n)
        wb = syms["_ngpc_flash_write_bios"] - 0x200000
        check(rom[wb:wb + 2] == b"\x06\x07", "write_bios does not start with di")
        suite(rom, syms, n)
        negative_control(rom, syms, n)
    if "--keep" not in sys.argv:
        rmtree(WORK)
    print("ALL PASS (emulator only - no hardware validation)")


if __name__ == "__main__":
    main()
