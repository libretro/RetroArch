#!/usr/bin/env python3
"""Run the production descriptor manager (pool, alloc, per-frame reset)
against a recording fake device."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]


def function(source, name):
    masked = re.sub(r'/\*.*?\*/|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    match = re.search(r'\nstatic [^;{(]*?\b' + re.escape(name)
                      + r'\s*\([^;]*?\)\s*\{', masked)
    if not match:
        raise RuntimeError('function missing: ' + name)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start() + 1:end]


source = (root / 'gfx/drivers/vulkan.c').read_text(encoding='latin-1')
cut = '\n\n'.join(function(source, name) for name in (
    'vulkan_alloc_descriptor_pool',
    'vulkan_descriptor_manager_alloc',
    'vulkan_descriptor_manager_reset'))
with tempfile.TemporaryDirectory(prefix='vulkan-descriptor-manager-') as tmp:
    tmp = Path(tmp)
    (tmp / 'production_manager.h').write_text(cut + '\n',
                                              encoding='latin-1')
    exe = tmp / 'descriptor_manager_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
        '-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(here / 'descriptor_manager_test.c'), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.run(
        shlex.split(os.environ.get('RUNNER', '')) + [str(exe)]).returncode)
