#!/usr/bin/env python3
"""Build the pinned standalone validator without replacing system tools."""
import argparse
import os
from pathlib import Path
import subprocess

TOOLS = '3e73ae769e684f17b283250f803a231a830d45e3'
HEADERS = '5c6004e758e0c80185ef2cee93ae5c10fa342a5b'


def run(command, **kwargs):
    subprocess.run(command, check=True, **kwargs)


def checkout(path, repository, revision):
    if not path.exists():
        run(['git', 'init', '--quiet', str(path)])
    probe = subprocess.run(['git', '-C', str(path), 'rev-parse', '--verify', 'HEAD'],
                           capture_output=True, text=True)
    if probe.returncode:
        # An interrupted initial fetch has no HEAD yet. Resume only that
        # empty repository; never discard a different cached checkout.
        run(['git', '-C', str(path), 'fetch', '--depth=1', repository, revision],
            timeout=180)
        run(['git', '-C', str(path), 'checkout', '--detach', revision])
    head = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'],
                                   text=True).strip()
    if head != revision:
        raise RuntimeError(f'Unexpected cached source revision: {path}: {head}')
    run(['git', '-C', str(path), 'diff', '--exit-code', 'HEAD', '--'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache-dir', required=True, type=Path)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error('--jobs must be between 1 and 64')
    root = args.cache_dir.resolve() / TOOLS
    root.mkdir(parents=True, exist_ok=True)
    source, build = root / 'source', root / 'build'
    checkout(source, 'https://github.com/KhronosGroup/SPIRV-Tools.git', TOOLS)
    checkout(source / 'external/spirv-headers',
             'https://github.com/KhronosGroup/SPIRV-Headers.git', HEADERS)
    run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DSPIRV_SKIP_TESTS=ON',
         '-DSPIRV_SKIP_EXECUTABLES=OFF', '-DSPIRV_WERROR=OFF'], timeout=120)
    run(['cmake', '--build', str(build), '--target', 'spirv-val',
         '-j', str(args.jobs)], timeout=600)
    binary = build / 'tools' / ('spirv-val.exe' if os.name == 'nt' else 'spirv-val')
    run([str(binary), '--version'])
    if os.environ.get('GITHUB_PATH'):
        with open(os.environ['GITHUB_PATH'], 'a', encoding='utf-8') as output:
            output.write(str(binary.parent) + '\n')
    print(f'Validator: {binary}', flush=True)


if __name__ == '__main__':
    main()
