/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_ext_build_test.c).
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

/* rslc builder test, RSLC_EXTENDED: the stages, types and storage an
 * emulator's shaders use.  Each stage is written twice, as GLSL and as
 * builder calls; both must compile to the same words and print the same
 * text for every destination that takes the stage. */

#include <stdio.h>
#include <string.h>

#include <gfx/shaders/rslc.h>
#include <gfx/shaders/rslc_build.h>

static int fails;

#define CHECK(cond) do { \
   if (!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++; \
   } \
} while (0)

typedef struct rslc_builder *(*build_fn)(const struct rslc_opts *opts);

#define S(x) rslc_b_stmt(b, x)
#define E(x) S(rslc_b_expr(b, x))

/* ---- vertex: an unnamed uniform block, an out block, clip distances ---- */

static const char vs_glsl[] =
   "#version 450\n"
   "layout(location = 0) in vec4 rawpos;\n"
   "layout(location = 1) in uvec4 posmtx;\n"
   "layout(std140, set = 0, binding = 1) uniform VSBlock { vec4 cpnmtx[6]; uint components; vec4 transmtx[64]; };\n"
   "layout(location = 0) out VertexData { centroid vec4 pos; flat int layer; vec3 normal; } vs;\n"
   "void main()\n"
   "{\n"
   "   int idx = int(posmtx.x);\n"
   "   vec4 p = vec4(dot(transmtx[idx], rawpos), dot(transmtx[idx + 1], rawpos), dot(transmtx[idx + 2], rawpos), 1.0);\n"
   "   vs.pos = p;\n"
   "   vs.layer = gl_InstanceIndex;\n"
   "   vs.normal = cpnmtx[0].xyz * float(components);\n"
   "   gl_Position = p;\n"
   "   gl_ClipDistance[0] = p.z + p.w;\n"
   "   gl_ClipDistance[1] = -p.z;\n"
   "}\n";

static struct rslc_builder *vs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_VERTEX);
   const struct rslc_type *f1 = rslc_b_vec(b, RSLC_BASE_FLOAT, 1);
   const struct rslc_type *v3 = rslc_b_vec(b, RSLC_BASE_FLOAT, 3);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *u1 = rslc_b_vec(b, RSLC_BASE_UINT, 1);
   const struct rslc_type *u4 = rslc_b_vec(b, RSLC_BASE_UINT, 4);
   const struct rslc_type *blk_t[3];
   const char *blk_n[3] = { "cpnmtx", "components", "transmtx" };
   const struct rslc_type *io_t[3];
   const char *io_n[3] = { "pos", "layer", "normal" };
   const unsigned io_i[3] = { RSLC_B_INTERP_DEFAULT, RSLC_B_INTERP_FLAT, RSLC_B_INTERP_DEFAULT };
   const unsigned io_q[3] = { RSLC_B_CENTROID, 0, 0 };
   struct rslc_var *blk_m[3];
   struct rslc_b_quals q;
   struct rslc_var *rawpos, *posmtx, *vs, *idx, *p, *clip;
   struct rslc_func *fmain;
   uint32_t a[4], d[2], e;
   int k;

   rslc_b_quals_init(&q);
   q.storage  = RSLC_B_ST_IN;
   q.location = 0;
   rawpos     = rslc_b_declare(b, &q, "rawpos", v4, 0);
   q.location = 1;
   posmtx     = rslc_b_declare(b, &q, "posmtx", u4, 0);
   blk_t[0]   = rslc_b_array(b, v4, 6);
   blk_t[1]   = u1;
   blk_t[2]   = rslc_b_array(b, v4, 64);
   rslc_b_quals_init(&q);
   q.storage  = RSLC_B_ST_UNIFORM;
   q.std140   = true;
   q.set      = 0;
   q.binding  = 1;
   rslc_b_block_q(b, &q, "VSBlock", NULL, blk_t, blk_n, 3, blk_m);
   io_t[0]    = v4;
   io_t[1]    = i1;
   io_t[2]    = v3;
   rslc_b_quals_init(&q);
   q.storage  = RSLC_B_ST_OUT;
   q.location = 0;
   vs = rslc_b_io_block(b, &q, "VertexData", "vs", io_t, io_n, io_i, io_q, 3, -1);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   a[0] = rslc_b_swizzle(b, rslc_b_ref(b, posmtx), "x");
   idx  = rslc_b_local(b, "idx", i1, rslc_b_construct(b, i1, a, 1), false);
   for (k = 0; k < 3; k++)
   {
      e    = k ? rslc_b_binary(b, RSLC_B_ADD, rslc_b_ref(b, idx), rslc_b_int(b, k))
              : rslc_b_ref(b, idx);
      d[0] = rslc_b_index(b, rslc_b_ref(b, blk_m[2]), e);
      d[1] = rslc_b_ref(b, rawpos);
      a[k] = rslc_b_builtin(b, "dot", d, 2);
   }
   a[3] = rslc_b_float(b, 1.0);
   p    = rslc_b_local(b, "p", v4, rslc_b_construct(b, v4, a, 4), false);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, vs), 0), rslc_b_ref(b, p)));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, vs), 1),
            rslc_b_ref(b, rslc_b_builtin_var(b, "gl_InstanceIndex"))));
   e    = rslc_b_swizzle(b, rslc_b_index(b, rslc_b_ref(b, blk_m[0]), rslc_b_int(b, 0)), "xyz");
   a[0] = rslc_b_ref(b, blk_m[1]);
   e    = rslc_b_binary(b, RSLC_B_MUL, e, rslc_b_construct(b, f1, a, 1));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, vs), 2), e));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_Position")),
            rslc_b_ref(b, p)));
   clip = rslc_b_builtin_var(b, "gl_ClipDistance");
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_index(b, rslc_b_ref(b, clip), rslc_b_int(b, 0)),
            rslc_b_binary(b, RSLC_B_ADD, rslc_b_swizzle(b, rslc_b_ref(b, p), "z"),
               rslc_b_swizzle(b, rslc_b_ref(b, p), "w"))));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_index(b, rslc_b_ref(b, clip), rslc_b_int(b, 1)),
            rslc_b_unary(b, RSLC_B_NEG, rslc_b_swizzle(b, rslc_b_ref(b, p), "z"))));
   rslc_b_func_end(b);
   return b;
}

