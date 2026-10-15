#!/usr/bin/env python3
"""Compare borrowed AV1 container packets/config with ffprobe's byte output."""
from pathlib import Path
from fractions import Fraction
import json
import os
import subprocess
import tempfile

def run(*args):
    return subprocess.check_output(args, text=True)

def raw_dump(dump):
    return bytes.fromhex(''.join(line.split(':', 1)[1].split('  ')[0].replace(' ', '')
        for line in dump.splitlines() if ':' in line))

def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return f'{value:08x}'

exe = str(Path(os.environ.get('AV1_CONTAINER_TEST', './rav1_container_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-') as directory:
    cases = [('420-8', 'yuv420p', 8, 0), ('420-10', 'yuv420p10le', 10, 0),
             ('420-12', 'yuv420p12le', 12, 2), ('422-8', 'yuv422p', 8, 2),
             ('422-10', 'yuv422p10le', 10, 2), ('422-12', 'yuv422p12le', 12, 2),
             ('444-8', 'yuv444p', 8, 1), ('444-10', 'yuv444p10le', 10, 1),
             ('444-12', 'yuv444p12le', 12, 2), ('mono-8', 'gray', 8, 0),
             ('mono-12', 'gray12le', 12, 2)]
    for name, pixel_format, depth, profile in cases:
        for container in ('mp4', 'webm'):
            path = Path(directory) / f'av1-{name}.{container}'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=192x128:rate=12', '-frames:v', '12', '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', pixel_format,
                '-colorspace', 'bt709', '-color_primaries', 'bt709',
                '-color_trc', 'bt709', '-color_range', 'tv', str(path)], check=True)
            oracle = json.loads(run('ffprobe', '-v', 'error', '-select_streams', 'v:0',
                '-show_streams', '-show_packets', '-show_data', '-of', 'json', str(path)))
            stream = oracle['streams'][0]
            assert stream['codec_name'] == 'av1'
            private = raw_dump(stream['extradata'])
            expected = [f"S {stream['width']} {stream['height']} {depth} 1 1 1 0", f"C {stream['width']} {stream['height']} {profile} {depth} {len(private)} {fnv(private)}"]
            for packet in oracle['packets']:
                data = raw_dump(packet['data'])
                timestamp = int(int(packet['pts']) * Fraction(stream['time_base']) * 1000000000)
                expected.append(f"P {timestamp} {int('K' in packet['flags'])} {len(data)} {fnv(data)}")
            actual = run(exe, str(path)).splitlines()
            assert actual == expected, (depth, container, actual, expected)
            assert len(oracle['packets']) == 12
            print(f'AV1 {name} {container}: sequence/color and 12 packet bytes/timestamps match ffprobe')
