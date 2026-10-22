#!/usr/bin/env python3
"""Regenerate the native VP9 lossless fixtures and decoded-plane FNV references."""
from pathlib import Path
import subprocess
import tempfile

out = ["/* Lossless testsrc 128x96, four frames; FFmpeg/libvpx reference. */",
       "#ifndef VT_VP9_NATIVE_FIXTURE_H", "#define VT_VP9_NATIVE_FIXTURE_H"]
with tempfile.TemporaryDirectory(prefix="vt-vp9-fixture-") as directory:
    path = Path(directory)
    for depth in (8, 10):
        fmt = "yuv420p" if depth == 8 else "yuv420p10le"
        stream = path / (str(depth) + ".ivf")
        reference = stream.with_suffix(".yuv")
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
            "testsrc=size=128x96:rate=4", "-vf", "format=" + fmt,
            "-frames:v", "4", "-c:v", "libvpx-vp9", "-profile:v",
            "0" if depth == 8 else "2", "-g", "2", "-lag-in-frames", "0",
            "-threads", "2", "-lossless", "1", "-color_primaries", "bt709",
            "-color_trc", "bt709", "-colorspace", "bt709", "-color_range", "tv",
            "-y", str(stream)], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-i", str(stream), "-pix_fmt",
            fmt, "-f", "rawvideo", "-y", str(reference)], check=True)
        data, raw = stream.read_bytes(), reference.read_bytes()
        size = 128 * 96 * 3 // 2 * (1 if depth == 8 else 2)
        assert len(raw) == 4 * size
        out.append("static const uint8_t native_ivf" + str(depth) + "[] = {")
        for i in range(0, len(data), 16):
            out.append("   " + ", ".join("0x%02x" % v for v in data[i:i + 16]) + ",")
        out.append("};")
        hashes = []
        for frame in range(4):
            value = 2166136261
            for byte in raw[frame * size:(frame + 1) * size]:
                value = ((value ^ byte) * 16777619) & 0xffffffff
            hashes.append(value)
        out.append("static const uint32_t native_hash" + str(depth) + "[4] = {"
            + ", ".join("0x%08xu" % value for value in hashes) + "};")
out.append("#endif")
Path(__file__).with_name("native_fixture.h").write_text(
    "\n".join(out) + "\n", encoding="utf-8", newline="\n")