/* ---- geometry: primitives, an in block array, gl_in, layers ---- */

static const char gs_glsl[] =
   "#version 450\n"
   "layout(triangles) in;\n"
   "layout(triangle_strip, max_vertices = 6) out;\n"
   "layout(location = 0) in VertexData { centroid vec4 pos; flat int layer; vec3 normal; } vs[];\n"
   "layout(location = 0) out VertexData { centroid vec4 pos; flat int layer; vec3 normal; } ps;\n"
   "void main()\n"
   "{\n"
   "   for (int eye = 0; eye < 2; eye++)\n"
   "   {\n"
   "      for (int i = 0; i < 3; i++)\n"
   "      {\n"
   "         ps.pos = vs[i].pos;\n"
   "         ps.layer = eye;\n"
   "         ps.normal = vs[i].normal;\n"
   "         gl_Position = gl_in[i].gl_Position + vec4(float(eye) * 0.1, 0.0, 0.0, 0.0);\n"
   "         gl_Layer = eye;\n"
   "         EmitVertex();\n"
   "      }\n"
   "      EndPrimitive();\n"
   "   }\n"
   "}\n";

static struct rslc_builder *gs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_GEOMETRY);
   const struct rslc_type *f1 = rslc_b_vec(b, RSLC_BASE_FLOAT, 1);
   const struct rslc_type *v3 = rslc_b_vec(b, RSLC_BASE_FLOAT, 3);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *io_t[3];
   const char *io_n[3] = { "pos", "layer", "normal" };
   const unsigned io_i[3] = { RSLC_B_INTERP_DEFAULT, RSLC_B_INTERP_FLAT, RSLC_B_INTERP_DEFAULT };
   const unsigned io_q[3] = { RSLC_B_CENTROID, 0, 0 };
   struct rslc_b_quals q;
   struct rslc_var *vs, *ps, *eye, *i, *glin;
   struct rslc_func *fmain;
   uint32_t a[4], e, init, init2, body, body2;

   rslc_b_quals_init(&q);
   q.storage      = RSLC_B_ST_IN;
   q.prim         = RSLC_B_PRIM_TRIANGLES;
   rslc_b_layout(b, &q);
   rslc_b_quals_init(&q);
   q.storage      = RSLC_B_ST_OUT;
   q.prim         = RSLC_B_PRIM_TRIANGLE_STRIP;
   q.max_vertices = 6;
   rslc_b_layout(b, &q);
   io_t[0]        = v4;
   io_t[1]        = i1;
   io_t[2]        = v3;
   rslc_b_quals_init(&q);
   q.storage      = RSLC_B_ST_IN;
   q.location     = 0;
   vs = rslc_b_io_block(b, &q, "VertexData", "vs", io_t, io_n, io_i, io_q, 3, 0);
   q.storage      = RSLC_B_ST_OUT;
   ps = rslc_b_io_block(b, &q, "VertexData", "ps", io_t, io_n, io_i, io_q, 3, -1);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   init = rslc_b_decl_stmt(b, "eye", i1, rslc_b_int(b, 0), &eye);
   rslc_b_begin_block(b);
   init2 = rslc_b_decl_stmt(b, "i", i1, rslc_b_int(b, 0), &i);
   rslc_b_begin_block(b);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, ps), 0),
            rslc_b_member(b, rslc_b_index(b, rslc_b_ref(b, vs), rslc_b_ref(b, i)), 0)));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, ps), 1), rslc_b_ref(b, eye)));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_member(b, rslc_b_ref(b, ps), 2),
            rslc_b_member(b, rslc_b_index(b, rslc_b_ref(b, vs), rslc_b_ref(b, i)), 2)));
   glin = rslc_b_builtin_var(b, "gl_in");
   a[0] = rslc_b_ref(b, eye);
   a[0] = rslc_b_binary(b, RSLC_B_MUL, rslc_b_construct(b, f1, a, 1), rslc_b_float(b, 0.1));
   a[1] = rslc_b_float(b, 0.0);
   a[2] = rslc_b_float(b, 0.0);
   a[3] = rslc_b_float(b, 0.0);
   e    = rslc_b_binary(b, RSLC_B_ADD,
         rslc_b_member(b, rslc_b_index(b, rslc_b_ref(b, glin), rslc_b_ref(b, i)), 0),
         rslc_b_construct(b, v4, a, 4));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_Position")), e));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_Layer")),
            rslc_b_ref(b, eye)));
   E(rslc_b_builtin(b, "EmitVertex", NULL, 0));
   body2 = rslc_b_end_block(b);
   S(rslc_b_for(b, init2, rslc_b_binary(b, RSLC_B_LT, rslc_b_ref(b, i), rslc_b_int(b, 3)),
            rslc_b_unary(b, RSLC_B_POSTINC, rslc_b_ref(b, i)), body2));
   E(rslc_b_builtin(b, "EndPrimitive", NULL, 0));
   body = rslc_b_end_block(b);
   S(rslc_b_for(b, init, rslc_b_binary(b, RSLC_B_LT, rslc_b_ref(b, eye), rslc_b_int(b, 2)),
            rslc_b_unary(b, RSLC_B_POSTINC, rslc_b_ref(b, eye)), body));
   rslc_b_func_end(b);
   return b;
}

