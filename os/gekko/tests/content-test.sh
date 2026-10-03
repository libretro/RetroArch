#!/bin/sh
# End to end on the Wii: a loader starts RetroArch with content on SD,
# the core draws and reads the pad.  Needs RetroArch built with fceumm
# and Python's PIL:
#   cp fceumm_libretro_wii.a libretro_wii.a
#   make -f Makefile.wii HAVE_LIBOGC=0
#   os/gekko/tests/content-test.sh retroarch_wii.elf
# The test ROM (nes-rom.py) is red, and green while A is held.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ELF=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export MTOOLS_SKIP_CHECK=1
python3 "$HERE/nes-rom.py" "$tmp/test.nes"
truncate -s 64M "$tmp/sd.img"
mkfs.fat -F 32 "$tmp/sd.img" > /dev/null
mmd -i "$tmp/sd.img" ::/roms
mcopy -i "$tmp/sd.img" "$tmp/test.nes" "::/roms/a test.nes"
# A held from 25 s to 30 s.
printf '\n\n\n\nPRESS A\nRELEASE A\n' > "$tmp/pads"
PADS="$tmp/pads" PADSTEP=5 SDIMG="$tmp/sd.img" DUMP="$tmp/frames" \
   ARGS='sd:/apps/retroarch/boot.dol|sd:/roms|a test.nes' \
   "$HERE/run-dolphin.sh" "$ELF" 40 sw > "$tmp/log" || true
python3 - "$tmp/frames" <<'PY'
import os, re, sys
from PIL import Image
d = sys.argv[1]
names = sorted(os.listdir(d) if os.path.isdir(d) else [],
               key=lambda f: int(re.findall(r'\d+', f)[0]))
seen = []
for f in names:
    im = Image.open(os.path.join(d, f)).convert('RGB')
    r, g, b = im.getpixel((im.width // 2, im.height // 2))
    c = 'red' if r > 150 and g < 100 else 'green' if g > 150 and r < 120 \
        else 'other'
    if not seen or seen[-1] != c:
        seen.append(c)
print('screen: ' + ' '.join(seen))
ok = 'red green red' in ' '.join(c for c in seen if c != 'other')
print('PASSED' if ok else 'FAILED')
sys.exit(0 if ok else 1)
PY
