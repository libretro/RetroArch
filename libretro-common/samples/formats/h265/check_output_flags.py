#!/usr/bin/env python3
"""Compare HEVC hidden/discarded output with an independent decoder."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location(
    'fixtures', Path(__file__).resolve().parents[1] / 'vp9/h26x_fixture.py')
fixtures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixtures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tests', nargs='+')
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--runner', default=os.environ.get('RUNNER', ''))
    args = parser.parse_args()
    tests = [fixtures.executable(test) for test in args.tests]
    runner = shlex.split(args.runner)
    passed = []
    with tempfile.TemporaryDirectory(prefix='hevc-output-flags-') as directory:
        root = Path(directory)
        for reordered, discard in ((False, False), (True, False), (True, True)):
            movie, _, times = fixtures.hevc_nonoutput_fixture(
                args.ffmpeg, root, reordered=reordered, discard_prior=discard)
            mp4, reference = root / 'fixture.mp4', root / 'reference.yuv'
            subprocess.run([args.ffmpeg, '-v', 'error', '-y', '-i', str(movie),
                            '-c:v', 'copy', str(mp4)], check=True)
            subprocess.run([args.ffmpeg, '-v', 'error', '-y', '-i', str(mp4),
                            '-fps_mode', 'passthrough', '-f', 'rawvideo',
                            '-pix_fmt', 'yuv420p', str(reference)], check=True)
            if reference.stat().st_size != 24 * 64 * 64 * 3 // 2:
                raise ValueError('Independent decoder did not output 24 pictures')
            for test in tests:
                for contexts in (1, 4):
                    command = runner + [test, str(mp4), str(reference), str(times)]
                    if contexts == 4:
                        command.append('4')
                    subprocess.run(command, check=True)
                    passed.append({'test': test, 'discard': discard, 'contexts': contexts, 'reordered': reordered})
    print(json.dumps({'passed': passed}))


if __name__ == '__main__':
    main()
