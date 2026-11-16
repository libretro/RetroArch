#!/usr/bin/env python3
"""Calibrate the Metal pixel oracle against independent RGB-to-YUV conversion."""
import argparse
import array
import math
import shutil
import subprocess
import sys

from metal_vt_oracle import audio, pixels


def rejected(function, *args):
    try:
        function(*args)
    except AssertionError:
        return
    raise AssertionError('invalid decoded output was accepted')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    args = parser.parse_args()
    assert args.ffmpeg, 'independent FFmpeg converter is required'
    width, height = 162, 130
    rgb = b''.join(bytes(((frame * 37 + 64) % 192 + 32,
                          (frame * 53 + 100) % 192 + 32,
                          (frame * 29 + 150) % 192 + 32)) * (width * height) for frame in range(24))
    for depth in (8, 10):
        filters = 'scale=out_color_matrix=bt709:out_range=tv'
        if depth == 8:
            filters += ',format=yuv420p'
        filters += ',format=yuv420p10le'
        raw = subprocess.run([args.ffmpeg, '-v', 'error', '-threads', '1', '-filter_threads', '1',
                              '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', f'{width}x{height}',
                              '-i', 'pipe:0', '-vf', filters, '-f', 'rawvideo', 'pipe:1'],
                             input=rgb, capture_output=True, check=True, timeout=30).stdout
        result = pixels(raw, width, height, depth)
        frame_size = width * height * 3
        stale = raw[:frame_size] + raw[:frame_size] + raw[frame_size * 2:]
        rejected(pixels, stale, width, height, depth)
        rejected(pixels, raw[:-2], width, height, depth)
        print('PASS independent RGB conversion and stale/truncated-frame rejection:', depth, result)
    for rate, channels in ((48000, 2), (44100, 1)):
        leading, frames = 2112, 19220
        padding = -(leading + frames) % 1024
        samples = array.array('h', [0] * (leading * channels))
        samples.extend(int(math.sin(i * 2 * math.pi * (1499 if channel else 997) / rate) * 12000)
                       for i in range(frames) for channel in range(channels))
        samples.extend([0] * (padding * channels))
        if sys.byteorder != 'little':
            samples.byteswap()
        raw = samples.tobytes()
        audio(raw, rate, channels, leading, frames, rate)
        rejected(audio, raw, rate, channels, leading + 100, frames, rate)
    print('PASS aligned source-clock audio oracle and wrong-priming rejection')


if __name__ == '__main__':
    main()
