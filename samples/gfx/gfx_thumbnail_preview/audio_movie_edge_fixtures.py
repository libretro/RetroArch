#!/usr/bin/env python3
"""Keep valid video/setup while removing Duration or coded audio blocks."""
from pathlib import Path
import subprocess
import sys
from webm_audio_padding import element, elements, vint
from audio_movie_reference import aac_matroska_pcm


def rewrite(source, destination, unknown=False, empty=False):
    top = list(elements(source.read_bytes()))
    segment = next(body for ident, body in top if ident == 0x18538067)
    audio = set()
    for ident, body in elements(segment):
        if ident == 0x1654AE6B:
            for entry, track in elements(body):
                if entry != 0xAE:
                    continue
                fields = dict(elements(track))
                if int.from_bytes(fields.get(0x83, b""), "big") == 2:
                    audio.add(int.from_bytes(fields[0xD7], "big"))
    assert audio
    output = bytearray()
    for ident, body in top:
        if ident != 0x18538067:
            output += element(ident, body)
            continue
        children = bytearray()
        for child, payload in elements(body):
            if child in (0x114D9B74, 0x1C53BB6B):
                continue  # positions change when rebuilding element headers
            if child == 0x1549A966 and unknown:
                payload = b"".join(element(k, bytes(len(v)) if k == 0x4489 else v)
                                   for k, v in elements(payload))
            if child == 0x1F43B675 and empty:
                cluster = bytearray()
                for block, packet in elements(payload):
                    coded = packet if block == 0xA3 else next(
                        (v for k, v in elements(packet) if k == 0xA1), None
                    ) if block == 0xA0 else None
                    if coded is not None and vint(coded, 0)[0] in audio:
                        block = 0xEC  # legal Void, leaving the video untouched
                    cluster += element(block, packet)
                payload = cluster
            children += element(child, payload)
        output += element(ident, children)
    destination.write_bytes(output)


base = Path(sys.argv[1])
out = base / "edges"
out.mkdir(exist_ok=True)
for label, name in (("opus", "timeline-0.webm"),
                    ("vorbis", "timeline-delayed-vorbis.webm"),
                    ("aac", "timeline-aac-1.mkv")):
    for unknown, empty in ((True, False), (False, True), (True, True)):
        target = out / f"{label}-unknown{int(unknown)}-empty{int(empty)}.mkv"
        rewrite(base / name, target, unknown, empty)
        # Independent timestamp-preserving decoder length; a valid empty
        # track has no coded packets and therefore no decoded PCM.
        if not empty:
            pcm = subprocess.check_output(["ffmpeg", "-v", "error", "-copyts",
                "-i", str(target), "-map", "0:a:0", "-af",
                "aresample=async=1:first_pts=0", "-ar", "48000", "-ac", "2",
                "-f", "s16le", "pipe:1"])
            if label == "aac":
                pcm = aac_matroska_pcm(target)
            (target.with_suffix(".frames")).write_text(str(len(pcm)//4) + "\n")
        else:
            (target.with_suffix(".frames")).write_text("0\n")
