#!/usr/bin/env python3
"""Compare reconstructed VP9 planes to FFmpeg for scalar and tiled workers."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1] if len(sys.argv) > 1 else "./rvp9_pixel_test").resolve()
workers = [int(value) for value in os.environ.get("PIXEL_WORKERS", "1,4").split(",")]
with tempfile.TemporaryDirectory(prefix="rvp9-pixels-") as directory:
    path = Path(directory)
    for depth in (8, 10):
        geometries = []
        for mode in ("crop-lossless", "tiles-aq"):
            fmt = "yuv420p" if depth == 8 else "yuv420p10le"
            size = "135x73" if mode == "crop-lossless" else "1024x160"
            source = "testsrc" if mode == "crop-lossless" else "testsrc2"
            stream = path / (str(depth) + "-" + mode + ".ivf")
            reference = stream.with_suffix(".ref")
            flags = ["-lossless", "1"] if mode == "crop-lossless" else [
                "-crf", "28", "-b:v", "0", "-aq-mode", "1", "-tile-columns", "2"
            ]
            subprocess.run([
                "ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                source + "=size=" + size + ":rate=24", "-vf", "format=" + fmt,
                "-frames:v", "24", "-c:v", "libvpx-vp9", "-profile:v",
                "0" if depth == 8 else "2", "-g", "8", "-lag-in-frames", "0",
                "-threads", "2", *flags, "-y", str(stream)
            ], check=True)
            subprocess.run([
                "ffmpeg", "-v", "error", "-i", str(stream), "-pix_fmt", fmt,
                "-f", "rawvideo", "-y", str(reference)
            ], check=True)
            expected = reference.read_bytes()
            assert expected, stream
            geometries.append((stream.read_bytes(), expected))
            for count in workers:
                actual = stream.with_suffix("." + str(count) + ".raw")
                subprocess.run([str(binary), str(stream), str(actual), str(count)], check=True)
                assert actual.read_bytes() == expected, (stream.name, count)
                print("PASS: byte-exact VP9", depth, mode, "workers", count)
        # Keyframes switch geometry in the same decoder while earlier outputs
        # remain retained. Both old and new pictures must survive decoder free.
        joined = path / (str(depth) + "-geometry.ivf")
        joined.write_bytes(geometries[0][0][:32] + b"".join(
            data[32:] for data, _ in geometries))
        expected = b"".join(pixels for _, pixels in geometries)
        for count in workers:
            actual = joined.with_suffix("." + str(count) + ".raw")
            subprocess.run([str(binary), str(joined), str(actual), str(count)], check=True)
            assert actual.read_bytes() == expected, (joined.name, count)
            print("PASS: retained VP9 geometry change", depth, "workers", count)
    geometries = []
    for size in ("66x50", "192x112"):
        stream = path / ("vp8-" + size + ".ivf")
        reference = stream.with_suffix(".ref")
        subprocess.run([
            "ffmpeg", "-v", "error", "-f", "lavfi", "-i",
            "testsrc2=size=" + size + ":rate=24", "-frames:v", "32",
            "-c:v", "libvpx", "-g", "8", "-lag-in-frames", "0",
            "-deadline", "realtime", "-y", str(stream)
        ], check=True)
        subprocess.run([
            "ffmpeg", "-v", "error", "-i", str(stream), "-pix_fmt", "yuv420p",
            "-f", "rawvideo", "-y", str(reference)
        ], check=True)
        expected = reference.read_bytes()
        actual = stream.with_suffix(".raw")
        subprocess.run([str(binary), str(stream), str(actual), "1"], check=True)
        assert actual.read_bytes() == expected, stream.name
        geometries.append((stream.read_bytes(), expected))
        print("PASS: retained byte-exact VP8", size)
    joined = path / "vp8-geometry.ivf"
    joined.write_bytes(geometries[0][0][:32] + b"".join(
        data[32:] for data, _ in geometries))
    actual = joined.with_suffix(".raw")
    subprocess.run([str(binary), str(joined), str(actual), "1"], check=True)
    assert actual.read_bytes() == b"".join(pixels for _, pixels in geometries)
    print("PASS: retained VP8 geometry change")