/* ---- fragment: early tests, an unnamed in block, dual source, the
 * extended samplers, a storage block and its atomics ---- */

static const char fs_glsl[] =
   "#version 450\n"
   "layout(early_fragment_tests) in;\n"
   "layout(location = 0) in VertexData { centroid vec4 pos; flat int layer; vec3 normal; };\n"
   "layout(location = 0, index = 0) out vec4 ocol0;\n"
   "layout(location = 0, index = 1) out vec4 ocol1;\n"
   "layout(std140, set = 0, binding = 0) uniform PSBlock { ivec4 color[4]; uint genmode; };\n"
   "layout(set = 1, binding = 0) uniform sampler2DArray samp[8];\n"
   "layout(set = 1, binding = 8) uniform sampler2DMSArray msaa;\n"
   "layout(set = 2, binding = 0) uniform usamplerBuffer texel;\n"
   "layout(std430, set = 2, binding = 1) coherent buffer BBox { int bbox_data[4]; };\n"
   "void main()\n"
   "{\n"
   "   vec4 c = texture(samp[1], vec3(pos.xy, float(layer)));\n"
   "   uint t = texelFetch(texel, int(gl_FragCoord.x)).r;\n"
   "   vec4 m = texelFetch(msaa, ivec3(ivec2(gl_FragCoord.xy), 0), gl_SampleID);\n"
   "   if (!gl_HelperInvocation)\n"
   "   {\n"
   "      atomicMin(bbox_data[0], int(gl_FragCoord.x));\n"
   "      atomicMax(bbox_data[1], int(gl_FragCoord.y));\n"
   "   }\n"
   "   ocol0 = c + m * float(bitfieldExtract(t, 0, 8)) + vec4(color[0]) / 255.0;\n"
   "   ocol1 = vec4(normal, float(genmode));\n"
   "   gl_FragDepth = pos.z;\n"
   "}\n";

