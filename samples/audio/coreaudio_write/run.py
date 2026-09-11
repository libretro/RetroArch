#!/usr/bin/env python3
"""Compile the shipping CoreAudio ring/writer with deterministic OS stubs."""
import argparse
import pathlib
import re
import subprocess
import tempfile
import contextlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r"^static [^\n]*\b" + name + r"\(", source, re.M)
    if not match:
        raise ValueError("Function not found: " + name)
    start = source.index("{", match.start())
    # All selected definitions close at column zero.
    end = source.index("\n}", start) + 2
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--source", type=pathlib.Path,
                        default=ROOT / "audio/drivers/coreaudio.c")
    parser.add_argument("--build-dir", type=pathlib.Path)
    args = parser.parse_args()
    source = args.source.read_text()
    names = ("rb_write_avail", "rb_write", "rb_read", "coreaudio_signal",
             "coreaudio_wait", "coreaudio_audio_write_cb", "coreaudio_run",
             "coreaudio_unit_stalled", "coreaudio_write",
             "coreaudio_wait_writable")
    fixture = (HERE / "coreaudio_write_test.c").read_text()
    fixture = fixture.replace("/* DRIVER_FUNCTIONS */", "\n\n".join(function(source, n) for n in names))
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
    build = (contextlib.nullcontext(args.build_dir) if args.build_dir else
             tempfile.TemporaryDirectory(prefix="coreaudio-wait-"))
    with build as tmp:
        test = pathlib.Path(tmp) / "test.c"
        exe = pathlib.Path(tmp) / "test.exe"
        test.write_text(fixture)
        subprocess.run([args.cc, "-std=gnu89", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(ROOT / "libretro-common/include"),
                        str(test), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
