#!/usr/bin/env python3
"""Check overlap-safe loan repacking against an immutable source snapshot."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/common/d3d9_sw_fb.h').read_text(encoding='utf-8')
start = source.index('static INLINE void d3d9_sw_fb_settle(')
end = source.index('\n}\n', start) + 3
has_rows = 'd3d9_sw_fb_repack_rows(' in source
if has_rows:
    start = source.index('#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))', 0, start)
code = source[start:end]
cc = shlex.split(os.environ.get('CC', 'cc'))
windows = os.name == 'nt' or any('mingw' in part for part in cc)
with tempfile.TemporaryDirectory(prefix='retroarch-d3d9-repack-') as directory:
    temp = Path(directory)
    flags = ['-std=gnu89', '-O2', '-Wall', '-Wextra', '-Werror',
             '-Werror=declaration-after-statement', '-I' + str(temp),
             '-I' + str(root), '-I' + str(root / 'libretro-common/include')]
    if has_rows:
        flags += ['-DTEST_REPACK_ROWS']
    for stack_rows in ([False] if windows else [False, True]):
        selected = code
        if stack_rows:
            # Check the Windows x86 branch under Linux sanitizers too.
            selected = selected.replace(
                '#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))', '#if 1')
        (temp / 'settle.inc').write_text(selected, encoding='utf-8')
        exe = temp / ('repack.exe' if windows else 'repack')
        subprocess.run(cc + flags + shlex.split(os.environ.get('CFLAGS', ''))
                       + [str(Path(__file__).with_name('repack_test.c')), '-o', str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True, timeout=60)
