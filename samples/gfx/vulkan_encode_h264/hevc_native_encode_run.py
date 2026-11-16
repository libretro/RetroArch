#!/usr/bin/env python3
"""Compare native HEVC output against the submitted flat-picture sequence."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def verify(path, depth):
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-count_frames', '-show_streams',
        '-show_frames', '-of', 'json', str(path)]))
    stream = probe['streams'][0]
    assert stream['codec_name'] == 'hevc'
    assert stream['width'] == 192 and stream['height'] == 128
    assert int(stream['nb_read_frames']) == 24 and not stream['has_b_frames']
    assert stream['profile'] == ('Main' if depth == 8 else 'Main 10')
    assert stream['color_range'] == 'tv'
    assert stream['color_primaries'] == ('bt709' if depth == 8 else 'bt2020')
    assert stream['color_transfer'] == ('bt709' if depth == 8 else 'smpte2084')
    assert stream['color_space'] == ('bt709' if depth == 8 else 'bt2020nc')
    assert stream['r_frame_rate'] == '30/1'
    assert [i for i, frame in enumerate(probe['frames'])
            if frame['key_frame']] == [0, 7, 15, 23]
    raw = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-xerror', '-err_detect', 'explode',
        '-threads', '1', '-i', str(path), '-pix_fmt',
        'yuv420p' if depth == 8 else 'yuv420p10le', '-f', 'rawvideo', '-'])
    samples = raw if depth == 8 else struct.unpack('<%dH' % (len(raw) // 2), raw)
    luma = 192 * 128
    stride = luma * 3 // 2
    assert len(samples) == stride * 24
    peak = 0
    scale = 1 if depth == 8 else 4
    for frame in range(24):
        expected = (32 + frame * 6) * scale
        pixels = samples[frame * stride:frame * stride + luma]
        chroma = samples[frame * stride + luma:(frame + 1) * stride]
        peak = max(peak, max(abs(value - expected) for value in pixels))
        assert max(abs(value - 128 * scale) for value in chroma) <= 4 * scale
    assert peak <= 4 * scale, 'decoded luma error %d at depth %d' % (peak, depth)
    print('PASS native HEVC depth=%d: 24 pictures, four IDRs, color, clock, peak=%d'
          % (depth, peak))


def main():
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--native-test', type=Path)
    source.add_argument('--verify', type=Path)
    parser.add_argument('--depth', type=int, choices=(8, 10))
    parser.add_argument('--with-cancel', action='store_true')
    args = parser.parse_args()
    if args.verify:
        if args.depth is None:
            parser.error('--verify requires --depth')
        verify(args.verify, args.depth)
        return
    with tempfile.TemporaryDirectory(prefix='vulkan-hevc-pictures-') as directory:
        for depth in (8, 10):
            for cancel in ((False, True) if args.with_cancel else (False,)):
                output = Path(directory) / ('main%d-%d.hevc' % (depth, cancel))
                command = [str(args.native_test.resolve()), str(depth), str(output)]
                if cancel:
                    command.append('--cancel')
                result = subprocess.run(command, check=False)
                if result.returncode == 77:
                    raise SystemExit(77)
                result.check_returncode()
                verify(output, depth)


if __name__ == '__main__':
    main()
