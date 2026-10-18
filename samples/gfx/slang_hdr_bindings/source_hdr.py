#!/usr/bin/env python3
"""Check shipping PQ-source storage and declared uniform writes."""
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
constant = re.search(r"   static const uint32_t non_pq_source_hdr =.*?;", source).group()
resolve = re.search(r"         if \(_semantic == SLANG_SEMANTIC_SOURCE_HDR.*?"
                    r"            uniform.data = \(void\*\)&non_pq_source_hdr;", source, re.S).group()
resolve = "         uniform.data = map->uniforms[semantic];\n" + resolve

vulkan = (root / "gfx/drivers_shader/shader_vulkan.c").read_text(encoding="utf-8")
start = vulkan.index("static void slang_pass_build_semantic_uint(")
writer = vulkan[start:vulkan.index("\n}\n", start) + 3]
call = re.search(r"   slang_pass_build_semantic_uint\(pass, buffer, SLANG_SEMANTIC_SOURCE_HDR, pass->common->source_hdr\);", vulkan).group()
gl = (root / "gfx/drivers_shader/shader_gl3.c").read_text(encoding="utf-8")
assert 'gl3_pass_reflect_parameter(pass, "SourceHDR",' in gl
assert "gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_SOURCE_HDR, pass->common->source_hdr);" in gl
extra = "#define D3D11_FRAME_IMPORT\n#define D3D12_FRAME_IMPORT\n"
for driver in ("d3d11", "d3d12"):
    code = (root / f"gfx/drivers/{driver}.c").read_text(encoding="utf-8")
    statement = re.search(r"      pass->source_hdr =.*?#endif", code, re.S).group()
    flag = driver.upper() + "_ST_FLAG_SOURCE_HDR10"
    value = re.search(flag + r"\s*=\s*([^,]+),", code).group(1)
    extra += f"#define {flag} {value}\n"
    extra += f"static unsigned {driver}_source(unsigned flags, uintptr_t movie)\n{{\n"
    extra += "   struct { unsigned source_hdr; } storage, *pass = &storage;\n"
    extra += f"   struct {{ unsigned flags; uintptr_t movie_texture; }} video, *{driver} = &video;\n"
    extra += "   video.flags = flags; video.movie_texture = movie;\n" + statement + "\n   return pass->source_hdr;\n}\n"
for driver, code in (("gl3", gl), ("vulkan", vulkan)):
    function = re.search(r"void " + driver + r"_filter_chain_set_source_hdr\(.*?\n}\n", code, re.S).group()
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
struct common { unsigned source_hdr; };
struct slang_pass {
   struct common *common;
   struct { slang_semantic_meta semantics[SLANG_NUM_BASE_SEMANTICS]; } reflection;
   struct { uint32_t *buffer; } push;
};
WRITER
EXTRA
static void write_source(struct slang_pass *pass, uint8_t *buffer)
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
   CHECK(d3d11_source(0, 0) == 0);
   CHECK(d3d11_source(D3D11_ST_FLAG_SOURCE_HDR10, 0) == 1);
   CHECK(d3d11_source(D3D11_ST_FLAG_SOURCE_HDR10, 1) == 0);
   CHECK(d3d12_source(0, 0) == 0);
   CHECK(d3d12_source(D3D12_ST_FLAG_SOURCE_HDR10, 0) == 1);
   CHECK(d3d12_source(D3D12_ST_FLAG_SOURCE_HDR10, 1) == 0);
   gl3_filter_chain_set_source_hdr(&gl_chain, 1);
   vulkan_filter_chain_set_source_hdr(&vk_chain, 0);
   CHECK(gl_chain.common.source_hdr == 1 && vk_chain.common.source_hdr == 0);
   gl3_filter_chain_set_source_hdr(&gl_chain, 0);
   vulkan_filter_chain_set_source_hdr(&vk_chain, 1);
   CHECK(gl_chain.common.source_hdr == 0 && vk_chain.common.source_hdr == 1);
   memset(&map, 0, sizeof(map));
   memset(&pass, 0, sizeof(pass));
   fallback = resolve(&map, SLANG_SEMANTIC_SOURCE_HDR);
   CHECK(fallback && *(const uint32_t*)fallback == 0);
   CHECK(resolve(&map, SLANG_SEMANTIC_SOURCE_HDR) == fallback);
   CHECK(resolve(&map, SLANG_SEMANTIC_PAPER_WHITE_NITS) == NULL);
   map.uniforms[SLANG_SEMANTIC_SOURCE_HDR] = &known;
   CHECK(resolve(&map, SLANG_SEMANTIC_SOURCE_HDR) == &known);
   CHECK(known == 1 && *(const uint32_t*)fallback == 0);
   common.source_hdr = 1;
   pass.common = &common;
   pass.push.buffer = (uint32_t*)push;
   refl = &pass.reflection.semantics[SLANG_SEMANTIC_SOURCE_HDR];
   refl->ubo_offset = refl->push_constant_offset = sizeof(uint32_t);
   for (declared = 0; declared <= 1; declared++)
      for (storage = 0; storage <= 1; storage++)
      {
         ubo[0] = ubo[1] = push[0] = push[1] = 7;
         refl->uniform = declared && !storage;
         refl->push_constant = declared && storage;
         write_source(&pass, (uint8_t*)ubo);
         CHECK((storage ? push[1] : ubo[1]) == (declared ? 1 : 7));
         CHECK((storage ? ubo[1] : push[1]) == 7);
         CHECK(ubo[0] == 7 && push[0] == 7);
         common.source_hdr = 0;
         write_source(&pass, (uint8_t*)ubo);
         CHECK((storage ? push[1] : ubo[1]) == (declared ? 0 : 7));
         CHECK((storage ? ubo[1] : push[1]) == 7);
         common.source_hdr = 1;
      }
   puts("PASS: SourceHDR immutable non-PQ fallback and declared-only UBO/push writes");
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
