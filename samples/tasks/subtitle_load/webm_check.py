#!/usr/bin/env python3
"""Check embedded UTF-8 text and sparse WebM packet traversal."""
from pathlib import Path
import importlib.util
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
helper = Path(__file__).resolve().parents[2] / "gfx/gfx_thumbnail_preview/webm_audio_padding.py"
spec = importlib.util.spec_from_file_location("ebml", helper)
ebml = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ebml)
def rewrite(data, mode):
    output = bytearray()
    for ident, body in ebml.elements(data):
        if ident in (0x114D9B74, 0x1C53BB6B):
            continue
        if ident in (0x18538067, 0x1654AE6B, 0xAE, 0x1F43B675):
            body = rewrite(body, mode)
            if ident == 0xAE and mode == "default":
                body += ebml.element(0x23E383, (1100000000).to_bytes(4, "big"))
        elif ident == 0xA0:
            children = list(ebml.elements(body))
            if mode == "default":
                body = bytearray(next(packet for child, packet in children if child == 0xA1))
                body[ebml.vint(body, 0)[1] + 2] |= 0x80
                ident = 0xA3
            else:
                body = b"".join(ebml.element(child, bytes([255]) * 8 if child == 0x9B and mode == "overflow" else packet)
                                for child, packet in children if mode != "missing" or child != 0x9B)
            if mode == "many":
                body = ebml.element(0xEC, b"") * 300 + body
        output += ebml.element(ident, body)
    return output


with tempfile.TemporaryDirectory(prefix="subtitle-webm-") as temporary:
    root = Path(temporary)
    source = root / "captions.srt"
    source.write_text("1\n00:00:00,100 --> 00:00:01,200\nfirst caf\u00e9\n\n"
                      "2\n00:00:02,000 --> 00:00:03,500\nsecond \U0001f600\n", encoding="utf-8")
    movie = root / "captions.mkv"
    subprocess.run([os.environ.get("FFMPEG", "ffmpeg"), "-v", "error", "-y", "-i", str(source),
                    "-map", "0:0", "-map", "0:0", "-c:s", "srt", str(movie)], check=True)
    source.unlink()
    env = dict(os.environ, SUBTITLE_EXPECT_WEBM="1")
    subprocess.run([str(binary), str(movie)], check=True, env=env)
    original = movie.read_bytes()
    output = bytearray()
    for ident, body in ebml.elements(original):
        if ident == 0x18538067:
            segment = bytearray()
            for child, payload in ebml.elements(body):
                if child in (0x114D9B74, 0x1C53BB6B):
                    continue
                if child == 0x1F43B675:
                    payload = ebml.element(0xEC, bytes(64 * 1024 * 1024)) + payload
                segment += ebml.element(child, payload)
            body = segment
        output += ebml.element(ident, body)
    sparse = root / "sparse.webm"
    sparse.write_bytes(output)
    subprocess.run([str(binary), str(sparse)], check=True, env=env)
    metadata = root / "metadata.mkv"
    metadata.write_bytes(rewrite(original, "many"))
    subprocess.run([str(binary), str(metadata)], check=True, env=env)
    default = root / "default.mkv"
    default.write_bytes(rewrite(original, "default"))
    subprocess.run([str(binary), str(default)], check=True,
                   env=dict(env, SUBTITLE_EXPECT_DEFAULT="1"))
    if malformed:
        text = "first caf\u00e9".encode("utf-8")
        assert original.count(text) == 2
        bad = root / "bad.mkv"
        invalid = [original.replace(text, bytes([255]) * len(text)),
                   original.replace(text, b"a\0" + bytes(len(text) - 2)),
                   rewrite(original, "overflow"), rewrite(original, "missing")]
        for data in invalid:
            bad.write_bytes(data)
            subprocess.run([str(binary), str(bad)], check=True,
                           env=dict(env, SUBTITLE_EXPECT_FAILURE="1"))
