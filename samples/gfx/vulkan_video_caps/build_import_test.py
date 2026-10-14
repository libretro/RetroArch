#!/usr/bin/env python3
"""Build the real Vulkan import functions with a headless GPU oracle."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/vulkan.c').read_text()
common = (root / 'gfx/common/vulkan_common.c').read_text()

def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]

types = []
for name in ('vk_texture', 'vk_stream_state'):
    start = source.index('struct ' + name + '\n')
    types.append(source[start:source.index('\n};', start) + 3])
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'import_types.inc').write_text('\n'.join(types))
    bodies = [function(common, 'uint32_t vulkan_find_memory_type_fallback(')]
    for signature in ('static struct vk_stream_state *vulkan_import_state_get(',
                      'static enum video_texture_update vulkan_import_convert(',
                      'static bool vulkan_get_device('):
        bodies.append(function(source, signature))
    (tmp / 'import_functions.inc').write_text('\n'.join(bodies))
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=gnu89'))
    windows = 'mingw' in cc[0] or os.name == 'nt'
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root),
        '-DVULKAN_IMPORT_TEST', '-DHAVE_VULKAN', '-o', str(Path(sys.argv[1]).resolve()),
        str(Path(__file__).with_name('probe.c')),
        str(root / 'libretro-common/vulkan/vulkan_symbol_wrapper.c'),
        str(root / 'libretro-common/formats/image/image_yuv_blit.c')]
        + shlex.split(os.environ.get('LDFLAGS', '')) + ['-lm']
        + ([] if windows else ['-ldl']), check=True)
