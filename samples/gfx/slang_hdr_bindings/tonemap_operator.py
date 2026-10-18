#!/usr/bin/env python3
"""Check shipping rational-operator storage and declared uniform writes."""
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
constant = re.search(r"   static const uint32_t rational_tonemap_operator =.*?;", source).group()
resolve = re.search(r"         if \(_semantic == SLANG_SEMANTIC_TONEMAP_OPERATOR.*?"
                    r"            uniform.data = \(void\*\)&rational_tonemap_operator;", source, re.S).group()
resolve = "         uniform.data = map->uniforms[semantic];\n" + resolve

vulkan = (root / "gfx/drivers_shader/shader_vulkan.c").read_text(encoding="utf-8")
start = vulkan.index("static void slang_pass_build_semantic_uint(")
writer = vulkan[start:vulkan.index("\n}\n", start) + 3]
call = re.search(r"   slang_pass_build_semantic_uint\(pass, buffer, SLANG_SEMANTIC_TONEMAP_OPERATOR, 0\);", vulkan).group()
gl = (root / "gfx/drivers_shader/shader_gl3.c").read_text(encoding="utf-8")
assert 'gl3_pass_reflect_parameter(pass, "TonemapOperator",' in gl
assert "gl3_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_TONEMAP_OPERATOR, 0);" in gl
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
struct slang_pass {
   struct { slang_semantic_meta semantics[SLANG_NUM_BASE_SEMANTICS]; } reflection;
   struct { uint32_t *buffer; } push;
};
WRITER
static void write_operator(struct slang_pass *pass, uint8_t *buffer)
{
CALL
}
int main(void)
{
   struct map map;
   struct slang_pass pass;
   slang_semantic_meta *refl;
   uint32_t ubo[2], push[2], known = 1;
   void *rational;
   unsigned declared, storage;
   memset(&map, 0, sizeof(map));
   memset(&pass, 0, sizeof(pass));
   rational = resolve(&map, SLANG_SEMANTIC_TONEMAP_OPERATOR);
   CHECK(rational && *(const uint32_t*)rational == 0);
   CHECK(resolve(&map, SLANG_SEMANTIC_TONEMAP_OPERATOR) == rational);
   CHECK(resolve(&map, SLANG_SEMANTIC_PAPER_WHITE_NITS) == NULL);
   map.uniforms[SLANG_SEMANTIC_TONEMAP_OPERATOR] = &known;
   CHECK(resolve(&map, SLANG_SEMANTIC_TONEMAP_OPERATOR) == &known);
   CHECK(known == 1 && *(const uint32_t*)rational == 0);
   pass.push.buffer = (uint32_t*)push;
   refl = &pass.reflection.semantics[SLANG_SEMANTIC_TONEMAP_OPERATOR];
   refl->ubo_offset = refl->push_constant_offset = sizeof(uint32_t);
   for (declared = 0; declared <= 1; declared++)
      for (storage = 0; storage <= 1; storage++)
      {
         ubo[0] = ubo[1] = push[0] = push[1] = 7;
         refl->uniform = declared && !storage;
         refl->push_constant = declared && storage;
         write_operator(&pass, (uint8_t*)ubo);
         CHECK((storage ? push[1] : ubo[1]) == (declared ? 0 : 7));
         CHECK((storage ? ubo[1] : push[1]) == 7);
         CHECK(ubo[0] == 7 && push[0] == 7);
      }
   puts("PASS: TonemapOperator immutable rational fallback and declared-only UBO/push writes");
   return 0;
}
'''
for key, value in (("ENUM", enum), ("CONSTANT", constant), ("RESOLVE", resolve),
                   ("WRITER", writer), ("CALL", call)):
    fixture = fixture.replace(key, value)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-tonemap-operator-") as directory:
    directory = Path(directory)
    src = directory / "operator.c"
    exe = directory / ("operator.exe" if os.name == "nt" else "operator")
    src.write_text(fixture, encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                                "-Werror=declaration-after-statement", "-fno-strict-aliasing",
                                str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
