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

/* Clip distances, sized by the elements written; a cull distance unused. */
static const char vs_clip[] =
   "#version 450\n"
   "layout(location = 0) in vec4 pos;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   gl_Position = pos;\n"
   "   gl_ClipDistance[0] = pos.z;\n"
   "   gl_ClipDistance[1] = -pos.z;\n"
   "   col = pos;\n"
   "}\n";

/* An element past the guaranteed eight. */
static const char vs_clip_bad[] =
   "#version 450\n"
   "void main()\n"
   "{\n"
   "   gl_Position = vec4(0.0);\n"
   "   gl_ClipDistance[8] = 1.0;\n"
   "}\n";

/* An in / out block from the vertex stage to the fragment stage. */
static const char vs_ioblock[] =
   "#version 450\n"
   "layout(location = 0) in vec4 pos;\n"
   "layout(location = 0) out VertexData {\n"
   "   centroid vec4 pos;\n"
   "   centroid vec4 colors_0;\n"
   "   vec3 tex0;\n"
   "   float clipDist0;\n"
   "} vs;\n"
   "void main()\n"
   "{\n"
   "   vs.pos = pos;\n"
   "   vs.colors_0 = pos.zyxw;\n"
   "   vs.tex0 = pos.xyz;\n"
   "   vs.clipDist0 = pos.w;\n"
   "   gl_Position = pos;\n"
   "}\n";

static const char fs_ioblock[] =
   "#version 450\n"
   "layout(location = 0) in VertexData {\n"
   "   centroid vec4 pos;\n"
   "   centroid vec4 colors_0;\n"
   "   vec3 tex0;\n"
   "   float clipDist0;\n"
   "} ps;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   col = ps.colors_0 + vec4(ps.tex0, ps.clipDist0);\n"
   "}\n";

/* A wide-line geometry stage, as parallel-gamecubewii generates one:
 * a line in, a quad out, through in / out blocks. */
static const char gs_wide_line[] =
   "#version 450\n"
   "layout(lines, invocations = 1) in;\n"
   "layout(triangle_strip, max_vertices = 4) out;\n"
   "layout(std140, set = 0, binding = 3) uniform GSBlock { vec4 lineptparams; vec4 texOffset; ivec4 texoffsetflags; uint vpw; };\n"
   "struct VS_OUTPUT { vec4 pos; vec4 colors_0; vec3 tex0; float clipDist0; float clipDist1; };\n"
   "layout(location = 0) in VertexData {\n"
   "   centroid vec4 pos;\n"
   "   centroid vec4 colors_0;\n"
   "   centroid vec3 tex0;\n"
   "   centroid float clipDist0;\n"
   "   centroid float clipDist1;\n"
   "} vs[2];\n"
   "layout(location = 0) out VertexData {\n"
   "   centroid vec4 pos;\n"
   "   centroid vec4 colors_0;\n"
   "   centroid vec3 tex0;\n"
   "   centroid float clipDist0;\n"
   "   centroid float clipDist1;\n"
   "} ps;\n"
   "void emit(VS_OUTPUT v)\n"
   "{\n"
   "   gl_Position = vec4(v.pos.x, -v.pos.y, v.pos.z, v.pos.w);\n"
   "   gl_ClipDistance[0] = v.clipDist0;\n"
   "   gl_ClipDistance[1] = v.clipDist1;\n"
   "   ps.pos = v.pos; ps.colors_0 = v.colors_0; ps.tex0 = v.tex0;\n"
   "   ps.clipDist0 = v.clipDist0; ps.clipDist1 = v.clipDist1;\n"
   "   gl_Layer = gl_InvocationID;\n"
   "   EmitVertex();\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   VS_OUTPUT start, end;\n"
   "   int i;\n"
   "   start.pos = vs[0].pos; start.colors_0 = vs[0].colors_0; start.tex0 = vs[0].tex0;\n"
   "   start.clipDist0 = vs[0].clipDist0; start.clipDist1 = vs[0].clipDist1;\n"
   "   end.pos = vs[1].pos; end.colors_0 = vs[1].colors_0; end.tex0 = vs[1].tex0;\n"
   "   end.clipDist0 = vs[1].clipDist0; end.clipDist1 = vs[1].clipDist1;\n"
   "   vec2 offset = vec2(lineptparams.w / lineptparams.x, 0.0);\n"
   "   for (i = 0; i < 2; ++i)\n"
   "   {\n"
   "      VS_OUTPUT f = i == 0 ? start : end;\n"
   "      VS_OUTPUT l = f, r = f;\n"
   "      l.pos.xy -= offset * l.pos.w;\n"
   "      r.pos.xy += offset * r.pos.w;\n"
   "      if (texoffsetflags.z != 0)\n"
   "         r.tex0.x += 1.0 / float(texoffsetflags.z);\n"
   "      emit(l);\n"
   "      emit(r);\n"
   "   }\n"
   "   EndPrimitive();\n"
   "}\n";

