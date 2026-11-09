#!/usr/bin/env python3
"""Check the shipping ASIO ring lend on a real ring."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "audio/drivers/asio.c").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

with tempfile.TemporaryDirectory(prefix="retroarch-asio-lend-") as directory:
    temp = Path(directory)
    (temp / "asio_lend.inc").write_text("\n".join(function(name) for name in (
        "static size_t asio_ring_room(",
        "static size_t ra_asio_write_begin(",
        "static ssize_t ra_asio_write_end(")), encoding="utf-8")
    exe = temp / "asio_lend"
    common = root / "libretro-common"
    subprocess.run(["cc", "-std=gnu99", "-O1", "-g1", "-Wall", "-Wextra",
        "-fsanitize=address,undefined", "-I" + str(temp), "-I" + str(root),
        "-I" + str(common / "include"),
        str(Path(__file__).with_name("asio_lend_test.c")),
        str(common / "queues/retro_spsc.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
