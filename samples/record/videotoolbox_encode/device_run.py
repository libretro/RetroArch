#!/usr/bin/env python3
"""Test-only encoder/device inventory; enumeration is not encode acceptance."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('Native encoder/device inventory requires macOS')
here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix='vt-devices-') as temp:
    binary = Path(temp) / 'devices'
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror', '-Wno-long-long',
        '-Wno-c99-extensions', '-Wframe-larger-than=2048', '-mmacosx-version-min=10.14',
        '-fobjc-arc', '-fblocks', '-I' + str(root / 'libretro-common/include'), str(here / 'device_test.m'),
        '-o', str(binary), '-framework', 'Foundation', '-framework', 'Metal',
        '-framework', 'VideoToolbox', '-framework', 'CoreFoundation']
    subprocess.run(command, check=True)
    data = subprocess.check_output([str(binary)])
    report = json.loads(data)
    print(json.dumps(report, indent=2), flush=True)
    output = os.environ.get('ENCODER_DEVICE_ARTIFACT')
    if output:
        Path(output).write_bytes(data)
