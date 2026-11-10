#!/usr/bin/env python3
"""Independent native AAC decode, clock, channel and tail acceptance."""
import argparse
from array import array
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(command, **kwargs):
    if os.name == 'nt':
        kwargs['creationflags'] = subprocess.CREATE_NO_WINDOW
    try:
        return subprocess.run(command, check=True, timeout=30, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr.decode(errors='replace'), file=sys.stderr)
        raise


def reference(total, rate, channel):
    base, sweep = (1543, 900) if channel else (997, 500)
    return [int(12000 * math.sin(2 * math.pi *
                (base * i / rate + sweep * i * i / (2 * rate * 32768))))
            for i in range(total)]


def quality(raw, total, rate, channels):
    decoded = array('h', raw)
    if sys.byteorder != 'little':
        decoded.byteswap()
    samples = [decoded[c::channels] for c in range(channels)]
    expected = [reference(total, rate, c) for c in range(channels)]
    positions = range(4096, min(total - 2048, 16384), 8)
    probe = [(i, expected[0][i]) for i in positions]

    def score(lag, points):
        return sum(value * samples[0][i + lag] for i, value in points)

    coarse = max(range(0, 4097, 8), key=lambda lag: score(lag, probe))
    fine = [(i, expected[0][i]) for i in range(4096, min(total - 2048, 16384))]
    lag = max(range(max(0, coarse - 8), min(4096, coarse + 8) + 1),
              key=lambda shift: score(shift, fine))
    assert lag + total <= len(samples[0]), ('truncated tail', lag, total, len(samples[0]))
    result = []
    for source, actual in zip(expected, samples):
        begin, end = 1024, total - 256
        energy = sum(value * value for value in source[begin:end])
        noise = sum((actual[i + lag] - source[i]) ** 2 for i in range(begin, end))
        gain = sum(actual[i + lag] * source[i] for i in range(begin, end)) / energy
        snr = 10 * math.log10(energy / max(1, noise))
        tail = range(total - 2048, total - 256)
        tail_energy = sum(source[i] ** 2 for i in tail)
        tail_noise = sum((actual[i + lag] - source[i]) ** 2 for i in tail)
        tail_snr = 10 * math.log10(tail_energy / max(1, tail_noise))
        peak = max(abs(value) for value in actual)
        assert .9 < gain < 1.1 and snr > 15 and tail_snr > 10 and peak < 24000, (
            lag, gain, snr, tail_snr, peak)
        result.append(dict(gain=gain, snr=snr, tail_snr=tail_snr, peak=peak))
    return lag, result


def check(binary, ffmpeg, probe, root, rate, channels, bitrate, total=32768, large=False):
    name = '%d-%d-%d-%d-%d' % (rate, channels, bitrate, total, large)
    path = root / (name + '.aac')
    command = [binary, str(path), str(rate), str(channels), str(bitrate), str(total)]
    if large:
        command.append('large')
    with (root / (name + '.log')).open('w') as log:
        run(command, stdout=log, stderr=subprocess.STDOUT)
    text = (root / (name + '.log')).read_text()
    packets = re.findall(r'packet (\d+): time=(\d+):(\d+) duration=(\d+):(\d+) bytes=(\d+)', text)
    assert len(packets) == (total + 1023) // 1024
    epoch = (1 << 33) + 17 if large else 0
    duration_error = 0
    for expected_number, fields in enumerate(packets):
        number, hi, lo, dhi, dlo, size = map(int, fields)
        assert number == expected_number and size > 0
        timestamp, duration = (hi << 32) | lo, (dhi << 32) | dlo
        expected = (epoch + number * 1024) * 10000000 // rate
        end = (epoch + (number + 1) * 1024) * 10000000 // rate
        assert timestamp == expected, (name, number, timestamp, expected)
        duration_error = max(duration_error, abs(duration - (end - expected)))
    assert duration_error <= 10, ('native duration precision exceeds 1 us', duration_error)
    info = json.loads(run([probe, '-v', 'error', '-show_streams', '-of', 'json', str(path)],
                          capture_output=True).stdout)['streams']
    assert len(info) == 1
    assert info[0]['codec_name'] == 'aac' and info[0]['profile'] == 'LC'
    assert int(info[0]['sample_rate']) == rate and info[0]['channels'] == channels
    raw = run([ffmpeg, '-v', 'error', '-i', str(path), '-f', 's16le',
               '-acodec', 'pcm_s16le', '-'], capture_output=True).stdout
    assert len(raw) == len(packets) * 1024 * channels * 2
    lag, measured = quality(raw, total, rate, channels)
    result = dict(rate=rate, channels=channels, bitrate=bitrate, frames=total,
                  large_epoch=large, packets=len(packets), delay=lag,
                  max_duration_error_ticks=duration_error, quality=measured)
    print('PASS native AAC: ' + json.dumps(result), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('binary')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())
    lifecycle = run([binary, '--lifecycle'], capture_output=True)
    print(lifecycle.stdout.decode(errors='replace'), end='', flush=True)
    ffmpeg, probe = shutil.which(args.ffmpeg), shutil.which(args.ffprobe)
    if not ffmpeg or not probe:
        parser.error('ffmpeg and ffprobe are required independent oracles')
    with tempfile.TemporaryDirectory(prefix='mf-aac-') as temp:
        root = args.output_dir.resolve() if args.output_dir else Path(temp)
        root.mkdir(parents=True, exist_ok=True)
        results = []
        for rate in (44100, 48000):
            for channels in (1, 2):
                for bitrate in (96000, 128000, 160000, 192000):
                    results.append(check(binary, ffmpeg, probe, root, rate, channels, bitrate))
                results.append(check(binary, ffmpeg, probe, root, rate, channels,
                                     128000, total=32645, large=True))
        (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
