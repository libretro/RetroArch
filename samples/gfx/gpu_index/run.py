#!/usr/bin/env python3
"""Run the production GPU resolver without creating graphics devices."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def block(source, start):
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def function(source, signature):
    return block(source, source.index("\n" + signature) + 1) + "\n\n"


def case(source, label):
    start = source.index(label)
    end = start
    depth = 0
    while depth or not source.startswith("break;", end):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end + len("break;")]


video = (root / "gfx/video_driver.c").read_text()
resolver = "".join(function(video, signature) for signature in (
    "static char *video_driver_gpu_name_setting(",
    "void video_driver_gpu_index_select(",
    "int video_driver_gpu_index_resolve("))
menu = function((root / "menu/cbs/menu_cbs_left.c").read_text(),
                "static int action_left_video_gpu_index(")
menu += function((root / "menu/cbs/menu_cbs_right.c").read_text(),
                 "static int action_right_video_gpu_index(")
handler = function((root / "menu/menu_setting.c").read_text(),
                   "static void general_write_handler(")
menu += "static void write_setting(rarch_setting_t *setting)\n{\n"
menu += "   switch (setting->enum_idx)\n   {\n      "
menu += case(handler, "case MENU_ENUM_LABEL_VIDEO_GPU_INDEX:")
menu += "\n      default: break;\n   }\n}\n"

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / "resolver.inc").write_text(resolver)
    (tmp / "menu.inc").write_text(menu)
    cc = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get(
        "CFLAGS", "-O1 -g -std=c89 -Wall -Wextra -Werror=declaration-after-statement"))
    exe = tmp / "gpu_index_test.exe"
    subprocess.run(cc + flags + [
        "-I" + str(tmp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include"),
        str(Path(__file__).with_name("gpu_index_test.c")),
        str(root / "libretro-common/compat/compat_strl.c"),
        "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
