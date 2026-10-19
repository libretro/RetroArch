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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--threadless', action='store_true')
    parser.add_argument('--apple-limits', action='store_true')
    args = parser.parse_args()
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
                    '#include "' + (HERE / 'mock_apple.h').as_posix() + '"\n', encoding='utf8')
            flags += ['-I' + str(tmp)]
        flags += shlex.split(os.environ.get('CFLAGS', ''))
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
            probe.write_text('#include "' + (ROOT / 'gfx/common/videotoolbox_hevc_slots.h').as_posix()
                             + '"\n', encoding='utf8')
            cmd = compiler + unit_flags + sdk_flags + ['-c', str(probe), '-o', str(tmp / 'apple_probe.o')]
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
        objects = [str(obj)]
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
