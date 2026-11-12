#!/usr/bin/env python3
"""Independent decoded-pixel and source-clock oracle for Metal RTMP capture."""
import array
import math
import sys


def pixels(raw, width, height, depth):
    values = array.array('H', raw)
    if sys.byteorder != 'little':
        values.byteswap()
    assert len(values) == 24 * width * height * 3 // 2
    error = peak = 0
    offset = 0
    for frame in range(24):
        r, g, b = (((frame * 37 + 64) % 192 + 32) / 255,
                   ((frame * 53 + 100) % 192 + 32) / 255,
                   ((frame * 29 + 150) % 192 + 32) / 255)
        y = .2126 * r + .7152 * g + .0722 * b
        cb, cr = (b - y) / 1.8556, (r - y) / 1.5748
        if depth == 10:
            reference = (64 + 876 * y, 512 + 896 * cb, 512 + 896 * cr)
            scale = 1
        else:
            reference = (16 + 219 * y, 128 + 224 * cb, 128 + 224 * cr)
            scale = 4
        for expected, count in zip(reference, (width * height, width * height // 4, width * height // 4)):
            expected = math.floor(expected + .5) * scale
            for actual in values[offset:offset + count]:
                difference = abs(actual - expected)
                error += difference
                peak = max(peak, difference)
            offset += count
    mean = error / len(values)
    assert mean < 4 and peak <= 32, (mean, peak)
    return dict(mean=mean, peak=peak, pictures=24)


def audio(raw, rate, channels, leading, source_frames, source_rate):
    values = array.array('h', raw)
    if sys.byteorder != 'little':
        values.byteswap()
    assert len(values) % channels == 0
    coded = len(values) // channels
    expected = round(source_frames * rate / source_rate)
    # SRC rounding permits one output sample; exact native padding is covered
    # by the separate native AAC/SRC fixtures. Exclude that final sample here.
    assert coded % 1024 == 0 and -1 <= coded - leading - expected <= 2048
    values = values[leading * channels:(leading + expected - 1) * channels]
    assert len(values) == (expected - 1) * channels
    energy = error = 0
    for i, actual in enumerate(values):
        frequency = 1499 if i % channels else 997
        reference = int(math.sin((i // channels) * 2 * math.pi * frequency / rate) * 12000)
        energy += reference ** 2
        error += (actual - reference) ** 2
    snr = 10 * math.log10(energy / max(error, 1))
    assert snr > 30, snr
    return dict(snr=snr, source_frames=source_frames, decoded_frames=coded,
                compared_frames=expected - 1, scope='aligned quality; exact SRC/padding covered separately')
