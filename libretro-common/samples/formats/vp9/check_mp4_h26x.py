#!/usr/bin/env python3
"""Check MP4 visible-picture clocks against an independent decoder."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from h26x_fixture import executable, hevc_nonoutput_fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--runner', default=os.environ.get('RUNNER', ''))
    args = parser.parse_args()
    runner = shlex.split(args.runner)
    stream = executable('./rmp4_h26x_stream_test')
    hardware = executable('./rmp4_h26x_hw_test')
    passed = []
    with tempfile.TemporaryDirectory(prefix='mp4-h26x-') as directory:
        root = Path(directory)
        for reordered, discard in ((False, False), (True, False), (True, True)):
            movie, reference, times = hevc_nonoutput_fixture(args.ffmpeg, root,
                    reordered=reordered, discard_prior=discard)
            mp4 = root / 'fixture.mp4'
            subprocess.run([args.ffmpeg, '-v', 'error', '-y', '-i', str(movie),
                            '-map', '0:v:0', '-c', 'copy', '-movflags', '+faststart',
                            str(mp4)], check=True)
            subprocess.run(runner + [stream, str(mp4), str(reference), str(times)],
                           check=True)
            subprocess.run(runner + [hardware, str(mp4), str(times)], check=True)
            passed.append({'reordered': reordered, 'discard_prior': discard})
    print(json.dumps({'passed': passed, 'native_gpu_acceptance': False}))


if __name__ == '__main__':
    main()
