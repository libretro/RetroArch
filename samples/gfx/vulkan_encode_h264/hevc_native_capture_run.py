#!/usr/bin/env python3
"""Decode native GPU capture and compare BT.709 samples, including padding."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def capture_pq(nits):
    power = (min(max(nits, 0), 10000) / 10000) ** (2610 / 16384)
    return ((3424 / 4096 + 2413 / 128 * power) /
            (1 + 2392 / 128 * power)) ** (2523 / 32)


def verify(path, depth, full, hdr_mode=0):
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-count_frames', '-show_streams',
        '-show_frames', '-of', 'json', str(path)], timeout=30))
    stream = probe['streams'][0]
    assert stream['codec_name'] == 'hevc'
    assert (stream['width'], stream['height']) == (192, 128)
    assert int(stream['nb_read_frames']) == 24 and not stream['has_b_frames']
    assert stream['profile'] == ('Main' if depth == 8 else 'Main 10')
    assert stream['color_range'] == ('pc' if full else 'tv')
    colors = ('bt2020nc', 'smpte2084', 'bt2020') if hdr_mode else ('bt709',) * 3
    for key, color in zip(('color_space', 'color_transfer', 'color_primaries'), colors):
        assert stream[key] == color, (key, stream[key])
    assert stream['r_frame_rate'] == '30/1'
    assert [i for i, frame in enumerate(probe['frames'])
            if frame['key_frame']] == [0, 7, 15, 23]
    raw = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-xerror', '-err_detect', 'explode',
        '-threads', '1', '-i', str(path), '-pix_fmt',
        'yuv420p' if depth == 8 else 'yuv420p10le', '-f', 'rawvideo', '-'], timeout=30)
    samples = raw if depth == 8 else struct.unpack('<%dH' % (len(raw) // 2), raw)
    stride = 192 * 128 * 3 // 2
    assert len(samples) == stride * 24
    scale = 1 << (depth - 8)
    maximum = (1 << depth) - 1
    yoffset = 0 if full else 16 * scale
    yscale = maximum if full else 219 * scale
    cscale = maximum if full else 224 * scale
    center = 1 << (depth - 1)
    errors = []
    for frame in range(24):
        rgb = [16 / 255] * 3
        rgb[frame % 3] = (32 + frame * 6) / 255
        if hdr_mode:
            peak_nits = (0, .01, 1, 16, 200, 1000, 4000, 12000)[frame // 3]
            rgb = [capture_pq(peak_nits / 16)] * 3
            rgb[frame % 3] = capture_pq(peak_nits)
            if hdr_mode == 1:
                rgb = [round(value * 1023) / 1023 for value in rgb]
        coefficients = (.2627, .6780, .0593) if hdr_mode else (.2126, .7152, .0722)
        luma = sum(a * b for a, b in zip(rgb, coefficients))
        start = frame * stride
        if hdr_mode:
            interior = abs(samples[start + 64 * 192 + 64] - round(yoffset + yscale * luma))
            assert interior <= 3, ('interior Y', frame, interior)
        for y in range(128):
            for x in range(192):
                expected = round(yoffset + yscale * (luma if x < 185 and y < 123 else 0))
                errors.append(abs(samples[start + y * 192 + x] - expected))
        cb, cr = (1.8814, 1.4746) if hdr_mode else (1.8556, 1.5748)
        chroma = ((rgb[2] - luma) / cb, (rgb[0] - luma) / cr)
        for plane, coefficient in enumerate(chroma):
            if hdr_mode:
                offset = start + 192 * 128 + plane * 96 * 64 + 32 * 96 + 32
                interior = abs(samples[offset] - round(center + cscale * coefficient))
                assert interior <= 3, ('interior UV', frame, interior)
            for y in range(64):
                for x in range(96):
                    coverage = max(0, min(2, 185 - 2*x)) * max(0, min(2, 123 - 2*y)) / 4
                    expected = round(center + cscale * coefficient * coverage)
                    offset = start + 192 * 128 + plane * 96 * 64 + y * 96 + x
                    errors.append(abs(samples[offset] - expected))
    errors.sort()
    mean = sum(errors) / len(errors)
    peak = errors[-1]
    p99 = errors[int(len(errors) * .99)]
    # Lossy QP22 encoding permits ringing at the sharp padded boundary.
    assert mean < .5 * scale and p99 <= 3 * scale and peak <= 16 * scale, (mean, p99, peak)
    print('PASS HEVC GPU capture depth=%d full=%d hdr=%d: 24 pictures, mean=%.3f p99=%d peak=%d'
          % (depth, full, hdr_mode, mean, p99, peak))


def main():
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--native-test', type=Path)
    source.add_argument('--verify', type=Path)
    parser.add_argument('--depth', type=int, choices=(8, 10))
    parser.add_argument('--full', action='store_true')
    parser.add_argument('--hdr-mode', type=int, choices=(0, 1, 2), default=0)
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    if args.negative and args.hdr_mode != 2:
        parser.error("--negative requires --hdr-mode 2")
    if args.verify:
        if args.depth is None:
            parser.error('--verify requires --depth')
        verify(args.verify, args.depth, args.full, args.hdr_mode)
        return
    with tempfile.TemporaryDirectory(prefix='vulkan-hevc-capture-') as directory:
        for depth in ((10,) if args.hdr_mode else (8, 10)):
            for direct in (0, 1):
                for full in ((args.full,) if args.hdr_mode else (False, True)):
                    output = Path(directory) / ('capture-%d-%d-%d.hevc' % (depth, direct, full))
                    command = [str(args.native_test.resolve()), str(depth), str(output),
                               str(direct), str(args.hdr_mode or int(full))]
                    extra = ['--full-range'] if args.hdr_mode and full else []
                    result = subprocess.run(command + extra, check=False, timeout=30)
                    if result.returncode == 77:
                        raise SystemExit(77)
                    result.check_returncode()
                    verify(output, depth, full, args.hdr_mode)
                    if args.negative:
                        wrong = output.with_name('wrong-' + output.name)
                        subprocess.run([str(args.native_test.resolve()), str(depth), str(wrong),
                                        str(direct), '2', '--pq-as-linear'] + extra, check=True, timeout=30)
                        try:
                            verify(wrong, depth, full, args.hdr_mode)
                        except AssertionError as error:
                            assert len(error.args) == 1 and isinstance(error.args[0], tuple)
                            assert len(error.args[0]) == 3
                            print('PASS: treating PQ as linear input is rejected by pixel oracle')
                        else:
                            raise AssertionError('pixel oracle accepted a repeated transfer')


if __name__ == '__main__':
    main()
