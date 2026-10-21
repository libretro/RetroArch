#!/usr/bin/env python3
"""Native Metal->fixed queue->VideoToolbox worker encode pixel/timing gate."""
import array
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('Native recording pipeline acceptance requires Apple frameworks')
here = Path(__file__).resolve().parent
root = here.parents[2]
ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
if not ffmpeg or not ffprobe:
    raise SystemExit('Independent native pipeline checks require test-only ffmpeg/ffprobe')
compiler = shlex.split(os.environ.get('CC', 'cc'))
flags = ['-DHAVE_THREADS', '-O2', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
         '-Wno-long-long', '-Wno-c99-extensions', '-Wframe-larger-than=2048',
         '-mmacosx-version-min=10.14', '-I' + str(root / 'libretro-common/include')]
subprocess.run([sys.executable, str(root / 'record/shaders/generate_metal.py'), '--check'], check=True)
unavailable = 0
with tempfile.TemporaryDirectory(prefix='metal-vt-pipeline-') as temp:
    directory = Path(temp)
    objects = []
    for source in ('record/record_gpu_worker_vt.c', 'record/record_gpu_queue.c',
                   'libretro-common/queues/retro_spsc.c'):
        obj = directory / (Path(source).stem + '.o')
        subprocess.run(compiler + flags + ['-c', str(root / source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    for ownership in ('-fobjc-arc', '-fno-objc-arc'):
        binary = directory / ('pipeline-arc' if ownership == '-fobjc-arc' else 'pipeline-mrc')
        command = compiler + flags + ['-fblocks', ownership, '-Wno-unused-function',
            str(here / 'pipeline_test.m'), str(root / 'record/record_gpu_capture_metal.m'),
            *objects, '-o', str(binary)]
        for framework in ('Foundation', 'Metal', 'CoreVideo', 'CoreMedia', 'VideoToolbox', 'CoreFoundation'):
            command += ['-framework', framework]
        print('+', shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        for hevc, depth, mode in ((0, 8, 0), (1, 8, 0), (1, 10, 0), (1, 10, 1), (1, 10, 2)):
            coded, reference = directory / 'coded.bin', directory / 'reference.yuv'
            result = subprocess.run([str(binary), str(hevc), str(depth), str(mode),
                                     str(coded), str(reference)], timeout=60)
            if result.returncode == 77:
                unavailable += 1
                print(f'SKIP: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}', flush=True)
                continue
            result.check_returncode()
            info = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams',
                                                      '-of', 'json', str(coded)]))['streams'][0]
            assert (info['width'], info['height']) == (162, 130), info
            assert info['codec_name'] == ('hevc' if hevc else 'h264'), info
            assert info['pix_fmt'] == ('yuv420p10le' if depth == 10 else 'yuv420p'), info
            assert info['color_range'] == 'tv', info
            assert info['color_primaries'] == ('bt2020' if mode else 'bt709'), info
            assert info['color_transfer'] == ('smpte2084' if mode else 'bt709'), info
            assert info['color_space'] == ('bt2020nc' if mode else 'bt709'), info
            pixels = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(coded),
                                               '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-'])
            actual, expected = array.array('H'), array.array('H')
            actual.frombytes(pixels); expected.frombytes(reference.read_bytes())
            assert len(actual) == len(expected) == 24 * 162 * 130 * 3 // 2
            errors = sorted(abs(a-b) for a, b in zip(actual, expected))
            mean, peak, p99 = sum(errors) / len(errors), errors[-1], errors[int(.99 * len(errors))]
            assert mean < 4 and peak <= 64 and p99 <= 16, (mean, peak, p99)
            print(f'PASS: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}: '
                  f'24 frames, mean={mean:.3f}, peak={peak}, p99={p99}', flush=True)
if unavailable:
    raise SystemExit(77)
