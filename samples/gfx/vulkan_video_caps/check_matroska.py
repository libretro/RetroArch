#!/usr/bin/env python3
"""Generate and compare native Matroska AVC/HEVC output against software."""
import argparse
from contextlib import nullcontext
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'h26x_fixtures', ROOT / 'libretro-common/samples/formats/vp9/h26x_fixture.py')
FIXTURES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vulkan-test')
    parser.add_argument('--d3d11-test')
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--runner', default=os.environ.get('RUNNER', ''))
    parser.add_argument('--codec', choices=('h264', 'hevc', 'both'), default='both')
    parser.add_argument('--mp4', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--fixture-dir', type=Path)
    args = parser.parse_args()
    if not args.vulkan_test and not args.d3d11_test:
        parser.error('Provide --vulkan-test or --d3d11-test')
    backends = [(name, FIXTURES.executable(path)) for name, path in
                (('vulkan', args.vulkan_test), ('d3d11', args.d3d11_test)) if path]
    runner = shlex.split(args.runner)
    results = []

    def check(movie, color, name):
        container = 'MP4' if args.mp4 else 'Matroska'
        if args.mp4:
            converted = movie.with_suffix('.mp4')
            subprocess.run([args.ffmpeg, '-v', 'error', '-y', '-i', str(movie),
                            '-map', '0:v:0', '-c', 'copy', '-movflags', '+faststart',
                            str(converted)], check=True)
            movie = converted
        for backend, executable in backends:
            env = os.environ.copy()
            if backend == 'vulkan':
                env.pop('H26X_TEST_WEBM', None)
                env.pop('H26X_TEST_MP4', None)
                env['H26X_TEST_MP4' if args.mp4 else 'H26X_TEST_WEBM'] = str(movie)
                if color:
                    env['H26X_TEST_COLOR'] = color
                else:
                    env.pop('H26X_TEST_COLOR', None)
                command = runner + [executable]
                marker = f'{container} native Vulkan: 24 displayed pictures, exact planes and timing PASS'
            else:
                command = runner + [executable, str(movie)] + ([color] if color else [])
                marker = f'{container} native D3D11: 24 software frames, 24 GPU frames, PASS'
            process = subprocess.run(command, env=env, capture_output=True,
                                     text=True, encoding='utf-8', errors='replace')
            seeks = not args.mp4 or (f'MP4 native {"Vulkan" if backend == "vulkan" else "D3D11"}: '
                    '3 seek pictures, exact planes and timing PASS') in process.stdout
            status = ('skip' if process.returncode == 77 else
                      'pass' if process.returncode == 0 and marker in process.stdout and seeks else 'fail')
            results.append({'backend': backend, 'container': container, 'case': name, 'status': status,
                            'fixture': str(movie), 'sha256': hashlib.sha256(movie.read_bytes()).hexdigest(),
                            'returncode': process.returncode, 'stdout': process.stdout,
                            'stderr': process.stderr})
            print(f'{status} {backend}/{name}', flush=True)

    if args.fixture_dir:
        args.fixture_dir.mkdir(parents=True, exist_ok=True)
    with (nullcontext(str(args.fixture_dir.resolve())) if args.fixture_dir else
          tempfile.TemporaryDirectory(prefix='native-matroska-')) as directory:
        root = Path(directory)
        codecs = ('h264', 'hevc') if args.codec == 'both' else (args.codec,)
        for codec in codecs:
            for depth in ((8, 10) if codec == 'hevc' else (8,)):
                color = 'pq' if depth == 10 else '709'
                for variable in (False, True):
                    name = f'{codec}-{depth}-' + ('vfr' if variable else 'cfr')
                    movie = root / (name + '.mkv')
                    command = [args.ffmpeg, '-v', 'error', '-f', 'lavfi', '-i',
                               'testsrc2=size=512x512:rate=24']
                    if variable:
                        command += ['-vf', 'setpts=(N+floor(N/3))/24/TB']
                    command += ['-frames:v', '24', '-fps_mode', 'vfr', '-threads', '1',
                                '-pix_fmt', 'yuv420p10le' if depth == 10 else 'yuv420p']
                    if codec == 'h264':
                        command += ['-c:v', 'libx264', '-bf', '3', '-g', '12',
                                    '-x264-params', 'colorprim=bt709:transfer=bt709:colormatrix=bt709',
                                    '-color_primaries', 'bt709', '-color_trc', 'bt709',
                                    '-colorspace', 'bt709']
                    else:
                        command += ['-color_primaries', 'bt2020' if depth == 10 else 'bt709',
                                    '-color_trc', 'smpte2084' if depth == 10 else 'bt709',
                                    '-colorspace', 'bt2020nc' if depth == 10 else 'bt709']
                        cicp = ':colorprim=9:transfer=16:colormatrix=9' if depth == 10 else \
                               ':colorprim=1:transfer=1:colormatrix=1'
                        command += ['-c:v', 'libx265', '-x265-params',
                                    'pools=none:frame-threads=1:bframes=3:keyint=12:log-level=error' + cicp]
                    subprocess.run(command + ['-y', str(movie)], check=True)
                    check(movie, color, name)
                    sps = root / (name + '-sps.mkv')
                    sps.write_bytes(FIXTURES.hide_element(movie.read_bytes(), b'\x55\xb0'))
                    check(sps, color, name + '-sps')
        if 'hevc' in codecs:
            for reordered, discard in ((False, False), (True, False), (True, True)):
                name = 'hevc-nonoutput' + ('-reordered' if reordered else '') \
                    + ('-discard-prior' if discard else '')
                directory = root / name
                directory.mkdir(exist_ok=True)
                movie, _, _ = FIXTURES.hevc_nonoutput_fixture(
                    args.ffmpeg, directory, 512, reordered=reordered, discard_prior=discard)
                check(movie, None, name)
    report = {'passed': sum(row['status'] == 'pass' for row in results),
              'skipped': sum(row['status'] == 'skip' for row in results),
              'failed': sum(row['status'] == 'fail' for row in results), 'results': results}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key != 'results'}))
    return 1 if report['failed'] else 77 if report['skipped'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
