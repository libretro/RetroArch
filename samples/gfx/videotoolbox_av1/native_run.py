#!/usr/bin/env python3
"""Exercise native VideoToolbox AV1 with independently decoded FFmpeg references."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", required=True)
parser.add_argument("--results", type=Path)
args = parser.parse_args()
binary = Path(args.binary).resolve()
expected_mode = "native-decode"
results = []
with tempfile.TemporaryDirectory(prefix="vt-av1-native-") as directory:
    root = Path(directory)
    for depth in (8, 10):
        fmt = "yuv420p" if depth == 8 else "yuv420p10le"
        for position, grain in ((0, 0), (1, 0), (2, 0), (0, 1)):
            stem = f"{depth}-{position}-{grain}"
            stream = root / (stem + ".ivf")
            reference = root / (stem + ".yuv")
            options = ["-aom-params", "film-grain-test=5"] if grain else []
            options += ["-bsf:v", "av1_metadata=color_primaries=1:"
                        "transfer_characteristics=1:matrix_coefficients=1:"
                        "color_range=0:chroma_sample_position=" + str(position)]
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                "testsrc2=size=128x96:rate=4", "-frames:v", "4", "-an",
                "-c:v", "libaom-av1", "-cpu-used", "8", "-threads", "2",
                "-pix_fmt", fmt, "-g", "2", "-lag-in-frames", "0",
                "-crf", "0", "-b:v", "0", "-color_primaries", "bt709",
                "-color_trc", "bt709", "-colorspace", "bt709", "-color_range",
                "tv", *options, "-y", str(stream)], check=True)
            subprocess.run(["ffmpeg", "-v", "error", "-i", str(stream),
                "-pix_fmt", fmt, "-f", "rawvideo", "-y", str(reference)], check=True)
            assert reference.stat().st_size == 4 * 128 * 96 * 3 // 2 * (2 if depth == 10 else 1)
            completed = subprocess.run([str(binary), "-", str(stream),
                str(reference), str(depth), str(position), str(grain)], check=False,
                capture_output=True, text=True, timeout=90)
            print(completed.stdout, end="")
            print(completed.stderr, end="")
            if completed.returncode not in (0, 77):
                completed.check_returncode()
            assert "MODE: " + expected_mode in completed.stdout, "acceptance binary mode mismatch"
            if not completed.returncode:
                assert "PASS: native AV1 hardware pixels," in completed.stdout, "native completion marker missing"
            results.append(dict(depth=depth, position=position, grain=grain,
                                stream_sha256=hashlib.sha256(stream.read_bytes()).hexdigest(),
                                reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                                status="passed" if not completed.returncode else "unavailable"))
record = dict(api="videotoolbox", mode=expected_mode, binary=str(binary),
              native_executed=any(r["status"] == "passed" for r in results),
              cases=results, complete=all(r["status"] == "passed" for r in results))
if args.results:
    args.results.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
print(json.dumps(record))
raise SystemExit(0 if record["complete"] else 77)
