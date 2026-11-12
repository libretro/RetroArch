#!/usr/bin/env python3
"""Real VT/AAC output through RTMP(S), independently received and decoded."""
import argparse
import array
from contextlib import ExitStack
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import socket
import subprocess
import sys
import tempfile
import time
import traceback


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', default=os.environ.get('VT_STREAM_ARTIFACT_DIR'))
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--first-case', action='store_true')
    parser.add_argument('--metal', action='store_true')
    parser.add_argument('--dispatcher', action='store_true')
    parser.add_argument('--ownership', choices=('arc', 'mrc'), default='arc')
    args = parser.parse_args()
    args.metal = args.metal or args.dispatcher
    if sys.platform != 'darwin':
        parser.error('native VT/AAC streaming requires Apple hardware')
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    network = repo / 'libretro-common/samples/net/rtmp'
    sys.path.insert(0, str(network))
    from session_peer import native_ffmpeg
    from tls_relay import TlsRelay
    root = Path(args.artifacts).resolve() if args.artifacts else Path(
        tempfile.mkdtemp(prefix='vt-stream-'))
    if args.artifacts:
        root.mkdir(parents=True, exist_ok=False)
    evidence = dict(platform=sys.platform, backend='metal' if args.metal else 'pixel-pool',
                    ownership=args.ownership if args.metal else None, dispatcher=args.dispatcher,
                    commands=[], cases=[], result='running')

    def save():
        (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')

    def command(arguments, cwd=None, timeout=60, env=None, check=True):
        entry = dict(arguments=list(map(str, arguments)), result='running')
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
        binary = args.binary.resolve() if args.binary else root / 'native_vt_test'
        if not args.binary:
            command(['make', 'rtmps_client_native_test'], cwd=network, timeout=180)
            dry = command(['make', '-B', '-n', 'rtmps_client_native_test'], cwd=network)
            link = next(shlex.split(line) for line in dry.stdout.decode().splitlines()
                        if ' -o rtmps_client_native_test ' in line)
            link[link.index('-o') + 1] = str(binary)
            fixture = 'metal_dispatch_test.m' if args.dispatcher else 'metal_vt_test.m' if args.metal else 'native_vt_test.c'
            link[link.index('rtmp_client_native_test.c')] = str(here / fixture)
            link += ['-O2', '-DHAVE_AVF', '-Wno-unused-function', '-Wno-long-long',
                     '-Wno-c99-extensions', '-Wno-c11-extensions', '-mmacosx-version-min=10.14']
            link += [str(repo / name) for name in (
                'record/record_stream_vt.c', 'record/record_gpu_worker_vt.c',
                'record/record_gpu_queue.c', 'record/record_vt_packet.c',
                'record/record_at_audio.c', 'record/record_at_resample.c', 'record/record_audio_queue.c',
                'record/record_stream_output_vt.c', 'record/record_avfoundation_session.c',
                'record/record_avfoundation_worker.c',
                'libretro-common/queues/retro_spsc.c')]
            for framework in ('CoreMedia', 'CoreVideo', 'VideoToolbox', 'AudioToolbox', 'CoreFoundation'):
                link += ['-framework', framework]
            if args.metal:
                command([sys.executable, here / 'metal_vt_oracle_test.py', '--ffmpeg', ffmpeg])
                command([sys.executable, repo / 'record/shaders/generate_metal.py', '--check'])
                link += [str(repo / name) for name in ('record/record_gpu_capture_metal.m',
                         'record/record_gpu_session_metal.m', 'record/record_mux_avfoundation.m',
                         'libretro-common/formats/h265/rh265_sps_crop.c')]
                for framework in ('Foundation', 'Metal', 'AVFoundation'):
                    link += ['-framework', framework]
                if args.dispatcher:
                    link += ['-DHAVE_NETWORKING', '-DHAVE_RTMP', '-DHAVE_RFLV',
                             str(repo / 'record/record_gpu_dispatch_metal.m'),
                             str(repo / 'libretro-common/net/net_wake.c')]
                # Compile Objective-C ownership modes separately from C inputs.
                compile_flags = []
                skip = False
                for flag in link:
                    if skip:
                        skip = False
                        continue
                    if flag in ('-o', '-framework'):
                        skip = True
                    elif not flag.endswith(('.c', '.m', '.o')) and not flag.startswith(('-l', '-L', '-Wl,')):
                        compile_flags.append(flag)
                for number, source in enumerate(list(link)):
                    if not source.endswith('.m'):
                        continue
                    obj = root / f'objc-{number}.o'
                    ownership = '-fobjc-arc' if args.ownership == 'arc' else '-fno-objc-arc'
                    command(compile_flags + ['-fblocks', ownership, '-c', source, '-o', obj], cwd=network, timeout=180)
                    link[number] = str(obj)
            command(link, cwd=network, timeout=180)
        evidence['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
        cases = (
                (0, 8, 48000, 2, None, False, None, False), (1, 8, 44100, 1, '1.2', False, None, False),
                (1, 10, 48000, 2, '1.3', False, None, False), (0, 8, 48000, 2, None, True, None, False),
                (0, 8, 48000, 2, None, True, 32040.5, False), (1, 8, 44100, 1, '1.2', True, 96000, False),
                (0, 8, 48000, 2, None, True, None, True),
                (0, 8, 48000, 2, None, True, 32040.5, True),
                (1, 8, 44100, 1, '1.2', True, 96000, True))
        cases = [case + (False,) for case in cases]
        cases += [(0, 8, 48000, 2, None, True, None, True, True),
                  (1, 8, 44100, 1, '1.2', True, 96000, True, True)]
        if args.metal:
            cases = [(0, 8, 48000, 2, None, True, None, True, False),
                     (1, 8, 48000, 2, '1.2', True, 32040.5, True, False),
                     (1, 10, 44100, 1, '1.3', True, 96000, True, False)]
        for hevc, depth, rate, channels, tls, queued, source_rate, owned, cancel in cases[:1] if args.first_case else cases:
            case = dict(hevc=hevc, depth=depth, rate=rate, channels=channels,
                        tls=tls, queued=queued, source_rate=source_rate, owned=owned,
                        cancel=cancel, result='running')
            evidence['cases'].append(case)
            directory = root / f'{hevc}-{depth}-{rate}-{channels}-{tls or "tcp"}{"-queued" if queued else ""}'
            if source_rate:
                directory = directory.with_name(directory.name + f'-src-{source_rate}')
            if owned:
                directory = directory.with_name(directory.name + '-owned')
            if cancel:
                directory = directory.with_name(directory.name + '-cancel')
            directory.mkdir()
            received, coded = directory / 'received.flv', directory / 'native.bin'
            with socket.socket() as reserve:
                reserve.bind(('127.0.0.1', 0))
                port = reserve.getsockname()[1]
            url = f'rtmp://127.0.0.1:{port}/live/test'
            with ExitStack() as stack, (directory / 'receiver.log').open('w+b') as log:
                server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout',
                                           '10', '-copyts', '-i', url, '-map', '0', '-c',
                                           'copy', '-f', 'flv', str(received)],
                                          stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                          stderr=log)
                try:
                    relay = stack.enter_context(TlsRelay(directory, port, tls)) if tls else None
                    env = os.environ.copy()
                    env.pop('VT_STREAM_SOURCE_QUEUE', None)
                    env.pop('VT_STREAM_SOURCE_RATE', None)
                    env.pop('VT_STREAM_OWNED_OUTPUT', None)
                    env.pop('VT_STREAM_CANCEL_OWNED', None)
                    env.pop('RTMPS_CA', None)
                    if queued:
                        env['VT_STREAM_SOURCE_QUEUE'] = '1'
                    if source_rate:
                        env['VT_STREAM_SOURCE_RATE'] = str(source_rate)
                    if owned:
                        env['VT_STREAM_OWNED_OUTPUT'] = '1'
                    if cancel:
                        env['VT_STREAM_CANCEL_OWNED'] = '1'
                    if relay:
                        url = f'rtmps://localhost:{relay.port}/live/test'
                        env['RTMPS_CA'] = str(relay.ca)
                    deadline = time.monotonic() + 20
                    while True:
                        result = command([binary, hevc, depth, rate, channels, url, coded],
                                         timeout=35, env=env, check=False)
                        if result.returncode != 78 or server.poll() is not None:
                            break
                        if time.monotonic() >= deadline:
                            raise TimeoutError('native publisher did not connect')
                        time.sleep(0.01)
                    result.check_returncode()
                    receiver_status = server.wait(timeout=20)
                    case['receiver_status'] = receiver_status
                    if not cancel:
                        assert receiver_status == 0, 'independent receiver failed'
                finally:
                    if server.poll() is None:
                        server.terminate()
                        try:
                            server.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            server.kill()
                            server.wait(timeout=5)
            values = {key: int(value) for key, value in re.findall(
                r'(\w+)=(\d+)', result.stdout.decode())}
            case['native'] = values
            assert bool(values.get('metal', 0)) == args.metal, values
            assert bool(values.get('dispatcher', 0)) == args.dispatcher, values
            if cancel:
                assert values['cancelled'] == values['owned'] == 1, values
                assert 2 <= values['submitted'] < 24, values
                assert values['released'] == values['submitted'], values
                case['scope'] = 'native cancellation and complete resource retirement; truncated wire is not a decode-quality pass'
                case['result'] = 'PASS'
                save()
                print('PASS native owned-session cancellation:', case, flush=True)
                continue
            assert bool(values['prebuffer']) == queued, values
            assert bool(values['owned']) == owned, values
            info = json.loads(command([probe, '-v', 'error', '-show_streams',
                                       '-show_packets', '-of', 'json', received]).stdout)
            video = next(s for s in info['streams'] if s['codec_type'] == 'video')
            audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
            assert video['codec_name'] == ('hevc' if hevc else 'h264')
            assert video['pix_fmt'] == ('yuv420p10le' if depth == 10 else 'yuv420p')
            width, height = (162, 130) if args.metal else (64, 48)
            assert (int(video['width']), int(video['height'])) == (width, height)
            assert audio['codec_name'] == 'aac' and int(audio['sample_rate']) == rate
            assert audio['channels'] == channels
            timestamps = [round(float(p['pts_time']) * 1000) for p in info['packets']
                          if p['codec_type'] == 'video']
            epoch = (values['leading'] * 60000 + rate - 1) // rate
            expected = [(epoch + i * 1001) * 1000 // 60000 for i in range(24)]
            assert timestamps == expected, ('video timestamps changed', timestamps, expected)
            if args.metal:
                from metal_vt_oracle import pixels, audio as audio_oracle
                assert values['frames'] == 24 and (values['width'], values['height']) == (width, height)
                input_rate = source_rate or rate
                assert values['input'] == math.ceil(24 * 1001 * input_rate / 60000)
                assert video.get('color_primaries') == video.get('color_transfer') == video.get('color_space') == 'bt709', video
                assert video.get('color_range') == 'tv', video
                raw = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:v:0',
                               '-fps_mode', 'passthrough', '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-']).stdout
                case['pixels'] = pixels(raw, width, height, depth)
                raw = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:a:0',
                               '-c:a', 'pcm_s16le', '-f', 's16le', '-']).stdout
                case['audio'] = audio_oracle(raw, rate, channels, values['leading'], values['input'], input_rate)
                case['result'] = 'PASS'
                save()
                print('PASS native Metal factory RTMP receiver:', case, flush=True)
                continue
            decoded = []
            for source in (coded, received):
                decoded.append(command([ffmpeg, '-v', 'error', '-i', source,
                                        '-map', '0:v:0', '-fps_mode', 'passthrough',
                                        '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo', '-']).stdout)
            assert decoded[0] == decoded[1], 'wire changed decoded native pictures'
            assert len(decoded[0]) == 24 * 64 * 48 * 3
            pixels = array.array('H', decoded[1])
            if sys.byteorder != 'little':
                pixels.byteswap()
            error = 0
            for frame in range(24):
                offset = frame * 64 * 48 * 3 // 2
                reference = ((64 + frame * 4) * 4, (100 + frame) * 4, (150 - frame) * 4)
                for plane, size in enumerate((64 * 48, 32 * 24, 32 * 24)):
                    error += sum(abs(value - reference[plane]) for value in pixels[offset:offset + size])
                    offset += size
            case['pixel_mean_error'] = error / len(pixels)
            assert case['pixel_mean_error'] < 4, case
            raw = command([ffmpeg, '-v', 'error', '-i', received, '-map', '0:a:0',
                           '-c:a', 'pcm_s16le', '-f', 's16le', '-']).stdout
            samples = array.array('h', raw)
            if sys.byteorder != 'little':
                samples.byteswap()
            frames, leading = values['audio'], values['leading']
            assert len(samples) // channels == frames + leading + values['trailing']
            samples = samples[leading * channels:(leading + frames) * channels]
            energy = error = 0
            for i, actual in enumerate(samples):
                frequency = 1499 if i % channels else 997
                expected_sample = int(math.sin((i // channels) * 2 * math.pi * frequency / rate) * 12000)
                energy += expected_sample ** 2
                error += (actual - expected_sample) ** 2
            case['audio_snr'] = 10 * math.log10(energy / max(error, 1))
            assert case['audio_snr'] > 30, case
            case['result'] = 'PASS'
            save()
            print('PASS native VT/AAC RTMP receiver:', case, flush=True)
        if not args.binary:
            source_path = repo / 'record/record_stream_vt.c'
            source = source_path.read_text()
            clock = 'packet.dts + p->video_epoch, packet.pts + p->video_epoch'
            assert source.count(clock) == 1
            mutant_source, mutant_binary = root / 'missing_epoch.c', root / 'missing_epoch_test'
            mutant_source.write_text(source.replace(clock, 'packet.dts, packet.pts'))
            link[link.index(str(source_path))] = str(mutant_source)
            link[link.index('-o') + 1] = str(mutant_binary)
            command(link + ['-I' + str(repo / 'record')], cwd=network, timeout=180)
            negative_dir = root / 'negative-epoch'
            negative_args = [sys.executable, here / 'native_vt_run.py', '--binary',
                             mutant_binary, '--first-case', '--artifacts', negative_dir]
            if args.metal:
                negative_args += ['--metal', '--ownership', args.ownership]
            if args.dispatcher:
                negative_args += ['--dispatcher']
            negative = command(negative_args,
                               timeout=90, check=False)
            assert negative.returncode != 0
            assert 'video timestamps changed' in (negative_dir / 'failure.txt').read_text()
            evidence['negative_epoch'] = 'rejected missing video epoch through real receiver'
            print('PASS negative control: missing native video epoch rejected', flush=True)
        evidence['result'] = 'PASS'
        save()
    except BaseException:
        evidence['result'] = 'FAIL'
        (root / 'failure.txt').write_text(traceback.format_exc())
        save()
        raise


if __name__ == '__main__':
    main()
