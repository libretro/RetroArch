#!/usr/bin/env python3
"""Check owned HEVC parameters and the shipping VideoToolbox setup helper."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def metal_test_source():
    source = (ROOT / 'gfx/drivers/metal.m').read_text(encoding='utf8')
    header = (ROOT / 'gfx/video_driver.h').read_text(encoding='utf8')
    text = '#include <formats/video_hw_frame.h>\n#include <formats/image_yuv_blit.h>\n'
    text += '#include <defines/cocoa_defines.h>\n'
    text += 'enum video_texture_update' + header.split('enum video_texture_update', 1)[1].split('};', 1)[0] + '};\n'
    text += 'typedef struct video_frame_import\n' + header.split('typedef struct video_frame_import\n', 1)[1].split('} video_frame_import_t;', 1)[0] + '} video_frame_import_t;\n'
    for section in ('LEASE', 'CV', 'CV_POOL', 'CV_POOL_SETUP', 'SDR_PIPELINE', 'SDR_CONVERT'):
        text += source.split('/* METAL_FRAME_' + section + '_BEGIN */', 1)[1].split('/* METAL_FRAME_' + section + '_END */', 1)[0]
    text = text.replace('#include "../common/metal_frame.h"', '#include "' + (ROOT / 'gfx/common/metal_frame.h').as_posix() + '"')
    text = text.replace('#include "../common/metal_frame_convert.h"', '#include "' + (ROOT / 'gfx/common/metal_frame_convert.h').as_posix() + '"')
    methods = ''
    for part in ('HISTORY', 'FRONT', 'TEXTURE'):
        methods += source.split('/* METAL_FRAME_' + part + '_BEGIN */', 1)[1].split('/* METAL_FRAME_' + part + '_END */', 1)[0]
    methods = methods.replace('- (void)_updateHistory\n{', '- (void)_updateHistory\n{\n   history_updates++;')
    methods += source.split('/* METAL_MOVIE_CLEAR_BEGIN */', 1)[1].split('/* METAL_MOVIE_CLEAR_END */', 1)[0]
    methods += source.split('/* METAL_MOVIE_FRAME_BEGIN */', 1)[1].split('/* METAL_MOVIE_FRAME_END */', 1)[0]
    movie = (HERE / 'movie_test.m').read_text(encoding='utf8')
    frame_view = (HERE.parent / 'metal_frame/frame_view_fixture.m').read_text(encoding='utf8')
    movie = movie.replace('#include "movie_reference.h"', '#include "' + (HERE / 'movie_reference.h').as_posix() + '"')
    text += movie.replace('/* SHIPPING_FRAME_VIEW */', frame_view.replace('/* SHIPPING_FRAME_METHODS */', methods))
    text += '#include "' + (ROOT / 'gfx/common/metal_hevc_driver.h').as_posix() + '"\n'
    text += (HERE.parent / 'metal_frame/pool_test.m').read_text(encoding='utf8')
    return (HERE / 'metal_test.m').read_text(encoding='utf8').replace('/* SHIPPING_METAL */', text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--threadless', action='store_true')
    parser.add_argument('--apple-limits', action='store_true')
    parser.add_argument('--metal', action='store_true')
    args = parser.parse_args()
    if args.metal and (not args.native or args.threadless):
        parser.error('--metal requires --native and threaded slots')
    with tempfile.TemporaryDirectory(prefix='videotoolbox-hevc-') as name:
        tmp = Path(name)
        flags = ['-O2', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                 '-Wframe-larger-than=4096', '-I' + str(ROOT / 'libretro-common/include')]
        libs = ['-lm']
        sdk_flags = []
        if args.apple_limits:
            limits = tmp / 'apple_limits.h'
            limits.write_text('#include <stdint.h>\n#undef UINT64_MAX\n'
                              '#define UINT64_MAX 18446744073709551615ULL\n'
                              '#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8\n'
                              '#undef SIZE_MAX\n#define SIZE_MAX UINT64_MAX\n#endif\n', encoding='utf8')
            sdk_flags = ['-include', str(limits), '-Werror=long-long']
        if args.native:
            flags += ['-DVT_HEVC_NATIVE', '-mmacosx-version-min=10.14']
            libs += ['-framework', 'CoreMedia', '-framework', 'CoreVideo',
                     '-framework', 'VideoToolbox', '-framework', 'CoreFoundation']
        else:
            for framework in ('CoreMedia', 'CoreVideo', 'VideoToolbox'):
                folder = tmp / framework
                folder.mkdir()
                (folder / (framework + '.h')).write_text(
                    '#include "' + (HERE / 'mock_apple.h').as_posix() + '"\n'
                    + ('#if defined(__APPLE__)\nstatic inline int vt_sdk_inline(void) { return 0; }\n#endif\n'
                       if framework == 'CoreMedia' else ''), encoding='utf8')
            flags += ['-I' + str(tmp)]
        flags += shlex.split(os.environ.get('CFLAGS', ''))
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        reference = tmp / ('reference.exe' if os.name == 'nt' else 'reference')
        cmd = compiler + flags + [str(HERE / 'movie_reference_test.c'), '-o', str(reference), '-lm']
        print('+', shlex.join(cmd), flush=True)
        subprocess.run(cmd, check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(reference)], check=True)

        exe = tmp / ('setup.exe' if os.name == 'nt' else 'setup')
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        obj = tmp / 'rh265.o'
        decoder_flags = ['-Wno-unused-parameter', '-Wframe-larger-than=2048']
        # boolean.h uses Clang's stdbool.h, including in the C89 decoder build.
        if 'clang' in subprocess.check_output(compiler + ['--version'], text=True).lower():
            decoder_flags += ['-Wno-c99-extensions']
            flags += ['-Wno-c99-extensions']
        cmd = compiler + flags + decoder_flags + sdk_flags + [
            '-c', str(ROOT / 'libretro-common/formats/h265/rh265.c'), '-o', str(obj)]
        print('+', shlex.join(cmd), flush=True)
        subprocess.run(cmd, check=True)
        unit_flags = flags + ([] if args.threadless else ['-DHAVE_THREADS'])
        if args.apple_limits:
            probe = tmp / 'apple_probe.c'
            probe.write_text('#include "' + (ROOT / 'gfx/common/videotoolbox_hevc_backend.h').as_posix()
                             + '"\n', encoding='utf8')
            cmd = compiler + unit_flags + sdk_flags + ['-c', str(probe), '-o', str(tmp / 'apple_probe.o')]
            print('+', shlex.join(cmd), flush=True)
            subprocess.run(cmd, check=True)
            if not args.native:
                probe.write_text('#include "' + (ROOT / 'gfx/common/videotoolbox_hevc.h').as_posix()
                    + '"\n#ifdef inline\n#error SDK inline alias leaked\n#endif\n'
                    'int vt_sdk_probe(void) { return vt_sdk_inline(); }\n', encoding='utf8')
                cmd = compiler + flags + sdk_flags + ['-D__APPLE__', '-c', str(probe),
                    '-o', str(tmp / 'apple_inline.o')]
                print('+', shlex.join(cmd), flush=True)
                subprocess.run(cmd, check=True)
            if sys.platform.startswith('linux'):
                signed = tmp / 'apple_int64.h'
                signed.write_text('#include <stdint.h>\n#undef INT64_C\n'
                                  '#define INT64_C(v) v##LL\n', encoding='utf8')
                cmd = compiler + flags + ['-Wno-unused-parameter', '-Wframe-larger-than=2048',
                    '-Werror=long-long', '-D_POSIX_C_SOURCE=200809L', '-U__linux__',
                    '-include', str(signed), '-c', str(ROOT / 'libretro-common/rthreads/rthreads.c'),
                    '-o', str(tmp / 'apple_pthread.o')]
                print('+', shlex.join(cmd), flush=True)
                subprocess.run(cmd, check=True)
            if not args.native and not args.threadless:
                # Compile the native adapter control flow against framework
                # declarations; this does not execute Metal or prove pixels.
                cmd = compiler + unit_flags + ['-DVT_HEVC_METAL', '-c',
                    str(HERE / 'setup_test.c'), '-o', str(tmp / 'adapter_control.o')]
                print('+', shlex.join(cmd), flush=True)
                subprocess.run(cmd, check=True)
        objects = [str(obj)]
        if args.metal:
            # Verify startup remains legal below the shared-event OS gate.
            probe = tmp / 'bootstrap.m'
            probe.write_text('#include "' + (ROOT / 'gfx/common/metal_hevc_driver.h').as_posix()
                + '"\nstatic bool metal_hevc_decode_enabled(void *p) { (void)p; return true; }\n'
                  'int bootstrap_probe(void *o, void *d, void *p) { return metal_hevc_start(o, d, p); }\n', encoding='utf8')
            cmd = compiler + ['-DHAVE_THREADS', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra',
                '-Werror', '-Wno-unused-function', '-Werror=unguarded-availability',
                '-mmacosx-version-min=10.13', '-I' + str(ROOT / 'libretro-common/include'),
                '-c', str(probe), '-o', str(tmp / 'bootstrap.o')]
            print('+', shlex.join(cmd), flush=True)
            subprocess.run(cmd, check=True)
            unit = tmp / 'metal.m'
            unit.write_text(metal_test_source(), encoding='utf8')
            metal_obj = tmp / 'metal.o'
            cmd = compiler + ['-DHAVE_THREADS', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-function', '-Wframe-larger-than=4096', '-mmacosx-version-min=10.14',
                '-I' + str(ROOT / 'libretro-common/include'), '-c', str(unit), '-o', str(metal_obj)]
            print('+', shlex.join(cmd), flush=True)
            subprocess.run(cmd, check=True)
            objects.append(str(metal_obj))
            unit_flags += ['-DVT_HEVC_METAL']
            libs += ['-framework', 'Foundation', '-framework', 'Metal']
        if not args.threadless:
            for source in ('rthreads/rthreads.c', 'rthreads/retro_eventcount.c'):
                thread_obj = tmp / (Path(source).stem + '.o')
                cmd = compiler + unit_flags + ['-Wno-unused-parameter', '-c',
                    str(ROOT / 'libretro-common' / source), '-o', str(thread_obj)]
                print('+', shlex.join(cmd), flush=True)
                subprocess.run(cmd, check=True)
                objects.append(str(thread_obj))
            libs += ['-lpthread']
        cmd = compiler + unit_flags + [str(HERE / 'setup_test.c')] + objects + ['-o', str(exe)] + libs
        print('+', shlex.join(cmd), flush=True)
        subprocess.run(cmd, check=True)
        result = subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=False, timeout=180)
        if not result.returncode and args.apple_limits:
            print('VideoToolbox C89: Apple-style limits compile probes and fixture passed', flush=True)
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
