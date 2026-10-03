#!/usr/bin/env python3
"""Converts an ELF to a DOL: loaded sections merged into runs of code
and of data, the rest of the image up to its end as .bss.
usage: elf2dol.py <in.elf> <out.dol>"""

import struct
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 2:
        sys.exit('not a 32-bit big-endian ELF')
    entry, _, shoff = struct.unpack_from('>III', data, 24)
    shentsize, shnum = struct.unpack_from('>HH', data, 46)
    shdrs = [struct.unpack_from('>10I', data, shoff + i * shentsize)
             for i in range(shnum)]
    runs = []
    image_end = 0
    for sh in sorted((h for h in shdrs if h[2] & 2 and h[5]),
                     key=lambda h: h[3]):
        _, kind, flags, addr, off, size = sh[:6]
        image_end = max(image_end, addr + size)
        if kind == 8:                      # NOBITS
            continue
        body = data[off:off + size]
        code = bool(flags & 4)
        if runs and runs[-1][2] == code and \
                0 <= addr - (runs[-1][0] + len(runs[-1][1])) < 32:
            last = runs[-1][1]
            last += b'\0' * (addr - runs[-1][0] - len(last)) + body
        else:
            runs.append((addr, bytearray(body), code))
    text = [r for r in runs if r[2]]
    dat = [r for r in runs if not r[2]]
    if len(text) > 7 or len(dat) > 11:
        sys.exit('too many sections for a DOL')
    offs, addrs, sizes = [0] * 18, [0] * 18, [0] * 18
    out = bytearray(0x100)
    for slot, (addr, body, _) in list(enumerate(text)) + \
            [(7 + i, r) for i, r in enumerate(dat)]:
        body = body + b'\0' * (-len(body) % 32)
        offs[slot], addrs[slot], sizes[slot] = len(out), addr, len(body)
        out += body
    bss = max(a + len(b) for a, b, _ in runs)
    struct.pack_into('>18I18I18IIII', out, 0, *(offs + addrs + sizes),
                     bss, max(image_end - bss, 0), entry)
    open(sys.argv[2], 'wb').write(out)


main()
