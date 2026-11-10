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

/* A layered pass's pixel stage: the layer the geometry stage chose,
 * read through gl_Layer, picks the array slice. */
static const char fs_layer[] =
   "#version 450\n"
   "layout(set = 0, binding = 0) uniform sampler2DArray samp0;\n"
   "layout(location = 0) in vec3 v_tex0;\n"
   "layout(location = 0) out vec4 ocol0;\n"
   "void main()\n"
   "{\n"
   "   int layer = gl_Layer;\n"
   "   ocol0 = texture(samp0, vec3(v_tex0.xy, float(layer))) + vec4(gl_FragCoord.x * 0.0);\n"
   "}\n";

/* Calls with early returns, one from inside a loop, and an indexed
 * parameter: Direct3D bytecode expands each call where it is made. */
static const char fs_inline[] =
   "#version 450\n"
   "layout(location = 0) in vec4 c;\n"
   "layout(location = 0) out vec4 o;\n"
   "float find(vec4 v, float t)\n"
   "{\n"
   "   for (int i = 0; i < 4; i++)\n"
   "   {\n"
   "      if (v[i] > t)\n"
   "         return float(i);\n"
   "   }\n"
   "   return -1.0;\n"
   "}\n"
   "float pick(float x)\n"
   "{\n"
   "   if (x < 0.0)\n"
   "      return 0.0;\n"
   "   return x * 2.0;\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   o = vec4(find(c, 0.5), pick(c.x), find(c, pick(c.y)), 1.0);\n"
   "}\n";

/* What a pixel stage may not do with it. */
static const char fs_layer_write[] =
   "#version 450\n"
   "layout(location = 0) out vec4 ocol0;\n"
   "void main() { gl_Layer = 1; ocol0 = vec4(1.0); }\n";

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

/* The other sampler shapes and the shadow samplers, as a Wii U
 * emulator's shaders read them: every texture function on each */
static const char fs_shapes[] =
   "#version 450\n"
   "layout(set = 1, binding = 0) uniform sampler1D s1;\n"
   "layout(set = 1, binding = 1) uniform sampler3D s3;\n"
   "layout(set = 1, binding = 2) uniform samplerCube sc;\n"
   "layout(set = 1, binding = 3) uniform samplerCubeArray sca;\n"
   "layout(set = 1, binding = 4) uniform usampler1D u1;\n"
   "layout(set = 1, binding = 5) uniform sampler2DArray sa;\n"
   "layout(location = 0) in vec4 uv;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   int  w  = textureSize(s1, 0) + textureQueryLevels(s1) + textureQueryLevels(sa);\n"
   "   ivec3 d = textureSize(s3, 1) + textureSize(sca, 0);\n"
   "   ivec2 c = textureSize(sc, 0);\n"
   "   vec4 a  = texture(s1, uv.x) + texture(s1, uv.x, 0.5) + textureLod(s1, uv.x, 1.0)\n"
   "           + textureOffset(s1, uv.x, 1) + textureLodOffset(s1, uv.x, 0.0, -1)\n"
   "           + textureGrad(s1, uv.x, 0.1, 0.2) + texelFetch(s1, w - 1, 0);\n"
   "   vec4 b  = texture(s3, uv.xyz) + textureLod(s3, uv.xyz, 2.0) + textureOffset(s3, uv.xyz, ivec3(1, 0, -1))\n"
   "           + textureGrad(s3, uv.xyz, vec3(0.1), vec3(0.2)) + texelFetch(s3, d - 1, 0);\n"
   "   vec4 e  = texture(sc, uv.xyz) + textureLod(sc, uv.xyz, 1.0) + textureGather(sc, uv.xyz, 2)\n"
   "           + texture(sca, uv) + textureLod(sca, uv, 0.5) + textureGather(sca, uv)\n"
   "           + textureGrad(sca, uv, vec3(0.1), vec3(0.2));\n"
   "   vec4 f  = textureOffset(sa, uv.xyz, ivec2(1, 1)) + textureLodOffset(sa, uv.xyz, 1.0, ivec2(-1, 0))\n"
   "           + textureGrad(sa, uv.xyz, vec2(0.1), vec2(0.2)) + textureGather(sa, uv.xyz, 1);\n"
   "   vec2 l  = textureQueryLod(s1, uv.x) + textureQueryLod(s3, uv.xyz) + textureQueryLod(sa, uv.xy);\n"
   "   uvec4 u = texture(u1, uv.x) + texelFetch(u1, 0, 0);\n"
   "   col = a + b + e + f + vec4(u) + vec4(l, float(c.x), float(w + d.z));\n"
   "}\n";

