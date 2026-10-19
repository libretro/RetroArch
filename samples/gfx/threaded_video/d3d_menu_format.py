#!/usr/bin/env python3
"""Exercise shipping D3D menu allocation checks with GPU format substitution."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned DXGI_FORMAT;
struct texture { struct { unsigned Width, Height, Format; } desc; void *srv_heap; unsigned srv_mapping; };
struct video {
   struct { struct texture texture; DXGI_FORMAT source_format; } menu;
   struct { int srv_heap; } desc;
   void *device;
};
static unsigned allocations, releases, substitute;
static void release_texture(struct texture *texture)
{
   (void)texture;
   releases++;
}
static void init_texture(void *device, struct texture *texture)
{
   (void)device;
   assert(texture->srv_mapping == 0);
   allocations++;
   if (substitute)
      texture->desc.Format = 32;
}
MAPPING
static void submit(struct video *driver,
      unsigned width, unsigned height, DXGI_FORMAT format)
{
ALLOCATION
}
int main(void)
{
   struct video video;
   unsigned mode, i;
   const unsigned formats[]  = {16, 16, 32, 32, 16, 32};
   const unsigned expected[] = { 1,  1,  2,  2,  3,  4};
   for (mode = 0; mode < 2; mode++)
   {
      memset(&video, 0, sizeof(video));
      allocations = releases = 0;
      substitute = mode;
      for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
      {
         submit(&video, 320, 240, formats[i]);
         assert(video.menu.source_format == formats[i]);
         assert(video.menu.texture.desc.Format == (mode ? 32 : formats[i]));
         assert(allocations == expected[i] && releases == expected[i]);
      }
      submit(&video, 640, 240, 32);
      assert(allocations == 5 && releases == 5);
      submit(&video, 640, 480, 32);
      assert(allocations == 6 && releases == 6);
      submit(&video, 640, 480, 32);
      assert(allocations == 6 && releases == 6);
   }
   puts("PASS: DRIVER menu format switches, substitution and reuse");
   return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="retroarch-d3d-menu-") as directory:
    for driver in ("d3d10", "d3d11", "d3d12"):
        source = (root / f"gfx/drivers/{driver}.c").read_text(encoding="utf-8")
        start = source.index(f"static void {driver}_set_menu_texture_frame(")
        start = source.index("   if (\n", start)
        end = source.index("\n   }", start) + 5
        allocation = source[start:end]
        allocation = allocation.replace(f"{driver}_release_texture", "release_texture")
        allocation = allocation.replace(f"{driver}_init_texture", "init_texture")
        allocation = allocation.replace(f"{driver}->", "driver->")
        # D3D12's release takes the driver, to wait for frames in flight
        allocation = allocation.replace("release_texture(d3d12, ", "release_texture(")
        allocation = allocation.replace("d3d12_menu_mapping", "menu_mapping").replace("rgb32", "(format == 32)")
        mapping = "static void menu_mapping(void *device, struct texture *t, int rgb32) { (void)device; t->srv_mapping = rgb32 ? 0 : 7; }" if driver == "d3d12" else ""
        code = fixture.replace("MAPPING", mapping).replace("ALLOCATION", allocation).replace("DRIVER", driver)
        src = Path(directory) / f"{driver}.c"
        exe = Path(directory) / (driver + ".exe")
        src.write_text(code, encoding="utf-8")
        command = shlex.split(os.environ.get("CC", "cc"))
        command += ["-std=gnu89", "-Wall", "-Wextra", "-Werror"]
        command += shlex.split(os.environ.get("CFLAGS", ""))
        subprocess.run(command + [str(src), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