static struct rslc_builder *fs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_FRAGMENT);
   const struct rslc_type *f1 = rslc_b_vec(b, RSLC_BASE_FLOAT, 1);
   const struct rslc_type *v3 = rslc_b_vec(b, RSLC_BASE_FLOAT, 3);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *i2 = rslc_b_vec(b, RSLC_BASE_INT, 2);
   const struct rslc_type *i3 = rslc_b_vec(b, RSLC_BASE_INT, 3);
   const struct rslc_type *i4 = rslc_b_vec(b, RSLC_BASE_INT, 4);
   const struct rslc_type *u1 = rslc_b_vec(b, RSLC_BASE_UINT, 1);
   const struct rslc_type *io_t[3];
   const char *io_n[3] = { "pos", "layer", "normal" };
   const unsigned io_i[3] = { RSLC_B_INTERP_DEFAULT, RSLC_B_INTERP_FLAT, RSLC_B_INTERP_DEFAULT };
   const unsigned io_q[3] = { RSLC_B_CENTROID, 0, 0 };
   const struct rslc_type *ps_t[2];
   const char *ps_n[2] = { "color", "genmode" };
   const struct rslc_type *bb_t[1];
   const char *bb_n[1] = { "bbox_data" };
   struct rslc_var *ps_m[2], *bb_m[1];
   struct rslc_b_quals q;
   struct rslc_var *vin, *ocol0, *ocol1, *samp, *msaa, *texel, *c, *t, *m, *fc;
   struct rslc_func *fmain;
   uint32_t a[3], e, then;

   rslc_b_quals_init(&q);
   q.storage    = RSLC_B_ST_IN;
   q.early_fragment_tests = true;
   rslc_b_layout(b, &q);
   io_t[0]      = v4;
   io_t[1]      = i1;
   io_t[2]      = v3;
   rslc_b_quals_init(&q);
   q.storage    = RSLC_B_ST_IN;
   q.location   = 0;
   vin = rslc_b_io_block(b, &q, "VertexData", NULL, io_t, io_n, io_i, io_q, 3, -1);
   rslc_b_quals_init(&q);
   q.storage    = RSLC_B_ST_OUT;
   q.location   = 0;
   ocol0        = rslc_b_declare(b, &q, "ocol0", v4, 0);
   q.index      = 1;
   ocol1        = rslc_b_declare(b, &q, "ocol1", v4, 0);
   ps_t[0]      = rslc_b_array(b, i4, 4);
   ps_t[1]      = u1;
   rslc_b_quals_init(&q);
   q.storage    = RSLC_B_ST_UNIFORM;
   q.std140     = true;
   q.set        = 0;
   q.binding    = 0;
   rslc_b_block_q(b, &q, "PSBlock", NULL, ps_t, ps_n, 2, ps_m);
   q.std140     = false;
   q.set        = 1;
   samp  = rslc_b_declare(b, &q, "samp", rslc_b_array(b,
            rslc_b_sampler_type(b, RSLC_B_SAMPLER_2D_ARRAY, RSLC_BASE_FLOAT), 8), 0);
   q.binding    = 8;
   msaa  = rslc_b_declare(b, &q, "msaa",
         rslc_b_sampler_type(b, RSLC_B_SAMPLER_2D_MS_ARRAY, RSLC_BASE_FLOAT), 0);
   q.set        = 2;
   q.binding    = 0;
   texel = rslc_b_declare(b, &q, "texel",
         rslc_b_sampler_type(b, RSLC_B_SAMPLER_BUFFER, RSLC_BASE_UINT), 0);
   bb_t[0]      = rslc_b_array(b, i1, 4);
   q.storage    = RSLC_B_ST_BUFFER;
   q.binding    = 1;
   q.access     = RSLC_B_COHERENT;
   rslc_b_block_q(b, &q, "BBox", NULL, bb_t, bb_n, 1, bb_m);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   fc   = rslc_b_builtin_var(b, "gl_FragCoord");
   a[0] = rslc_b_member(b, rslc_b_ref(b, vin), 1);
   a[1] = rslc_b_construct(b, f1, a, 1);
   a[0] = rslc_b_swizzle(b, rslc_b_member(b, rslc_b_ref(b, vin), 0), "xy");
   a[1] = rslc_b_construct(b, v3, a, 2);
   a[0] = rslc_b_index(b, rslc_b_ref(b, samp), rslc_b_int(b, 1));
   c    = rslc_b_local(b, "c", v4, rslc_b_builtin(b, "texture", a, 2), false);
   a[1] = rslc_b_swizzle(b, rslc_b_ref(b, fc), "x");
   a[1] = rslc_b_construct(b, i1, a + 1, 1);
   a[0] = rslc_b_ref(b, texel);
   t    = rslc_b_local(b, "t", u1,
         rslc_b_swizzle(b, rslc_b_builtin(b, "texelFetch", a, 2), "r"), false);
   a[0] = rslc_b_swizzle(b, rslc_b_ref(b, fc), "xy");
   a[0] = rslc_b_construct(b, i2, a, 1);
   a[1] = rslc_b_int(b, 0);
   a[1] = rslc_b_construct(b, i3, a, 2);
   a[0] = rslc_b_ref(b, msaa);
   a[2] = rslc_b_ref(b, rslc_b_builtin_var(b, "gl_SampleID"));
   m    = rslc_b_local(b, "m", v4, rslc_b_builtin(b, "texelFetch", a, 3), false);

   rslc_b_begin_block(b);
   a[0] = rslc_b_index(b, rslc_b_ref(b, bb_m[0]), rslc_b_int(b, 0));
   a[1] = rslc_b_swizzle(b, rslc_b_ref(b, fc), "x");
   a[1] = rslc_b_construct(b, i1, a + 1, 1);
   E(rslc_b_builtin(b, "atomicMin", a, 2));
   a[0] = rslc_b_index(b, rslc_b_ref(b, bb_m[0]), rslc_b_int(b, 1));
   a[1] = rslc_b_swizzle(b, rslc_b_ref(b, fc), "y");
   a[1] = rslc_b_construct(b, i1, a + 1, 1);
   E(rslc_b_builtin(b, "atomicMax", a, 2));
   then = rslc_b_end_block(b);
   S(rslc_b_if(b, rslc_b_unary(b, RSLC_B_NOT,
               rslc_b_ref(b, rslc_b_builtin_var(b, "gl_HelperInvocation"))), then, 0));

   a[0] = rslc_b_ref(b, t);
   a[1] = rslc_b_int(b, 0);
   a[2] = rslc_b_int(b, 8);
   a[0] = rslc_b_builtin(b, "bitfieldExtract", a, 3);
   e    = rslc_b_binary(b, RSLC_B_MUL, rslc_b_ref(b, m), rslc_b_construct(b, f1, a, 1));
   e    = rslc_b_binary(b, RSLC_B_ADD, rslc_b_ref(b, c), e);
   a[0] = rslc_b_index(b, rslc_b_ref(b, ps_m[0]), rslc_b_int(b, 0));
   a[0] = rslc_b_binary(b, RSLC_B_DIV, rslc_b_construct(b, v4, a, 1), rslc_b_float(b, 255.0));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, ocol0), rslc_b_binary(b, RSLC_B_ADD, e, a[0])));
   a[1] = rslc_b_ref(b, ps_m[1]);
   a[1] = rslc_b_construct(b, f1, a + 1, 1);
   a[0] = rslc_b_member(b, rslc_b_ref(b, vin), 2);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, ocol1), rslc_b_construct(b, v4, a, 2)));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_FragDepth")),
            rslc_b_swizzle(b, rslc_b_member(b, rslc_b_ref(b, vin), 0), "z")));
   rslc_b_func_end(b);
   return b;
}

