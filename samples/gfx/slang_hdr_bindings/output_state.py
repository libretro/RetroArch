#!/usr/bin/env python3
"""Exercise shipping D3D HDR uniform state through output transitions."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))


def function(source, name):
    match = re.search(r"^static unsigned " + name + r"\(", source, re.M)
    assert match, name
    brace = source.index("{", match.start())
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


fixture = r'''
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
#define DXGI_SWAPCHAIN_BIT_DEPTH_10 10
#define DXGI_SWAPCHAIN_BIT_DEPTH_16 16
#define DRIVER_FLAG 1
typedef struct { unsigned hdr_mode, expand_gamut, subpixel_layout;
   float paper_white_nits, scanlines, inverse_tonemap, hdr10, peak_nits, enable_hdr; } pass_t;
typedef struct { unsigned flags, chain_bit_depth;
   struct { unsigned bit_depth; } chain;
   struct { pass_t ubo_values; } hdr; } DRIVER_video_t;
HELPER
static void update(DRIVER_video_t *DRIVER, pass_t *pass, unsigned requested)
{
   struct { unsigned hdr_mode; float hdr_max_nits; } info;
   const void *unused;
   info.hdr_mode = requested;
   info.hdr_max_nits = 600.0f;
   unused = &info;
   (void)unused;
   (void)DRIVER;
   (void)pass;
#define video_info (&info)
BLOCK
#undef video_info
}
int main(void)
{
   DRIVER_video_t video;
   pass_t pass;
   unsigned depth;
   memset(&video, 0, sizeof(video));
   memset(&pass, 0, sizeof(pass));
   CHECK(DRIVER_hdr_output_mode(&video) == 0);
   video.hdr.ubo_values.paper_white_nits = 203.0f;
   video.hdr.ubo_values.scanlines = 1.0f;
   video.hdr.ubo_values.subpixel_layout = 2;
   video.hdr.ubo_values.expand_gamut = 2;
#ifdef HAVE_DXGI_HDR
   for (depth = 10; depth <= 16; depth += 6)
   {
      video.flags = DRIVER_FLAG;
      video.chain_bit_depth = video.chain.bit_depth = depth;
      pass.inverse_tonemap = 0.25f;
      pass.hdr10 = 0.5f;
      update(&video, &pass, depth == 10 ? 2 : 1);
      CHECK(pass.hdr_mode == (depth == 10 ? 1u : 2u));
      CHECK(pass.enable_hdr == 1.0f);
      CHECK(pass.peak_nits == (DRIVER_hdr_output_mode(&video) ? 600.0f : 0.0f));
      CHECK(pass.paper_white_nits == 203.0f && pass.scanlines == 1.0f);
      CHECK(pass.expand_gamut == 2 && pass.subpixel_layout == 2);
      CHECK(pass.inverse_tonemap == 0.25f && pass.hdr10 == 0.5f);
      video.flags = 0;
      update(&video, &pass, 2);
      CHECK(pass.hdr_mode == 0 && pass.paper_white_nits == 0.0f);
      CHECK(pass.enable_hdr == 0.0f);
      CHECK(pass.scanlines == 0.0f && pass.subpixel_layout == 0);
      CHECK(pass.expand_gamut == 0);
      CHECK(pass.inverse_tonemap == 0.25f && pass.hdr10 == 0.5f);
   }
   video.flags = DRIVER_FLAG;
   video.chain_bit_depth = video.chain.bit_depth = 8;
   update(&video, &pass, 2);
   CHECK(pass.hdr_mode == 0 && pass.paper_white_nits == 0.0f);
   CHECK(pass.enable_hdr == 0.0f);
#else
   (void)depth;
   video.flags = DRIVER_FLAG;
   CHECK(DRIVER_hdr_output_mode(&video) == 0);
   update(&video, &pass, 2);
   CHECK(pass.hdr_mode == 0);
#endif
   puts("PASS: DRIVER actual output, HDR-to-SDR clearing and compiled fallback");
   return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="retroarch-hdr-output-") as directory:
    directory = Path(directory)
    for driver in ("d3d11", "d3d12"):
        source = (root / f"gfx/drivers/{driver}.c").read_text(encoding="utf-8")
        start = source.index("      pass->hdr_mode =")
        end = source.index("#endif", start)
        block = "#ifdef HAVE_DXGI_HDR\n" + source[start:end] + "#endif\n"
        helper = function(source, driver + "_hdr_output_mode")
        code = fixture.replace("HELPER", helper).replace("BLOCK", block)
        code = code.replace("DRIVER_FLAG", driver.upper() + "_ST_FLAG_HDR_ENABLE")
        code = code.replace("DRIVER", driver)
        src = directory / "output.c"
        exe = directory / ("output.exe" if os.name == "nt" else "output")
        src.write_text(code, encoding="utf-8")
        for hdr in (False, True):
            defines = ["-DHAVE_DXGI_HDR"] if hdr else []
            subprocess.run(cc + flags + defines + ["-std=gnu89", "-O1", "-Wall",
                           "-Wextra", "-Werror", "-Werror=declaration-after-statement",
                           str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

# Execute the actual Vulkan HDR block with prefilled UBO/push storage.
source = (root / "gfx/drivers_shader/shader_vulkan.c").read_text(encoding="utf-8")
header = (root / "gfx/drivers_shader/slang_process.h").read_text(encoding="utf-8")
semantic_enum = re.search(r"enum slang_semantic\s*\{.*?\};", header, re.S).group()
start = source.index("#ifdef VULKAN_HDR_SWAPCHAIN", source.index("static void slang_pass_build_semantics("))
block = source[start:source.index("#endif /* VULKAN_HDR_SWAPCHAIN */", start)] + "#endif"
def helper(name):
    start = source.index("static void " + name + "(")
    return source[start:source.index("\n}\n", start) + 3]
code = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
ENUM
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct { unsigned uniform, push_constant, ubo_offset, push_constant_offset; } slang_semantic_meta;
struct common {
#ifdef VULKAN_HDR_SWAPCHAIN
   unsigned hdr_mode, subpixel_layout, expand_gamut;
   float paper_white_nits, peak_nits, scanlines, inverse_tonemap, hdr10;
#endif
   unsigned unused;
};
struct slang_pass {
   struct { slang_semantic_meta semantics[SLANG_NUM_BASE_SEMANTICS]; } reflection;
   struct { uint32_t *buffer; } push;
   struct common *common;
};
HELPERS
static void update(struct slang_pass *pass, uint8_t *buffer)
{
   (void)pass;
   (void)buffer;
BLOCK
}
int main(void)
{
   struct slang_pass pass;
   struct common common;
   slang_semantic_meta *refl;
   float ubo, push;
   unsigned mode, declared, storage;
   memset(&pass, 0, sizeof(pass));
   memset(&common, 0, sizeof(common));
   pass.common = &common;
   pass.push.buffer = (uint32_t*)&push;
   refl = &pass.reflection.semantics[SLANG_SEMANTIC_ENABLE_HDR];
   for (mode = 0; mode <= 2; mode++)
      for (declared = 0; declared <= 1; declared++)
         for (storage = 0; storage <= 1; storage++)
         {
            float expected = 7.0f;
            ubo = push = 7.0f;
            refl->uniform = declared && !storage;
            refl->push_constant = declared && storage;
#ifdef VULKAN_HDR_SWAPCHAIN
            common.hdr_mode = mode;
#endif
            update(&pass, (uint8_t*)&ubo);
            if (declared)
            {
#ifdef VULKAN_HDR_SWAPCHAIN
               expected = mode ? 1.0f : 0.0f;
#else
               expected = 0.0f;
#endif
            }
            CHECK((storage ? push : ubo) == expected);
            CHECK((storage ? ubo : push) == 7.0f);
         }
   puts("PASS: Vulkan active HDR float, declared-only UBO/push and compiled fallback");
   return 0;
}
'''
code = code.replace("ENUM", semantic_enum).replace("HELPERS", helper("slang_pass_build_semantic_uint") + helper("slang_pass_build_semantic_float")).replace("BLOCK", block)
with tempfile.TemporaryDirectory(prefix="retroarch-vulkan-hdr-output-") as directory:
    directory = Path(directory)
    src = directory / "output.c"
    exe = directory / ("output.exe" if os.name == "nt" else "output")
    src.write_text(code, encoding="utf-8")
    for hdr in (False, True):
        defines = ["-DVULKAN_HDR_SWAPCHAIN"] if hdr else []
        subprocess.run(cc + flags + defines + ["-std=gnu89", "-O1", "-Wall",
                       "-Wextra", "-Werror", "-Werror=declaration-after-statement",
                       "-Wno-unused-function", "-fno-strict-aliasing",
                       str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