/* The screen-quad vertex stage parallel-gamecubewii shares between its
 * utility passes: an unnamed out block, its member named on its own. */
static const char vs_unnamed[] =
   "#version 450\n"
   "layout(location = 0) out VertexData {\n"
   "   vec3 v_tex0;\n"
   "};\n"
   "void main()\n"
   "{\n"
   "   v_tex0 = vec3(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2), 0.0);\n"
   "   gl_Position = vec4(v_tex0.xy * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\n"
   "}\n";

/* Its layered pass-through geometry stage: the input instance sized by
 * the input primitive, the position and clip distances read from gl_in. */
static const char gs_passthrough[] =
   "#version 450\n"
   "layout(triangles) in;\n"
   "layout(triangle_strip, max_vertices = 6) out;\n"
   "layout(location = 0) in VertexData { vec3 v_tex0; } v_in[];\n"
   "layout(location = 0) out VertexData { vec3 v_tex0; } v_out;\n"
   "void main()\n"
   "{\n"
   "   int j, v;\n"
   "   for (j = 0; j < 2; j++)\n"
   "   {\n"
   "      for (v = 0; v < 3; v++)\n"
   "      {\n"
   "         gl_Layer = j;\n"
   "         gl_Position = gl_in[v].gl_Position;\n"
   "         gl_ClipDistance[0] = gl_in[v].gl_ClipDistance[0];\n"
   "         gl_ClipDistance[1] = gl_in[v].gl_ClipDistance[1];\n"
   "         v_out.v_tex0 = vec3(v_in[v].v_tex0.xy, float(j));\n"
   "         EmitVertex();\n"
   "      }\n"
   "      EndPrimitive();\n"
   "   }\n"
   "}\n";

/* A uniform block holding bools, as the ubershaders' does, and a depth
 * write: SPIR-V holds the bools as uint and declares DepthReplacing. */
static const char fs_bool_depth[] =
   "#version 450\n"
   "layout(std140, set = 0, binding = 0) uniform PSBlock { bool enabled; bvec2 flags; vec4 color; };\n"
   "layout(location = 0) in vec3 v_tex0;\n"
   "layout(location = 0) out vec4 ocol0;\n"
   "void main()\n"
   "{\n"
   "   ocol0 = enabled && flags.y ? color : vec4(v_tex0, 1.0);\n"
   "   gl_FragDepth = flags.x ? 0.5 : v_tex0.z;\n"
   "}\n";

/* A texture decoder, as parallel-gamecubewii generates one: texels from
 * a buffer, a block shared across the workgroup, an image written. */
static const char cs_decode[] =
   "#version 450\n"
   "layout(std140, set = 0, binding = 0) uniform UBO { uvec2 u_dst_size; uint u_src_offset; };\n"
   "layout(set = 0, binding = 9) uniform usamplerBuffer s_input_buffer;\n"
   "layout(rgba8, set = 0, binding = 11) uniform writeonly image2DArray output_image;\n"
   "shared uvec2 shared_temp[4];\n"
   "layout(local_size_x = 64, local_size_y = 1) in;\n"
   "void main()\n"
   "{\n"
   "   uint group = gl_LocalInvocationID.x / 16u;\n"
   "   uvec2 coords = gl_GlobalInvocationID.xy;\n"
   "   if ((gl_LocalInvocationID.x % 16u) == 0u)\n"
   "      shared_temp[group] = texelFetch(s_input_buffer, int(gl_WorkGroupID.x + u_src_offset)).rg;\n"
   "   memoryBarrierShared();\n"
   "   barrier();\n"
   "   if (all(lessThan(coords, u_dst_size)))\n"
   "      imageStore(output_image, ivec3(ivec2(coords), 0),\n"
   "                 vec4(unpackUnorm4x8(shared_temp[group].x).rgb, 1.0));\n"
   "}\n";

