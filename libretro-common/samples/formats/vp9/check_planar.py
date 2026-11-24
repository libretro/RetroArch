#!/usr/bin/env python3
"""Planar video output against the RGB blit, over streams FFmpeg makes."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import struct
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
    ('h264_bframes.mkv', 'webm', SOURCE + ['-c:v', 'libx264', '-bf', '2',
        '-pix_fmt', 'yuv420p'], 1, 0),
    ('h264_709_full.mkv', 'webm', SOURCE + ['-c:v', 'libx264', '-pix_fmt',
        'yuv420p', '-colorspace', 'bt709', '-color_primaries', 'bt709',
        '-color_trc', 'bt709', '-color_range', 'pc'], 1, BT709 | FULL_RANGE),
    ('h265.mkv', 'webm', SOURCE + ['-c:v', 'libx265', '-x265-params',
        'log-level=error', '-pix_fmt', 'yuv420p'], 1, 0),
    ('h265_reuse.mp4', 'mp4', SOURCE + ['-frames:v', '64', '-c:v',
        'libx265', '-x265-params', 'log-level=error:pools=none', '-bf', '2',
        '-pix_fmt', 'yuv420p', '-tag:v', 'hvc1'] + MP4, 1, 0),
    ('h265_reuse.mkv', 'webm', SOURCE + ['-frames:v', '64', '-c:v',
        'libx265', '-x265-params', 'log-level=error:pools=none', '-bf', '2',
        '-pix_fmt', 'yuv420p', '-colorspace', 'bt709', '-color_primaries',
        'bt709', '-color_trc', 'bt709', '-color_range', 'pc'], 1,
        BT709 | FULL_RANGE),
    ('vp9.webm', 'webm', SOURCE + ['-c:v', 'libvpx-vp9', '-pix_fmt',
        'yuv420p'], 1, 0),
    ('vp9_reuse.webm', 'webm', SOURCE + ['-frames:v', '64', '-c:v',
        'libvpx-vp9', '-lag-in-frames', '16', '-auto-alt-ref', '1',
        '-cpu-used', '8', '-pix_fmt', 'yuv420p'], 1, 0),
    ('vp9_reuse.mp4', 'mp4', SOURCE + ['-frames:v', '64', '-c:v',
        'libvpx-vp9', '-lag-in-frames', '16', '-auto-alt-ref', '1',
        '-cpu-used', '8', '-pix_fmt', 'yuv420p', '-colorspace', 'bt709',
        '-color_primaries', 'bt709', '-color_trc', 'bt709',
        '-color_range', 'pc'] + MP4, 1, BT709 | FULL_RANGE),
    ('vp8.webm', 'webm', SOURCE + ['-c:v', 'libvpx'], 1, 0),
    ('vp8_reuse.webm', 'webm', SOURCE + ['-frames:v', '64', '-c:v',
        'libvpx', '-lag-in-frames', '16', '-auto-alt-ref', '1',
        '-deadline', 'realtime', '-pix_fmt', 'yuv420p'], 1, 0),
    ('vp8_reuse.mp4', 'mp4', SOURCE + ['-frames:v', '64', '-c:v',
        'libvpx', '-deadline', 'realtime', '-pix_fmt', 'yuv420p'], 1, 0),
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


def vp8_mp4(blob):
    """Minimal vp08 fixture: FFmpeg does not mux VP8 into MP4."""
    def words(*values):
        return struct.pack('>' + 'I' * len(values), *values)

    def box(kind, data):
        return words(len(data) + 8) + kind.encode('ascii') + data

    width, height = struct.unpack_from('<HH', blob, 12)
    rate, scale = struct.unpack_from('<II', blob, 16)
    frames = []
    offset = 32
    while offset < len(blob):
        size = struct.unpack_from('<I', blob, offset)[0]
        offset += 12
        frames.append(blob[offset:offset + size])
        offset += size
    count = len(frames)
    ftyp = box('ftyp', b'isom' + words(0))
    entry = box('vp08', bytes(24) + struct.pack('>HH', width, height) + bytes(50))
    sync = [i + 1 for i, frame in enumerate(frames) if not frame[0] & 1]
    tables = box('stsd', words(0, 1) + entry)
    tables += box('stts', words(0, 1, count, scale))
    tables += box('stsc', words(0, 1, 1, count, 1))
    tables += box('stsz', words(0, 0, count, *map(len, frames)))
    tables += box('stco', words(0, 1, len(ftyp) + 8))
    tables += box('stss', words(0, len(sync), *sync))
    mdhd = box('mdhd', bytes(12) + words(rate, count * scale, 0))
    hdlr = box('hdlr', bytes(8) + b'vide' + bytes(13))
    mdia = box('mdia', mdhd + hdlr + box('minf', box('stbl', tables)))
    trak = box('trak', box('tkhd', bytes(12) + words(1) + bytes(68)) + mdia)
    moov = box('moov', box('mvhd', bytes(12) + words(rate, count * scale)
                           + bytes(80)) + trak)
    return ftyp + box('mdat', b''.join(frames)) + moov


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
            vp8_in_mp4 = name.startswith('vp8') and container == 'mp4'
            encoded = path.with_suffix('.ivf') if vp8_in_mp4 else path
            subprocess.run([args.ffmpeg, '-v', 'error', '-y'] + ffargs
                           + [str(encoded)], check=True)
            if vp8_in_mp4:
                path.write_bytes(vp8_mp4(encoded.read_bytes()))
            command = runner + [test, str(path), container, str(planar)]
            if flags is not None:
                command.append(str(flags))
            env = os.environ.copy()
            env.pop('RETAIN_PLANAR', None)
            if name.startswith(('h264', 'h265', 'vp9', 'vp8')) and planar:
                env['RETAIN_PLANAR'] = '1'
            subprocess.run(command, check=True, env=env)


if __name__ == '__main__':
    main()
