#!/usr/bin/env python3
"""Cropped GPU HDR conversion and independently decoded native encoder output."""
import argparse
import array
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--runner', default='')
parser.add_argument('binary')
args = parser.parse_args()
binary = str(Path(args.binary).resolve())
runner = shlex.split(args.runner)
width, height, frames = 162, 130, 12
with tempfile.TemporaryDirectory(prefix='hdr-capture-') as directory:
    for path_mode in range(5):
        for scrgb in (0, 1):
            for full in (0, 1):
                for codec, suffix in ((1, 'hevc'), (2, 'obu')):
                    output = Path(directory) / f'{path_mode}-{scrgb}-{full}.{suffix}'
                    result = subprocess.run([*runner, binary, str(scrgb), str(full), str(codec), str(output), str(path_mode)])
                    if result.returncode == 77 and not os.environ.get('NVENC_TEST_REQUIRE_HW'):
                        raise SystemExit(0)
                    result.check_returncode()
                    ffmpeg = shutil.which('ffmpeg')
                    if not ffmpeg:
                        raise SystemExit('Native HDR oracle requires ffmpeg (test-only)')
                    expected = array.array('H')
                    expected.frombytes(Path(str(output) + '.raw').read_bytes())
                    expected = array.array('H', (v >> 6 for v in expected))
                    uv = expected[width * height:]
                    expected = expected[:width * height] + uv[::2] + uv[1::2]
                    decoded = subprocess.run([ffmpeg, '-hide_banner', '-i', str(output),
                                              '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-'],
                                             capture_output=True, check=True)
                    actual = array.array('H')
                    actual.frombytes(decoded.stdout)
                    assert len(actual) == len(expected) * frames
                    errors = sorted(abs(a - b) for a, b in zip(actual, expected * frames))
                    mean, peak, p99 = sum(errors) / len(errors), errors[-1], errors[int(.99 * len(errors))]
                    assert mean < 2 and peak <= 32 and p99 <= 8, (mean, peak, p99)
                    log = decoded.stderr.decode(errors='replace').split('Output #', 1)[0]
                    assert ('pc' if full else 'tv') + ', bt2020nc/bt2020/smpte2084' in log, log
                    print(f'[pass] HDR capture path={path_mode} scrgb={scrgb} full={full} codec={suffix}: '
                          f'{frames} decoded frames, mean={mean:.3f}, peak={peak}, p99={p99}', flush=True)
