#!/usr/bin/env python3
"""Independently parse FLV/AMF0 color metadata and source-unit conversion."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


class Amf:
    def __init__(self, data):
        self.data, self.at = data, 0

    def read(self, count):
        assert count <= len(self.data) - self.at
        result = self.data[self.at:self.at + count]
        self.at += count
        return result

    def name(self):
        return self.read(int.from_bytes(self.read(2), 'big')).decode('utf-8')

    def value(self):
        kind = self.read(1)[0]
        if kind == 0:
            return struct.unpack('>d', self.read(8))[0]
        if kind == 2:
            return self.name()
        assert kind == 3, kind
        result = {}
        while self.data[self.at:self.at + 3] != b'\0\0\x09':
            name = self.name()
            assert name not in result
            result[name] = self.value()
        self.read(3)
        return result


def check(binary, runner, root, mode):
    path = root / f'color-{mode}.flv'
    subprocess.run(runner + [binary, str(path), str(mode)], check=True, timeout=20)
    data = path.read_bytes()
    assert data[:5] == b'FLV\x01\x05' and data[5:13] == b'\0\0\0\x09\0\0\0\0'
    at, tags = 13, []
    while at < len(data):
        header = data[at:at + 11]
        size = int.from_bytes(header[1:4], 'big')
        assert len(header) == 11 and header[4:11] == bytes(7)
        payload = data[at + 11:at + 11 + size]
        assert len(payload) == size
        assert int.from_bytes(data[at + 11 + size:at + 15 + size], 'big') == size + 11
        tags.append((header[0], payload))
        at += size + 15
    assert at == len(data) and [kind for kind, _ in tags] == [9, 9, 8]
    metadata, sequence, audio = [payload for _, payload in tags]
    assert metadata[:5] == b'\xd4hvc1'
    assert sequence[:5] == b'\x90hvc1' and audio == b'\xaf\0\x12\x10'
    reader = Amf(metadata[5:])
    assert reader.value() == 'colorInfo'
    color = reader.value()
    assert reader.at == len(reader.data)
    depth = 8 if mode == 1 else 12 if mode == 2 else 10
    expected = {'colorConfig': {'bitDepth': depth}}
    if mode != 1:
        expected['colorConfig'].update(colorPrimaries=9,
                                       transferCharacteristics=18 if mode == 2 else 16,
                                       matrixCoefficients=9)
    if mode in (0, 3):
        expected['hdrCll'] = {'maxCLL': 10000 if mode == 3 else 1000,
                              'maxFall': 10000 if mode == 3 else 400}
        expected['hdrMdcv'] = dict(redX=.68, redY=.32, greenX=.265, greenY=.69,
                                  blueX=.15, blueY=.06, whitePointX=.3127, whitePointY=.329,
                                  maxLuminance=10000 if mode == 3 else 1000,
                                  minLuminance=.0001 if mode == 3 else .005)
    assert color == expected, (mode, color, expected)
    print(f'PASS independent AMF0 colorInfo: mode={mode}, depth={depth}, '
          'source units, omission, ordering and binary64 values')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='flv-color-') as temp:
        for mode in range(4):
            check(str(Path(args.binary).resolve()), shlex.split(args.runner), Path(temp), mode)
