#!/usr/bin/env python3
"""Independent RTMP wire timestamps for the VT/AAC consumer contract."""
import argparse
import io
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here.parents[2] / 'libretro-common/samples/net/rtmp'))
from peer import replies

parser = argparse.ArgumentParser()
parser.add_argument('binary')
parser.add_argument('--runner', default='')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='vt-wire-', dir=Path.cwd()) as temporary:
    relative = Path(temporary).relative_to(Path.cwd()) / 'capture.bin'
    subprocess.run(shlex.split(args.runner) + [str(Path(args.binary).resolve()), str(relative)],
                   check=True, timeout=30)
    counts = {8: 0, 9: 0}
    def stop(kind, payload):
        if kind in counts and len(payload) > 1 and payload[1] == 1:
            counts[kind] += 1
        return counts == {8: 5, 9: 2}
    records, _ = replies(io.BytesIO(relative.read_bytes()).read, stop=stop)
    stamps = {8: [], 9: []}
    offset = 0
    while offset < len(records):
        channel, stream, kind, timestamp, length = struct.unpack_from('>IIIII', records, offset)
        offset += 20
        payload = records[offset:offset + length]
        offset += length
        if kind in stamps and len(payload) > 1 and payload[1] == 1:
            stamps[kind].append(timestamp)
    assert stamps[8] == [0, 21, 42, 64, 85], stamps
    assert stamps[9] == [44, 60], stamps
    print('PASS independent RTMP wire oracle: explicit 44 ms video epoch and source-frame AAC timestamps')
