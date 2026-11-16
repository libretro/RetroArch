#!/usr/bin/env python3
"""Native Vulkan resource checks, with optional independent picture validation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native', action='store_true')
parser.add_argument('--binary', type=Path, help='reuse a previously built native fixture')
parser.add_argument('--capture', choices=['direct', 'buffer'], help='GPU RGB clear through the shipping conversion shader')
parser.add_argument('--pictures', action='store_true', help='submit test-only uploaded NV12 pictures and independently decode')
parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
parser.add_argument('--ffprobe', default=shutil.which('ffprobe'))
parser.add_argument('--artifacts', type=Path, required=True)
args = parser.parse_args()
if not args.native:
    parser.error('--native is required; a real device and validation layer are mandatory')
if args.capture:
    args.pictures = True
if args.pictures and (not args.ffmpeg or not args.ffprobe):
    parser.error('--pictures requires independent FFmpeg and FFprobe executables')
here = Path(__file__).resolve().parent
repo = here.parents[2]
root = args.artifacts.resolve()
root.mkdir(parents=True, exist_ok=True)
evidence = dict(scope=__doc__, result='running', commands=[], submitted_pictures=0,
                validation_layer='VK_LAYER_KHRONOS_validation required', synchronization_validation=True)
if args.pictures:
    evidence['scope'] = 'Native cold resources and 12 H.264 pictures from test-only CPU upload; GPU capture not exercised'
if args.capture:
    evidence['scope'] = 'Native GPU RGB clear, shipping RGB-to-NV12 shader and H264 encoder; independent decoded-plane oracle'
evidence['source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
evidence['dirty'] = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=repo))

def save():
    (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')

def run(command, timeout):
    entry = dict(arguments=list(map(str, command)))
    evidence['commands'].append(entry)
    save()
    number = len(evidence['commands'])
    try:
        result = subprocess.run(entry['arguments'], cwd=repo, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        (root / f'{number}.stdout').write_bytes(error.stdout or b'')
        (root / f'{number}.stderr').write_bytes(error.stderr or b'')
        evidence['result'] = entry['result'] = 'TIMEOUT'
        save()
        raise
    (root / f'{number}.stdout').write_bytes(result.stdout)
    (root / f'{number}.stderr').write_bytes(result.stderr)
    entry['exit_code'] = result.returncode
    save()
    return result

binary = args.binary.resolve() if args.binary else root / ('native.exe' if os.name == 'nt' else 'native')
evidence['binary_supplied'] = args.binary is not None
if not args.binary:
    compiler = shlex.split(os.environ.get('CC', 'gcc' if os.name == 'nt' else 'cc'))
    flags = ['-O1', '-g1', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
             '-Wno-long-long', '-Wframe-larger-than=2048', '-DHAVE_DYLIB', '-DRSLC_EXTENDED', '-DRSLC_NO_DXBC',
             '-I' + str(repo / 'libretro-common/include'), '-I' + str(repo / 'gfx/include')]
    if 'clang' in subprocess.check_output(compiler + ['--version'], text=True).lower():
        flags += ['-Wno-c99-extensions']
    sources = ['samples/gfx/vulkan_encode_h264/native_test.c', 'gfx/common/vulkan_encode_h264.c',
               'gfx/common/vulkan_encode_h264_session.c', 'gfx/common/vulkan_encode_h264_submit.c',
               'gfx/common/vulkan_encode_pool.c', 'gfx/common/vulkan_encode_capture.c',
               'gfx/common/vulkan_encode_capture_shader.c', 'libretro-common/gfx/shaders/rslc_front.c',
               'libretro-common/gfx/shaders/rslc_ir.c', 'libretro-common/gfx/shaders/rslc_emit.c',
               'libretro-common/dynamic/dylib.c',
               'libretro-common/encodings/encoding_utf.c', 'libretro-common/compat/compat_strl.c']
    result = run(compiler + flags + [str(repo / name) for name in sources] + ['-o', str(binary)]
                 + (['-lm'] if os.name == 'nt' else ['-ldl', '-lm']), 120)
    if result.returncode:
        evidence['result'] = 'FAIL compile'
        save()
        result.check_returncode()
evidence['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
coded = root / 'output.h264'
mode = '--capture-' + args.capture if args.capture else '--pictures'
result = run([binary] + ([mode, coded] if args.pictures else []), 30)
output = result.stdout.decode(errors='replace')
print(output, end='', flush=True)
evidence['result'] = 'UNAVAILABLE' if result.returncode == 77 else 'VERIFYING' if result.returncode == 0 else 'FAIL'
if result.returncode == 0:
    if output.count('PASS native cold H264') != 3 or 'validation_errors=0' not in output or 'validation_sync=1' not in output:
        evidence['result'] = 'FAIL output contract'
        save()
        raise AssertionError(output)
if result.returncode == 0 and args.pictures:
    try:
        dimensions = re.search(r'extent=(\d+)x(\d+)', output)
        assert dimensions and 'PASS native coded pictures=12' in output
        if args.capture:
            assert 'PASS native two in-flight pictures, full-ring BUSY, borrowed cancellation and drain' in output
            evidence['submitted_pictures_including_cancellation'] = 14
            evidence['maximum_in_flight'] = 2
            evidence['cancelled_output_pictures'] = 2
        width, height = map(int, dimensions.groups())
        probe = run([args.ffprobe, '-v', 'error', '-show_streams', '-show_frames',
                     '-of', 'json', coded], 30)
        probe.check_returncode()
        info = json.loads(probe.stdout)
        streams, frames = info['streams'], info['frames']
        assert len(streams) == 1 and len(frames) == 12
        stream = streams[0]
        assert stream['codec_name'] == 'h264' and stream['pix_fmt'] == 'yuv420p'
        assert stream['width'] == width and stream['height'] == height
        assert stream['color_range'] == 'tv' and stream['color_space'] == 'bt709'
        assert stream['color_transfer'] == 'bt709' and stream['color_primaries'] == 'bt709'
        trace = run([args.ffmpeg, '-v', 'trace', '-i', coded, '-c', 'copy',
                     '-bsf:v', 'trace_headers', '-f', 'null', '-'], 30)
        trace.check_returncode()
        timing = {}
        for name, expected in (('num_units_in_tick', 1), ('time_scale', 60),
                ('frame_mbs_only_flag', 1), ('timing_info_present_flag', 1),
                ('fixed_frame_rate_flag', 1)):
            values = [int(value) for value in re.findall(
                r'\b' + name + r'\s+[01]+\s*=\s*(\d+)', trace.stderr.decode(errors='replace'))]
            assert values and all(value == expected for value in values), (name, values)
            timing[name] = expected
        assert [frame['pict_type'] for frame in frames] == ['I' if i % 4 == 0 else 'P' for i in range(12)]
        keys = [i for i, frame in enumerate(frames) if frame['key_frame']]
        assert keys == [0, 4, 8]
        decoded = run([args.ffmpeg, '-v', 'error', '-hwaccel', 'none', '-threads', '1',
                       '-i', coded, '-map', '0:v:0', '-fps_mode', 'passthrough',
                       '-pix_fmt', 'yuv420p', '-f', 'rawvideo', '-'], 30)
        decoded.check_returncode()
        luma = width * height
        size = luma * 3 // 2
        assert len(decoded.stdout) == 12 * size
        planes = []
        references = []
        for frame in range(12):
            if args.capture:
                rgb = [(32 + frame * 12 if frame % 3 == channel else 16) / 255
                       for channel in range(3)]
                kr, kb = .2126, .0722
                y = kr * rgb[0] + (1 - kr - kb) * rgb[1] + kb * rgb[2]
                references.append([round(16 + 219 * y),
                    round(128 + 224 * (rgb[2] - y) / (2 * (1 - kb))),
                    round(128 + 224 * (rgb[0] - y) / (2 * (1 - kr)))])
            else:
                references.append([32 + frame * 12, 128, 128])
        for frame in range(12):
            for name, offset, count, reference in (('Y', 0, luma, references[frame][0]),
                    ('U', luma, luma // 4, references[frame][1]),
                    ('V', luma * 5 // 4, luma // 4, references[frame][2])):
                start = frame * size + offset
                errors = [abs(value - reference) for value in decoded.stdout[start:start + count]]
                planes.append(dict(frame=frame, plane=name, peak=max(errors),
                                   mean=sum(errors) / count))
        evidence['pictures'] = dict(frames=12, keyframes=keys, width=width, height=height,
            source='GPU cleared RGB primary colors' if args.capture else 'Test-only NV12 upload',
            capture=args.capture, references=references,
            planes=planes, timing=timing, raw_probe_rate=stream['r_frame_rate'], decoded_sha256=hashlib.sha256(decoded.stdout).hexdigest())
        save()
        limits = (4, 2) if args.capture else (2, .5)
        evidence['pictures']['limits'] = dict(peak=limits[0], mean=limits[1])
        assert all(p['peak'] <= limits[0] and p['mean'] <= limits[1] for p in planes)
        evidence['submitted_pictures'] = 12
    except Exception:
        evidence['result'] = 'FAIL picture oracle'
        save()
        raise
if result.returncode == 0:
    evidence['result'] = 'PASS'
save()
raise SystemExit(result.returncode)
