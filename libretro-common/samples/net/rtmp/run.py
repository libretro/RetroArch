#!/usr/bin/env python3
"""Decode transmitter fixtures independently using RTMP chunk grammar."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='rtmp-chunk-') as temp:
        output = Path(temp) / 'chunks.bin'
        subprocess.run(shlex.split(args.runner) +
                       [str(Path(args.binary).resolve()), str(output)], check=True, timeout=30)
        data = output.read_bytes()
    pos = 0

    def take(n):
        nonlocal pos
        assert n <= len(data) - pos, 'truncated chunk'
        result = data[pos:pos + n]
        pos += n
        return result

    def basic():
        value = take(1)[0]
        ident = value & 63
        if ident == 0:
            ident = 64 + take(1)[0]
        elif ident == 1:
            ident = 64 + int.from_bytes(take(2), 'little')
        return value >> 6, ident

    ids = (2, 63, 64, 319, 320, 65599)
    times = (0, 0xfffffe, 0xffffff, 0x1000000, 0xffffffff, 42)
    sizes = (0, 1, 127, 128, 129, 700)
    for ident, timestamp, size in zip(ids, times, sizes):
        assert basic() == (0, ident)
        header = take(11)
        stamp = int.from_bytes(header[:3], 'big')
        assert stamp == min(timestamp, 0xffffff)
        assert int.from_bytes(header[3:6], 'big') == size
        assert header[6] == 9 and int.from_bytes(header[7:], 'little') == 0x12345678
        if stamp == 0xffffff:
            assert int.from_bytes(take(4), 'big') == timestamp
        payload = bytearray()
        while len(payload) < size:
            if payload:
                assert basic() == (3, ident)
                if stamp == 0xffffff:
                    assert int.from_bytes(take(4), 'big') == timestamp
            payload.extend(take(min(128, size - len(payload))))
        assert payload == bytes((i * 37 + 11) & 255 for i in range(size))
    assert pos == len(data), 'trailing data'
    print('PASS: independent RTMP chunk grammar and payload oracle')


if __name__ == '__main__':
    main()
