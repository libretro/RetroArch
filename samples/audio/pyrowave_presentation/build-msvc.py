# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Build the native presentation test from an MSVC Native Tools prompt."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true', help='Run shared and exclusive endpoint checks (silent PCM)')
parser.add_argument('--unit', action='store_true', help='Run only the scripted COM clock checks')
args = parser.parse_args()
sample = Path(__file__).resolve().parent
root = sample.parents[2]
obj = sample / 'obj-msvc'
obj.mkdir(exist_ok=True)
text = (sample / 'Makefile').read_text(encoding='utf-8')
sources = text[text.index('SOURCES := '):text.index('\nelse\n')]
sources = sources.removeprefix('SOURCES := ').replace('\\\n', ' ').split()
sources = [str((sample / s.replace('$(RA)', '../../..').replace(
    '$(COMMON)', '../../../libretro-common')).resolve()) for s in sources]
exe = sample / 'wasapi_presentation_test.exe'
command = ['cl', '/nologo', '/TC', '/W3', '/O2', '/D_CRT_SECURE_NO_WARNINGS',
    '/D_CRT_NONSTDC_NO_WARNINGS', '/DRARCH_INTERNAL', '/DHAVE_THREADS',
    '/DHAVE_WASAPI', '/DHAVE_PYROWAVE', '/D_WIN32_WINNT=0x0602',
    '/Dsmall=char', '/Dhyper=__int64', '/I' + str(root / 'libretro-common/include'),
    '/I' + str(root / 'deps'), '/I' + str(root), '/Fo' + str(obj) + '/',
    '/Fe' + str(exe)]
subprocess.run(command + sources + ['/link', 'ole32.lib', 'ksuser.lib', 'uuid.lib',
    'winmm.lib', 'user32.lib'], cwd=obj, check=True)
if args.unit:
    subprocess.run([str(exe), '--unit'], check=True)
if args.check:
    subprocess.run([str(exe)], check=True)
    subprocess.run([str(exe), '--exclusive'], check=True)
