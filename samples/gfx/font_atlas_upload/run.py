#!/usr/bin/env python3
"""Run the font atlas uploads of the GX2, GXM, RSX, D3D10 and D3D11 drivers,
extracted from the shipping sources, against textures in memory that
are exactly their size, and against a D3D11 device whose texture
creation can be made to fail."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, name):
    """The definition of @name: the first mention whose line, or the one
    before, starts 'static' and that opens a body before any ';'."""
    at = 0
    while True:
        at = source.index(name, at) + 1
        line = source.rindex('\n', 0, at) + 1
        prev = source.rindex('\n', 0, line - 1) + 1
        start = line if source.startswith('static', line) else prev
        if not source.startswith('static', start):
            continue
        opening = source.index('{', at)
        if source.find(';', at, opening) >= 0:
            continue
        depth, end = 1, opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end] + '\n'


parts = {
    'gx2.inc':   ('gfx/drivers/gx2_gfx.c',
                  ('gx2_font_invalidate_sprites(', 'gx2_font_upload_dirty(')),
    'gxm.inc':   ('gfx/drivers/gxm_gfx.c', ('gxm_font_upload_dirty(',)),
    'rsx.inc':   ('gfx/drivers/rsx_gfx.c', ('rsx_font_copy_dirty(',)),
    'd3d11.inc': ('gfx/drivers/d3d11.c', ('d3d11_font_make_texture(',)),
    'd3d11_rects.inc': ('gfx/drivers/d3d11.c',
                        ('d3d11_font_update_atlas_rects(',)),
    'd3d10_rects.inc': ('gfx/drivers/d3d10.c',
                        ('d3d10_font_update_atlas_rects(',)),
}

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    for inc, (path, names) in parts.items():
        source = (root / path).read_text(encoding='utf-8')
        (tmp / inc).write_text(''.join(function(source, n) for n in names))
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS',
        '-O1 -g -std=c89 -Wall -Wextra -Werror=declaration-after-statement'))
    exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
    subprocess.run(cc + flags + ['-I' + str(tmp),
                   '-I' + str(root / 'libretro-common/include'),
                   str(Path(__file__).with_name('test.c')), '-o', str(exe)],
                   check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)],
                   check=True)
