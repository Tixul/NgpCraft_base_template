"""Check the real cc900 code against Nayuki, and decode a demo screenshot.

python check_rom.py --rom bin/main.ngc --map main.map --profile 3 \
    --emulator /path/to/vendor/emulator --bios /path/to/bios.bin

Requires the NgpCraft native emulator, numpy, OpenCV. The demo ROM must expose
ngpc_qr_encode/get_module/draw and show HTTPS://EXAMPLE.COM/QR/TEST123 at boot.
No files are written to the user's ROM or flash; all injection is in emulator RAM.
"""
import argparse
from pathlib import Path
import random
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'vendor'))
from qrcodegen import QrCode, QrSegment


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('rom', 'map', 'emulator', 'bios'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--profile', type=int, choices=(2, 3), required=True)
    args = p.parse_args()
    sys.path.insert(0, args.emulator)
    from core import native
    import numpy as np
    import cv2
    rom, bios = Path(args.rom).read_bytes(), Path(args.bios).read_bytes()
    symbols = Path(args.map).read_text()
    def boot():
        m = native.NativeMachine(rom, bios=bios)
        m.set_cart_wait(3); m.set_cart_data_wait(0); m.set_ldir_cost(14)
        m.reset(bios_handoff=True)
        if hasattr(m, 'set_hw_guard'): m.set_hw_guard(0)
        return m
    m = boot()
    def call(name, values, fmt):
        cpu = m.cpu()
        cpu.pc = int(re.search(r'\b_' + name + r'\s+([0-9A-F]+)', symbols).group(1), 16)
        cpu.regs[7] = 0x6d00; cpu.iff_level = 7; cpu.sr_raw |= 0x7000
        m.write(0x6d00, struct.pack('<I' + fmt, 0x200100, *values))
        m.set_cpu(cpu); m.set_breakpoints([0x200100])
        summary, _ = m.run(300000, record=False)
        assert summary.stop_status == 40 and summary.stop_pc == 0x200100
        return m.cpu().regs[3] & 255
    n = 25 if args.profile == 2 else 29
    limit = 38 if args.profile == 2 else 77
    ecl = QrCode.Ecc.MEDIUM if args.profile == 2 else QrCode.Ecc.LOW
    rng = random.Random(1818)
    alphabet = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:'
    texts = [''.join(rng.choice(alphabet) for _ in range(i)) for i in range(1, limit + 1)]
    texts += ['HTTPS://EXAMPLE.COM/QR/TEST123', 'Z' * limit]
    for text in texts:
        m.write(0x5fff, b'\x5a' * 210)
        m.write(0x6200, text.encode() + b'\0')
        assert call('ngpc_qr_encode', [0x6000, 0x6200], 'II') == 1
        reference = QrCode.encode_segments([QrSegment.make_alphanumeric(text)], ecl, args.profile, args.profile, 0, False)
        data = m.read(0x6000, (n*n+7)//8)
        for y in range(n):
            for x in range(n):
                bit = y*n+x
                assert ((data[bit//8] >> (bit%8)) & 1) == reference.get_module(x, y)
        assert m.read(0x5fff, 1) == b'\x5a' and m.read(0x60d0, 1) == b'\x5a'
        assert m.read(0x6200, len(text)+1) == text.encode()+b'\0'
    # Check independent contexts and invalidation after previous success.
    m.write(0x6200, b'SECOND\0')
    previous = m.read(0x6000, 208)
    assert call('ngpc_qr_encode', [0x6400, 0x6200], 'II') == 1
    assert m.read(0x6000, 208) == previous
    for text in ('', 'lowercase', 'A'*(limit+1), 'BAD_NAME', 'HTTPS://X/?Q=1'):
        m.write(0x6200, text.encode()+b'\0')
        assert call('ngpc_qr_encode', [0x6000, 0x6200], 'II') == 0
        assert call('ngpc_qr_get_module', [0x6000, 0, 0], 'IHH') == 0
    assert call('ngpc_qr_encode', [0, 0x6200], 'II') == 0
    assert call('ngpc_qr_encode', [0x6000, 0], 'II') == 0
    # Valid secondary context; reject placement before touching VRAM/palettes.
    before = m.read(0x9000, 0x3000) + m.read(0x8200, 0x200)
    for values in ([0x6400,2,128,0,0,0], [0x6400,0,497,0,0,0],
                   [0x6400,0,128,16,0,0], [0x6400,0,128,0,20,0],
                   [0x6400,0,128,0,0,19], [0x6000,0,128,0,0,0]):
        assert call('ngpc_qr_draw', values, 'IHHHHH') == 0
    assert m.read(0x9000,0x3000)+m.read(0x8200,0x200) == before
    assert call('ngpc_qr_draw', [0x6400,1,496,15,0,0], 'IHHHHH') == 1
    assert m.read(0x9800,2) == (496 | (15<<9)).to_bytes(2,'little')
    m.close()
    m = boot(); m.run_frames(50)
    fb = np.array(m.framebuffer(), dtype=np.uint16).reshape(152,160)
    image = np.stack([((fb>>8)&15)*17,((fb>>4)&15)*17,(fb&15)*17],axis=-1).astype('uint8')
    assert cv2.QRCodeDetector().detectAndDecode(image)[0] == 'HTTPS://EXAMPLE.COM/QR/TEST123'
    m.close()
    print('PASS profile',args.profile,':',len(texts),'reference matrices, contexts, invalid input, renderer bounds, SCR2, screenshot decode')


if __name__ == '__main__': main()
