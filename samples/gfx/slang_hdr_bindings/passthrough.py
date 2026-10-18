#!/usr/bin/env python3
"""Check shipping HDR-ownership storage and declared uniform writes."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers_shader/slang_process.c").read_text(encoding="utf-8")
header = (root / "gfx/drivers_shader/slang_process.h").read_text(encoding="utf-8")
enum = re.search(r"enum slang_semantic\s*\{.*?\};", header, re.S).group()
constant = re.search(r"   static const uint32_t no_hdr_ownership =.*?;", source).group()
resolve = re.search(r"         if \(_semantic == SLANG_SEMANTIC_PASSTHROUGH.*?"
                    r"            uniform.data = \(void\*\)&no_hdr_ownership;", source, re.S).group()
resolve = "         uniform.data = map->uniforms[semantic];\n" + resolve

vulkan = (root / "gfx/drivers_shader/shader_vulkan.c").read_text(encoding="utf-8")
start = vulkan.index("static void slang_pass_build_semantic_uint(")
writer = vulkan[start:vulkan.index("\n}\n", start) + 3]
call = re.search(r"   slang_pass_build_semantic_uint\(pass, buffer, SLANG_SEMANTIC_PASSTHROUGH, pass->common->passthrough\);", vulkan).group()
gl = (root / "gfx/drivers_shader/shader_gl3.c").read_text(encoding="utf-8")
assert 'gl3_pass_reflect_parameter(pass, "Passthrough",' in gl
assert "gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_PASSTHROUGH, pass->common->passthrough);" in gl
extra = ""
for driver, code in (("gl3", gl), ("vulkan", vulkan)):
    function = re.search(r"void " + driver + r"_filter_chain_set_passthrough\(.*?\n}\n", code, re.S).group()
    extra += f"typedef struct {{ struct common common; }} {driver}_filter_chain_t;\n" + function
fixture = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
ENUM
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
struct map { void *uniforms[SLANG_NUM_BASE_SEMANTICS]; };
static void *resolve(const struct map *map, enum slang_semantic _semantic)
{
CONSTANT
   int semantic = (int)_semantic;
   struct { void *data, *param; } uniform;
RESOLVE
   return uniform.data;
}
typedef struct { unsigned uniform, push_constant, ubo_offset, push_constant_offset; } slang_semantic_meta;
struct common { unsigned passthrough; };
struct slang_pass {
   struct common *common;
   struct { slang_semantic_meta semantics[SLANG_NUM_BASE_SEMANTICS]; } reflection;
   struct { uint32_t *buffer; } push;
};
WRITER
EXTRA
static void write_passthrough(struct slang_pass *pass, uint8_t *buffer)
{
CALL
}
int main(void)
{
   struct map map;
   struct slang_pass pass;
   struct common common;
   slang_semantic_meta *refl;
   uint32_t ubo[2], push[2], known = 1;
   void *fallback;
   unsigned declared, storage;
   gl3_filter_chain_t gl_chain;
   vulkan_filter_chain_t vk_chain;
   gl3_filter_chain_set_passthrough(&gl_chain, 1);
   vulkan_filter_chain_set_passthrough(&vk_chain, 0);
   CHECK(SLANG_SEMANTIC_PASSTHROUGH == 33);
   CHECK(SLANG_NUM_BASE_SEMANTICS == 34);
   CHECK(gl_chain.common.passthrough == 1 && vk_chain.common.passthrough == 0);
   gl3_filter_chain_set_passthrough(&gl_chain, 0);
   vulkan_filter_chain_set_passthrough(&vk_chain, 1);
   CHECK(gl_chain.common.passthrough == 0 && vk_chain.common.passthrough == 1);
   memset(&map, 0, sizeof(map));
   memset(&pass, 0, sizeof(pass));
   fallback = resolve(&map, SLANG_SEMANTIC_PASSTHROUGH);
   CHECK(fallback && *(const uint32_t*)fallback == 0);
   CHECK(resolve(&map, SLANG_SEMANTIC_PASSTHROUGH) == fallback);
   CHECK(resolve(&map, SLANG_SEMANTIC_PAPER_WHITE_NITS) == NULL);
   map.uniforms[SLANG_SEMANTIC_PASSTHROUGH] = &known;
   CHECK(resolve(&map, SLANG_SEMANTIC_PASSTHROUGH) == &known);
   CHECK(known == 1 && *(const uint32_t*)fallback == 0);
   common.passthrough = 1;
   pass.common = &common;
   pass.push.buffer = (uint32_t*)push;
   refl = &pass.reflection.semantics[SLANG_SEMANTIC_PASSTHROUGH];
   refl->ubo_offset = refl->push_constant_offset = sizeof(uint32_t);
   for (declared = 0; declared <= 1; declared++)
      for (storage = 0; storage <= 1; storage++)
      {
         ubo[0] = ubo[1] = push[0] = push[1] = 7;
         refl->uniform = declared && !storage;
         refl->push_constant = declared && storage;
         write_passthrough(&pass, (uint8_t*)ubo);
         CHECK((storage ? push[1] : ubo[1]) == (declared ? 1 : 7));
         CHECK((storage ? ubo[1] : push[1]) == 7);
         CHECK(ubo[0] == 7 && push[0] == 7);
         common.passthrough = 0;
         write_passthrough(&pass, (uint8_t*)ubo);
         CHECK((storage ? push[1] : ubo[1]) == (declared ? 0 : 7));
         CHECK((storage ? ubo[1] : push[1]) == 7);
         common.passthrough = 1;
      }
   puts("PASS: Passthrough immutable no-ownership fallback and declared-only UBO/push writes");
   return 0;
}
'''
for key, value in (("ENUM", enum), ("CONSTANT", constant), ("RESOLVE", resolve),
                   ("WRITER", writer), ("EXTRA", extra), ("CALL", call)):
    fixture = fixture.replace(key, value)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-source-hdr-") as directory:
    directory = Path(directory)
    src = directory / "source.c"
    exe = directory / ("source.exe" if os.name == "nt" else "source")
    src.write_text(fixture, encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                                "-Werror=declaration-after-statement", "-fno-strict-aliasing",
                                str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

# Execute each driver's ownership decision, including explicit SDR overrides.
util = (root / "gfx/drivers_shader/glslang_util.h").read_text(encoding="utf-8")
formats = re.search(r"typedef enum glslang_format\s*\{.*?\}[^;]*;", util, re.S).group()
encoding = re.search(r"static INLINE enum glslang_format glslang_hdr_output_encoding\(.*?\n}\n", util, re.S).group()
policy = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define INLINE __inline__
FORMATS
struct video_shader { unsigned passes; int mode; };
static int video_shader_get_hdr_mode(const struct video_shader *shader) { return shader->mode; }
ENCODING
#define DRIVER_FRAME_IMPORT
#define DRIVER_ST_FLAG_SOURCE_FP16 1
#define DRIVER_ST_FLAG_SOURCE_HDR10 2
typedef struct { unsigned flags, output; uintptr_t movie_texture; } driver_video_t;
typedef struct { struct video_shader *shader_preset; struct { struct { unsigned explicit_format; enum glslang_format format; } semantics; } pass[2]; } chain_type;
static unsigned driver_hdr_output_mode(const driver_video_t *video) { return video->output; }
HELPER
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
   struct video_shader shader = { 2, -1 };
   driver_video_t video;
   chain_type chain;
   unsigned output;
   memset(&video, 0, sizeof(video));
   memset(&chain, 0, sizeof(chain));
   chain.shader_preset = &shader;
   chain.pass[0].semantics.explicit_format = 1;
   chain.pass[0].semantics.format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
   for (output = 0; output < 3; output++)
   {
      video.output = output;
      CHECK(driver_chain_passthrough(&video, &chain) == 0);
      video.flags = DRIVER_ST_FLAG_SOURCE_HDR10;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      video.flags = DRIVER_ST_FLAG_SOURCE_FP16;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      shader.mode = 0;
      CHECK(driver_chain_passthrough(&video, &chain) == 0);
      video.flags = 0;
      shader.mode = 1;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      shader.mode = 2;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      shader.mode = -1;
      chain.pass[1].semantics.explicit_format = 1;
      chain.pass[1].semantics.format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      chain.pass[1].semantics.format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      chain.pass[1].semantics.format = SLANG_FORMAT_R8G8B8A8_UNORM;
      video.flags = DRIVER_ST_FLAG_SOURCE_HDR10;
      CHECK(driver_chain_passthrough(&video, &chain) == 0);
      chain.pass[1].semantics.explicit_format = 0;
      CHECK(driver_chain_passthrough(&video, &chain) == (output != 0));
      video.movie_texture = 1;
      CHECK(driver_chain_passthrough(&video, &chain) == 0);
      video.movie_texture = 0;
      video.flags = 0;
   }
   shader.passes = 0;
   CHECK(driver_chain_passthrough(&video, &chain) == 0);
   puts("PASS: native ownership, output negotiation, intermediate precision and final overrides");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="retroarch-passthrough-policy-") as directory:
    directory = Path(directory)
    for driver, chain in (("d3d11", "d3d11_chain_t"), ("d3d12", "d3d12_filter_chain_t")):
        code = (root / f"gfx/drivers/{driver}.c").read_text(encoding="utf-8")
        helper = re.search(r"static unsigned " + driver + r"_chain_passthrough\(.*?\n}\n", code, re.S).group()
        fixture = policy.replace("FORMATS", formats).replace("ENCODING", encoding).replace("HELPER", helper)
        fixture = fixture.replace("chain_type", chain).replace("DRIVER", driver.upper()).replace("driver", driver)
        src = directory / "policy.c"
        exe = directory / ("policy.exe" if os.name == "nt" else "policy")
        src.write_text(fixture, encoding="utf-8")
        subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
