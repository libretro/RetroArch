#!/usr/bin/env python3
"""Independent AV1 decode references; optional native D3D11 acceptance."""
from pathlib import Path
import argparse
from array import array
import sys
import os
import shlex
import subprocess


def run(args, env=None):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--hdr', action='store_true', help='generate PQ/HLG BT.2020 fixtures')
    args = parser.parse_args()
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    exe = args.exe.resolve() if args.exe else None
    cases = (
        ('tiles', 24, '256x144', ['-lag-in-frames', '0', '-g', '12',
                               '-tiles', '2x2', '-aom-params', 'sb-size=64']),
        ('reorder', 30, '256x256', ['-lag-in-frames', '25', '-g', '24']),
        ('sb128', 12, '256x256', ['-aom-params', 'sb-size=128']),
        ('lossless', 12, '256x144', ['-crf', '0', '-b:v', '0']),
    )
    depths = (8, 10)
    if args.hdr:
        depths = (10,)
        cases = tuple((f'{transfer}-{range_name}', 12, '256x144',
                       ['-lag-in-frames', '0', '-g', '6', '-tiles', '2x2',
                        '-color_primaries', 'bt2020', '-color_trc', transfer,
                        '-colorspace', 'bt2020nc', '-color_range', range_name])
                      for transfer in ('smpte2084', 'arib-std-b67')
                      for range_name in ('tv', 'pc'))
    for depth in depths:
        for name, count, geometry, options in cases:
            stem = directory / f'{depth}-{name}'
            obu = stem.with_suffix('.obu')
            reference = stem.with_suffix('.raw')
            run(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i',
                 f'testsrc2=size={geometry}:rate=24', '-frames:v', count,
                 '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                 '-c:v', 'libaom-av1', '-cpu-used', '6', '-threads', '2',
                 *options, '-f', 'obu', obu])
            width, height = map(int, geometry.split('x'))
            if args.hdr:
                # Keep decoded code values: swscale's default planar-to-P010
                # conversion remaps full range to limited range. Packing the
                # native planar samples ourselves avoids any colour transform.
                planar = stem.with_suffix('.yuv420p10le')
                run(['ffmpeg', '-v', 'error', '-y', '-c:v', 'libdav1d', '-i', obu,
                     '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', planar])
                samples = array('H')
                samples.frombytes(planar.read_bytes())
                if sys.byteorder != 'little':
                    samples.byteswap()
                packed = array('H')
                luma = width * height
                for first in range(0, len(samples), luma * 3 // 2):
                    frame = samples[first:first + luma * 3 // 2]
                    packed.extend(value << 6 for value in frame[:luma])
                    for pixel in range(luma // 4):
                        packed.extend((frame[luma + pixel] << 6,
                                       frame[luma + luma // 4 + pixel] << 6))
                if sys.byteorder != 'little':
                    packed.byteswap()
                reference.write_bytes(packed.tobytes())
            else:
                run(['ffmpeg', '-v', 'error', '-y', '-c:v', 'libdav1d', '-i', obu,
                     '-pix_fmt', 'nv12' if depth == 8 else 'p010le',
                     '-f', 'rawvideo', reference])
            expected = width * height * 3 // 2 * (2 if depth == 10 else 1) * count
            assert reference.stat().st_size == expected
            streams = [obu]
            for extension in ('.webm', '.mp4'):
                container = stem.with_suffix(extension)
                run(['ffmpeg', '-v', 'error', '-y', '-r', '24', '-i', obu,
                     '-c:v', 'copy', container])
                streams.append(container)
            if exe:
                for direct in (False, True):
                    env = os.environ.copy()
                    env.pop('D3D11_TEST_SHARED_DPB', None)
                    if direct:
                        env['D3D11_TEST_SHARED_DPB'] = '1'
                    for stream in streams:
                        run(shlex.split(env.get('RUNNER', '')) +
                            [exe, stream, reference], env=env)


if __name__ == '__main__':
    main()
