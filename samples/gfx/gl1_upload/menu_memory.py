#!/usr/bin/env python3
"""Exercise GL1's shipping menu-memory ownership and upload paths."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / 'gfx/drivers/gl1.c').read_text()


def function(name):
    match = re.search(re.escape(name) + r'[^;{]*\{', driver)
    assert match, name
    return driver[match.start():driver.index('\n}\n', match.end()) + 3]


with tempfile.TemporaryDirectory(prefix='gl1-menu-memory-') as directory:
    temp = Path(directory)
    start = driver.index('typedef struct gl1_menu_mem\n')
    end = driver.index('} gl1_menu_mem_t;', start) + len('} gl1_menu_mem_t;')
    (temp / 'memory_type.inc').write_text(driver[start:end])
    end = driver.index('GL1_FLAG_MENU_SIZE_CHANGED')
    start = driver.rindex('enum', 0, end)
    (temp / 'flags.inc').write_text(driver[start:driver.index('\n};', end) + 3])
    wrapper = (root / 'gfx/video_thread_wrapper.h').read_text()
    count = re.search(r'^#define VIDEO_THREAD_MENU_MEM \d+', wrapper, re.M).group()
    (temp / 'memory.inc').write_text(count + '\n' + '\n'.join(function(name) for name in (
        'static gl1_menu_mem_t *gl1_menu_mem_find(',
        'static void *gl1_menu_mem_alloc(',
        'static bool gl1_menu_mem_ready(',
        'static void gl1_menu_mem_free(',
        'static void gl1_set_texture_frame(')))
    start = driver.index('enum gl1_src_fmt\n')
    (temp / 'source_format.inc').write_text(driver[start:driver.index('\n};', start) + 3])
    (temp / 'upload.inc').write_text('\n'.join(function(name) for name in (
        'static void gl1_tex_store(', 'static void gl1_draw_tex(')))
    start = driver.index('   if (gl1->menu_source && menu_is_alive)')
    (temp / 'draw.inc').write_text(driver[start:driver.index('\n#ifdef HAVE_OVERLAY', start)])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = ['-O1', '-g1', '-std=c89', '-Wall', '-Wextra', '-Werror=declaration-after-statement', '-Werror=implicit-function-declaration',
             '-DHAVE_THREADS', '-I' + str(temp), '-I' + str(root / 'libretro-common/include')]
    flags += shlex.split(os.environ.get('CFLAGS', ''))
    windows = os.name == 'nt' or 'mingw' in cc[0]
    runner = shlex.split(os.environ.get('RUNNER', ''))
    for name in ('menu_memory_test', 'menu_memory_pixels'):
        exe = temp / (name + ('.exe' if windows else ''))
        libs = (['-lopengl32', '-lgdi32'] if windows else ['-lEGL', '-lGL']) if name.endswith('pixels') else []
        sources = [str(Path(__file__).with_name(name + '.c')),
                   str(root / 'libretro-common/memmap/memalign.c')]
        if name.endswith('pixels'):
            sources += [str(root / 'libretro-common/gfx/scaler/pixconv.c')]
        subprocess.run(cc + flags + sources + ['-o', str(exe)] + libs, check=True)
        subprocess.run(runner + [str(exe)], check=True)
