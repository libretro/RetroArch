#!/usr/bin/env python3
"""MP4 video-only presentation offset with continuous audio."""
from pathlib import Path
import json
import subprocess
import sys
out = Path(sys.argv[1] if len(sys.argv) > 1 else "fixtures")
out.mkdir(parents=True, exist_ok=True)
for delay in (0, 1):
    path = out / f"video-timeline-{delay}.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-itsoffset", str(delay),
         "-f", "lavfi", "-i", "testsrc2=size=64x48:rate=12:duration=2",
         "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=4",
         "-map", "0:v:0", "-map", "1:a:0", "-fps_mode", "passthrough", "-c:v", "libx264",
         "-bf", "0", "-c:a", "aac", "-ac", "2", "-movflags", "+faststart", str(path)], check=True)
    packets = json.loads(subprocess.check_output(["ffprobe", "-v", "error",
         "-select_streams", "v:0", "-show_packets", "-show_entries", "packet=pts_time",
         "-of", "json", str(path)]))["packets"]
    assert len(packets) == 24 and abs(float(packets[0]["pts_time"]) - delay) < .001
    print(path.name, "pictures", len(packets), "first PTS", packets[0]["pts_time"])