/* ---- fragment: a subpass input ---- */

static const char sub_glsl[] =
   "#version 450\n"
   "layout(location = 0) in vec4 col;\n"
   "layout(location = 0) out vec4 ocol0;\n"
   "layout(set = 0, binding = 3, input_attachment_index = 0) uniform subpassInput fbfetch;\n"
   "void main()\n"
   "{\n"
   "   ocol0 = col * subpassLoad(fbfetch);\n"
   "}\n";

static struct rslc_builder *sub_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_FRAGMENT);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   struct rslc_b_quals q;
   struct rslc_var *col, *ocol0, *fb;
   struct rslc_func *fmain;
   uint32_t a[1];

   rslc_b_quals_init(&q);
   q.storage  = RSLC_B_ST_IN;
   q.location = 0;
   col        = rslc_b_declare(b, &q, "col", v4, 0);
   q.storage  = RSLC_B_ST_OUT;
   ocol0      = rslc_b_declare(b, &q, "ocol0", v4, 0);
   rslc_b_quals_init(&q);
   q.storage  = RSLC_B_ST_UNIFORM;
   q.set      = 0;
   q.binding  = 3;
   q.input_attachment = 0;
   fb = rslc_b_declare(b, &q, "fbfetch",
         rslc_b_sampler_type(b, RSLC_B_SUBPASS_INPUT, RSLC_BASE_FLOAT), 0);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   a[0] = rslc_b_ref(b, fb);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, ocol0), rslc_b_binary(b, RSLC_B_MUL,
               rslc_b_ref(b, col), rslc_b_builtin(b, "subpassLoad", a, 1))));
   rslc_b_func_end(b);
   return b;
}