/* The bounding box: a coherent storage buffer, kept by atomics. */
static const char fs_bbox[] =
   "#version 450\n"
   "layout(std430, set = 2, binding = 0) coherent buffer BBox {\n"
   "  int bbox_data[4];\n"
   "};\n"
   "#define bbox_left bbox_data[0]\n"
   "#define bbox_right bbox_data[1]\n"
   "#define bbox_top bbox_data[2]\n"
   "#define bbox_bottom bbox_data[3]\n"
   "layout(location = 0) out vec4 col;\n"
   "void UpdateBoundingBoxBuffer(ivec2 min_pos, ivec2 max_pos) {\n"
   "  if (bbox_left > min_pos.x)\n"
   "    atomicMin(bbox_left, min_pos.x);\n"
   "  if (bbox_right < max_pos.x)\n"
   "    atomicMax(bbox_right, max_pos.x);\n"
   "  if (bbox_top > min_pos.y)\n"
   "    atomicMin(bbox_top, min_pos.y);\n"
   "  if (bbox_bottom < max_pos.y)\n"
   "    atomicMax(bbox_bottom, max_pos.y);\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   ivec2 p = ivec2(gl_FragCoord.xy);\n"
   "   UpdateBoundingBoxBuffer(p, p);\n"
   "   col = vec4(1.0);\n"
   "}\n";

/* The dynamic vertex loader: readonly storage buffers of unstated
 * length, one of structs. */
static const char vs_vertex_fetch[] =
   "#version 450\n"
   "uvec4 unpack_ubyte4(uint value) {\n"
   "  return uvec4(value & 0xffu, (value >> 8) & 0xffu, (value >> 16) & 0xffu, value >> 24);\n"
   "}\n"
   "struct InputData {\n"
   "  uint posmtx;\n"
   "  float pos0;\n"
   "  float pos1;\n"
   "  float pos2;\n"
   "  uint color0;\n"
   "};\n"
   "layout(std430, set = 2, binding = 1) readonly restrict buffer InputBuffer {\n"
   "  InputData input_buffer[];\n"
   "};\n"
   "layout(std430, set = 2, binding = 2) readonly restrict buffer Vertices {\n"
   "  uint vertex_buffer[];\n"
   "};\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   InputData i = input_buffer[gl_VertexIndex];\n"
   "   col = vec4(unpack_ubyte4(i.color0)) / 255.0 + vec4(float(vertex_buffer[gl_VertexIndex * 3 + 1]));\n"
   "   gl_Position = vec4(i.pos0, i.pos1, i.pos2, 1.0);\n"
   "}\n";

static enum rslc_stage cur_stage = RSLC_STAGE_FRAGMENT;

