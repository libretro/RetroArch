#!/usr/bin/env python3
"""Regenerate lossless MP4/WebM VP9 fixtures and decoded-plane CRC references."""
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

out = ["/* Generated lossless 64x64 VP9 stream fixtures; see generate_pipeline_fixture.py. */",
       "#ifndef VT_VP9_PIPELINE_FIXTURE_H", "#define VT_VP9_PIPELINE_FIXTURE_H"]
references = {}
cases = [(8, 0, "bt709", "bt709", "bt709", 1),
         (8, 1, "bt709", "iec61966-2-1", "bt709", 13),
         (10, 0, "bt2020", "smpte2084", "bt2020nc", 16),
         (10, 1, "bt2020", "arib-std-b67", "bt2020nc", 18)]
with tempfile.TemporaryDirectory(prefix="vt-vp9-pipeline-fixture-") as directory:
    path = Path(directory)
    for case, (depth, full, primaries, transfer, matrix, code) in enumerate(cases * 2):
        fmt = "yuv420p" if depth == 8 else "yuv420p10le"
        color = ["-color_primaries", primaries, "-color_trc", transfer,
                 "-colorspace", matrix, "-color_range", "pc" if full else "tv"]
        stream = path / (str(case) + ".ivf")
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
            "testsrc=size=64x64:rate=4", "-vf", "format=" + fmt,
            "-frames:v", "4", "-c:v", "libvpx-vp9", "-profile:v",
            "0" if depth == 8 else "2", "-g", "2", "-lag-in-frames", "0",
            "-threads", "2", "-lossless", "1", *color, "-y", str(stream)], check=True)
        if case >= len(cases):
            # A hidden keyframe refreshes all slots; show-existing slot zero
            # supplies the visible picture. The superframe must retain both
            # coded units through asynchronous packet-decoder completion.
            original = stream.read_bytes()
            length = struct.unpack_from("<I", original, 32)[0]
            key = bytearray(original[44:44 + length])
            assert len(key) == length and key[0] & 2 and length < 65536
            key[0] &= ~2
            repeat = bytes([0x88 if depth == 8 else 0x98])
            marker = bytes([0xc9])
            packet = key + repeat + marker + struct.pack("<HH", length, 1) + marker
            stream.write_bytes(original[:32] + struct.pack("<I", len(packet))
                               + original[36:44] + packet + original[44 + length:])
        for container in ("mp4", "webm"):
            movie = stream.with_suffix("." + container)
            reference = stream.with_suffix("." + container + ".yuv")
            subprocess.run(["ffmpeg", "-v", "error", "-i", str(stream),
                "-c:v", "copy", *color, *(["-movflags", "+faststart"] if container == "mp4" else []),
                "-y", str(movie)], check=True)
            subprocess.run(["ffmpeg", "-v", "error", "-i", str(movie), "-pix_fmt",
                fmt, "-f", "rawvideo", "-y", str(reference)], check=True)
            data, raw = movie.read_bytes(), reference.read_bytes()
            if case >= len(cases):
                assert raw == references[(case % len(cases), container)]
            else:
                references[(case, container)] = raw
            stride = 1 if depth == 8 else 2
            frame_size = 64 * 64 * 3 // 2 * stride
            assert len(raw) == 4 * frame_size
            label = "pipeline_" + container + str(case)
            out.append("static const uint8_t " + label + "[] = {")
            for i in range(0, len(data), 16):
                out.append("   " + ", ".join("0x%02x" % v for v in data[i:i + 16]) + ",")
            out.append("};")
            out.append("static const uint32_t " + label + "_crc[4][3] = {")
            for frame in range(4):
                offset = frame * frame_size
                sizes = (64 * 64 * stride, 32 * 32 * stride, 32 * 32 * stride)
                crc = []
                for size in sizes:
                    crc.append(zlib.crc32(raw[offset:offset + size]))
                    offset += size
                out.append("   {" + ", ".join("0x%08xu" % value for value in crc) + "},")
            out.append("};")
out += ["typedef struct vp9_pipeline_fixture", "{", "   const uint8_t *data;",
        "   size_t size;", "   const uint32_t (*crc)[3];", "   unsigned webm, depth, full, transfer, hidden_repeat;",
        "} vp9_pipeline_fixture_t;", "static const vp9_pipeline_fixture_t pipeline_fixtures[] = {"]
for case, (depth, full, primaries, transfer, matrix, code) in enumerate(cases * 2):
    for container in ("mp4", "webm"):
        label = "pipeline_" + container + str(case)
        out.append("   {%s, sizeof(%s), %s_crc, %u, %u, %u, %u, %u}," %
                   (label, label, label, container == "webm", depth, full, code, case >= len(cases)))
out += ["};", "#endif"]
Path(__file__).with_name("pipeline_fixture.h").write_text(
    "\n".join(out) + "\n", encoding="utf-8", newline="\n")
