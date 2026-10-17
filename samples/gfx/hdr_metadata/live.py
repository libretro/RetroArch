#!/usr/bin/env python3
"""Run replacement environment queries through the shipping frontend."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--retroarch", required=True)
p.add_argument("--driver", default="null")
p.add_argument("--log-dir", required=True)
p.add_argument("--hdr", action="store_true")
a = p.parse_args()
exe = str(Path(a.retroarch).resolve())
root = Path(__file__).resolve().parents[3]
logs = Path(a.log_dir).resolve()
logs.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="retroarch-hdr-live-") as directory:
    directory = Path(directory)
    core = directory / ("hdr_core.dll" if os.name == "nt" else "hdr_core.so")
    cc = shlex.split(os.environ.get("CC", "cc"))
    subprocess.run(cc + ["-std=gnu89", "-shared", "-fPIC", "-O1",
                        "-I" + str(root / "libretro-common/include"),
                        str(Path(__file__).with_name("live_core.c")),
                        "-o", str(core), "-lm"], check=True)
    for threaded in ("false", "true"):
        for requested in ((0, 2) if a.driver == "null" else ((0, 1, 2) if a.hdr else (0,))):
            config = directory / "retroarch.cfg"
            config.write_text(f'''video_driver = "{a.driver}"
video_threaded = "{threaded}"
video_hdr_mode = "{requested}"
video_hdr_paper_white_nits = "203"
video_hdr_max_nits = "600"
video_hdr_use_display_peak = "false"
video_hdr_expand_gamut = "2"
video_fullscreen = "false"
video_vsync = "false"
audio_enable = "false"
input_driver = "null"
input_joypad_driver = "null"
config_save_on_exit = "false"
menu_show_start_screen = "false"
''', encoding="utf-8")
            expected = requested if a.driver != "null" else 0
            if requested and a.driver in ("gl", "glcore", "gl1"):
                expected = 2
            environment = dict(os.environ, HDR_QUERY_REQUESTED_MODE=str(expected))
            result = subprocess.run([exe, "-v", "--config", str(config), "-L", str(core),
                                     "--max-frames", "20"], cwd=directory,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=45, env=environment)
            log = logs / f"{a.driver}-{threaded}-{requested}.log"
            log.write_bytes(result.stdout)
            if result.returncode or b"[hdr_metadata] PASS:" not in result.stdout:
                raise RuntimeError(f"Live query test failed: {log}")
            evidence = next(line.decode(errors="replace") for line in result.stdout.splitlines()
                            if b"[hdr_metadata] PASS:" in line)
            print(f"PASS: {a.driver}, threaded={threaded}, requested HDR={requested}; {evidence}")