static const char fs_shadow[] =
   "#version 450\n"
   "layout(set = 1, binding = 0) uniform sampler1DShadow h1;\n"
   "layout(set = 1, binding = 1) uniform sampler2DShadow h2;\n"
   "layout(set = 1, binding = 2) uniform sampler2DArrayShadow ha;\n"
   "layout(set = 1, binding = 3) uniform samplerCubeShadow hc;\n"
   "layout(set = 1, binding = 4) uniform samplerCubeArrayShadow hr;\n"
   "layout(location = 0) in vec4 uv;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   float a = texture(h1, uv.xyz) + textureLod(h1, uv.xyz, 1.0) + textureOffset(h1, uv.xyz, 1)\n"
   "           + textureGrad(h1, uv.xyz, 0.1, 0.2);\n"
   "   float b = texture(h2, uv.xyz) + texture(h2, uv.xyz, 0.5) + textureLod(h2, uv.xyz, 0.0)\n"
   "           + textureOffset(h2, uv.xyz, ivec2(1, -1)) + textureLodOffset(h2, uv.xyz, 0.0, ivec2(1, 0))\n"
   "           + textureGrad(h2, uv.xyz, vec2(0.1), vec2(0.2));\n"
   "   float c = texture(ha, uv) + textureOffset(ha, uv, ivec2(0, 1)) + textureGrad(ha, uv, vec2(0.1), vec2(0.2));\n"
   "   float d = texture(hc, uv) + texture(hr, uv, 0.5);\n"
   "   vec4  g = textureGather(h2, uv.xy, uv.z) + textureGather(ha, uv.xyz, uv.w) + textureGather(hr, uv, 0.5);\n"
   "   ivec3 s = textureSize(hr, 0) + ivec3(textureSize(h2, 0), textureSize(h1, 0));\n"
   "   col = vec4(a, b, c, d) + g + vec4(s, 0.0);\n"
   "}\n";

/* A vertex stage that redeclares its gl_PerVertex and asks, through
 * GL_EXT_spirv_intrinsics, for round-to-even arithmetic */
static const char vs_pervertex[] =
   "#version 450\n"
   "#extension GL_EXT_spirv_intrinsics: enable\n"
   "spirv_execution_mode(capabilities = [4467], extensions = [\"SPV_KHR_float_controls\"], 4462, 32);\n"
   "out gl_PerVertex\n"
   "{\n"
   "   vec4 gl_Position;\n"
   "   float gl_PointSize;\n"
   "};\n"
   "layout(location = 0) in vec4 pos;\n"
   "void main()\n"
   "{\n"
   "   gl_Position  = pos;\n"
   "   gl_PointSize = 2.0;\n"
   "}\n";

/* A float controls mode without its capability or extension: both are
 * added, as the module needs them */
static const char fs_rte_bare[] =
   "#version 450\n"
   "spirv_execution_mode(4462, 32);\n"
   "spirv_execution_mode(4461, 32);\n"
   "layout(location = 0) out vec4 col;\n"
   "void main() { col = vec4(1.0); }\n";

/* A pixel stage redeclaring gl_FragCoord, reading gl_PointCoord */
static const char fs_point[] =
   "#version 450\n"
   "in vec4 gl_FragCoord;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main()\n"
   "{\n"
   "   col = vec4(gl_PointCoord, gl_FragCoord.xy);\n"
   "}\n";

/* A geometry stage's inputs as plain arrays, sized by the primitive,
 * and its gl_PrimitiveIDIn */
static const char gs_arrays[] =
   "#version 450\n"
   "layout(triangles) in;\n"
   "layout(triangle_strip) out;\n"
   "layout(max_vertices = 3) out;\n"
   "layout(location = 0) in vec4 vcol[];\n"
   "layout(location = 1) flat in ivec4 vid[];\n"
   "layout(location = 0) out vec4 gcol;\n"
   "void main()\n"
   "{\n"
   "   for (int i = 0; i < 3; i++)\n"
   "   {\n"
   "      gcol = vcol[i] + vec4(vid[i]) + vec4(float(gl_PrimitiveIDIn));\n"
   "      gl_Position = gl_in[i].gl_Position;\n"
   "      EmitVertex();\n"
   "   }\n"
   "   EndPrimitive();\n"
   "}\n";

static const char gs_array_early[] =
   "#version 450\n"
   "layout(location = 0) in vec4 vcol[];\n"
   "layout(triangles) in;\n"
   "layout(points, max_vertices = 1) out;\n"
   "void main() { gl_Position = vcol[0]; EmitVertex(); }\n";

static const char fs_redecl_bad[] =
   "#version 450\n"
   "in vec3 gl_FragCoord;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main() { col = vec4(1.0); }\n";

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

/* Framebuffer fetch: an input attachment, read in main and in a
 * function it calls. */
