#!/usr/bin/env python3
"""Check native PCM SRC clocks and AAC output with an independent decoder."""
import array
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import traceback

binary = Path(sys.argv[1]).resolve()
root = Path(os.environ.get('AUDIO_TOOLBOX_SRC_ARTIFACT_DIR') or
            tempfile.mkdtemp(prefix='at-src-'))
root.mkdir(parents=True, exist_ok=True)
evidence = dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                commands=[], cases=[], result='running')


def save():
    (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


def run(arguments):
    command = dict(arguments=list(map(str, arguments)), result='running')
    evidence['commands'].append(command)
    save()
    result = subprocess.run(command['arguments'], capture_output=True, timeout=30)
    index = len(evidence['commands'])
    (root / f'{index}.stdout').write_bytes(result.stdout)
    (root / f'{index}.stderr').write_bytes(result.stderr)
    command['result'] = result.returncode
    save()
    result.check_returncode()
    return result.stdout


def samples(raw):
    values = array.array('h', raw)
    if sys.byteorder != 'little':
        values.byteswap()
    return values


def snr(actual, rate, channels):
    energy = error = 0
    for i, value in enumerate(actual):
        frequency = 1499 if i % channels else 997
        expected = int(math.sin(i // channels * 2 * math.pi * frequency / rate) * 12000)
        energy += expected ** 2
        error += (value - expected) ** 2
    return 10 * math.log10(energy / max(error, 1))


try:
    for input_rate in (8000, 32040, 32040.5, 96000, 192000):
        for output_rate, channels in ((44100, 1), (48000, 2)):
            stem = root / f'{input_rate}-{output_rate}-{channels}'
            aac, pcm = stem.with_suffix(stem.suffix + '.aac'), stem.with_suffix(stem.suffix + '.pcm')
            output = run([binary, input_rate, output_rate, channels, aac, pcm])
            values = {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', output.decode())}
            duration_error = abs(values['frames'] - values['source'] * output_rate / input_rate)
            assert duration_error <= 1, (input_rate, values, duration_error)
            converted = samples(pcm.read_bytes())
            assert len(converted) == values['frames'] * channels
            src_snr = snr(converted, output_rate, channels)
            assert src_snr > 30, (input_rate, src_snr)
            decoded = samples(run([os.environ.get('FFMPEG', 'ffmpeg'), '-v', 'error',
                                   '-i', aac, '-map', '0:a:0', '-c:a', 'pcm_s16le', '-f', 's16le', '-']))
            assert len(decoded) == values['packets'] * 1024 * channels
            assert values['packets'] * 1024 - values['leading'] - values['trailing'] == values['frames']
            aligned = decoded[values['leading'] * channels:
                              (values['leading'] + values['frames']) * channels]
            aac_snr = snr(aligned, output_rate, channels)
            assert aac_snr > 30, (input_rate, aac_snr)
            evidence['cases'].append(dict(input_rate=input_rate, output_rate=output_rate,
                                          channels=channels, native=values, src_snr=src_snr,
                                          aac_snr=aac_snr, duration_error=duration_error, result='PASS'))
            save()
            print(f'PASS native SRC {input_rate}->{output_rate}/{channels}: '
                  f'{src_snr:.2f} dB PCM, {aac_snr:.2f} dB AAC', flush=True)
    evidence['result'] = 'PASS'
    save()
except BaseException:
    evidence['result'] = 'FAIL'
    (root / 'failure.txt').write_text(traceback.format_exc())
    save()
    raise
