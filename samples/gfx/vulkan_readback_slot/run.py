#!/usr/bin/env python3
"""Exercise the production synchronous HDR fence selection without a GPU."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, default=root / "gfx/drivers/vulkan.c")
a = p.parse_args()
s = a.source.read_text()
s = s[s.index("static bool vulkan_read_viewport_hdr("):]
start = s.index("   readback_slot =") if "   readback_slot =" in s else s.index("   staging = &vk->readback.staging[")
s = s[start:s.index("   vk->flags &= ~VK_FLAG_READBACK_HDR;")]
header = "static unsigned probe(vk_t *vk, bool is_idle)\n{\n   struct vk_texture *staging;\n"
if "readback_slot" in s:
    header += "   unsigned readback_slot;\n"
header += s + "   return (unsigned)(staging - vk->readback.staging);\n}\n"
with tempfile.TemporaryDirectory(prefix="vulkan-readback-slot-") as tmp:
    tmp = Path(tmp)
    (tmp / "production_wait.h").write_text(header)
    exe = tmp / ("slot.exe" if os.name == "nt" else "slot")
    cmd = [os.environ.get("CC", "cc"), "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-I" + str(tmp), str(here / "slot_test.c"), "-o", str(exe)]
    subprocess.run(cmd, check=True)
    if os.name == "nt":
        cmd = [os.environ.get("GDB", "gdb"), "--batch", "-return-child-result", "-ex", "run", "-ex", "thread apply all bt", "--args", str(exe)]
    else:
        cmd = [str(exe)]
    subprocess.run(cmd, check=True)
