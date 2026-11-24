#!/usr/bin/env python3
"""Independent AAC decode and shared-stream RTMP sample clocks."""
import argparse
import array
import io
import math
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here.parents[2] / 'libretro-common/samples/net/rtmp'))
from peer import replies


def decode(ffmpeg, path, expected, rate, channels):
    result = subprocess.run([ffmpeg, '-v', 'error', '-i', str(path),
                             '-f', 's16le', '-acodec', 'pcm_s16le', '-'],
                            capture_output=True, check=True, timeout=30)
    actual = array.array('h')
    actual.frombytes(result.stdout)
    if sys.byteorder != 'little':
        actual.byteswap()
    frames = len(expected) // channels
    assert len(actual) // channels == (frames + 1023) // 1024 * 1024
    error = sum((int(a) - b) ** 2 for a, b in zip(expected, actual))
    energy = sum(int(v) ** 2 for v in expected)
    snr = 10 * math.log10(energy / error)
    assert snr > 15, (path, snr)
    print(f'PASS independent builtin AAC decode {rate} Hz {channels} channels: '
          f'{frames} source frames, bounded final padding, SNR={snr:.2f} dB', flush=True)


def adts(payload, rate_index, channels):
    length = len(payload) + 7
    assert 7 < length < 8192
    return bytes((255, 241, 64 | rate_index << 2 | channels >> 2,
                  channels << 6 | length >> 11, length >> 3 & 255,
                  (length << 5 & 255) | 31, 252)) + payload


p = argparse.ArgumentParser()
p.add_argument('owner')
p.add_argument('stream')
p.add_argument('--runner', default='')
a = p.parse_args()
ffmpeg = shutil.which('ffmpeg')
if not ffmpeg:
    raise SystemExit('Independent builtin AAC oracle requires ffmpeg (test only)')
runner = shlex.split(a.runner)
with tempfile.TemporaryDirectory(prefix='builtin-aac-', dir=Path.cwd()) as temp:
    directory = Path(temp).relative_to(Path.cwd())
    subprocess.run(runner + [str(Path(a.owner).resolve()), str(directory)], check=True, timeout=30)
    for rate in (44100, 48000):
        for channels in (1, 2):
            source = directory / f'aac-{rate}-{channels}.aac'
            expected = array.array('h')
            expected.frombytes(source.with_suffix('.s16').read_bytes())
            decode(ffmpeg, source, expected, rate, channels)
    wire = directory / 'stream.bin'
    subprocess.run(runner + [str(Path(a.stream).resolve()), str(wire)], check=True, timeout=30)
    counts = {8: 0, 9: 0}
    def stop(kind, payload):
        if kind in counts and len(payload) > 1 and payload[1] == 1:
            counts[kind] += 1
        return counts == {8: 9, 9: 12}
    records, _ = replies(io.BytesIO(wire.read_bytes()).read, stop=stop)
    offset = 0
    stamps = []
    audio = bytearray()
    have_config = False
    while offset < len(records):
        channel, stream, kind, timestamp, length = struct.unpack_from('>IIIII', records, offset)
        offset += 20
        payload = records[offset:offset + length]
        offset += length
        if kind == 8 and len(payload) > 1:
            if payload[1] == 0:
                assert payload[2:] == bytes((18, 16)), payload
                have_config = True
            elif payload[1] == 1:
                assert have_config
                stamps.append(timestamp)
                audio.extend(adts(payload[2:], 4, 2))
    assert stamps == [i * 1024 * 1000 // 44100 for i in range(9)], stamps
    source = directory / 'wire.aac'
    source.write_bytes(audio)
    expected = array.array('h')
    for frame in range(8365):
        for hz in (440, 880):
            expected.append(0 if frame < 500 else int(12000 * math.sin(2 * math.pi * hz * frame / 44100)))
    decode(ffmpeg, source, expected, 44100, 2)
    print('PASS independent RTMP AAC clock: nine packets, source-aligned 0..185 ms, no priming packet')
