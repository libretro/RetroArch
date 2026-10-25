#!/usr/bin/env python3
"""Run the real task loader against muxed text without a whole-file input."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

arguments = sys.argv[1:]
malformed = arguments[0] == "--malformed"
if malformed:
    arguments = arguments[1:]
binary = Path(arguments[0]).resolve()
if os.name == "nt" and not binary.suffix:
    binary = binary.with_suffix(".exe")
with tempfile.TemporaryDirectory(prefix="subtitle-embedded-") as temporary:
    root = Path(temporary)
    source = root / "captions.srt"
    source.write_text("1\n00:00:00,100 --> 00:00:01,200\nfirst caf\u00e9\n\n"
                      "2\n00:00:02,000 --> 00:00:03,500\nsecond \U0001f600\n", encoding="utf-8")
    movie = root / "captions.mp4"
    subprocess.run([os.environ.get("FFMPEG", "ffmpeg"), "-v", "error", "-y", "-i", str(source),
                    "-map", "0:0", "-map", "0:0", "-c:s", "mov_text", str(movie)], check=True)
    source.unlink()
    subprocess.run([str(binary), str(movie)], check=True)
    original = movie.read_bytes()
    marker = original.index(b"moov") - 4
    sparse = root / "sparse.mp4"
    with sparse.open("wb") as stream:
        stream.write(original[:marker])
        padding = 64 * 1024 * 1024
        stream.write(padding.to_bytes(4, "big") + b"free")
        stream.seek(padding - 8, 1)
        stream.write(original[marker:])
    subprocess.run([str(binary), str(sparse)], check=True)
    text = "first caf\u00e9".encode("utf-8")
    old = len(text).to_bytes(2, "big") + text
    for encoding, bom in [("utf-16-be", b"\xfe\xff"), ("utf-16-le", b"\xff\xfe")]:
        encoded = bom + "Z\U0001f600".encode(encoding)
        replacement = len(encoded).to_bytes(2, "big") + encoded + bytes(len(text) - len(encoded))
        assert original.count(old) == 2
        movie.write_bytes(original.replace(old, replacement))
        subprocess.run([str(binary), str(movie)], check=True,
                       env=dict(os.environ, SUBTITLE_EXPECT_UTF16="1"))
    if malformed:
        invalid = [b"\xff\xff" + text,
                   len(text).to_bytes(2, "big") + bytes([255]) * len(text),
                   len(text).to_bytes(2, "big") + b"a\0" + bytes(len(text) - 2)]
        for bad in [b"\xfe\xffA", b"\xfe\xff\xd8\x00", b"\xfe\xff\xdc\x00",
                    b"\xfe\xff\xd8\x00\x00A"]:
            invalid.append(len(bad).to_bytes(2, "big") + bad + bytes(len(text) - len(bad)))
        for replacement in invalid:
            assert len(replacement) == len(old)
            movie.write_bytes(original.replace(old, replacement))
            subprocess.run([str(binary), str(movie)], check=True,
                           env=dict(os.environ, SUBTITLE_EXPECT_FAILURE="1"))
