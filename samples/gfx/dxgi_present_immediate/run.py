#!/usr/bin/env python3
"""Whether a vsync-off present goes straight to the scanout, as the
D3D11, D3D12 and Vulkan drivers answer it: the shipping functions, cut from
their sources and built against a stand-in swap chain and output."""
from pathlib import Path
import os, shlex, subprocess, tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]


def block(text, name):
    start = text.index(name)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


dxgi = (root / 'gfx/common/dxgi_common.c').read_text()
d3d11 = (root / 'gfx/drivers/d3d11.c').read_text()
d3d12 = (root / 'gfx/drivers/d3d12.c').read_text()
vulkan = (root / 'gfx/drivers/vulkan.c').read_text()

# The chain's resize is where the window may have changed outputs: the
# answer is asked for again after it.
for name, text in (('d3d11', d3d11), ('d3d12', d3d12)):
    flag = name.upper() + '_ST_FLAG_'
    i = text.index('&= ~(' + flag + 'RESIZE_CHAIN')
    assert flag + 'FLIPS_ASKED' in text[i:i + 120], \
        name + ': a resized chain does not ask its output again'

with tempfile.TemporaryDirectory(prefix='dxgi-immediate-') as tmp:
    tmp = Path(tmp)
    (tmp / 'shipping.inc').write_text('\n'.join((
        block(dxgi, 'bool dxgi_chain_flips_fullscreen('),
        block(d3d11, 'enum d3d11_state_flags') + ';',
        block(d3d12, 'enum d3d12_video_flags') + ';',
        block(d3d11, 'static bool d3d11_present_immediate('),
        block(d3d12, 'static bool d3d12_present_immediate('))) + '\n')
    (tmp / 'vulkan.inc').write_text(
        block(vulkan, 'static bool vulkan_present_immediate(') + '\n')
    for test in ('test', 'vulkan'):
        exe = tmp / (test + ('.exe' if os.environ.get('RUNNER')
                             or os.name == 'nt' else ''))
        cmd = shlex.split(os.environ.get('CC', 'cc')) \
            + shlex.split(os.environ.get('CFLAGS', '')) \
            + ['-std=c89', '-Wall', '-Wextra', '-Werror',
               '-Werror=declaration-after-statement',
               '-I' + str(root / 'libretro-common/include'), '-I' + str(tmp),
               str(here / (test + '.c')), '-o', str(exe)]
        subprocess.run(cmd, stdin=subprocess.DEVNULL, check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', ''))
                       + [str(exe)], stdin=subprocess.DEVNULL, check=True)
