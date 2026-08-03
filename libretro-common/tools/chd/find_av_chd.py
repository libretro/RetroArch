#!/usr/bin/env python3
"""Scans a tree of CHD files and reports which codecs they use, marking
any that use an audio/video codec.

Written to find an image using the pre-version-5 A/V codec, which no
longer exists in circulation for the obvious reason: a set rebuilt at
any point in the last decade has been recompressed to version 5, so what
decides the codec is the age of the *file*, not of the game or of the
emulator that reads it.

    find_av_chd.py <directory> [...]
"""
import os, struct, sys

V15_ENUM = {0: 'none', 1: 'zlib', 2: 'zlib+', 3: 'A/V (avcomp)'}

def probe(path):
    try:
        with open(path, 'rb') as f:
            h = f.read(124)
    except OSError:
        return None
    if len(h) < 32 or h[:8] != b'MComprHD':
        return None
    ver = struct.unpack('>I', h[12:16])[0]
    if ver >= 5:
        tags = [h[16 + i * 4:20 + i * 4] for i in range(4)]
        names = [t.decode('latin1') for t in tags if t != b'\0\0\0\0']
        return ver, ','.join(names), 'avhu' in names
    if ver < 1 or ver > 4:
        return ver, '?', False
    enum = struct.unpack('>I', h[20:24])[0]
    return ver, V15_ENUM.get(enum, 'enum %d' % enum), enum == 3

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    found = 0
    for root in sys.argv[1:]:
        for dirpath, _, files in os.walk(root):
            for fn in files:
                if not fn.lower().endswith('.chd'):
                    continue
                p = os.path.join(dirpath, fn)
                r = probe(p)
                if r is None:
                    print("  %-52s not a CHD" % fn[:52]); continue
                ver, codecs, is_av = r
                print("  %-52s v%-2d %-14s%s"
                      % (fn[:52], ver, codecs, "  <-- A/V" if is_av else ""))
                if is_av and ver < 5:
                    found += 1
    print("\n%d image(s) using the pre-version-5 A/V codec" % found)
    return 0

if __name__ == '__main__':
    sys.exit(main())
