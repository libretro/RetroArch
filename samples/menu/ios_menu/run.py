#!/usr/bin/env python3
"""Build the shipping iOS menu adapter with a recording native host."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-only', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='ios-menu-') as tmp:
    exe = Path(tmp) / 'ios_menu_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-Wall', '-Wextra', '-Werror',
        '-DHAVE_MENU', '-DHAVE_IOS_MENU',
        '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('ios_menu_test.c')),
        str(root / 'libretro-common/compat/compat_strl.c'),
        '-o', str(exe)] + shlex.split(os.environ.get('CFLAGS', '')), check=True)
    if not args.build_only:
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
subprocess.run([sys.executable, str(Path(__file__).with_name('availability.py'))] +
               (['--build-only'] if args.build_only else []), check=True)
