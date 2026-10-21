#!/usr/bin/env python3
"""Independent timestamp discontinuities for the actual builtin core host."""
from pathlib import Path
import array
import json
import subprocess
import sys
from fractions import Fraction
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "gfx/gfx_thumbnail_preview"))
from audio_movie_reference import aac_matroska_pcm

out = Path(sys.argv[1] if len(sys.argv) > 1 else "fixtures")
out.mkdir(parents=True, exist_ok=True)

def make(name, *, delay=0, duration=4, audio_duration=2, codec="libopus", filter=None, mp4=False, trim=0):
    args = ["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
            f"testsrc2=size=64x48:rate=12:duration={duration}", "-itsoffset", str(delay),
            "-f", "lavfi", "-i", f"sine=frequency=440:sample_rate=48000:duration={audio_duration}"]
    if filter:
        args += ["-af", filter]
    if mp4:
        args += ["-c:v", "libx264", "-bf", "0", "-movflags", "+faststart"]
    else:
        args += ["-c:v", "libvpx", "-deadline", "realtime", "-cpu-used", "8", "-auto-alt-ref", "0"]
    if codec == "libvorbis" and mp4:
        args += ["-strict", "experimental"]
    args += ["-c:a", codec, "-ac", "2", str(out / name)]
    subprocess.run(args, check=True)
    if trim:
        # Change only the audio edit's media_time. Coded packets and
        # offsets remain untouched, so the direct decoder slice is an
        # independent oracle for the requested source-sample trim.
        data = bytearray((out / name).read_bytes())
        def boxes(lo, hi):
            while lo + 8 <= hi:
                n = int.from_bytes(data[lo:lo+4], "big")
                if n < 8 or lo + n > hi:
                    raise ValueError("invalid generated box")
                yield bytes(data[lo+4:lo+8]), lo+8, lo+n
                lo += n
        def child(lo, hi, kind):
            return next((a,b) for k,a,b in boxes(lo,hi) if k == kind)
        moov = child(0,len(data),b"moov")
        for kind,lo,hi in boxes(*moov):
            if kind != b"trak":
                continue
            mdia = child(lo,hi,b"mdia")
            hdlr,_ = child(*mdia,b"hdlr")
            if data[hdlr+8:hdlr+12] != b"soun":
                continue
            mdhd,_ = child(*mdia,b"mdhd")
            assert data[mdhd] == 0 and int.from_bytes(data[mdhd+12:mdhd+16],"big") == 48000
            edit = child(lo,hi,b"edts")
            elst,end = child(*edit,b"elst")
            assert int.from_bytes(data[elst+4:elst+8],"big") == 1
            version = data[elst]
            at = elst + (16 if version else 12)
            size = 8 if version else 4
            data[at:at+size] = trim.to_bytes(size,"big")
            (out / name).write_bytes(data)
            break
        else:
            raise ValueError("audio edit missing")
    # Keep original timestamps: FFmpeg's default input-origin shift would
    # otherwise hide a negative codec-delay timestamp in the zero case.
    pcm = subprocess.check_output(["ffmpeg", "-v", "error", "-copyts", "-i", str(out / name),
          "-map", "0:a:0", "-af", "aresample=async=1:first_pts=0", "-ar", "48000",
          "-ac", "2", "-f", "s16le", "pipe:1"])
    if codec == "aac" and not mp4:
        pcm = aac_matroska_pcm(out / name)
    if trim:
        # MOV demuxing rounds Vorbis edits to whole packets. Build the
        # sample-accurate oracle from edit-free decoded frames and their
        # original timestamp instead of inheriting that rounding policy.
        frames = json.loads(subprocess.check_output(["ffprobe", "-v", "error",
              "-ignore_editlist", "1", "-select_streams", "a:0", "-show_frames",
              "-show_entries", "frame=pts", "-of", "json", str(out / name)]))["frames"]
        origin = int(frames[0]["pts"])
        skip = max(0, trim - origin)
        raw = subprocess.check_output(["ffmpeg", "-v", "error", "-ignore_editlist", "1",
              "-i", str(out / name), "-map", "0:a:0", "-ar", "48000", "-ac", "2",
              "-f", "s16le", "pipe:1"])
        (out / (name + ".head")).write_bytes(raw[skip*4:skip*4+16384])
        (out / (name + ".slice")).write_text(str(skip) + "\n", encoding="utf-8")
    samples = array.array("h")
    samples.frombytes(pcm)
    if sys.byteorder != "little":
        samples.byteswap()
    first = next(i // 2 for i in range(0, len(samples), 2)
                 if abs(samples[i]) > 64 or abs(samples[i + 1]) > 64)
    (out / (name + ".onset")).write_text(str(first) + "\n", encoding="utf-8")
    # The movie period includes the video-only tail. Keep this independent
    # of the frontend's decoder duration and loop bookkeeping. Activity is
    # deliberately coarse: it tests placement, not codec waveform equality.
    probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error",
          "-show_entries", "format=duration", "-of", "json", str(out / name)]))
    period = round(Fraction(probe["format"]["duration"]) * 48000)
    activity = bytearray()
    for start in range(0, period, 960):
        peak = max((abs(x) for x in samples[start*2:min(start+960,period)*2]), default=0)
        activity.append(2 if peak > 512 else 0 if peak <= 64 else 1)
    (out / (name + ".activity")).write_bytes(activity)
    (out / (name + ".period")).write_text(str(period) + "\n", encoding="utf-8")

make("timeline-control.mp4", codec="aac", mp4=True, audio_duration=4,
     filter=r"volume=enable='between(t,1,2)':volume=0")
for delay in (0, 1, 2):
    make(f"timeline-{delay}.webm", delay=delay)
for codec in ("libopus", "libvorbis"):
    make(f"timeline-gap-{codec}.webm", duration=5, audio_duration=4,
         codec=codec, filter=r"aselect=not(between(t\,1\,2))")
make("timeline-overlap.webm", duration=5, audio_duration=4,
     filter=r"asetpts=PTS-0.5/TB*gte(T\,2)")
for delay in (0, 1):
    make(f"timeline-{delay}.mp4", delay=delay, codec="aac", mp4=True)

make("timeline-vorbis-trim.mp4", codec="libvorbis", mp4=True, trim=4801)
make("timeline-opus-trim.mp4", codec="libopus", mp4=True, trim=4801)
make("timeline-opus.mp4", codec="libopus", mp4=True)
make("timeline-vorbis.mp4", codec="libvorbis", mp4=True)
for codec in ("libopus", "libvorbis"):
    # More than 256 video packets precede the first audio packet. The
    # movie reader must yield without reporting EOF or rewinding a voice.
    make(f"timeline-late-{codec}.webm", delay=30, duration=35, codec=codec)
make("timeline-delayed-vorbis.webm", delay=1, codec="libvorbis")
for delay in (0, 1):
    make(f"timeline-aac-{delay}.mkv", delay=delay, codec="aac")