static bool compile(const char *src, enum rslc_dst dst, unsigned version,
      struct rslc_opts *opts, struct rslc_result *res, char *err, size_t err_cap)
{
   memset(opts, 0, sizeof(*opts));
   opts->dst     = dst;
   opts->version = version;
   opts->err     = err;
   opts->err_cap = err_cap;
   err[0]        = '\0';
   return rslc_compile(src, strlen(src), cur_stage, opts, res);
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

static void write_text(const char *dir, const char *name, const char *text)
{
   char  path[1024];
   FILE *f;
   if (!dir || strlen(dir) + strlen(name) + 2 > sizeof(path))
      return;
   sprintf(path, "%s/%s", dir, name);
   f = fopen(path, "wb");
   CHECK(f != NULL);
   if (!f)
      return;
   CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
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

   /* ClipDistance 32, CullDistance 33; HLSL carries them in
    * SV_ClipDistance0, MSL refuses them yet */
   cur_stage = RSLC_STAGE_VERTEX;
   {
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      memset(&r, 0, sizeof(r));
      CHECK(compile(vs_clip, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      if (r.blob)
      {
         CHECK(has_capability(r.spirv, r.spirv_len, 32));
         CHECK(!has_capability(r.spirv, r.spirv_len, 33));
         write_spirv(dir, "clip", &r);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(vs_clip, RSLC_DST_GLSL, 450, &o, &r, err, sizeof(err)));
      if (r.blob)
      {
         CHECK(strstr(r.text, "gl_ClipDistance[1]") != NULL);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(vs_clip, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
      if (r.blob)
      {
         CHECK(strstr(r.text, "float2 gl_ClipDistance0 : SV_ClipDistance0;") != NULL);
         write_text(dir, "clip.vert.hlsl", r.text);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(!compile(vs_clip, RSLC_DST_MSL, 20000, &o, &r, err, sizeof(err)));
      CHECK(!compile(vs_clip_bad, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "constant index below 8") != NULL);
   }

   /* in / out blocks, a geometry stage (Geometry 2), a compute stage and
    * storage buffers: SPIR-V and GLSL; HLSL save the geometry stage, its
    * text written beside the SPIR-V for an HLSL compiler to check */
   {
      static const struct { const char *name, *src; enum rslc_stage stage;
         const char *needle, *hlsl; }
      cases[] = {
         { "ioblock_vert", vs_ioblock, RSLC_STAGE_VERTEX, "out VertexData {",
            "VertexData_colors_0 : TEXCOORD1;" },
         { "ioblock_frag", fs_ioblock, RSLC_STAGE_FRAGMENT, "in VertexData {",
            "VertexData_colors_0 : TEXCOORD1;" },
         { "wide_line", gs_wide_line, RSLC_STAGE_GEOMETRY,
            "layout(triangle_strip, max_vertices = 4) out;", NULL },
         { "bbox", fs_bbox, RSLC_STAGE_FRAGMENT, "coherent buffer BBox {",
            "RWByteAddressBuffer BBox : register(u" },
         { "vertex_fetch", vs_vertex_fetch, RSLC_STAGE_VERTEX, "InputData input_buffer[];",
            "ByteAddressBuffer InputBuffer : register(t" },
         { "unnamed_vert", vs_unnamed, RSLC_STAGE_VERTEX, "out VertexData {",
            "VertexData_v_tex0 : TEXCOORD0;" },
         { "passthrough", gs_passthrough, RSLC_STAGE_GEOMETRY, "gl_in[", NULL },
         { "decode", cs_decode, RSLC_STAGE_COMPUTE,
            "layout(rgba8) uniform writeonly image2DArray output_image;",
            "[numthreads(64, 1, 1)]" }
      };
      unsigned i;
      for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      {
         char err[256];
         struct rslc_opts   o;
         struct rslc_result r;
         cur_stage = cases[i].stage;
         memset(&r, 0, sizeof(r));
         CHECK(compile(cases[i].src, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
         if (err[0])
            fprintf(stderr, "%s: %s\n", cases[i].name, err);
         if (r.blob)
         {
            if (cases[i].stage == RSLC_STAGE_GEOMETRY)
               CHECK(has_capability(r.spirv, r.spirv_len, 2));
            write_spirv(dir, cases[i].name, &r);
            rslc_result_free(&r, &o);
         }
         memset(&r, 0, sizeof(r));
         CHECK(compile(cases[i].src, RSLC_DST_GLSL, 450, &o, &r, err, sizeof(err)));
         if (r.blob)
         {
            CHECK(strstr(r.text, cases[i].needle) != NULL);
            rslc_result_free(&r, &o);
         }
         memset(&r, 0, sizeof(r));
         if (!cases[i].hlsl)
         {
            CHECK(!compile(cases[i].src, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
            continue;
         }
         CHECK(compile(cases[i].src, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
         if (err[0])
            fprintf(stderr, "%s hlsl: %s\n", cases[i].name, err);
         if (r.blob)
         {
            char name[64];
            CHECK(strstr(r.text, cases[i].hlsl) != NULL);
            sprintf(name, "%s.%s.hlsl", cases[i].name, cases[i].stage == RSLC_STAGE_VERTEX ? "vert"
                  : cases[i].stage == RSLC_STAGE_FRAGMENT ? "frag" : "comp");
            write_text(dir, name, r.text);
            rslc_result_free(&r, &o);
         }
      }
   }

   /* bools in a block and a depth write */
   {
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      cur_stage = RSLC_STAGE_FRAGMENT;
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_bool_depth, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "bool_depth: %s\n", err);
      if (r.blob)
      {
         write_spirv(dir, "bool_depth", &r);
         rslc_result_free(&r, &o);
      }
   }

   if (fails)
   {
      fprintf(stderr, "rslc_ext_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_ext_test: ok\n");
   return 0;
}
