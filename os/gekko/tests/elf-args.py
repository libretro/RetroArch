#!/usr/bin/env python3
"""Passes arguments to an os/gekko ELF the way loaders do: the command
line goes in a segment past the image and the argument block after the
'_arg' tag at the entry point.  --attach also loads FILE at 0x80c00000
and adds its address and length (hex) to the arguments.
usage: elf-args.py <in.elf> <out.elf> [--attach FILE] arg..."""

import struct
import sys

ARGV_MAGIC = 0x5f617267
ATTACH_AT = 0x80c00000


def add_segment(elf, phoff, phentsize, addr, data):
    """Appends data as a loaded segment at addr."""
    phnum = struct.unpack_from('>H', elf, 44)[0]
    elf += b'\0' * (-len(elf) % 32)
    struct.pack_into('>8I', elf, phoff + phnum * phentsize, 1, len(elf),
                     addr, addr, len(data), len(data), 6, 32)
    struct.pack_into('>H', elf, 44, phnum + 1)
    elf += data


def main():
    args = sys.argv[3:]
    attach = None
    if args[:1] == ['--attach'] and len(args) > 1:
        attach = open(args[1], 'rb').read()
        args = args[2:] + ['%x' % ATTACH_AT, '%x' % len(attach)]
    if len(sys.argv) < 3 or not args:
        sys.exit(__doc__)
    elf = bytearray(open(sys.argv[1], 'rb').read())
    if elf[:4] != b'\x7fELF' or elf[4] != 1 or elf[5] != 2:
        sys.exit('not a 32-bit big-endian ELF')
    entry, phoff, shoff = struct.unpack_from('>III', elf, 24)
    phentsize, phnum, shentsize, shnum = struct.unpack_from('>HHHH', elf, 42)
    shdrs = [struct.unpack_from('>10I', elf, shoff + i * shentsize)
             for i in range(shnum)]
    image_end = max(h[3] + h[5] for h in shdrs if h[2] & 2)
    first_data = min(h[4] for h in shdrs if h[1] and h[5] and h[4])
    if phoff + (phnum + 2) * phentsize > first_data:
        sys.exit('no room for more program headers')

    line = b''.join(a.encode() + b'\0' for a in args)
    addr = (image_end + 0x1000 + 31) & ~31
    block = struct.pack('>6I', ARGV_MAGIC, addr, len(line), 0, 0, 0)
    for i in range(phnum):
        p_type, off, vaddr, _, filesz = struct.unpack_from(
            '>5I', elf, phoff + i * phentsize)
        if p_type == 1 and vaddr <= entry and entry + 32 <= vaddr + filesz:
            o = off + entry - vaddr
            if struct.unpack_from('>I', elf, o + 4)[0] != ARGV_MAGIC:
                sys.exit('no argument tag at the entry point')
            elf[o + 8:o + 8 + len(block)] = block
            break
    else:
        sys.exit('entry point not in the image')

    add_segment(elf, phoff, phentsize, addr, line)
    if attach is not None:
        if addr + len(line) > ATTACH_AT:
            sys.exit('image too large to attach a file')
        add_segment(elf, phoff, phentsize, ATTACH_AT, attach)
    open(sys.argv[2], 'wb').write(elf)


main()
