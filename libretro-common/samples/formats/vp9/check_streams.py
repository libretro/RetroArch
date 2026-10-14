#!/usr/bin/env python3
"""Exercise hardware-session metadata with libvpx packets, without a GPU."""
from pathlib import Path
import subprocess
import sys
import tempfile

test = Path(sys.argv[1] if len(sys.argv) > 1 else './rvp9_hw_session_test').resolve()
with tempfile.TemporaryDirectory(prefix='rvp9-hw-') as directory:
    for depth in (8, 10):
        stream = Path(directory) / f'profile-{0 if depth == 8 else 2}.ivf'
        subprocess.run([
            'ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
            'testsrc2=size=320x192:rate=24', '-vf', f'format=yuv420p{ "10le" if depth == 10 else ""}',
            '-frames:v', '48', '-c:v', 'libvpx-vp9', '-profile:v', '0' if depth == 8 else '2',
            '-g', '12', '-lag-in-frames', '0', '-threads', '2', '-y', str(stream)
        ], check=True)
        subprocess.run([str(test), str(stream)], check=True)
