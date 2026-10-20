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
p.add_argument("--ffprobe", default="ffprobe")
p.add_argument("--depth", type=int, choices=(8, 10), default=8)
p.add_argument("--range", choices=("limited", "full"), default="limited")
a = p.parse_args()
w = a.output.resolve()
w.mkdir(parents=True, exist_ok=True)
movie = w / "hevc-aac.mp4"
full = a.range == "full"
pixel_format = "yuv420p10le" if a.depth == 10 else "yuv420p"
command = [a.ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
           "-f", "lavfi", "-i", "testsrc2=size=256x192:rate=12",
           "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
           "-t", "16", "-c:v", "libx265", "-preset", "fast", "-pix_fmt", pixel_format,
           "-x265-params", "pools=2:frame-threads=1:log-level=error:range=" + a.range,
           "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709",
           "-color_range", "pc" if full else "tv"]
if full:
    command += ["-vf", "scale=in_range=limited:out_range=full"]
command += ["-c:a", "aac", "-b:a", "96k", str(movie)]
subprocess.run(command, check=True)
reference = w / "reference.rgb"
subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(movie),
                "-frames:v", "12", "-sws_flags", "bilinear+accurate_rnd+full_chroma_int",
                "-pix_fmt", "rgb24", "-f", "rawvideo", str(reference)], check=True)
assert reference.stat().st_size == 12 * 256 * 192 * 3
info = json.loads(subprocess.check_output([a.ffprobe, "-v", "error", "-select_streams", "v:0",
    "-show_entries", "stream=profile,pix_fmt,color_range,color_space,color_transfer,color_primaries",
    "-of", "json", str(movie)], text=True))
stream = info["streams"][0]
assert stream["pix_fmt"] == ("yuv420p10le" if a.depth == 10 else ("yuvj420p" if full else "yuv420p"))
assert stream["profile"] == ("Main 10" if a.depth == 10 else "Main")
assert stream["color_range"] == ("pc" if full else "tv")
assert all(stream[key] == "bt709" for key in ("color_space", "color_transfer", "color_primaries"))
(w / "fixture.json").write_text(json.dumps({"command": command, "ffprobe": info,
    "reference_filter": "bilinear+accurate_rnd+full_chroma_int",
    "movie_sha256": hashlib.sha256(movie.read_bytes()).hexdigest(),
    "reference_sha256": hashlib.sha256(reference.read_bytes()).hexdigest()}, indent=2))
