#!/usr/bin/env python3
# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT
"""Compile the shipping Win32 stat helpers and branch with injected SDK calls."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cc', default='cc')
parser.add_argument('--sanitizer', default='')
parser.add_argument('--source', type=Path)
a = parser.parse_args()
here = Path(__file__).resolve().parent
source = (a.source or here.parents[2] / 'vfs/vfs_implementation.c').read_text(encoding='utf-8')
start = source.index('#if defined(_WIN32) && !defined(VITA) && !defined(__PSL1GHT__) && !defined(__PS3__)')
stop = source.index('/* One platform ladder', start)
helpers = source[start:stop]
# The wide name on the stack the wide stat converts into (vfs_wide)
start = source.rindex('#if', 0, source.index('/* Room for a path in UTF-16 on the stack'))
stop = source.index('#endif', source.index('static void vfs_wide_free', start)) + len('#endif')
helpers = source[start:stop] + '\n' + helpers
start = source.index('      /* Windows\n', source.index('static int retro_vfs_stat_full'))
stop = source.index('#elif defined(GEKKO)', start)
body = source[start:stop]
fragment = helpers + '\nstatic int test_stat(const char *path, int64_t *size, int64_t *mtime)\n{\n'
fragment += '   int ret = RETRO_VFS_STAT_IS_VALID;\n   if (!path || !*path) return 0;\n   {\n'
fragment += body + '\n   }\n   return ret;\n}\n'
with tempfile.TemporaryDirectory(prefix='vfs-stat-') as directory:
    path = Path(directory)
    (path / 'win32_stat_fragment.h').write_text(fragment, encoding='utf-8')
    for profile in range(4):
        exe = path / ('stat-' + str(profile) + '.exe')
        flags = ['-O1', '-g1', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                 '-Wno-long-long', '-Wframe-larger-than=2048', '-I' + str(path), '-DPROFILE=' + str(profile)]
        if a.sanitizer:
            flags += ['-fsanitize=' + a.sanitizer, '-fno-omit-frame-pointer']
            if 'undefined' in a.sanitizer.split(','):
                flags += ['-fno-sanitize-recover=undefined']
        subprocess.run(shlex.split(a.cc) + flags + [str(here / 'win32_stat_test.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('PASS wide, legacy ANSI, runtime encoding and VC6 stat branches')
