#!/usr/bin/env python3
"""Builds a CD image carrying subchannel data, for testing the sector
and subchannel split of the cd* codecs.

Almost nothing in circulation carries subchannel: it is only present
when a disc was dumped by a drive able to read it, and the protection
that uses it -- LibCrypt, on PAL PlayStation titles -- is distributed as
a separate sidecar rather than inside the image.  Every commercial image
to hand reports SUBTYPE:NONE and a subchannel of nothing but zeros,
which leaves the half of each frame it occupies untested.

The sectors here are structured rather than taken from a real disc, for
a reason worth recording: real game data is already compressed, so an
image made from it stores every hunk whole and the codec under test
never runs.  Content that compresses is what makes the framing
observable.

    make_subchannel_cd.py <out-prefix> [sectors]

then, with chdman:

    chdman createcd -i <out-prefix>.toc -o <out>.chd -c cdlz
"""
import sys

def bcd(v):
    return ((v // 10) << 4) | (v % 10)

def build(n):
    img, sub = bytearray(), bytearray()
    for s in range(n):
        lba = s + 150
        sec = bytearray(2352)
        sec[0] = 0
        for i in range(1, 11):
            sec[i] = 0xFF
        sec[11] = 0
        sec[12] = bcd(lba // (60 * 75))
        sec[13] = bcd((lba // 75) % 60)
        sec[14] = bcd(lba % 75)
        sec[15] = 2                                    # Mode 2
        sec[16:24] = bytes([0, 0, 0x08, 0, 0, 0, 0x08, 0])   # form 1
        body = ("sector %06d " % s).encode() * 171
        sec[24:24 + 2048] = body[:2048]
        img += sec
        # Distinctive per sector so a misplaced byte is obvious, but
        # structured enough to compress.
        b = bytearray(96)
        b[0], b[1], b[2], b[3] = 0x41, (s >> 8) & 0xFF, s & 0xFF, 0x5A
        for k in range(4, 96):
            b[k] = (k * 3) & 0xFF
        sub += b
    return bytes(img), bytes(sub)

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    pre = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    img, sub = build(n)
    out = bytearray()
    for s in range(n):
        out += img[s * 2352:(s + 1) * 2352]
        out += sub[s * 96:(s + 1) * 96]
    open(pre + '_rw.img', 'wb').write(bytes(out))
    open(pre + '.sub', 'wb').write(sub)
    # cdrdao TOC.  RW_RAW means the data file holds 2448 bytes per
    # sector, the subchannel interleaved after each one, and the track
    # length has to be stated or the length reads as zero.
    open(pre + '.toc', 'w').write(
        'CD_ROM\n\nTRACK MODE2_RAW RW_RAW\nDATAFILE "%s_rw.img" %02d:%02d:%02d\n'
        % (pre.split('/')[-1], n // (60 * 75), (n // 75) % 60, n % 75))
    print("wrote %s.toc, %s_rw.img (%d sectors of 2448)" % (pre, pre, n))
    return 0

if __name__ == '__main__':
    sys.exit(main())
