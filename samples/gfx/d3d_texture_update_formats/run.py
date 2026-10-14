#!/usr/bin/env python3
"""Run D3D10's production load/update with a forced RGBA fallback."""
from pathlib import Path
import os, re, shlex, subprocess, tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / 'gfx/drivers/d3d10.c').read_text()
copy = (root / 'gfx/common/dxgi_common.c').read_text()

def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]

copy = function(copy, 'void dxgi_copy(')
formats = sorted(set(re.findall(r'DXGI_FORMAT[_A-Z0-9]+', copy)))
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / 'formats.inc').write_text('typedef enum { ' + ', '.join(formats) + ' } DXGI_FORMAT;\n')
    (tmp / 'copy.inc').write_text(copy)
    bodies = [function(driver, s) for s in (
        'static void d3d10_update_texture(',
        'static uintptr_t d3d10_gfx_load_texture_internal(',
        'static enum video_texture_update d3d10_gfx_update_texture_internal(')]
    (tmp / 'driver.inc').write_text('\n'.join(bodies))
    cc = shlex.split(os.environ.get('CC', 'cc'))
    exe = tmp / ('test.exe' if 'mingw' in cc[0] or os.name == 'nt' else 'test')
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test.c')), str(root / 'libretro-common/gfx/scaler/pixconv.c'),
        '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
