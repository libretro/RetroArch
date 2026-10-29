#!/usr/bin/env python3
"""Independent compressed/interleaved RTMP receive fixtures."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


def basic(fmt, ident):
    if ident < 64:
        return bytes([(fmt << 6) | ident])
    if ident < 320:
        return bytes([fmt << 6, ident - 64])
    return bytes([(fmt << 6) | 1]) + (ident - 64).to_bytes(2, 'little')


def chunks(ident, stream, kind, stamp, payload, fmt=0, size=128):
    header = basic(fmt, ident)
    if fmt != 3:
        header += min(stamp, 0xffffff).to_bytes(3, 'big')
    if fmt <= 1:
        header += len(payload).to_bytes(3, 'big') + bytes([kind])
    if fmt == 0:
        header += stream.to_bytes(4, 'little')
    extended = stamp.to_bytes(4, 'big') if stamp >= 0xffffff else b''
    result = [header + extended + payload[:size]]
    for offset in range(size, len(payload), size):
        result.append(basic(3, ident) + extended + payload[offset:offset + size])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    expected = bytearray()
    wire = bytearray()

    def expect(ident, stream, kind, timestamp, payload):
        expected.extend(struct.pack('>IIIII', ident, stream, kind, timestamp, len(payload)))
        expected.extend(payload)

    video = bytes((i * 17 + 11) & 255 for i in range(300))
    first = chunks(3, 1, 9, 1000, video)
    control = struct.pack('>I', 65536)
    wire.extend(first[0])
    wire.extend(b''.join(chunks(2, 0, 5, 0, control)))
    expect(2, 0, 5, 0, control)
    wire.extend(b''.join(first[1:]))
    expect(3, 1, 9, 1000, video)
    cases = [(3, 1, 20, 10, 1010, 17, 1),
             (3, 1, 20, 20, 1030, 17, 2), (3, 1, 20, 20, 1050, 17, 3),
             (64, 9, 18, 0xfffffe, 0xfffffe, 129, 0),
             (64, 9, 18, 0xfffffe, 0x1fffffc, 129, 3),
             (319, 2, 20, 0xffffff, 0xffffff, 257, 0),
             (319, 2, 20, 0x1000000, 0x1ffffff, 385, 1),
             (319, 2, 20, 0x1000001, 0x3000000, 385, 2),
             (319, 2, 20, 0x1000001, 0x4000001, 385, 3),
             (65599, 4, 20, 0xfffffff0, 0xfffffff0, 2, 0),
             (65599, 4, 20, 32, 16, 2, 2), (65599, 4, 20, 32, 48, 2, 3),
             (320, 0, 4, 0, 0, 0, 0), (320, 0, 4, 0, 0, 0, 3)]
    for index, (ident, stream, kind, stamp, timestamp, length, fmt) in enumerate(cases):
        payload = bytes((i * 37 + index) & 255 for i in range(length))
        wire.extend(b''.join(chunks(ident, stream, kind, stamp, payload, fmt)))
        expect(ident, stream, kind, timestamp, payload)
    control = struct.pack('>I', 7)
    wire.extend(b''.join(chunks(2, 0, 1, 0, control)))
    expect(2, 0, 1, 0, control)
    payload = bytes((i * 19) & 255 for i in range(5000))
    wire.extend(b''.join(chunks(3, 7, 18, 2222, payload, size=7)))
    expect(3, 7, 18, 2222, payload)
    with tempfile.TemporaryDirectory(prefix='rtmp-receive-') as temp:
        source, output = Path(temp) / 'input.bin', Path(temp) / 'output.bin'
        source.write_bytes(wire)
        subprocess.run(shlex.split(args.runner) + [str(Path(args.binary).resolve()),
                       str(source), str(output)], check=True, timeout=30)
        assert output.read_bytes() == expected, 'reassembled message or timestamp differs'
    print('PASS: independent RTMP receive interleave, compression, extended time and rollover oracle')


if __name__ == '__main__':
    main()
