#!/usr/bin/env python3
"""Writes a 24 KiB NES ROM that fills the screen red, and green while
the pad's A button is held.  usage: nes-rom.py <out.nes>"""

import sys

ORG = 0xC000
code = bytearray()
labels = {}
fixups = []


def emit(*b):
    code.extend(b)


def label(name):
    labels[name] = ORG + len(code)


def absolute(op, name):
    emit(op, 0, 0)
    fixups.append((len(code) - 2, name, False))


def branch(op, name):
    emit(op, 0)
    fixups.append((len(code) - 1, name, True))


# sei; cld; no APU frame IRQ; stack; PPU off
emit(0x78, 0xD8, 0xA2, 0x40, 0x8E, 0x17, 0x40, 0xA2, 0xFF, 0x9A, 0xE8,
     0x8E, 0x00, 0x20, 0x8E, 0x01, 0x20)
for name in ('warm1', 'warm2'):         # two vblanks for the PPU
    label(name)
    emit(0x2C, 0x02, 0x20)
    branch(0x10, name)
label('frame')                          # each vblank:
emit(0x2C, 0x02, 0x20)
branch(0x10, 'frame')
emit(0xA9, 1, 0x8D, 0x16, 0x40,         # latch the pad, read A
     0xA9, 0, 0x8D, 0x16, 0x40,
     0xAD, 0x16, 0x40, 0x29, 1, 0xAA)
absolute(0xBD, 'colors')                # backdrop colour by A
emit(0xA0, 0x3F, 0x8C, 0x06, 0x20, 0xA0, 0, 0x8C, 0x06, 0x20,
     0x8D, 0x07, 0x20,
     0x8C, 0x06, 0x20, 0x8C, 0x06, 0x20,  # address and scroll back to 0
     0x8C, 0x05, 0x20, 0x8C, 0x05, 0x20,
     0xA9, 0x0A, 0x8D, 0x01, 0x20)        # background on
absolute(0x4C, 'frame')
label('colors')
emit(0x16, 0x2A)                        # red, green

for at, name, rel in fixups:
    target = labels[name]
    if rel:
        code[at] = (target - (ORG + at + 1)) & 0xFF
    else:
        code[at:at + 2] = bytes((target & 0xFF, target >> 8))

prg = bytearray(0x4000)
prg[:len(code)] = code
prg[0x3FFA:] = bytes((0, 0xC0)) * 3     # NMI, reset, IRQ
with open(sys.argv[1], 'wb') as f:
    f.write(b'NES\x1a\x01\x01' + bytes(10) + prg + bytes(0x2000))
