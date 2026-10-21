#!/usr/bin/env python3
"""Independent native Metal capture gate; no encoder or FFmpeg is required."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('Native Metal capture acceptance requires Apple frameworks')
here = Path(__file__).resolve().parent
root = here.parents[2]
subprocess.run([sys.executable, str(root / 'record/shaders/generate_metal.py'), '--check'], check=True)
subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
    '-DHAVE_THREADS', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
    '-Wno-long-long', '-Wno-c99-extensions', '-mmacosx-version-min=10.14',
    '-I' + str(root / 'libretro-common/include'), '-fsyntax-only',
    str(here / 'worker_native_compile.c')], check=True)
unavailable = 0
with tempfile.TemporaryDirectory(prefix='metal-record-capture-') as temp:
    for ownership in ('-fobjc-arc', '-fno-objc-arc'):
        binary = Path(temp) / ('capture-arc' if ownership == '-fobjc-arc' else 'capture-mrc')
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-DHAVE_THREADS', '-O2', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
            '-Wno-long-long', '-Wno-c99-extensions', '-Wframe-larger-than=2048',
            '-mmacosx-version-min=10.14', '-fblocks', ownership,
            '-I' + str(root / 'libretro-common/include'), str(here / 'capture_test.m'),
            str(root / 'record/record_gpu_capture_metal.m'), '-o', str(binary)]
        for framework in ('Foundation', 'Metal', 'CoreVideo', 'CoreFoundation'):
            command += ['-framework', framework]
        print('+', shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        result = subprocess.run([str(binary)])
        if result.returncode == 77:
            unavailable += 1
            print(f'SKIP: Metal capture {ownership}; acceptance incomplete', flush=True)
        else:
            result.check_returncode()
if unavailable:
    raise SystemExit(77)
