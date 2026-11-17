#!/usr/bin/env python3
"""Native HEVC capture/session files with independent video and PCM checks."""
import argparse
from fractions import Fraction
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
from hevc_native_capture_run import verify


def verify_file(path, depth, audio):
    verify(path, depth, False)
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-show_streams', '-show_packets',
        '-of', 'json', str(path)], timeout=30))
    streams = probe['streams']
    assert len(streams) == (2 if audio else 1)
    for stream in streams:
        assert Fraction(stream['start_time']) == 0
        assert Fraction(stream['duration']) == Fraction(4, 5)
        packets = [p for p in probe['packets'] if p['stream_index'] == stream['index']]
        timebase = Fraction(stream['time_base'])
        end = 0
        for packet in packets:
            assert int(packet['pts']) == int(packet['dts']) == end
            end += int(packet['duration'])
        assert end * timebase == Fraction(4, 5)
    if audio:
        stream = streams[1]
        assert stream['codec_name'] == 'ac3'
        assert stream['sample_rate'] == '48000' and stream['channels'] == 2
        raw = subprocess.check_output([
            'ffmpeg', '-v', 'error', '-xerror', '-err_detect', 'explode',
            '-i', str(path), '-map', '0:a:0', '-f', 's16le', '-'], timeout=30)
        pcm = struct.unpack('<%dh' % (len(raw) // 2), raw)
        assert len(pcm) == 38400 * 2
        left, right = pcm[::2], pcm[1::2]
        energy = sum(x*x for x in left)
        rms = math.sqrt(energy / len(left))
        assert 4000 < rms < 8000, rms
        correlation = sum(x*y for x, y in zip(left, right)) / math.sqrt(
            energy * sum(y*y for y in right))
        assert correlation < -.99, correlation
    print('PASS native HEVC session A/V timestamps, samples and channels audio=%d' % audio)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native-test', required=True, type=Path)
    parser.add_argument("--with-cancel", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='hevc-native-session-') as directory:
        for depth in (8, 10):
            for direct in (0, 1):
                for audio in (False, True):
                    path = Path(directory) / ('session-%d-%d-%d.mp4' % (depth, direct, audio))
                    result = subprocess.run([str(args.native_test.resolve()), str(depth),
                                             str(path), str(direct), str(int(audio))],
                                            check=False, timeout=30)
                    if result.returncode == 77:
                        raise SystemExit(77)
                    result.check_returncode()
                    verify_file(path, depth, audio)
                    if args.with_cancel:
                        cancelled = path.with_name("cancel-" + path.name)
                        result = subprocess.run([str(args.native_test.resolve()), str(depth),
                                                 str(cancelled), str(direct), str(int(audio)),
                                                 '--cancel'], check=False, timeout=30)
                        result.check_returncode()
                        print('PASS native HEVC dispatcher cancellation and retirement')


if __name__ == '__main__':
    main()