/* ---- compute: a local size, a buffer sampler, a storage image, an
 * unsized storage block and shared memory ---- */

static const char cs_glsl[] =
   "#version 450\n"
   "layout(local_size_x = 8, local_size_y = 8) in;\n"
   "layout(set = 0, binding = 0) uniform usamplerBuffer s_input_buffer;\n"
   "layout(rgba8, set = 1, binding = 0) writeonly uniform image2DArray output_image;\n"
   "layout(std430, set = 2, binding = 0) readonly buffer Palette { uint pal[]; };\n"
   "shared uint shared_temp[64];\n"
   "void main()\n"
   "{\n"
   "   uvec2 coords = gl_GlobalInvocationID.xy;\n"
   "   uint v = texelFetch(s_input_buffer, int(coords.y * 64u + coords.x)).r;\n"
   "   shared_temp[gl_LocalInvocationID.y * 8u + gl_LocalInvocationID.x] = v ^ pal[v & 15u];\n"
   "   memoryBarrierShared();\n"
   "   barrier();\n"
   "   uint w = shared_temp[(gl_LocalInvocationID.x + 1u) & 63u];\n"
   "   imageStore(output_image, ivec3(ivec2(coords), 0), unpackUnorm4x8(w));\n"
   "}\n";

static struct rslc_builder *cs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_COMPUTE);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *i2 = rslc_b_vec(b, RSLC_BASE_INT, 2);
   const struct rslc_type *i3 = rslc_b_vec(b, RSLC_BASE_INT, 3);
   const struct rslc_type *u1 = rslc_b_vec(b, RSLC_BASE_UINT, 1);
   const struct rslc_type *u2 = rslc_b_vec(b, RSLC_BASE_UINT, 2);
   const struct rslc_type *pal_t[1];
   const char *pal_n[1] = { "pal" };
   struct rslc_var *pal_m[1];
   struct rslc_b_quals q;
   struct rslc_var *inbuf, *img, *temp, *coords, *v, *w, *gid, *lid;
   struct rslc_func *fmain;
   uint32_t a[3], e;

   rslc_b_quals_init(&q);
   q.storage       = RSLC_B_ST_IN;
   q.local_size[0] = 8;
   q.local_size[1] = 8;
   rslc_b_layout(b, &q);
   rslc_b_quals_init(&q);
   q.storage       = RSLC_B_ST_UNIFORM;
   q.set           = 0;
   q.binding       = 0;
   inbuf = rslc_b_declare(b, &q, "s_input_buffer",
         rslc_b_sampler_type(b, RSLC_B_SAMPLER_BUFFER, RSLC_BASE_UINT), 0);
   q.format        = "rgba8";
   q.access        = RSLC_B_WRITEONLY;
   q.set           = 1;
   img   = rslc_b_declare(b, &q, "output_image",
         rslc_b_sampler_type(b, RSLC_B_IMAGE_2D_ARRAY, RSLC_BASE_FLOAT), 0);
   rslc_b_quals_init(&q);
   q.storage       = RSLC_B_ST_BUFFER;
   q.access        = RSLC_B_READONLY;
   q.set           = 2;
   q.binding       = 0;
   pal_t[0]        = rslc_b_array(b, u1, 0);
   rslc_b_block_q(b, &q, "Palette", NULL, pal_t, pal_n, 1, pal_m);
   rslc_b_quals_init(&q);
   q.storage       = RSLC_B_ST_SHARED;
   temp  = rslc_b_declare(b, &q, "shared_temp", rslc_b_array(b, u1, 64), 0);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   gid    = rslc_b_builtin_var(b, "gl_GlobalInvocationID");
   lid    = rslc_b_builtin_var(b, "gl_LocalInvocationID");
   coords = rslc_b_local(b, "coords", u2, rslc_b_swizzle(b, rslc_b_ref(b, gid), "xy"), false);
   e      = rslc_b_binary(b, RSLC_B_MUL, rslc_b_swizzle(b, rslc_b_ref(b, coords), "y"),
         rslc_b_uint(b, 64));
   a[1]   = rslc_b_binary(b, RSLC_B_ADD, e, rslc_b_swizzle(b, rslc_b_ref(b, coords), "x"));
   a[1]   = rslc_b_construct(b, i1, a + 1, 1);
   a[0]   = rslc_b_ref(b, inbuf);
   v      = rslc_b_local(b, "v", u1,
         rslc_b_swizzle(b, rslc_b_builtin(b, "texelFetch", a, 2), "r"), false);
   e      = rslc_b_binary(b, RSLC_B_ADD,
         rslc_b_binary(b, RSLC_B_MUL, rslc_b_swizzle(b, rslc_b_ref(b, lid), "y"), rslc_b_uint(b, 8)),
         rslc_b_swizzle(b, rslc_b_ref(b, lid), "x"));
   a[0]   = rslc_b_index(b, rslc_b_ref(b, pal_m[0]),
         rslc_b_binary(b, RSLC_B_BAND, rslc_b_ref(b, v), rslc_b_uint(b, 15)));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_index(b, rslc_b_ref(b, temp), e),
            rslc_b_binary(b, RSLC_B_BXOR, rslc_b_ref(b, v), a[0])));
   E(rslc_b_builtin(b, "memoryBarrierShared", NULL, 0));
   E(rslc_b_builtin(b, "barrier", NULL, 0));
   e      = rslc_b_binary(b, RSLC_B_BAND,
         rslc_b_binary(b, RSLC_B_ADD, rslc_b_swizzle(b, rslc_b_ref(b, lid), "x"), rslc_b_uint(b, 1)),
         rslc_b_uint(b, 63));
   w      = rslc_b_local(b, "w", u1, rslc_b_index(b, rslc_b_ref(b, temp), e), false);
   a[0]   = rslc_b_ref(b, coords);
   a[0]   = rslc_b_construct(b, i2, a, 1);
   a[1]   = rslc_b_int(b, 0);
   a[1]   = rslc_b_construct(b, i3, a, 2);
   a[0]   = rslc_b_ref(b, img);
   a[2]   = rslc_b_ref(b, w);
   a[2]   = rslc_b_builtin(b, "unpackUnorm4x8", a + 2, 1);
   E(rslc_b_builtin(b, "imageStore", a, 3));
   rslc_b_func_end(b);
   return b;
}

