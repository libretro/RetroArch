/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_test.c).
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

/* rslc unit test.  Feeds strings only; the library never opens a
 * file.  Covers the ABI contract and the shared primitives: the arena
 * (chunking, caller block, cap), the intern table, the print buffer,
 * the error sink, and blob validation. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gfx/shaders/rslc.h>
#include "rslc_share.h"

static int fails;

#define CHECK(cond) do { \
   if (!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++; \
   } \
} while (0)

/* Counting allocator: proves every allocation is returned and that
 * the cap is honoured through opts, not through malloc. */
static size_t alloc_live;
static size_t alloc_calls;
static long   alloc_fail_at = -1; /* the call that is refused */

static void *count_alloc(void *ud, size_t len)
{
   size_t *p;
   (void)ud;
   if (alloc_fail_at >= 0 && (long)alloc_calls >= alloc_fail_at)
   {
      alloc_calls++;
      return NULL;
   }
   p = (size_t*)malloc(len + sizeof(size_t) * 2);
   if (!p)
      return NULL;
   p[0]        = len;
   alloc_live += len;
   alloc_calls++;
   return p + 2;
}

static void count_free(void *ud, void *ptr)
{
   size_t *p;
   (void)ud;
   if (!ptr)
      return;
   p           = (size_t*)ptr - 2;
   alloc_live -= p[0];
   free(p);
}

static void opts_default(struct rslc_opts *o, char *err, size_t cap)
{
   memset(o, 0, sizeof(*o));
   o->alloc   = count_alloc;
   o->free_fn = count_free;
   o->err     = err;
   o->err_cap = cap;
}

