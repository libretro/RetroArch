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

def check_mp4(ffmpeg, stream, codec, count, suffix='.mp4'):
    mp4 = Path(str(stream) + suffix)
    outputs = []
    for path in (stream, mp4):
        result = subprocess.run([ffmpeg, '-hide_banner', '-i', str(path),
                                 '-vf', 'showinfo', '-pix_fmt', 'yuv420p10le',
                                 '-f', 'rawvideo', '-'], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, check=True)
        text = result.stderr.decode(errors='replace')
        records = re.findall(r'\bn:\s*(\d+).*?pts_time:\s*([\d.]+).*?\biskey:\s*(\d+)', text)
        assert len(records) == count, (path, len(records), count)
        if path == mp4:
            for i, (_, timestamp, _) in enumerate(records):
                assert abs(float(timestamp) - i / 60) < 0.00001, (path, i, timestamp)
        outputs.append((result.stdout, [record[2] for record in records]))
    assert outputs[0] == outputs[1], 'MP4 differs from elementary stream pixels/keyframes'
    print(f'[pass] MP4: {count} decoded frames, timestamps and keyframes match', flush=True)


def check_color(ffmpeg, stream, color, full):
    probe = subprocess.run([ffmpeg, '-hide_banner', '-i', str(stream),
                            '-frames:v', '1', '-f', 'null', '-'],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    source = probe.stderr.decode(errors='replace').split('Output #', 1)[0]
    tag = {1: 'bt709', 2: 'bt2020nc/bt2020/smpte2084',
           3: 'bt2020nc/bt2020/arib-std-b67'}[color]
    assert ('pc' if full else 'tv') + ', ' + tag in source, source


def check_metadata(ffmpeg, stream, codec, mode):
    name = ('h264', 'hevc', 'obu')[codec]
    input_format = [] if str(stream).endswith('.mp4') else ['-f', name]
    trace = subprocess.run([ffmpeg, '-hide_banner', *input_format, '-i', str(stream),
                            '-c:v', 'copy', '-bsf:v', 'trace_headers',
                            '-frames:v', '1', '-f', 'null', '-'],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    text = trace.stderr.decode(errors='replace')
    fields = {}
    for key, value in re.findall(r'\]\s+\d+\s+(\S+)\s+[01]+\s+=\s+(\d+)', text):
        fields.setdefault(key, []).append(int(value))
    av1 = codec == 2
    master = bool(mode & 1) or mode == 4
    light = bool(mode & 2) or mode == 4
    expected = {}
    xy = ([50000, 0, 0, 50000, 0, 0, 0, 0] if mode == 4 else
          [34000, 16000, 13250, 34500, 7500, 3000, 15635, 16450])
    if master:
        if av1:
            xy = [min(65535, (v * 65536 + 25000) // 50000) for v in xy]
            for i in range(3):
                expected[f'primary_chromaticity_x[{i}]'] = xy[i * 2]
                expected[f'primary_chromaticity_y[{i}]'] = xy[i * 2 + 1]
            expected.update(white_point_chromaticity_x=xy[6], white_point_chromaticity_y=xy[7],
                            luminance_max=2560000 if mode == 4 else 256000,
                            luminance_min=0 if mode == 4 else 82)
        else:
            for i, start in enumerate((2, 4, 0)):
                expected[f'display_primaries_x[{i}]'] = xy[start]
                expected[f'display_primaries_y[{i}]'] = xy[start + 1]
            expected.update(white_point_x=xy[6], white_point_y=xy[7],
                            max_display_mastering_luminance=100000000 if mode == 4 else 10000000,
                            min_display_mastering_luminance=0 if mode == 4 else 50)
    else:
        assert not any('chromaticity' in k or 'display_primaries' in k for k in fields), fields
    if light:
        expected['max_cll' if av1 else 'max_content_light_level'] = 65535 if mode == 4 else 1000
        expected['max_fall' if av1 else 'max_pic_average_light_level'] = 0 if mode == 4 else 400
    else:
        assert ('max_cll' if av1 else 'max_content_light_level') not in fields, fields
    for key, value in expected.items():
        assert key in fields and all(v == value for v in fields[key]), (key, value, fields)
    decoded = subprocess.run([ffmpeg, '-hide_banner', *input_format, '-i', str(stream),
                              '-vf', 'showinfo', '-f', 'null', '-'],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    frame_info = re.findall(r'\bn:\s*(\d+).*?\biskey:\s*(\d+)', decoded.stderr.decode(errors='replace'))
    count = (12 if str(stream).endswith('.gop') else 72 if str(stream).endswith('.key')
             else 84 if mode else frames)
    assert len(frame_info) == count, frame_info
    assert frame_info[0][1] == '1', frame_info

def check_pixels(ffmpeg, stream, codec, fmt):
    name = ('h264', 'hevc', 'obu')[codec]
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
    return raw


with tempfile.TemporaryDirectory(prefix='nvenc-d3d12-') as temporary:
    references = {}
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
            check_mp4(ffmpeg, stream, codec, frames)
            check_color(ffmpeg, stream, 2 if fmt == 3 else 1, False)
            if fmt == 3:
                check_metadata(ffmpeg, stream, codec, 0)
                check_metadata(ffmpeg, Path(str(stream) + '.mp4'), codec, 0)
            references[codec, fmt] = check_pixels(ffmpeg, stream, codec, fmt)
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
            check_mp4(ffmpeg, stream, codec, frames)
            check_color(ffmpeg, stream, color, bool(full))
            print(f'[pass] {name}: color {color}, full range {full}', flush=True)
        for codec in (1, 2):
            for color, mode in [(c, m) for c in (2, 3) for m in (1, 2, 3)] + [(2, 4)]:
                name = ('h264', 'hevc', 'obu')[codec]
                stream = Path(temporary) / f'metadata-{codec}-{color}-{mode}.{name}'
                result = subprocess.run([*runner, binary, str(codec), '3', str(stream),
                                         str(color), '0', str(mode)], timeout=60)
                if result.returncode == 77 and not os.environ.get('NVENC_TEST_REQUIRE_HW'):
                    continue
                if result.returncode:
                    raise SystemExit(result.returncode)
                check_mp4(ffmpeg, stream, codec, 84 if mode else frames)
                check_color(ffmpeg, stream, color, False)
                check_metadata(ffmpeg, stream, codec, mode)
                check_metadata(ffmpeg, Path(str(stream) + '.mp4'), codec, mode)
                check_metadata(ffmpeg, Path(str(stream) + '.key'), codec, mode)
                check_metadata(ffmpeg, Path(str(stream) + '.gop'), codec, mode)
                print(f'[pass] {name}: color {color}, HDR metadata {mode}, including requested and periodic keyframe restarts', flush=True)
    if passed:
        for codec in range(3):
            for fmt in (1, 2, 3):
                if codec == 0 and fmt == 3:
                    continue
                name = ('h264', 'hevc', 'obu')[codec]
                stream = Path(temporary) / f'capture-{codec}-{fmt}.{name}'
                color = 2 if fmt == 3 else 1
                result = subprocess.run([*runner, binary, str(codec), str(fmt), str(stream),
                                         str(color), '0', '0', '1'], timeout=60)
                if result.returncode:
                    raise SystemExit(result.returncode)
                check_color(ffmpeg, stream, color, False)
                check_mp4(ffmpeg, stream, codec, frames)
                assert check_pixels(ffmpeg, stream, codec, fmt) == references[codec, fmt], 'capture differs from direct input'
                print(f'[pass] {name}: GPU capture format {fmt}', flush=True)
        for codec in (1, 2):
            name = ('h264', 'hevc', 'obu')[codec]
            stream = Path(temporary) / f'capture-hdr-{codec}.{name}'
            result = subprocess.run([*runner, binary, str(codec), '3', str(stream),
                                     '2', '0', '3', '1'], timeout=60)
            if result.returncode:
                raise SystemExit(result.returncode)
            check_mp4(ffmpeg, stream, codec, 84)
            check_metadata(ffmpeg, Path(str(stream) + '.mp4'), codec, 3)
            for part in ('', '.key', '.gop'):
                check_metadata(ffmpeg, Path(str(stream) + part), codec, 3)
            print(f'[pass] {name}: GPU capture HDR metadata and keyframe restarts', flush=True)
        for codec in range(3):
            name = ('h264', 'hevc', 'obu')[codec]
            stream = Path(temporary) / f'capture-bgra-{codec}.{name}'
            result = subprocess.run([*runner, binary, str(codec), '1', str(stream),
                                     '1', '0', '0', '2'], timeout=60)
            if result.returncode:
                raise SystemExit(result.returncode)
            check_mp4(ffmpeg, stream, codec, frames)
            assert check_pixels(ffmpeg, stream, codec, 1) == references[codec, 1], 'BGRA capture differs from direct input'
            print(f'[pass] {name}: BGRA viewport copy', flush=True)
    if passed:
        for mode, variable, owned in (('GPU/PCM', 'NVENC_TEST_GPU_AV', False),
                                      ('dispatcher', 'NVENC_TEST_DISPATCH', False),
                                      ('owned dispatcher', 'NVENC_TEST_DISPATCH', True)):
            for codec, fmt, rate, channels in ((0, 1, 48000, 2), (1, 3, 44100, 1), (2, 3, 32000, 6)):
                name = ('h264', 'hevc', 'obu')[codec]
                stream = Path(temporary) / f'{variable}-{int(owned)}-{codec}.{name}'
                env = dict(os.environ, **{variable: '1'}, NVENC_TEST_AUDIO_RATE=str(rate),
                           NVENC_TEST_CAPTURE_OWNED=str(int(owned)))
                subprocess.run([*runner, binary, str(codec), str(fmt), str(stream),
                                '2' if fmt == 3 else '1', '0', '0', '1'], env=env, check=True, timeout=60)
                check_mp4(ffmpeg, stream, codec, frames, '.av.mp4')
                assert check_pixels(ffmpeg, stream, codec, fmt) == references[codec, fmt]
                audio = []
                for suffix in ('.av.mp4', '.av.ac3'):
                    result = subprocess.run([ffmpeg, '-hide_banner', '-i', str(stream) + suffix,
                                             '-map', '0:a:0', '-af', 'ashowinfo', '-f', 'f32le', '-'],
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                    records = re.findall(r'\bn:\s*(\d+).*?\bpts:\s*(\d+).*?nb_samples:\s*(\d+)',
                                         result.stderr.decode(errors='replace'))
                    count = math.ceil(frames * rate / 60 / 1536)
                    assert len(records) == count and all(int(n) == 1536 for _, _, n in records), (suffix, records)
                    if suffix == '.av.mp4':
                        assert [int(t) for _, t, _ in records] == [i * 1536 for i in range(count)], records
                    assert len(result.stdout) == count * 1536 * channels * 4
                    audio.append(result.stdout)
                assert audio[0] == audio[1], 'live PCM output differs from independent AC-3 encode'
                print(f'[pass] native {mode} pipeline: {rate} Hz/{channels} ch, padded tail, pixels and sample clock', flush=True)
print(f'{passed} native codec/format cases plus color signaling passed; unsupported runtimes explicitly skipped')
