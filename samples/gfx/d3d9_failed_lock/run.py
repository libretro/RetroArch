#!/usr/bin/env python3
"""Exercise production D3D9 lock failures and ribbon mesh replacement."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/d3d9hlsl.c').read_text()

def block(start):
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

vertex = block(source.index('   if (pass->last_dims != VIDEO_SCALE_PACK(width, height))'))
start = source.index('   d3d->menu->alpha_mod = alpha;',
                     source.index('static void d3d9_hlsl_set_menu_texture_frame('))
end = source.index('\n}', start)
menu = source[start:end]
texture = block(source.index('static void d3d9_hlsl_video_texture_load_d3d('))
readback = block(source.index('static bool d3d9_hlsl_read_viewport('))
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    for name, text in [('vertex', vertex), ('menu', menu),
                       ('texture', texture), ('readback', readback)]:
        (tmp / (name + '.inc')).write_text(text)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
                   str(Path(__file__).with_name('test.c')), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
    for backend in ('hlsl', 'cg'):
        source = (root / ('gfx/drivers/d3d9' + backend + '.c')).read_text()
        start = source.index('static void gfx_display_d3d9_' + backend + '_draw_pipeline(')
        start = source.index('      case VIDEO_SHADER_MENU_2:', start)
        (tmp / 'ribbon.inc').write_text(block(start))
        prefix = 'd3d9_' + backend + '_menu_pipeline_'
        declarations = '\n'.join(line for line in source.splitlines()
                                 if line.startswith('static ') and prefix in line)
        (tmp / 'ribbon_state.inc').write_text(declarations)
        subprocess.run(cc + flags + ['-I' + str(tmp),
                       '-I' + str(root / 'libretro-common/include'),
                       '-D' + prefix + 'vbo=pipeline_vbo',
                       '-D' + prefix + 'mesh_id=pipeline_mesh_id',
                       str(Path(__file__).with_name('ribbon_test.c')),
                       '-o', str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
        print('D3D9 ' + backend + ' ribbon mesh replacement PASS', flush=True)