static void test_abi_rejects(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  src[] = "#version 450\nvoid main(){}\n";
   static const char  bad[] = "void main(){}\n";
   uint32_t           words[5];

   opts_default(&o, err, sizeof(err));
   err[0] = 0;
   CHECK(rslc_compile(src, sizeof(src) - 1, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(r.spirv && r.spirv_len > 5 && r.spirv[0] == 0x07230203u);
   CHECK(r.blob && r.blob_len == ((const struct rslc_blob_hdr*)r.blob)->total_len);
   rslc_result_free(&r, &o);
   CHECK(alloc_live == 0);

   err[0] = 0;
   CHECK(!rslc_compile(bad, sizeof(bad) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   CHECK(!strcmp(err, "rslc: first line must be #version (line 1)"));

   err[0] = 0;
   CHECK(!rslc_compile(src, sizeof(src) - 1, (enum rslc_stage)7, &o, &r));
   CHECK(!strcmp(err, "rslc: unknown stage"));

   /* A truncated message still terminates. */
   o.err_cap = 12;
   err[0] = 0;
   CHECK(!rslc_compile(bad, sizeof(bad) - 1, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(strlen(err) == 11);
   o.err_cap = sizeof(err);

   err[0] = 0;
   CHECK(!rslc_compile(NULL, 0, RSLC_STAGE_VERTEX, &o, &r));

   words[0] = 0x07230203u; words[1] = 0x00010000u; words[2] = 0;
   words[3] = 100; words[4] = 0;
   err[0] = 0;
   CHECK(!rslc_compile_spirv(words, 5, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(!strcmp(err, "rslc: SPIR-V module without an entry point"));
   words[0] = 1;
   err[0] = 0;
   CHECK(!rslc_compile_spirv(words, 5, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(!strcmp(err, "rslc: not a SPIR-V module"));
   CHECK(alloc_live == 0);
}

static void test_arena(void)
{
   struct rslc_opts o;
   struct rslc_ctx  c;
   char             err[128];
   void            *p;
   size_t           calls;
   unsigned         i;
   static uint8_t   block[32768 + 7];

   opts_default(&o, err, sizeof(err));

   /* Chunked: a request larger than a chunk gets its own, small ones
    * share; everything comes back on close. */
   CHECK(rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));
   calls = alloc_calls;
   p = rslc_arena_alloc(&c, 3);
   CHECK(p != NULL);
   CHECK(((uintptr_t)p & (RSLC_ARENA_ALIGN - 1)) == 0);
   CHECK(rslc_arena_alloc(&c, 5) == (uint8_t*)p + RSLC_ARENA_ALIGN);
   CHECK(alloc_calls == calls);
   p = rslc_arena_alloc(&c, RSLC_ARENA_CHUNK * 2);
   CHECK(p != NULL);
   CHECK(alloc_calls == calls + 1);
   rslc_ctx_close(&c);
   CHECK(alloc_live == 0);

   /* Cap: fails closed with the message, nothing leaks. */
   o.arena_cap = RSLC_ARENA_CHUNK * 3;
   CHECK(rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));
   for (i = 0; i < 8; i++)
      p = rslc_arena_alloc(&c, RSLC_ARENA_CHUNK / 2);
   CHECK(p == NULL);
   CHECK(c.failed);
   CHECK(!strcmp(err, "rslc: arena cap exceeded"));
   rslc_ctx_close(&c);
   CHECK(alloc_live == 0);
   o.arena_cap = 0;

   /* Caller block: misaligned start is padded, no allocator call is
    * made for the arena, exhaustion fails closed. */
   o.arena     = block + 7;
   o.arena_len = sizeof(block) - 7;
   err[0] = 0;
   calls = alloc_calls;
   CHECK(rslc_ctx_init(&c, &o, RSLC_STAGE_FRAGMENT));
   CHECK(c.arena.fixed);
   p = rslc_arena_alloc(&c, 16);
   CHECK(p != NULL);
   CHECK(((uintptr_t)p & (RSLC_ARENA_ALIGN - 1)) == 0);
   CHECK((uint8_t*)p >= block && (uint8_t*)p < block + sizeof(block));
   CHECK(alloc_calls == calls);
   CHECK(rslc_arena_alloc(&c, 65536) == NULL);
   CHECK(!strcmp(err, "rslc: caller arena exhausted"));
   rslc_ctx_close(&c);
   CHECK(alloc_live == 0);
   o.arena     = NULL;
   o.arena_len = 0;

   /* Too small to hold anything. */
   o.arena     = block;
   o.arena_len = 64;
   err[0] = 0;
   CHECK(!rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));
   CHECK(!strcmp(err, "rslc: caller arena too small"));
   rslc_ctx_close(&c);
   /* Big enough to start, too small for the keyword seed. */
   o.arena_len = 4200;
   err[0] = 0;
   CHECK(!rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));
   CHECK(!strcmp(err, "rslc: caller arena exhausted"));
   rslc_ctx_close(&c);
}

static void test_intern(void)
{
   struct rslc_opts o;
   struct rslc_ctx  c;
   char             err[128];
   const char      *a;
   const char      *b;
   char             name[16];
   unsigned         i;
   uint32_t         base;

   opts_default(&o, err, sizeof(err));
   CHECK(rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));
   base = c.intern.count;   /* the seeded keywords */
   CHECK(rslc_sym(&c, "vec4", 4) != NULL);
   CHECK(rslc_sym(&c, "vec4", 4)->kw == RSLC_KW_VEC4);
   CHECK(rslc_sym(&c, "vec4x", 5)->kw == RSLC_KW_NONE);
   CHECK(c.intern.count == base + 1);
   base = c.intern.count;

   a = rslc_intern(&c, "vTexCoordXYZ", 9);
   b = rslc_intern_cstr(&c, "vTexCoord");
   CHECK(a == b);
   CHECK(!strcmp(a, "vTexCoord"));
   CHECK(rslc_intern_cstr(&c, "vTexCoord2") != a);
   CHECK(rslc_intern(&c, "", 0) == rslc_intern(&c, "", 0));

   /* Grow past the seed table several times; earlier pointers stay
    * valid and keep deduplicating. */
   for (i = 0; i < 5000; i++)
   {
      sprintf(name, "id%u", i);
      CHECK(rslc_intern_cstr(&c, name) != NULL);
   }
   CHECK(c.intern.count == base + 5003);
   CHECK(rslc_intern_cstr(&c, "vTexCoord") == a);
   CHECK(rslc_intern_cstr(&c, "id4999") == rslc_intern_cstr(&c, "id4999"));
   CHECK(c.intern.count == base + 5003);

   rslc_ctx_close(&c);
   CHECK(alloc_live == 0);
}

static void test_pbuf(void)
{
   struct rslc_opts o;
   struct rslc_ctx  c;
   struct rslc_pbuf b;
   char             err[128];
   unsigned         i;
   size_t           calls;

   opts_default(&o, err, sizeof(err));
   CHECK(rslc_ctx_init(&c, &o, RSLC_STAGE_VERTEX));

   /* A right hint is one allocation and no growth. */
   calls = alloc_calls;
   CHECK(rslc_pbuf_init(&c, &b, 64, 39));
   CHECK(b.len == 64);
   CHECK(b.cap >= 103);
   CHECK(rslc_pbuf_puts(&c, &b, "layout(location = "));
   CHECK(rslc_pbuf_putu(&c, &b, 0));
   CHECK(rslc_pbuf_puts(&c, &b, ") in vec"));
   CHECK(rslc_pbuf_putu(&c, &b, 4294967295u));
   CHECK(rslc_pbuf_putc(&c, &b, ';'));
   CHECK(rslc_pbuf_putc(&c, &b, '\0'));
   CHECK(!strcmp((const char*)b.data + 64,
            "layout(location = 0) in vec4294967295;"));
   CHECK(b.grows == 0);
   CHECK(alloc_calls == calls + 1);
   /* A wrong hint still works, and is counted. */
   for (i = 0; i < 20000; i++)
      CHECK(rslc_pbuf_puts(&c, &b, "0123456789"));
   CHECK(b.len == 64 + 39 + 200000);
   CHECK(b.grows > 0);
   CHECK(!memcmp(b.data + b.len - 10, "0123456789", 10));
   rslc_pbuf_free(&c, &b);
   CHECK(b.data == NULL);

   rslc_ctx_close(&c);
   CHECK(alloc_live == 0);
}


/* Runs the preprocessor over src and renders the token stream as one
 * line per token: "<line> <text>".  Returns false with err set when
 * preprocessing fails. */
static bool pp_dump(const char *src, char *out, size_t cap, char *err, size_t errcap)
{
   struct rslc_opts o;
   struct rslc_ctx  c;
   size_t           at = 0;
   uint32_t         i;
   bool             ok;

   opts_default(&o, err, errcap);
   err[0] = 0;
   if (!rslc_ctx_init(&c, &o, RSLC_STAGE_FRAGMENT))
      return false;
   ok = rslc_pp_run(&c, src, strlen(src));
   if (ok)
   {
      /* Nothing of the expansion machinery outlives the run. */
      if (c.scratch.used != 0)
      {
         strncpy(err, "scratch not released", errcap - 1);
         err[errcap - 1] = 0;
         ok = false;
      }
      for (i = 0; ok && i < c.ntoks && rslc_tok_at(&c, i)->kind != RSLC_TK_EOF; i++)
      {
         const struct rslc_tok *t = rslc_tok_at(&c, i);
         int n = snprintf(out + at, cap - at, "%u %.*s\n", t->line,
               (int)t->len, rslc_tok_text(&c, t));
         if (n < 0 || (size_t)n >= cap - at)
         {
            ok = false;
            break;
         }
         at += (size_t)n;
      }
   }
   out[at < cap ? at : cap - 1] = '\0';
   rslc_ctx_close(&c);
   return ok;
}

/* The token text only, space separated, for comparisons that do not
 * care about line numbers. */
static void pp_flat(const char *dump, char *out, size_t cap)
{
   size_t at = 0;
   const char *p = dump;
   while (*p && at + 1 < cap)
   {
      const char *sp = strchr(p, ' ');
      const char *nl = strchr(p, '\n');
      if (!sp || !nl || sp > nl)
         break;
      sp++;
      if (at)
         out[at++] = ' ';
      while (sp < nl && at + 1 < cap)
         out[at++] = *sp++;
      p = nl + 1;
   }
   out[at] = '\0';
}

static void test_pp(void)
{
   static char dump[65536];
   static char flat[65536];
   char        err[160];
   bool        ok;

   /* Object-like, function-like, nesting, prescan, painting. */
   ok = pp_dump(
      "#version 450\n"
      "#define A 1\n"
      "#define B A + A\n"
      "#define F(x, y) ((x) * (y) + B)\n"
      "#define G(x) F(x, A)\n"
      "#define REC REC + 1\n"
      "int v = G(B);\n"
      "int w = REC;\n"
      "int z = F((1,2), 3);\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat,
      "int v = ( ( 1 + 1 ) * ( 1 ) + 1 + 1 ) ; "
      "int w = REC + 1 ; "
      "int z = ( ( ( 1 , 2 ) ) * ( 3 ) + 1 + 1 ) ;"));

   /* A function-like name without '(' is left alone; "f()" is one
    * empty argument for a one-parameter macro. */
   ok = pp_dump(
      "#version 450\n#define F(x) [x]\n#define E() 7\nint a = F; int b = F(); int c = E();\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat, "int a = F ; int b = [ ] ; int c = 7 ;"));

   /* Conditionals: arithmetic, defined, nesting, elif, undef, inactive
    * branches with unknown directives and unbalanced tokens. */
   ok = pp_dump(
      "#version 450\n"
      "#define LEVEL 3\n"
      "#if (LEVEL * 2 > 5) && defined(LEVEL) && !defined NOPE\n"
      "int a;\n"
      "#else\n"
      "#def garbage (((\n"
      "int b;\n"
      "#endif\n"
      "#ifdef NOPE\n"
      "#if 1/0\n"
      "#endif\n"
      "#elif LEVEL == 2\n"
      "int c;\n"
      "#elif LEVEL == 3\n"
      "int d;\n"
      "#else\n"
      "int e;\n"
      "#endif\n"
      "#undef LEVEL\n"
      "#ifndef LEVEL\nint f;\n#endif\n"
      "#if UNDEFINED_NAME\nint g;\n#endif\n"
      "#if 0x10 == 16 && 010 == 8 && (1 ? 2 : 3) == 2 && (7 % 4) == 3 && (1 << 3) == 8\nint h;\n#endif\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat, "int a ; int d ; int f ; int h ;"));

   /* Line tracking: continuations, block comments spanning lines,
    * #line with a file string, __LINE__. */
   ok = pp_dump(
      "#version 450\n"
      "#define M(a) a + \\\n"
      "   a\n"
      "int a; /* two\nlines */ int b;\n"
      "#line 100 \"inc.inc\"\n"
      "int c = __LINE__;\n"
      "int d = M(__LINE__);\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   CHECK(!strcmp(dump,
      "4 int\n4 a\n4 ;\n5 int\n5 b\n5 ;\n"
      "100 int\n100 c\n100 =\n100 100\n100 ;\n"
      "101 int\n101 d\n101 =\n101 101\n101 +\n101 101\n101 ;\n"));

   /* Token paste: argument operands go in unexpanded, the pasted
    * token is one identifier, and an identifier made by pasting is
    * itself subject to expansion. */
   ok = pp_dump(
      "#version 450\n"
      "#define CAT(a, b) a##b\n"
      "#define ROW(n) level##n[0] = CAT(x, n);\n"
      "#define x1 ONE\n"
      "ROW(1) ROW(2)\n"
      "#define LVL 7\n"
      "int q = CAT(LV, L);\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat, "level1 [ 0 ] = ONE ; level2 [ 0 ] = x2 ; int q = 7 ;"));

   /* Inactive branches: glslang reads "##else" as "#else" there and
    * refuses it in an active branch. */
   ok = pp_dump("#version 450\n#ifdef NOPE\nint a;\n##else\nint b;\n#endif\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat, "int b ;"));
   CHECK(!pp_dump("#version 450\n#define NOPE\n#ifdef NOPE\nint a;\n##else\nint b;\n#endif\n",
      dump, sizeof(dump), err, sizeof(err)));

   /* Predefined macros and #extension / #pragma pass-through. */
   ok = pp_dump(
      "#version 450\n#extension GL_GOOGLE_cpp_style_line_directive : require\n"
      "#pragma parameter X \"x\" 1.0 0.0 1.0\n"
      "#ifdef VULKAN\nint v = VULKAN + __VERSION__ + GL_core_profile;\n#endif\n",
      dump, sizeof(dump), err, sizeof(err));
   CHECK(ok);
   pp_flat(dump, flat, sizeof(flat));
   CHECK(!strcmp(flat, "int v = 100 + 450 + 1 ;"));

   /* Refusals. */
   CHECK(!pp_dump("int a;\n#version 450\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(!strcmp(err, "rslc: first line must be #version (line 1)"));
   CHECK(!pp_dump("#version 450\n#include \"x.inc\"\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(strstr(err, "#include") != NULL);
   CHECK(!pp_dump("#version 450\n#define S(x) #x\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(strstr(err, "not supported") != NULL);
   CHECK(!pp_dump("#version 450\n#define P(x) x##\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(strstr(err, "edge") != NULL);
   CHECK(!pp_dump("#version 450\n#if 1\nint a;\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(!strcmp(err, "rslc: unterminated #if (line 4)"));
   CHECK(!pp_dump("#version 450\n#error custom message here\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(!strcmp(err, "rslc: #error custom message here (line 2)"));
   CHECK(!pp_dump("#version 450\n#frobnicate\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(!strcmp(err, "rslc: unknown directive #frobnicate (line 2)"));
   CHECK(!pp_dump("#version 450\n#define F(a,b) a\nint x = F(1);\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(strstr(err, "wrong number of arguments") != NULL);
   CHECK(!pp_dump("#version 100\nvoid main(){}\n", dump, sizeof(dump), err, sizeof(err)));
   CHECK(!strcmp(err, "rslc: unsupported #version (line 1)"));
   CHECK(alloc_live == 0);
}


/* ---- IR dump ---- */

static const char *ir_type_name(const struct rslc_type *t, char *buf, size_t cap)
{
   static const char *const vn[5][5] = {
      { "void", "?", "?", "?", "?" },
      { "bool", "bool", "bvec2", "bvec3", "bvec4" },
      { "int", "int", "ivec2", "ivec3", "ivec4" },
      { "uint", "uint", "uvec2", "uvec3", "uvec4" },
      { "float", "float", "vec2", "vec3", "vec4" }
   };
   if (!t)
      return "-";
   switch (t->kind)
   {
      case RSLC_TY_VOID:      return "void";
      case RSLC_TY_SCALAR:    return vn[t->base][1];
      case RSLC_TY_VEC:       return vn[t->base][t->rows];
      case RSLC_TY_MAT:
         snprintf(buf, cap, "mat%ux%u", t->cols, t->rows);
         return buf;
      case RSLC_TY_SAMPLER2D: return "sampler2D";
      case RSLC_TY_ARRAY:
      {
         char inner[64];
         const char *e = ir_type_name(t->elem, inner, sizeof(inner));
         snprintf(buf, cap, "%s[%u]", e, t->array_len);
         return buf;
      }
      case RSLC_TY_STRUCT:    return t->name ? t->name->str : "struct";
   }
   return "?";
}

static void ir_indent(FILE *f, int d)
{
   while (d-- > 0)
      fputs("  ", f);
}

static void ir_dump_node(const struct rslc_ctx *c, FILE *f, uint32_t id, int d);

static void ir_dump_list(const struct rslc_ctx *c, FILE *f, uint32_t list, int d)
{
   uint32_t i, n = list ? rslc_list_len(c, list) : 0;
   for (i = 0; i < n; i++)
      ir_dump_node(c, f, rslc_list_item(c, list, i), d);
}

static void ir_dump_node(const struct rslc_ctx *c, FILE *f, uint32_t id, int d)
{
   static const char *const ops[] = {
      "none", "const", "var", "member", "index", "swizzle", "call", "builtin",
      "construct", "unary", "binary", "assign", "ternary", "seq", "convert",
      "block", "decl", "expr", "if", "for", "while", "do", "switch", "case",
      "break", "continue", "return", "discard"
   };
   static const char *const uns[] = { "", "neg", "not", "bitnot", "preinc", "predec", "postinc", "postdec" };
   static const char *const bins[] = { "=", "+", "-", "*", "/", "%", "<<", ">>", "<", ">", "<=", ">=",
      "==", "!=", "&&", "||", "^^", "&", "|", "^" };
   const struct rslc_node *n;
   char tb[64];

   if (!id)
   {
      ir_indent(f, d);
      fputs("(none)\n", f);
      return;
   }
   n = rslc_node_at(c, id);
   ir_indent(f, d);
   fprintf(f, "%s", ops[n->op]);
   if (n->type)
      fprintf(f, " :%s", ir_type_name(n->type, tb, sizeof(tb)));
   if (n->prec)
      fprintf(f, " p%u", n->prec);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         if (n->type->base == RSLC_BASE_FLOAT)
         {
            float v;
            memcpy(&v, &n->a, 4);
            fprintf(f, " %g\n", (double)v);
         }
         else if (n->type->base == RSLC_BASE_INT)
            fprintf(f, " %d\n", (int)n->a);
         else
            fprintf(f, " %u\n", n->a);
         return;
      case RSLC_OP_VAR:
      {
         const struct rslc_var *v = c->prog.vars[n->a];
         fprintf(f, " %s#%u\n", v->name ? v->name->str : "?", v->id);
         return;
      }
      case RSLC_OP_MEMBER:
         fprintf(f, " .%u\n", n->b);
         ir_dump_node(c, f, n->a, d + 1);
         return;
      case RSLC_OP_INDEX:
         fputc('\n', f);
         ir_dump_node(c, f, n->a, d + 1);
         ir_dump_node(c, f, n->b, d + 1);
         return;
      case RSLC_OP_SWIZZLE:
      {
         unsigned cnt = n->aux & 3u, i;
         if (!cnt) cnt = 4;
         fputs(" .", f);
         for (i = 0; i < cnt; i++)
            fputc("xyzw"[(n->aux >> (2 + 2 * i)) & 3u], f);
         fputc('\n', f);
         ir_dump_node(c, f, n->a, d + 1);
         return;
      }
      case RSLC_OP_CALL:
         fprintf(f, " %s#%u\n", c->prog.funcs[n->a]->name->str, n->a);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_BUILTIN:
         fprintf(f, " bfn%u\n", n->a);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_CONSTRUCT:
         fputc('\n', f);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_UNARY:
         fprintf(f, " %s\n", uns[n->aux]);
         ir_dump_node(c, f, n->a, d + 1);
         return;
      case RSLC_OP_BINARY:
      case RSLC_OP_ASSIGN:
         fprintf(f, " %s\n", bins[n->aux]);
         ir_dump_node(c, f, n->a, d + 1);
         ir_dump_node(c, f, n->b, d + 1);
         return;
      case RSLC_OP_TERNARY:
      case RSLC_OP_IF:
         fputc('\n', f);
         ir_dump_node(c, f, n->a, d + 1);
         ir_dump_node(c, f, n->b, d + 1);
         if (n->c)
            ir_dump_node(c, f, n->c, d + 1);
         return;
      case RSLC_OP_SEQ:
      case RSLC_OP_WHILE:
      case RSLC_OP_DO:
         fputc('\n', f);
         ir_dump_node(c, f, n->a, d + 1);
         ir_dump_node(c, f, n->b, d + 1);
         return;
      case RSLC_OP_CONVERT:
      case RSLC_OP_EXPR:
      case RSLC_OP_RETURN:
         fputc('\n', f);
         if (n->a)
            ir_dump_node(c, f, n->a, d + 1);
         return;
      case RSLC_OP_BLOCK:
         fputc('\n', f);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_DECL:
      {
         const struct rslc_var *v = c->prog.vars[n->a];
         fprintf(f, " %s#%u :%s\n", v->name ? v->name->str : "?", v->id,
               ir_type_name(v->type, tb, sizeof(tb)));
         if (v->init)
            ir_dump_node(c, f, v->init, d + 1);
         return;
      }
      case RSLC_OP_FOR:
         fputc('\n', f);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_SWITCH:
         fputc('\n', f);
         ir_dump_node(c, f, n->a, d + 1);
         ir_dump_list(c, f, n->b, d + 1);
         return;
      case RSLC_OP_CASE:
         fputc('\n', f);
         if (n->a)
            ir_dump_node(c, f, n->a, d + 1);
         return;
      default:
         fputc('\n', f);
         return;
   }
}

static void ir_dump(const struct rslc_ctx *c, FILE *f)
{
   const struct rslc_prog *pr = &c->prog;
   uint32_t i, k;
   char     tb[64];

   for (k = 0; k < 2; k++)
   {
      if (!pr->has_block[k])
         continue;
      fprintf(f, "%s %s binding=%d set=%d\n", k ? "push" : "ubo",
            pr->blocks[k].type->name ? pr->blocks[k].type->name->str : "?",
            (int)pr->blocks[k].binding, (int)pr->blocks[k].set);
      for (i = 0; i < pr->blocks[k].type->nfields; i++)
         fprintf(f, "  %s :%s\n", pr->blocks[k].type->fields[i].name->str,
               ir_type_name(pr->blocks[k].type->fields[i].type, tb, sizeof(tb)));
   }
   for (i = 0; i < pr->nsamplers; i++)
      fprintf(f, "sampler %s binding=%d set=%d\n", pr->samplers[i]->name->str,
            (int)pr->samplers[i]->binding, (int)pr->samplers[i]->set);
   for (i = 0; i < pr->ninputs; i++)
      fprintf(f, "in %s :%s location=%d\n", pr->inputs[i]->name->str,
            ir_type_name(pr->inputs[i]->type, tb, sizeof(tb)), (int)pr->inputs[i]->location);
   for (i = 0; i < pr->noutputs; i++)
      fprintf(f, "out %s :%s location=%d\n", pr->outputs[i]->name->str,
            ir_type_name(pr->outputs[i]->type, tb, sizeof(tb)), (int)pr->outputs[i]->location);
   for (i = 1; i < pr->nfuncs; i++)
   {
      const struct rslc_func *fn = pr->funcs[i];
      uint32_t a;
      if (!fn->body)
         continue;
      fprintf(f, "func %s#%u :%s (", fn->name->str, fn->id, ir_type_name(fn->ret, tb, sizeof(tb)));
      for (a = 0; a < fn->nparams; a++)
         fprintf(f, "%s%s#%u :%s", a ? ", " : "", fn->params[a]->name ? fn->params[a]->name->str : "_",
               fn->params[a]->id, ir_type_name(fn->params[a]->type, tb, sizeof(tb)));
      fputs(")\n", f);
      ir_dump_node(c, f, fn->body, 1);
   }
}

/* Runs the front end over a source string.  mode 0: result only;
 * mode 1: print the IR to stdout. */
static bool front_run(const char *src, enum rslc_stage stage, int mode, char *err, size_t errcap)
{
   struct rslc_opts o;
   struct rslc_ctx  c;
   bool             ok;

   opts_default(&o, err, errcap);
   err[0] = 0;
   if (!rslc_ctx_init(&c, &o, stage))
      return false;
   ok = rslc_front_compile(&c, src, strlen(src));
   if (ok && mode == 1)
      ir_dump(&c, stdout);
   rslc_ctx_close(&c);
   return ok;
}

static int front_file(const char *path, int mode)
{
   char  err[256];
   FILE *f = fopen(path, "rb");
   char *src;
   long  n;
   bool  ok;
   enum rslc_stage stage = strstr(path, ".vert") ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT;

   if (!f)
   {
      fprintf(stderr, "cannot open %s\n", path);
      return 2;
   }
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   src = (char*)malloc((size_t)n + 1);
   if (!src || fread(src, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(src);
      return 2;
   }
   fclose(f);
   src[n] = '\0';
   ok = front_run(src, stage, mode, err, sizeof(err));
   free(src);
   if (!ok)
   {
      fprintf(stderr, "%s: %s\n", path, err);
      return 1;
   }
   if (mode == 0)
      printf("%s: ok\n", path);
   return 0;
}

/* --spirv <in> <out.spv>: compile a stage to SPIR-V and write the
 * words, for spirv-val and the corpus check. */
static int spirv_file(const char *path, const char *out)
{
   char  err[256];
   FILE *f = fopen(path, "rb");
   char *src;
   long  n;
   bool  ok;
   struct rslc_opts   opts;
   struct rslc_result res;
   enum rslc_stage stage = strstr(path, ".vert") ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT;

   if (!f)
   {
      fprintf(stderr, "cannot open %s\n", path);
      return 2;
   }
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   src = (char*)malloc((size_t)n + 1);
   if (!src || fread(src, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(src);
      return 2;
   }
   fclose(f);
   src[n] = '\0';
   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   ok = rslc_compile(src, (size_t)n, stage, &opts, &res);
   free(src);
   if (!ok)
   {
      fprintf(stderr, "%s: %s\n", path, err);
      return 1;
   }
   f = fopen(out, "wb");
   if (!f)
   {
      rslc_result_free(&res, &opts);
      return 2;
   }
   fwrite(res.spirv, 4, res.spirv_len, f);
   fclose(f);
   rslc_result_free(&res, &opts);
   return 0;
}

/* SPIR-V emission: a stock-shaped fragment stage compiles to a module
 * whose reflection carries glslang's layout (push std430, UBO
 * std140), and the blob re-opens as bytes. */
static void test_spirv(void)
{
   struct rslc_opts   o;
   struct rslc_result r, r2;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float f[3]; mat3 m; uint FrameCount; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; vec2 v2[2]; float x; } global;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 1) flat in int idx;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 3) uniform sampler2D Source;\n"
      "layout(set = 0, binding = 4) uniform sampler2D Other;\n"
      "void main() { FragColor = texture(Source, vTexCoord) * params.f[idx] + global.v2[1].xyxy; }\n";
   const struct rslc_reflect *rf;
   const struct rslc_member  *m;
   const struct rslc_sampler *smp;
   const struct rslc_io      *io;
   const uint8_t             *blob;
   void                      *copy;

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_SPIRV;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (!r.blob)
      return;
   blob = (const uint8_t*)r.blob;
   rf   = r.refl;
   CHECK(r.spirv_len > 5 && r.spirv[0] == 0x07230203u && r.spirv[3] > 10);
   CHECK(((uintptr_t)r.spirv & 3) == 0);

   /* push block, std430: vec4 @0, float[3] stride 4 @16 size 12,
    * mat3 @32 (col stride 16) size 48, uint @80; declared size 84 */
   CHECK(rf->push.present && rf->push.member_count == 4);
   CHECK(!strcmp(RSLC_STR(blob, rf->push.name), "Push"));
   CHECK(!strcmp(RSLC_STR(blob, rf->push.instance), "params"));
   m = (const struct rslc_member*)(blob + rf->push.members);
   CHECK(!strcmp(RSLC_STR(blob, m[0].name), "SourceSize") && m[0].offset == 0 && m[0].size == 16);
   CHECK(m[0].base == RSLC_BASE_FLOAT && m[0].cols == 1 && m[0].rows == 4 && m[0].array_len == 0);
   CHECK(!strcmp(RSLC_STR(blob, m[1].name), "f") && m[1].offset == 16 && m[1].size == 12 && m[1].array_len == 3);
   CHECK(!strcmp(RSLC_STR(blob, m[2].name), "m") && m[2].offset == 32 && m[2].size == 48 && m[2].cols == 3 && m[2].rows == 3);
   CHECK(!strcmp(RSLC_STR(blob, m[3].name), "FrameCount") && m[3].offset == 80 && m[3].base == RSLC_BASE_UINT);
   CHECK(rf->push.size == 84);
   /* only params.f is read: the active set is what the chain uploads */
   CHECK(!m[0].active && m[1].active && !m[2].active && !m[3].active);

   /* UBO, std140: mat4 @0 size 64, vec2[2] stride 16 @64 size 32,
    * float @96; declared size 100 (no tail padding, as SPIRV-Cross) */
   CHECK(rf->ubo.present && rf->ubo.member_count == 3 && rf->ubo.binding == 0);
   m = (const struct rslc_member*)(blob + rf->ubo.members);
   CHECK(m[0].offset == 0 && m[0].size == 64 && m[0].cols == 4 && m[0].rows == 4);
   CHECK(m[1].offset == 64 && m[1].size == 32 && m[1].array_len == 2 && m[1].rows == 2);
   CHECK(m[2].offset == 96 && m[2].size == 4);
   CHECK(rf->ubo.size == 100);
   CHECK(!m[0].active && m[1].active && !m[2].active);

   CHECK(rf->sampler_count == 2);
   smp = (const struct rslc_sampler*)(blob + rf->samplers);
   CHECK(!strcmp(RSLC_STR(blob, smp[0].name), "Source") && smp[0].binding == 3);
   CHECK(!strcmp(RSLC_STR(blob, smp[1].name), "Other") && smp[1].binding == 4);
   CHECK(rf->input_count == 2 && rf->output_count == 1);
   io = (const struct rslc_io*)(blob + rf->inputs);
   CHECK(!strcmp(RSLC_STR(blob, io[0].name), "vTexCoord") && io[0].location == 0 && io[0].rows == 2 && !io[0].flat);
   CHECK(!strcmp(RSLC_STR(blob, io[1].name), "idx") && io[1].location == 1 && io[1].base == RSLC_BASE_INT && io[1].flat);
   io = (const struct rslc_io*)(blob + rf->outputs);
   CHECK(!strcmp(RSLC_STR(blob, io[0].name), "FragColor") && io[0].rows == 4);

   /* the blob is self-contained: a byte copy re-opens to the same view */
   copy = malloc(r.blob_len);
   CHECK(copy != NULL);
   if (copy)
   {
      memcpy(copy, r.blob, r.blob_len);
      CHECK(rslc_blob_open(copy, r.blob_len, &r2));
      CHECK(r2.spirv_len == r.spirv_len && !memcmp(r2.spirv, r.spirv, r.spirv_len * 4));
      CHECK(r2.refl->push.size == 84 && r2.refl->sampler_count == 2);
      free(copy);
   }
   rslc_result_free(&r, &o);
   CHECK(alloc_live == 0);
}

/* Whether the module holds OpConstant of 32-bit scalar type tid... :
 * scans for an OpConstant whose value word is v. */
static bool spirv_has_const(const uint32_t *w, size_t n, uint32_t v)
{
   size_t i = 5;
   while (i < n)
   {
      uint32_t len = w[i] >> 16, op = w[i] & 0xffffu;
      if (!len)
         return false;
      if (op == 43 && len == 4 && w[i + 3] == v)   /* OpConstant */
         return true;
      i += len;
   }
   return false;
}

/* Constant folding runs in double and rounds once, as glslang's does,
 * and overload choice prefers the fewest conversions: the module's
 * constants are glslang's constants. */
static void test_folding(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { uint FrameCount; } params;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "const vec3 Y = vec3(0.299, 0.587, 0.114);\n"
      "float luma(vec3 c) { return c.y * (0.587/0.299) + c.x; }\n"
      "void main() {\n"
      "  const float arr[3] = float[](0.35, 0.518, 0.962);\n"
      "  const float unused = 10000.0;\n"
      "  float f = float(max(params.FrameCount, 8000));\n"
      "  FragColor = vec4(luma(vec3(vTexCoord, arr[1])), f, radians(-15.0), 1);\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_SPIRV;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (!r.blob)
      return;
   CHECK(spirv_has_const(r.spirv, r.spirv_len, 0x3ffb4a7du));    /* 0.587/0.299 in double */
   CHECK(!spirv_has_const(r.spirv, r.spirv_len, 0x3ffb4a7eu));   /* not the float quotient */
   CHECK(spirv_has_const(r.spirv, r.spirv_len, 0x3f049ba6u));    /* arr[1] = 0.518 folded */
   CHECK(!spirv_has_const(r.spirv, r.spirv_len, 0x461c4000u));   /* unused const: nothing emitted */
   CHECK(spirv_has_const(r.spirv, r.spirv_len, 8000u));          /* max(uint, int) is max(uint, uint) */
   CHECK(!spirv_has_const(r.spirv, r.spirv_len, 0x45fa0000u));   /* ... not max(float, float) */
   CHECK(spirv_has_const(r.spirv, r.spirv_len, 0xbe860a92u));    /* radians(-15.0) folded */
   rslc_result_free(&r, &o);
   CHECK(alloc_live == 0);
}

/* --glsl <in> <version> [es]: print a stage as GLSL to stdout, with the
 * interface named as RetroArch's glcore driver names it.
 * --hlsl <in> <model>: print it as HLSL for shader model 40 / 50.
 * --msl <in>: print it as MSL. */
static int glsl_file(const char *path, unsigned version, bool es, enum rslc_dst dst)
{
   char  err[256];
   FILE *f = fopen(path, "rb");
   char *src;
   long  n;
   bool  ok;
   struct rslc_opts   opts;
   struct rslc_result res;
   struct rslc_names  names;
   enum rslc_stage stage = strstr(path, ".vert") ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT;

   if (!f)
   {
      fprintf(stderr, "cannot open %s\n", path);
      return 2;
   }
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   src = (char*)malloc((size_t)n + 1);
   if (!src || fread(src, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(src);
      return 2;
   }
   fclose(f);
   src[n] = '\0';
   memset(&names, 0, sizeof(names));
   names.attr_prefix    = "RARCH_ATTRIBUTE_";
   names.varying_prefix = "RARCH_VARYING_";
   names.sampler_prefix = "RARCH_TEXTURE_";
   names.ubo_block      = stage == RSLC_STAGE_VERTEX ? "RARCH_UBO_VERTEX" : "RARCH_UBO_FRAGMENT";
   names.ubo_instance   = stage == RSLC_STAGE_VERTEX ? "RARCH_UBO_VERTEX_INSTANCE" : "RARCH_UBO_FRAGMENT_INSTANCE";
   names.push_block     = stage == RSLC_STAGE_VERTEX ? "RARCH_PUSH_VERTEX" : "RARCH_PUSH_FRAGMENT";
   names.push_instance  = stage == RSLC_STAGE_VERTEX ? "RARCH_PUSH_VERTEX_INSTANCE" : "RARCH_PUSH_FRAGMENT_INSTANCE";
   memset(&opts, 0, sizeof(opts));
   opts.dst     = dst;
   opts.version = version;
   opts.names   = dst == RSLC_DST_GLSL || dst == RSLC_DST_GLSL_ES ? &names : NULL;
   (void)es;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   ok = rslc_compile(src, (size_t)n, stage, &opts, &res);
   free(src);
   if (!ok)
   {
      fprintf(stderr, "%s: %s\n", path, err);
      return 1;
   }
   fwrite(res.text, 1, res.text_len, stdout);
   rslc_result_free(&res, &opts);
   return 0;
}

/* The GLSL printer: the interface named as the driver asks, push
 * constants as a struct uniform, constants folded, scalar swizzles
 * and local consts as glslang leaves them, and glslangValidator-shaped
 * output for both the desktop and the ES target. */
static void test_glsl(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   struct rslc_names  names;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float gamma; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 1) flat in int idx;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "float luma(vec3 c) { return c.y * (0.587/0.299) + c.x; }\n"
      "float unused_helper(float x) { return x * 2.0; }\n"
      "void main() {\n"
      "  const float k = 0.25 * 4.0;\n"
      "  float s = params.gamma;\n"
      "  vec3 c = texture(Source, vTexCoord).rgb * s.xxx;\n"
      "  FragColor = vec4(luma(c) * k, float(idx), 0, 1);\n"
      "}\n";

   memset(&names, 0, sizeof(names));
   names.attr_prefix    = "RARCH_ATTRIBUTE_";
   names.varying_prefix = "RARCH_VARYING_";
   names.sampler_prefix = "RARCH_TEXTURE_";
   names.ubo_block      = "RARCH_UBO_FRAGMENT";
   names.ubo_instance   = "RARCH_UBO_FRAGMENT_INSTANCE";
   names.push_block     = "RARCH_PUSH_FRAGMENT";
   names.push_instance  = "RARCH_PUSH_FRAGMENT_INSTANCE";
   opts_default(&o, err, sizeof(err));
   o.dst     = RSLC_DST_GLSL;
   o.version = 330;
   o.names   = &names;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (!r.text)
      return;
   CHECK(r.text_len == strlen(r.text));
   CHECK(strstr(r.text, "#version 330\n") == r.text);
   CHECK(strstr(r.text, "layout(std140) uniform RARCH_UBO_FRAGMENT {") != NULL);
   CHECK(strstr(r.text, "} RARCH_UBO_FRAGMENT_INSTANCE;") != NULL);
   CHECK(strstr(r.text, "struct RARCH_PUSH_FRAGMENT {") != NULL);
   CHECK(strstr(r.text, "uniform RARCH_PUSH_FRAGMENT RARCH_PUSH_FRAGMENT_INSTANCE;") != NULL);
   CHECK(strstr(r.text, "uniform sampler2D RARCH_TEXTURE_2;") != NULL);
   CHECK(strstr(r.text, "in vec2 RARCH_VARYING_0;") != NULL);
   CHECK(strstr(r.text, "flat in int RARCH_VARYING_1;") != NULL);
   CHECK(strstr(r.text, "layout(location = 0) out vec4 FragColor;") != NULL);
   CHECK(strstr(r.text, "RARCH_PUSH_FRAGMENT_INSTANCE.gamma") != NULL);
   CHECK(strstr(r.text, "1.9632107 ") != NULL);           /* folded in double */
   CHECK(strstr(r.text, "0.587") == NULL);
   CHECK(strstr(r.text, "vec3(s)") != NULL);              /* scalar swizzle */
   CHECK(strstr(r.text, "unused_helper") == NULL);        /* unreachable */
   CHECK(strstr(r.text, "const float k") == NULL);        /* folded away */
   CHECK(strstr(r.text, "* 1.0") != NULL);
   CHECK(strstr(r.text, ", 0.0, 1.0)") != NULL);          /* int literals converted */
   CHECK(r.refl->push.present && r.refl->sampler_count == 1);
   {
      const struct rslc_member *m = (const struct rslc_member*)((const uint8_t*)r.blob + r.refl->push.members);
      CHECK(!m[0].active && m[1].active);
   }
   rslc_result_free(&r, &o);

   o.dst     = RSLC_DST_GLSL_ES;
   o.version = 300;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "#version 300 es\n") == r.text);
      CHECK(strstr(r.text, "precision highp float;") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

/* Words Vulkan GLSL leaves free and GLSL 3.30 or ES 3.00 reserve -
 * packed, as scanline-classic names a local and a parameter - are
 * printed renamed, every use with its declaration. */
static void test_glsl_reserved(void)
{
   static const char frag[] =
      "#version 450\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "struct S { float row_major; };\n"
      "float unpack(float packed) { return packed * 2.0; }\n"
      "void main() {\n"
      "  vec3 packed = texture(Source, vTexCoord).rgb;\n"
      "  S varying; varying.row_major = packed.r;\n"
      "  float attribute = unpack(varying.row_major);\n"
      "  FragColor = vec4(packed, attribute);\n"
      "}\n";
   static const enum rslc_dst dsts[] = { RSLC_DST_GLSL, RSLC_DST_GLSL_ES };
   static const char *const words[] = { "packed", "row_major", "varying", "attribute" };
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   unsigned           d, w;

   for (d = 0; d < 2; d++)
   {
      opts_default(&o, err, sizeof(err));
      o.dst = dsts[d];
      CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
      if (!r.text)
      {
         fprintf(stderr, "  %s\n", err);
         continue;
      }
      for (w = 0; w < 4; w++)
      {
         const char *at = r.text;
         size_t      n  = strlen(words[w]);
         while ((at = strstr(at, words[w])))
         {
            /* a whole word, not followed by the rename's underscore */
            bool start = at == r.text || !(isalnum((unsigned char)at[-1]) || at[-1] == '_');
            bool whole = !(isalnum((unsigned char)at[n]) || at[n] == '_');
            if (start && whole)
            {
               fprintf(stderr, "  dst %u prints %s bare\n", (unsigned)dsts[d], words[w]);
               CHECK(0);
               break;
            }
            at += n;
         }
      }
      CHECK(strstr(r.text, "packed_") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

/* Flattened blocks: each block one vec4 array named for the block,
 * members read out at their std140 / std430 offsets - a matrix from
 * its columns, a narrow member through a swizzle, an integer through
 * a bitcast - so glcore uploads a block's bytes with one glUniform4fv;
 * a block that does not fit vec4 slots is refused. */
static void test_glsl_flatten(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   struct rslc_names  names;
   char               err[128];
   static const char  vert[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO {\n"
      "  mat4 MVP; vec2 size; float alpha; uint mode; mat3 tint; vec4 extra;\n"
      "} global;\n"
      "layout(push_constant) uniform Push { float time; vec3 dir; } params;\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 0) out vec4 vColor;\n"
      "void main() {\n"
      "  gl_Position = global.MVP * Position;\n"
      "  vColor = vec4(global.tint * params.dir, global.alpha)\n"
      "         + vec4(global.size, float(global.mode), params.time) + global.extra;\n"
      "}\n";
   static const char  bad[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; float w[4]; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "void main() { gl_Position = global.MVP * Position * global.w[1]; }\n";

   memset(&names, 0, sizeof(names));
   names.attr_prefix   = "RARCH_ATTRIBUTE_";
   names.varying_prefix = "RARCH_VARYING_";
   names.ubo_block     = "RARCH_UBO_VERTEX";
   names.ubo_instance  = "RARCH_UBO_VERTEX_INSTANCE";
   names.push_block    = "RARCH_PUSH_VERTEX";
   names.push_instance = "RARCH_PUSH_VERTEX_INSTANCE";
   names.flatten       = true;
   opts_default(&o, err, sizeof(err));
   o.dst     = RSLC_DST_GLSL;
   o.version = 330;
   o.names   = &names;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (r.text)
   {
      /* MVP 0-63, size 64, alpha 72, mode 76, tint 80-127, extra 128 */
      CHECK(strstr(r.text, "uniform vec4 RARCH_UBO_VERTEX[9];") != NULL);
      CHECK(strstr(r.text, "mat4(RARCH_UBO_VERTEX[0], RARCH_UBO_VERTEX[1], RARCH_UBO_VERTEX[2], RARCH_UBO_VERTEX[3])") != NULL);
      CHECK(strstr(r.text, "RARCH_UBO_VERTEX[4].xy") != NULL);
      CHECK(strstr(r.text, "RARCH_UBO_VERTEX[4].z") != NULL);
      CHECK(strstr(r.text, "floatBitsToUint(RARCH_UBO_VERTEX[4].w)") != NULL);
      CHECK(strstr(r.text, "mat3(RARCH_UBO_VERTEX[5].xyz, RARCH_UBO_VERTEX[6].xyz, RARCH_UBO_VERTEX[7].xyz)") != NULL);
      CHECK(strstr(r.text, "RARCH_UBO_VERTEX[8]") != NULL);
      /* std430 push: time 0, dir 16 */
      CHECK(strstr(r.text, "uniform vec4 RARCH_PUSH_VERTEX[2];") != NULL);
      CHECK(strstr(r.text, "RARCH_PUSH_VERTEX[0].x") != NULL);
      CHECK(strstr(r.text, "RARCH_PUSH_VERTEX[1].xyz") != NULL);
      CHECK(strstr(r.text, "INSTANCE") == NULL);
      CHECK(strstr(r.text, "uniform RARCH_UBO_VERTEX") == NULL);
      rslc_result_free(&r, &o);
   }
   o.dst     = RSLC_DST_GLSL_ES;
   o.version = 300;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "uniform highp vec4 RARCH_UBO_VERTEX[9];") != NULL);
      rslc_result_free(&r, &o);
   }
   /* an array member has no vec4 reading */
   err[0]    = '\0';
   o.dst     = RSLC_DST_GLSL;
   o.version = 330;
   CHECK(!rslc_compile(bad, sizeof(bad) - 1, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(strstr(err, "flatten") != NULL);
   CHECK(alloc_live == 0);
}

/* The HLSL printer: matrices transposed so cbuffer members overlay
 * std140 and products swap into mul(), the uniform block at b0 and the
 * push block at b1 with packoffsets, Texture2D + SamplerState pairs at
 * the binding, locations as TEXCOORDn / SV_Target, builtins through
 * the wrapper main, aggregate constructors hoisted, GLSL mod kept,
 * and shader model 4's gather limit refused. */
static void test_hlsl(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  vert[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; vec2 scale; float t; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "layout(location = 1) flat out int vIdx;\n"
      "void main() {\n"
      "  mat2 r = mat2(global.t);\n"
      "  gl_Position = global.MVP * Position;\n"
      "  vTexCoord = TexCoord * r * global.scale;\n"
      "  vIdx = gl_VertexIndex;\n"
      "}\n";
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float gamma; } params;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "float pick(sampler2D s, float k[3], int i) { return texture(s, vTexCoord).r * k[i]; }\n"
      "void main() {\n"
      "  vec2 m = mod(vTexCoord, 0.5);\n"
      "  float v = pick(Source, float[3](1.0, params.gamma, 3.0), int(m.x));\n"
      "  if (gl_FragCoord.x > 8.0 && vTexCoord == m) discard;\n"
      "  FragColor = vec4(v.xxx, textureGather(Source, vTexCoord, 1).x);\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst     = RSLC_DST_HLSL;
   o.version = 50;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (!r.text)
      return;
   CHECK(strstr(r.text, "cbuffer UBO : register(b0)") != NULL);
   CHECK(strstr(r.text, "row_major float4x4 global_MVP : packoffset(c0);") != NULL);
   CHECK(strstr(r.text, "float2 global_scale : packoffset(c4);") != NULL);
   CHECK(strstr(r.text, "float global_t : packoffset(c4.z);") != NULL);
   CHECK(strstr(r.text, "float4 Position : TEXCOORD0;") != NULL);
   CHECK(strstr(r.text, "nointerpolation int vIdx : TEXCOORD1;") != NULL);
   CHECK(strstr(r.text, "float4 gl_Position : SV_Position;") != NULL);
   CHECK(strstr(r.text, "uint gl_VertexIndex : SV_VertexID;") != NULL);
   CHECK(strstr(r.text, "gl_Position = mul(Position, global_MVP);") != NULL);
   CHECK(strstr(r.text, "mul(r, TexCoord) * global_scale") != NULL);
   CHECK(strstr(r.text, "float2x2((global_t), 0.0f, 0.0f, (global_t))") != NULL);
   CHECK(strstr(r.text, "gl_VertexIndex = int(stage_input.gl_VertexIndex);") != NULL);
   CHECK(strstr(r.text, "rslc_out main(rslc_in stage_input)") != NULL);
   CHECK(strstr(r.text, "gl_FragCoord") == NULL);
   rslc_result_free(&r, &o);

   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "float2 mod(float2 x, float2 y)") != NULL);
      CHECK(strstr(r.text, "cbuffer Push : register(b1)") != NULL);
      CHECK(strstr(r.text, "float params_gamma : packoffset(c1);") != NULL);
      CHECK(strstr(r.text, "Texture2D<float4> Source : register(t2);") != NULL);
      CHECK(strstr(r.text, "SamplerState _Source_sampler : register(s2);") != NULL);
      CHECK(strstr(r.text, "float pick(Texture2D<float4> s, SamplerState _s_sampler, float k[3], int i)") != NULL);
      CHECK(strstr(r.text, "s.Sample(_s_sampler, vTexCoord).x") != NULL);
      CHECK(strstr(r.text, "float _rslc_t0[3] = { 1.0f, params_gamma, 3.0f };") != NULL);
      CHECK(strstr(r.text, "pick(Source, _Source_sampler, _rslc_t0, int(m.x))") != NULL);
      CHECK(strstr(r.text, "mod(vTexCoord, (0.5f).xx)") != NULL);
      CHECK(strstr(r.text, "all(vTexCoord == m)") != NULL);
      CHECK(strstr(r.text, "(v).xxx") != NULL);
      CHECK(strstr(r.text, "Source.GatherGreen(_Source_sampler, vTexCoord).x") != NULL);
      CHECK(strstr(r.text, "float4 gl_FragCoord : SV_Position;") != NULL);
      CHECK(strstr(r.text, "gl_FragCoord.w = 1.0f / gl_FragCoord.w;") != NULL);
      CHECK(strstr(r.text, "float4 FragColor : SV_Target0;") != NULL);
      rslc_result_free(&r, &o);
   }

   /* shader model 4 has Gather for the red component only */
   o.version = 40;
   CHECK(!rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   CHECK(strstr(err, "red component") != NULL);
   CHECK(alloc_live == 0);
}

/* HLSL scoping and ?: as Microsoft's D3DCompile reads them: a ?:
 * statement with assignments for operands is printed as if / else
 * (HLSL takes no assignment there and evaluates both operands), and a
 * local whose initializer reads the global of its name is renamed,
 * since in HLSL the initializer would read the local itself; a local
 * declared without a value starts at 0, and a function that can run
 * off its end returns 0 there, since D3DCompile refuses a read it
 * cannot prove follows a write and a path that returns nothing. */
static void test_hlsl_scoping(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { float bgr; float lit; } params;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 1) in vec2 vStep;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "vec2 step_ = vec2(0.0);\n"
      "float half_(float x) { if (x > 0.5) return 1.0; }\n"
      "void main() {\n"
      "  vec3 mask = vec3(0.0);\n"
      "  step_ = vStep;\n"
      "  params.bgr == 1.0 ? mask.z = params.lit : mask.x = params.lit;\n"
      "  if (vTexCoord.x > 0.5) { vec2 step_ = step_ * 2.0; mask.xy += step_; }\n"
      "  float u;\n"
      "  if (vTexCoord.y > 0.5) u = 1.0;\n"
      "  FragColor = vec4(mask, u * half_(vTexCoord.x));\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst     = RSLC_DST_HLSL;
   o.version = 50;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (!r.text)
      return;
   CHECK(strstr(r.text, "float u = (float)0;") != NULL);
   CHECK(strstr(r.text, "return (float)0;") != NULL);
   CHECK(strstr(r.text, " ? ") == NULL);
   CHECK(strstr(r.text, "if (params_bgr == 1.0f) {") != NULL);
   CHECK(strstr(r.text, "mask.z = params_lit;") != NULL);
   CHECK(strstr(r.text, "} else {") != NULL);
   CHECK(strstr(r.text, "mask.x = params_lit;") != NULL);
   CHECK(strstr(r.text, "float2 step__") != NULL);
   CHECK(strstr(r.text, "float2 step_ = step_") == NULL);
   rslc_result_free(&r, &o);
   CHECK(alloc_live == 0);
}

/* The MSL printer: no globals, so every function takes what it
 * reaches by reference (blocks as constant references, samplers as
 * texture + sampler pairs) and main0 holds the stage's variables;
 * blocks laid out at their std140 offsets with vec3 packed where the
 * tail is taken; arrays as metal::array with braced initialisers; a
 * vector component passed to an out parameter through a temporary;
 * the interface at the chain's buffer, texture and sampler indices. */
/* MSL as Apple's compiler takes it: array, matrix and struct varyings
 * one member per element, column or field at consecutive locations;
 * length / distance / dot / normalize of scalars spelled out; x *= M
 * as x = x * M; constructor arguments that mix scalars and vectors for
 * a matrix, or run past a vector, as components; program-scope
 * constants built from columns of literals (a matrix of scalars or a
 * constant naming another needs a global constructor, which Metal
 * refuses); and names Metal's headers take renamed. */
static void test_msl_apple(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  vert[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "struct P { vec2 a; float b; };\n"
      "layout(location = 0) out vec2 vTap[3];\n"
      "layout(location = 3) out mat2 vRot;\n"
      "layout(location = 5) out P vP;\n"
      "void main() {\n"
      "  gl_Position = global.MVP * Position;\n"
      "  vTap[0] = Position.xy; vTap[1] = Position.yx; vTap[2] = Position.zw;\n"
      "  vRot = mat2(Position.x, Position.yz, 1.0);\n"
      "  vP.a = Position.xy; vP.b = Position.w;\n"
      "}\n";
   static const char  frag[] =
      "#version 450\n"
      "layout(location = 0) in vec2 vTap[3];\n"
      "layout(location = 3) in mat2 vRot;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "const mat2 kA = mat2(1.0, 2.0, 3.0, 4.0);\n"
      "const mat2 kB[2] = mat2[2](kA, mat2(1.0));\n"
      "const float decay = 2.0;\n"
      "void main() {\n"
      "  mat2 m = vRot;\n"
      "  m *= kB[1];\n"
      "  vec2 v = vTap[1] * m;\n"
      "  v *= kA;\n"
      "  float l = length(v.x) + distance(v.x, v.y) + dot(v.x, v.y) + normalize(v.y);\n"
      "  FragColor = vec4(vec3(v, v + decay), l);\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_MSL;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "float2 vTap_0 [[user(locn0)]];") != NULL);
      CHECK(strstr(r.text, "float2 vTap_2 [[user(locn2)]];") != NULL);
      CHECK(strstr(r.text, "float2 vRot_1 [[user(locn4)]];") != NULL);
      CHECK(strstr(r.text, "float2 vP_a [[user(locn5)]];") != NULL);
      CHECK(strstr(r.text, "float vP_b [[user(locn6)]];") != NULL);
      CHECK(strstr(r.text, "out.vTap_1 = vTap[1];") != NULL);
      CHECK(strstr(r.text, "out.vRot_0 = vRot[0];") != NULL);
      CHECK(strstr(r.text, "out.vP_b = vP.b;") != NULL);
      CHECK(strstr(r.text, "array<float2, 3> vTap [[") == NULL);
      CHECK(strstr(r.text, "(Position.yz).x, (Position.yz).y") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "vTap[1] = in.vTap_1;") != NULL);
      CHECK(strstr(r.text, "vRot[1] = in.vRot_1;") != NULL);
      CHECK(strstr(r.text, "m = m * ") != NULL);
      CHECK(strstr(r.text, "v = v * ") != NULL);
      CHECK(strstr(r.text, "*=") == NULL);
      CHECK(strstr(r.text, "abs(v.x)") != NULL);
      CHECK(strstr(r.text, "abs(v.x - v.y)") != NULL);
      CHECK(strstr(r.text, "(v.x * v.y)") != NULL);
      CHECK(strstr(r.text, "sign(v.y)") != NULL);
      CHECK(strstr(r.text, "length(") == NULL);
      CHECK(strstr(r.text, "float2x2(float2(1.0f, 2.0f), float2(3.0f, 4.0f))") != NULL);
      CHECK(strstr(r.text, "float2x2(1.0f, 2.0f") == NULL);
      CHECK(strstr(r.text, "decay_") != NULL);
      CHECK(strstr(r.text, "float3(v, (v + decay_).x)") != NULL);
      CHECK(strstr(r.text, "array<float2x2, 2>{{ float2x2(float2(1.0f, 2.0f), float2(3.0f, 4.0f)), float2x2(float2(1.0f, 0.0f), float2(0.0f, 1.0f)) }}") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

/* Metal's fast-math pow() takes |x|: pow() goes out as powr(),
 * undefined below zero as in GLSL, unless the base is a constant
 * known >= 0, where pow() is the same and more exact. */
static void test_msl_powr(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(location = 0) in vec4 vColor;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "const float powr = 2.0;\n"
      "void main() {\n"
      "  vec2 e = vColor.xy;\n"
      "  FragColor = pow(vColor * powr, vec4(1.0 / 2.2));\n"
      "  FragColor.xy += pow(vec2(2.0, 10.0), e);\n"
      "  FragColor.z += pow(2.0, e.x);\n"
      "  FragColor.w += pow(-2.0, e.y);\n"
      "  FragColor.x += pow(vec2(2.0, -1.0), e).x;\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_MSL;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "powr(vColor * powr_, ") != NULL);
      CHECK(strstr(r.text, " pow(float2(2.0f, 10.0f), e)") != NULL);
      CHECK(strstr(r.text, " pow(2.0f, e.x)") != NULL);
      CHECK(strstr(r.text, "powr(-2.0f, e.y)") != NULL);
      CHECK(strstr(r.text, "powr(float2(2.0f, -1.0f), e)") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

/* Every function but main0 is printed static inline always_inline,
 * as SPIRV-Cross prints them: under fast math a call rounds
 * differently from the same code inlined. */
static void test_msl_inline(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(location = 0) in vec4 vColor;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "float half_of(float x);\n"
      "void main() {\n"
      "  FragColor = vColor * half_of(vColor.x);\n"
      "}\n"
      "float half_of(float x) { return x * 0.5; }\n";
   static const char  helpers[] =
      "#version 450\n"
      "layout(location = 0) in vec4 vColor;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "void main() {\n"
      "  mat2 m = mat2(vColor);\n"
      "  mat2 k = inverse(m) + matrixCompMult(m, m);\n"
      "  FragColor = mod(vColor, 3.0) + vec4(k[0], k[1]);\n"
      "  FragColor.x += float(findLSB(int(vColor.w)));\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_MSL;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "static inline __attribute__((always_inline)) float half_of(float x);") != NULL);
      CHECK(strstr(r.text, "static inline __attribute__((always_inline)) float half_of(float x) {") != NULL);
      CHECK(strstr(r.text, "static inline __attribute__((always_inline)) void rslc_main(") != NULL);
      CHECK(strstr(r.text, "always_inline)) fragment") == NULL);
      CHECK(strstr(r.text, " main0(") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);

   /* the helpers rslc prints for GLSL builtins MSL lacks, too */
   CHECK(rslc_compile(helpers, sizeof(helpers) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "template<typename Tx, typename Ty> static inline __attribute__((always_inline)) Tx mod(") != NULL);
      CHECK(strstr(r.text, "static inline __attribute__((always_inline)) float2x2 rslc_inverse2(") != NULL);
      CHECK(strstr(r.text, "static inline __attribute__((always_inline)) float2x2 rslc_cmul(") != NULL);
      CHECK(strstr(r.text, "template<typename T> static inline __attribute__((always_inline)) T rslc_findlsb(") != NULL);
      CHECK(strstr(r.text, "\ninline ") == NULL);
      CHECK(strstr(r.text, "> inline ") == NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

static void test_msl(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  vert[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; vec3 tint; float t; vec2 scale; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "vec2 half_scale;\n"
      "vec2 sc(vec2 v) { return v * global.scale * half_scale; }\n"
      "void main() {\n"
      "  half_scale = global.scale * 0.5;\n"
      "  gl_Position = global.MVP * Position;\n"
      "  vTexCoord = sc(TexCoord) + global.tint.xy * global.t;\n"
      "}\n";
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float gamma; } params;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "void split(float v, out float lo, out float hi) { lo = fract(v); hi = v - lo; }\n"
      "float pick(sampler2D s, float k[3], int i) { return texture(s, vTexCoord).r * k[i]; }\n"
      "void main() {\n"
      "  vec2 m = mod(vTexCoord, 0.5);\n"
      "  vec3 c = vec3(0.0);\n"
      "  split(params.gamma, c.x, c.y);\n"
      "  float v = pick(Source, float[3](1.0, params.gamma, 3.0), int(m.x));\n"
      "  if (gl_FragCoord.x > 8.0 && vTexCoord == m) discard;\n"
      "  FragColor = vec4(v.xxx + c, textureGather(Source, vTexCoord, 1).x);\n"
      "}\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_MSL;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (!r.text)
      return;
   CHECK(strstr(r.text, "#include <metal_stdlib>") == r.text);
   CHECK(strstr(r.text, "float4x4 MVP;") != NULL);
   CHECK(strstr(r.text, "packed_float3 tint;") != NULL);     /* t sits in its tail */
   CHECK(strstr(r.text, "float t;") != NULL);
   CHECK(strstr(r.text, "float2 scale;") != NULL);
   CHECK(strstr(r.text, "float4 Position [[attribute(0)]];") != NULL);
   CHECK(strstr(r.text, "float2 vTexCoord [[user(locn0)]];") != NULL);
   CHECK(strstr(r.text, "float4 gl_Position [[position]];") != NULL);
   CHECK(strstr(r.text, "float2 sc(float2 v, thread float2& half_scale, constant UBO& global)") != NULL);
   CHECK(strstr(r.text, "vertex main0_out main0(main0_in in [[stage_in]], constant UBO& global [[buffer(0)]])") != NULL);
   CHECK(strstr(r.text, "float2 half_scale = {};") != NULL);
   CHECK(strstr(r.text, "sc(TexCoord, half_scale, global)") != NULL);
   CHECK(strstr(r.text, "global.MVP * Position") != NULL);
   CHECK(strstr(r.text, "out.gl_Position = gl_Position;") != NULL);
   rslc_result_free(&r, &o);

   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (r.text)
   {
      CHECK(strstr(r.text, "always_inline)) Tx mod(Tx x, Ty y)") != NULL);
      CHECK(strstr(r.text, "constant Push& params [[buffer(1)]]") != NULL);
      CHECK(strstr(r.text, "texture2d<float> Source [[texture(2)]], sampler SourceSmplr [[sampler(2)]]") != NULL);
      CHECK(strstr(r.text, "float4 gl_FragCoord [[position]]") != NULL);
      CHECK(strstr(r.text, "float pick(texture2d<float> s, sampler sSmplr, array<float, 3> k, int i, thread float2& vTexCoord)") != NULL);
      CHECK(strstr(r.text, "void split(float v, thread float& lo, thread float& hi)") != NULL);
      CHECK(strstr(r.text, "float _rslc_t0 = c.x;") != NULL);
      CHECK(strstr(r.text, "split(params.gamma, _rslc_t0, _rslc_t1);") != NULL);
      CHECK(strstr(r.text, "c.y = _rslc_t1;") != NULL);
      CHECK(strstr(r.text, "array<float, 3>{{ 1.0f, params.gamma, 3.0f }}") != NULL);
      CHECK(strstr(r.text, "all(vTexCoord == m)") != NULL);
      CHECK(strstr(r.text, "discard_fragment();") != NULL);
      CHECK(strstr(r.text, "Source.gather(SourceSmplr, vTexCoord, int2(0), component::y).x") != NULL);
      CHECK(strstr(r.text, "float4 FragColor [[color(0)]];") != NULL);
      CHECK(strstr(r.text, "s.sample(sSmplr, vTexCoord).x") != NULL);
      rslc_result_free(&r, &o);
   }
   CHECK(alloc_live == 0);
}

static void test_front(void)
{
   char err[256];
   static const char stock_frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; vec4 OriginalSize; vec4 OutputSize; uint FrameCount; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
      "void main() { FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0); }\n";
   static const char stock_vert[] =
      "#version 450\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "void main() { gl_Position = global.MVP * Position; vTexCoord = TexCoord; }\n";

   CHECK(front_run(stock_frag, RSLC_STAGE_FRAGMENT, 0, err, sizeof(err)));
   CHECK(front_run(stock_vert, RSLC_STAGE_VERTEX, 0, err, sizeof(err)));

   /* Overloads, implicit conversions, const folding into array sizes,
    * swizzle lvalues, struct members, precision propagation. */
   CHECK(front_run(
      "#version 450\n"
      "precision mediump float;\n"
      "struct S { float a; vec3 b[2]; };\n"
      "const int N = 2 + 2;\n"
      "float f(float x) { return x; }\n"
      "vec2 f(vec2 x) { return x * 2; }\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "void main() {\n"
      "   float arr[N];\n"
      "   S s;\n"
      "   highp float h = 1.0;\n"
      "   int i = 3;\n"
      "   s.b[1].yz = vec2(i);\n"
      "   arr[i] = f(2) + f(vec2(1)).x + mix(h, 2.0, 0.5) + pow(2.0, float(i));\n"
      "   for (int k = 0; k < N; k++) arr[k] += float(k);\n"
      "   switch (i) { case 1: break; default: i = N; }\n"
      "   FragColor = vec4(arr[0], s.a, s.b[0].x, (i > 2) ? 1.0 : 0.0);\n"
      "}\n", RSLC_STAGE_FRAGMENT, 0, err, sizeof(err)));
   CHECK(!strcmp(err, ""));

   /* Refusals. */
   CHECK(!front_run("#version 450\nvoid main() { int x = 1.0; }\n", RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(strstr(err, "mismatch") != NULL);
   CHECK(!front_run("#version 450\nvoid main() { undeclared = 1; }\n", RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(strstr(err, "undeclared") != NULL);
   CHECK(!front_run("#version 450\nvoid main() { float a[-1]; }\n", RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(!front_run("#version 450\nvoid foo() {}\n", RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(strstr(err, "main") != NULL);
   CHECK(!front_run("#version 450\nvoid main() { vec3 v; v.xx = vec2(1); }\n", RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(strstr(err, "lvalue") != NULL);
   CHECK(!front_run("#version 450\nlayout(binding=0) uniform A { float x; }; layout(binding=1) uniform B { float y; };\nvoid main(){}\n",
            RSLC_STAGE_VERTEX, 0, err, sizeof(err)));
   CHECK(strstr(err, "more than one uniform block") != NULL);
   CHECK(alloc_live == 0);
}

/* Build a minimal valid blob by hand, then break it one field at a
 * time: every offset in the reflection section is range-checked and
 * every name must be NUL-terminated inside the blob. */
static void test_blob(void)
{
   struct rslc_result   r;
   struct rslc_blob_hdr *h;
   struct rslc_reflect  *refl;
   struct rslc_member   *m;
   struct rslc_sampler  *smp;
   struct rslc_io       *io;
   uint32_t             *words;
   char                 *str;
   uint8_t               buf[512];
   uint32_t              off = 0;

   memset(buf, 0, sizeof(buf));
   h = (struct rslc_blob_hdr*)buf;
   off = sizeof(*h);
   refl = (struct rslc_reflect*)(buf + off);
   off += sizeof(*refl);
   str = (char*)buf + off;
   strcpy(str, "UBO");
   strcpy(str + 4, "global");
   strcpy(str + 11, "MVP");
   strcpy(str + 15, "Source");
   strcpy(str + 22, "vTexCoord");
   off += 32;
   m = (struct rslc_member*)(buf + off);
   m->name = (uint32_t)(str - (char*)buf) + 11;
   m->offset = 0; m->size = 64; m->base = RSLC_BASE_FLOAT; m->cols = 4; m->rows = 4;
   off += sizeof(*m);
   smp = (struct rslc_sampler*)(buf + off);
   smp->name = (uint32_t)(str - (char*)buf) + 15;
   smp->binding = 1;
   off += sizeof(*smp);
   io = (struct rslc_io*)(buf + off);
   io->name = (uint32_t)(str - (char*)buf) + 22;
   io->location = 0; io->base = RSLC_BASE_FLOAT; io->cols = 1; io->rows = 2;
   off += sizeof(*io);
   off = (off + 3) & ~3u;
   words = (uint32_t*)(buf + off);
   words[0] = 0x07230203u; words[1] = 0x00010000u; words[2] = 0;
   words[3] = 10; words[4] = 0;

   refl->ubo.present      = 1;
   refl->ubo.name         = (uint32_t)(str - (char*)buf);
   refl->ubo.instance     = (uint32_t)(str - (char*)buf) + 4;
   refl->ubo.members      = (uint32_t)((uint8_t*)m - buf);
   refl->ubo.member_count = 1;
   refl->ubo.size         = 64;
   refl->ubo.binding      = 0;
   refl->push.present     = 0;
   refl->samplers         = (uint32_t)((uint8_t*)smp - buf);
   refl->sampler_count    = 1;
   refl->inputs           = (uint32_t)((uint8_t*)io - buf);
   refl->input_count      = 1;
   refl->outputs          = 0;
   refl->output_count     = 0;

   h->magic       = RSLC_BLOB_MAGIC;
   h->version     = RSLC_BLOB_VERSION;
   h->refl_off    = sizeof(*h);
   h->refl_len    = sizeof(*refl);
   h->payload_off = off;
   h->payload_len = 5 * 4;
   h->total_len   = off + 5 * 4;
   h->stage       = RSLC_STAGE_VERTEX;
   h->dst         = RSLC_DST_SPIRV;

   CHECK(rslc_blob_open(buf, sizeof(buf), &r));
   CHECK(r.blob == buf);
   CHECK(r.blob_len == h->total_len);
   CHECK(r.refl == refl);
   CHECK(r.spirv == words);
   CHECK(r.spirv_len == 5);
   CHECK(r.text == NULL);
   CHECK(!strcmp(RSLC_STR(r.blob, r.refl->ubo.instance), "global"));
   CHECK(!strcmp(RSLC_STR(r.blob,
               ((const struct rslc_member*)((const uint8_t*)r.blob
                  + r.refl->ubo.members))[0].name), "MVP"));

   /* Exact length is enough; one byte short is not. */
   CHECK(rslc_blob_open(buf, h->total_len, &r));
   CHECK(!rslc_blob_open(buf, h->total_len - 1, &r));

   h->magic++;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   h->magic--;
   h->version++;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   h->version--;
   h->payload_len++;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   h->payload_len--;
   h->payload_off += 2;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   h->payload_off -= 2;
   refl->sampler_count = RSLC_MAX_SAMPLERS + 1;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   refl->sampler_count = 1;
   refl->inputs = h->total_len;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   refl->inputs = (uint32_t)((uint8_t*)io - buf);
   refl->ubo.member_count = 0x40000000u;
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   refl->ubo.member_count = 1;
   m->name = h->total_len - 2;
   buf[h->total_len - 2] = 'x';
   buf[h->total_len - 1] = 'y';
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   m->name = (uint32_t)(str - (char*)buf) + 11;
   buf[h->total_len - 2] = 0;
   buf[h->total_len - 1] = 0;
   CHECK(rslc_blob_open(buf, sizeof(buf), &r));

   /* A text blob must carry a trailing NUL. */
   h->dst = RSLC_DST_GLSL;
   buf[h->payload_off + h->payload_len - 1] = 'z';
   CHECK(!rslc_blob_open(buf, sizeof(buf), &r));
   buf[h->payload_off + h->payload_len - 1] = 0;
   CHECK(rslc_blob_open(buf, sizeof(buf), &r));
   CHECK(r.text == (const char*)words);
   CHECK(r.text_len == 19);
   CHECK(r.spirv == NULL);

   CHECK(!rslc_blob_open(buf + 1, sizeof(buf) - 1, &r));
   CHECK(!rslc_blob_open(NULL, 0, &r));
}

/* How many instructions of opcode @op a module holds. */
static unsigned spv_count(const uint32_t *w, size_t n, unsigned op)
{
   size_t   at = 5;
   unsigned k  = 0;
   while (at < n && (w[at] >> 16))
   {
      if ((w[at] & 0xffffu) == op)
         k++;
      at += w[at] >> 16;
   }
   return k;
}

/* The shapes glslang gives ?: and && / ||, which the emitter mirrors
 * so a driver's compiler rounds the two modules alike: OpSelect only
 * over variables and constants, OpLogicalAnd only over a trivial
 * right side, control flow otherwise. */
static void test_shapes(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char *srcs[] = {
      "float g(float q) { return q * 2.0; }\n"
      "void main() { float y = 1.0; FragColor = vec4(x > 0.0 ? x : y); }\n",         /* select */
      "void main() { FragColor = vec4(x > 0.0 ? vec3(1.0) : vec3(0.5), 1.0); }\n",   /* select: constants */
      "float g(float q) { return q * 2.0; }\n"
      "void main() { FragColor = vec4(x > 0.0 ? g(x) : 1.0); }\n",                   /* branch: a call */
      "void main() { FragColor = vec4(x > 0.0 ? params.a : 1.0); }\n",               /* branch: a member */
      "void main() { FragColor = vec4(x > 0.0 ? -x : 1.0); }\n",                     /* branch: an operator */
      "void main() { float d = x; FragColor = vec4((x > 0.0 && d > 0.5) ? 1.0 : 0.0); }\n", /* and: comparison of leaves */
      "float g(float q) { return q * 2.0; }\n"
      "void main() { FragColor = vec4((x > 0.0 && g(x) > 0.5) ? 1.0 : 0.0); }\n",    /* short circuit: a call */
      "void main() { FragColor = vec4((x > 0.0 || params.a > 0.5) ? 1.0 : 0.0); }\n" /* short circuit: member in a comparison */
   };
   /* OpSelect 169, OpLogicalAnd 167, OpLogicalOr 166, OpBranchConditional 250 */
   static const unsigned want_select[] = { 1, 1, 0, 0, 0, 1, 1, 1 };
   static const unsigned want_and[]    = { 0, 0, 0, 0, 0, 1, 0, 0 };
   static const unsigned want_or[]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
   static const unsigned want_branch[] = { 0, 0, 1, 1, 1, 0, 1, 1 };
   unsigned i;

   for (i = 0; i < sizeof(srcs) / sizeof(srcs[0]); i++)
   {
      char src[1024];
      strcpy(src, "#version 450\n"
            "layout(push_constant) uniform Push { float a; float b; } params;\n"
            "layout(location = 0) in float x;\n"
            "layout(location = 0) out vec4 FragColor;\n");
      strcat(src, srcs[i]);
      opts_default(&o, err, sizeof(err));
      o.dst = RSLC_DST_SPIRV;
      CHECK(rslc_compile(src, strlen(src), RSLC_STAGE_FRAGMENT, &o, &r));
      if (!r.blob)
      {
         fprintf(stderr, "  case %u: %s\n", i, err);
         continue;
      }
      if (spv_count(r.spirv, r.spirv_len, 169) != want_select[i]
            || spv_count(r.spirv, r.spirv_len, 167) != want_and[i]
            || spv_count(r.spirv, r.spirv_len, 166) != want_or[i]
            || spv_count(r.spirv, r.spirv_len, 250) != want_branch[i])
      {
         fprintf(stderr, "FAIL shapes case %u: select %u and %u or %u branch %u\n", i,
               spv_count(r.spirv, r.spirv_len, 169), spv_count(r.spirv, r.spirv_len, 167),
               spv_count(r.spirv, r.spirv_len, 166), spv_count(r.spirv, r.spirv_len, 250));
         fails++;
      }
      rslc_result_free(&r, &o);
   }
}

/* The CafeGLSL flavour: version 150, both blocks as uniform blocks
 * under their source names, a location on every in and out, samplers
 * under their source names, and the extension GLSL 150 needs for the
 * bit casts. */
static void test_cafe(void)
{
   struct rslc_opts   o;
   struct rslc_result r;
   char               err[128];
   static const char  vert[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float gamma; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n"
      "layout(location = 0) in vec4 Position;\n"
      "layout(location = 1) in vec2 TexCoord;\n"
      "layout(location = 0) out vec2 vTexCoord;\n"
      "void main() { gl_Position = global.MVP * Position; vTexCoord = TexCoord * params.gamma"
      " + intBitsToFloat(int(params.SourceSize.x)); }\n";

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_GLSL_CAFE;
   CHECK(rslc_compile(vert, sizeof(vert) - 1, RSLC_STAGE_VERTEX, &o, &r));
   if (!r.text)
   {
      fprintf(stderr, "  %s\n", err);
      return;
   }
   CHECK(!strncmp(r.text, "#version 150\n", 13));
   CHECK(strstr(r.text, "#extension GL_ARB_shader_bit_encoding : require\n"));
   CHECK(strstr(r.text, "layout(std140) uniform UBO {\n   mat4 MVP;\n} global;\n"));
   CHECK(strstr(r.text, "\nuniform Push {\n   vec4 SourceSize;\n   float gamma;\n} params;\n"));
   CHECK(!strstr(r.text, "struct Push"));
   CHECK(strstr(r.text, "layout(location = 0) in vec4 Position;\n"));
   CHECK(strstr(r.text, "layout(location = 1) in vec2 TexCoord;\n"));
   CHECK(strstr(r.text, "layout(location = 0) out vec2 vTexCoord;\n"));
   CHECK(strstr(r.text, "params.gamma"));
   CHECK(!strstr(r.text, "binding"));
   rslc_result_free(&r, &o);
}

/* The ingest reads a module back to the reflection the GLSL gave,
 * carries the words through for the Vulkan driver, and refuses what
 * the cache must not serve: another stage, a printed destination, a
 * capability beyond Shader, unstructured control flow, a layout this
 * compiler would not have produced. */
static void test_ingest(void)
{
   struct rslc_opts   o, oi;
   struct rslc_result r, ri;
   char               err[128], erri[128];
   static const char  frag[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; float f[3]; mat3 m; uint FrameCount; } params;\n"
      "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; vec2 v2[2]; float x; } global;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 1) flat in int idx;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 3) uniform sampler2D Source;\n"
      "layout(set = 0, binding = 4) uniform sampler2D Other;\n"
      "float g(float a) { return a * global.x; }\n"
      "void main() { if (idx > 0) FragColor = texture(Source, vTexCoord) * params.f[idx] + global.v2[1].xyxy;"
      " else FragColor = vec4(g(params.SourceSize.x)); }\n";
   const uint8_t *b, *bi;
   const struct rslc_member *m, *mi;
   const struct rslc_sampler *s, *si;
   const struct rslc_io *io, *ioi;
   uint32_t *copy;
   size_t    n, i;
   unsigned  k;

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_SPIRV;
   CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
   if (!r.blob)
      return;
   n = r.spirv_len;

   opts_default(&oi, erri, sizeof(erri));
   oi.dst = RSLC_DST_SPIRV;
   CHECK(rslc_compile_spirv(r.spirv, n, RSLC_STAGE_FRAGMENT, &oi, &ri));
   if (!ri.blob)
   {
      fprintf(stderr, "  %s\n", erri);
      rslc_result_free(&r, &o);
      return;
   }
   b  = (const uint8_t*)r.blob;
   bi = (const uint8_t*)ri.blob;
   CHECK(ri.spirv_len == n && !memcmp(ri.spirv, r.spirv, n * 4));
   CHECK(ri.refl->ubo.present && ri.refl->push.present);
   CHECK(!strcmp(RSLC_STR(bi, ri.refl->push.name), "Push") && !strcmp(RSLC_STR(bi, ri.refl->push.instance), "params"));
   CHECK(!strcmp(RSLC_STR(bi, ri.refl->ubo.name), "UBO") && !strcmp(RSLC_STR(bi, ri.refl->ubo.instance), "global"));
   CHECK(ri.refl->ubo.size == r.refl->ubo.size && ri.refl->push.size == r.refl->push.size);
   CHECK(ri.refl->ubo.binding == 0 && ri.refl->push.member_count == 4 && ri.refl->ubo.member_count == 3);
   for (k = 0; k < 2; k++)
   {
      const struct rslc_block *x  = k ? &r.refl->push : &r.refl->ubo;
      const struct rslc_block *xi = k ? &ri.refl->push : &ri.refl->ubo;
      m  = (const struct rslc_member*)(b + x->members);
      mi = (const struct rslc_member*)(bi + xi->members);
      for (i = 0; i < x->member_count; i++)
      {
         CHECK(!strcmp(RSLC_STR(b, m[i].name), RSLC_STR(bi, mi[i].name)));
         CHECK(m[i].offset == mi[i].offset && m[i].size == mi[i].size && m[i].array_len == mi[i].array_len);
         CHECK(m[i].base == mi[i].base && m[i].cols == mi[i].cols && m[i].rows == mi[i].rows);
         CHECK(m[i].active == mi[i].active);
      }
   }
   /* params.SourceSize is read in g(), reached from main; m and FrameCount are not */
   mi = (const struct rslc_member*)(bi + ri.refl->push.members);
   CHECK(mi[0].active && mi[1].active && !mi[2].active && !mi[3].active);
   mi = (const struct rslc_member*)(bi + ri.refl->ubo.members);
   CHECK(!mi[0].active && mi[1].active && mi[2].active);

   CHECK(ri.refl->sampler_count == 2);
   s  = (const struct rslc_sampler*)(b + r.refl->samplers);
   si = (const struct rslc_sampler*)(bi + ri.refl->samplers);
   for (i = 0; i < 2; i++)
      CHECK(!strcmp(RSLC_STR(b, s[i].name), RSLC_STR(bi, si[i].name)) && s[i].binding == si[i].binding
            && s[i].active == si[i].active);
   /* Source is read in main; Other is declared and never read */
   CHECK(si[0].active && !si[1].active);
   CHECK(ri.refl->input_count == 2 && ri.refl->output_count == 1);
   io  = (const struct rslc_io*)(b + r.refl->inputs);
   ioi = (const struct rslc_io*)(bi + ri.refl->inputs);
   for (i = 0; i < 2; i++)
   {
      CHECK(!strcmp(RSLC_STR(b, io[i].name), RSLC_STR(bi, ioi[i].name)));
      CHECK(io[i].location == ioi[i].location && io[i].flat == ioi[i].flat && io[i].rows == ioi[i].rows);
   }
   rslc_result_free(&ri, &oi);

   /* reflection only: no payload */
   oi.dst = RSLC_DST_REFLECT;
   CHECK(rslc_compile_spirv(r.spirv, n, RSLC_STAGE_FRAGMENT, &oi, &ri));
   CHECK(ri.spirv == NULL && ri.refl && ri.refl->sampler_count == 2);
   rslc_result_free(&ri, &oi);

   /* refusals */
   CHECK(!rslc_compile_spirv(r.spirv, n, RSLC_STAGE_VERTEX, &oi, &ri));
   oi.dst = RSLC_DST_HLSL;
   CHECK(!rslc_compile_spirv(r.spirv, n, RSLC_STAGE_FRAGMENT, &oi, &ri));
   oi.dst = RSLC_DST_REFLECT;
   CHECK(!rslc_compile_spirv(r.spirv, 4, RSLC_STAGE_FRAGMENT, &oi, &ri));

   copy = (uint32_t*)malloc(n * 4);
   if (copy)
   {
      /* a capability beyond Shader */
      memcpy(copy, r.spirv, n * 4);
      CHECK((copy[5] & 0xffffu) == 17 && copy[6] == 1);
      copy[6] = 2;   /* Geometry */
      CHECK(!rslc_compile_spirv(copy, n, RSLC_STAGE_FRAGMENT, &oi, &ri));

      /* the selection merge removed: the branch is no longer structured */
      memcpy(copy, r.spirv, n * 4);
      for (i = 5; i < n; i += copy[i] >> 16)
         if ((copy[i] & 0xffffu) == 247)
         {
            copy[i] = (copy[i] & 0xffff0000u);   /* OpNop of the same length */
            break;
         }
      CHECK(i < n);
      CHECK(!rslc_compile_spirv(copy, n, RSLC_STAGE_FRAGMENT, &oi, &ri));

      /* a member offset this compiler would not lay out */
      memcpy(copy, r.spirv, n * 4);
      for (i = 5; i < n; i += copy[i] >> 16)
         if ((copy[i] & 0xffffu) == 72 && copy[i + 3] == 35 && copy[i + 4] == 16)
         {
            copy[i + 4] = 20;
            break;
         }
      CHECK(i < n);
      CHECK(!rslc_compile_spirv(copy, n, RSLC_STAGE_FRAGMENT, &oi, &ri));
      free(copy);
   }
   rslc_result_free(&r, &o);
}

/* rslc_test --pp <file>: preprocess a stage source and print the token
 * stream, for the comparison against glslangValidator -E that the
 * corpus check runs. */
static int pp_file(const char *path)
{
   static char out[4 << 20];
   char        err[256];
   FILE       *f = fopen(path, "rb");
   char       *src;
   long        n;
   bool        ok;

   if (!f)
   {
      fprintf(stderr, "cannot open %s\n", path);
      return 2;
   }
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   src = (char*)malloc((size_t)n + 1);
   if (!src || fread(src, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(src);
      return 2;
   }
   fclose(f);
   src[n] = '\0';
   ok = pp_dump(src, out, sizeof(out), err, sizeof(err));
   free(src);
   if (!ok)
   {
      fprintf(stderr, "%s: %s\n", path, err);
      return 1;
   }
   fputs(out, stdout);
   return 0;
}


/* A compile that runs out of memory - a caller's block too small for
 * the stage, or an allocator that refuses - fails with a message at
 * whatever point it ran out, never reads what it could not make, and
 * returns every allocation.  The stage is long enough for its nodes to
 * span several node chunks, so the refusals land inside expressions,
 * declarations and lists alike.  A block large enough compiles the
 * stage to the same blob as the allocator does, the scratch quarter
 * rewound at every release. */
/* A sampler is active when code main reaches reads it or hands it to
 * a call, as SPIRV-Cross's active interface variables say: every
 * destination's tables and the ingest of the SPIR-V agree. */
static void test_sampler_active(void)
{
   static const char frag[] =
      "#version 450\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "layout(set = 0, binding = 1) uniform sampler2D Read;\n"
      "layout(set = 0, binding = 2) uniform sampler2D Handed;\n"
      "layout(set = 0, binding = 3) uniform sampler2D Uncalled;\n"
      "layout(set = 0, binding = 4) uniform sampler2D Declared;\n"
      "vec4 at(sampler2D s, vec2 uv) { return texture(s, uv); }\n"
      "vec4 never() { return texture(Uncalled, vec2(0.5)); }\n"
      "void main() { FragColor = texture(Read, vTexCoord) + at(Handed, vTexCoord); }\n";
   static const enum rslc_dst dsts[] = {
      RSLC_DST_SPIRV, RSLC_DST_REFLECT, RSLC_DST_GLSL, RSLC_DST_HLSL, RSLC_DST_MSL };
   static const unsigned expect[4] = { 1, 1, 0, 0 };
   struct rslc_opts   o, oi;
   struct rslc_result r, ri;
   char               err[128], erri[128];
   unsigned           d, i;

   for (d = 0; d < sizeof(dsts) / sizeof(dsts[0]); d++)
   {
      const struct rslc_sampler *smp;
      opts_default(&o, err, sizeof(err));
      o.dst = dsts[d];
      CHECK(rslc_compile(frag, sizeof(frag) - 1, RSLC_STAGE_FRAGMENT, &o, &r));
      if (!r.blob)
      {
         fprintf(stderr, "  dst %u: %s\n", (unsigned)dsts[d], err);
         continue;
      }
      CHECK(r.refl->sampler_count == 4);
      smp = (const struct rslc_sampler*)((const uint8_t*)r.blob + r.refl->samplers);
      for (i = 0; i < 4 && i < r.refl->sampler_count; i++)
         if (smp[i].active != expect[i])
         {
            fprintf(stderr, "  dst %u: %s active %u\n", (unsigned)dsts[d],
                  RSLC_STR((const uint8_t*)r.blob, smp[i].name), smp[i].active);
            CHECK(0);
         }
      if (dsts[d] == RSLC_DST_SPIRV)
      {
         opts_default(&oi, erri, sizeof(erri));
         oi.dst = RSLC_DST_REFLECT;
         CHECK(rslc_compile_spirv(r.spirv, r.spirv_len, RSLC_STAGE_FRAGMENT, &oi, &ri));
         if (ri.blob)
         {
            smp = (const struct rslc_sampler*)((const uint8_t*)ri.blob + ri.refl->samplers);
            CHECK(ri.refl->sampler_count == 4);
            for (i = 0; i < 4 && i < ri.refl->sampler_count; i++)
               CHECK(smp[i].active == expect[i]);
            rslc_result_free(&ri, &oi);
         }
      }
      rslc_result_free(&r, &o);
   }
}

static void test_exhaustion(void)
{
   struct rslc_opts   o;
   struct rslc_result r, ref;
   char               err[128];
   char              *src;
   unsigned char     *block;
   size_t             len = 0, cap = 160 * 1024, at;
   size_t             block_len = 4u * 1024u * 1024u;
   size_t             held;
   unsigned           i, failed = 0, calls;
   static const char  head[] =
      "#version 450\n"
      "layout(push_constant) uniform Push { vec4 SourceSize; } params;\n"
      "layout(location = 0) in vec2 vTexCoord;\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "const float K[4] = float[](0.25, 0.5, 0.75, 1.0);\n"
      "float f(vec2 v) {\n"
      "  return 0.0";
   static const char  tail[] =
      ";\n}\n"
      "void main() {\n"
      "  vec2 v = vTexCoord * params.SourceSize.xy;\n"
      "  FragColor = vec4(f(v), K[1] * 2.0 + 1.0, vec2(K[2], K[3]));\n"
      "}\n";

   if (!(src = (char*)malloc(cap)) || !(block = (unsigned char*)malloc(block_len)))
   {
      free(src);
      CHECK(0);
      return;
   }
   memcpy(src, head, sizeof(head) - 1);
   len = sizeof(head) - 1;
   for (i = 0; i < 2600; i++)
      len += (size_t)sprintf(src + len, " + v.%c * %u.0 / (1.0 + %u.0)",
            (i & 1) ? 'y' : 'x', i % 97, i % 13);
   memcpy(src + len, tail, sizeof(tail));
   len += sizeof(tail) - 1;

   opts_default(&o, err, sizeof(err));
   o.dst = RSLC_DST_SPIRV;
   CHECK(rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &ref));

   /* a block it fits in, dirty from the start: the same module */
   memset(block, 0xa5, block_len);
   o.arena     = block;
   o.arena_len = block_len;
   CHECK(rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &r));
   if (ref.blob && r.blob)
   {
      CHECK(r.blob_len == ref.blob_len);
      CHECK(!memcmp(r.blob, ref.blob, ref.blob_len));
      rslc_result_free(&r, &o);
   }

   /* blocks too small, by steps through the whole compile */
   for (at = 8192; at < block_len; at += 4096)
   {
      memset(block, 0xa5, at);
      o.arena_len = at;
      err[0]      = '\0';
      if (rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &r))
      {
         rslc_result_free(&r, &o);
         break;
      }
      CHECK(!strcmp(err, "rslc: caller arena exhausted"));
      failed++;
   }
   CHECK(at < block_len && failed > 32);
   o.arena     = NULL;
   o.arena_len = 0;

   /* an allocator refusing its nth call, for every n the compile
    * makes: nothing kept but the reference module */
   held        = alloc_live;
   alloc_calls = 0;
   CHECK(rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &r));
   calls = (unsigned)alloc_calls;
   if (r.blob)
      rslc_result_free(&r, &o);
   for (i = 0; i < calls; i++)
   {
      alloc_fail_at = (long)i;
      alloc_calls   = 0;
      CHECK(!rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &r));
      CHECK(alloc_live == held);
   }
   alloc_fail_at = -1;

   if (ref.blob)
      rslc_result_free(&ref, &o);
   CHECK(alloc_live == 0);
   free(block);
   free(src);
}

int main(int argc, char **argv)
{
   if (argc == 3 && !strcmp(argv[1], "--pp"))
      return pp_file(argv[2]);
   if (argc == 3 && !strcmp(argv[1], "--front"))
      return front_file(argv[2], 0);
   if (argc == 3 && !strcmp(argv[1], "--ir"))
      return front_file(argv[2], 1);
   if (argc == 4 && !strcmp(argv[1], "--spirv"))
      return spirv_file(argv[2], argv[3]);
   if ((argc == 4 || argc == 5) && !strcmp(argv[1], "--glsl"))
      return glsl_file(argv[2], (unsigned)atoi(argv[3]), false,
            argc == 5 && !strcmp(argv[4], "es") ? RSLC_DST_GLSL_ES : RSLC_DST_GLSL);
   if (argc == 4 && !strcmp(argv[1], "--hlsl"))
      return glsl_file(argv[2], (unsigned)atoi(argv[3]), false, RSLC_DST_HLSL);
   if (argc == 3 && !strcmp(argv[1], "--msl"))
      return glsl_file(argv[2], 20000, false, RSLC_DST_MSL);
   if (argc == 3 && !strcmp(argv[1], "--cafe"))
      return glsl_file(argv[2], 150, false, RSLC_DST_GLSL_CAFE);

   test_abi_rejects();
   test_arena();
   test_intern();
   test_pbuf();
   test_pp();
   test_front();
   test_blob();
   test_spirv();
   test_folding();
   test_glsl();
   test_glsl_reserved();
   test_glsl_flatten();
   test_hlsl();
   test_hlsl_scoping();
   test_msl();
   test_msl_apple();
   test_msl_powr();
   test_msl_inline();
   test_shapes();
   test_cafe();
   test_ingest();
   test_sampler_active();
   test_exhaustion();

   if (fails)
   {
      printf("rslc_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_test: ok\n");
   return 0;
}
