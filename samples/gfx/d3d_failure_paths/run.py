#!/usr/bin/env python3
"""Run production D3D10/11 map and swapchain guards against failing APIs."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def block(source, start):
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

s10 = (root / 'gfx/drivers/d3d10.c').read_text()
s11 = (root / 'gfx/drivers/d3d11.c').read_text()
upload = block(s10, s10.index('static void d3d10_update_texture('))
start = s10.index('d3d10->frame.ubo->lpVtbl->Map',
                  s10.index('static void d3d10_gfx_set_rotation('))
rotation = s10[s10.rfind('\n', 0, start) + 1:]
rotation = rotation[:rotation.index('\n}')]
frame10 = s10.index('static bool d3d10_gfx_frame(')
resize10 = block(s10, s10.index('   if (d3d10->flags & D3D10_ST_FLAG_RESIZE_CHAIN)', frame10))
frame11 = s11.index('static bool d3d11_gfx_frame_body(')
start = s11.index('      UINT swapchain_flags', frame11)
end = s11.index('      video_driver_set_output_dims(', start)
end = s11.index(';', end) + 1
resize11 = s11[start:end]
start = s11.index('       D3D11Texture2D back_buffer', end)
buffer11 = block(s11, s11.rfind('{', 0, start))
hdr = []
for match in re.finditer(r'D3D11_MAPPED_SUBRESOURCE mapped_ubo;', s11[frame11:]):
    start = frame11 + match.start()
    hdr.append(block(s11, s11.rfind('{', frame11, start)))
assert len(hdr) == 3

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    for name, text in [('upload', upload), ('rotation', rotation),
                       ('resize10', resize10), ('resize11', resize11),
                       ('buffer11', buffer11)] + list(zip(('hdr0', 'hdr1', 'hdr2'), hdr)):
        (tmp / (name + '.inc')).write_text(text)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    for hdr_enabled in (0, 1):
        exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
        subprocess.run(cc + flags + (['-DHAVE_DXGI_HDR'] if hdr_enabled else []) +
                       ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
                        str(Path(__file__).with_name('test.c')),
                        '-o', str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
