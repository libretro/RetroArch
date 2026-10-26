#!/usr/bin/env python3
"""Run the real task loader against muxed text without a whole-file input."""
from pathlib import Path
import os
import subprocess
import sys
import struct
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
    def language_movie(code, elng=None, first=False, version=0):
        def rewrite(data):
            result = bytearray()
            offset = 0
            while offset < len(data):
                size, kind = struct.unpack_from(">I4s", data, offset)
                assert size >= 8 and offset + size <= len(data)
                payload = data[offset + 8:offset + size]
                if kind in (b"moov", b"trak", b"mdia"):
                    payload = rewrite(payload)
                    if kind == b"mdia" and elng is not None:
                        tag = struct.pack(">I4sI", len(elng) + 12, b"elng", version) + elng
                        payload = tag + payload if first else payload + tag
                elif kind == b"mdhd":
                    assert payload[0] == 0 and len(payload) >= 24
                    payload = payload[:20] + struct.pack(">H", code) + payload[22:]
                result += struct.pack(">I4s", len(payload) + 8, kind) + payload
                offset += size
            return bytes(result)
        # ffmpeg writes the movie metadata after the samples, so their offsets stay fixed.
        assert original.index(b"mdat") < original.index(b"moov")
        return rewrite(original)

    def packed(code):
        return sum((ord(c) - 96) << shift for c, shift in zip(code, (10, 5, 0)))

    language_cases = [
        (0, None, "en", "en"), (19, None, "zh-Hant", "zh-Hant"),
        (33, None, "zh-Hans", "zh-Hans"), (49, None, "az-Cyrl", "az-Cyrl"),
        (50, None, "az-Arab", "az-Arab"), (53, None, "ro-Cyrl", "ro-Cyrl"),
        (57, None, "mn-Mong", "mn-Mong"), (58, None, "mn-Cyrl", "mn-Cyrl"),
        (82, None, "tl", "tl"), (83, None, "ms-Latn", "ms-Latn"),
        (84, None, "ms-Arab", "ms-Arab"), (146, None, "ga-Latg", "ga-Latg"),
        (150, None, "az-Latn", "az-Latn"), (151, None, "nn", "nn"),
        (95, None, "", ""), (127, None, "", ""), (152, None, "", ""),
        (1023, None, "", ""), (32767, None, "", ""), (65535, None, "", ""),
        (packed("eng"), None, "en", "en"), (packed("fre"), None, "fr", "fr"),
        (packed("deu"), None, "de", "de"), (packed("und"), None, "und", "und"),
        (0x400, None, "", ""), (0x7ffe, None, "", ""),
        (packed("fra"), b"EN-us\0ignored", "EN-us", "EN-us"),
        (0, b"\0", "", ""), (0, b"", "", ""),
        (0, b"en--US\0", "en--US", "en--US"),
        (0, b"i-klingon\0", "i-klingon", "tlh"),
        (0, b"zh-cmn-Hans\0", "zh-cmn-Hans", "zh-cmn-Hans"),
    ]
    for code, tag, expected, key in language_cases:
        for first in ([False, True] if tag is not None else [False]):
            movie.write_bytes(language_movie(code, tag, first))
            subprocess.run([str(binary), str(movie)], check=True,
                           env=dict(os.environ, SUBTITLE_EXPECT_LANGUAGE_SET="1",
                                    SUBTITLE_EXPECT_LANGUAGE=expected,
                                    SUBTITLE_EXPECT_LANGUAGE_KEY=key))
    for version in (1, 0x1000000):
        movie.write_bytes(language_movie(packed("eng"), b"fr\0", version=version))
        subprocess.run([str(binary), str(movie)], check=True,
                       env=dict(os.environ, SUBTITLE_EXPECT_LANGUAGE_SET="1",
                                SUBTITLE_EXPECT_LANGUAGE="en"))
    movie.write_bytes(original)
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
