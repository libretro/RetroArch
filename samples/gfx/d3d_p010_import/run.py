#!/usr/bin/env python3
"""Compile the production import shaders and check their GPU color output."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[3]
subprocess.run([__import__('sys').executable, str(root / 'tools/generate_video_hdr.py'), '--check'], check=True)
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    shaders = []
    for api, name in [('d3d11', 'd3d11_planar_cs_src'), ('d3d12', 'd3d12_nv12_cs_src')]:
        source = (root / 'gfx/drivers' / (api + '.c')).read_text()
        start = source.index('static const char ' + name + '[] =')
        shaders.append(source[start:source.index(';\n', start) + 1].replace(
            '#include "../drivers_shader/video_hdr.inc"',
            (root / 'gfx/drivers_shader/video_hdr.inc').read_text()))
    (tmp / 'shaders.inc').write_text('\n'.join(shaders))
    source = (root / 'gfx/drivers/d3d11.c').read_text()
    start = source.index('static bool d3d11_planar_convert(')
    (tmp / 'convert.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static int d3d11_import_tex(')
    (tmp / 'import.inc').write_text(source[start:source.index('\n}', start) + 2])
    source12 = (root / 'gfx/drivers/d3d12.c').read_text()
    bodies = []
    for signature in ('static void d3d12_import_forget(', 'static int d3d12_import_tex(',
                      'static ID3D12Fence *d3d12_import_fence(', 'static bool d3d12_import_list(',
                      'static void d3d12_import_transition(',
                      'static enum video_texture_update d3d12_import_texture('):
        start = source12.index(signature)
        bodies.append(source12[start:source12.index('\n}', start) + 2])
    (tmp / 'import12.inc').write_text('\n'.join(bodies))
    start = source12.index('typedef struct d3d12_import\n')
    (tmp / 'types12.inc').write_text(source12[start:source12.index('} d3d12_import_t;', start) + len('} d3d12_import_t;')])
    video = (root / 'gfx/video_driver.h').read_text()
    start = video.index('typedef struct video_frame_import\n')
    (tmp / 'frame.inc').write_text(video[start:video.index('} video_frame_import_t;', start) + len('} video_frame_import_t;')])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    windows = 'mingw' in cc[0] or os.name == 'nt'
    exe = tmp / ('test.exe' if windows else 'test')
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe)] + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)

    exe12 = tmp / ('test12.exe' if windows else 'test12')
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test12.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe12)] + (['-ld3d11', '-ld3d12', '-ld3dcompiler', '-ldxgi'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe12)], check=True)
