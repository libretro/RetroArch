#!/usr/bin/env python3
"""Independent FLV packet, timestamp and decoded video/audio checks."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def command(args):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=60)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace'))
    return result.stdout


def probe(path):
    data = json.loads(command(['ffprobe', '-v', 'error', '-show_packets',
                               '-show_data_hash', 'sha256', '-of', 'json', str(path)]))
    return [(p['codec_type'], p['dts'], p['pts'], p['size'], p['data_hash'])
            for p in data['packets']]


def decode(path, audio):
    return command(['ffmpeg', '-v', 'error', '-i', str(path), '-map',
                    '0:a:0' if audio else '0:v:0',
                    *(['-f', 'f32le'] if audio else
                      ['-fps_mode', 'passthrough', '-pix_fmt', 'yuv420p', '-f', 'rawvideo']), '-'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    for tool in ('ffmpeg', 'ffprobe'):
        if not shutil.which(tool):
            raise SystemExit(f'{tool} is required (test only)')
    binary = str(Path(args.binary).resolve())
    with tempfile.TemporaryDirectory(prefix='flv-mux-') as temp:
        root = Path(temp)
        for bframes in (0, 2):
            source, output = root / 'source.flv', root / 'output.flv'
            command(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i',
                     'testsrc2=size=128x96:rate=25:duration=1', '-f', 'lavfi', '-i',
                     'sine=frequency=997:sample_rate=44100:duration=1',
                     '-c:v', 'libx264', '-threads', '1', '-bf', str(bframes),
                     '-g', '12', '-c:a', 'aac', '-profile:a', 'aac_low',
                     '-flvflags', 'no_duration_filesize', '-f', 'flv', str(source)])
            command(shlex.split(args.runner) + [binary, str(source), str(output)])
            assert probe(source) == probe(output), 'encoded packets or timestamps differ'
            for audio in (False, True):
                assert decode(source, audio) == decode(output, audio), 'decoded output differs'
            print(f'PASS: independent FLV packet/timestamp/video/PCM oracle, B-frames={bframes}')


if __name__ == '__main__':
    main()
