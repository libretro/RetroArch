#!/usr/bin/env python3
"""Generate coded input and an independent FFmpeg RGB reference."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
p = argparse.ArgumentParser()
p.add_argument("output", type=Path)
p.add_argument("--ffmpeg", default="ffmpeg")
a = p.parse_args()
w = a.output.resolve()
w.mkdir(parents=True, exist_ok=True)
movie = w / "hevc-aac.mp4"
command = [a.ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
           "-f", "lavfi", "-i", "testsrc2=size=256x192:rate=12",
           "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
           "-t", "16", "-c:v", "libx265", "-preset", "fast", "-pix_fmt", "yuv420p",
           "-x265-params", "pools=2:frame-threads=1:log-level=error",
           "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709",
           "-c:a", "aac", "-b:a", "96k", str(movie)]
subprocess.run(command, check=True)
reference = w / "reference.rgb"
subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(movie),
                "-frames:v", "12", "-pix_fmt", "rgb24", "-f", "rawvideo", str(reference)], check=True)
assert reference.stat().st_size == 12 * 256 * 192 * 3
(w / "fixture.json").write_text(json.dumps({"command": command,
    "movie_sha256": hashlib.sha256(movie.read_bytes()).hexdigest(),
    "reference_sha256": hashlib.sha256(reference.read_bytes()).hexdigest()}, indent=2))
