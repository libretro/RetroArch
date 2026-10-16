#!/usr/bin/env python3
"""Native GPU-input encode, independently decoded with FFmpeg."""
from pathlib import Path
import argparse
import array
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--runner', default='')
parser.add_argument('binary')
args = parser.parse_args()
binary = str(Path(args.binary).resolve())
runner = shlex.split(args.runner)
width, height, frames = 320, 192, 24
passed = 0

def check_color(ffmpeg, stream, color, full):
    probe = subprocess.run([ffmpeg, '-hide_banner', '-i', str(stream),
                            '-frames:v', '1', '-f', 'null', '-'],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    source = probe.stderr.decode(errors='replace').split('Output #', 1)[0]
    tag = {1: 'bt709', 2: 'bt2020nc/bt2020/smpte2084',
           3: 'bt2020nc/bt2020/arib-std-b67'}[color]
    assert ('pc' if full else 'tv') + ', ' + tag in source, source

with tempfile.TemporaryDirectory(prefix='nvenc-d3d12-') as temporary:
    for codec in range(3):
        for fmt in (1, 2, 3):
            if codec == 0 and fmt == 3:
                continue
            name = ('h264', 'hevc', 'obu')[codec]
            stream = Path(temporary) / f'{codec}-{fmt}.{name}'
            result = subprocess.run([*runner, binary, str(codec), str(fmt), str(stream)], timeout=60)
            if result.returncode == 77:
                if os.environ.get('NVENC_TEST_REQUIRE_HW'):
                    raise SystemExit('Hardware encode was required but skipped')
                continue
            if result.returncode:
                raise SystemExit(result.returncode)
            ffmpeg = shutil.which('ffmpeg')
            if not ffmpeg:
                raise SystemExit('Native hardware validation needs ffmpeg')
            check_color(ffmpeg, stream, 2 if fmt == 3 else 1, False)
            raw_format = ('', 'bgra', 'nv12', 'p010le')[fmt]
            decode = subprocess.run([ffmpeg, '-hide_banner', '-f', name, '-i', str(stream),
                                     '-vf', 'showinfo', '-f', 'rawvideo', '-pix_fmt', raw_format, '-'],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
            source = decode.stderr.decode(errors='replace').split('Output #', 1)[0]
            assert f'{width}x{height}' in source, source
            if fmt == 3:
                assert 'yuv420p10le' in source, source
            raw = decode.stdout
            samples = width * height * (4 if fmt == 1 else 3) // (1 if fmt == 1 else 2)
            assert len(raw) == samples * frames * (2 if fmt == 3 else 1)
            states = {int(n): int(key) for n, key in re.findall(r'\bn:\s*(\d+).*?\biskey:\s*(\d+)', decode.stderr.decode(errors='replace'))}
            assert len(states) == frames and states[0] == states[12] == 1, states
            error = total = square = worst = 0
            for frame in range(frames):
                slot = frame % 4
                values = raw[frame * samples * (2 if fmt == 3 else 1):(frame + 1) * samples * (2 if fmt == 3 else 1)]
                if fmt == 3:
                    values = array.array('H', values)
                    if sys.byteorder != 'little':
                        values.byteswap()
                for index, actual in enumerate(values):
                    if fmt == 1:
                        pixel, channel = divmod(index, 4)
                        if channel == 3:
                            continue
                        y, x = divmod(pixel, width)
                        expected = (24 + slot * 48, 32 + (x // 40) * 24, 32 + (y // 24) * 24)[channel]
                    else:
                        chroma = index >= width * height
                        expected = (144 if index % 2 else 112) if chroma else 32 + slot * 40 + (index % width // 40) * 3
                        if fmt == 3:
                            expected = expected * 4 + (0 if chroma else 1)
                            actual >>= 6
                    difference = abs(actual - expected)
                    error += difference
                    square += difference * difference
                    worst = max(worst, difference)
                    total += 1
            mean = error / total
            psnr = math.inf if not square else 10 * math.log10((1023 if fmt == 3 else 255)**2 / (square / total))
            assert mean < (4 if fmt != 3 else 8) and psnr > 32, (codec, fmt, mean, psnr, worst)
            print(f'[pass] {name} {raw_format}: {frames} independently decoded frames, MAE {mean:.3f}, PSNR {psnr:.2f} dB', flush=True)
            passed += 1
    if passed:
        for codec, fmt, color, full in ((0, 2, 1, 1), (1, 2, 1, 1), (2, 2, 1, 1),
                                       (1, 3, 3, 0), (2, 3, 3, 0),
                                       (1, 3, 2, 1), (2, 3, 2, 1)):
            name = ('h264', 'hevc', 'obu')[codec]
            stream = Path(temporary) / f'color-{codec}-{color}-{full}.{name}'
            result = subprocess.run([*runner, binary, str(codec), str(fmt), str(stream),
                                     str(color), str(full)], timeout=60)
            if result.returncode == 77 and not os.environ.get('NVENC_TEST_REQUIRE_HW'):
                continue
            if result.returncode:
                raise SystemExit(result.returncode)
            check_color(ffmpeg, stream, color, bool(full))
            print(f'[pass] {name}: color {color}, full range {full}', flush=True)
print(f'{passed} native codec/format cases plus color signaling passed; unsupported runtimes explicitly skipped')