static char err[256];

static void opts_for(struct rslc_opts *o, enum rslc_dst dst, unsigned version)
{
   memset(o, 0, sizeof(*o));
   o->dst     = dst;
   o->version = version;
   o->err     = err;
   o->err_cap = sizeof(err);
}

/* The parsed and the built stage, compiled for one destination, must
 * come out byte for byte the same. */
static void same(const char *name, const char *glsl, enum rslc_stage stage, build_fn build,
      enum rslc_dst dst, unsigned version)
{
   struct rslc_opts   o;
   struct rslc_result rp, rb;
   bool               okp, okb;

   memset(&rp, 0, sizeof(rp));
   memset(&rb, 0, sizeof(rb));
   opts_for(&o, dst, version);
   err[0] = '\0';
   okp    = rslc_compile(glsl, strlen(glsl), stage, &o, &rp);
   if (!okp)
      fprintf(stderr, "%s: parse: %s\n", name, err);
   err[0] = '\0';
   okb    = rslc_b_end(build(&o), &rb);
   if (!okb)
      fprintf(stderr, "%s: build: %s\n", name, err);
   CHECK(okp && okb);
   if (okp && okb)
   {
      if (dst == RSLC_DST_SPIRV || dst == RSLC_DST_DXBC)
      {
         CHECK(rp.spirv_len == rb.spirv_len
               && !memcmp(rp.spirv, rb.spirv, rp.spirv_len * sizeof(uint32_t)));
         if (rp.spirv_len != rb.spirv_len
               || memcmp(rp.spirv, rb.spirv, rp.spirv_len * sizeof(uint32_t)))
            fprintf(stderr, "%s: the words differ\n", name);
      }
      else
      {
         CHECK(rp.text_len == rb.text_len && !memcmp(rp.text, rb.text, rp.text_len));
         if (rp.text_len != rb.text_len || memcmp(rp.text, rb.text, rp.text_len))
            fprintf(stderr, "%s parsed:\n%s\n%s built:\n%s\n", name, rp.text, name, rb.text);
      }
   }
   rslc_result_free(&rp, &o);
   rslc_result_free(&rb, &o);
}

