#!/usr/bin/env python3
"""Decode native GPU capture and compare BT.709 samples, including padding."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def verify(path, depth, full):
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-count_frames', '-show_streams',
        '-show_frames', '-of', 'json', str(path)], timeout=30))
    stream = probe['streams'][0]
    assert stream['codec_name'] == 'hevc'
    assert (stream['width'], stream['height']) == (192, 128)
    assert int(stream['nb_read_frames']) == 24 and not stream['has_b_frames']
    assert stream['profile'] == ('Main' if depth == 8 else 'Main 10')
    assert stream['color_range'] == ('pc' if full else 'tv')
    for key in ('color_space', 'color_transfer', 'color_primaries'):
        assert stream[key] == 'bt709', (key, stream[key])
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
        luma = sum(a * b for a, b in zip(rgb, (.2126, .7152, .0722)))
        start = frame * stride
        for y in range(128):
            for x in range(192):
                expected = round(yoffset + yscale * (luma if x < 185 and y < 123 else 0))
                errors.append(abs(samples[start + y * 192 + x] - expected))
        chroma = ((rgb[2] - luma) / 1.8556, (rgb[0] - luma) / 1.5748)
        for plane, coefficient in enumerate(chroma):
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
    print('PASS HEVC GPU capture depth=%d full=%d: 24 pictures, mean=%.3f p99=%d peak=%d'
          % (depth, full, mean, p99, peak))


def main():
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--native-test', type=Path)
    source.add_argument('--verify', type=Path)
    parser.add_argument('--depth', type=int, choices=(8, 10))
    parser.add_argument('--full', action='store_true')
    args = parser.parse_args()
    if args.verify:
        if args.depth is None:
            parser.error('--verify requires --depth')
        verify(args.verify, args.depth, args.full)
        return
    with tempfile.TemporaryDirectory(prefix='vulkan-hevc-capture-') as directory:
        for depth in (8, 10):
            for direct in (0, 1):
                for full in (False, True):
                    output = Path(directory) / ('capture-%d-%d-%d.hevc' % (depth, direct, full))
                    result = subprocess.run([str(args.native_test.resolve()), str(depth),
                                             str(output), str(direct), str(int(full))], check=False, timeout=30)
                    if result.returncode == 77:
                        raise SystemExit(77)
                    result.check_returncode()
                    verify(output, depth, full)


if __name__ == '__main__':
    main()
