#!/usr/bin/env python3
"""Generate small, local-only media for the builtin core's host test."""
from pathlib import Path
import subprocess
import sys

out = Path(sys.argv[1] if len(sys.argv) > 1 else "fixtures")
out.mkdir(parents=True, exist_ok=True)
subprocess.run([sys.executable, str(Path(__file__).with_name("vp8_reference_fixtures.py")), str(out)], check=True)
cases = [
    ("vp8-long-timing.webm", ["-c:v", "libvpx", "-deadline", "realtime", "-cpu-used", "8", "-g", "120", "-auto-alt-ref", "0", "-an"]),
    ("vp8-seek-opus.webm", ["-c:v", "libvpx", "-g", "12", "-auto-alt-ref", "0", "-cluster_time_limit", "1000", "-c:a", "libopus"]),
    ("vp9-seek-vorbis.webm", ["-c:v", "libvpx-vp9", "-g", "12", "-auto-alt-ref", "0", "-cluster_time_limit", "1000", "-c:a", "libvorbis"]),
    ("vp9-seek-live.webm", ["-c:v", "libvpx-vp9", "-g", "12", "-auto-alt-ref", "0", "-live", "1", "-c:a", "libvorbis"]),
    ("h264-long-seek.mp4", ["-c:v", "libx264", "-bf", "2", "-g", "600", "-keyint_min", "600", "-sc_threshold", "0", "-an", "-movflags", "+faststart"]),
    ("av1-opus.webm", ["-s", "64x64", "-c:v", "libaom-av1", "-cpu-used", "8", "-threads", "2", "-g", "8", "-c:a", "libopus"]),
    ("av1-vorbis.webm", ["-s", "64x64", "-c:v", "libaom-av1", "-cpu-used", "8", "-threads", "2", "-g", "8", "-c:a", "libvorbis"]),
    ("vp8-opus.webm", ["-c:v", "libvpx", "-deadline", "best", "-c:a", "libopus"]),
    ("vp9-vorbis.webm", ["-c:v", "libvpx-vp9", "-c:a", "libvorbis"]),
    ("h264-aac.mp4", ["-c:v", "libx264", "-bf", "2", "-g", "12", "-c:a", "aac", "-movflags", "+faststart"]),
    ("h264-aac.mkv", ["-c:v", "libx264", "-bf", "2", "-g", "12", "-c:a", "aac"]),
    ("hevc-aac.mp4", ["-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error", "-c:a", "aac"]),
    ("hevc-seek-aac.mp4", ["-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error", "-c:a", "aac"]),
    ("hevc-seek-aac44.mp4", ["-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error", "-c:a", "aac", "-ar", "44100"]),
    ("hevc-seek-opus.mp4", ["-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error", "-c:a", "libopus"]),
    ("h264-seek-aac.mp4", ["-c:v", "libx264", "-bf", "2", "-g", "12", "-c:a", "aac", "-movflags", "+faststart"]),
]
for name, codec in cases:
    duration = 700 if "-long-" in name else 16 if "-seek-" in name else 2
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                    "-f", "lavfi", "-i", f"testsrc2=size=64x48:rate=12:duration={duration}",
                    "-f", "lavfi", "-i", f"sine=frequency=440:sample_rate=48000:duration={duration}",
                    "-ac", "2", "-pix_fmt", "yuv420p", *codec, str(out / name)], check=True)
(out / "invalid.webm").write_bytes(b"not an EBML or MP4 movie\n")
(out / "empty.webm").write_bytes(b"")

# Beyond the core's 512-picture opening prefix and 16 MiB lookahead.
subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
                "testsrc2=size=64x48:rate=12:duration=53", "-f", "lavfi", "-i",
                "sine=frequency=440:sample_rate=48000:duration=53", "-vf",
                "noise=alls=100:allf=t+u", "-c:v", "libvpx", "-deadline",
                "realtime", "-cpu-used", "8", "-qmin", "1", "-qmax", "1",
                "-b:v", "100M", "-g", "12", "-auto-alt-ref", "0", "-c:a",
                "libopus", "-ac", "2", str(out / "core-group-seed.webm")], check=True)
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "gfx/gfx_thumbnail_preview"))
from webm_audio_padding import insert_void
insert_void(out / "core-group-seed.webm", out / "core-group.webm",
            amount=24 * 1024 * 1024, after_picture=541, in_group=True)
insert_void(out / "core-group-seed.webm", out / "core-sparse.webm",
            amount=96 * 1024 * 1024, after_picture=541)

subprocess.run([sys.executable, str(Path(__file__).with_name("mp4_audio_gap.py")),
                str(out / "hevc-seek-aac.mp4"), str(out / "hevc-audio-gap.mp4")], check=True)

subprocess.run([sys.executable, str(Path(__file__).with_name("mp4_audio_gap.py")),
                str(out / "hevc-seek-opus.mp4"), str(out / "hevc-opus-audio-gap.mp4")], check=True)

subprocess.run([sys.executable, str(Path(__file__).with_name("mp4_audio_gap.py")),
                str(out / "hevc-seek-aac44.mp4"), str(out / "hevc-aac44-audio-gap.mp4")], check=True)

subprocess.run([sys.executable, str(Path(__file__).with_name("audio_timeline_fixtures.py")), str(out)], check=True)

subprocess.run([sys.executable, str(Path(__file__).with_name("video_timeline_fixtures.py")), str(out)], check=True)
