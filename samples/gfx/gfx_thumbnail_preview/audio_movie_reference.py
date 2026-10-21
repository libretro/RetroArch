"""Independent AAC Matroska PCM placement with full nanosecond precision.

Matroska timestamp rule: https://www.matroska.org/technical/notes.html
FFmpeg's millisecond stream time base rounds a fractional CodecDelay. Use
its contiguous decoded PCM, but place it from the actual EBML timestamps
and CodecDelay using rational arithmetic. These fixtures have no gaps or
audio lacing; their sample count is exact rather than millisecond-rounded.
"""
from fractions import Fraction
import subprocess
from webm_audio_padding import elements, vint


def aac_matroska_pcm(path):
    segment = next(v for k, v in elements(path.read_bytes()) if k == 0x18538067)
    tracks = next(v for k, v in elements(segment) if k == 0x1654AE6B)
    track = next(dict(elements(v)) for k, v in elements(tracks)
                 if k == 0xAE and dict(elements(v)).get(0x86) == b"A_AAC")
    number = int.from_bytes(track[0xD7], "big")
    delay = int.from_bytes(track.get(0x56AA, b""), "big")
    info = dict(elements(next(v for k, v in elements(segment) if k == 0x1549A966)))
    scale = int.from_bytes(info.get(0x2AD7B1, b"\x0f\x42\x40"), "big")
    first = None
    for k, cluster in elements(segment):
        if k != 0x1F43B675:
            continue
        children = list(elements(cluster))
        origin = int.from_bytes(next(v for k, v in children if k == 0xE7), "big")
        for k, packet in children:
            if k == 0xA0:
                packet = next((v for k, v in elements(packet) if k == 0xA1), None)
            elif k != 0xA3:
                continue
            if packet is None:
                continue
            tid, width = vint(packet, 0)
            if tid != number:
                continue
            assert not packet[width + 2] & 6, "reference expects unlaced AAC"
            relative = int.from_bytes(packet[width:width+2], "big", signed=True)
            first = (origin + relative) * scale - delay
            break
        if first is not None:
            break
    assert first is not None
    pcm = subprocess.check_output(["ffmpeg", "-v", "error", "-copyts", "-i", str(path),
          "-map", "0:a:0", "-ar", "48000", "-ac", "2", "-f", "s16le", "pipe:1"])
    start = round(Fraction(first * 48000, 1000000000))
    return bytes(start * 4) + pcm if start >= 0 else pcm[-start*4:]
