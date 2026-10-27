#!/usr/bin/env python3
"""Exercise production frontend, D3D and Vulkan movie handoff decisions."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[3]
def body(source, signature):
    start = source.index(signature)
    while source.index(';', start) < source.index('{', start):
        start = source.index(signature, start + len(signature))
    return source[start:source.index('\n}', start) + 2]
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    source = (root / 'gfx/video_driver.c').read_text(encoding='utf-8')
    (tmp / 'front.inc').write_text('\n'.join(body(source, s) for s in
        ('bool video_driver_movie_refresh(', 'void video_driver_movie_clear(',
         'enum video_texture_update video_driver_texture_import(')))
    source = (root / 'gfx/video_thread_wrapper.c').read_text(encoding='utf-8')
    start = source.index('      if (n->kind == VIDEO_THREAD_ASYNC_IMPORT)',
                         source.index('static void video_thread_async_run('))
    end = source.index('      else if (n->kind == VIDEO_THREAD_ASYNC_UPDATE)', start)
    (tmp / 'async_import.inc').write_text(source[start:end])
    (tmp / 'wrapper.inc').write_text('\n'.join(body(source, sig) for sig in
        ('bool video_thread_movie_supported(',
         'static enum video_texture_update thread_import_movie_frame(')))
    for api in ('d3d11', 'd3d12', 'vulkan'):
        source = (root / 'gfx/drivers' / (api + '.c')).read_text(encoding='utf-8')
        (tmp / (api + '.inc')).write_text('\n'.join(body(source, s) for s in
            ('static enum video_texture_update ' + api + '_import_movie_frame(void *data,',
             'static const void *' + api + '_movie_prepare(')))
    with (tmp / 'vulkan.inc').open('a', encoding='utf-8') as output:
        output.write('\n' + body(source, 'static void vulkan_movie_input('))
    source = (root / 'gfx/video_driver.h').read_text(encoding='utf-8')
    start = source.index('typedef struct video_frame_import\n')
    (tmp / 'frame.inc').write_text(source[start:source.index('} video_frame_import_t;', start) + len('} video_frame_import_t;')])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    exe = tmp / ('test.exe' if 'mingw' in cc[0] or os.name == 'nt' else 'test')
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        '-DHAVE_THREADS', '-D_GNU_SOURCE', str(Path(__file__).with_name('test.c'))] +
        [str(root / ('libretro-common/' + p)) for p in
         ('rthreads/rthreads.c', 'rthreads/retro_eventcount.c',
          'features/features_cpu.c', 'time/rtime.c')] +
        ['-lpthread', '-lm'] + (['-lwinmm'] if 'mingw' in cc[0] or os.name == 'nt' else []) +
        ['-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    source = (root / 'gfx/gfx_surface_player.c').read_text(encoding='utf-8')
    (tmp / 'surface.inc').write_text('\n'.join(body(source, s) for s in
        ('static int gfx_surface_player_start_hw(',
         'static enum video_player_present gfx_surface_player_present_hw(',
         'bool gfx_surface_player_release(',
         'static int gfx_surface_player_restart_software(',
         'gfx_surface_player_t *gfx_surface_player_new(')))
    with (tmp / 'surface.inc').open('a', encoding='utf-8') as output:
        output.write('\n' + body((root / 'gfx/gfx_thumbnail.c').read_text(encoding='utf-8'),
            'static void gfx_thumbnail_anim_open_probed('))
    for threaded in (False, True):
        exe = tmp / (('surface-threaded' if threaded else 'surface') +
            ('.exe' if 'mingw' in cc[0] or os.name == 'nt' else ''))
        subprocess.run(cc + flags + (['-DHAVE_THREADS'] if threaded else []) +
            ['-I' + str(tmp), '-I' + str(root),
             '-I' + str(root / 'libretro-common/include'),
             str(Path(__file__).with_name('surface_output_test.c')),
             '-o', str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
