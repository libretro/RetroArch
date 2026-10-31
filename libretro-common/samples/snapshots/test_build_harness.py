"""Check that shared build outputs always use the selected harness sources."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--make', default='make')
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--shell')
    args = parser.parse_args()
    makefile = Path(__file__).with_name('Makefile')
    suffix = '.exe' if os.name == 'nt' else ''
    library = '.dll' if os.name == 'nt' else '.so'
    old_time = 946684800

    with tempfile.TemporaryDirectory(prefix='snapshot build ') as temporary:
        base = Path(temporary)
        sources = []
        for name, value in [('cached A', 11), ('cached B', 22)]:
            common = base / name / 'libretro-common'
            tools = common / 'samples/snapshots'
            tools.mkdir(parents=True)
            (common / 'include').mkdir()
            shutil.copyfile(makefile, tools / 'Makefile')
            header = common / 'include/libretro.h'
            header.write_text('#define SNAPSHOT_MARKER %d\n' % value, encoding='utf-8')
            (tools / 'snapshot_smoke.c').write_text(
                '#include "../../include/libretro.h"\n'
                'int snapshot_marker(void) { return SNAPSHOT_MARKER; }\n'
                '#ifndef SNAPSHOT_PROXY\n'
                'int main(void) { return snapshot_marker(); }\n'
                '#endif\n', encoding='utf-8')
            (tools / 'snapshot_proxy.c').write_text(
                '#define SNAPSHOT_PROXY\n#include "snapshot_smoke.c"\n', encoding='utf-8')
            for path in tools.iterdir():
                os.utime(path, (old_time, old_time))
            os.utime(header, (old_time, old_time))
            sources.append(tools)

        def build(tools, success=True):
            command = [args.make, 'BUILD=../../../../output', 'CC=' + args.cc]
            if args.shell:
                command.append('SHELL=' + args.shell)
            result = subprocess.run(command + ['all', 'gpu'], cwd=tools,
                                    capture_output=True, text=True)
            if (result.returncode == 0) != success:
                raise AssertionError(result.stdout + result.stderr)

        def check(value):
            host = base / ('output/snapshot_smoke' + suffix)
            proxy = base / ('output/snapshot_proxy' + library)
            query = ('import ctypes, sys; '
                     'sys.exit(ctypes.CDLL(sys.argv[1]).snapshot_marker())')
            for name, command in [('host', [str(host)]),
                                  ('proxy', [sys.executable, '-c', query, str(proxy)])]:
                actual = subprocess.run(command).returncode
                if actual != value:
                    raise AssertionError('%s: expected source %d, got %d' %
                                         (name, value, actual))

        for index, value in [(0, 11), (1, 22), (0, 11), (0, 11)]:
            build(sources[index])
            check(value)

        header = sources[0].parents[1] / 'include/libretro.h'
        header.write_text('#define SNAPSHOT_MARKER 33\n', encoding='utf-8')
        os.utime(header, (old_time, old_time))
        build(sources[0])
        check(33)

        header.write_text('#error rejected harness source\n', encoding='utf-8')
        os.utime(header, (old_time, old_time))
        build(sources[0], success=False)
        header.write_text('#define SNAPSHOT_MARKER 44\n', encoding='utf-8')
        os.utime(header, (old_time, old_time))
        build(sources[0])
        check(44)

    print('Harness host/proxy revision, local edit and failed-build retry: passed')


if __name__ == '__main__':
    main()
