#!/usr/bin/env python3
"""Planar video output against the RGB blit, over streams FFmpeg makes."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from h26x_fixture import executable

BT709 = 1 << 2
FULL_RANGE = 1 << 1

SOURCE = ['-f', 'lavfi', '-i', 'testsrc2=size=66x50:rate=24',
          '-frames:v', '8', '-an']
HD_SOURCE = ['-f', 'lavfi', '-i', 'testsrc2=size=1280x720:rate=24',
             '-frames:v', '2', '-an']
MP4 = ['-movflags', '+faststart']

# name, container, ffmpeg arguments, takes planar output, flags (None:
# not checked)
CASES = [
    ('h264_baseline.mp4', 'mp4', SOURCE + ['-c:v', 'libx264', '-profile:v',
        'baseline', '-pix_fmt', 'yuv420p'] + MP4, 1, 0),
    ('h264_bframes.mp4', 'mp4', SOURCE + ['-c:v', 'libx264', '-bf', '2',
        '-pix_fmt', 'yuv420p'] + MP4, 1, 0),
    ('h264_709_full.mp4', 'mp4', SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv420p', '-colorspace', 'bt709', '-color_primaries', 'bt709',
        '-color_trc', 'bt709', '-color_range', 'pc'] + MP4, 1,
        BT709 | FULL_RANGE),
    ('h264_hd.mp4', 'mp4', HD_SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv420p'] + MP4, 1, BT709),
    ('h265.mp4', 'mp4', SOURCE + ['-c:v', 'libx265', '-x265-params',
        'log-level=error', '-pix_fmt', 'yuv420p', '-tag:v', 'hvc1'] + MP4,
        1, 0),
    ('h264.mkv', 'webm', SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv420p'], 1, 0),
    ('h265.mkv', 'webm', SOURCE + ['-c:v', 'libx265', '-x265-params',
        'log-level=error', '-pix_fmt', 'yuv420p'], 1, 0),
    ('vp9.webm', 'webm', SOURCE + ['-c:v', 'libvpx-vp9', '-pix_fmt',
        'yuv420p'], 1, 0),
    ('vp8.webm', 'webm', SOURCE + ['-c:v', 'libvpx'], 1, 0),
    ('vp9.mp4', 'mp4', SOURCE + ['-c:v', 'libvpx-vp9', '-pix_fmt',
        'yuv420p'] + MP4, 1, 0),
    # not 8-bit 4:2:0 SDR: refused before any decode
    ('h264_444.mp4', 'mp4', SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv444p'] + MP4, 0, None),
    ('h265_10.mp4', 'mp4', SOURCE + ['-c:v', 'libx265', '-x265-params',
        'log-level=error', '-pix_fmt', 'yuv420p10le', '-tag:v', 'hvc1']
        + MP4, 0, None),
    ('vp9_444.webm', 'webm', SOURCE + ['-c:v', 'libvpx-vp9', '-pix_fmt',
        'yuv444p'], 0, None),
    ('h264_2020.mp4', 'mp4', SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv420p', '-colorspace', 'bt2020nc', '-color_primaries', 'bt2020',
        '-color_trc', 'bt709'] + MP4, 0, None),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('test')
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--runner', default=os.environ.get('RUNNER', ''))
    args = parser.parse_args()
    runner = shlex.split(args.runner)
    test = executable(args.test)
    with tempfile.TemporaryDirectory(prefix='video-planar-') as directory:
        for name, container, ffargs, planar, flags in CASES:
            path = Path(directory) / name
            subprocess.run([args.ffmpeg, '-v', 'error', '-y'] + ffargs
                           + [str(path)], check=True)
            command = runner + [test, str(path), container, str(planar)]
            if flags is not None:
                command.append(str(flags))
            subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
