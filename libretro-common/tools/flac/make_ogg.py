#!/usr/bin/env python3
"""Builds an Ogg FLAC stream (RFC 5334) from a native header and raw frames."""
import struct, sys

def crc32_ogg(data):
    crc = 0
    for b in data:
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc

def page(serial, seq, htype, granule, packets):
    """One page carrying whole packets, laced per the Ogg rules."""
    seg = bytearray(); body = b''
    for p in packets:
        n = len(p)
        while n >= 255:
            seg.append(255); n -= 255
        seg.append(n)
        body += p
    hdr = bytearray(b'OggS' + bytes([0, htype]))
    hdr += struct.pack('<q', granule)
    hdr += struct.pack('<I', serial) + struct.pack('<I', seq)
    hdr += b'\x00\x00\x00\x00'
    hdr += bytes([len(seg)]) + bytes(seg)
    full = bytes(hdr) + body
    crc = crc32_ogg(full)
    return full[:22] + struct.pack('<I', crc) + full[26:]

def build(header, frames, serial=0x12345678, block=2352):
    # First packet: the FLAC-in-Ogg mapping header, then the native header.
    first = b'\x7FFLAC' + bytes([1, 0]) + struct.pack('>H', 1) + header
    out = page(serial, 0, 0x02, 0, [first])
    seq = 1; granule = 0
    for f in frames:
        granule += block
        out += page(serial, seq, 0, granule, [f]); seq += 1
    return out

if __name__ == '__main__':
    sys.path.insert(0, '.')
    from make_mka import split_frames
    hdr = open(sys.argv[1], 'rb').read()[:42]
    frames = split_frames(open(sys.argv[2], 'rb').read())
    open(sys.argv[3], 'wb').write(build(hdr, frames))
    print("wrote %s: %d frames" % (sys.argv[3], len(frames)))
