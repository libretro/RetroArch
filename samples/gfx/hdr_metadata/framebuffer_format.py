#!/usr/bin/env python3
"""Execute the shipping framebuffer descriptors for every admitted format."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
fixture = r'''#include <libretro.h>
#include <stdio.h>
struct info { unsigned source_fp16, source_hdr10, source_10bit, rgb32; };
static enum retro_pixel_format choose(struct info info, unsigned bpp)
{
   struct retro_framebuffer result, *fb = &result;
   (void)bpp;
   (void)info.source_fp16;
ASSIGNMENT
   return fb->format;
}
int main(void)
{
   static const struct test {
      struct info info;
      unsigned bpp;
      enum retro_pixel_format expected;
   } tests[] = {
      {{0,0,0,0},2,RETRO_PIXEL_FORMAT_RGB565},
      {{0,0,0,1},4,RETRO_PIXEL_FORMAT_XRGB8888},
      {{0,0,1,1},4,RETRO_PIXEL_FORMAT_XRGB2101010},
      {{0,1,1,1},4,RETRO_PIXEL_FORMAT_HDR10_2101010},
      {{1,0,0,1},8,RETRO_PIXEL_FORMAT_SCRGB_RGBA16F}
   };
   unsigned i;
   for (i = 0; i < COUNT; i++)
      if (choose(tests[i].info, tests[i].bpp) != tests[i].expected)
      {
         fprintf(stderr, "FAIL LABEL format %u\n", i);
         return 1;
      }
   puts("PASS LABEL framebuffer format descriptors");
   return 0;
}
'''
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-fb-format-") as directory:
    directory = Path(directory)
    for filename, member, prefix, count in (
        ("gfx/video_thread_wrapper.c", "fb", "thr->info", 5),
        ("gfx/drivers/vulkan.c", "framebuffer", "vk->video", 5),
        ("gfx/drivers/gl2.c", "fb", "gl->video_info", 4),
    ):
        source = (root / filename).read_text(encoding="utf-8")
        assignment = re.search(r"   " + member + r"->format\s*=.*?;", source, re.S).group()
        assignment = assignment.replace(prefix, "info").replace("gl->base_size", "bpp").replace("framebuffer->", "fb->")
        code = fixture.replace("ASSIGNMENT", assignment).replace("COUNT", str(count)).replace("LABEL", filename)
        src = directory / "format.c"
        exe = directory / ("format.exe" if os.name == "nt" else "format")
        src.write_text(code, encoding="utf-8")
        subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                                    "-Werror=declaration-after-statement", "-I" + str(root / "libretro-common/include"),
                                    str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

    source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
    start = source.index("static void *d3d12_frame_mem_alloc(")
    allocator = source[start:source.index("\n}\n", start) + 3]
    guard = re.search(r"   if \(.*?\n      return NULL;", allocator, re.S).group()
    code = r'''#include <stddef.h>
#include <stdio.h>
#define DXGI_FORMAT_R10G10B10A2_UNORM 24
struct video { void *device; unsigned format; };
static void *admit(struct video *d3d12, size_t size)
{
GUARD
   return d3d12;
}
int main(void)
{
   struct video v;
   v.device = &v;
   v.format = DXGI_FORMAT_R10G10B10A2_UNORM;
   if (admit(&v, 4096) != &v) return 1;
   v.format = 87;
   if (admit(&v, 4096) != &v) return 2;
   v.format = 10;
   if (admit(&v, 4096) != &v) return 3;
   if (admit(&v, 0) != NULL || admit(NULL, 4096) != NULL) return 4;
   v.device = NULL;
   if (admit(&v, 4096) != NULL) return 5;
   puts("PASS D3D12 frame memory admits raw packed frames");
   return 0;
}
'''.replace("GUARD", guard)
    src.write_text(code, encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                                "-Werror=declaration-after-statement", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
