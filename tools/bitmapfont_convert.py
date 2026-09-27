#!/usr/bin/env python3
"""Convert RetroArch's built-in bitmap font between its three forms.

The font that gfx/bitmapfont.c compiles in is 256 glyphs of 5x10
pixels, one bit per pixel, seven bytes per glyph. Two other
representations of the same data live beside this script:

  bitmapfont.bin   the raw 1792 bytes, in glyph order
  bitmapfont.bmp   a 256x256 4bpp sheet, 16x16 cells of 16x16 pixels,
                   glyph in the top-left 5x10 of each cell, palette
                   index 1 = ink. This is the form you can edit in an
                   image editor.

Usage:
  bitmapfont_convert.py export <bitmapfont.c> [--bin F] [--bmp F]
  bitmapfont_convert.py import <bitmapfont.bmp> [--bin F] [--carray F]
  bitmapfont_convert.py check  <bitmapfont.c> <bitmapfont.bin> <bitmapfont.bmp>

'check' is the one worth wiring into anything automated: it verifies
all three agree, which they had not for some time - the C array had
been edited directly and the .bin/.bmp left behind.
"""
import sys, re, argparse

GLYPHS, GW, GH = 256, 5, 10
GBYTES         = (GW * GH + 7) // 8      # 7
CELL, COLS     = 16, 16
DIM            = CELL * COLS             # 256

def carray_read(path):
    s = open(path, encoding='utf-8', errors='surrogateescape').read()
    i = s.find('bitmap_bin[%d] = {' % (GLYPHS * GBYTES))
    if i < 0:
        raise SystemExit('%s: no bitmap_bin[] found' % path)
    body = re.sub(r'/\*.*?\*/', '', s[i:s.find('};', i)], flags=re.S)
    v = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', body))
    if len(v) != GLYPHS * GBYTES:
        raise SystemExit('%s: got %d bytes, expected %d'
                         % (path, len(v), GLYPHS * GBYTES))
    return v

def carray_write(data):
    out = ['const unsigned char bitmap_bin[%d] = {' % len(data)]
    for g in range(GLYPHS):
        row = data[g * GBYTES:(g + 1) * GBYTES]
        out.append('   ' + ','.join('0x%02x' % b for b in row)
                   + ', /* code=0x%02X */' % g)
    out.append('};')
    return '\n'.join(out) + '\n'

def get_px(data, g, x, y):
    p = x + y * GW
    return (data[g * GBYTES + (p >> 3)] >> (p & 7)) & 1

def set_px(buf, g, x, y):
    p = x + y * GW
    buf[g * GBYTES + (p >> 3)] |= 1 << (p & 7)

def bmp_write(data):
    stride = (DIM * 4 + 31) // 32 * 4          # 128
    pix    = bytearray(stride * DIM)
    for g in range(GLYPHS):
        ox, oy = (g % COLS) * CELL, (g // COLS) * CELL
        for y in range(GH):
            for x in range(GW):
                if not get_px(data, g, x, y):
                    continue
                px, py = ox + x, oy + y
                row    = DIM - 1 - py           # BMP rows are bottom-up
                idx    = row * stride + (px >> 1)
                pix[idx] |= 0x10 if (px & 1) == 0 else 0x01
    hdr  = bytearray()
    hdr += b'BM'
    hdr += (62 + len(pix)).to_bytes(4, 'little') + b'\0\0\0\0'
    hdr += (62).to_bytes(4, 'little')
    hdr += (40).to_bytes(4, 'little')
    hdr += DIM.to_bytes(4, 'little') + DIM.to_bytes(4, 'little')
    hdr += (1).to_bytes(2, 'little') + (4).to_bytes(2, 'little')
    hdr += (0).to_bytes(4, 'little') + len(pix).to_bytes(4, 'little')
    hdr += (0).to_bytes(4, 'little') * 2
    hdr += (2).to_bytes(4, 'little') + (2).to_bytes(4, 'little')
    hdr += bytes((255, 0, 0, 0)) + bytes((255, 255, 255, 0))
    return bytes(hdr) + bytes(pix)

def bmp_read(path):
    m = open(path, 'rb').read()
    if m[:2] != b'BM':
        raise SystemExit('%s: not a BMP' % path)
    off = int.from_bytes(m[10:14], 'little')
    w   = int.from_bytes(m[18:22], 'little')
    h   = int.from_bytes(m[22:26], 'little')
    bpp = int.from_bytes(m[28:30], 'little')
    if (w, h, bpp) != (DIM, DIM, 4):
        raise SystemExit('%s: expected %dx%d 4bpp, got %dx%d %dbpp'
                         % (path, DIM, DIM, w, h, bpp))
    stride = (w * 4 + 31) // 32 * 4
    out    = bytearray(GLYPHS * GBYTES)
    for g in range(GLYPHS):
        ox, oy = (g % COLS) * CELL, (g // COLS) * CELL
        for y in range(GH):
            for x in range(GW):
                px, py = ox + x, oy + y
                b = m[off + (h - 1 - py) * stride + (px >> 1)]
                v = (b >> 4) if (px & 1) == 0 else (b & 0xF)
                if v:
                    set_px(out, g, x, y)
    return bytes(out)

def main():
    ap  = argparse.ArgumentParser(add_help=False)
    ap.add_argument('mode', choices=('export', 'import', 'check'))
    ap.add_argument('files', nargs='*')
    ap.add_argument('--bin'); ap.add_argument('--bmp'); ap.add_argument('--carray')
    a = ap.parse_args()

    if a.mode == 'export':
        data = carray_read(a.files[0])
        if a.bin: open(a.bin, 'wb').write(data);      print('wrote', a.bin)
        if a.bmp: open(a.bmp, 'wb').write(bmp_write(data)); print('wrote', a.bmp)
    elif a.mode == 'import':
        data = bmp_read(a.files[0])
        if a.bin:    open(a.bin, 'wb').write(data);   print('wrote', a.bin)
        if a.carray: open(a.carray, 'w').write(carray_write(data)); print('wrote', a.carray)
    else:
        c, b, m = (carray_read(a.files[0]),
                   open(a.files[1], 'rb').read(),
                   bmp_read(a.files[2]))
        ok = True
        if c != b:
            n = sum(1 for i in range(len(c)) if c[i] != b[i])
            print('MISMATCH: C array vs .bin - %d bytes differ' % n); ok = False
        if c != m:
            n = sum(1 for i in range(len(c)) if c[i] != m[i])
            print('MISMATCH: C array vs .bmp - %d bytes differ' % n); ok = False
        print('all three agree' if ok else 'sources disagree')
        return 0 if ok else 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
