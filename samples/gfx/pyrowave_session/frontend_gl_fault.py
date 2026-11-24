# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Build a real GL dispatch with a deliberately missing completion marker."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-directory', type=Path, required=True)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sender', action='store_true')
args = parser.parse_args()
if args.output.resolve() == args.retroarch.resolve():
    parser.error('--output must differ from the configured executable')
build = args.build_directory.resolve()
source = build / 'record/record_pyrowave_gl.c'
original = source.read_bytes()
patched = b'#include "../verbosity.h"\n' + original.replace(b'\r\n', b'\n')
name = b'int record_pyrowave_gl_push(' if args.sender else b'int record_pyrowave_gl_decode('
start = patched.index(name)
end = patched.index(b'\n}\n', start) + 3
function = patched[start:end]
marker = b'   s->fence = glFenceSync(0x9117, 0);'
if function.count(marker) != 1:
    raise RuntimeError('Expected one native completion marker in the selected operation')
function = function.replace(b'{\n', b'{\n   static unsigned pyrowave_fault_dispatches;\n', 1)
function = function.replace(marker, b'''   if (++pyrowave_fault_dispatches == 8)
   {
      /* Dispatches above remain real GPU work without their original fence. */
      s->fence = NULL;
      RARCH_LOG("[PyroWave oracle] Recorded decode finalization fault.\\n");
   }
   else s->fence = glFenceSync(0x9117, 0);''')
if args.sender:
    function = function.replace(b'pyrowave_fault_dispatches == 8', b'pyrowave_fault_dispatches == 32')
patched = patched[:start] + function + patched[end:]
try:
    source.write_bytes(patched)
    os.utime(source, None)
    subprocess.run(['make', '-j8', 'GL_DEBUG=1'], cwd=build, check=True)
    args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.retroarch.resolve(), args.output.resolve())
finally:
    source.write_bytes(original)
    os.utime(source, None)
    subprocess.run(['make', '-j8', 'GL_DEBUG=1'], cwd=build, check=True)
print('PYROWAVE_GL_MARKER_FAULT_BUILT_SOURCE_RESTORED', flush=True)
