#!/usr/bin/env python3
"""AVC/AAC packet replay through the production MediaCodec output and real RTMP(S).

Uses independent FFmpeg generation/reception/decoding. Encoder metadata getters
are mocked; this does not establish native MediaCodec or source priming proof.
"""
import argparse
from contextlib import ExitStack
import json
import hashlib
import os
from pathlib import Path
import re
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import traceback


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', type=Path, default=os.environ.get('MC_STREAM_ARTIFACT_DIR'))
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--first-case', action='store_true')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    network = repo / 'libretro-common/samples/net/rtmp'
    sys.path.insert(0, str(network))
    from session_peer import native_ffmpeg
    from tls_relay import TlsRelay
    root = args.artifacts.resolve() if args.artifacts else Path(tempfile.mkdtemp(prefix='mc-wire-'))
    if args.artifacts:
        root.mkdir(parents=True, exist_ok=False)
    evidence = dict(result='running', scope=__doc__, codecs='AVC/AAC', commands=[], cases=[])

    def save():
        (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')

    def command(argv, cwd=None, env=None, timeout=120, check=True):
        entry = dict(arguments=list(map(str, argv)), result='running')
        evidence['commands'].append(entry)
        number = len(evidence['commands'])
        save()
        print('+', shlex.join(entry['arguments']), flush=True)
        try:
            result = subprocess.run(entry['arguments'], cwd=cwd, env=env,
                                    capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            entry['result'] = 'timeout'
            (root / f'command-{number}.stdout').write_bytes(error.stdout or b'')
            (root / f'command-{number}.stderr').write_bytes(error.stderr or b'')
            save()
            raise
        entry['result'] = result.returncode
        (root / f'command-{number}.stdout').write_bytes(result.stdout)
        (root / f'command-{number}.stderr').write_bytes(result.stderr)
        save()
        if check:
            result.check_returncode()
        return result

    try:
        ffmpeg = native_ffmpeg()
        probe = str(Path(ffmpeg).with_name('ffprobe')) if '/' in ffmpeg else 'ffprobe'
        binary = args.binary.resolve() if args.binary else root / ('receiver.exe' if os.name == 'nt' else 'receiver')
        if not args.binary:
            command(['make', 'rtmps_client_native_test', 'SAMPLE_OBJDIR=' + str(root / 'objects')], cwd=network)
            dry = command(['make', '-B', '-n', 'rtmps_client_native_test',
                           'SAMPLE_OBJDIR=' + str(root / 'objects')], cwd=network)
            link = next(shlex.split(line) for line in dry.stdout.decode().splitlines()
                        if ' -o rtmps_client_native_test ' in line)
            link[link.index('-o') + 1] = str(binary)
            link[link.index('rtmp_client_native_test.c')] = str(here / 'stream_receiver_test.c')
            javac_path = shutil.which('javac')
            assert javac_path, 'JDK headers are required'
            javac = Path(javac_path).resolve()
            java = Path(os.environ.get('JAVA_HOME', javac.parents[1]))
            link += ['-Wno-long-long', '-I' + str(java / 'include'),
                     '-I' + str(java / 'include' / ('win32' if os.name == 'nt'
                            else 'darwin' if sys.platform == 'darwin' else 'linux'))]
            link += [str(repo / name) for name in ('record/record_stream_mediacodec.c',
                                                  'record/record_mediacodec_stream_config.c')]
            command(link, cwd=network)
        evidence['binary'] = dict(path=str(binary), supplied=bool(args.binary),
                                  sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
        video = root / 'source.h264'
        audio = root / 'source.aac'
        command([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=64x48:rate=60',
                 '-frames:v', '24', '-c:v', 'libx264', '-profile:v', 'baseline',
                 '-x264-params', 'bframes=0:keyint=12:min-keyint=12:scenecut=0:aud=1:repeat-headers=1',
                 '-f', 'h264', '-y', video])
        command([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'sine=frequency=997:sample_rate=48000',
                 '-t', '0.4', '-ac', '2', '-c:a', 'aac', '-b:a', '128k', '-f', 'adts', '-y', audio])
        data = video.read_bytes()
        starts = list(re.finditer(rb'\x00\x00(?:\x00)?\x01', data))
        nals = [(match.start(), data[match.end()] & 31,
                 data[match.start():starts[i + 1].start() if i + 1 < len(starts) else len(data)])
                for i, match in enumerate(starts)]
        csd = b''.join(next(blob for _, kind, blob in nals if kind == wanted) for wanted in (7, 8))
        boundaries = [at for at, kind, _ in nals if kind == 9] + [len(data)]
        pictures = [data[a:b] for a, b in zip(boundaries, boundaries[1:])]
        assert len(pictures) == 24
        encoded = audio.read_bytes()
        packets = []
        at = 0
        while at < len(encoded):
            assert encoded[at] == 255 and encoded[at + 1] & 240 == 240
            length = ((encoded[at + 3] & 3) << 11) | (encoded[at + 4] << 3) | (encoded[at + 5] >> 5)
            header = 7 if encoded[at + 1] & 1 else 9
            packets.append(encoded[at + header:at + length])
            at += length
        assert at == len(encoded)
        profile = (encoded[2] >> 6) + 1
        rate_index = (encoded[2] >> 2) & 15
        channels = ((encoded[2] & 1) << 2) | (encoded[3] >> 6)
        asc = bytes([(profile << 3) | (rate_index >> 1), ((rate_index & 1) << 7) | (channels << 3)])
        reference_video = command([ffmpeg, '-v', 'error', '-i', video, '-fps_mode', 'passthrough',
                                   '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']).stdout
        reference_audio = command([ffmpeg, '-v', 'error', '-i', audio, '-f', 'f32le', '-']).stdout
        assert len(reference_video) == 24 * 64 * 48 * 3
        cases = [('rtmp', None, False, True), ('legacy', None, True, True),
                 ('tls12', '1.2', False, True), ('tls13', '1.3', False, True),
                 ('video-only', None, False, False)]
        for name, tls, legacy, with_audio in cases[:1] if args.first_case else cases:
            directory = root / name
            directory.mkdir()
            fixture = directory / 'packets.bin'
            media = [(i * 1000000 // 60, 0, 1 if i % 12 == 0 else 0, packet)
                     for i, packet in enumerate(pictures)]
            if with_audio:
                media += [(i * 1024 * 1000000 // 48000, 1, 0, packet)
                          for i, packet in enumerate(packets)]
            with fixture.open('wb') as out:
                out.write(struct.pack('<I', len(csd)) + csd)
                out.write(struct.pack('<I', len(asc) if with_audio else 0))
                if with_audio:
                    out.write(asc)
                for pts, track, flags, payload in sorted(media):
                    out.write(struct.pack('<IIQI', track, flags, pts, len(payload)) + payload)
            with socket.socket() as reserve:
                reserve.bind(('127.0.0.1', 0))
                port = reserve.getsockname()[1]
            url = f'rtmp://127.0.0.1:{port}/live/test'
            received = directory / 'received.flv'
            with ExitStack() as stack, (directory / 'receiver.log').open('w+b') as log:
                server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout', '10',
                                           '-copyts', '-i', url, '-map', '0', '-c', 'copy', '-f', 'flv',
                                           '-y', str(received)], stdin=subprocess.DEVNULL,
                                          stdout=subprocess.DEVNULL, stderr=log)
                try:
                    relay = stack.enter_context(TlsRelay(directory, port, tls)) if tls else None
                    env = os.environ.copy()
                    env.pop('RTMPS_CA', None)
                    if relay:
                        url = f'rtmps://localhost:{relay.port}/live/test'
                        env['RTMPS_CA'] = str(relay.ca)
                    deadline = time.monotonic() + 20
                    while True:
                        result = command([binary, url, fixture, 'legacy' if legacy else 'standard'],
                                         env=env, timeout=30, check=False)
                        if result.returncode != 78 or server.poll() is not None:
                            break
                        if time.monotonic() >= deadline:
                            raise TimeoutError('receiver did not accept connection')
                        time.sleep(0.01)
                    result.check_returncode()
                    assert b'native_mediacodec=0' in result.stdout
                    assert server.wait(timeout=15) == 0
                finally:
                    if server.poll() is None:
                        server.terminate()
                        try:
                            server.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            server.kill()
                            server.wait(timeout=5)
            info = json.loads(command([probe, '-v', 'error', '-show_streams', '-show_packets',
                                       '-of', 'json', received]).stdout)
            video_index = next(s['index'] for s in info['streams'] if s['codec_type'] == 'video')
            assert len(info['streams']) == (2 if with_audio else 1)
            assert next(s for s in info['streams'] if s['index'] == video_index)['codec_name'] == 'h264'
            audio_streams = [s for s in info['streams'] if s['codec_type'] == 'audio']
            assert bool(audio_streams) == with_audio
            coded_video = [p for p in info['packets'] if p['stream_index'] == video_index]
            assert len(coded_video) == 24
            for i, packet in enumerate(coded_video):
                assert abs(float(packet['pts_time']) - i / 60) < .0011
            decoded = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:v:0',
                               '-fps_mode', 'passthrough', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']).stdout
            assert decoded == reference_video
            if with_audio:
                assert audio_streams[0]['codec_name'] == 'aac'
                assert int(audio_streams[0]['sample_rate']) == 48000 and audio_streams[0]['channels'] == 2
                coded_audio = [p for p in info['packets'] if p['stream_index'] == audio_streams[0]['index']]
                assert len(coded_audio) == len(packets)
                for i, packet in enumerate(coded_audio):
                    assert abs(float(packet['pts_time']) - i * 1024 / 48000) < .0011
                decoded_audio = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:a:0',
                                         '-f', 'f32le', '-']).stdout
                assert decoded_audio == reference_audio
            evidence['cases'].append(dict(name=name, video_packets=24,
                                           audio_packets=len(packets) if with_audio else 0,
                                           pixels='byte-identical independent decode',
                                           audio='byte-identical independent decode' if with_audio else None,
                                           timestamps='within FLV millisecond rounding', result='PASS'))
            save()
            print('PASS', name, flush=True)
        evidence['result'] = 'PASS'
    except BaseException:
        evidence['result'] = 'FAIL'
        evidence['failure'] = traceback.format_exc()
        raise
    finally:
        save()
        print('Evidence:', root, flush=True)


if __name__ == '__main__':
    main()
