#!/usr/bin/env python3
"""Generate OBU streams and lossless NV12/P010 decode oracles for real GPUs."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)
for depth in (8, 10):
    for mode, count, options in (
            ('intra', 6, ['-g', '1']),
            ('inter', 12, ['-lag-in-frames', '0']),
            ('reorder', 30, ['-lag-in-frames', '25']),
            ('tiles', 6, ['-tiles', '2x2']),
            ('sb128', 6, ['-aom-params', 'sb-size=128']),
            ('lossless', 6, ['-crf', '0', '-b:v', '0']),
            ('still', 1, ['-still-picture', '1']),
            ('color', 6, ['-color_range', 'pc', '-colorspace', 'bt709',
                          '-color_primaries', 'bt709', '-color_trc', 'bt709'])):
        path = args.directory / f'{depth}-{mode}.obu'
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
            'testsrc2=size=256x256:rate=12', '-frames:v', str(count), '-an',
            '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
            '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
            *options, '-y', str(path)], check=True)
        # NV12/P010 conversion must preserve full-range encoded sample values.
        scale = ['-vf', 'scale=in_range=full:out_range=full'] if mode == 'color' else []
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), *scale,
            '-pix_fmt', 'nv12' if depth == 8 else 'p010le', '-f', 'rawvideo',
            '-y', str(path.with_suffix('.raw'))], check=True)
        subprocess.run(['ffmpeg', '-v', 'error', '-fflags', '+genpts', '-r', '12',
            '-i', str(path), '-c:v', 'copy', '-movflags', '+faststart',
            '-y', str(path.with_suffix('.mp4'))], check=True)
        subprocess.run(['ffmpeg', '-v', 'error', '-i',
            str(path.with_suffix('.mp4')), '-c:v', 'copy',
            '-y', str(path.with_suffix('.webm'))], check=True)
        print(f'{path}: {count} output frames')