static const char fs_subpass[] =
   "#version 450\n"
   "layout(location = 0, index = 0) out vec4 real_ocol0;\n"
   "layout(location = 0, index = 1) out vec4 ocol1;\n"
   "layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput in_ocol0;\n"
   "layout(location = 0) in vec4 c;\n"
   "float dst_alpha() { return subpassLoad(in_ocol0).a; }\n"
   "void main()\n"
   "{\n"
   "   vec4 initial_ocol0 = subpassLoad(in_ocol0);\n"
   "   real_ocol0 = c * dst_alpha() + initial_ocol0;\n"
   "   ocol1 = initial_ocol0;\n"
   "}\n";

/* Framebuffer fetch with no colour output for the attachment's index */
static const char fs_subpass_no_out[] =
   "#version 450\n"
   "layout(set = 0, binding = 0, input_attachment_index = 1) uniform subpassInput in_ocol1;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main() { col = subpassLoad(in_ocol1); }\n";

static const char fs_subpass_bad[] =
   "#version 450\n"
   "layout(set = 0, binding = 0) uniform subpassInput in_ocol0;\n"
   "layout(location = 0) out vec4 col;\n"
   "void main() { col = subpassLoad(in_ocol0); }\n";

/* The bounding box reduced over the subgroup: helper invocations
 * stay out, the first active one writes. */
static const char fs_subgroup[] =
   "#version 450\n"
   "#extension GL_KHR_shader_subgroup_basic : enable\n"
   "#extension GL_KHR_shader_subgroup_arithmetic : enable\n"
   "layout(std430, set = 2, binding = 0) coherent buffer BBox {\n"
   "  int bbox_data[4];\n"
   "};\n"
   "layout(location = 0) out vec4 col;\n"
   "void update(ivec2 tl, ivec2 br) {\n"
   "  atomicMin(bbox_data[0], tl.x);\n"
   "  atomicMax(bbox_data[1], br.x);\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   ivec2 pos_tl = ivec2(gl_FragCoord.xy) & ~1;\n"
   "   ivec2 pos_br = ivec2(gl_FragCoord.xy) | 1;\n"
   "   if (!gl_HelperInvocation)\n"
   "   {\n"
   "      pos_tl = subgroupMin(pos_tl);\n"
   "      pos_br = subgroupMax(pos_br);\n"
   "      if (subgroupElect())\n"
   "         update(pos_tl, pos_br);\n"
   "   }\n"
   "   col = vec4(subgroupMax(gl_FragCoord.x));\n"
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

/* How many of the instruction there are, and the first one's words
 * after its opcode in *at */
