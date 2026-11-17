#!/usr/bin/env python3
"""Native Vulkan HEVC/AAC producer through independent TCP/TLS receivers."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--native-test', type=Path, required=True)
parser.add_argument('--artifacts', type=Path)
parser.add_argument('--dispatch', action='store_true', help='use the async recording dispatcher')
parser.add_argument('--session', action='store_true', help='use the complete recording session owner')
parser.add_argument('--output', action='store_true', help='use the session output wrapper')
a = parser.parse_args()
if sys.platform != 'win32':
    raise SystemExit('Native Vulkan/AAC streaming requires the Windows AAC backend')
root = a.artifacts.resolve() if a.artifacts else Path(tempfile.mkdtemp(prefix='vulkan-stream-'))
root.mkdir(parents=True, exist_ok=True)
oracle = Path(__file__).resolve().parents[1] / 'nvenc_d3d12' / 'stream.py'
cases = [('sdr8', 8, 1, None, False), ('sdr10', 10, 1, None, False),
         ('pq', 10, 2, None, False), ('pq12', 10, 2, '1.2', False),
         ('pq13', 10, 2, '1.3', False), ('scrgb', 10, 2, None, True),
         ('scrgb13', 10, 2, '1.3', True)]
print(f'Native Vulkan streaming artifacts: {root}', flush=True)
for name, depth, color, tls, scrgb in cases:
    env = os.environ.copy()
    env.pop('VULKAN_STREAM_SCRGB', None)
    env.pop('VULKAN_STREAM_OUTPUT', None)
    env.pop('VULKAN_STREAM_SESSION', None)
    env.pop('VULKAN_STREAM_DISPATCH', None)
    if a.dispatch:
        env['VULKAN_STREAM_DISPATCH'] = '1'
    if a.session:
        env['VULKAN_STREAM_SESSION'] = '1'
    if a.output:
        env['VULKAN_STREAM_OUTPUT'] = '1'
    if scrgb:
        env['VULKAN_STREAM_SCRGB'] = '1'
    command = [sys.executable, str(oracle), str(a.native_test.resolve()), '--producer', 'Vulkan',
               '--codec', 'hevc', '--depth', str(depth), '--color', str(color),
               '--fps', '30', '--width', '192', '--height', '128',
               '--output-dir', str(root / name)]
    if tls:
        command += ['--tls', tls]
    with (root / (name + '.log')).open('w') as log:
        subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    print(f'PASS Vulkan receiver group {name}', flush=True)
print('PASS 28 native Vulkan HEVC/AAC receiver cases', flush=True)
