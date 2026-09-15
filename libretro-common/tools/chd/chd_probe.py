#!/usr/bin/env python3
"""Regenerates the reference images FORMAT.md is derived from and
re-checks every claim marked [V] in it.

Each check is written as a prediction that either holds against the
observed bytes or does not; a failure means FORMAT.md is wrong, not that
the probe is.  Requires chdman on PATH."""

import hashlib, os, struct, subprocess, sys, tempfile

HUNK = 4096
HUNKS = 64

def make_source(path):
    """Mixed entropy soevery codec is actually exercised: runs, text,
    noise, and a ramp.  Hunk 0 is deliberately all zeros to exercise the
    uncompressed map's hole case."""
    buf = bytearray(); st = 0x2545F4914F6CDD1D
    def rnd():
        nonlocal st
        st ^= (st << 13) & 0xFFFFFFFFFFFFFFFF; st ^= st >> 7
        st ^= (st << 17) & 0xFFFFFFFFFFFFFFFF
        return st & 0xFF
    for h in range(HUNKS):
        if   h % 4 == 0: blk = bytes([h & 0xFF]) * HUNK
        elif h % 4 == 1: blk = (("hunk %04d " % h).encode() * 512)[:HUNK]
        elif h % 4 == 2: blk = bytes(rnd() for _ in range(HUNK))
        else:            blk = bytes((i * 7 + h) & 0xFF for i in range(HUNK))
        buf += blk
    open(path, 'wb').write(bytes(buf))
    return bytes(buf)

def build(src, out, comp):
    subprocess.run(['chdman', 'createraw', '-i', src, '-o', out,
                    '-hs', str(HUNK), '-us', str(HUNK), '-c', comp, '-f'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return open(out, 'rb').read()

def check(label, cond):
    print("  %-58s %s" % (label, "ok" if cond else "FAIL"))
    return bool(cond)

def main():
    ok = True
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, 'raw.bin')
        raw = make_source(src)
        raw_sha1 = hashlib.sha1(raw).hexdigest()

        print("header layout (FORMAT.md 1.1)")
        for comp in ['zlib', 'lzma', 'huff', 'flac']:
            d = build(src, os.path.join(td, comp + '.chd'), comp)
            ok &= check("%s: tag, length, version" % comp,
                        d[0:8] == b'MComprHD'
                        and struct.unpack('>II', d[8:16]) == (124, 5))
            ok &= check("%s: compressor slot 0 reads back" % comp,
                        d[16:20] == comp.encode())
            ok &= check("%s: logical/hunk/unit" % comp,
                        struct.unpack('>Q', d[32:40])[0] == HUNK * HUNKS
                        and struct.unpack('>II', d[56:64]) == (HUNK, HUNK))
            ok &= check("%s: rawsha1 at offset 64" % comp,
                        d[64:84].hex() == raw_sha1)
            ok &= check("%s: parentsha1 zero" % comp, d[104:124] == b'\0' * 20)

        d = build(src, os.path.join(td, 'multi.chd'), 'lzma,zlib,huff,flac')
        ok &= check("four codecs read back in request order",
                    [d[16 + i * 4:20 + i * 4] for i in range(4)]
                    == [b'lzma', b'zlib', b'huff', b'flac'])

        print("uncompressed image (FORMAT.md 1.1, 2.1)")
        d = build(src, os.path.join(td, 'none.chd'), 'none')
        ok &= check("no hashes recorded", d[64:124] == b'\0' * 60)
        mo = struct.unpack('>Q', d[40:48])[0]
        ok &= check("mapoffset is 124", mo == 124)
        m = struct.unpack('>%dI' % HUNKS, d[mo:mo + 4 * HUNKS])
        ok &= check("map entry 0 is a hole, source hunk 0 is zeros",
                    m[0] == 0 and raw[:HUNK] == b'\0' * HUNK)
        good = all((b'\0' * HUNK if m[n] == 0
                    else d[m[n] * HUNK:(m[n] + 1) * HUNK])
                   == raw[n * HUNK:(n + 1) * HUNK] for n in range(HUNKS))
        ok &= check("hole rule resolves all %d hunks" % HUNKS, good)
        ok &= check("naive reading of entry 0 would differ",
                    d[:HUNK] != raw[:HUNK])

        print("compressed map header (FORMAT.md 2.2)")
        for comp in ['zlib', 'lzma', 'huff', 'flac']:
            d = build(src, os.path.join(td, comp + '.chd'), comp)
            mo = struct.unpack('>Q', d[40:48])[0]
            h = d[mo:mo + 16]
            maplen = struct.unpack('>I', h[0:4])[0]
            ok &= check("%s: datastart is 124" % comp,
                        int.from_bytes(h[4:10], 'big') == 124)
            ok &= check("%s: map is last, ends at EOF" % comp,
                        mo + 16 + maplen == len(d))
            ok &= check("%s: no self or parent refs, reserved zero" % comp,
                        h[13] == 0 and h[14] == 0 and h[15] == 0)
            ok &= check("%s: lengthbits bounds hunkbytes" % comp,
                        (1 << h[12]) > HUNK and (1 << (h[12] - 1)) <= HUNK)

    print("PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
