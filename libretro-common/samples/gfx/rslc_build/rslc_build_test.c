/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_build_test.c).
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

/* rslc builder test.  Each stage is written twice, as GLSL and as
 * builder calls; both must compile to the same SPIR-V words and print
 * the same GLSL.  Then the builder's own errors. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static const char vs_glsl[] =
   "#version 450\n"
   "layout(location = 0) in vec4 Position;\n"
   "layout(location = 1) in vec2 TexCoord;\n"
   "layout(location = 0) out vec2 vTexCoord;\n"
   "layout(location = 1) flat out int vIndex;\n"
   "layout(set = 0, binding = 0, std140) uniform UBO { mat4 MVP; vec4 SourceSize; } global;\n"
   "layout(push_constant) uniform Push { float Scale; uint Frame; } params;\n"
   "void main()\n"
   "{\n"
   "   gl_Position = global.MVP * Position;\n"
   "   vTexCoord = TexCoord * params.Scale + global.SourceSize.zw;\n"
   "   vIndex = gl_VertexIndex + int(params.Frame & 3u);\n"
   "}\n";

#define S(x) rslc_b_stmt(b, x)
#define E(x) S(rslc_b_expr(b, x))

static struct rslc_builder *vs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_VERTEX);
   const struct rslc_type *f1 = rslc_b_vec(b, RSLC_BASE_FLOAT, 1);
   const struct rslc_type *v2 = rslc_b_vec(b, RSLC_BASE_FLOAT, 2);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *u1 = rslc_b_vec(b, RSLC_BASE_UINT, 1);
   const struct rslc_type *ubo_t[2];
   const char *ubo_n[2]  = { "MVP", "SourceSize" };
   const struct rslc_type *push_t[2];
   const char *push_n[2] = { "Scale", "Frame" };
   struct rslc_var *pos, *tc, *vtc, *vidx, *ubo, *push;
   struct rslc_func *fmain;
   uint32_t e;

   ubo_t[0]  = rslc_b_mat(b, 4, 4);
   ubo_t[1]  = v4;
   push_t[0] = f1;
   push_t[1] = u1;
   pos  = rslc_b_input(b, "Position", v4, 0, RSLC_B_INTERP_DEFAULT, 0);
   tc   = rslc_b_input(b, "TexCoord", v2, 1, RSLC_B_INTERP_DEFAULT, 0);
   vtc  = rslc_b_output(b, "vTexCoord", v2, 0, RSLC_B_INTERP_DEFAULT, 0);
   vidx = rslc_b_output(b, "vIndex", i1, 1, RSLC_B_INTERP_FLAT, 0);
   ubo  = rslc_b_block(b, false, true, "UBO", "global", 0, 0, ubo_t, ubo_n, 2, NULL);
   push = rslc_b_block(b, true, false, "Push", "params", RSLC_NO_BINDING, RSLC_NO_BINDING,
         push_t, push_n, 2, NULL);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_Position")),
            rslc_b_binary(b, RSLC_B_MUL, rslc_b_member(b, rslc_b_ref(b, ubo), 0),
               rslc_b_ref(b, pos))));
   e = rslc_b_binary(b, RSLC_B_MUL, rslc_b_ref(b, tc), rslc_b_member(b, rslc_b_ref(b, push), 0));
   e = rslc_b_binary(b, RSLC_B_ADD, e,
         rslc_b_swizzle(b, rslc_b_member(b, rslc_b_ref(b, ubo), 1), "zw"));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, vtc), e));
   e = rslc_b_binary(b, RSLC_B_BAND, rslc_b_member(b, rslc_b_ref(b, push), 1), rslc_b_uint(b, 3));
   e = rslc_b_construct(b, i1, &e, 1);
   e = rslc_b_binary(b, RSLC_B_ADD, rslc_b_ref(b, rslc_b_builtin_var(b, "gl_VertexIndex")), e);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, vidx), e));
   rslc_b_func_end(b);
   return b;
}

static const char fs_glsl[] =
   "#version 450\n"
   "layout(location = 0) in vec2 vTexCoord;\n"
   "layout(location = 1) flat in int vIndex;\n"
   "layout(location = 0) out vec4 FragColor;\n"
   "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
   "layout(set = 0, binding = 3) uniform usampler2D Lut;\n"
   "const float weights[3] = float[3](0.25, 0.5, 0.25);\n"
   "struct Tap { vec3 color; float w; };\n"
   "float luma(vec3 c, out float peak)\n"
   "{\n"
   "   peak = max(c.r, max(c.g, c.b));\n"
   "   return dot(c, vec3(0.299, 0.587, 0.114));\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   vec3 acc = vec3(0.0);\n"
   "   float peak;\n"
   "   for (int i = 0; i < 3; i++)\n"
   "   {\n"
   "      Tap t = Tap(texture(Source, vTexCoord + vec2(float(i - 1), 0.0) * 0.001).rgb, weights[i]);\n"
   "      acc += t.color * t.w;\n"
   "   }\n"
   "   float l = luma(acc, peak);\n"
   "   uint bits = texelFetch(Lut, ivec2(vIndex, 0), 0).r;\n"
   "   int mode = int(bitfieldExtract(bits, 4, 3));\n"
   "   switch (mode)\n"
   "   {\n"
   "   case 0:\n"
   "      acc = vec3(l);\n"
   "      break;\n"
   "   case 1:\n"
   "      acc.gb = acc.bg;\n"
   "      break;\n"
   "   default:\n"
   "      acc = mix(acc, vec3(peak), 0.5);\n"
   "      break;\n"
   "   }\n"
   "   while (l > 1.0)\n"
   "      l *= 0.5;\n"
   "   if (l < 0.01 && !(vIndex > 2))\n"
   "      discard;\n"
   "   else\n"
   "      acc = -acc * (l > 0.5 ? 2.0 : 1.0);\n"
   "   FragColor = vec4(acc, 1.0);\n"
   "}\n";

static struct rslc_builder *fs_build(const struct rslc_opts *opts)
{
   struct rslc_builder *b = rslc_b_begin(opts, RSLC_STAGE_FRAGMENT);
   const struct rslc_type *f1 = rslc_b_vec(b, RSLC_BASE_FLOAT, 1);
   const struct rslc_type *v2 = rslc_b_vec(b, RSLC_BASE_FLOAT, 2);
   const struct rslc_type *v3 = rslc_b_vec(b, RSLC_BASE_FLOAT, 3);
   const struct rslc_type *v4 = rslc_b_vec(b, RSLC_BASE_FLOAT, 4);
   const struct rslc_type *i1 = rslc_b_vec(b, RSLC_BASE_INT, 1);
   const struct rslc_type *i2 = rslc_b_vec(b, RSLC_BASE_INT, 2);
   const struct rslc_type *u1 = rslc_b_vec(b, RSLC_BASE_UINT, 1);
   const struct rslc_type *f3a = rslc_b_array(b, f1, 3);
   const struct rslc_type *tap_t[2];
   const char *tap_n[2] = { "color", "w" };
   const struct rslc_type *tap;
   const struct rslc_type *luma_t[2];
   const unsigned luma_d[2] = { RSLC_B_IN, RSLC_B_OUT };
   const char *luma_n[2] = { "c", "peak" };
   struct rslc_var *vtc, *vidx, *frag, *src, *lut, *weights, *acc, *peak, *i, *t, *l, *bits, *mode;
   struct rslc_func *fluma, *fmain;
   uint32_t a[4], e, init, body, cond;

   tap_t[0]  = v3;
   tap_t[1]  = f1;
   luma_t[0] = v3;
   luma_t[1] = f1;
   vtc  = rslc_b_input(b, "vTexCoord", v2, 0, RSLC_B_INTERP_DEFAULT, 0);
   vidx = rslc_b_input(b, "vIndex", i1, 1, RSLC_B_INTERP_FLAT, 0);
   frag = rslc_b_output(b, "FragColor", v4, 0, RSLC_B_INTERP_DEFAULT, 0);
   src  = rslc_b_sampler(b, "Source", rslc_b_sampler2d(b, RSLC_BASE_FLOAT), 0, 2);
   lut  = rslc_b_sampler(b, "Lut", rslc_b_sampler2d(b, RSLC_BASE_UINT), 0, 3);
   a[0] = rslc_b_float(b, 0.25);
   a[1] = rslc_b_float(b, 0.5);
   a[2] = rslc_b_float(b, 0.25);
   weights = rslc_b_global(b, "weights", f3a, rslc_b_construct(b, f3a, a, 3), true);
   tap  = rslc_b_struct(b, "Tap", tap_t, tap_n, 2);

   fluma = rslc_b_func(b, "luma", f1, 2, luma_t, luma_d, luma_n);
   rslc_b_func_begin(b, fluma);
   {
      struct rslc_var *c  = rslc_b_param(fluma, 0);
      struct rslc_var *pk = rslc_b_param(fluma, 1);
      a[0] = rslc_b_swizzle(b, rslc_b_ref(b, c), "g");
      a[1] = rslc_b_swizzle(b, rslc_b_ref(b, c), "b");
      a[1] = rslc_b_builtin(b, "max", a, 2);
      a[0] = rslc_b_swizzle(b, rslc_b_ref(b, c), "r");
      E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, pk), rslc_b_builtin(b, "max", a, 2)));
      a[0] = rslc_b_float(b, 0.299);
      a[1] = rslc_b_float(b, 0.587);
      a[2] = rslc_b_float(b, 0.114);
      a[1] = rslc_b_construct(b, v3, a, 3);
      a[0] = rslc_b_ref(b, c);
      S(rslc_b_return(b, rslc_b_builtin(b, "dot", a, 2)));
   }
   rslc_b_func_end(b);

   fmain = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, fmain);
   a[0] = rslc_b_float(b, 0.0);
   acc  = rslc_b_local(b, "acc", v3, rslc_b_construct(b, v3, a, 1), false);
   peak = rslc_b_local(b, "peak", f1, 0, false);

   init = rslc_b_decl_stmt(b, "i", i1, rslc_b_int(b, 0), &i);
   rslc_b_begin_block(b);
   a[0] = rslc_b_binary(b, RSLC_B_SUB, rslc_b_ref(b, i), rslc_b_int(b, 1));
   a[0] = rslc_b_construct(b, f1, a, 1);
   a[1] = rslc_b_float(b, 0.0);
   e    = rslc_b_construct(b, v2, a, 2);
   e    = rslc_b_binary(b, RSLC_B_MUL, e, rslc_b_float(b, 0.001));
   a[0] = rslc_b_ref(b, src);
   a[1] = rslc_b_binary(b, RSLC_B_ADD, rslc_b_ref(b, vtc), e);
   a[0] = rslc_b_swizzle(b, rslc_b_builtin(b, "texture", a, 2), "rgb");
   a[1] = rslc_b_index(b, rslc_b_ref(b, weights), rslc_b_ref(b, i));
   t    = rslc_b_local(b, "t", tap, rslc_b_construct(b, tap, a, 2), false);
   E(rslc_b_assign(b, RSLC_B_ADD, rslc_b_ref(b, acc),
            rslc_b_binary(b, RSLC_B_MUL, rslc_b_member(b, rslc_b_ref(b, t), 0),
               rslc_b_member(b, rslc_b_ref(b, t), 1))));
   body = rslc_b_end_block(b);
   S(rslc_b_for(b, init, rslc_b_binary(b, RSLC_B_LT, rslc_b_ref(b, i), rslc_b_int(b, 3)),
            rslc_b_unary(b, RSLC_B_POSTINC, rslc_b_ref(b, i)), body));

   a[0] = rslc_b_ref(b, acc);
   a[1] = rslc_b_ref(b, peak);
   l    = rslc_b_local(b, "l", f1, rslc_b_call(b, fluma, a, 2), false);

   a[1] = rslc_b_ref(b, vidx);
   a[2] = rslc_b_int(b, 0);
   a[1] = rslc_b_construct(b, i2, a + 1, 2);
   a[0] = rslc_b_ref(b, lut);
   a[2] = rslc_b_int(b, 0);
   bits = rslc_b_local(b, "bits", u1,
         rslc_b_swizzle(b, rslc_b_builtin(b, "texelFetch", a, 3), "r"), false);
   a[0] = rslc_b_ref(b, bits);
   a[1] = rslc_b_int(b, 4);
   a[2] = rslc_b_int(b, 3);
   a[0] = rslc_b_builtin(b, "bitfieldExtract", a, 3);
   mode = rslc_b_local(b, "mode", i1, rslc_b_construct(b, i1, a, 1), false);

   rslc_b_begin_block(b);
   S(rslc_b_case(b, 0));
   a[0] = rslc_b_ref(b, l);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, acc), rslc_b_construct(b, v3, a, 1)));
   S(rslc_b_break(b));
   S(rslc_b_case(b, 1));
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_swizzle(b, rslc_b_ref(b, acc), "gb"),
            rslc_b_swizzle(b, rslc_b_ref(b, acc), "bg")));
   S(rslc_b_break(b));
   S(rslc_b_default(b));
   a[0] = rslc_b_ref(b, peak);
   a[1] = rslc_b_construct(b, v3, a, 1);
   a[0] = rslc_b_ref(b, acc);
   a[2] = rslc_b_float(b, 0.5);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, acc), rslc_b_builtin(b, "mix", a, 3)));
   S(rslc_b_break(b));
   S(rslc_b_switch(b, rslc_b_ref(b, mode), rslc_b_end_block(b)));

   S(rslc_b_while(b, rslc_b_binary(b, RSLC_B_GT, rslc_b_ref(b, l), rslc_b_float(b, 1.0)),
            rslc_b_expr(b, rslc_b_assign(b, RSLC_B_MUL, rslc_b_ref(b, l),
                  rslc_b_float(b, 0.5)))));

   cond = rslc_b_binary(b, RSLC_B_AND,
         rslc_b_binary(b, RSLC_B_LT, rslc_b_ref(b, l), rslc_b_float(b, 0.01)),
         rslc_b_unary(b, RSLC_B_NOT,
            rslc_b_binary(b, RSLC_B_GT, rslc_b_ref(b, vidx), rslc_b_int(b, 2))));
   e = rslc_b_select(b, rslc_b_binary(b, RSLC_B_GT, rslc_b_ref(b, l), rslc_b_float(b, 0.5)),
         rslc_b_float(b, 2.0), rslc_b_float(b, 1.0));
   e = rslc_b_binary(b, RSLC_B_MUL, rslc_b_unary(b, RSLC_B_NEG, rslc_b_ref(b, acc)), e);
   S(rslc_b_if(b, cond, rslc_b_discard(b),
            rslc_b_expr(b, rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, acc), e))));

   a[0] = rslc_b_ref(b, acc);
   a[1] = rslc_b_float(b, 1.0);
   E(rslc_b_assign(b, RSLC_B_SET, rslc_b_ref(b, frag), rslc_b_construct(b, v4, a, 2)));
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
      if (dst == RSLC_DST_SPIRV)
      {
         CHECK(rp.spirv_len == rb.spirv_len);
         CHECK(rp.spirv_len == rb.spirv_len
               && !memcmp(rp.spirv, rb.spirv, rp.spirv_len * sizeof(uint32_t)));
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

/* With a caller arena, all a build takes beyond it comes from the
 * caller's allocator, so a compile worker can keep the heap still. */
