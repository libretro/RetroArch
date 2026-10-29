#!/usr/bin/env python3
"""Independent AMF0 decoding oracle for production publisher commands."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


def decode(data):
    pos = 0

    def take(n):
        nonlocal pos
        assert n <= len(data) - pos
        value = data[pos:pos + n]
        pos += n
        return value

    def text():
        return take(int.from_bytes(take(2), 'big')).decode('utf-8')

    def value(depth=0):
        assert depth < 8
        marker = take(1)[0]
        if marker == 0:
            return struct.unpack('>d', take(8))[0]
        if marker == 1:
            flag = take(1)[0]
            assert flag in (0, 1)
            return bool(flag)
        if marker == 2:
            return text()
        if marker == 5:
            return None
        assert marker == 3
        result = {}
        while data[pos:pos + 3] != b'\x00\x00\x09':
            name = text()
            assert name not in result
            result[name] = value(depth + 1)
        take(3)
        return result

    result = []
    while pos < len(data):
        result.append(value())
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    expected = [
        ['connect', 1, {'app': 'live', 'tcUrl': 'rtmp://127.0.0.1/live',
                        'flashVer': 'FMLE/3.0 (compatible; RetroArch)', 'fpad': False,
                        'objectEncoding': 0}],
        ['createStream', 0xffffffff, None],
        ['publish', 0, None, 'test/é', 'live'],
        ['deleteStream', 0, None, 0xffffffff],
        ['releaseStream', 0x80000000, None, 'test/é'],
        ['FCPublish', 0x1000001, None, 'test/é']]
    with tempfile.TemporaryDirectory(prefix='rtmp-amf-') as temp:
        output = Path(temp) / 'commands.bin'
        subprocess.run(shlex.split(args.runner) + [str(Path(args.binary).resolve()), str(output)],
                       check=True, timeout=30)
        data = output.read_bytes()
    pos = 0
    for command in expected:
        size = int.from_bytes(data[pos:pos + 2], 'big')
        pos += 2
        assert decode(data[pos:pos + size]) == command
        pos += size
    assert pos == len(data)
    print('PASS: independent AMF0 publisher command and exact integer-number oracle')


if __name__ == '__main__':
    main()
