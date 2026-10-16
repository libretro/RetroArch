#!/usr/bin/env python3
"""Build the real Vulkan import functions with a headless GPU oracle."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
subprocess.run([sys.executable, str(Path(__file__).with_name('generate_ui_roundtrip.py')), '--check'], check=True)
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
    for signature in ('static bool vulkan_import_buffer(',
                      'static struct vk_stream_state *vulkan_import_state_get(',
                      'static enum video_texture_update vulkan_import_convert(',
                      'static bool vulkan_get_device(',
                      'static void vulkan_movie_input('):
        bodies.append(function(source, signature))
    (tmp / 'import_functions.inc').write_text('\n'.join(bodies))
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=gnu89'))
    windows = 'mingw' in cc[0] or os.name == 'nt'
    sample = Path(__file__).parent
    sources = [str(sample / 'probe.c')]
    defines = ['-DVULKAN_IMPORT_TEST', '-DHAVE_VULKAN']
    if len(sys.argv) > 2 and sys.argv[2] == '--hevc-render':
        defines = ['-DHEVC_DECODE_TEST', '-DHEVC_RENDER_TEST', '-DHAVE_VULKAN',
                   '-DHAVE_RMP4', '-DHAVE_RH264', '-DHAVE_RVP9', '-DHAVE_RAV1']
        sources += [str(sample / 'hevc_render.c'),
                    str(root / 'libretro-common/formats/h265/rh265.c'),
                    str(root / 'libretro-common/formats/mp4/rmp4.c'),
                    str(root / 'libretro-common/formats/h264/rh264.c'),
                    str(root / 'libretro-common/formats/vp9/rvp9.c'),
                    str(root / 'libretro-common/formats/av1/rav1_hw.c'),
                    str(root / 'libretro-common/formats/mp4/rmp4_video.c'),
                    str(root / 'libretro-common/formats/vp8/rvp8.c'),
                    str(root / 'libretro-common/formats/image/image_blit_bands.c'),
                    str(root / 'libretro-common/formats/image/image_hdr_blit.c'),
                    str(root / 'libretro-common/features/features_cpu.c'),
                    str(root / 'libretro-common/time/rtime.c')]
        sources += [str(root / 'libretro-common' / path) for path in (
            'compat/compat_strl.c',
            'compat/compat_strcasestr.c',
            'compat/fopen_utf8.c',
            'encodings/encoding_utf.c',
            'encodings/encoding_crc32.c',
            'file/file_path.c',
            'file/file_path_io.c',
            'formats/data_transfer.c',
            'formats/webm/rwebm.c',
            'formats/webm/rwebm_video.c',
            'lists/string_list.c',
            'memmap/memmap.c',
            'memmap/memalign.c',
            'streams/file_stream.c',
            'string/rstrtod.c',
            'string/stdstring.c',
            'vfs/vfs_implementation.c',
        )]
    if len(sys.argv) <= 2:
        sources.append(str(root / 'libretro-common/formats/image/image_hdr_blit.c'))
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root)] + defines
        + ['-o', str(Path(sys.argv[1]).resolve())] + sources + [
        str(root / 'libretro-common/vulkan/vulkan_symbol_wrapper.c'),
        str(root / 'libretro-common/formats/image/image_yuv_blit.c')]
        + shlex.split(os.environ.get('LDFLAGS', '')) + ['-lm']
        + ([] if windows else ['-ldl']), check=True)
