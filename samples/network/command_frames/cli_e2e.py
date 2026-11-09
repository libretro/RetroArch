#!/usr/bin/env python3
"""Check --command exit statuses and delivery with a built frontend."""

import argparse
import os
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('retroarch', type=Path)
    parser.add_argument('--runner', default='', help='for example, wine')
    args = parser.parse_args()
    command = shlex.split(args.runner) + [str(args.retroarch.resolve())]

    with tempfile.TemporaryDirectory(prefix='retroarch-command-') as directory:
        work = Path(directory)
        (work / 'retroarch.cfg').write_text(
            'video_driver = "null"\n'
            'audio_driver = "null"\n'
            'audio_enable = "false"\n'
            'input_driver = "null"\n'
            'input_joypad_driver = "test"\n'
            'config_save_on_exit = "false"\n', encoding='utf-8')
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env['WAYLAND_DISPLAY'] = str(work / 'missing-wayland')
        for kind in ('CONFIG', 'CACHE', 'DATA', 'STATE'):
            path = work / kind.lower()
            path.mkdir()
            env['XDG_' + kind + '_HOME'] = str(path)

        def launch(arguments, expected):
            result = subprocess.run(
                command + ['-v', '-c', 'retroarch.cfg'] + arguments,
                cwd=work, env=env, stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                timeout=30)
            output = result.stdout.decode('utf-8', errors='replace')
            if result.returncode != expected:
                raise AssertionError('expected exit %d, got %d\n%s' % (
                    expected, result.returncode, output))
            return output

        def run(value, expected):
            return launch(['--command=' + value], expected)

        output = run('E2E_NOT_A_COMMAND', 1)
        if 'not recognized' not in output:
            raise AssertionError('missing invalid-command diagnostic\n' + output)
        print('[pass] unknown command exits with an error')
        run('', 1)
        print('[pass] empty command exits with an error')
        run('x' * 4096, 1)
        print('[pass] oversized command exits with an error')
        run('VERSION;;99999', 1)
        run('VERSION;;notaport', 1)
        print('[pass] invalid port exits with an error')
        launch(['--menu', 'missing.bin'], 1)
        print('[pass] argument error before the task queue exits cleanly')

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(('127.0.0.1', 0))
            receiver.settimeout(5)
            run('VERSION;127.0.0.1;%d' % receiver.getsockname()[1], 0)
            packet, _ = receiver.recvfrom(4096)
            if packet != b'VERSION':
                raise AssertionError('unexpected command packet: %r' % packet)
        print('[pass] valid command is delivered and exits successfully')


if __name__ == '__main__':
    main()
