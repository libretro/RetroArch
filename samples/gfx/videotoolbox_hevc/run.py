#!/usr/bin/env python3
"""Check owned HEVC parameters and the shipping VideoToolbox setup helper."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--threadless', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='videotoolbox-hevc-') as name:
        tmp = Path(name)
        flags = ['-O2', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                 '-Wframe-larger-than=4096', '-I' + str(ROOT / 'libretro-common/include')]
        libs = ['-lm']
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
        cmd = compiler + flags + decoder_flags + [
            '-c', str(ROOT / 'libretro-common/formats/h265/rh265.c'), '-o', str(obj)]
        print('+', shlex.join(cmd), flush=True)
        subprocess.run(cmd, check=True)
        unit_flags = flags + ([] if args.threadless else ['-DHAVE_THREADS'])
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
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
