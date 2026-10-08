"""Build the standalone engine sample with Python and the Toshiba toolchain."""
from pathlib import Path
import argparse
import os
import sys
import shutil

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools'))
import build_utils

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--thome', help='Toshiba SDK root containing BIN, INCLUDE and LIB')
    args = parser.parse_args()
    if args.thome:
        os.environ['THOME'] = str(Path(args.thome).resolve())
    if not os.environ.get('THOME'):
        parser.error('Set THOME or pass --thome PATH to your Toshiba SDK')
    os.chdir(ROOT)
    flags = ['-Nb2', '-DNGP_ENABLE_SOUND=0', '-DNGP_ENABLE_FLASH_SAVE=0',
             '-DNGP_ENABLE_DEBUG=0', '-DNGP_PROFILE_RELEASE=1',
             '-DNGP_ENABLE_DMA=1', '-DNGP_ENABLE_SPRMUX=0', '-DNGP_ENABLE_PROFILER=0',
             '-DNGP_DMA_ALLOW_VBLANK_TRIGGER=0', '-DNGP_DMA_INSTALL_DONE_ISR=0',
             '-DNGP_DMA_INSTALL_REARM_ISR=1', '-DID_PROBE=0',
             '-DNGP_FAR=__far', '-DNGP_NEAR=__near']
    sources = sorted([*Path('src').rglob('*.c'), *Path('src').rglob('*.asm'),
                      *Path('GraphX').rglob('*.c')])
    # cc900 places the cartridge header in main's const section. It must be
    # the first linked object so the BIOS finds it at cartridge offset zero.
    sources.remove(Path('src/main.c'))
    sources.insert(0, Path('src/main.c'))
    objects = []
    # Compile every source: header/flag changes must never reuse stale objects.
    for source in sources:
        obj = Path('build/obj') / source.with_suffix('.rel')
        command = build_utils.cmd_compile if source.suffix == '.c' else build_utils.cmd_asm
        code = command(str(source), str(obj), flags) if source.suffix == '.c' else command(str(source), str(obj))
        if code:
            return code
        objects.append(str(obj))
    code = build_utils.cmd_link('raceengine.abs', 'ngpc.lcf', objects)
    if code:
        return code
    code = build_utils.cmd_s242ngp('raceengine.s24')
    if code:
        return code
    Path('bin').mkdir(exist_ok=True)
    shutil.copyfile('raceengine.ngp', 'bin/raceengine.ngc')
    # No flash save in this sample: do not pad the cartridge to 2 MiB.
    print('Built bin/raceengine.ngc (%d bytes)' % Path('bin/raceengine.ngc').stat().st_size)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
