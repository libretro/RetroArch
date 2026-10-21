#!/usr/bin/env python3
"""Native VT hardware encode oracle; CPU fixtures and FFmpeg are test-only."""
import argparse
import array
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--native', action='store_true')
parser.add_argument('--metal', action='store_true')
args = parser.parse_args()
if not args.native or sys.platform != 'darwin':
    raise SystemExit('Native encode acceptance requires --native on Apple')
here = Path(__file__).resolve().parent
root = here.parents[2]
ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
if not ffmpeg or not ffprobe:
    raise SystemExit('Independent native encode checks require test-only ffmpeg and ffprobe')
with tempfile.TemporaryDirectory(prefix='vt-encode-') as temp:
    directory = Path(temp)
    binary = directory / 'encode'
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-DHAVE_THREADS', '-O2', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
        '-Wno-long-long', '-Wno-c99-extensions', '-Wframe-larger-than=2048',
        '-mmacosx-version-min=10.14', '-I' + str(root / 'libretro-common/include'),
        str(here / 'native_test.c'), '-o', str(binary)]
    for framework in ('CoreMedia', 'CoreVideo', 'VideoToolbox', 'CoreFoundation'):
        command += ['-framework', framework]
    print('+', shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    passed, unavailable = 0, 0
    for hevc, depth, hdr in ((0, 8, 0), (1, 8, 0), (1, 10, 0), (1, 10, 1)):
        coded, raw = directory / 'coded.bin', directory / 'reference.yuv'
        result = subprocess.run([str(binary), str(hevc), str(depth), str(hdr), str(coded), str(raw)])
        if result.returncode == 77:
            unavailable += 1
            print(f'SKIP: required hardware codec={hevc} depth={depth} hdr={hdr}', flush=True)
            continue
        result.check_returncode()
        info = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams',
                                                  '-of', 'json', str(coded)]))['streams'][0]
        assert info['codec_name'] == ('hevc' if hevc else 'h264'), info
        assert info['pix_fmt'] == ('yuv420p10le' if depth == 10 else 'yuv420p'), info
        assert info['color_range'] == 'tv', info
        assert info['color_primaries'] == ('bt2020' if hdr else 'bt709'), info
        assert info['color_transfer'] == ('smpte2084' if hdr else 'bt709'), info
        assert info['color_space'] == ('bt2020nc' if hdr else 'bt709'), info
        decoded = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(coded),
                                           '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-'])
        expected, actual = array.array('H'), array.array('H')
        expected.frombytes(raw.read_bytes()); actual.frombytes(decoded)
        assert len(actual) == len(expected) == 12 * 64 * 48 * 3 // 2
        errors = sorted(abs(a-b) for a, b in zip(actual, expected))
        mean, peak = sum(errors) / len(errors), errors[-1]
        assert mean < 2 and peak <= 32, (hevc, depth, hdr, mean, peak)
        passed += 1
        print(f'PASS: native hardware hevc={hevc} depth={depth} hdr={hdr}, '
              f'12 frames, mean={mean:.4f}, peak={peak}', flush=True)
    print(f'Native VT encode: {passed} passed, {unavailable} unavailable', flush=True)
    if args.metal:
        result = subprocess.run([sys.executable, str(here / 'capture_run.py')])
        if result.returncode == 77:
            unavailable += 1
        else:
            result.check_returncode()
    if unavailable:
        raise SystemExit(77)
