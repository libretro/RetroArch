#!/usr/bin/env python3
"""Exercise the shipping frame peak snapshot and display override selection."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
helper = re.search(r"static unsigned video_driver_display_peak_in_use\(void\)\n"
                   r"\{.*?\n\}", source, re.S).group()
snapshot = re.search(r"   video_info->hdr_display_peak\s*=.*?;\n"
                     r"   video_info->hdr_max_nits\s*=.*?;", source, re.S).group()
fixture = r'''
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct {
   struct { int video_hdr_use_display_peak; } bools;
   struct { float video_hdr_max_nits; } floats;
} settings_t;
typedef struct { float hdr_display_peak, hdr_max_nits; } frame_t;
static settings_t config;
static unsigned published, reads;
static settings_t *config_get_ptr(void) { return &config; }
static unsigned video_driver_get_display_peak_nits(void)
{
   reads++;
   return published;
}
HELPER
static void capture(frame_t *video_info)
{
   settings_t *settings = &config;
SNAPSHOT
}
int main(void)
{
   frame_t frame, queued;
   unsigned enabled, known;
   for (enabled = 0; enabled <= 1; enabled++)
   {
      for (known = 0; known <= 1; known++)
      {
         config.bools.video_hdr_use_display_peak = enabled;
         config.floats.video_hdr_max_nits = 600.0f;
         published = known ? 1000 : 0;
         reads = 0;
         capture(&frame);
         CHECK(reads == enabled);
         CHECK(frame.hdr_display_peak == (enabled && known ? 1000.0f : 0.0f));
         CHECK(frame.hdr_max_nits == (enabled && known ? 1000.0f : 600.0f));
         queued = frame;
         published = 2000;
         config.floats.video_hdr_max_nits = 800.0f;
         capture(&frame);
         CHECK(frame.hdr_max_nits == (enabled ? 2000.0f : 800.0f));
         CHECK(queued.hdr_max_nits == (enabled && known ? 1000.0f : 600.0f));
      }
   }
   puts("PASS: frame peak override, unknown fallback and queued snapshot");
   return 0;
}
'''
code = fixture.replace("HELPER", helper).replace("SNAPSHOT", snapshot)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-peak-") as directory:
    directory = Path(directory)
    src = directory / "peak.c"
    exe = directory / ("peak.exe" if os.name == "nt" else "peak")
    src.write_text(code, encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra",
                   "-Werror", "-Werror=declaration-after-statement",
                   str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
