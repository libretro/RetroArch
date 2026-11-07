#!/usr/bin/env python3
"""vulkan_hw_level_view() from gfx/drivers/vulkan.c, without a GPU."""
from pathlib import Path
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
s = (root / "gfx/drivers/vulkan.c").read_text()
start = s.index("static bool vulkan_hw_level_view(")
s = s[start:s.index("\n}\n", start) + 3]
with tempfile.TemporaryDirectory(prefix="vulkan-hw-level-view-") as tmp:
    tmp = Path(tmp)
    (tmp / "production_level_view.h").write_text(s)
    exe = tmp / ("level.exe" if os.name == "nt" else "level")
    cmd = [os.environ.get("CC", "cc"), "-std=c99", "-Wall",
           "-Wextra", "-Werror", "-I" + str(tmp),
           "-I" + str(root / "gfx/include"),
           "-I" + str(root / "libretro-common/include"),
           str(here / "level_view_test.c"), "-o", str(exe)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True)
