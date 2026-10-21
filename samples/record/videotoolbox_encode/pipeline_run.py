#!/usr/bin/env python3
"""Native Metal->fixed queue->VideoToolbox worker encode pixel/timing gate."""
import contextlib
import array
import argparse
import json
import os
import math
from pathlib import Path
import shlex
import shutil
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--mux', action='store_true')
parser.add_argument('--session', action='store_true')
parser.add_argument('--cold', action='store_true')
parser.add_argument('--dispatch', action='store_true')
args = parser.parse_args()
if args.dispatch:
    args.cold = True
    os.environ['SESSION_DISPATCH_CONTROL'] = '1'
if args.cold:
    args.session = True
    os.environ['SESSION_COLD_STARTUP_CONTROL'] = '1'
if args.session:
    args.mux = True

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
    for source in ('record/record_gpu_worker_vt.c', 'record/record_gpu_queue.c', 'record/record_audio_queue.c', 'record/record_avfoundation_worker.c', 'record/record_avfoundation_session.c',
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
            str(root / 'record/record_mux_avfoundation.m'), str(root / 'record/record_gpu_session_metal.m'), str(root / 'record/record_gpu_dispatch_metal.m'),
            *objects, '-o', str(binary)]
        if args.session:
            command += ['-DRECORD_AVF_SESSION_NATIVE_TEST']
        for framework in ('Foundation', 'Metal', 'CoreVideo', 'CoreMedia', 'VideoToolbox', 'CoreFoundation', 'AVFoundation', 'AudioToolbox'):
            command += ['-framework', framework]
        print('+', shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        if args.session and width == 162:
            for fault in (1, 2, 3, 4):
                fault_dir = directory / f'{binary.name}-fault{fault}'
                fault_dir.mkdir()
                result = subprocess.run([str(binary), '1', '10', '1', str(fault_dir / 'unused.bin'),
                    str(fault_dir / 'reference.yuv'), str(fault_dir / 'output.mp4'), '48000'],
                    timeout=60, env=dict(os.environ, SESSION_FAULT_CONTROL=str(fault)))
                if result.returncode == 77:
                    unavailable += 1
                    print(f'SKIP: native owned session fault={fault} {ownership}', flush=True)
                else:
                    result.check_returncode()
        for hevc, depth, mode, control, dynamic, aperture in (
                (0, 8, 0, False, False, False), (1, 8, 0, False, False, False),
                (1, 10, 0, False, False, False), (1, 10, 1, False, False, False), (1, 10, 2, False, False, False),
                (1, 10, 0, True, False, False), (1, 10, 1, True, False, False),
                (1, 10, 0, False, True, False), (1, 10, 1, False, True, False), (1, 10, 2, False, True, False),
                (1, 10, 1, False, False, True), (1, 10, 1, False, True, True)):
            if args.session and (control or aperture):
                continue
            if width != 162 and (depth != 10 or mode == 2 or control or dynamic or aperture):
                continue
            case = directory / f'{binary.name}-hevc{hevc}-depth{depth}-mode{mode}-cpu{int(control)}-dynamic{int(dynamic)}-aperture{int(aperture)}'
            case.mkdir()
            coded, reference = case / 'coded.bin', case / 'reference.yuv'
            movie = case / 'output.mp4'
            result = subprocess.run([str(binary), str(hevc), str(depth), str(mode),
                                     str(coded), str(reference),
                                     *([str(movie), '48000'] if args.mux else [])], timeout=60,
                                     env=dict(os.environ, **({'PIPELINE_CPU_CONTROL': '1'} if control else {}),
                                              **({'PIPELINE_DYNAMIC_CONTROL': '1'} if dynamic else {}),
                                              **({'PIPELINE_APERTURE_CONTROL': '1'} if aperture else {})))
            if result.returncode == 77:
                unavailable += 1
                print(f'SKIP: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}', flush=True)
                continue
            result.check_returncode()
            if args.session:
                coded.write_bytes(subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie),
                    '-map', '0:v:0', '-c:v', 'copy', '-bsf:v',
                    'hevc_mp4toannexb' if hevc else 'h264_mp4toannexb', '-f', 'hevc' if hevc else 'h264', '-']))
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
            else:
                print(f'PASS: native pipeline {ownership} hevc={hevc} depth={depth} mode={mode}: '
                  f'24 frames, mean={mean:.3f}, peak={peak}, p99={p99}', flush=True)
            if args.mux:
                container = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams',
                    '-show_packets', '-of', 'json', str(movie)]))
                tracks = {s['codec_type']: s for s in container['streams']}
                assert len(container['streams']) == 2 and set(tracks) == {'video', 'audio'}, container
                video, audio = tracks['video'], tracks['audio']
                for key in ('codec_name', 'pix_fmt', 'color_range', 'color_primaries', 'color_transfer', 'color_space'):
                    assert video[key] == info[key], (key, video, info)
                assert video['time_base'] == '1/60000', video
                packets = [p for p in container['packets'] if p['stream_index'] == video['index']]
                assert [int(p['pts']) for p in packets] == [i * 1001 for i in range(24)], packets
                assert [int(p['dts']) for p in packets] == [i * 1001 for i in range(24)], packets
                assert all(int(p['duration']) == 1001 for p in packets), packets
                raw_packets = json.loads(subprocess.check_output([ffprobe, '-v', 'error',
                    '-show_packets', '-of', 'json', str(coded)]))['packets']
                raw_keys = ['K' in p.get('flags', '') for p in raw_packets]
                movie_keys = ['K' in p.get('flags', '') for p in packets]
                assert len(raw_keys) == 24 and movie_keys == raw_keys, (raw_keys, movie_keys)
                assert movie_keys[0] and sum(movie_keys) >= 2, movie_keys
                movie_pixels = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie), '-map', '0:v:0',
                    '-pix_fmt', 'yuv420p10le', '-fps_mode', 'passthrough', '-f', 'rawvideo', '-'])
                assert movie_pixels == pixels, 'MP4 passthrough changed decoded video'
                # Seek via the container index, not a linear decode from frame zero.
                # Compare the complete independently decoded keyframe to its exact
                # source-clock frame in the sequential output.
                frame_bytes = width * height * 3
                for index, keyframe in enumerate(movie_keys):
                    if not keyframe:
                        continue
                    seek_pixels = subprocess.check_output([ffmpeg, '-v', 'error', '-ss',
                        f'{index * 1001 / 60000:.9f}', '-i', str(movie), '-map', '0:v:0',
                        '-frames:v', '1', '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-'])
                    assert seek_pixels == pixels[index * frame_bytes:(index + 1) * frame_bytes], (
                        'MP4 indexed seek changed frame', index, len(seek_pixels))
                annexb = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie), '-map', '0:v:0',
                    '-c:v', 'copy', '-bsf:v', 'hevc_mp4toannexb' if hevc else 'h264_mp4toannexb',
                    '-f', 'hevc' if hevc else 'h264', '-'])
                def vcl(data):
                    nals = [n for n in (p.rstrip(b'\0') for p in re.split(b'\x00\x00\x00\x01|\x00\x00\x01', data)) if n]
                    return [n for n in nals if ((n[0] >> 1) & 63) <= 31] if hevc else [n for n in nals if 1 <= (n[0] & 31) <= 5]
                assert vcl(annexb) == vcl(coded.read_bytes()) and vcl(annexb), 'VCL bitstream changed'
                assert audio['codec_name'] == 'aac' and int(audio['sample_rate']) == 48000 and audio['channels'] == 2, audio
                pcm = array.array('f')
                pcm.frombytes(subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie), '-map', '0:a:0', '-f', 'f32le', '-']))
                target = math.ceil(24 * 1001 * 48000 / 60000)
                assert target - 1 <= len(pcm) // 2 <= target + 1024, (len(pcm), target)
                assert abs(float(audio['duration']) - target / 48000) <= 2 / 48000, audio
                for channel, frequency in enumerate((440, 660)):
                    values = pcm[2048 * 2:min(len(pcm), target * 2)][channel::2]
                    assert len(values) > 2000
                    omega = 2 * math.pi * frequency / 48000
                    amplitude = 2 * abs(sum(v * complex(math.cos(omega * n), math.sin(omega * n))
                                            for n, v in enumerate(values))) / len(values)
                    assert abs(amplitude - .2) < .04, (channel, amplitude)
                print('PASS: owned session independent pixels, sync flags/indexed seeks, source video clock and AAC stereo tones/duration' if args.session else
                      'PASS: MP4 unchanged VCL/pixels, sync flags/indexed seeks, source video clock and AAC stereo tones/duration', flush=True)
if failures:
    raise SystemExit('Native pipeline pixel failures: ' + ', '.join(failures))
if unavailable:
    raise SystemExit(77)