static void errors(void)
{
   struct rslc_opts     o;
   struct rslc_result   r;
   struct rslc_builder *b;
   struct rslc_b_quals  q;

   memset(&r, 0, sizeof(r));
   opts_for(&o, RSLC_DST_SPIRV, 0);

   /* a geometry stage without its primitives */
   b = rslc_b_begin(&o, RSLC_STAGE_GEOMETRY);
   rslc_b_func_begin(b, rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL));
   rslc_b_func_end(b);
   CHECK(!rslc_b_end(b, &r));
   CHECK(strstr(err, "input primitive") != NULL);

   /* an image without a format */
   b = rslc_b_begin(&o, RSLC_STAGE_COMPUTE);
   rslc_b_quals_init(&q);
   q.storage = RSLC_B_ST_UNIFORM;
   CHECK(rslc_b_declare(b, &q, "img",
            rslc_b_sampler_type(b, RSLC_B_IMAGE_2D, RSLC_BASE_FLOAT), 0) == NULL);
   CHECK(strstr(err, "format") != NULL);
   rslc_b_abort(b);

   /* an unknown image format */
   b = rslc_b_begin(&o, RSLC_STAGE_COMPUTE);
   rslc_b_quals_init(&q);
   q.storage = RSLC_B_ST_UNIFORM;
   q.format  = "rgb10";
   CHECK(rslc_b_declare(b, &q, "img",
            rslc_b_sampler_type(b, RSLC_B_IMAGE_2D, RSLC_BASE_FLOAT), 0) == NULL);
   rslc_b_abort(b);

   /* shared memory outside a compute stage */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   rslc_b_quals_init(&q);
   q.storage = RSLC_B_ST_SHARED;
   CHECK(rslc_b_declare(b, &q, "s", rslc_b_vec(b, RSLC_BASE_UINT, 1), 0) == NULL);
   rslc_b_abort(b);

   /* a layout with nothing for the stage */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   rslc_b_quals_init(&q);
   q.storage = RSLC_B_ST_IN;
   CHECK(!rslc_b_layout(b, &q));
   rslc_b_abort(b);

   /* gl_in before the input primitive */
   b = rslc_b_begin(&o, RSLC_STAGE_GEOMETRY);
   CHECK(rslc_b_builtin_var(b, "gl_in") == NULL);
   rslc_b_abort(b);
}

int main(void)
{
   static const struct
   {
      const char     *name;
      const char     *glsl;
      build_fn        build;
      enum rslc_stage stage;
      enum rslc_dst   dst;
      unsigned        version;
   } cases[] = {
      { "vertex spirv",   vs_glsl,  vs_build,  RSLC_STAGE_VERTEX,   RSLC_DST_SPIRV,  0 },
      { "vertex glsl",    vs_glsl,  vs_build,  RSLC_STAGE_VERTEX,   RSLC_DST_GLSL,   450 },
      { "vertex hlsl",    vs_glsl,  vs_build,  RSLC_STAGE_VERTEX,   RSLC_DST_HLSL,   50 },
      { "vertex dxbc",    vs_glsl,  vs_build,  RSLC_STAGE_VERTEX,   RSLC_DST_DXBC,   50 },
      { "geometry spirv", gs_glsl,  gs_build,  RSLC_STAGE_GEOMETRY, RSLC_DST_SPIRV,  0 },
      { "geometry glsl",  gs_glsl,  gs_build,  RSLC_STAGE_GEOMETRY, RSLC_DST_GLSL,   450 },
      { "geometry dxbc",  gs_glsl,  gs_build,  RSLC_STAGE_GEOMETRY, RSLC_DST_DXBC,   50 },
      { "fragment spirv", fs_glsl,  fs_build,  RSLC_STAGE_FRAGMENT, RSLC_DST_SPIRV,  0 },
      { "fragment glsl",  fs_glsl,  fs_build,  RSLC_STAGE_FRAGMENT, RSLC_DST_GLSL,   450 },
      { "subpass spirv",  sub_glsl, sub_build, RSLC_STAGE_FRAGMENT, RSLC_DST_SPIRV,  0 },
      { "subpass msl",    sub_glsl, sub_build, RSLC_STAGE_FRAGMENT, RSLC_DST_MSL,    20 },
      { "compute spirv",  cs_glsl,  cs_build,  RSLC_STAGE_COMPUTE,  RSLC_DST_SPIRV,  0 },
      { "compute glsl",   cs_glsl,  cs_build,  RSLC_STAGE_COMPUTE,  RSLC_DST_GLSL,   450 },
      { "compute hlsl",   cs_glsl,  cs_build,  RSLC_STAGE_COMPUTE,  RSLC_DST_HLSL,   50 },
      { "compute dxbc",   cs_glsl,  cs_build,  RSLC_STAGE_COMPUTE,  RSLC_DST_DXBC,   50 },
      { "compute msl",    cs_glsl,  cs_build,  RSLC_STAGE_COMPUTE,  RSLC_DST_MSL,    20 }
   };
   unsigned i;
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      same(cases[i].name, cases[i].glsl, cases[i].stage, cases[i].build,
            cases[i].dst, cases[i].version);
   errors();
   if (fails)
   {
      fprintf(stderr, "rslc_ext_build_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_ext_build_test: ok\n");
   return 0;
}
