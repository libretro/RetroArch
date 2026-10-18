#!/usr/bin/env python3
"""Exercise the shipping HDR declaration parser and Metal ownership policy."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_shader_parse.c").read_text(encoding="utf-8")


def function(name):
    match = re.search(r"^(?:static )?bool " + name + r"\(", source, re.M)
    assert match, name
    brace = source.index("{", match.start())
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


metal = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
start = metal.index("               if (_engine.pass[i].semantics.explicit_format)")
end = metal.index("               /* Method-scope flags", start)
metal = metal[start:end]

fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/drivers_shader/glslang_util.h"
static const char *value;
static struct config_entry_list entry;
static bool slang_source = true;
static unsigned errors;
void RARCH_ERR(const char *fmt, ...) { (void)fmt; errors++; }
struct config_entry_list *config_get_entry(const config_file_t *conf, const char *key)
{
   (void)conf;
   assert(!strcmp(key, "hdr_mode"));
   entry.value = (char*)value;
   return value ? &entry : NULL;
}
bool video_shader_source_ident_is_slang(const char *ident)
{
   (void)ident;
   return slang_source;
}
PARSER
CHECK_OUTPUT
static unsigned metal_policy(struct video_shader *shader,
      enum glslang_format format, bool explicit_format)
{
   typedef bool BOOL;
   struct { struct { struct { bool explicit_format; enum glslang_format format; }
      semantics; } pass[1]; } _engine;
   unsigned i = 0;
   BOOL passEmitsHDR10 = false;
   BOOL passEmitsHDR16 = false;
   _engine.pass[0].semantics.format = format;
   _engine.pass[0].semantics.explicit_format = explicit_format;
#define YES true
#define NO false
METAL
   return passEmitsHDR10 ? 1 : (passEmitsHDR16 ? 2 : 0);
}
int main(void)
{
   static const struct { const char *value; int mode; bool accepted; } cases[] = {
      {"0", 0, true}, {"1", 1, true}, {"2", 2, true}, {"auto", -1, true},
      {NULL, 2, false}, {"", 2, false}, {"3", 2, false}, {"-1", 2, false},
      {"2.0", 2, false}, {"02", 2, false}, {"0x1", 2, false}, {"1x", 2, false}
   };
   struct video_shader *shader = (struct video_shader*)calloc(1, sizeof(*shader));
   config_file_t *conf = (config_file_t*)malloc(sizeof(config_file_t));
   unsigned i, output;
   assert(shader && conf);
   shader->passes = 1;
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      value = cases[i].value;
      shader->hdr_mode = 3;
      assert(video_shader_parse_hdr_mode(conf, shader) == cases[i].accepted);
      assert(video_shader_get_hdr_mode(shader) == cases[i].mode);
   }
   value = "2";
   shader->hdr_mode = 0;
   shader->num_parameters = 1;
   strcpy(shader->parameters[0].id, "hdr_mode");
   assert(!video_shader_parse_hdr_mode(conf, shader));
   assert(video_shader_get_hdr_mode(shader) == -1);
   shader->num_parameters = 0;
   slang_source = false;
   assert(!video_shader_parse_hdr_mode(conf, shader));
   assert(video_shader_get_hdr_mode(shader) == -1);
   slang_source = true;
   for (i = 0; i < 4; i++)
   {
      static const bool compatibility[4][3] = {
         {true, true, true}, {true, true, true},
         {false, true, false}, {false, false, true}
      };
      shader->hdr_mode = i;
      for (output = 0; output < 3; output++)
         assert(video_shader_hdr_output_compatible(shader, output) == compatibility[i][output]);
   }
   shader->hdr_mode = 3;
   assert(!video_shader_check_hdr_output(shader, 1) && errors == 1);
   assert(video_shader_check_hdr_output(shader, 2) && errors == 1);
   shader->passes = 2;
   assert(glslang_hdr_output_format(shader, 0, SLANG_FORMAT_R8G8B8A8_UNORM) == SLANG_FORMAT_R8G8B8A8_UNORM);
   assert(glslang_hdr_output_format(shader, 1, SLANG_FORMAT_UNKNOWN) == SLANG_FORMAT_R16G16B16A16_SFLOAT);
   shader->hdr_mode = 2;
   assert(glslang_hdr_output_format(shader, 1, SLANG_FORMAT_R8G8B8A8_UNORM) == SLANG_FORMAT_A2B10G10R10_UNORM_PACK32);
   shader->hdr_mode = 1;
   assert(glslang_hdr_output_format(shader, 1, SLANG_FORMAT_R16G16B16A16_SFLOAT) == SLANG_FORMAT_R16G16B16A16_SFLOAT);
   assert(glslang_hdr_output_format(shader, 1, SLANG_FORMAT_R8G8B8A8_SRGB) == SLANG_FORMAT_R8G8B8A8_UNORM);
   assert(glslang_hdr_output_encoding(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT) == SLANG_FORMAT_UNKNOWN);
   shader->hdr_mode = 0;
   assert(glslang_hdr_output_format(shader, 1, SLANG_FORMAT_UNKNOWN) == SLANG_FORMAT_UNKNOWN);
   assert(glslang_hdr_output_encoding(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT) == SLANG_FORMAT_R16G16B16A16_SFLOAT);
   assert(metal_policy(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT, false) == 0);
   assert(metal_policy(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT, true) == 2);
   assert(metal_policy(shader, SLANG_FORMAT_A2B10G10R10_UNORM_PACK32, true) == 1);
   shader->hdr_mode = 1;
   assert(metal_policy(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT, true) == 0);
   shader->hdr_mode = 2;
   assert(metal_policy(shader, SLANG_FORMAT_R16G16B16A16_SFLOAT, false) == 1);
   shader->hdr_mode = 3;
   assert(metal_policy(shader, SLANG_FORMAT_UNKNOWN, false) == 2);
   free(conf);
   free(shader);
   puts("PASS: HDR declarations, legacy parameters, precision, admission and Metal ownership policy");
   return 0;
}
'''.replace("PARSER", function("video_shader_parse_hdr_mode"))
fixture = fixture.replace("CHECK_OUTPUT", function("video_shader_check_hdr_output"))
fixture = fixture.replace("METAL", metal)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-hdr-ownership-") as directory:
    directory = Path(directory)
    src = directory / "ownership.c"
    exe = directory / ("ownership.exe" if os.name == "nt" else "ownership")
    src.write_text(fixture, encoding="utf-8")
    ps3 = (root / "libretro-common/include/defines/ps3_defines.h").read_text(encoding="utf-8")
    alias = re.search(r"^#define output_mode (\w+)$", ps3, re.M).group(1)
    for defines in ([], ["-Doutput_mode=" + alias]):
        subprocess.run(cc + flags + defines + ["-std=gnu89", "-Wall", "-Wextra",
                                    "-Werror=declaration-after-statement", "-O1",
                                    "-I" + str(root), "-I" + str(root / "libretro-common/include"),
                                    str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
