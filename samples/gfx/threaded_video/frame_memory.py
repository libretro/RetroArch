#!/usr/bin/env python3
"""Exercise renewable frame mappings and bounded cache retirement from shipping C."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
wrapper = (root / 'gfx/video_thread_wrapper.c').read_text(encoding='utf-8')
video = (root / 'gfx/video_driver.c').read_text(encoding='utf-8')
d3d = (root / 'gfx/drivers/d3d11.c').read_text(encoding='utf-8')

def function(source, name):
    match = re.search(r'^(?:static\s+)?(?:INLINE\s+)?(?:bool|void|int|unsigned)\s+\*?' + name + r'\([^;]*?\n\{', source, re.M)
    if not match:
        raise RuntimeError('Missing shipping function: ' + name)
    return source[match.start():source.index('\n}\n', match.start()) + 3]

cc = shlex.split(os.environ.get('CC', 'cc'))
windows = os.name == 'nt' or any('mingw' in part for part in cc)
with tempfile.TemporaryDirectory(prefix='retroarch-frame-memory-') as directory:
    temp = Path(directory)
    start = wrapper.index('typedef struct video_thread_private\n')
    private = wrapper[start:wrapper.index('} video_thread_private_t;', start) + len('} video_thread_private_t;')]
    start = wrapper.index('static uint8_t *video_thread_host_alloc(')
    end = wrapper.index('/* Video thread: the busy frame buffers', start)
    memory = private + '\nstatic bool thread_get_mapped_framebuffer(void*, struct retro_framebuffer*);\n'
    memory += 'static bool thread_get_current_software_framebuffer(void*, struct retro_framebuffer*);\n'
    memory += wrapper[start:end]
    memory += '\n'.join(function(wrapper, name) for name in (
        'video_thread_mem_poll', 'video_thread_mem_poll_mapped', 'video_thread_mem_release', 'video_thread_mem_provision',
        'video_thread_mem_unprovision', 'thread_get_current_software_framebuffer',
        'thread_get_mapped_framebuffer'))
    (temp / 'memory.inc').write_text(memory, encoding='utf-8')
    cache = '\n'.join(function(video, name) for name in (
        'frame_cache_hazard_acquire', 'frame_cache_hazard_release', 'frame_cache_hazard_drain',
        'frame_cache_snapshot', 'frame_cache_store', 'video_driver_cached_frame_read',
        'video_driver_cached_frame_invalidate', 'video_driver_cached_frame_retire',
        'video_driver_cached_frame_try_retire'))
    (temp / 'cache.inc').write_text(cache, encoding='utf-8')
    (temp / 'd3d.inc').write_text('\n'.join(function(d3d, name) for name in (
        'd3d11_format_bytes', 'd3d11_frame_mem_free', 'd3d11_frame_mem_alloc',
        'd3d11_frame_mem_ready', 'd3d11_frame_mem_map', 'd3d11_frame_mem_push',
        'd3d11_sw_lend_push')), encoding='utf-8')
    exe = temp / ('frame.exe' if windows else 'frame')
    flags = ['-std=gnu89', '-O1', '-g1', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-function', '-Wno-unused-parameter', '-Werror=declaration-after-statement',
             '-DHAVE_THREADS', '-DHAVE_MENU', '-I' + str(temp), '-I' + str(root),
             '-I' + str(root / 'libretro-common/include'), '-I' + str(root / 'deps')]
    extra = shlex.split(os.environ.get('CFLAGS', ''))
    if any('fsanitize=thread' in part for part in extra):
        if 'clang' not in subprocess.check_output(cc + ['--version'], text=True).lower():
            flags += ['-Wno-error=tsan']
    subprocess.run(cc + flags + extra + [
        str(Path(__file__).with_name('frame_memory_test.c')),
        str(root / 'libretro-common/rthreads/rthreads.c'),
        str(root / 'libretro-common/memmap/memalign.c'), '-o', str(exe)]
        + (['-lwinmm', '-ld3d11', '-ldxgi', '-luuid'] if windows else ['-lpthread']), check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True, timeout=60)
