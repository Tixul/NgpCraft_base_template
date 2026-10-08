"""Emulator check of ngpc_qr_result.c against result_codec.py, end to end.

Builds a throw-away copy of the template with result_test_main.c, runs it on
the NgpCraft native emulator, then checks:
  - every vector equals the Python reference (incl. an OV1 vector),
  - every error case returns 0 with an empty string,
  - the QR shown on screen (vector 0, full URL) matches Nayuki's QR of the
    same text module by module (screen read back at 4 px per module).

    python optional/ngpc_qr/tools/check_result.py
Env: NGPC_EMU (emulator checkout with core/native.py), NGPC_BIOS (your
bios.bin), optional THOME (t900 root, default C:/t900), NGPC_QR_WORK.
Emulator only, not a reading by a phone on a real LCD.
"""
from __future__ import annotations
import os, pathlib, re, shutil, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
TPL = HERE.parents[2]
sys.path.insert(0, str(HERE)); sys.path.insert(0, str(HERE / "vendor"))
import result_codec as rc  # noqa: E402
from qrcodegen import QrCode, QrSegment  # noqa: E402

for _v in ("NGPC_EMU", "NGPC_BIOS"):
    if not os.environ.get(_v) or not pathlib.Path(os.environ[_v]).exists():
        sys.exit("set %s (see the docstring)" % _v)
sys.path.insert(0, os.environ["NGPC_EMU"])
from core import native  # noqa: E402

WORK = pathlib.Path(os.environ.get("NGPC_QR_WORK", pathlib.Path(tempfile.gettempdir()) / "ngpc_qr_result_test"))
THOME = os.environ.get("THOME", "C:/t900")
URL = "HTTPS://WWW.NGPC-DEV.COM/OVERREV/"


def rmtree(path):
    def retry(func, p, _e):
        if os.name != "nt":
            return func(p)
        os.remove("\\\\?\\" + os.path.join(os.path.abspath(os.path.dirname(p)), os.path.basename(p)))
    if path.exists():
        shutil.rmtree(path, onexc=retry) if sys.version_info >= (3, 12) else shutil.rmtree(path, onerror=retry)


def build():
    rmtree(WORK)
    WORK.mkdir(parents=True)
    for name in ("src", "GraphX", "sound", "tools", "optional", "examples"):
        shutil.copytree(TPL / name, WORK / name, ignore=shutil.ignore_patterns("__pycache__"))
    for name in ("ngpc.lcf", "asm900.exe", "thc1.exe", "thc2.exe"):
        shutil.copy2(TPL / name, WORK / name)
    shutil.copy2(HERE / "result_test_main.c", WORK / "src" / "main.c")
    mk = (TPL / "makefile").read_text(encoding="utf-8")
    anchor = "OBJS += $(OBJ_DIR)/GraphX/intro_ngpc_craft_png.rel"
    assert anchor in mk
    mk = mk.replace(anchor, anchor + """
CDEFS += -Ioptional/ngpc_qr
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr.rel
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_draw.rel
OBJS += $(OBJ_DIR)/optional/ngpc_qr/ngpc_qr_result.rel
OBJS += $(OBJ_DIR)/examples/qr_example.rel""")
    (WORK / "makefile").write_text(mk, encoding="utf-8")
    env = dict(os.environ, THOME=THOME, PATH=THOME + "/BIN;" + os.environ["PATH"])
    r = subprocess.run(["make", "NGP_ENABLE_SOUND=0"], cwd=WORK, env=env, capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stdout[-3000:] + r.stderr[-3000:])
    txt = (WORK / "main.map").read_text(encoding="latin-1")
    syms = {m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"^\s+(_\w+)\s*\n?\s+([0-9A-F]{4,6})\s", txt, re.M)}
    return (WORK / "bin/main.ngc").read_bytes(), syms


def main():
    rom, syms = build()
    m = native.NativeMachine(rom, bios=pathlib.Path(os.environ["NGPC_BIOS"]).read_bytes())
    m.set_cart_wait(3); m.set_cart_data_wait(0); m.set_ldir_cost(14)
    m.reset(bios_handoff=True)
    for _ in range(600):
        m.run_frames(1)
        if m.read(syms["_g_ready"], 1)[0]:
            break
    else:
        sys.exit("test ROM never got ready")
    m.run_frames(30)                           # let the QR page settle
    lens = m.read(syms["_g_len"], 12)
    outs = [m.read(syms["_g_out"] + 80 * i, 80).split(b"\0")[0].decode("ascii") for i in range(12)]
    big = bytes((i * 7 + 3) & 255 for i in range(32))
    expect = [rc.encode(URL, "OV1", "PLAYER01", rc.OV1_DATA),
              rc.encode("", "T1", "N", b"\xA5"),
              rc.encode("", "MG1", "ABCDEFGHIJKLMNOP", big)]
    assert expect[0] == URL + rc.OV1_TEXT
    for i, e in enumerate(expect):
        assert outs[i] == e and lens[i] == len(e), (i, outs[i], e)
        assert rc.decode(outs[i], URL if i == 0 else "")[1:] in [("PLAYER01", rc.OV1_DATA),
                                                                ("N", b"\xA5"), ("ABCDEFGHIJKLMNOP", big)]
    print(f"PASS 3 vectors identical to the reference, incl. an OV1 vector ({len(expect[0])} chars)")
    for i in range(3, 12):
        assert lens[i] == 0 and outs[i] == "", (i, lens[i], outs[i])
    print("PASS 9 refusals (too long for V3-L, lowercase, '?', len 0/33, tag 9, name 17, no tag, small buffer)")
    fb = m.framebuffer()
    qr = QrCode.encode_segments([QrSegment.make_alphanumeric(expect[0])], QrCode.Ecc.LOW, 3, 3, 0, False)
    bad = 0
    for y in range(29):
        for x in range(29):
            px = fb[((4 + y) * 4 + 2) * 160 + (4 + x) * 4 + 2]
            dark = (px & 0xFFF) == 0
            bad += dark != qr.get_module(x, y)
    for x in range(38):                        # quiet zone stays white
        for y in (0, 37):
            assert fb[(y * 4 + 2) * 160 + x * 4 + 2] & 0xFFF == 0xFFF, ("quiet zone", x, y)
    assert bad == 0, f"{bad} modules differ on screen"
    print("PASS on-screen QR of the full URL = Nayuki V3-L mask 0, 841/841 modules, quiet zone white")
    rmtree(WORK)
    print("ALL PASS (emulator only - read the QR with a phone on real hardware before release)")


if __name__ == "__main__":
    main()
