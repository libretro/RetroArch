#!/usr/bin/env python3
"""Independent native-shaped MP4 metadata fixtures; no encoded media required."""
from pathlib import Path
import os
import shlex
import struct
import subprocess
import sys
import tempfile

def box(tag, data):
    return struct.pack(">I4s", len(data) + 8, tag) + data

def fixture(version, empty=False):
    def header(tag, scale, duration, track=False):
        offset = (28 if track else 24) if version else (20 if track else 16)
        width = 8 if version else 4
        data = bytearray(offset + width + 16)
        data[0] = version
        if not track:
            struct.pack_into(">I", data, offset - 4, scale)
        data[offset:offset + width] = duration.to_bytes(width, "big")
        return box(tag, data)
    unknown = box(b"free", b"unchanged opaque metadata")
    stsd = box(b"stsd", struct.pack(">II", 0, 1) + box(b"mp4a", bytes(28)))
    stsz = box(b"stsz", struct.pack(">III", 0, 1, 49))
    tables = stsd + stsz + unknown
    handler = box(b"hdlr", bytes(8) + b"soun" + bytes(4))
    mdhd = header(b"mdhd", 48000, 50176)
    tkhd = header(b"tkhd", 0, 10454, True)
    audio = box(b"trak", tkhd + box(b"mdia", mdhd + handler + box(b"minf", box(b"stbl", tables))))
    video = box(b"trak", header(b"tkhd", 0, 10000, True) + box(b"mdia", box(b"hdlr", bytes(8) + b"vide" + bytes(4))))
    source = box(b"moov", header(b"mvhd", 10000, 10454) + video + audio + unknown)
    original_audio = audio
    edit = box(b"edts", box(b"elst", struct.pack(">IIQqhh", 0x01000000, 1, 10000, 2048, 1, 0)))
    groups = box(b"sgpd", struct.pack(">I4sIIh", 0x01000000, b"roll", 2, 1, -2))
    groups += box(b"sbgp", struct.pack(">I4sIII", 0, b"roll", 1, 49, 1))
    audio = box(b"trak", header(b"tkhd", 0, 10000, True) + edit + box(b"mdia", mdhd + handler + box(b"minf", box(b"stbl", tables + groups))))
    if empty:
        audio = original_audio[:4] + b"free" + original_audio[8:]
    expected = box(b"moov", header(b"mvhd", 10000, 10000) + video + audio + unknown)
    return source, expected

runner = shlex.split(os.environ.get("RUNNER", ""))
binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="mp4-audio-edit-") as directory:
    folder = Path(directory)
    for version, empty in [(0, False), (1, False), (0, True), (1, True)]:
        source, expected = fixture(version, empty)
        paths = [folder / "source.bin", folder / "expected.bin"]
        paths[0].write_bytes(source)
        paths[1].write_bytes(expected)
        args = list(map(str, paths))
        if runner and "wine" in runner[0]:
            args = [subprocess.check_output(["winepath", "-w", x], text=True).strip() for x in args]
        subprocess.run(runner + [binary] + args + ["0" if empty else "48000"], check=True, timeout=30)
