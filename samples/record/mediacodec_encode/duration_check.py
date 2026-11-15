#!/usr/bin/env python3
"""Compare shipping PCM duration arithmetic with exact binary64 rationals."""
from fractions import Fraction
import math
import random
import subprocess
import sys

rng = random.Random(716)
rates = [8000.0, 192000.0, 32040.5, 44100.5,
         math.nextafter(48000.0, 0.0), math.nextafter(44100.0, math.inf)]
rates += [rng.uniform(8000, 192000) for _ in range(16)]
cases = []
for output_rate in (44100, 48000):
    maximum = ((1 << 64) - 1) // output_rate
    for rate in rates:
        for frames in (0, 1, maximum, rng.randrange(maximum)):
            numerator, denominator = Fraction.from_float(rate).as_integer_ratio()
            expected = (frames * output_rate * denominator + numerator - 1) // numerator
            cases.append((rate, output_rate, frames, expected))
payload = ''.join(f'{rate!r} {output} {frames >> 32} {frames & 0xffffffff}\n'
                  for rate, output, frames, _ in cases)
result = subprocess.run([sys.argv[1], '--durations'], input=payload.encode(),
                        capture_output=True, timeout=30)
if result.returncode:
    sys.stderr.write(result.stderr.decode(errors='replace'))
result.check_returncode()
lines = result.stdout.splitlines()
assert len(lines) == len(cases)
for line, case in zip(lines, cases):
    high, low = map(int, line.split())
    assert (high << 32) | low == case[3], (case, high, low)
print(f'PASS {len(cases)} duration cases against independent exact-rational oracle')
