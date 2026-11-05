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
    setting_source = (root / 'menu/menu_setting.c').read_text()
    start = setting_source.index('int menu_setting_set_with_string_representation(')
    end = setting_source.index('\nstatic void menu_input_st_uint_cb', start)
    source = Path(tmp) / 'ios_menu_test.c'
    test = Path(__file__).with_name('ios_menu_test.c')
    source.write_text('#include "' + test.as_posix() + '"\n' + setting_source[start:end])
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-Wall', '-Wextra', '-Werror',
        '-D_GNU_SOURCE', '-D_DARWIN_C_SOURCE',
        '-DHAVE_MENU', '-DHAVE_IOS_MENU',
        '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        str(source),
        str(root / 'libretro-common/compat/compat_strl.c'),
        '-o', str(exe)] + shlex.split(os.environ.get('CFLAGS', '')), check=True)
    if not args.build_only:
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
subprocess.run([sys.executable, str(Path(__file__).with_name('availability.py'))] +
               (['--build-only'] if args.build_only else []), check=True)
