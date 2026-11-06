/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_ext_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* rslc RSLC_EXTENDED test: the sampler types and builtins only an
 * emulator's own shaders use.  Each stage goes to every destination;
 * the SPIR-V is written under the directory given, for spirv-val. */

#include <stdio.h>
#include <string.h>

#include <gfx/shaders/rslc.h>

static int fails;

#define CHECK(cond) do { \
   if (!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++; \
   } \
} while (0)

static const char fs_samplers[] =
   "#version 450\n"
   "layout(set = 0, binding = 0) uniform sampler2DArray samp;\n"
   "layout(set = 0, binding = 1) uniform usamplerBuffer texels;\n"
   "layout(set = 0, binding = 2) uniform sampler2DMSArray ms;\n"
   "layout(set = 0, binding = 3) uniform isampler2DArray isamp;\n"
   "layout(location = 0) in vec3 uv;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   ivec3 sz  = textureSize(samp, 0);\n"
   "   int   n   = textureSize(texels);\n"
   "   ivec3 msz = textureSize(ms);\n"
   "   uvec4 t   = texelFetch(texels, n - 1);\n"
   "   ivec4 i   = texelFetch(isamp, ivec3(0), 0);\n"
   "   vec4  a   = texture(samp, uv) + textureLod(samp, uv, 1.0) + texture(samp, uv, 0.5);\n"
   "   vec4  b   = texelFetch(samp, ivec3(sz.xy - 1, 0), 0)\n"
   "             + texelFetch(ms, ivec3(msz.xy - 1, 0), 3);\n"
   "   col = a + b + vec4(t) + vec4(i) + vec4(float(n));\n"
   "}\n";

static const char fs_sample_id[] =
   "#version 450\n"
   "layout(set = 0, binding = 0) uniform sampler2DMSArray ms;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   col = texelFetch(ms, ivec3(ivec2(gl_FragCoord.xy), 0), gl_SampleID);\n"
   "}\n";

/* Not a sample-rate stage: gl_SampleID is declared but never read. */
static const char fs_plain[] =
   "#version 450\n"
   "layout(set = 0, binding = 0) uniform sampler2DArray samp;\n"
   "layout(location = 0) in vec3 uv;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   col = texture(samp, uv);\n"
   "}\n";

/* More than one uniform block, at their own sets and bindings. */
static const char fs_blocks[] =
   "#version 450\n"
   "layout(std140, set = 0, binding = 0) uniform PSBlock { vec4 color; ivec4 konst[4]; };\n"
   "layout(std140, set = 0, binding = 1) uniform VSBlock { vec4 light; mat4 proj; } vs;\n"
   "layout(std140, set = 0, binding = 2) uniform GSBlock { vec4 stereo; };\n"
   "layout(push_constant) uniform Push { vec4 scale; } push;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   col = color * vs.light + vec4(konst[1]) + stereo * push.scale + vs.proj[0];\n"
   "}\n";

/* A sampler array, indexed by constants and by a uniform. */
static const char fs_sampler_array[] =
   "#version 450\n"
   "layout(set = 1, binding = 0) uniform sampler2DArray samp[8];\n"
   "layout(std140, set = 0, binding = 0) uniform PSBlock { ivec4 sel; };\n"
   "layout(location = 0) in vec3 uv;\n"
   "layout(location = 0) out vec4 col;\n"
   "vec4 fetch(int i) { return texture(samp[i], uv); }\n"
   "void main()\n"
   "{\n"
   "   col = texture(samp[2], uv) + fetch(sel.x) + texelFetch(samp[sel.y], ivec3(0), 0);\n"
   "   col.x += float(textureSize(samp[1], 0).x);\n"
   "}\n";

/* Dual-source blending and early depth. */
static const char fs_dual_source[] =
   "#version 450\n"
   "layout(early_fragment_tests) in;\n"
   "layout(location = 0) in vec4 c;\n"
   "layout(location = 0, index = 0) out vec4 ocol0;\n"
   "layout(location = 0, index = 1) out vec4 ocol1;\n"
   "void main()\n"
   "{\n"
   "   ocol0 = c;\n"
   "   ocol1 = c.aaaa;\n"
   "}\n";