static int  allocs, live;
static void *count_alloc(void *ud, size_t len)
{
   (void)ud;
   allocs++;
   live++;
   return malloc(len);
}

static void count_free(void *ud, void *ptr)
{
   (void)ud;
   if (ptr)
      live--;
   free(ptr);
}

static void allocator(void)
{
   static uint64_t    block[(256u * 1024u) / sizeof(uint64_t)];
   struct rslc_opts   o;
   struct rslc_result r;
   opts_for(&o, RSLC_DST_SPIRV, 0);
   o.alloc     = count_alloc;
   o.free_fn   = count_free;
   o.arena     = block;
   o.arena_len = sizeof(block);
   memset(&r, 0, sizeof(r));
   rslc_b_abort(rslc_b_begin(&o, RSLC_STAGE_FRAGMENT));
   CHECK(allocs == 1 && live == 0);   /* the builder */
   CHECK(rslc_b_end(fs_build(&o), &r));
   CHECK(allocs == 3 && live == 1);   /* and the result's blob */
   rslc_result_free(&r, &o);
   CHECK(live == 0);
}

static void errors(void)
{
   struct rslc_opts     o;
   struct rslc_result   r;
   struct rslc_builder *b;
   struct rslc_func    *f;
   uint32_t             e;

   memset(&r, 0, sizeof(r));
   opts_for(&o, RSLC_DST_SPIRV, 0);

   /* no main */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   CHECK(!rslc_b_end(b, &r));
   CHECK(strstr(err, "no main") != NULL);

   /* a type error poisons the rest: every later call returns 0 */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   f = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, f);
   e = rslc_b_binary(b, RSLC_B_ADD, rslc_b_bool(b, true), rslc_b_float(b, 1.0));
   CHECK(e == 0);
   CHECK(rslc_b_int(b, 1) == 0);
   CHECK(!rslc_b_end(b, &r));
   CHECK(strstr(err, "non-numeric") != NULL);

   /* discard only in a fragment stage */
   b = rslc_b_begin(&o, RSLC_STAGE_VERTEX);
   f = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, f);
   CHECK(rslc_b_discard(b) == 0);
   CHECK(!rslc_b_end(b, &r));

   /* an unclosed block */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   f = rslc_b_func(b, "main", rslc_b_void(b), 0, NULL, NULL, NULL);
   rslc_b_func_begin(b, f);
   rslc_b_begin_block(b);
   CHECK(!rslc_b_func_end(b));
   CHECK(!rslc_b_end(b, &r));

   /* a builtin of another stage */
   b = rslc_b_begin(&o, RSLC_STAGE_FRAGMENT);
   CHECK(rslc_b_builtin_var(b, "gl_Position") == NULL);
   rslc_b_abort(b);
}

int main(void)
{
   same("vertex spirv", vs_glsl, RSLC_STAGE_VERTEX, vs_build, RSLC_DST_SPIRV, 0);
   same("fragment spirv", fs_glsl, RSLC_STAGE_FRAGMENT, fs_build, RSLC_DST_SPIRV, 0);
   same("vertex glsl", vs_glsl, RSLC_STAGE_VERTEX, vs_build, RSLC_DST_GLSL, 450);
   same("fragment glsl", fs_glsl, RSLC_STAGE_FRAGMENT, fs_build, RSLC_DST_GLSL, 450);
   same("vertex hlsl", vs_glsl, RSLC_STAGE_VERTEX, vs_build, RSLC_DST_HLSL, 50);
   same("fragment hlsl", fs_glsl, RSLC_STAGE_FRAGMENT, fs_build, RSLC_DST_HLSL, 50);
   errors();
   allocator();
   if (fails)
   {
      fprintf(stderr, "rslc_build_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_build_test: ok\n");
   return 0;
}
