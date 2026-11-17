#!/usr/bin/env python3
"""Display Pacing On VBlank Events on Wayland: the connector a window's
output names, the CRTC behind it found in DRM, and whether the
compositor let a present tear. The shipping functions, cut from their
sources (test.c) or included as they are (connector.c), against
stand-ins for Wayland and for the kernel."""
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
    if text[end:end + 1] == ';':
        end += 1
    return text[start:end]


common = (root / 'gfx/common/wayland_common.c').read_text()
present = (root / 'gfx/common/wayland_present.c').read_text()
inputc = (root / 'input/common/wayland_common.c').read_text()
serv = (root / 'gfx/display_servers/dispserv_wl.c').read_text()
egl = (root / 'gfx/drivers_context/wayland_ctx.c').read_text()
vk = (root / 'gfx/drivers_context/wayland_vk_ctx.c').read_text()

# Both contexts answer with the common function, and the display server
# has the clock in its table.
for name, text in (('wayland_ctx', egl), ('wayland_vk_ctx', vk)):
    assert 'gfx_ctx_wl_present_immediate_common\n};' in text, \
        name + ': no present_immediate in its table'
assert '   wl_display_server_wait_vblank,\n' in serv, \
    'dispserv_wl: no wait_vblank in its table'
# The window's connector is told as its output changes, as an output is
# named, and is none once the context is gone.
report = block(inputc, 'static void wl_report_window_output(')
assert 'gfx_ctx_wl_window_connector_set(' in report
destroy = block(common, 'void gfx_ctx_wl_destroy_resources_common(')
assert 'gfx_ctx_wl_window_connector_set(0);' in destroy

with tempfile.TemporaryDirectory(prefix='wl-vblank-') as tmp:
    tmp = Path(tmp)
    (tmp / 'shipping.inc').write_text('\n'.join((
        block(common, 'static const char *const wl_connector_kinds[]'),
        'static retro_atomic_int_t wl_window_connector;',
        block(common, 'unsigned gfx_ctx_wl_connector_code('),
        block(serv, 'void gfx_ctx_wl_window_connector_set(unsigned code)\n{'),
        block(serv, 'unsigned gfx_ctx_wl_window_connector(void)\n{'),
        block(common, 'bool gfx_ctx_wl_present_immediate_common('),
        block(present, 'static void wl_present_feedback_presented('),
        block(inputc, 'static void wl_output_handle_name('),
        'static unsigned     wl_vblank_connector;',
        block(serv, 'static void wl_vblank_close('),
        block(serv, 'static unsigned wl_display_server_wait_vblank('))) + '\n')
    runner = shlex.split(os.environ.get('RUNNER', ''))
    for test, flags in (('test', ['-std=c89', '-Werror=declaration-after-statement']),
                        ('connector', ['-std=gnu89', '-Werror=declaration-after-statement'])):
        if test == 'connector' and (runner or os.name == 'nt'):
            # the kernel's own headers: Linux only
            continue
        exe = tmp / (test + ('.exe' if runner or os.name == 'nt' else ''))
        cmd = shlex.split(os.environ.get('CC', 'cc')) \
            + shlex.split(os.environ.get('CFLAGS', '')) \
            + flags + ['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                       '-I' + str(root / 'libretro-common/include'),
                       '-I' + str(root), '-I' + str(tmp),
                       str(here / (test + '.c')), '-o', str(exe)]
        subprocess.run(cmd, stdin=subprocess.DEVNULL, check=True)
        subprocess.run(runner + [str(exe)], stdin=subprocess.DEVNULL, check=True)
