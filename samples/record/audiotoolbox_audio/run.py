#!/usr/bin/env python3
"""Native AudioToolbox AAC packets, independently decoded with FFmpeg."""
import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import traceback

parser = argparse.ArgumentParser()
parser.add_argument('binary')
parser.add_argument('--artifacts', default=os.environ.get('AUDIO_TOOLBOX_ARTIFACT_DIR'))
args = parser.parse_args()
root = Path(args.artifacts) if args.artifacts else Path(tempfile.mkdtemp(prefix='at-aac-'))
if args.artifacts:
    root.mkdir(parents=True, exist_ok=False)
binary = Path(args.binary).resolve()
evidence = dict(platform=platform.platform(), binary=str(binary),
                binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), commands=[], cases=[])
print(f'AudioToolbox evidence: {root.resolve()}', flush=True)


def save():
    (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


def command(arguments):
    entry = dict(arguments=list(map(str, arguments)), result='running')
    evidence['commands'].append(entry)
    number = len(evidence['commands'])
    save()
    try:
        result = subprocess.run(entry['arguments'], capture_output=True, timeout=30)
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
    result.check_returncode()
    return result


try:
    for rate in (44100, 48000):
        for channels in (1, 2):
            path = root / f'{rate}-{channels}.aac'
            run = command([binary, rate, channels, path])
            print(run.stdout.decode(errors='replace'), end='', flush=True)
            values = {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', run.stdout.decode())}
            decoded_path = root / f'{rate}-{channels}.pcm'
            command([os.environ.get('FFMPEG', 'ffmpeg'), '-v', 'error', '-i', path, '-map', '0:a:0',
                     '-c:a', 'pcm_s16le', '-f', 's16le', decoded_path])
            raw = decoded_path.read_bytes()
            decoded = array.array('h', raw)
            if sys.byteorder != 'little':
                decoded.byteswap()
            assert len(decoded) == values['packets'] * 1024 * channels
            leading, frames = values['leading'], values['frames']
            assert values['packets'] * 1024 - leading - values['trailing'] == frames, values
            output = decoded[leading * channels:(leading + frames) * channels]
            assert len(output) == frames * channels
            error = energy = 0.0
            for i, actual in enumerate(output):
                frequency = 1499 if i % channels else 997
                expected = int(math.sin((i // channels) * 2 * math.pi * frequency / rate) * 12000)
                error += (actual - expected) ** 2
                energy += expected ** 2
            snr = 10 * math.log10(energy / max(error, 1))
            evidence['cases'].append(dict(rate=rate, channels=channels, snr=snr,
                                          native=values, result='PASS' if snr > 30 else 'FAIL'))
            save()
            assert snr > 30, (rate, channels, snr, values)
            print(f'PASS native AAC {rate}/{channels}: {frames} aligned source frames, '
                  f'{snr:.2f} dB SNR, native priming and exact partial drain', flush=True)
    evidence['result'] = 'PASS'
    save()
except BaseException:
    evidence['result'] = 'FAIL'
    (root / 'failure.txt').write_text(traceback.format_exc())
    save()
    raise
