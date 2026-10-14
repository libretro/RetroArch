#!/usr/bin/env python3
"""Compile the production import shaders and check their GPU color output."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    shaders = []
    for api, name in [('d3d11', 'd3d11_planar_cs_src'), ('d3d12', 'd3d12_nv12_cs_src')]:
        source = (root / 'gfx/drivers' / (api + '.c')).read_text()
        start = source.index('static const char ' + name + '[] =')
        shaders.append(source[start:source.index(';\n', start) + 1])
    (tmp / 'shaders.inc').write_text('\n'.join(shaders))
    source = (root / 'gfx/drivers/d3d11.c').read_text()
    start = source.index('static bool d3d11_planar_convert(')
    (tmp / 'convert.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static int d3d11_import_tex(')
    (tmp / 'import.inc').write_text(source[start:source.index('\n}', start) + 2])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    windows = 'mingw' in cc[0] or os.name == 'nt'
    exe = tmp / ('test.exe' if windows else 'test')
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe)] + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
