#!/usr/bin/env python3
"""Regenerate synthetic multitrack tone fixtures; tests need no FFmpeg runtime."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
root = Path(__file__).resolve().parent / 'fixtures'
root.mkdir(exist_ok=True)
base = [args.ffmpeg, '-y', '-v', 'error', '-f', 'lavfi', '-i',
        'sine=frequency=440:sample_rate=48000:duration=0.35', '-f', 'lavfi', '-i',
        'sine=frequency=880:sample_rate=48000:duration=0.35', '-f', 'lavfi', '-i',
        'color=size=16x16:rate=10:duration=0.35']
for name, codec, container in [('aac-mp4', 'aac', 'mp4'), ('aac-webm', 'aac', 'matroska'),
                               ('opus', 'libopus', 'matroska'),
                               ('vorbis', 'libvorbis', 'matroska'), ('flac', 'flac', 'matroska')]:
    subprocess.run(base + ['-map', '2:v:0', '-map', '0:a:0', '-map', '1:a:0',
                   '-c:v', 'libx264', '-preset', 'ultrafast', '-c:a', codec,
                   '-metadata:s:a:0', 'language=eng', '-metadata:s:a:1', 'language=jpn',
                   '-f', container, str(root / (name + '.bin'))], check=True)
subprocess.run(base + ['-map', '2:v:0', '-map', '0:a:0', '-map', '1:a:0', '-map', '0:a:0',
               '-map', '1:a:0', '-c:v', 'libx264', '-preset', 'ultrafast',
               '-c:a:0', 'libopus', '-c:a:1', 'libvorbis', '-c:a:2', 'aac', '-c:a:3', 'flac',
               '-f', 'matroska', str(root / 'mixed.bin')], check=True)
