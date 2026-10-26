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


def webvtt(data, mode):
    prelude = b"WEBVTT\n\nSTYLE\n::cue { color: lime; }\n\nREGION\nid:lower\n\n"
    if mode == "bad-header":
        prelude = b"WEBVTT\n\nSTYLE\n" + bytes([255])
    elif mode == "header-cue":
        prelude = b"WEBVTT\n\n00:00.000 --> 00:01.000\nnot global"
    output = bytearray()
    for ident, body in ebml.elements(data):
        if ident in (0x114D9B74, 0x1C53BB6B, 0x63A2):
            continue
        if ident in (0x18538067, 0x1654AE6B, 0xAE, 0x1F43B675):
            body = webvtt(body, mode)
            if ident == 0xAE:
                body += ebml.element(0x63A2, prelude)
                if mode == "delay":
                    body += ebml.element(0x56AA, (500000123).to_bytes(4, "big"))
        elif ident == 0x86:
            assert body == b"S_TEXT/UTF8"
            body = b"S_TEXT/WEBVTT"
        elif ident == 0xA0:
            children = list(ebml.elements(body))
            packet = next(payload for child, payload in children if child == 0xA1)
            first = b"first " in packet
            settings = b"align:start" if first else b"align:end"
            identifier = b"first-id" if first else b"second-id"
            if mode == "bad-settings":
                settings = bytes([255])
            elif mode == "long-id":
                identifier = b"x" * 129
            metadata = settings + b"\n" + identifier
            if mode == "bad-lines":
                metadata = b"no separator"
            if mode == "comments":
                metadata += b"\nNOTE original comment\nretained cue"
            add_id = 0 if mode == "zero-id" else 1
            more = ebml.element(0xA5, metadata)
            if mode != "default-id":
                more = ebml.element(0xEE, bytes([add_id])) + more
            additions = ebml.element(0xA6, more)
            if mode == "unknown-id":
                additions = ebml.element(0xA6, ebml.element(0xEE, b"\2")
                                         + ebml.element(0xA5, b"not WebVTT")) + additions
            elif mode == "duplicate-id":
                additions *= 2
            elif mode == "too-many":
                additions = ebml.element(0xEC, b"") * 257 + additions
            extra = b"" if mode == "absent" else ebml.element(0x75A1, additions)
            body = b"".join(ebml.element(child, payload) for child, payload in children)
            body = extra + body if mode == "before" else body + extra
        output += ebml.element(ident, body)
    return output


def languages(data, fields):
    output = bytearray()
    first_track = True
    for ident, body in ebml.elements(data):
        if ident in (0xBF, 0x114D9B74, 0x1C53BB6B, 0x22B59C, 0x22B59D):
            continue
        if ident in (0x18538067, 0x1654AE6B):
            body = languages(body, fields)
        elif ident == 0xAE and first_track:
            body = languages(body, fields)
            body += b"".join(ebml.element(key, value) for key, value in fields)
            first_track = False
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
    legacy, bcp47 = 0x22B59C, 0x22B59D
    cases = [([], "en", "en"), ([(legacy, b"eng")], "en", "en"),
             ([(legacy, b"fre-ca")], "fr-ca", "fr-ca"),
             ([(legacy, b"deu")], "de", "de"),
             ([(legacy, b"FRE-CA")], "fr-CA", "fr-CA"),
             ([(legacy, b"qaa")], "qaa", "qaa"),
             ([(legacy, b"")], "en", "en"),
             ([(legacy, b"\0")], "", ""),
             ([(legacy, b"fre-ca\0ignored")], "fr-ca", "fr-ca"),
             ([(legacy, b"eng\0" + bytes(65536))], "en", "en"),
             ([(bcp47, b"eng")], "eng", "eng"),
             ([(bcp47, b"iw")], "iw", "he"),
             ([(bcp47, b"en-GB\0ignored")], "en-GB", "en-GB"),
             ([(legacy, b"ger"), (bcp47, b"fr-FR")], "fr-FR", "fr-FR"),
             ([(bcp47, b"fr-FR"), (legacy, b"ger")], "fr-FR", "fr-FR"),
             ([(legacy, b"ger"), (bcp47, b"")], "", ""),
             ([(bcp47, b""), (legacy, b"ger")], "", ""),
             ([(bcp47, b"en--US")], "en--US", "en--US")]
    language_file = root / "language.mkv"
    for fields, expected, key in cases:
        language_file.write_bytes(languages(original, fields))
        subprocess.run([str(binary), str(language_file)], check=True,
                       env=dict(env, SUBTITLE_EXPECT_LANGUAGE_SET="1",
                                SUBTITLE_EXPECT_LANGUAGE=expected,
                                SUBTITLE_EXPECT_LANGUAGE_KEY=key))
    print("PASS: owned Matroska track language, legacy conversion and BCP47 precedence", flush=True)
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
    vtt = root / "webvtt.mkv"
    for mode in ("before", "after", "default-id", "unknown-id", "comments", "absent", "delay"):
        vtt.write_bytes(webvtt(original, mode))
        options = dict(env, SUBTITLE_EXPECT_WEBVTT="1")
        if mode == "absent":
            options["SUBTITLE_EXPECT_VTT_EMPTY"] = "1"
        if mode == "delay":
            options["SUBTITLE_EXPECT_VTT_DELAY"] = "1"
        subprocess.run([str(binary), str(vtt)], check=True, env=options)
        print("PASS: Matroska WebVTT " + mode, flush=True)
    sparse.write_bytes(webvtt(output, "after"))
    subprocess.run([str(binary), str(sparse)], check=True,
                   env=dict(env, SUBTITLE_EXPECT_WEBVTT="1"))
    print("PASS: sparse Matroska WebVTT bounded file reads", flush=True)
    if malformed:
        for mode in ("bad-header", "header-cue", "bad-settings", "long-id",
                     "bad-lines", "zero-id", "duplicate-id", "too-many"):
            vtt.write_bytes(webvtt(original, mode))
            subprocess.run([str(binary), str(vtt)], check=True,
                           env=dict(env, SUBTITLE_EXPECT_FAILURE="1"))
            print("PASS: refused Matroska WebVTT " + mode, flush=True)
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
