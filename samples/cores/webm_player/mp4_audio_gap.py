#!/usr/bin/env python3
"""Build a fast-start HEVC fixture with audio beyond the initial fill budget."""
from pathlib import Path
import argparse
import bisect
import json
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("source", type=Path)
p.add_argument("output", type=Path)
a = p.parse_args()
a.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="mp4-audio-gap-") as temporary:
    fast = Path(temporary) / "fast.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(a.source),
                    "-c", "copy", "-movflags", "+faststart", str(fast)], check=True)
    data = bytearray(fast.read_bytes())
    packets = json.loads(subprocess.check_output([
        "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_packets",
        "-show_entries", "packet=pos,size", "-of", "json", str(fast)], text=True))["packets"]

def u32(offset):
    return struct.unpack_from(">I", data, offset)[0]

def put(offset, value):
    struct.pack_into(">I", data, offset, value)

def boxes(lo, hi):
    while lo < hi:
        size = u32(lo)
        assert size >= 8 and lo + size <= hi
        yield lo, size, bytes(data[lo + 4:lo + 8])
        lo += size
    assert lo == hi

def walk(offset, size):
    for child, length, kind in boxes(offset + 8, offset + size):
        yield child, length, kind
        if kind in (b"trak", b"mdia", b"minf", b"stbl"):
            yield from walk(child, length)

positions = [int(packet["pos"]) for packet in packets[:4]]
assert len(positions) == 4 and positions == sorted(positions)
# HEVC filler_data_rbsp: header, ff_bytes, rbsp_trailing_bits.
nal = b"\x4c\x01" + b"\xff" * (8 * 1024 * 1024 - 3) + b"\x80"
filler = struct.pack(">I", len(nal)) + nal
added = len(filler)
video_tables = 0
for offset, size, kind in list(boxes(0, len(data))):
    if kind == b"mdat":
        put(offset, size + sum(added for pos in positions if offset + 8 <= pos < offset + size))
    if kind != b"moov":
        continue
    for track, track_size, track_kind in boxes(offset + 8, offset + size):
        if track_kind != b"trak":
            continue
        entries = list(walk(track, track_size))
        video = any(k == b"hdlr" and data[e + 16:e + 20] == b"vide" for e, n, k in entries)
        for entry, length, box_kind in entries:
            if box_kind == b"stco":
                for index in range(u32(entry + 12)):
                    at = entry + 16 + 4 * index
                    old = u32(at)
                    put(at, old + added * bisect.bisect_left(positions, old))
            if box_kind == b"stsz" and video:
                assert u32(entry + 12) == 0 and u32(entry + 16) == len(packets)
                for index in range(4):
                    at = entry + 20 + 4 * index
                    assert u32(at) == int(packets[index]["size"])
                    put(at, u32(at) + added)
                video_tables += 1
assert video_tables == 1
output = bytearray()
previous = 0
for position in positions:
    output += data[previous:position] + filler
    previous = position
output += data[previous:]
a.output.write_bytes(output)
# The added coded filler must not alter decoded video.
def digest(path):
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(path),
        "-frames:v", "4", "-an", "-f", "framemd5", "-"])
    return [line for line in raw.splitlines() if not line.startswith(b"#")]
assert digest(a.source) == digest(a.output)
print("Generated progressive audio gap:", len(output), "bytes")
