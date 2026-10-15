#!/usr/bin/env python3
"""Generate small, local-only media for the builtin core's host test."""
from pathlib import Path
import subprocess
import sys

out = Path(sys.argv[1] if len(sys.argv) > 1 else "fixtures")
out.mkdir(parents=True, exist_ok=True)
cases = [
    ("av1-opus.webm", ["-s", "64x64", "-c:v", "libaom-av1", "-cpu-used", "8", "-threads", "2", "-g", "8", "-c:a", "libopus"]),
    ("av1-vorbis.webm", ["-s", "64x64", "-c:v", "libaom-av1", "-cpu-used", "8", "-threads", "2", "-g", "8", "-c:a", "libvorbis"]),
    ("vp8-opus.webm", ["-c:v", "libvpx", "-deadline", "best", "-c:a", "libopus"]),
    ("vp9-vorbis.webm", ["-c:v", "libvpx-vp9", "-c:a", "libvorbis"]),
    ("h264-aac.mp4", ["-c:v", "libx264", "-bf", "2", "-g", "12", "-c:a", "aac", "-movflags", "+faststart"]),
    ("h264-aac.mkv", ["-c:v", "libx264", "-bf", "2", "-g", "12", "-c:a", "aac"]),
    ("hevc-aac.mp4", ["-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error", "-c:a", "aac"]),
]
for name, codec in cases:
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                    "-f", "lavfi", "-i", "testsrc2=size=64x48:rate=12:duration=2",
                    "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=2",
                    "-ac", "2", "-pix_fmt", "yuv420p", *codec, str(out / name)], check=True)
(out / "invalid.webm").write_bytes(b"not an EBML or MP4 movie\n")
(out / "empty.webm").write_bytes(b"")
