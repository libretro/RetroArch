#!/usr/bin/env python3
"""Link the configured non-Qt frontend's objects with a settings test main."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--runtime', action='store_true',
                        help='build opt-in XR output test frontend')
    parser.add_argument('--runner', help='executable runner, for example wine')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    dry = subprocess.check_output(
        ['make', '-Bn', '-o', 'config.mk', '-o', 'config.h'],
        cwd=root, text=True)
    commands = [shlex.split(line) for line in dry.splitlines() if line.strip()]
    compile_cmd = next((cmd for cmd in commands
                        if '-c' in cmd and 'retroarch.c' in cmd and '-o' in cmd), None)
    if not compile_cmd:
        parser.error('cannot find retroarch.c compile command; build a non-Qt frontend first')
    original = compile_cmd[compile_cmd.index('-o') + 1]
    link_cmd = next((cmd for cmd in reversed(commands)
                     if original in cmd and '-o' in cmd and '-c' not in cmd), None)
    if not link_cmd or not (root / original).is_file():
        parser.error('build the configured non-Qt frontend first')
    if any('ui_qt.o' in token for token in link_cmd):
        parser.error('build a frontend configured with --disable-qt')
    target = link_cmd[link_cmd.index('-o') + 1]
    suffix = '.exe' if target.endswith('.exe') or (root / (target + '.exe')).is_file() else ''
    binary = output / ('video_screens_test' + suffix)
    objects = [output / 'retroarch_nomain.o', output / 'harness_main.o']
    for obj, source, rename in (
            (objects[0], 'retroarch.c', True),
            (objects[1], str(Path(__file__).with_name('video_screens_test.c').resolve()), False)):
        cmd = list(compile_cmd)
        cmd[cmd.index('-o') + 1] = str(obj)
        cmd[cmd.index('retroarch.c')] = source
        for flag in ('-MF', '-MT', '-MQ'):
            if flag in cmd:
                cmd[cmd.index(flag) + 1] = str(obj.with_suffix('.d') if flag == '-MF' else obj)
        if rename:
            cmd.append('-Dmain=rarch_harness_unused_main')
            if args.runtime:
                cmd.append('-Drunloop_iterate=video_screens_test_iterate')
        elif args.runtime:
            cmd.append('-DVIDEO_SCREENS_RUNTIME_TEST')
        subprocess.run(cmd, cwd=root, check=True)
    cmd = []
    for token in link_cmd:
        if token == original:
            cmd.extend(map(str, objects))
        else:
            cmd.append(token)
    cmd[cmd.index('-o') + 1] = str(binary)
    subprocess.run(cmd, cwd=root, check=True)
    print('built ' + str(binary), flush=True)
    if not args.build_only and not args.runtime:
        env = os.environ.copy()
        for name, directory in (('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'),
                                ('XDG_DATA_HOME', 'data'), ('XDG_STATE_HOME', 'state')):
            path = output / directory
            path.mkdir(exist_ok=True)
            env[name] = str(path)
        fixture = output / 'fixtures'
        fixture.mkdir(exist_ok=True)
        runner = shlex.split(args.runner) if args.runner else []
        fixture_arg = str(fixture)
        if runner and suffix == '.exe':
            env.setdefault('WINEPREFIX', str(output / 'wine-prefix'))
            fixture_arg = subprocess.check_output(
                ['winepath', '-w', str(fixture)], env=env, text=True).strip()
            frontend_dir = subprocess.check_output(
                ['winepath', '-w', str(root)], env=env, text=True).strip()
            env['WINEPATH'] = frontend_dir + ';' + env.get('WINEPATH', '')
        subprocess.run(runner + [str(binary), fixture_arg], cwd=output, env=env, check=True)


if __name__ == '__main__':
    main()
