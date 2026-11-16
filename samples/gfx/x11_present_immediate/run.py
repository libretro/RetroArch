#!/usr/bin/env python3
"""Whether a vsync-off present on X11 tears, as the X contexts and the
Vulkan driver answer it: the shipping functions, cut from their sources
and built against a stand-in Xlib."""
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


common = (root / 'gfx/common/x11_common.c').read_text()
glx = (root / 'gfx/drivers_context/x_ctx.c').read_text()
egl = (root / 'gfx/drivers_context/xegl_ctx.c').read_text()
xvk = (root / 'gfx/drivers_context/x_vk_ctx.c').read_text()
vulkan = (root / 'gfx/drivers/vulkan.c').read_text()

# Each context asks as its window is made, with the bypass it asked for,
# and has the answer in its driver table.
for name, text, table in (('x_ctx', glx, 'gfx_ctx_x_present_immediate\n};'),
                          ('xegl_ctx', egl, 'gfx_ctx_xegl_present_immediate\n};'),
                          ('x_vk_ctx', xvk, 'gfx_ctx_x_vk_present_immediate\n};')):
    i = text.index('_NET_WM_BYPASS_COMPOSITOR')
    assert 'uncomposed = x11_window_uncomposed(' in text[i:i + 700], \
        name + ': the window is not asked about a compositor'
    assert table in text, name + ': no present_immediate in its table'

with tempfile.TemporaryDirectory(prefix='x11-immediate-') as tmp:
    tmp = Path(tmp)
    (tmp / 'shipping.inc').write_text('\n'.join((
        block(common, 'bool x11_window_uncomposed('),
        block(glx, 'static bool gfx_ctx_x_present_immediate('),
        block(egl, 'static bool gfx_ctx_xegl_present_immediate('),
        block(xvk, 'static bool gfx_ctx_x_vk_present_immediate('),
        block(vulkan, 'static bool vulkan_present_immediate('))) + '\n')
    exe = tmp / ('test.exe' if os.environ.get('RUNNER') or os.name == 'nt'
                 else 'test')
    cmd = shlex.split(os.environ.get('CC', 'cc')) \
        + shlex.split(os.environ.get('CFLAGS', '')) \
        + ['-std=c89', '-Wall', '-Wextra', '-Werror',
           '-Werror=declaration-after-statement',
           '-I' + str(root / 'libretro-common/include'), '-I' + str(tmp),
           str(here / 'test.c'), '-o', str(exe)]
    subprocess.run(cmd, stdin=subprocess.DEVNULL, check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)],
                   stdin=subprocess.DEVNULL, check=True)
