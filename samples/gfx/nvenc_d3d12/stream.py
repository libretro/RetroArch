#!/usr/bin/env python3
"""Native GPU/AAC stream, checked by an independent RTMP receiver/decoder."""
import argparse
import array
from contextlib import ExitStack
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'libretro-common/samples/net/rtmp'))
from tls_relay import TlsRelay


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, timeout=60, **kwargs)


def decode(ffmpeg, path, audio=False):
    options = ['-map', '0:a:0', '-c:a', 'pcm_s16le', '-f', 's16le'] if audio else [
        '-map', '0:v:0', '-pix_fmt', 'yuv420p', '-f', 'rawvideo']
    return run([ffmpeg, '-v', 'error', '-i', path, *options, '-'], capture_output=True).stdout


def check(binary, ffmpeg, probe, root, rate, capture, tls=None):
    root.mkdir(parents=True, exist_ok=False)
    source, received = root / 'source.h264', root / 'received.flv'
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
            if tls:
                relay = stack.enter_context(TlsRelay(root, port, tls))
                env['NVENC_TEST_STREAM_URL'] = f'rtmps://localhost:{relay.port}/live/test'
                env['RTMPS_CA'] = str(relay.ca)
            with (root / 'publisher.log').open('wb') as log:
                run([binary, '0', '1', source, '1', '0', '0', str(capture)],
                    env=env, stdout=log, stderr=subprocess.STDOUT)
            assert server.wait(timeout=20) == 0, 'independent receiver failed'
        finally:
            if server.poll() is None:
                server.kill()
            server.wait(timeout=5)
    expected, actual = decode(ffmpeg, source), decode(ffmpeg, received)
    assert len(expected) == 24 * 320 * 192 * 3 // 2
    assert actual == expected, ('video differs or truncated', len(expected), len(actual))
    info = json.loads(run([probe, '-v', 'error', '-show_streams', '-show_packets',
                          '-of', 'json', received], capture_output=True).stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
    channels = 1 if rate == 44100 else 2
    assert video['codec_name'] == 'h264' and audio['codec_name'] == 'aac'
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
    args = parser.parse_args()
    for rate in (48000, 44100):
        for capture in (1, 2):
            check(args.binary.resolve(), args.ffmpeg, args.ffprobe,
                  args.output_dir.resolve() / f'{rate}-{capture}', rate, capture, args.tls)
