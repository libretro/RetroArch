#!/usr/bin/env python3
"""Run the production vulkan_run_hdr_pipeline() against recording stubs:
each HDR pass in a command buffer must read its own uniform values."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]


def function(source, name):
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    match = re.search(r'static (?:bool|void) ' + re.escape(name)
                      + r'\([^;]*?\)\s*\{', masked)
    if not match:
        raise RuntimeError('function missing: ' + name)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


source = (root / 'gfx/drivers/vulkan.c').read_text()
cut = '\n\n'.join(function(source, name) for name in (
    'vulkan_hdr_ubo_mode', 'vulkan_run_hdr_pipeline'))
with tempfile.TemporaryDirectory(prefix='vulkan-hdr-ubo-') as tmp:
    tmp = Path(tmp)
    (tmp / 'production_hdr.h').write_text(cut + '\n')
    exe = tmp / 'hdr_ubo_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
        '-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(here / 'hdr_ubo_test.c'), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.run(
        shlex.split(os.environ.get('RUNNER', '')) + [str(exe)]).returncode)
