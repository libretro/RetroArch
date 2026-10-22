#!/usr/bin/env python3
"""Enforce the VP9 production stack ceiling with and without tile workers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = [
    "-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
    "-Wno-long-long", "-Wframe-larger-than=2048", "-fstack-usage",
    "-I" + str(root / "include"),
]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True):
    flags += ["-Wno-c99-extensions"]
with tempfile.TemporaryDirectory(prefix="rvp9-stack-") as directory:
    for threaded in (False, True):
        name = "threaded" if threaded else "threadless"
        obj = Path(directory) / (name + ".o")
        command = compiler + flags + (["-DHAVE_THREADS"] if threaded else [])
        command += ["-c", str(root / "formats/vp9/rvp9.c"), "-o", str(obj)]
        subprocess.run(command, check=True)
        rows = obj.with_suffix(".su").read_text().splitlines()
        assert rows, name
        maximum = 0
        for row in rows:
            function, size, kind = row.rsplit("\t", 2)
            assert int(size) <= 2048, (function, size, kind)
            maximum = max(maximum, int(size))
        print("PASS: VP9", name, "maximum frame", maximum, "bytes")