static unsigned count_op(const uint32_t *w, size_t n, uint32_t op, size_t *at)
{
   size_t   i = 5;
   unsigned k = 0;
   while (i < n)
   {
      uint32_t len = w[i] >> 16;
      if (!len)
         break;
      if ((w[i] & 0xffffu) == op)
      {
         if (!k && at)
            *at = i + 1;
         k++;
      }
      i += len;
   }
   return k;
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

/* SPIR-V, GLSL, HLSL and MSL; the MSL is written beside the SPIR-V
 * (msl_needle NULL: MSL refuses the stage). */
static void test_stage(const char *dir, const char *name, const char *src,
      const char *glsl_needle, const char *hlsl_needle, uint32_t want_cap, uint32_t no_cap,
      const char *msl_needle)
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
   if (!msl_needle)
   {
      CHECK(!compile(src, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "msl") != NULL);
      return;
   }
   CHECK(compile(src, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
   if (err[0])
      fprintf(stderr, "%s msl: %s\n", name, err);
   if (r.blob)
   {
      char file[64];
      CHECK(strstr(r.text, msl_needle) != NULL);
      sprintf(file, "%s.msl", name);
      write_text(dir, file, r.text);
      rslc_result_free(&r, &o);
   }
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;

   /* SampledBuffer 46, ImageQuery 50, SampleRateShading 35 */
   test_stage(dir, "samplers", fs_samplers, "usamplerBuffer", "Texture2DMSArray<float4>", 46, 35,
         "texture2d_ms_array<float>");
   test_stage(dir, "sample_id", fs_sample_id, "gl_SampleID", "SV_SampleIndex", 35, 0, "[[sample_id]]");
   test_stage(dir, "plain", fs_plain, "sampler2DArray", "Texture2DArray<float4>", 0, 35,
         "texture2d_array<float>");
   test_stage(dir, "blocks", fs_blocks, "uniform GSBlock", "cbuffer GSBlock : register(b2)", 0, 0,
         NULL);   /* MSL: the push block's buffer 1 is VSBlock's binding */
   test_stage(dir, "sampler_array", fs_sampler_array, "sampler2DArray samp[8]",
         "samp[i].Sample(_samp_sampler[i]", 0, 0, "array<texture2d_array<float>, 8> samp [[texture(");
   test_stage(dir, "dual_source", fs_dual_source, "layout(early_fragment_tests) in;",
         "SV_Target1", 0, 0, "[[color(0), index(1)]]");

   /* ClipDistance 32, CullDistance 33; HLSL carries them in
    * SV_ClipDistance0, MSL in a [[clip_distance]] array */
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
      CHECK(compile(vs_clip, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
      if (r.blob)
      {
         CHECK(strstr(r.text, "[[clip_distance]] [2];") != NULL);
         write_text(dir, "clip.msl", r.text);
         rslc_result_free(&r, &o);
      }
      CHECK(!compile(vs_clip_bad, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "constant index below 8") != NULL);
   }

   /* in / out blocks, a geometry stage (Geometry 2), a compute stage and
    * storage buffers: SPIR-V and GLSL; HLSL save the geometry stage; MSL
    * save it and the in / out blocks; the texts written beside the SPIR-V
    * for a compiler to check */
   {
      static const struct { const char *name, *src; enum rslc_stage stage;
         const char *needle, *hlsl, *msl; }
      cases[] = {
         { "ioblock_vert", vs_ioblock, RSLC_STAGE_VERTEX, "out VertexData {",
            "VertexData_colors_0 : TEXCOORD1;", NULL },
         { "ioblock_frag", fs_ioblock, RSLC_STAGE_FRAGMENT, "in VertexData {",
            "VertexData_colors_0 : TEXCOORD1;", NULL },
         { "wide_line", gs_wide_line, RSLC_STAGE_GEOMETRY,
            "layout(triangle_strip, max_vertices = 4) out;", NULL, NULL },
         { "bbox", fs_bbox, RSLC_STAGE_FRAGMENT, "coherent buffer BBox {",
            "RWByteAddressBuffer BBox : register(u", "atomic_fetch_min_explicit((volatile device atomic_int*)&" },
         { "vertex_fetch", vs_vertex_fetch, RSLC_STAGE_VERTEX, "InputData input_buffer[];",
            "ByteAddressBuffer InputBuffer : register(t", "const device InputBuffer& " },
         { "unnamed_vert", vs_unnamed, RSLC_STAGE_VERTEX, "out VertexData {",
            "VertexData_v_tex0 : TEXCOORD0;", NULL },
         { "passthrough", gs_passthrough, RSLC_STAGE_GEOMETRY, "gl_in[", NULL, NULL },
         { "layer_frag", fs_layer, RSLC_STAGE_FRAGMENT, "int layer = gl_Layer;",
            ": SV_RenderTargetArrayIndex;", "uint gl_Layer_in [[render_target_array_index]]" },
         { "decode", cs_decode, RSLC_STAGE_COMPUTE,
            "layout(rgba8) uniform writeonly image2DArray output_image;",
            "[numthreads(64, 1, 1)]", "kernel void main0(" }
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
            /* Geometry 2: a geometry stage, and a pixel stage reading
             * the layer */
            if (cases[i].stage == RSLC_STAGE_GEOMETRY || cases[i].src == fs_layer)
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
         if (!cases[i].msl)
            CHECK(!compile(cases[i].src, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
         else
         {
            CHECK(compile(cases[i].src, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
            if (err[0])
               fprintf(stderr, "%s msl: %s\n", cases[i].name, err);
            if (r.blob)
            {
               char file[64];
               CHECK(strstr(r.text, cases[i].msl) != NULL);
               sprintf(file, "%s.msl", cases[i].name);
               write_text(dir, file, r.text);
               rslc_result_free(&r, &o);
            }
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

   /* an input attachment (InputAttachment 40): SPIR-V and MSL, which
    * takes it as the colour attachment's value; GLSL as the caller's
    * framebuffer fetch extension, refused without one; HLSL refuses */
   {
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      cur_stage = RSLC_STAGE_FRAGMENT;
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_subpass, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "subpass: %s\n", err);
      if (r.blob)
      {
         CHECK(has_capability(r.spirv, r.spirv_len, 40));
         write_spirv(dir, "subpass", &r);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_subpass, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "subpass msl: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "float4 in_ocol0 [[color(0)]]") != NULL);
         CHECK(strstr(r.text, "dst_alpha(float4 in_ocol0)") != NULL);
         write_text(dir, "subpass.msl", r.text);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(!compile(fs_subpass, RSLC_DST_GLSL, 450, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "subpassInput") != NULL);
      {
         static const struct { enum rslc_dst dst; unsigned version, fetch; const char *ext, *decl,
               *read; } gl[] = {
            { RSLC_DST_GLSL_ES, 320, RSLC_FB_FETCH_EXT, "GL_EXT_shader_framebuffer_fetch",
               "inout vec4 real_ocol0;", "vec4 initial_ocol0 = real_ocol0;" },
            { RSLC_DST_GLSL, 450, RSLC_FB_FETCH_EXT, "GL_EXT_shader_framebuffer_fetch",
               "inout vec4 real_ocol0;", "return real_ocol0.w;" },
            { RSLC_DST_GLSL_ES, 320, RSLC_FB_FETCH_ARM, "GL_ARM_shader_framebuffer_fetch",
               "out vec4 real_ocol0;", "vec4 initial_ocol0 = gl_LastFragColorARM;" } };
         unsigned i;
         for (i = 0; i < sizeof(gl) / sizeof(gl[0]); i++)
         {
            memset(&o, 0, sizeof(o));
            o.dst     = gl[i].dst;
            o.version = gl[i].version;
            o.err     = err;
            o.err_cap = sizeof(err);
            o.fb_fetch = gl[i].fetch;
            err[0]    = '\0';
            memset(&r, 0, sizeof(r));
            CHECK(rslc_compile(fs_subpass, strlen(fs_subpass), RSLC_STAGE_FRAGMENT, &o, &r));
            if (err[0])
               fprintf(stderr, "subpass glsl: %s\n", err);
            if (r.blob)
            {
               CHECK(strstr(r.text, gl[i].ext) != NULL);
               CHECK(strstr(r.text, gl[i].decl) != NULL);
               CHECK(strstr(r.text, gl[i].read) != NULL);
               CHECK(strstr(r.text, "subpass") == NULL);
               if (gl[i].fetch == RSLC_FB_FETCH_ARM)
                  CHECK(strstr(r.text, "inout") == NULL);
               if (!i)
                  write_text(dir, "subpass.glsl", r.text);
               rslc_result_free(&r, &o);
            }
            /* GL_EXT_shader_framebuffer_fetch reads the output at the
             * attachment's index, which has to be there */
            o.err = err;
            err[0] = '\0';
            CHECK(rslc_compile(fs_subpass_no_out, strlen(fs_subpass_no_out), RSLC_STAGE_FRAGMENT,
                  &o, &r) == (gl[i].fetch == RSLC_FB_FETCH_ARM));
            if (r.blob && gl[i].fetch == RSLC_FB_FETCH_ARM)
               rslc_result_free(&r, &o);
            if (gl[i].fetch == RSLC_FB_FETCH_EXT)
               CHECK(strstr(err, "colour output") != NULL);
            memset(&r, 0, sizeof(r));
         }
      }
      CHECK(!compile(fs_subpass, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "subpassInput") != NULL);
      CHECK(!compile(fs_subpass_bad, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "input_attachment_index") != NULL);
   }

   /* early fragment tests as conservative depth's depth_unchanged, for
    * a GL context without image load / store: under the extension
    * below GLSL 4.20 and in GLSL ES */
   {
      static const struct { enum rslc_dst dst; unsigned version; const char *ext; } gl[] = {
         { RSLC_DST_GLSL, 410, "#extension GL_ARB_conservative_depth : require" },
         { RSLC_DST_GLSL, 450, NULL },
         { RSLC_DST_GLSL_ES, 320, "#extension GL_EXT_conservative_depth : require" } };
      unsigned i;
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      cur_stage = RSLC_STAGE_FRAGMENT;
      for (i = 0; i < sizeof(gl) / sizeof(gl[0]); i++)
      {
         memset(&o, 0, sizeof(o));
         o.dst     = gl[i].dst;
         o.version = gl[i].version;
         o.err     = err;
         o.err_cap = sizeof(err);
         o.early_z_depth_unchanged = true;
         err[0]    = '\0';
         memset(&r, 0, sizeof(r));
         CHECK(rslc_compile(fs_dual_source, strlen(fs_dual_source), RSLC_STAGE_FRAGMENT, &o, &r));
         if (err[0])
            fprintf(stderr, "depth_unchanged: %s\n", err);
         if (r.blob)
         {
            CHECK(strstr(r.text, "layout(depth_unchanged) out float gl_FragDepth;") != NULL);
            CHECK(strstr(r.text, "early_fragment_tests") == NULL);
            CHECK(gl[i].ext ? strstr(r.text, gl[i].ext) != NULL
                  : strstr(r.text, "conservative_depth") == NULL);
            if (gl[i].version == 450)
               write_text(dir, "depth_unchanged.glsl", r.text);
            rslc_result_free(&r, &o);
         }
      }
   }

   /* GLSL ES: a sampler type without a default precision is declared
    * highp, as the stage cannot compile without one; sampler2D keeps its
    * default */
   {
      static const char fs_sampler2d[] =
         "#version 450\n"
         "layout(set = 0, binding = 0) uniform sampler2D tex;\n"
         "layout(location = 0) in vec2 uv;\n"
         "layout(location = 0) out vec4 col;\n"
         "void main() { col = texture(tex, uv); }\n";
      static const char fs_sampler_param[] =
         "#version 450\n"
         "layout(set = 0, binding = 0) uniform sampler2DArray samp[2];\n"
         "layout(location = 0) in vec3 uv;\n"
         "layout(location = 0) out vec4 col;\n"
         "vec4 fetch(sampler2DArray s) { return texture(s, uv); }\n"
         "void main() { col = fetch(samp[0]) + fetch(samp[1]); }\n";
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      cur_stage = RSLC_STAGE_FRAGMENT;
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_sampler_array, RSLC_DST_GLSL_ES, 310, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "es sampler array: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "uniform highp sampler2DArray samp[8];") != NULL);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_samplers, RSLC_DST_GLSL_ES, 320, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "es samplers: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "uniform highp sampler2DArray samp;") != NULL);
         CHECK(strstr(r.text, "uniform highp usamplerBuffer texels;") != NULL);
         CHECK(strstr(r.text, "uniform highp sampler2DMSArray ms;") != NULL);
         CHECK(strstr(r.text, "uniform highp isampler2DArray isamp;") != NULL);
         write_text(dir, "es_samplers.glsl", r.text);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_sampler2d, RSLC_DST_GLSL_ES, 310, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "es sampler2D: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "uniform sampler2D tex;") != NULL);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_sampler_param, RSLC_DST_GLSL_ES, 310, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "es sampler parameter: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "highp sampler2DArray s") != NULL);
         write_text(dir, "es_sampler_param.glsl", r.text);
         rslc_result_free(&r, &o);
      }
   }

   /* subgroup operations (GroupNonUniform 61, its Arithmetic 63) in a
    * SPIR-V 1.3 module; MSL's simdgroup functions; GLSL's under
    * GL_KHR_shader_subgroup, read back; HLSL refuses */
   {
      char err[256];
      struct rslc_opts   o;
      struct rslc_result r;
      cur_stage = RSLC_STAGE_FRAGMENT;
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_subgroup, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "subgroup: %s\n", err);
      if (r.blob)
      {
         CHECK(r.spirv[1] == 0x00010300u);
         CHECK(has_capability(r.spirv, r.spirv_len, 61));
         CHECK(has_capability(r.spirv, r.spirv_len, 63));
         write_spirv(dir, "subgroup", &r);
         rslc_result_free(&r, &o);
      }
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_subgroup, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
      if (err[0])
         fprintf(stderr, "subgroup msl: %s\n", err);
      if (r.blob)
      {
         CHECK(strstr(r.text, "if (!simd_is_helper_thread())") != NULL);
         CHECK(strstr(r.text, "simd_min(pos_tl)") != NULL);
         CHECK(strstr(r.text, "if (simd_is_first())") != NULL);
         write_text(dir, "subgroup.msl", r.text);
         rslc_result_free(&r, &o);
      }
      {
         static const struct { enum rslc_dst dst; unsigned version; } gl[] = {
            { RSLC_DST_GLSL, 450 }, { RSLC_DST_GLSL_ES, 320 } };
         unsigned i;
         for (i = 0; i < sizeof(gl) / sizeof(gl[0]); i++)
         {
            memset(&r, 0, sizeof(r));
            CHECK(compile(fs_subgroup, gl[i].dst, gl[i].version, &o, &r, err, sizeof(err)));
            if (err[0])
               fprintf(stderr, "subgroup glsl: %s\n", err);
            if (r.blob)
            {
               struct rslc_opts   o2;
               struct rslc_result r2;
               char err2[256];
               CHECK(strstr(r.text, "#extension GL_KHR_shader_subgroup_basic : require") != NULL);
               CHECK(strstr(r.text, "#extension GL_KHR_shader_subgroup_arithmetic : require") != NULL);
               CHECK(strstr(r.text, "subgroupMin(") != NULL);
               CHECK(strstr(r.text, "subgroupElect()") != NULL);
               CHECK(strstr(r.text, "gl_HelperInvocation") != NULL);
               if (gl[i].dst == RSLC_DST_GLSL)
               {
                  /* what it printed reads back as the same stage */
                  memset(&r2, 0, sizeof(r2));
                  CHECK(compile(r.text, RSLC_DST_SPIRV, 0, &o2, &r2, err2, sizeof(err2)));
                  if (err2[0])
                     fprintf(stderr, "subgroup glsl read back: %s\n", err2);
                  if (r2.blob)
                  {
                     CHECK(has_capability(r2.spirv, r2.spirv_len, 63));
                     rslc_result_free(&r2, &o2);
                  }
                  write_text(dir, "subgroup.glsl", r.text);
               }
               rslc_result_free(&r, &o);
            }
         }
      }
      memset(&r, 0, sizeof(r));
      CHECK(!compile(fs_subgroup, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
      CHECK(strstr(err, "subgroup") != NULL);
      /* a plain stage stays SPIR-V 1.0 */
      memset(&r, 0, sizeof(r));
      CHECK(compile(fs_plain, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
      if (r.blob)
      {
         CHECK(r.spirv[1] == 0x00010000u);
         rslc_result_free(&r, &o);
      }
   }

   /* Direct3D bytecode: a container of the input and output signatures
    * and the shader model 5 program (a geometry stage's outputs with
    * their streams); written beside the SPIR-V for a disassembler to
    * check */
   {
      static const struct { const char *name, *src; enum rslc_stage stage; uint32_t prog; }
      dx[] = {
         { "samplers", fs_samplers, RSLC_STAGE_FRAGMENT, 0 },
         { "dual_source", fs_dual_source, RSLC_STAGE_FRAGMENT, 0 },
         { "clip", vs_clip, RSLC_STAGE_VERTEX, 1 },
         { "bbox", fs_bbox, RSLC_STAGE_FRAGMENT, 0 },
         { "vertex_fetch", vs_vertex_fetch, RSLC_STAGE_VERTEX, 1 },
         { "decode", cs_decode, RSLC_STAGE_COMPUTE, 5 },
         { "wide_line", gs_wide_line, RSLC_STAGE_GEOMETRY, 2 },
         { "passthrough", gs_passthrough, RSLC_STAGE_GEOMETRY, 2 },
         { "layer_frag", fs_layer, RSLC_STAGE_FRAGMENT, 0 },
         { "inline", fs_inline, RSLC_STAGE_FRAGMENT, 0 }
      };
      unsigned i;
      for (i = 0; i < sizeof(dx) / sizeof(dx[0]); i++)
      {
         char err[256];
         struct rslc_opts   o;
         struct rslc_result r;
         cur_stage = dx[i].stage;
         memset(&r, 0, sizeof(r));
         CHECK(compile(dx[i].src, RSLC_DST_DXBC, 50, &o, &r, err, sizeof(err)));
         if (err[0])
            fprintf(stderr, "%s dxbc: %s\n", dx[i].name, err);
         if (r.blob)
         {
            const uint8_t *b = (const uint8_t*)r.spirv;
            uint32_t total = r.spirv[6], shex;
            CHECK(!memcmp(b, "DXBC", 4) && r.spirv[5] == 1 && total == r.spirv_len * 4);
            CHECK(r.spirv[7] == 3);
            CHECK(!memcmp(b + r.spirv[8], "ISGN", 4));
            CHECK(!memcmp(b + r.spirv[9], dx[i].prog == 2 ? "OSG5" : "OSGN", 4));
            shex = r.spirv[10];
            CHECK(!memcmp(b + shex, "SHEX", 4));
            /* the program's type and model, and its length in words */
            CHECK(r.spirv[shex / 4 + 2] == ((dx[i].prog << 16) | 0x50u));
            CHECK(r.spirv[shex / 4 + 3] * 4 == r.spirv[shex / 4 + 1]);
            /* no subroutines: call and label are not taken everywhere */
            {
               const uint32_t *t = r.spirv + shex / 4 + 2;
               uint32_t words = t[1], k = 2;
               while (k < words)
               {
                  uint32_t op = t[k] & 0x7ffu;
                  uint32_t len = op == 53 ? t[k + 1] : (t[k] >> 24) & 0x7fu;
                  CHECK(op != 4 && op != 44);
                  if (!len)
                     break;
                  k += len;
               }
            }
            {
               char file[64];
               FILE *f;
               sprintf(file, "%s.dxbc", dx[i].name);
               if (dir && strlen(dir) + strlen(file) + 2 < 1024)
               {
                  char path[1024];
                  sprintf(path, "%s/%s", dir, file);
                  f = fopen(path, "wb");
                  CHECK(f != NULL);
                  if (f)
                  {
                     CHECK(fwrite(r.spirv, 4, r.spirv_len, f) == r.spirv_len);
                     fclose(f);
                  }
               }
            }
            rslc_result_free(&r, &o);
         }
      }
      /* what Direct3D bytecode has no form of */
      cur_stage = RSLC_STAGE_FRAGMENT;
      {
         char err[256];
         struct rslc_opts   o;
         struct rslc_result r;
         CHECK(!compile(fs_subpass, RSLC_DST_DXBC, 50, &o, &r, err, sizeof(err)));
         CHECK(!compile(fs_subgroup, RSLC_DST_DXBC, 50, &o, &r, err, sizeof(err)));
         /* a pixel stage reads the layer, never writes it */
         CHECK(!compile(fs_layer_write, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
         cur_stage = RSLC_STAGE_COMPUTE;
         CHECK(!compile(cs_decode, RSLC_DST_DXBC, 40, &o, &r, err, sizeof(err)));
         CHECK(strstr(err, "shader model 5") != NULL);
      }
   }

   /* the other sampler shapes and the shadow samplers (Sampled1D 43,
    * SampledCubeArray 45, ImageQuery 50; the depth comparing ones'
    * OpImageSampleDref* 89 and 90, OpImageDrefGather 97), the builtins
    * redeclared, spirv_execution_mode, gl_PointCoord and a geometry
    * stage's plain input arrays: SPIR-V only, every other destination
    * refusing */
   {
      static const struct { const char *name, *src; enum rslc_stage stage; } sp[] = {
         { "shapes", fs_shapes, RSLC_STAGE_FRAGMENT },
         { "shadow", fs_shadow, RSLC_STAGE_FRAGMENT },
         { "pervertex", vs_pervertex, RSLC_STAGE_VERTEX },
         { "rte_bare", fs_rte_bare, RSLC_STAGE_FRAGMENT },
         { "point", fs_point, RSLC_STAGE_FRAGMENT },
         { "gs_arrays", gs_arrays, RSLC_STAGE_GEOMETRY }
      };
      unsigned i;
      for (i = 0; i < sizeof(sp) / sizeof(sp[0]); i++)
      {
         char err[256];
         struct rslc_opts   o;
         struct rslc_result r;
         size_t at = 0;
         cur_stage = sp[i].stage;
         memset(&r, 0, sizeof(r));
         CHECK(compile(sp[i].src, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
         if (err[0])
            fprintf(stderr, "%s: %s\n", sp[i].name, err);
         if (!r.blob)
            continue;
         if (sp[i].src == fs_shapes)
         {
            CHECK(has_capability(r.spirv, r.spirv_len, 43));
            CHECK(has_capability(r.spirv, r.spirv_len, 45));
            CHECK(has_capability(r.spirv, r.spirv_len, 50));
            CHECK(!count_op(r.spirv, r.spirv_len, 89, NULL));
         }
         if (sp[i].src == fs_shadow)
         {
            /* the implicit ones: 1D's two, 2D's three, 2D array's two,
             * cube, cube array; the explicit ones: 1D's lod and grad,
             * 2D's lod, lod offset and grad, 2D array's grad */
            CHECK(count_op(r.spirv, r.spirv_len, 89, NULL) == 9);
            CHECK(count_op(r.spirv, r.spirv_len, 90, NULL) == 6);
            CHECK(count_op(r.spirv, r.spirv_len, 97, NULL) == 3);
         }
         if (sp[i].src == fs_rte_bare)
         {
            /* RoundingModeRTE 4467, SignedZeroInfNanPreserve 4466 */
            CHECK(has_capability(r.spirv, r.spirv_len, 4467));
            CHECK(has_capability(r.spirv, r.spirv_len, 4466));
            CHECK(count_op(r.spirv, r.spirv_len, 10, &at) == 1
                  && !memcmp(r.spirv + at, "SPV_KHR_float_controls", 22));
         }
         if (sp[i].src == vs_pervertex)
         {
            /* RoundingModeRTE 4467 with its extension, the mode on main */
            CHECK(has_capability(r.spirv, r.spirv_len, 4467));
            CHECK(count_op(r.spirv, r.spirv_len, 10, &at) == 1
                  && !memcmp(r.spirv + at, "SPV_KHR_float_controls", 22));
            CHECK(count_op(r.spirv, r.spirv_len, 16, &at) == 1
                  && r.spirv[at + 1] == 4462 && r.spirv[at + 2] == 32);
         }
         write_spirv(dir, sp[i].name, &r);
         rslc_result_free(&r, &o);
         memset(&r, 0, sizeof(r));
         if (sp[i].src != gs_arrays)
         {
            CHECK(!compile(sp[i].src, RSLC_DST_GLSL, 450, &o, &r, err, sizeof(err)));
            CHECK(strstr(err, "only the SPIR-V destination") != NULL);
            CHECK(!compile(sp[i].src, RSLC_DST_HLSL, 50, &o, &r, err, sizeof(err)));
            CHECK(!compile(sp[i].src, RSLC_DST_MSL, 23, &o, &r, err, sizeof(err)));
            CHECK(!compile(sp[i].src, RSLC_DST_DXBC, 50, &o, &r, err, sizeof(err)));
         }
      }
      cur_stage = RSLC_STAGE_GEOMETRY;
      {
         char err[256];
         struct rslc_opts   o;
         struct rslc_result r;
         CHECK(!compile(gs_array_early, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
         CHECK(strstr(err, "before the input primitive") != NULL);
         cur_stage = RSLC_STAGE_FRAGMENT;
         CHECK(!compile(fs_redecl_bad, RSLC_DST_SPIRV, 0, &o, &r, err, sizeof(err)));
         CHECK(strstr(err, "another type") != NULL);
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