static bool compile(const char *src, enum rslc_dst dst, unsigned version,
      struct rslc_opts *opts, struct rslc_result *res, char *err, size_t err_cap)
{
   memset(opts, 0, sizeof(*opts));
   opts->dst     = dst;
   opts->version = version;
   opts->err     = err;
   opts->err_cap = err_cap;
   err[0]        = '\0';
   return rslc_compile(src, strlen(src), RSLC_STAGE_FRAGMENT, opts, res);
}

/* Whether the module declares the capability. */
static bool has_capability(const uint32_t *w, size_t n, uint32_t cap)
{
   size_t i = 5;
   while (i < n)
   {
      uint32_t len = w[i] >> 16;
      if (!len)
         return false;
      if ((w[i] & 0xffffu) == 17 /* OpCapability */ && w[i + 1] == cap)
         return true;
      i += len;
   }
   return false;
}

static void write_spirv(const char *dir, const char *name, const struct rslc_result *res)
{
   char  path[1024];
   FILE *f;
   if (!dir || strlen(dir) + strlen(name) + 6 > sizeof(path))
      return;
   sprintf(path, "%s/%s.spv", dir, name);
   f = fopen(path, "wb");
   CHECK(f != NULL);
   if (!f)
      return;
   CHECK(fwrite(res->spirv, 4, res->spirv_len, f) == res->spirv_len);
   fclose(f);
}

/* SPIR-V, GLSL and HLSL; MSL has no form of the new samplers or of a
 * third block yet and says so (msl_ok when the stage has neither). */
static void test_stage(const char *dir, const char *name, const char *src,
      const char *glsl_needle, const char *hlsl_needle, uint32_t want_cap, uint32_t no_cap,
      bool msl_ok)
{
   char err[256];
   struct rslc_opts   o;
   struct rslc_result r;

   CHECK(compile(src, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
   if (err[0])
      fprintf(stderr, "%s: %s\n", name, err);
   if (r.blob)
   {
      if (want_cap)
         CHECK(has_capability(r.spirv, r.spirv_len, want_cap));
      if (no_cap)
         CHECK(!has_capability(r.spirv, r.spirv_len, no_cap));
      write_spirv(dir, name, &r);
      rslc_result_free(&r, &o);
   }

   memset(&r, 0, sizeof(r));
   CHECK(compile(src, RSLC_DST_GLSL, 450, &o, &r, err, sizeof(err)));
   if (r.blob)
   {
      CHECK(strstr(r.text, glsl_needle) != NULL);
      rslc_result_free(&r, &o);
   }

   memset(&r, 0, sizeof(r));
   CHECK(compile(src, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
   if (r.blob)
   {
      CHECK(strstr(r.text, hlsl_needle) != NULL);
      rslc_result_free(&r, &o);
   }

   memset(&r, 0, sizeof(r));
   if (msl_ok)
   {
      CHECK(compile(src, RSLC_DST_MSL, 20000, &o, &r, err, sizeof(err)));
      if (r.blob)
         rslc_result_free(&r, &o);
   }
   else
   {
      CHECK(!compile(src, RSLC_DST_MSL, 20000, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "msl") != NULL);
   }
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;

   /* SampledBuffer 46, ImageQuery 50, SampleRateShading 35 */
   test_stage(dir, "samplers", fs_samplers, "usamplerBuffer", "Texture2DMSArray<float4>", 46, 35, false);
   test_stage(dir, "sample_id", fs_sample_id, "gl_SampleID", "SV_SampleIndex", 35, 0, false);
   test_stage(dir, "plain", fs_plain, "sampler2DArray", "Texture2DArray<float4>", 0, 35, false);
   test_stage(dir, "blocks", fs_blocks, "uniform GSBlock", "cbuffer GSBlock : register(b2)", 0, 0, false);
   test_stage(dir, "sampler_array", fs_sampler_array, "sampler2DArray samp[8]",
         "samp[i].Sample(_samp_sampler[i]", 0, 0, false);
   test_stage(dir, "dual_source", fs_dual_source, "layout(early_fragment_tests) in;",
         "SV_Target1", 0, 0, true);

   if (fails)
   {
      fprintf(stderr, "rslc_ext_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_ext_test: ok\n");
   return 0;
}
