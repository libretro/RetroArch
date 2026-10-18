#!/usr/bin/env python3
"""Check shipping Metal HDR bindings, frame settings and view rebasing."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
header = (root / "gfx/drivers_shader/slang_process.h").read_text(encoding="utf-8")
semantic_enum = re.search(r"enum slang_semantic\s*\{.*?\};", header, re.S).group()
start = source.index("               mvp,                              /* MVP */")
bindings = source[start:source.index("\n            }\n         };", start)]
start = source.index("      uint32_t frame_count;", source.index("} texture_t;"))
fields = source[start:source.index("      pass_semantics_t semantics;", start)]
state = re.search(r"   e->pass\[i\]\.hdr_mode =.*?"
                  r"   e->pass\[i\]\.expand_gamut =.*?;", source, re.S)
latch = re.search(r"      _frameView\.hdrPaperWhiteNits =.*?"
                  r"      _frameView\.hdrExpandGamut =.*?;", source, re.S)
rebase = re.search(r"#define METAL_REBASE\(ptr\).*?\n\n", source, re.S).group()
assert "u[k].data = METAL_REBASE(u[k].data);" in source
publish = re.search(r"   output_mode = \(!wantEnable.*?;\n"
                    r"   _hdrActiveOutputMode =.*?;", source, re.S)
assert "const HDRUniforms *hdr = [_context currentHDRUniforms];" in source
assert "_hdrUniforms.Scanlines = scanlines ? 1.0f : 0.0f;" in source
assert "_hdrUniforms.SubpixelLayout = layout;" in source
assert state and latch and publish, "Metal HDR state assignments are missing"
start = source.index("- (unsigned)hdrOutputMode\n{")
getter = source[source.index("{", start) + 1:source.index("\n}", start)]

start = source.index("- (void)setHDRShaderEmitsHDR10:(bool)emitsHDR10 emitsHDR16:(bool)emitsHDR16\n{")
policy = source[source.index("{", start) + 1:source.index("\n}", start)]

aliases = "\n".join(re.findall(r"         semantics_map\.uniforms\[SLANG_SEMANTIC_(?:PAPER_WHITE_ALIAS|PEAK_NITS|MAX_NITS_ALIAS|ENABLE_HDR|PASSTHROUGH)\] =.*?;", source, re.S))

fixture = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
#define METAL_HDR_OUTPUT_OFF 0
#define METAL_HDR_OUTPUT_HDR10 1
#define METAL_HDR_OUTPUT_SCRGB 2
typedef float float_t;
ENUM
typedef struct { struct { float size_data[4]; } rt; FIELDS } pass_t;
typedef struct { struct { float output_size[4]; } frame; pass_t pass[2]; } engine_t;
struct values { float hdrPaperWhiteNits; unsigned hdrExpandGamut; float hdrPeakNits; };
typedef struct { float Scanlines; unsigned SubpixelLayout; float InverseTonemap; float HDR10; unsigned HDRMode; } HDRUniforms;
static HDRUniforms conversion_policy(unsigned _hdrOutputMode, bool emitsHDR10, bool emitsHDR16)
{
   HDRUniforms _hdrUniforms = { 0 };
   bool _hdrShaderEmitsHDR10 = false, _hdrShaderEmitsHDR16 = false;
   (void)_hdrOutputMode;
   (void)_hdrShaderEmitsHDR10;
   (void)_hdrShaderEmitsHDR16;
POLICY
   return _hdrUniforms;
}
static unsigned output_mode(unsigned _hdrActiveOutputMode)
{
   (void)_hdrActiveOutputMode;
GETTER
}
static unsigned publish_mode(unsigned mode, int wantEnable, const void *newCS)
{
   unsigned output_mode = 0, _hdrActiveOutputMode = 0;
   (void)mode;
   (void)wantEnable;
   (void)newCS;
   (void)output_mode;
PUBLISH_CODE
   return _hdrActiveOutputMode;
}
static struct values latch_frame(float paper_white, unsigned gamut, float peak)
{
   struct values _frameView = { 0 };
   struct { float hdr_paper_white_nits; unsigned hdr_expand_gamut; float hdr_max_nits; } info;
   info.hdr_paper_white_nits = paper_white;
   info.hdr_expand_gamut = gamut;
   info.hdr_max_nits = peak;
   (void)info;
#define video_info (&info)
LATCH
#undef video_info
   return _frameView;
}
static bool _shaderEmitsHDR10, _shaderEmitsHDR16;
static void update(engine_t *e, unsigned i, unsigned requested, struct values frame, const HDRUniforms *hdr)
{
   struct { unsigned hdrOutputMode; } _context;
   float _hdrPaperWhiteNits = frame.hdrPaperWhiteNits;
   float _hdrPeakNits = frame.hdrPeakNits;
   unsigned _hdrExpandGamut = frame.hdrExpandGamut;
   _context.hdrOutputMode = output_mode(requested);
   (void)hdr;
   (void)e;
   (void)i;
   (void)_context;
   (void)_hdrPaperWhiteNits;
   (void)_hdrExpandGamut;
STATE
}
int main(void)
{
   engine_t _engine;
   struct view_chain { engine_t engine; } view;
   struct view_chain *c = &view;
   void *copy[7];
   HDRUniforms hdr = { 1.0f, 4, 1.0f, 1.0f, 1 };
   HDRUniforms other_hdr = { 0.0f, 2, 1.0f, 0.0f, 2 };
   float mvp[16] = { 0 };
   const char *lo = (const char *)&_engine;
   const char *hi = lo + sizeof(_engine);
   unsigned i, mode, encoding;
   struct values frame = latch_frame(203.0f, 2, 600.0f);
   CHECK(publish_mode(2, 1, &view) == 2);
   CHECK(publish_mode(1, 1, &view) == 1);
   CHECK(publish_mode(2, 1, NULL) == 0);
   CHECK(publish_mode(0, 0, NULL) == 0);
   CHECK(publish_mode(3, 1, &view) == 0);
   memset(&_engine, 0, sizeof(_engine));
   memset(&view, 0, sizeof(view));
   for (i = 0; i < 2; i++)
   {
      struct { void *uniforms[SLANG_NUM_BASE_SEMANTICS]; } semantics_map = {{
BINDINGS
      }};
ALIASES
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_ALIAS] == &_engine.pass[i].paper_white_nits);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_PEAK_NITS] == &_engine.pass[i].peak_nits);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_ENABLE_HDR] == &_engine.pass[i].enable_hdr);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_MAX_NITS_ALIAS] == &_engine.pass[i].peak_nits);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_PASSTHROUGH] == &_engine.pass[i].passthrough);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_HDR] == &_engine.pass[i].hdr_mode);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_NITS] == &_engine.pass[i].paper_white_nits);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_EXPAND_GAMUT] == &_engine.pass[i].expand_gamut);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_SWAP_COUNT] == &_engine.pass[i].swap_count);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_SCANLINES] == &_engine.pass[i].scanlines);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_SUBPIXEL_LAYOUT] == &_engine.pass[i].subpixel_layout);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_INVERSE_TONEMAP] == &_engine.pass[i].inverse_tonemap);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_HDR10] == &_engine.pass[i].hdr10);
      CHECK(semantics_map.uniforms[SLANG_SEMANTIC_GYROSCOPE] == NULL);
      _shaderEmitsHDR10 = _shaderEmitsHDR16 = false;
      for (mode = 0; mode <= 2; mode++)
      {
         unsigned actual = METAL_HDR_AVAILABLE ? mode : 0;
         CHECK(output_mode(mode) == actual);
         update(&_engine, i, mode, frame, &hdr);
         CHECK(*(uint32_t*)semantics_map.uniforms[SLANG_SEMANTIC_HDR] == actual);
         CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_PEAK_NITS] == (actual ? 600.0f : 0.0f));
         CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_ENABLE_HDR] == (actual ? 1.0f : 0.0f));
         CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_NITS] == (actual ? 203.0f : 0.0f));
         CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_SCANLINES] == (actual ? 1.0f : 0.0f));
         CHECK(*(unsigned*)semantics_map.uniforms[SLANG_SEMANTIC_SUBPIXEL_LAYOUT] == (actual ? 4u : 0u));
         CHECK(*(unsigned*)semantics_map.uniforms[SLANG_SEMANTIC_EXPAND_GAMUT] == (actual ? 2u : 0u));
      }
      _shaderEmitsHDR10 = _shaderEmitsHDR16 = false;
      for (mode = 0; mode <= 2; mode++)
      {
         for (encoding = 0; encoding < 4; encoding++)
         {
            HDRUniforms policy = conversion_policy(mode, (encoding & 1) != 0,
                  (encoding & 2) != 0);
            float inverse = METAL_HDR_AVAILABLE && mode && !encoding ? 1.0f : 0.0f;
            float pq = METAL_HDR_AVAILABLE && mode == 1 && !encoding ? 1.0f : 0.0f;
            _shaderEmitsHDR10 = (encoding & 1) != 0;
            _shaderEmitsHDR16 = (encoding & 2) != 0;
            update(&_engine, i, mode, frame, &policy);
            CHECK(_engine.pass[i].passthrough == (METAL_HDR_AVAILABLE && mode && encoding ? 1u : 0u));
            CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_INVERSE_TONEMAP] == inverse);
            CHECK(*(float*)semantics_map.uniforms[SLANG_SEMANTIC_HDR10] == pq);
            if (METAL_HDR_AVAILABLE && mode == 2 && encoding == 1)
            {
               CHECK(policy.HDRMode == 3);
               CHECK(_engine.pass[i].hdr_mode == 2);
            }
         }
      }
      update(&_engine, i, publish_mode(2, 1, NULL), frame, &hdr);
      CHECK(_engine.pass[i].hdr_mode == 0 && _engine.pass[i].paper_white_nits == 0.0f);
      CHECK(_engine.pass[i].expand_gamut == 0);
      CHECK(_engine.pass[i].inverse_tonemap == 0.0f && _engine.pass[i].hdr10 == 0.0f);
      CHECK(_engine.pass[i].scanlines == 0.0f && _engine.pass[i].subpixel_layout == 0);
      update(&_engine, i, 2, frame, NULL);
      CHECK(_engine.pass[i].inverse_tonemap == 0.0f && _engine.pass[i].hdr10 == 0.0f);
      CHECK(_engine.pass[i].scanlines == 0.0f && _engine.pass[i].subpixel_layout == 0);
      {
REBASE_CODE
         CHECK(METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_PASSTHROUGH]) == &view.engine.pass[i].passthrough);
         copy[0] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_HDR]);
         copy[1] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_NITS]);
         CHECK(METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_PEAK_NITS]) == &view.engine.pass[i].peak_nits);
         CHECK(METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_ENABLE_HDR]) == &view.engine.pass[i].enable_hdr);
         CHECK(METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_MAX_NITS_ALIAS]) == &view.engine.pass[i].peak_nits);
         CHECK(METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_ALIAS]) == copy[1]);
         copy[2] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_EXPAND_GAMUT]);
         copy[3] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_SCANLINES]);
         copy[4] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_SUBPIXEL_LAYOUT]);
         copy[5] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_INVERSE_TONEMAP]);
         copy[6] = METAL_REBASE(semantics_map.uniforms[SLANG_SEMANTIC_HDR10]);
#undef METAL_REBASE
      }
      update(&_engine, i, 1, frame, &hdr);
      update(&view.engine, i, 2, latch_frame(100.0f, 1, 1000.0f), &other_hdr);
      CHECK(copy[5] == &view.engine.pass[i].inverse_tonemap);
      CHECK(copy[6] == &view.engine.pass[i].hdr10);
      CHECK(*(float*)copy[5] == (output_mode(2) ? 1.0f : 0.0f));
      CHECK(*(float*)copy[6] == 0.0f);
      CHECK(_engine.pass[i].hdr10 == (output_mode(1) ? 1.0f : 0.0f));
      CHECK(copy[3] == &view.engine.pass[i].scanlines);
      CHECK(copy[4] == &view.engine.pass[i].subpixel_layout);
      CHECK(*(float*)copy[3] == 0.0f);
      CHECK(*(unsigned*)copy[4] == (output_mode(2) ? 2u : 0u));
      CHECK(_engine.pass[i].scanlines == (output_mode(1) ? 1.0f : 0.0f));
      CHECK(_engine.pass[i].subpixel_layout == (output_mode(1) ? 4u : 0u));
      CHECK(view.engine.pass[i].peak_nits == (output_mode(2) ? 1000.0f : 0.0f));
      CHECK(_engine.pass[i].peak_nits == (output_mode(1) ? 600.0f : 0.0f));
      CHECK(copy[0] == &view.engine.pass[i].hdr_mode);
      CHECK(copy[1] == &view.engine.pass[i].paper_white_nits);
      CHECK(copy[2] == &view.engine.pass[i].expand_gamut);
      CHECK(*(uint32_t*)copy[0] == output_mode(2));
      CHECK(*(float*)copy[1] == (output_mode(2) ? 100.0f : 0.0f));
      CHECK(*(unsigned*)copy[2] == (output_mode(2) ? 1u : 0u));
      CHECK(_engine.pass[i].hdr_mode == output_mode(1));
      CHECK(_engine.pass[i].paper_white_nits == (output_mode(1) ? 203.0f : 0.0f));
   }
   puts("PASS: Metal HDR bindings, frame values, SDR reset and isolated views");
   return 0;
}
'''
code = fixture
for key, value in (("ALIASES", aliases), ("ENUM", semantic_enum), ("FIELDS", fields), ("POLICY", policy),
                   ("GETTER", getter), ("LATCH", latch.group()),
                   ("STATE", state.group()), ("BINDINGS", bindings),
                   ("PUBLISH_CODE", publish.group()),
                   ("REBASE_CODE", rebase)):
    code = code.replace(key, value)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-metal-hdr-") as directory:
    directory = Path(directory)
    src = directory / "metal.c"
    exe = directory / ("metal.exe" if os.name == "nt" else "metal")
    src.write_text(code, encoding="utf-8")
    for hdr in (0, 1):
        subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra",
                       "-Werror", "-Werror=declaration-after-statement",
                       f"-DMETAL_HDR_AVAILABLE={hdr}", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
