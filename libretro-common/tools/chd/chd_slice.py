#!/usr/bin/env python3
"""Extracts a small, self-contained slice of a CHD: the header, the
hunk map, the metadata chain, and the first few hunk blobs.

Enough to establish a codec's framing without moving the whole image.
The output is not a valid CHD -- it is a container of the pieces, with
an index, meant to be handed to someone working on the format.

    chd_slice.py <image.chd> <out.slice> [hunks]
"""
import struct, sys, json

def rd(f, off, n):
    f.seek(off); return f.read(n)

def main():
    if len(sys.argv) < 3:
        print(__doc__); return 2
    src, dst = sys.argv[1], sys.argv[2]
    nhunks = int(sys.argv[3]) if len(sys.argv) > 3 else 8

    f = open(src, 'rb')
    hdr = rd(f, 0, 124)
    if hdr[:8] != b'MComprHD':
        print("not a CHD"); return 1
    ver = struct.unpack('>I', hdr[12:16])[0]
    if ver != 5:
        print("this helper only handles version 5"); return 1

    codecs  = [hdr[16+i*4:20+i*4].decode('latin1') for i in range(4)]
    logical, mapoff, metaoff = struct.unpack('>QQQ', hdr[32:56])
    hunkb, unitb = struct.unpack('>II', hdr[56:64])
    count = (logical + hunkb - 1) // hunkb

    parts = [('header', 0, 124)]

    # map: header states the body length
    mh = rd(f, mapoff, 16)
    maplen = struct.unpack('>I', mh[:4])[0]
    if all(c == '\0\0\0\0' for c in codecs):
        parts.append(('map', mapoff, count * 4))
    else:
        parts.append(('map', mapoff, 16 + maplen))

    # metadata chain
    off, n = metaoff, 0
    while off and n < 256:
        e = rd(f, off, 16)
        ln = int.from_bytes(e[5:8], 'big')
        parts.append(('meta%d' % n, off, 16 + ln))
        off = int.from_bytes(e[8:16], 'big'); n += 1

    # the first hunks' blobs, straight after the map's datastart
    datastart = int.from_bytes(mh[4:10], 'big')
    parts.append(('hunkdata', datastart, min(hunkb * nhunks * 2,
                                             64 * 1024 * 1024)))

    index, blob, at = [], bytearray(), 0
    for name, off, ln in parts:
        f.seek(off); d = f.read(ln)
        index.append({'name': name, 'offset': off, 'length': len(d),
                      'at': at})
        blob += d; at += len(d)

    meta = {'source': src, 'version': ver,
            'codecs': [c for c in codecs if c != '\0\0\0\0'],
            'logical_bytes': logical, 'hunk_bytes': hunkb,
            'unit_bytes': unitb, 'hunk_count': count,
            'map_offset': mapoff, 'meta_offset': metaoff,
            'parts': index}
    j = json.dumps(meta, indent=1).encode()

    with open(dst, 'wb') as o:
        o.write(b'CHDSLICE')
        o.write(struct.pack('>I', len(j)))
        o.write(j)
        o.write(blob)

    print("wrote %s: %.1f MiB  (source is %.1f GiB)"
          % (dst, (len(j) + len(blob)) / 1048576.0,
             logical / 1073741824.0))
    print("  codecs: %s" % ','.join(meta['codecs']))
    print("  %d hunks of %d bytes, unit %d" % (count, hunkb, unitb))
    f.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())
