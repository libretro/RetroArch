#!/usr/bin/env python3
"""Builds a Matroska file holding a FLAC track, from a header and a run
of raw FLAC frames.

Written because the container arms of the FLAC decoder could otherwise
only be built, not run: nothing in the tree produces a .mka, and testing
against one recorded elsewhere is not reproducible.  A file assembled
here is, and it found a real defect the synthetic native-stream tests
could not reach."""
import struct, sys

def vint(n, length=None):
    if length is None:
        length = 1
        while n >= (1 << (7*length)) - 1:
            length += 1
    out = bytearray(length); v = n | (1 << (7*length))
    for i in range(length-1, -1, -1):
        out[i] = v & 0xFF; v >>= 8
    return bytes(out)

def uint(n):
    if n == 0: return b'\x00'
    b = b''
    while n: b = bytes([n & 0xFF]) + b; n >>= 8
    return b

def el(eid, payload): return eid + vint(len(payload)) + payload
def f64(x): return struct.pack('>d', x)

def build(header, frames, rate=44100, channels=2, block=2352):
    ebml = el(b'\x1A\x45\xDF\xA3',
          el(b'\x42\x86', uint(1)) + el(b'\x42\xF7', uint(1))
        + el(b'\x42\xF2', uint(4)) + el(b'\x42\xF3', uint(8))
        + el(b'\x42\x82', b'matroska') + el(b'\x42\x87', uint(2))
        + el(b'\x42\x85', uint(2)))
    info = el(b'\x15\x49\xA9\x66',
          el(b'\x2A\xD7\xB1', uint(1000000))
        + el(b'\x44\x89', f64(len(frames)*float(block)*1000.0/rate)))
    audio = el(b'\xE1', el(b'\xB5', f64(float(rate))) + el(b'\x9F', uint(channels)))
    track = el(b'\xAE',
          el(b'\xD7', uint(1)) + el(b'\x73\xC5', uint(1))
        + el(b'\x83', uint(2)) + el(b'\x86', b'A_FLAC')
        + el(b'\x63\xA2', header) + audio)
    tracks = el(b'\x16\x54\xAE\x6B', track)
    blocks = b''
    for i, p in enumerate(frames):
        ts = int(i * block * 1000.0 / rate)
        blocks += el(b'\xA3', vint(1) + struct.pack('>h', ts) + b'\x80' + p)
    cluster = el(b'\x1F\x43\xB6\x75', el(b'\xE7', uint(0)) + blocks)
    return ebml + el(b'\x18\x53\x80\x67', info + tracks + cluster)

def split_frames(blob):
    """Frame starts, found by sync code and spacing."""
    syncs = [i for i in range(len(blob)-1)
             if blob[i] == 0xFF and (blob[i+1] & 0xFC) == 0xF8]
    starts, prev = [], None
    for s in syncs:
        if prev is None or s - prev > 1000:
            starts.append(s); prev = s
    b = starts + [len(blob)]
    return [blob[b[i]:b[i+1]] for i in range(len(starts))]

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("usage: make_mka.py <native.flac> <raw-frames.bin> <out.mka>")
        raise SystemExit(2)
    hdr = open(sys.argv[1], 'rb').read()[:42]
    frames = split_frames(open(sys.argv[2], 'rb').read())
    open(sys.argv[3], 'wb').write(build(hdr, frames))
    print("wrote %s: %d frames" % (sys.argv[3], len(frames)))
