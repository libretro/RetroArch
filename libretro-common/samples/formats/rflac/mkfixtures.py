#!/usr/bin/env python3
"""Regenerates the fixtures of rflac_s16_test: FLAC streams of 16, 24 and
32 bits, mono, stereo and six channels, made by the reference encoder
from PCM built with integers only, which the test builds again the same
way and so knows every sample of. The stereo streams are made so that the
encoder picks each of the four channel assignments somewhere (checked
here with flac --analyze), and the 32-bit one so that its side channel
needs 33 bits. Needs flac; the committed fixtures are what CI uses."""

import os
import subprocess
import struct

FRAMES = 5003  # not a multiple of 4: the SIMD loops' scalar tails run too
here = os.path.dirname(os.path.abspath(__file__))
fix = os.path.join(here, "fixtures")
os.makedirs(fix, exist_ok=True)


def lcg(seed):
    while True:
        seed = (seed * 1103515245 + 12345) & 0xffffffff
        yield seed


def tri(i, period, amp):
    """A triangle wave, integer only: rises then falls over period frames."""
    p = i % period
    half = period // 2
    v = p if p < half else period - p
    return (v * 2 * amp) // half - amp


def sample(kind, bps, ch, i, c, noise):
    top = (1 << (bps - 1)) - 1
    n = next(noise) >> 20  # 12 bits of noise
    if kind == "stereo":
        # Frames 0-1023 the two noisy about one wave (mid/side), 1024-2047
        # left clean and right the same with noise (left/side), 2048-3071
        # the other way round (side/right), the rest unrelated
        # (independent)
        base = tri(i, 397, top // 2)
        seg = i // 1024
        if seg == 0:
            v = base + (n if c else -n)
        elif seg == 1:
            v = base if c == 0 else base + n
        elif seg == 2:
            v = base + n if c == 0 else base
        else:
            v = tri(i, 397 + 113 * c, top // 3) + n * (c + 1)
    elif kind == "wide":
        # Full scale and opposite: left - right needs 33 bits
        v = top - (n & 255) if c == 0 else -top + (n & 255)
        if i % 7 == 0:
            v = -v
    else:
        v = tri(i, 211 + 37 * c, top // 2) + n
    v = max(-top - 1, min(top, v))
    return v


def pcm(kind, bps, ch):
    noise = lcg(bps * 131 + ch)
    out = bytearray()
    width = (bps + 7) // 8
    for i in range(FRAMES):
        for c in range(ch):
            v = sample(kind, bps, ch, i, c, noise)
            out += (v & ((1 << (width * 8)) - 1)).to_bytes(width, "little")
    return out


FIXTURES = [
    ("s16_stereo.flac", "stereo", 16, 2),
    ("s24_stereo.flac", "stereo", 24, 2),
    ("s24_mono.flac", "mono", 24, 1),
    ("s24_6ch.flac", "multi", 24, 6),
    ("s32_wide.flac", "wide", 32, 2),
]

seen = set()
for name, kind, bps, ch in FIXTURES:
    raw = os.path.join(fix, "src.raw")
    out = os.path.join(fix, name)
    with open(raw, "wb") as f:
        f.write(pcm(kind, bps, ch))
    subprocess.check_call(["flac", "--silent", "--force", "--force-raw-format",
                           "--endian=little", "--sign=signed",
                           "--channels=" + str(ch), "--bps=" + str(bps),
                           "--sample-rate=48000", "-8", "-b", "1024",
                           "-o", out, raw])
    os.remove(raw)
    if ch == 2:
        a = subprocess.run(["flac", "--analyze", "--stdout", out],
                           capture_output=True, text=True).stdout
        for line in a.splitlines():
            for tag in ("INDEPENDENT", "LEFT_SIDE", "RIGHT_SIDE", "MID_SIDE"):
                if "channel_assignment=" + tag in line:
                    seen.add(tag)
missing = {"INDEPENDENT", "LEFT_SIDE", "RIGHT_SIDE", "MID_SIDE"} - seen
if missing:
    raise SystemExit("channel assignments not made: " + ", ".join(sorted(missing)))
print("assignments:", ", ".join(sorted(seen)))
