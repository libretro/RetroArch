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

static void *count_alloc(void *ud, size_t len)
{
   size_t *p;
   (void)ud;
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
   CHECK(!rslc_compile(src, sizeof(src) - 1, RSLC_STAGE_VERTEX, &o, &r));
   CHECK(!strcmp(err, "rslc: parser not implemented"));
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
   CHECK(!strcmp(err, "rslc: SPIR-V ingest not implemented"));
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

int main(int argc, char **argv)
{
   if (argc == 3 && !strcmp(argv[1], "--pp"))
      return pp_file(argv[2]);

   test_abi_rejects();
   test_arena();
   test_intern();
   test_pbuf();
   test_pp();
   test_blob();

   if (fails)
   {
      printf("rslc_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rslc_test: ok\n");
   return 0;
}
