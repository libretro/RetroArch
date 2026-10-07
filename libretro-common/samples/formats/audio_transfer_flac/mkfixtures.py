#!/usr/bin/env python3
"""Regenerates fixtures/tone.flac and fixtures/tone.mka.

20000 stereo frames at 44100 Hz - not a multiple of the encoder's block
size, so each stream ends on a short frame - of two sines with a little
low-order noise so the predictor has residual to code. The test
regenerates the same PCM from the same formula and compares every
sample, so keep the two in step. Needs ffmpeg; the committed fixtures
are what CI uses."""

import math
import os
import struct
import subprocess

N, RATE = 20000, 44100
here = os.path.dirname(os.path.abspath(__file__))
fix = os.path.join(here, "fixtures")
os.makedirs(fix, exist_ok=True)
raw = os.path.join(fix, "src.s16")

pcm = bytearray()
for i in range(N):
    left = int(12000 * math.sin(i * 2 * math.pi * 440 / RATE)) \
        + ((i * 1103515245 + 12345) >> 16 & 63) - 32
    right = int(9000 * math.sin(i * 2 * math.pi * 660 / RATE)) \
        + ((i * 69069 + 1) >> 16 & 31) - 16
    pcm += struct.pack("<hh", left, right)
with open(raw, "wb") as f:
    f.write(pcm)

for out in ("tone.flac", "tone.mka"):
    subprocess.check_call(["ffmpeg", "-loglevel", "error", "-y",
                           "-f", "s16le", "-ar", str(RATE), "-ac", "2",
                           "-i", raw, "-c:a", "flac",
                           os.path.join(fix, out)])
os.remove(raw)
