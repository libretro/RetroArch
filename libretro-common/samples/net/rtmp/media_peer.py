#!/usr/bin/env python3
"""Independent decode oracle for native RTMP audio/video delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile
import time

from session_peer import native_ffmpeg


def decode(ffmpeg, path, audio):
    args = ['-map', '0:a:0', '-f', 's16le', '-acodec', 'pcm_s16le'] if audio else [
        '-map', '0:v:0', '-pix_fmt', 'yuv420p', '-f', 'rawvideo']
    return subprocess.run([ffmpeg, '-v', 'error', '-i', str(path)] + args + ['-'],
                          check=True, stdout=subprocess.PIPE, timeout=20).stdout


def run(binary, runner, legacy):
    ffmpeg = native_ffmpeg()
    with tempfile.TemporaryDirectory(prefix='rtmp-media-') as temp:
        root = Path(temp)
        fixture, received = root / 'source.flv', root / 'received.flv'
        subprocess.run([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i',
                        'testsrc2=size=320x240:rate=24:duration=2,'
                        'noise=alls=100:allf=t+u:all_seed=12345', '-f', 'lavfi', '-i',
                        'sine=frequency=997:sample_rate=48000:duration=2',
                        '-c:v', 'libx264', '-preset', 'medium', '-pix_fmt', 'yuv420p',
                        '-crf', '10', '-g', '24', '-bf', '2', '-c:a', 'aac', '-ar', '48000', '-ac', '2',
                        '-t', '2', '-flvflags', 'no_duration_filesize', '-f', 'flv',
                        str(fixture)], check=True, timeout=30)
        probe = str(Path(ffmpeg).with_name('ffprobe' + Path(ffmpeg).suffix))
        info = json.loads(subprocess.run([probe, '-v', 'error', '-show_streams',
                                         '-of', 'json', str(fixture)], check=True,
                                        capture_output=True, timeout=15).stdout)
        video = next(s for s in info['streams'] if s['codec_type'] == 'video')
        audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
        assert video['codec_name'] == 'h264' and video['has_b_frames'] > 0
        assert audio['codec_name'] == 'aac' and audio['sample_rate'] == '48000'
        assert fixture.stat().st_size > 3000000, 'fixture must cross the peer credit window'
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1', 0))
            port = reserve.getsockname()[1]
        url = f'rtmp://127.0.0.1:{port}/live'
        with (root / 'receiver.log').open('w+b') as log:
            server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout',
                                       '10', '-copyts', '-i', url + '/test', '-map', '0',
                                       '-c', 'copy', '-f', 'flv', str(received)],
                                      stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                      stderr=log)
            try:
                deadline = time.monotonic() + 20
                fixture_arg = str(fixture)
                if 'wine' in shlex.split(runner) and fixture.is_absolute():
                    fixture_arg = 'Z:' + str(fixture).replace('/', '\\')
                while True:
                    if time.monotonic() >= deadline:
                        raise TimeoutError('native media publisher did not connect')
                    result = subprocess.run(shlex.split(runner) + [str(Path(binary).resolve()),
                                            url, str(port), 'legacy' if legacy else 'standard',
                                            fixture_arg], timeout=15, capture_output=True)
                    if result.returncode == 0:
                        break
                    if result.returncode != 77 or server.poll() is not None:
                        raise RuntimeError(result.stdout + result.stderr)
                    time.sleep(0.01)
                if server.wait(timeout=15):
                    raise RuntimeError('independent receiver failed')
            except Exception as error:
                if isinstance(error, subprocess.TimeoutExpired):
                    print((error.stdout or b'').decode(errors='replace'))
                    print((error.stderr or b'').decode(errors='replace'))
                log.seek(0)
                print(log.read().decode(errors='replace'))
                raise
            finally:
                if server.poll() is None:
                    server.kill()
                server.wait(timeout=5)
        hashes = {}
        for audio in (False, True):
            expected, actual = decode(ffmpeg, fixture, audio), decode(ffmpeg, received, audio)
            if actual != expected:
                print(result.stdout.decode(errors='replace'))
                print(result.stderr.decode(errors='replace'))
                print((root / 'receiver.log').read_text(errors='replace'))
            assert expected and actual == expected, ('audio' if audio else 'video',
                                                    len(expected), len(actual))
            if not audio:
                assert len(actual) == 48 * 320 * 240 * 3 // 2
            hashes['audio' if audio else 'video'] = hashlib.sha256(actual).hexdigest()
        print('PASS native RTMP H.264/AAC decoded A/V, ' +
              ('legacy' if legacy else 'standard') + ': ' + json.dumps(hashes))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    run(args.binary, args.runner, False)
    run(args.binary, args.runner, True)
