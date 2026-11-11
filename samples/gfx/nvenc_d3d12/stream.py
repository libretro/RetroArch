#!/usr/bin/env python3
"""Native GPU/AAC stream, checked by an independent RTMP receiver/decoder."""
import argparse
import array
from io import BytesIO
from contextlib import ExitStack
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import sys
import struct

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'libretro-common/samples/net/rtmp'))
from tls_relay import TlsRelay
from peer import replies
from command import decode as decode_amf


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, timeout=60, **kwargs)


def decode(ffmpeg, path, audio=False, depth=8):
    options = ['-map', '0:a:0', '-c:a', 'pcm_s16le', '-f', 's16le'] if audio else [
        '-map', '0:v:0', '-pix_fmt', 'yuv420p10le' if depth == 10 else 'yuv420p', '-f', 'rawvideo']
    return run([ffmpeg, '-v', 'error', '-i', path, *options, '-'], capture_output=True).stdout


def check(binary, ffmpeg, probe, root, rate, capture, tls=None,
          codec='h264', depth=8, color=1, metadata=0):
    root.mkdir(parents=True, exist_ok=False)
    source, received = root / ('source.' + codec), root / 'received.flv'
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    url = f'rtmp://127.0.0.1:{port}/live/test'
    env = dict(os.environ, NVENC_TEST_REQUIRE_HW='1', NVENC_TEST_DISPATCH='1',
               NVENC_TEST_CAPTURE_OWNED='1', NVENC_TEST_AUDIO_RATE=str(rate),
               NVENC_TEST_STREAM_URL=url)
    with ExitStack() as stack, (root / 'receiver.log').open('wb') as receiver_log:
        server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout',
                                   '15', '-copyts', '-i', url, '-map', '0', '-c',
                                   'copy', '-f', 'flv', str(received)],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=receiver_log)
        try:
            relay = stack.enter_context(TlsRelay(root, port, tls, capture_connect=True))
            env['NVENC_TEST_STREAM_URL'] = f'{"rtmps" if tls else "rtmp"}://localhost:{relay.port}/live/test'
            if tls:
                env['RTMPS_CA'] = str(relay.ca)
            with (root / 'publisher.log').open('wb') as log:
                run([binary, '1' if codec == 'hevc' else '0', '3' if depth == 10 else '1',
                     source, str(color), '0', str(metadata), str(capture)],
                    env=env, stdout=log, stderr=subprocess.STDOUT)
            assert server.wait(timeout=20) == 0, 'independent receiver failed'
            wire = BytesIO(relay.connect_bytes)
            assert wire.read(1) == b'\x03' and len(wire.read(3072)) == 3072
            stop = (lambda kind, payload: kind == 9 and payload[:5] == b'\xd4hvc1') if codec == 'hevc' else None
            records, payload = replies(wire.read, command=b'connect', stop=stop)
            connect = None
            at = 0
            while at < len(records):
                _, _, kind, _, size = struct.unpack('>IIIII', records[at:at + 20])
                body = records[at + 20:at + 20 + size]
                if kind == 20 and body.startswith(b'\x02\0\x07connect'):
                    connect = decode_amf(body)[2]
                at += 20 + size
            code = 'hvc1' if codec == 'hevc' else 'avc1'
            assert connect['fourCcList'] == [code, 'mp4a']
            assert connect['videoFourCcInfoMap'] == {code: 2}
            assert connect['audioFourCcInfoMap'] == {'mp4a': 2}
            if codec == 'hevc':
                described = decode_amf(payload[5:])
                expected = {'colorConfig': {'bitDepth': depth}}
                if color:
                    expected['colorConfig'].update(colorPrimaries=1 if color == 1 else 9,
                        transferCharacteristics={1: 1, 2: 16, 3: 18}[color],
                        matrixCoefficients=1 if color == 1 else 9)
                if metadata & 2:
                    expected['hdrCll'] = {'maxCLL': 1000, 'maxFall': 400}
                if metadata & 1:
                    expected['hdrMdcv'] = dict(redX=.68, redY=.32, greenX=.265, greenY=.69,
                        blueX=.15, blueY=.06, whitePointX=.3127, whitePointY=.329,
                        maxLuminance=1000, minLuminance=.005)
                assert described == ['colorInfo', expected], described
        finally:
            if server.poll() is None:
                server.kill()
            server.wait(timeout=5)
    expected, actual = decode(ffmpeg, source, depth=depth), decode(ffmpeg, received, depth=depth)
    assert len(expected) == 24 * 320 * 192 * 3 // 2 * (2 if depth == 10 else 1)
    assert actual == expected, ('video differs or truncated', len(expected), len(actual))
    info = json.loads(run([probe, '-v', 'error', '-show_streams', '-show_packets',
                          '-of', 'json', received], capture_output=True).stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
    channels = 1 if rate == 44100 else 2
    assert video['codec_name'] == codec and audio['codec_name'] == 'aac'
    assert int(audio['sample_rate']) == rate and audio['channels'] == channels
    total = 24 * rate // 60
    for stream, count, duration in ((video, 24, 1 / 60),
                                    (audio, math.ceil(total / 1024), 1024 / rate)):
        packets = [p for p in info['packets'] if p['stream_index'] == stream['index']]
        assert len(packets) == count, (stream['codec_name'], len(packets), count)
        for i, packet in enumerate(packets):
            assert abs(float(packet['pts_time']) - i * duration) <= .0011, packet
    pcm = array.array('h', decode(ffmpeg, received, True))
    expected_pcm = array.array('h', Path(str(source) + '.av.pcm').read_bytes())
    assert len(expected_pcm) == total * channels
    assert len(pcm) == math.ceil(total / 1024) * 1024 * channels
    measurements = []
    for channel in range(channels):
        original, decoded = expected_pcm[channel::channels], pcm[channel::channels]
        points = range(1024, total - 256, 7)
        lag = max(range(len(decoded) - total + 1),
                  key=lambda delay: sum(original[i] * decoded[i + delay] for i in points))
        energy = sum(original[i] ** 2 for i in points)
        noise = sum((original[i] - decoded[i + lag]) ** 2 for i in points)
        gain = sum(original[i] * decoded[i + lag] for i in points) / energy
        snr = 10 * math.log10(energy / max(noise, 1))
        assert .9 < gain < 1.1 and snr > 15, (channel, lag, gain, snr)
        measurements.append(dict(delay=lag, gain=gain, snr=snr))
    result = dict(rate=rate, channels=channels, capture=capture, frames=24, tls=tls,
                  codec=codec, depth=depth, color=color, metadata=metadata,
                  audio=measurements)
    (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS native GPU capture -> NVENC/AAC -> RTMP: ' + json.dumps(result), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--tls', choices=['1.2', '1.3'])
    parser.add_argument('--codec', choices=['h264', 'hevc'], default='h264')
    parser.add_argument('--depth', type=int, choices=[8, 10], default=8)
    parser.add_argument('--color', type=int, choices=[0, 1, 2, 3], default=1)
    parser.add_argument('--metadata', type=int, choices=[0, 1, 2, 3], default=0)
    args = parser.parse_args()
    for rate in (48000, 44100):
        for capture in (1, 2):
            check(args.binary.resolve(), args.ffmpeg, args.ffprobe,
                  args.output_dir.resolve() / f'{rate}-{capture}', rate, capture, args.tls,
                  args.codec, args.depth, args.color, args.metadata)
