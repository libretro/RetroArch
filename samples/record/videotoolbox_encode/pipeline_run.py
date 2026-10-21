#!/usr/bin/env python3
"""Native Metal->fixed queue->VideoToolbox worker encode pixel/timing gate."""
import contextlib
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
failures = []
artifact_dir = os.environ.get('PIPELINE_ARTIFACT_DIR')
if artifact_dir:
    Path(artifact_dir).mkdir(parents=True, exist_ok=True)
workspace = (contextlib.nullcontext(artifact_dir) if artifact_dir else
             tempfile.TemporaryDirectory(prefix='metal-vt-pipeline-'))
with workspace as temp:
    directory = Path(temp)
    objects = []
    for source in ('record/record_gpu_worker_vt.c', 'record/record_gpu_queue.c',
                   'libretro-common/queues/retro_spsc.c', 'libretro-common/formats/h265/rh265_sps_crop.c'):
        obj = directory / (Path(source).stem + '.o')
        subprocess.run(compiler + flags + ['-c', str(root / source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    for ownership, width, height in ((a, w, h) for w, h in ((162, 130), (192, 160))
                                     for a in ('-fobjc-arc', '-fno-objc-arc')):
        binary = directory / (('pipeline-arc' if ownership == '-fobjc-arc' else 'pipeline-mrc')
                              + f'-{width}x{height}')
        command = compiler + flags + [f'-DWIDTH={width}', f'-DHEIGHT={height}', '-fblocks', ownership, '-Wno-unused-function',
            str(here / 'pipeline_test.m'), str(root / 'record/record_gpu_capture_metal.m'),
            *objects, '-o', str(binary)]
        for framework in ('Foundation', 'Metal', 'CoreVideo', 'CoreMedia', 'VideoToolbox', 'CoreFoundation'):
            command += ['-framework', framework]
        print('+', shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        for hevc, depth, mode, control, dynamic, aperture in (
                (0, 8, 0, False, False, False), (1, 8, 0, False, False, False),
                (1, 10, 0, False, False, False), (1, 10, 1, False, False, False), (1, 10, 2, False, False, False),
                (1, 10, 0, True, False, False), (1, 10, 1, True, False, False),
                (1, 10, 0, False, True, False), (1, 10, 1, False, True, False), (1, 10, 2, False, True, False),
                (1, 10, 1, False, False, True), (1, 10, 1, False, True, True)):
            if width != 162 and (depth != 10 or mode == 2 or control or dynamic or aperture):
                continue
            case = directory / f'{binary.name}-hevc{hevc}-depth{depth}-mode{mode}-cpu{int(control)}-dynamic{int(dynamic)}-aperture{int(aperture)}'
            case.mkdir()
            coded, reference = case / 'coded.bin', case / 'reference.yuv'
            result = subprocess.run([str(binary), str(hevc), str(depth), str(mode),
                                     str(coded), str(reference)], timeout=60,
                                     env=dict(os.environ, **({'PIPELINE_CPU_CONTROL': '1'} if control else {}),
                                              **({'PIPELINE_DYNAMIC_CONTROL': '1'} if dynamic else {}),
                                              **({'PIPELINE_APERTURE_CONTROL': '1'} if aperture else {})))
            if result.returncode == 77:
                unavailable += 1
                print(f'SKIP: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}', flush=True)
                continue
            result.check_returncode()
            info = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams',
                                                      '-of', 'json', str(coded)]))['streams'][0]
            (case / 'probe.json').write_text(json.dumps(info, indent=2))
            assert (info['width'], info['height']) == (width, height), info
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
            assert len(actual) == len(expected) == 24 * width * height * 3 // 2
            errors = sorted(abs(a-b) for a, b in zip(actual, expected))
            mean, peak, p99 = sum(errors) / len(errors), errors[-1], errors[int(.99 * len(errors))]
            (case / 'decoded.yuv').write_bytes(pixels)
            report = dict(mean=mean, peak=peak, p99=p99, stream=info, planes=[])
            frame_size, luma_size = width * height * 3 // 2, width * height
            for plane, offset, plane_width, plane_height in (('Y', 0, width, height),
                    ('U', luma_size, width // 2, height // 2), ('V', luma_size * 5 // 4, width // 2, height // 2)):
                worst = []
                for frame in range(24):
                    start = frame * frame_size + offset
                    index = max(range(plane_width * plane_height),
                                key=lambda i: abs(actual[start+i]-expected[start+i]))
                    a, b = actual[start+index], expected[start+index]
                    worst.append(dict(frame=frame, x=index % plane_width, y=index // plane_width,
                                      actual=a, expected=b, error=abs(a-b)))
                report['planes'].append(dict(plane=plane, worst=worst))
            (case / 'errors.json').write_text(json.dumps(report, indent=2) + '\n')
            if not (mean < 4 and peak <= 64 and p99 <= 16):
                failures.append(str(case))
                print(f'FAIL: {case.name}: {json.dumps(report)}', flush=True)
                continue
            print(f'PASS: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}: '
                  f'24 frames, mean={mean:.3f}, peak={peak}, p99={p99}', flush=True)
if failures:
    raise SystemExit('Native pipeline pixel failures: ' + ', '.join(failures))
if unavailable:
    raise SystemExit(77)
