#!/usr/bin/env python3
"""Check capture source retirement against real DXGI ResizeBuffers."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--runner', default='')
parser.add_argument('binary')
args = parser.parse_args()
result = subprocess.run([*shlex.split(args.runner), str(Path(args.binary).resolve())])
if result.returncode == 77 and not os.environ.get('NVENC_TEST_REQUIRE_HW'):
    raise SystemExit(0)
raise SystemExit(result.returncode)
