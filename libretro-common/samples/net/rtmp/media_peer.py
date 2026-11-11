#!/usr/bin/env python3
"""Independent decode oracle for native RTMP audio/video delivery."""
import argparse
from contextlib import ExitStack
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile
import time

from session_peer import native_ffmpeg
from tls_relay import TlsRelay
from peer import replies
from command import decode as decode_amf


def decode(ffmpeg, path, audio):
    args = ['-map', '0:a:0', '-f', 's16le', '-acodec', 'pcm_s16le'] if audio else [
        '-map', '0:v:0', '-fps_mode', 'passthrough', '-enc_time_base', '1:1000',
        '-pix_fmt', 'yuv420p', '-f', 'rawvideo']
    return subprocess.run([ffmpeg, '-v', 'error', '-i', str(path)] + args + ['-'],
                          check=True, stdout=subprocess.PIPE, timeout=20).stdout


def run(binary, runner, legacy, tls=None, remux=False, caps=False, codec='h264'):
    ffmpeg = native_ffmpeg()
    video_flags = (['-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                    '-crf', '0', '-b:v', '0', '-lag-in-frames', '0', '-auto-alt-ref', '0']
                   if codec == 'av1' else ['-c:v', 'libx264', '-preset', 'medium',
                                          '-crf', '10', '-bf', '2'])
    with tempfile.TemporaryDirectory(prefix='rtmp-media-') as temp:
        root = Path(temp)
        fixture, received = root / 'source.flv', root / 'received.flv'
        subprocess.run([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i',
                        'testsrc2=size=320x240:rate=24:duration=2,'
                        'noise=alls=100:allf=t+u:all_seed=12345', '-f', 'lavfi', '-i',
                        'sine=frequency=997:sample_rate=48000:duration=2',
                        '-pix_fmt', 'yuv420p'] + video_flags + [
                        '-g', '24', '-c:a', 'aac', '-ar', '48000', '-ac', '2',
                        '-t', '2', '-flvflags', 'no_duration_filesize', '-f', 'flv',
                        str(fixture)], check=True, timeout=30)
        probe = str(Path(ffmpeg).with_name('ffprobe' + Path(ffmpeg).suffix))
        info = json.loads(subprocess.run([probe, '-v', 'error', '-show_streams',
                                         '-of', 'json', str(fixture)], check=True,
                                        capture_output=True, timeout=15).stdout)
        video = next(s for s in info['streams'] if s['codec_type'] == 'video')
        audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
        assert video['codec_name'] == codec
        if codec == 'h264':
            assert video['has_b_frames'] > 0
        assert audio['codec_name'] == 'aac' and audio['sample_rate'] == '48000'
        assert fixture.stat().st_size > 3000000, 'fixture must cross the peer credit window'
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1', 0))
            port = reserve.getsockname()[1]
        url = f'rtmp://127.0.0.1:{port}/live'
        with ExitStack() as stack, (root / 'receiver.log').open('w+b') as log:
            server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout',
                                       '10', '-copyts', '-i', url + '/test', '-map', '0',
                                       '-c', 'copy', '-f', 'flv', str(received)],
                                      stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                      stderr=log)
            try:
                relay = stack.enter_context(TlsRelay(root, port, tls, caps)) if tls or caps else None
                client_url = f'{"rtmps" if tls else "rtmp"}://localhost:{relay.port}/live' if relay else url
                client_port = relay.port if relay else port
                env = os.environ.copy()
                if codec == 'av1':
                    env['RTMP_TEST_AV1'] = '1'
                if caps:
                    env['RTMP_TEST_CAPS'] = '1'
                if tls:
                    env['RTMPS_CA'] = str(relay.ca)
                    if 'wine' in shlex.split(runner):
                        env['RTMPS_CA'] = 'Z:' + str(relay.ca).replace('/', '\\')
                deadline = time.monotonic() + 20
                fixture_arg = str(fixture)
                if 'wine' in shlex.split(runner) and fixture.is_absolute():
                    fixture_arg = 'Z:' + str(fixture).replace('/', '\\')
                while True:
                    if time.monotonic() >= deadline:
                        raise TimeoutError('native media publisher did not connect')
                    result = subprocess.run(shlex.split(runner) + [str(Path(binary).resolve()),
                                            client_url, str(client_port),
                                            'legacy' if legacy else 'standard',
                                            fixture_arg] + (['mux'] if remux else []),
                                            timeout=15, capture_output=True, env=env)
                    if result.returncode == 0:
                        break
                    if result.returncode != 77 or server.poll() is not None:
                        raise RuntimeError(result.stdout + result.stderr)
                    time.sleep(0.01)
                if server.wait(timeout=15):
                    raise RuntimeError('independent receiver failed')
                if caps:
                    wire = BytesIO(relay.connect_bytes)
                    assert wire.read(1) == b'\x03'
                    assert len(wire.read(3072)) == 3072
                    _, payload = replies(wire.read, command=b'connect')
                    properties = decode_amf(payload)[2]
                    fourcc = 'av01' if codec == 'av1' else 'avc1'
                    assert properties['fourCcList'] == [fourcc, 'mp4a']
                    assert properties['videoFourCcInfoMap'] == {fourcc: 2}
                    assert properties['audioFourCcInfoMap'] == {'mp4a': 2}
                    assert 'capsEx' not in properties
                    print('PASS: owned client serialized truthful connect capabilities before asynchronous setup')
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
        packets = []
        for path in (fixture, received):
            packets.append(json.loads(subprocess.run([
                probe, '-v', 'error', '-show_packets', '-show_data_hash', 'sha256',
                '-show_entries', 'packet=codec_type,pts,dts,data_hash', '-of', 'json',
                str(path)], check=True, capture_output=True, timeout=15).stdout)['packets'])
        assert packets[0] == packets[1], 'packet bytes or presentation/decode timestamps changed'
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
        print('PASS native ' + ('RTMPS TLS ' + tls if tls else 'RTMP')
              + (' muxed' if remux else '') + ' ' + codec + '/AAC decoded A/V, ' +
              ('legacy' if legacy else 'standard') + ': ' + json.dumps(hashes))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('--tls', choices=['1.2', '1.3'])
    parser.add_argument('--mux', action='store_true')
    parser.add_argument('--caps', action='store_true')
    parser.add_argument('--codec', choices=['h264', 'av1'], default='h264')
    parser.add_argument('binary')
    args = parser.parse_args()
    run(args.binary, args.runner, False, args.tls, args.mux, args.caps, args.codec)
    run(args.binary, args.runner, True, args.tls, args.mux, args.caps, args.codec)
