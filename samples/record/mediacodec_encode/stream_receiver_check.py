#!/usr/bin/env python3
"""AVC/HEVC/AV1 and AAC through the production MediaCodec output and real RTMP(S).

Uses independent FFmpeg generation/reception/decoding. Encoder metadata getters
are mocked. --worker adds actual AAC calibration, PCM feeding and worker
draining; neither mode establishes native MediaCodec device acceptance.
"""
import argparse
from array import array
from contextlib import ExitStack
import json
import hashlib
import math
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
    parser.add_argument('--worker', action='store_true', help='exercise the real worker and AAC calibration')
    parser.add_argument('--codec', choices=['avc', 'hevc', 'hevc10', 'av1'], default='avc')
    parser.add_argument('--source-rate', type=float)
    args = parser.parse_args()
    if args.source_rate is not None and (not args.worker or not 8000 <= args.source_rate <= 192000
                                        or args.source_rate == 48000):
        parser.error('--source-rate requires worker mode and a distinct supported source rate')
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
    evidence['worker'] = args.worker
    evidence['codecs'] = args.codec + '/AAC'
    evidence['source_rate'] = args.source_rate or 48000

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
            link[link.index('rtmp_client_native_test.c')] = str(here / (
                'stream_worker_receiver_test.c' if args.worker else 'stream_receiver_test.c'))
            javac_path = shutil.which('javac')
            assert javac_path, 'JDK headers are required'
            javac = Path(javac_path).resolve()
            java = Path(os.environ.get('JAVA_HOME', javac.parents[1]))
            link += ['-Wno-long-long', '-I' + str(java / 'include'),
                     '-I' + str(java / 'include' / ('win32' if os.name == 'nt'
                            else 'darwin' if sys.platform == 'darwin' else 'linux'))]
            link += [str(repo / name) for name in ('record/record_stream_mediacodec.c',
                                                  'record/record_mediacodec_stream_config.c')]
            if args.worker:
                link += ['-DHAVE_RAAC', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections']
                link += [str(repo / name) for name in (
                    'record/record_mediacodec_worker.c', 'record/record_aac_probe.c',
                    'record/record_mux_mediacodec.c',
                    'record/record_pcm_resample.c',
                    'libretro-common/audio/resampler/drivers/sinc_resampler_int16.c',
                    'libretro-common/memmap/memalign.c',
                    'record/record_aac_stream_timing.c', 'record/record_audio_queue.c',
                    'libretro-common/queues/retro_spsc.c', 'libretro-common/formats/aac/raac.c')]
            command(link, cwd=network)
        evidence['binary'] = dict(path=str(binary), supplied=bool(args.binary),
                                  sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
        hevc = args.codec in ('hevc', 'hevc10')
        av1 = args.codec == 'av1'
        video = root / ('source.ivf' if av1 else 'source.hevc' if hevc else 'source.h264')
        audio = root / 'source.aac'
        video_encoding = (['-c:v', 'libx265', '-pix_fmt', 'yuv420p10le' if args.codec == 'hevc10' else 'yuv420p',
                           '-x265-params', 'bframes=0:keyint=12:min-keyint=12:scenecut=0:aud=1:repeat-headers=1:pools=none:frame-threads=1' +
                           (':colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc' if args.codec == 'hevc10' else ''),
                           '-f', 'hevc'] if hevc else ['-c:v', 'libx264', '-profile:v', 'baseline',
                 '-x264-params', 'bframes=0:keyint=12:min-keyint=12:scenecut=0:aud=1:repeat-headers=1',
                 '-f', 'h264'])
        if av1:
            video_encoding = ['-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '1',
                              '-lag-in-frames', '0', '-g', '12', '-crf', '30',
                              '-color_primaries', 'bt709', '-color_trc', 'bt709',
                              '-colorspace', 'bt709', '-color_range', 'tv',
                              '-bsf:v', 'av1_metadata=color_primaries=1:transfer_characteristics=1:matrix_coefficients=1:color_range=0',
                              '-f', 'ivf']
        command([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=64x48:rate=60',
                 '-frames:v', '24'] + video_encoding + ['-y', video])
        if av1:
            source_info = json.loads(command([probe, '-v', 'error', '-show_streams',
                                              '-of', 'json', video]).stdout)['streams'][0]
            assert source_info['color_primaries'] == source_info['color_transfer'] == 'bt709'
            assert source_info['color_space'] == 'bt709' and source_info['color_range'] == 'tv'
        input_epoch = coded_origin = audio_epoch = video_epoch_us = 0
        full_pcm = root / 'input.pcm'
        raw_pcm = root / 'source.pcm'
        source_frames = 19200
        if args.worker:
            prefix = root / 'probe.pcm'
            command([binary, '--dump-probe', prefix])
            input_epoch, coded_origin, audio_epoch, video_epoch_us = map(
                int, command([binary, '--epochs']).stdout.split())
            if args.source_rate:
                from fractions import Fraction
                raw_frames = math.ceil(args.source_rate * .4)
                samples = array('h')
                for i in range(raw_frames):
                    value = int(12000 * math.sin(2 * math.pi * 997 * i / args.source_rate))
                    samples.extend((value, -value))
                if sys.byteorder != 'little':
                    samples.byteswap()
                raw_pcm.write_bytes(samples.tobytes())
                normalized = root / 'normalized.pcm'
                command([binary, '--resample-source', repr(args.source_rate), raw_pcm, normalized])
                source = normalized.read_bytes()
                numerator, denominator = Fraction.from_float(args.source_rate).as_integer_ratio()
                source_frames = (raw_frames * 48000 * denominator + numerator - 1) // numerator
                assert len(source) == source_frames * 4
                evidence['normalization'] = dict(raw_frames=raw_frames, output_frames=source_frames,
                    scope='Shipping sinc prepass for encoder replay bytes; worker independently converts raw PCM. Filter quality covered by mathematical tone/alias tests.')
            else:
                source = command([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i',
                                  'sine=frequency=997:sample_rate=48000', '-t', '0.4',
                                  '-ac', '2', '-f', 's16le', '-']).stdout
                assert len(source) == source_frames * 4
            full_pcm.write_bytes(prefix.read_bytes() + bytes((input_epoch - 8192) * 4) + source)
            audio_input = ['-f', 's16le', '-ar', '48000', '-ac', '2', '-i', str(full_pcm)]
        else:
            audio_input = ['-f', 'lavfi', '-i', 'sine=frequency=997:sample_rate=48000', '-t', '0.4', '-ac', '2']
        command([ffmpeg, '-v', 'error'] + audio_input +
                ['-c:a', 'aac', '-b:a', '128k', '-f', 'adts', '-y', audio])
        data = video.read_bytes()
        starts = list(re.finditer(rb'\x00\x00(?:\x00)?\x01', data))
        nals = [(match.start(), (data[match.end()] >> 1) & 63 if hevc else data[match.end()] & 31,
                 data[match.start():starts[i + 1].start() if i + 1 < len(starts) else len(data)])
                for i, match in enumerate(starts)]
        csd = b'' if av1 else b''.join(next(blob for _, kind, blob in nals if kind == wanted)
                       for wanted in ((32, 33, 34) if hevc else (7, 8)))
        boundaries = [at for at, kind, _ in nals if kind == (35 if hevc else 9)] + [len(data)]
        pictures = [data[a:b] for a, b in zip(boundaries, boundaries[1:])]
        if av1:
            mp4 = root / 'config.mp4'
            command([ffmpeg, '-v', 'error', '-i', video, '-c', 'copy', '-y', mp4])
            boxes = mp4.read_bytes()
            assert boxes.count(b'av1C') == 1
            at = boxes.index(b'av1C')
            length = struct.unpack('>I', boxes[at - 4:at])[0]
            assert 12 <= length <= 65543 and at - 4 + length <= len(boxes)
            csd = boxes[at + 4:at - 4 + length]
            assert csd[0] == 0x81
            assert data[:4] == b'DKIF' and data[8:12] == b'AV01'
            pictures = []
            at = struct.unpack_from('<H', data, 6)[0]
            while at < len(data):
                length = struct.unpack_from('<I', data, at)[0]
                assert length and at + 12 + length <= len(data)
                pictures.append(data[at + 12:at + 12 + length])
                at += 12 + length
            assert at == len(data)
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
        pixel_format = 'yuv420p10le' if args.codec == 'hevc10' else 'rgb24'
        reference_video = command([ffmpeg, '-v', 'error', '-i', video, '-fps_mode', 'passthrough',
                                   '-f', 'rawvideo', '-pix_fmt', pixel_format, '-']).stdout
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
                    env['MC_STREAM_CODEC'] = args.codec
                    env.pop('RTMPS_CA', None)
                    if relay:
                        url = f'rtmps://localhost:{relay.port}/live/test'
                        env['RTMPS_CA'] = str(relay.ca)
                    deadline = time.monotonic() + 20
                    while True:
                        replay_args = [binary, url, fixture, 'legacy' if legacy else 'standard']
                        if args.worker:
                            replay_args += [full_pcm, str(input_epoch)]
                            if args.source_rate:
                                replay_args += [raw_pcm, repr(args.source_rate)]
                        result = command(replay_args,
                                         env=env, timeout=30, check=False)
                        if result.returncode != 78 or server.poll() is not None:
                            break
                        if time.monotonic() >= deadline:
                            raise TimeoutError('receiver did not accept connection')
                        time.sleep(0.01)
                    result.check_returncode()
                    assert b'native_mediacodec=0' in result.stdout
                    if args.worker:
                        assert b'worker=1' in result.stdout
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
            video_stream = next(s for s in info['streams'] if s['index'] == video_index)
            assert video_stream['codec_name'] == ('av1' if av1 else 'hevc' if hevc else 'h264')
            if args.codec == 'hevc10':
                assert video_stream['pix_fmt'] == 'yuv420p10le'
                assert video_stream['color_primaries'] == 'bt2020'
                assert video_stream['color_transfer'] == 'smpte2084'
                assert video_stream['color_space'] == 'bt2020nc'
            if av1:
                assert video_stream['color_primaries'] == 'bt709'
                assert video_stream['color_transfer'] == 'bt709'
                assert video_stream['color_space'] == 'bt709'
                assert video_stream['color_range'] == 'tv'
            audio_streams = [s for s in info['streams'] if s['codec_type'] == 'audio']
            assert bool(audio_streams) == with_audio
            coded_video = [p for p in info['packets'] if p['stream_index'] == video_index]
            assert len(coded_video) == 24
            for i, packet in enumerate(coded_video):
                offset = video_epoch_us / 1000000 if args.worker and with_audio else 0
                assert abs(float(packet['pts_time']) - offset - i / 60) < .0011
            decoded = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:v:0',
                               '-fps_mode', 'passthrough', '-f', 'rawvideo', '-pix_fmt', pixel_format, '-']).stdout
            assert decoded == reference_video
            if with_audio:
                assert audio_streams[0]['codec_name'] == 'aac'
                assert int(audio_streams[0]['sample_rate']) == 48000 and audio_streams[0]['channels'] == 2
                coded_audio = [p for p in info['packets'] if p['stream_index'] == audio_streams[0]['index']]
                assert len(coded_audio) == len(packets) - coded_origin // 1024
                for i, packet in enumerate(coded_audio):
                    assert abs(float(packet['pts_time']) - i * 1024 / 48000) < .0011
                decoded_audio = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:a:0',
                                         '-f', 'f32le', '-']).stdout
                if args.worker:
                    samples = array('f')
                    samples.frombytes(decoded_audio[:1024 * 2 * 4])
                    assert max(map(abs, samples)) < .0001, 'calibration leaked into preroll'
                    source_bytes = source_frames * 2 * 4
                    start = audio_epoch * 2 * 4
                    reference_start = (input_epoch + 1024) * 2 * 4
                    assert decoded_audio[start:start + source_bytes] == reference_audio[
                        reference_start:reference_start + source_bytes]
                    assert len(decoded_audio[start:start + source_bytes]) == source_bytes
                else:
                    assert decoded_audio == reference_audio
            evidence['cases'].append(dict(name=name, video_packets=24,
                                           audio_packets=len(coded_audio) if with_audio else 0,
                                           pixels='byte-identical independent decode',
                                           audio=('silent preroll and byte-identical source window' if args.worker
                                                  else 'byte-identical independent decode') if with_audio else None,
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
