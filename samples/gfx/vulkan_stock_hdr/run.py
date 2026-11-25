#!/usr/bin/env python3
"""Run the production stock view draw, its HDR encode and the decisions
that send stock views straight into a second output or an HDR swapchain,
with no FP16 image between, against a recording
fake, and compile stock_rect_hdr.frag when glslc is installed."""
from pathlib import Path
import os
import re
import shlex
import shutil
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
shaders = root / 'gfx/drivers/vulkan_shaders'


def mask(source):
    return re.sub(r'/\*.*?\*/|"(?:\\.|[^"\\])*"',
                  lambda m: ' ' * len(m[0]), source, flags=re.S)


def function(source, name):
    masked = mask(source)
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


def typedef(source, name):
    masked = mask(source)
    end = masked.index('} ' + name + ';')
    start = masked.rindex('typedef struct', 0, end)
    return source[start:end] + '} ' + name + ';'


source = (root / 'gfx/drivers/vulkan.c').read_text(encoding='latin-1')
types = typedef(source, 'vulkan_hdr_uniform_t')
cut = '\n\n'.join(function(source, name) for name in (
        'vulkan_hdr_ubo_mode',
        'vulkan_views_stock_encodes',
        'vulkan_views_draw_stock',
        'vulkan_window_encodes',
        'vulkan_views_target_stock',
        'vulkan_window_stock_sdr',
        'vulkan_window_stock_encodes'))

if shutil.which('glslc'):
    with tempfile.TemporaryDirectory(prefix='stock-rect-hdr-') as tmp:
        subprocess.run(['glslc', '-mfmt=c', '-o',
                        str(Path(tmp) / 'stock_rect_hdr.frag.inc'),
                        str(shaders / 'stock_rect_hdr.frag')], check=True)
    print('[pass] stock_rect_hdr.frag compiles')

with tempfile.TemporaryDirectory(prefix='vulkan-stock-hdr-') as tmp:
    tmp = Path(tmp)
    (tmp / 'production_types.h').write_text(types + '\n',
                                            encoding='latin-1')
    (tmp / 'production_stock_hdr.h').write_text(cut + '\n',
                                                encoding='latin-1')
    exe = tmp / 'stock_hdr_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
        '-Wno-long-long', '-Wno-unused-function',
        '-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(here / 'stock_hdr_test.c'), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.run(
        shlex.split(os.environ.get('RUNNER', '')) + [str(exe)]).returncode)
