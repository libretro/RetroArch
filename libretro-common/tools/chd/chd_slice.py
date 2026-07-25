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
    hdrlen, ver = struct.unpack('>II', hdr[8:16])
    if ver < 1 or ver > 5:
        print("unknown CHD version %d" % ver); return 1

    parts = [('header', 0, max(hdrlen, 124))]
    metaoff = 0

    if ver == 5:
        codecs  = [hdr[16+i*4:20+i*4].decode('latin1') for i in range(4)]
        logical, mapoff, metaoff = struct.unpack('>QQQ', hdr[32:56])
        hunkb, unitb = struct.unpack('>II', hdr[56:64])
        count = (logical + hunkb - 1) // hunkb

        mh = rd(f, mapoff, 16)
        maplen = struct.unpack('>I', mh[:4])[0]
        if all(c == '\0\0\0\0' for c in codecs):
            parts.append(('map', mapoff, count * 4))
            datastart = hunkb          # a raw map indexes by hunk
        else:
            parts.append(('map', mapoff, 16 + maplen))
            datastart = int.from_bytes(mh[4:10], 'big')
    else:
        # Before version 5 one integer names the codec, the map follows
        # the header, and its entries are fixed width. There is no
        # datastart: the first hunk sits after the map, whose end is the
        # end-of-list entry.
        enum = struct.unpack('>I', hdr[20:24])[0]
        codecs = [{0: 'none', 1: 'zlib', 2: 'zlib+',
                   3: 'A/V (avcomp)'}.get(enum, 'enum %d' % enum)]
        count = struct.unpack('>I', hdr[24:28])[0]
        if ver <= 2:
            # Versions 1 and 2 record the hunk in sectors and derive the
            # image size from the drive geometry.
            seclen  = struct.unpack('>I', hdr[76:80])[0] if ver == 2 else 512
            hunkb   = struct.unpack('>I', hdr[24:28])[0] * seclen
            count   = struct.unpack('>I', hdr[28:32])[0]
            cyl, heads, sectors = struct.unpack('>III', hdr[32:44])
            logical = cyl * heads * sectors * seclen
            unitb   = seclen
            esz     = 8
        else:
            logical = struct.unpack('>Q', hdr[28:36])[0]
            metaoff = struct.unpack('>Q', hdr[36:44])[0]
            hunkb   = struct.unpack('>I', hdr[44:48])[0] if ver == 4 \
                      else struct.unpack('>I', hdr[76:80])[0]
            unitb   = hunkb
            esz     = 16
        mapoff = hdrlen
        parts.append(('map', mapoff, (count + 1) * esz))
        # the first hunk begins wherever the map's first entry points
        e0 = rd(f, mapoff, esz)
        if esz == 8:
            datastart = int.from_bytes(e0, 'big') & ((1 << 44) - 1)
        else:
            datastart = int.from_bytes(e0[:8], 'big')

    # metadata chain, where the version has one
    off, n = metaoff, 0
    while off and n < 256:
        e = rd(f, off, 16)
        ln = int.from_bytes(e[5:8], 'big')
        parts.append(('meta%d' % n, off, 16 + ln))
        off = int.from_bytes(e[8:16], 'big'); n += 1

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
