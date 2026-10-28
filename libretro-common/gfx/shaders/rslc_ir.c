/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_ir.c).
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

/* rslc TU 2: shared memory primitives, the blob format, SPIR-V in/out.
 *
 * This unit is present in every build that has rslc at all, so the
 * arena, intern table, print buffer and error sink live here and the
 * other two units call them. */

#include <stdlib.h>
#include <string.h>
#ifdef RSLC_SPV_TRACE
#include <stdio.h>
#endif

#include "rslc_share.h"

#define RSLC_INTERN_INITIAL 256u /* slots; power of two */

/* Allocator wrappers */

void *rslc_mem_alloc(const struct rslc_opts *opts, size_t len)
{
   if (opts->alloc)
      return opts->alloc(opts->ud, len);
   return malloc(len);
}

void rslc_mem_free(const struct rslc_opts *opts, void *ptr)
{
   if (!ptr)
      return;
   if (opts->free_fn)
      opts->free_fn(opts->ud, ptr);
   else
      free(ptr);
}

/* Error sink */

static size_t rslc_err_copy(char *dst, size_t cap, size_t at, const char *s)
{
   while (*s && at + 1 < cap)
      dst[at++] = *s++;
   return at;
}

void rslc_err(struct rslc_ctx *c, const char *a, const char *b, unsigned line)
{
   char  *dst;
   size_t cap;
   size_t at;

   if (c->failed)
      return;
   c->failed = true;

   dst = c->opts->err;
   cap = c->opts->err_cap;
   if (!dst || !cap)
      return;

   at = rslc_err_copy(dst, cap, 0, "rslc: ");
   if (a)
      at = rslc_err_copy(dst, cap, at, a);
   if (b)
      at = rslc_err_copy(dst, cap, at, b);
   if (line)
   {
      char        num[12];
      unsigned    n    = sizeof(num) - 1;
      const char *file = c->err_file < c->nfiles ? c->files[c->err_file] : NULL;

      num[n] = '\0';
      do
      {
         num[--n] = (char)('0' + (line % 10u));
         line    /= 10u;
      } while (line && n);
      if (file && file[0])
      {
         at = rslc_err_copy(dst, cap, at, " (");
         at = rslc_err_copy(dst, cap, at, file);
         at = rslc_err_copy(dst, cap, at, ":");
      }
      else
         at = rslc_err_copy(dst, cap, at, " (line ");
      at = rslc_err_copy(dst, cap, at, num + n);
      at = rslc_err_copy(dst, cap, at, ")");
   }
   dst[at] = '\0';
}

/* Arena */

/* Both arenas draw on one cap: held counts bytes taken from the caller
 * across them. */
static bool rslc_arena_grow(struct rslc_ctx *c, struct rslc_arena *a,
      size_t need)
{
   struct rslc_chunk *k;
   size_t             len;

   if (a->fixed)
   {
      rslc_err(c, "caller arena exhausted", NULL, 0);
      return false;
   }

   len = RSLC_ARENA_CHUNK;
   if (need + sizeof(*k) > len)
      len = need + sizeof(*k);

   /* A released scratch chunk of enough size is reused first. */
   if (a->spare)
   {
      struct rslc_chunk **pk = &a->spare;
      while (*pk)
      {
         if ((*pk)->len >= need)
         {
            k   = *pk;
            *pk = k->next;
            k->next   = a->chunks;
            a->chunks = k;
            a->cur    = (uint8_t*)(k + 1);
            a->end    = a->cur + k->len;
            return true;
         }
         pk = &(*pk)->next;
      }
   }

   if (c->arena.held + c->scratch.held + len > c->arena.cap)
   {
      rslc_err(c, "arena cap exceeded", NULL, 0);
      return false;
   }

   k = (struct rslc_chunk*)rslc_mem_alloc(c->opts, len);
   if (!k)
   {
      rslc_err(c, "out of memory", NULL, 0);
      return false;
   }
   k->next   = a->chunks;
   k->len    = len - sizeof(*k);
   a->chunks = k;
   a->cur    = (uint8_t*)(k + 1);
   a->end    = a->cur + k->len;
   a->held  += len;
   return true;
}

static void *rslc_arena_take(struct rslc_ctx *c, struct rslc_arena *a,
      size_t len)
{
   void *p;

   if (c->failed)
      return NULL;
   len = (len + (RSLC_ARENA_ALIGN - 1)) & ~(size_t)(RSLC_ARENA_ALIGN - 1);
   if ((size_t)(a->end - a->cur) < len)
      if (!rslc_arena_grow(c, a, len))
         return NULL;
   p        = a->cur;
   a->cur  += len;
   a->used += len;
   return p;
}

void *rslc_arena_alloc(struct rslc_ctx *c, size_t len)
{
   return rslc_arena_take(c, &c->arena, len);
}

void *rslc_arena_zalloc(struct rslc_ctx *c, size_t len)
{
   void *p = rslc_arena_alloc(c, len);
   if (p)
      memset(p, 0, len);
   return p;
}

void *rslc_scratch_alloc(struct rslc_ctx *c, size_t len)
{
   return rslc_arena_take(c, &c->scratch, len);
}

void rslc_scratch_mark(struct rslc_ctx *c, struct rslc_mark *m)
{
   m->chunk = c->scratch.chunks;
   m->cur   = c->scratch.cur;
   m->used  = c->scratch.used;
}

/* Chunks taken since the mark go to the spare list; the marked chunk
 * is rewound.  Nothing is returned to the allocator until close. */
void rslc_scratch_release(struct rslc_ctx *c, const struct rslc_mark *m)
{
   struct rslc_arena *a = &c->scratch;
   /* a caller's block has no chunks: the mark's point in it is all */
   if (a->fixed)
   {
      a->cur  = m->cur;
      a->used = m->used;
      return;
   }
   while (a->chunks && a->chunks != m->chunk)
   {
      struct rslc_chunk *k = a->chunks;
      a->chunks = k->next;
      k->next   = a->spare;
      a->spare  = k;
   }
   if (a->chunks)
   {
      a->cur = m->cur;
      a->end = (uint8_t*)(a->chunks + 1) + a->chunks->len;
   }
   else
   {
      a->cur = a->end = NULL;
   }
   a->used = m->used;
}

static void rslc_arena_free_all(struct rslc_ctx *c, struct rslc_arena *a)
{
   struct rslc_chunk *k;
   if (a->fixed)
      return;
   k = a->chunks;
   while (k)
   {
      struct rslc_chunk *next = k->next;
      rslc_mem_free(c->opts, k);
      k = next;
   }
   k = a->spare;
   while (k)
   {
      struct rslc_chunk *next = k->next;
      rslc_mem_free(c->opts, k);
      k = next;
   }
   a->chunks = a->spare = NULL;
   a->cur    = a->end   = NULL;
}

/* Token text and symbol */

const char *rslc_tok_text(const struct rslc_ctx *c, const struct rslc_tok *t)
{
   if (t->kind == RSLC_TK_IDENT)
      return c->syms[t->ref]->str;
   if (t->flags & RSLC_TF_SYNTH)
      return c->synth + t->ref;
   return c->src + t->ref;
}

struct rslc_sym *rslc_tok_sym(const struct rslc_ctx *c, const struct rslc_tok *t)
{
   return t->kind == RSLC_TK_IDENT ? c->syms[t->ref] : NULL;
}

/* Intern */

uint32_t rslc_hash(const char *s, size_t len)
{
   /* FNV-1a */
   uint32_t h = 2166136261u;
   size_t   i;
   for (i = 0; i < len; i++)
   {
      h ^= (uint8_t)s[i];
      h *= 16777619u;
   }
   return h;
}

static bool rslc_intern_rehash(struct rslc_ctx *c)
{
   struct rslc_intern *t    = &c->intern;
   uint32_t            n    = t->mask ? (t->mask + 1) * 2 : RSLC_INTERN_INITIAL;
   struct rslc_sym   **old  = t->slots;
   uint32_t            oldn = t->mask ? t->mask + 1 : 0;
   struct rslc_sym   **nw;
   uint32_t            i;

   nw = (struct rslc_sym**)rslc_arena_zalloc(c, n * sizeof(*nw));
   if (!nw)
      return false;
   for (i = 0; i < oldn; i++)
   {
      uint32_t j;
      if (!old[i])
         continue;
      j = old[i]->hash & (n - 1);
      while (nw[j])
         j = (j + 1) & (n - 1);
      nw[j] = old[i];
   }
   t->slots = nw;
   t->mask  = n - 1;
   return true;
}

struct rslc_sym *rslc_sym(struct rslc_ctx *c, const char *s, size_t len)
{
   struct rslc_intern *t = &c->intern;
   uint32_t            h = rslc_hash(s, len);
   uint32_t            j;
   char               *copy;
   struct rslc_sym    *e;

   if (c->failed)
      return NULL;
   if (!t->slots || (t->count + 1) * 4 > (t->mask + 1) * 3)
      if (!rslc_intern_rehash(c))
         return NULL;

   j = h & t->mask;
   for (;;)
   {
      e = t->slots[j];
      if (!e)
         break;
      if (e->hash == h && e->len == len && !memcmp(e->str, s, len))
         return e;
      j = (j + 1) & t->mask;
   }

   if (c->nsyms == c->syms_cap)
   {
      uint32_t          ncap = c->syms_cap ? c->syms_cap * 2 : 512;
      struct rslc_sym **nv   = (struct rslc_sym**)rslc_arena_alloc(c,
            ncap * sizeof(*nv));
      if (!nv)
         return NULL;
      if (c->nsyms)
         memcpy(nv, c->syms, c->nsyms * sizeof(*nv));
      c->syms     = nv;
      c->syms_cap = ncap;
   }
   e = (struct rslc_sym*)rslc_arena_zalloc(c, sizeof(*e));
   if (!e)
      return NULL;
   copy = (char*)rslc_arena_alloc(c, len + 1);
   if (!copy)
      return NULL;
   memcpy(copy, s, len);
   copy[len]   = '\0';
   e->str      = copy;
   e->hash     = h;
   e->len      = (uint32_t)len;
   e->index    = c->nsyms;
   c->syms[c->nsyms++] = e;
   t->slots[j] = e;
   t->count++;
   return e;
}

const char *rslc_intern(struct rslc_ctx *c, const char *s, size_t len)
{
   struct rslc_sym *e = rslc_sym(c, s, len);
   return e ? e->str : NULL;
}

const char *rslc_intern_cstr(struct rslc_ctx *c, const char *s)
{
   return rslc_intern(c, s, strlen(s));
}

/* Keywords, in enum rslc_kw order from RSLC_KW_CONST. */
static const char *const rslc_keywords[] = {
   "const", "in", "out", "inout", "uniform",
   "flat", "smooth", "noperspective", "centroid",
   "sample", "invariant", "precise", "layout",
   "precision", "lowp", "mediump", "highp",
   "struct",
   "if", "else", "for", "while", "do",
   "switch", "case", "default", "break",
   "continue", "return", "discard",
   "true", "false",
   "void", "bool", "int", "uint", "float",
   "bvec2", "bvec3", "bvec4",
   "ivec2", "ivec3", "ivec4",
   "uvec2", "uvec3", "uvec4",
   "vec2", "vec3", "vec4",
   "mat2", "mat3", "mat4",
   "mat2x2", "mat2x3", "mat2x4",
   "mat3x2", "mat3x3", "mat3x4",
   "mat4x2", "mat4x3", "mat4x4",
   "sampler2D", "isampler2D", "usampler2D"
};

static bool rslc_seed_keywords(struct rslc_ctx *c)
{
   unsigned i;
   for (i = 0; i < sizeof(rslc_keywords) / sizeof(rslc_keywords[0]); i++)
   {
      struct rslc_sym *e = rslc_sym(c, rslc_keywords[i],
            strlen(rslc_keywords[i]));
      if (!e)
         return false;
      e->kw = (uint16_t)(RSLC_KW_CONST + i);
   }
   return true;
}

/* Print buffer */

bool rslc_pbuf_init(struct rslc_ctx *c, struct rslc_pbuf *b,
      size_t prefix, size_t hint)
{
   size_t cap = prefix + hint;
   if (cap < 256)
      cap = 256;
   b->grows = 0;
   b->data  = (uint8_t*)rslc_mem_alloc(c->opts, cap);
   if (!b->data)
   {
      b->len = b->cap = 0;
      rslc_err(c, "out of memory", NULL, 0);
      return false;
   }
   b->cap = cap;
   b->len = prefix;
   if (prefix)
      memset(b->data, 0, prefix);
   return true;
}

bool rslc_pbuf_reserve(struct rslc_ctx *c, struct rslc_pbuf *b, size_t extra)
{
   size_t   need = b->len + extra;
   size_t   cap;
   uint8_t *nw;

   if (need <= b->cap)
      return true;
   cap = b->cap ? b->cap : 256;
   while (cap < need)
      cap *= 2;
   b->grows++;
   nw = (uint8_t*)rslc_mem_alloc(c->opts, cap);
   if (!nw)
   {
      rslc_err(c, "out of memory", NULL, 0);
      return false;
   }
   if (b->len)
      memcpy(nw, b->data, b->len);
   rslc_mem_free(c->opts, b->data);
   b->data = nw;
   b->cap  = cap;
   return true;
}

bool rslc_pbuf_put(struct rslc_ctx *c, struct rslc_pbuf *b,
      const void *src, size_t len)
{
   if (!rslc_pbuf_reserve(c, b, len))
      return false;
   memcpy(b->data + b->len, src, len);
   b->len += len;
   return true;
}

bool rslc_pbuf_puts(struct rslc_ctx *c, struct rslc_pbuf *b, const char *s)
{
   return rslc_pbuf_put(c, b, s, strlen(s));
}

bool rslc_pbuf_putc(struct rslc_ctx *c, struct rslc_pbuf *b, char ch)
{
   if (!rslc_pbuf_reserve(c, b, 1))
      return false;
   b->data[b->len++] = (uint8_t)ch;
   return true;
}

bool rslc_pbuf_putu(struct rslc_ctx *c, struct rslc_pbuf *b, uint32_t v)
{
   char     num[11];
   unsigned n = sizeof(num);

   do
   {
      num[--n] = (char)('0' + (v % 10u));
      v       /= 10u;
   } while (v);
   return rslc_pbuf_put(c, b, num + n, sizeof(num) - n);
}

void rslc_pbuf_free(struct rslc_ctx *c, struct rslc_pbuf *b)
{
   rslc_mem_free(c->opts, b->data);
   b->data = NULL;
   b->len  = b->cap = 0;
}

/* Context */

static uint32_t rslc_list_empty[1];

bool rslc_ctx_init(struct rslc_ctx *c, const struct rslc_opts *opts,
      enum rslc_stage stage)
{
   memset(c, 0, sizeof(*c));
   c->opts  = opts;
   c->stage = stage;
   /* list 0, the empty list, reads as empty before any list is made */
   c->lists = rslc_list_empty;

   if ((opts->alloc != NULL) != (opts->free_fn != NULL))
   {
      rslc_err(c, "alloc and free_fn must both be set or both unset", NULL, 0);
      return false;
   }
   if (stage != RSLC_STAGE_VERTEX && stage != RSLC_STAGE_FRAGMENT)
   {
      rslc_err(c, "unknown stage", NULL, 0);
      return false;
   }
   if (opts->dst > RSLC_DST_REFLECT)
   {
      rslc_err(c, "unknown destination", NULL, 0);
      return false;
   }

   if (opts->arena)
   {
      /* The caller's block is split: the top quarter is scratch. */
      uintptr_t p   = (uintptr_t)opts->arena;
      uintptr_t pad = (RSLC_ARENA_ALIGN - (p & (RSLC_ARENA_ALIGN - 1)))
         & (RSLC_ARENA_ALIGN - 1);
      size_t    main_len;
      if (opts->arena_len <= pad + 4096)
      {
         rslc_err(c, "caller arena too small", NULL, 0);
         return false;
      }
      main_len = ((opts->arena_len - pad) / 4 * 3) & ~(size_t)(RSLC_ARENA_ALIGN - 1);
      c->arena.fixed   = true;
      c->arena.cur     = (uint8_t*)opts->arena + pad;
      c->arena.end     = c->arena.cur + main_len;
      c->arena.cap     = opts->arena_len;
      c->scratch.fixed = true;
      c->scratch.cur   = c->arena.end;
      c->scratch.end   = (uint8_t*)opts->arena + opts->arena_len;
      c->scratch.cap   = opts->arena_len;
   }
   else
   {
      c->arena.cap   = opts->arena_cap ? opts->arena_cap : RSLC_ARENA_CAP_DEFAULT;
      c->scratch.cap = c->arena.cap;
   }

   /* Seed the intern table with the keywords; the first rehash happens
    * here, not on the first identifier. */
   if (!rslc_intern_rehash(c))
      return false;
   return rslc_seed_keywords(c);
}

void rslc_ctx_close(struct rslc_ctx *c)
{
   rslc_arena_free_all(c, &c->arena);
   rslc_arena_free_all(c, &c->scratch);
}

/* IR */

static struct rslc_type *rslc_type_find(struct rslc_ctx *c, enum rslc_tkind kind,
      enum rslc_base base, unsigned rows, unsigned cols,
      const struct rslc_type *elem, uint32_t array_len)
{
   struct rslc_type *t;
   for (t = c->type_chain; t; t = t->next)
      if (t->kind == kind && t->base == base && t->rows == rows
            && t->cols == cols && t->elem == elem && t->array_len == array_len
            && t->kind != RSLC_TY_STRUCT)
         return t;
   return NULL;
}

static struct rslc_type *rslc_type_add(struct rslc_ctx *c, enum rslc_tkind kind,
      enum rslc_base base, unsigned rows, unsigned cols,
      const struct rslc_type *elem, uint32_t array_len)
{
   struct rslc_type *t = (struct rslc_type*)rslc_arena_zalloc(c, sizeof(*t));
   if (!t)
      return NULL;
   t->kind      = (uint8_t)kind;
   t->base      = (uint8_t)base;
   t->rows      = (uint8_t)rows;
   t->cols      = (uint8_t)cols;
   t->elem      = elem;
   t->array_len = array_len;
   t->id        = ++c->ntypes;
   t->next      = c->type_chain;
   c->type_chain = t;
   return t;
}

const struct rslc_type *rslc_type_basic(struct rslc_ctx *c, enum rslc_tkind kind,
      enum rslc_base base, unsigned rows, unsigned cols)
{
   struct rslc_type *t = rslc_type_find(c, kind, base, rows, cols, NULL, 0);
   if (t)
      return t;
   return rslc_type_add(c, kind, base, rows, cols, NULL, 0);
}

const struct rslc_type *rslc_type_sampler2d(struct rslc_ctx *c, enum rslc_base base)
{
   return rslc_type_basic(c, RSLC_TY_SAMPLER2D, base, 1, 1);
}

const struct rslc_type *rslc_type_array(struct rslc_ctx *c,
      const struct rslc_type *elem, uint32_t len)
{
   struct rslc_type *t;
   if (!elem)
      return NULL;
   t = rslc_type_find(c, RSLC_TY_ARRAY, RSLC_BASE_VOID, 0, 0, elem, len);
   if (t)
      return t;
   return rslc_type_add(c, RSLC_TY_ARRAY, RSLC_BASE_VOID, 0, 0, elem, len);
}

struct rslc_type *rslc_type_struct(struct rslc_ctx *c, struct rslc_sym *name,
      const struct rslc_field *fields, uint32_t nfields)
{
   struct rslc_type *t = rslc_type_add(c, RSLC_TY_STRUCT, RSLC_BASE_VOID, 0, 0, NULL, 0);
   if (!t)
      return NULL;
   t->name    = name;
   t->fields  = fields;
   t->nfields = nfields;
   return t;
}

uint32_t rslc_node_new(struct rslc_ctx *c, enum rslc_op op,
      const struct rslc_type *type, uint32_t a, uint32_t b, uint32_t c_,
      uint16_t aux, uint32_t line)
{
   uint32_t          chunk;
   struct rslc_node *n;

   if (c->nnodes == 0)
      c->nnodes = 1;   /* node 0 is the sentinel */
   chunk = c->nnodes >> RSLC_NODE_CHUNK_SHIFT;
   if (chunk == c->nnode_chunks)
   {
      struct rslc_node *nc;
      if ((chunk & (chunk + 1)) == 0 || !c->node_chunks)
      {
         uint32_t           ncap = chunk ? (chunk + 1) * 2 : 1;
         struct rslc_node **nt   = (struct rslc_node**)rslc_arena_alloc(c,
               ncap * sizeof(*nt));
         if (!nt)
            return 0;
         if (chunk)
            memcpy(nt, c->node_chunks, chunk * sizeof(*nt));
         c->node_chunks = nt;
      }
      nc = (struct rslc_node*)rslc_arena_zalloc(c, RSLC_NODE_CHUNK * sizeof(*nc));
      if (!nc)
         return 0;
      c->node_chunks[chunk] = nc;
      c->nnode_chunks++;
   }
   n       = rslc_node_at(c, c->nnodes);
   n->type = type;
   n->a    = a;
   n->b    = b;
   n->c    = c_;
   n->line = line;
   n->op   = (uint16_t)op;
   n->aux  = aux;
   return c->nnodes++;
}

uint32_t rslc_list_new(struct rslc_ctx *c, const uint32_t *items, uint32_t n)
{
   uint32_t id;
   if (c->nlists == 0)
      c->nlists = 1;   /* list 0 is the sentinel (empty) */
   if (c->nlists + n + 1 > c->lists_cap)
   {
      uint32_t  ncap = c->lists_cap ? c->lists_cap * 2 : 1024;
      uint32_t *nv;
      while (ncap < c->nlists + n + 1)
         ncap *= 2;
      nv = (uint32_t*)rslc_arena_alloc(c, ncap * sizeof(*nv));
      if (!nv)
         return 0;
      if (c->lists)
         memcpy(nv, c->lists, c->nlists * sizeof(*nv));
      else
         nv[0] = 0;
      c->lists     = nv;
      c->lists_cap = ncap;
   }
   id = c->nlists;
   c->lists[id] = n;
   if (n)
      memcpy(c->lists + id + 1, items, n * sizeof(*items));
   c->nlists += n + 1;
   return id;
}

/* Blob */

static bool rslc_blob_range_ok(uint32_t off, uint32_t count,
      uint32_t elem, uint32_t total)
{
   uint32_t len = count * elem;
   if (elem && count > total / elem)
      return false;
   if (off > total || len > total - off)
      return false;
   return true;
}

static bool rslc_blob_str_ok(const uint8_t *blob, uint32_t total, uint32_t off)
{
   if (off >= total)
      return false;
   return memchr(blob + off, 0, total - off) != NULL;
}

static bool rslc_blob_block_ok(const uint8_t *blob, uint32_t total,
      const struct rslc_block *b)
{
   uint32_t i;
   const struct rslc_member *m;

   if (!b->present)
      return true;
   if (!rslc_blob_range_ok(b->members, b->member_count,
            sizeof(struct rslc_member), total))
      return false;
   if (b->name && !rslc_blob_str_ok(blob, total, b->name))
      return false;
   if (b->instance && !rslc_blob_str_ok(blob, total, b->instance))
      return false;
   m = (const struct rslc_member*)(blob + b->members);
   for (i = 0; i < b->member_count; i++)
      if (!rslc_blob_str_ok(blob, total, m[i].name))
         return false;
   return true;
}

bool rslc_blob_open(void *blob, size_t len, struct rslc_result *result)
{
   const uint8_t              *bytes = (const uint8_t*)blob;
   const struct rslc_blob_hdr *h     = (const struct rslc_blob_hdr*)blob;
   const struct rslc_reflect  *r;
   uint32_t                    i;

   if (!blob || len < sizeof(*h) || ((uintptr_t)blob & 3))
      return false;
   if (     h->magic   != RSLC_BLOB_MAGIC
         || h->version != RSLC_BLOB_VERSION
         || h->total_len > len
         || h->total_len < sizeof(*h))
      return false;
   if (     h->refl_off < sizeof(*h)
         || h->refl_len < sizeof(*r)
         || !rslc_blob_range_ok(h->refl_off, 1, h->refl_len, h->total_len))
      return false;
   if (     (h->payload_off & 3)
         || !rslc_blob_range_ok(h->payload_off, 1, h->payload_len, h->total_len))
      return false;
   if (h->stage > RSLC_STAGE_FRAGMENT || h->dst > RSLC_DST_REFLECT)
      return false;

   r = (const struct rslc_reflect*)(bytes + h->refl_off);
   if (     !rslc_blob_block_ok(bytes, h->total_len, &r->ubo)
         || !rslc_blob_block_ok(bytes, h->total_len, &r->push)
         || !rslc_blob_range_ok(r->samplers, r->sampler_count,
            sizeof(struct rslc_sampler), h->total_len)
         || !rslc_blob_range_ok(r->inputs, r->input_count,
            sizeof(struct rslc_io), h->total_len)
         || !rslc_blob_range_ok(r->outputs, r->output_count,
            sizeof(struct rslc_io), h->total_len))
      return false;
   if (r->sampler_count > RSLC_MAX_SAMPLERS)
      return false;
   for (i = 0; i < r->sampler_count; i++)
      if (!rslc_blob_str_ok(bytes, h->total_len,
               ((const struct rslc_sampler*)(bytes + r->samplers))[i].name))
         return false;
   for (i = 0; i < r->input_count; i++)
      if (!rslc_blob_str_ok(bytes, h->total_len,
               ((const struct rslc_io*)(bytes + r->inputs))[i].name))
         return false;
   for (i = 0; i < r->output_count; i++)
      if (!rslc_blob_str_ok(bytes, h->total_len,
               ((const struct rslc_io*)(bytes + r->outputs))[i].name))
         return false;

   memset(result, 0, sizeof(*result));
   result->blob     = blob;
   result->blob_len = h->total_len;
   result->refl     = r;
   switch (h->dst)
   {
      case RSLC_DST_SPIRV:
         if (h->payload_len & 3)
            return false;
         result->spirv     = (const uint32_t*)(bytes + h->payload_off);
         result->spirv_len = h->payload_len / 4;
         break;
      case RSLC_DST_REFLECT:
         break;
      default:
         if (!h->payload_len || bytes[h->payload_off + h->payload_len - 1])
            return false;
         result->text     = (const char*)(bytes + h->payload_off);
         result->text_len = h->payload_len - 1;
         break;
   }
   return true;
}

void rslc_result_free(struct rslc_result *result, const struct rslc_opts *opts)
{
   if (!result)
      return;
   rslc_mem_free(opts, result->blob);
   memset(result, 0, sizeof(*result));
}

/* Destinations */

/* ------------------------------------------------------------------ */
/* Layout (std140 / std430) and reflection                              */
/* ------------------------------------------------------------------ */

/* Base alignment and size of a type under a block layout.  std140
 * rounds array strides and struct alignment up to 16; std430 does
 * not.  Matrices are arrays of column vectors. */
static void rslc_layout_of(const struct rslc_type *t, bool std140,
      uint32_t *align, uint32_t *size, uint32_t *stride)
{
   uint32_t a = 4, s = 4, st = 0;
   switch (t->kind)
   {
      case RSLC_TY_SCALAR:
         a = s = 4;
         break;
      case RSLC_TY_VEC:
         a = t->rows == 2 ? 8 : 16;
         s = 4 * t->rows;
         break;
      case RSLC_TY_MAT:
         a = t->rows == 2 ? 8 : 16;
         if (std140 && a < 16)
            a = 16;
         st = a;
         s  = st * t->cols;
         break;
      case RSLC_TY_ARRAY:
      {
         uint32_t ea, es, est;
         rslc_layout_of(t->elem, std140, &ea, &es, &est);
         if (std140 && ea < 16)
            ea = 16;
         st = (es + ea - 1) / ea * ea;
         a  = ea;
         s  = st * t->array_len;
         break;
      }
      case RSLC_TY_STRUCT:
      {
         uint32_t i, off = 0, maxa = 1;
         for (i = 0; i < t->nfields; i++)
         {
            uint32_t fa, fs, fst;
            rslc_layout_of(t->fields[i].type, std140, &fa, &fs, &fst);
            off  = (off + fa - 1) / fa * fa;
            off += fs;
            if (fa > maxa)
               maxa = fa;
         }
         if (std140 && maxa < 16)
            maxa = 16;
         a = maxa;
         s = (off + a - 1) / a * a;
         break;
      }
      default:
         break;
   }
   *align  = a;
   *size   = s;
   *stride = st;
}

void rslc_layout_of_type(const struct rslc_type *t, bool std140,
      uint32_t *align, uint32_t *size, uint32_t *stride)
{
   rslc_layout_of(t, std140, align, size, stride);
}

/* Offset of field i of a struct under a layout. */
uint32_t rslc_layout_field_offset(const struct rslc_type *t, uint32_t field,
      bool std140)
{
   uint32_t i, off = 0;
   for (i = 0; i <= field; i++)
   {
      uint32_t fa, fs, fst;
      rslc_layout_of(t->fields[i].type, std140, &fa, &fs, &fst);
      off = (off + fa - 1) / fa * fa;
      if (i == field)
         return off;
      off += fs;
   }
   return off;
}

/* ------------------------------------------------------------------ */
/* SPIR-V emitter                                                      */
/* ------------------------------------------------------------------ */

/* Word buffers for the module's sections; concatenated at the end in
 * the order the spec requires. */
struct rslc_wbuf
{
   uint32_t *v;
   uint32_t  n;
   uint32_t  cap;
};

enum spv_section
{
   SPV_SEC_CAPS = 0,   /* OpCapability, OpExtInstImport, OpMemoryModel */
   SPV_SEC_ENTRY,      /* OpEntryPoint, OpExecutionMode */
   SPV_SEC_DEBUG,      /* OpSource, OpName, OpMemberName */
   SPV_SEC_DECOR,      /* OpDecorate, OpMemberDecorate */
   SPV_SEC_TYPES,      /* types, constants, global variables */
   SPV_SEC_FUNCS,
   SPV_SEC_COUNT
};

/* SPIR-V enumerants used here. */
#define SPV_MAGIC            0x07230203u
#define SPV_VERSION_1_0      0x00010000u
#define SPV_CAP_SHADER       1
#define SPV_CAP_IMAGEQUERY   50
#define SPV_MODEL_VERTEX     0
#define SPV_MODEL_FRAGMENT   4
#define SPV_MODE_ORIGINUPPERLEFT 7
#define SPV_SC_UNIFORMCONSTANT 0
#define SPV_SC_INPUT         1
#define SPV_SC_UNIFORM       2
#define SPV_SC_OUTPUT        3
#define SPV_SC_PRIVATE       6
#define SPV_SC_FUNCTION      7
#define SPV_SC_PUSHCONSTANT  9

#define SPV_DEC_RELAXEDPRECISION 0
#define SPV_DEC_BLOCK        2
#define SPV_DEC_COLMAJOR     5
#define SPV_DEC_ARRAYSTRIDE  6
#define SPV_DEC_MATRIXSTRIDE 7
#define SPV_DEC_BUILTIN      11
#define SPV_DEC_NOPERSPECTIVE 13
#define SPV_DEC_FLAT         14
#define SPV_DEC_CENTROID     16
#define SPV_DEC_SAMPLE       17
#define SPV_DEC_INVARIANT    18
#define SPV_DEC_LOCATION     30
#define SPV_DEC_BINDING      33
#define SPV_DEC_DESCRIPTORSET 34
#define SPV_DEC_OFFSET       35

#define SPV_BI_POSITION      0
#define SPV_BI_POINTSIZE     1
#define SPV_BI_CLIPDISTANCE  3
#define SPV_BI_CULLDISTANCE  4
#define SPV_BI_VERTEXINDEX   42
#define SPV_BI_INSTANCEINDEX 43
#define SPV_BI_FRAGCOORD     15
#define SPV_BI_FRONTFACING   17
#define SPV_BI_FRAGDEPTH     22

enum spv_op
{
   SpvOpUndef = 1, SpvOpSource = 3, SpvOpName = 5, SpvOpMemberName = 6,
   SpvOpExtInstImport = 11, SpvOpExtInst = 12, SpvOpMemoryModel = 14,
   SpvOpEntryPoint = 15, SpvOpExecutionMode = 16, SpvOpCapability = 17,
   SpvOpTypeVoid = 19, SpvOpTypeBool = 20, SpvOpTypeInt = 21, SpvOpTypeFloat = 22,
   SpvOpTypeVector = 23, SpvOpTypeMatrix = 24, SpvOpTypeImage = 25,
   SpvOpTypeSampledImage = 27, SpvOpTypeArray = 28, SpvOpTypeStruct = 30,
   SpvOpTypePointer = 32, SpvOpTypeFunction = 33,
   SpvOpConstantTrue = 41, SpvOpConstantFalse = 42, SpvOpConstant = 43,
   SpvOpConstantComposite = 44,
   SpvOpFunction = 54, SpvOpFunctionParameter = 55, SpvOpFunctionEnd = 56,
   SpvOpFunctionCall = 57, SpvOpVariable = 59, SpvOpLoad = 61, SpvOpStore = 62, SpvOpCopyMemory = 63,
   SpvOpAccessChain = 65, SpvOpInBoundsAccessChain = 66, SpvOpDecorate = 71, SpvOpMemberDecorate = 72,
   SpvOpVectorExtractDynamic = 77, SpvOpVectorInsertDynamic = 78,
   SpvOpVectorShuffle = 79, SpvOpCompositeConstruct = 80, SpvOpCompositeExtract = 81,
   SpvOpCompositeInsert = 82, SpvOpTranspose = 84,
   SpvOpSampledImage = 86, SpvOpImageSampleImplicitLod = 87, SpvOpImageSampleExplicitLod = 88,
   SpvOpImageSampleProjImplicitLod = 91, SpvOpImageSampleProjExplicitLod = 92,
   SpvOpImageFetch = 95, SpvOpImageGather = 96, SpvOpImage = 100,
   SpvOpImageQuerySizeLod = 103, SpvOpImageQueryLod = 105, SpvOpImageQueryLevels = 106,
   SpvOpConvertFToU = 109, SpvOpConvertFToS = 110, SpvOpConvertSToF = 111,
   SpvOpConvertUToF = 112, SpvOpBitcast = 124,
   SpvOpSNegate = 126, SpvOpFNegate = 127, SpvOpIAdd = 128, SpvOpFAdd = 129,
   SpvOpISub = 130, SpvOpFSub = 131, SpvOpIMul = 132, SpvOpFMul = 133,
   SpvOpUDiv = 134, SpvOpSDiv = 135, SpvOpFDiv = 136, SpvOpUMod = 137,
   SpvOpSRem = 138, SpvOpSMod = 139, SpvOpFRem = 140, SpvOpFMod = 141,
   SpvOpVectorTimesScalar = 142, SpvOpMatrixTimesScalar = 143,
   SpvOpVectorTimesMatrix = 144, SpvOpMatrixTimesVector = 145,
   SpvOpMatrixTimesMatrix = 146, SpvOpOuterProduct = 147, SpvOpDot = 148,
   SpvOpIAddCarry = 149, SpvOpISubBorrow = 150, SpvOpUMulExtended = 151,
   SpvOpSMulExtended = 152, SpvOpAny = 154, SpvOpAll = 155, SpvOpIsNan = 156,
   SpvOpIsInf = 157, SpvOpLogicalEqual = 164, SpvOpLogicalNotEqual = 165,
   SpvOpLogicalOr = 166, SpvOpLogicalAnd = 167, SpvOpLogicalNot = 168,
   SpvOpSelect = 169, SpvOpIEqual = 170, SpvOpINotEqual = 171,
   SpvOpUGreaterThan = 172, SpvOpSGreaterThan = 173, SpvOpUGreaterThanEqual = 174,
   SpvOpSGreaterThanEqual = 175, SpvOpULessThan = 176, SpvOpSLessThan = 177,
   SpvOpULessThanEqual = 178, SpvOpSLessThanEqual = 179,
   SpvOpFOrdEqual = 180, SpvOpFOrdNotEqual = 182, SpvOpFOrdLessThan = 184,
   SpvOpFOrdGreaterThan = 186, SpvOpFOrdLessThanEqual = 188,
   SpvOpFOrdGreaterThanEqual = 190,
   SpvOpShiftRightLogical = 194, SpvOpShiftRightArithmetic = 195,
   SpvOpShiftLeftLogical = 196, SpvOpBitwiseOr = 197, SpvOpBitwiseXor = 198,
   SpvOpBitwiseAnd = 199, SpvOpNot = 200, SpvOpBitFieldInsert = 201,
   SpvOpBitFieldSExtract = 202, SpvOpBitFieldUExtract = 203, SpvOpBitReverse = 204,
   SpvOpBitCount = 205, SpvOpDPdx = 207, SpvOpDPdy = 208, SpvOpFwidth = 209,
   SpvOpDPdxFine = 210, SpvOpDPdyFine = 211, SpvOpFwidthFine = 212,
   SpvOpDPdxCoarse = 213, SpvOpDPdyCoarse = 214, SpvOpFwidthCoarse = 215,
   SpvOpPhi = 245, SpvOpLoopMerge = 246, SpvOpSelectionMerge = 247,
   SpvOpLabel = 248, SpvOpBranch = 249, SpvOpBranchConditional = 250,
   SpvOpSwitch = 251, SpvOpKill = 252, SpvOpReturn = 253, SpvOpReturnValue = 254,
   SpvOpUnreachable = 255
};

/* GLSL.std.450 instruction numbers. */
enum glsl450
{
   GLSLstd450Round = 1, GLSLstd450RoundEven = 2, GLSLstd450Trunc = 3,
   GLSLstd450FAbs = 4, GLSLstd450SAbs = 5, GLSLstd450FSign = 6, GLSLstd450SSign = 7,
   GLSLstd450Floor = 8, GLSLstd450Ceil = 9, GLSLstd450Fract = 10,
   GLSLstd450Radians = 11, GLSLstd450Degrees = 12, GLSLstd450Sin = 13,
   GLSLstd450Cos = 14, GLSLstd450Tan = 15, GLSLstd450Asin = 16, GLSLstd450Acos = 17,
   GLSLstd450Atan = 18, GLSLstd450Sinh = 19, GLSLstd450Cosh = 20, GLSLstd450Tanh = 21,
   GLSLstd450Asinh = 22, GLSLstd450Acosh = 23, GLSLstd450Atanh = 24,
   GLSLstd450Atan2 = 25, GLSLstd450Pow = 26, GLSLstd450Exp = 27, GLSLstd450Log = 28,
   GLSLstd450Exp2 = 29, GLSLstd450Log2 = 30, GLSLstd450Sqrt = 31,
   GLSLstd450InverseSqrt = 32, GLSLstd450Determinant = 33, GLSLstd450MatrixInverse = 34,
   GLSLstd450Modf = 35, GLSLstd450FMin = 37, GLSLstd450UMin = 38,
   GLSLstd450SMin = 39, GLSLstd450FMax = 40, GLSLstd450UMax = 41, GLSLstd450SMax = 42,
   GLSLstd450FClamp = 43, GLSLstd450UClamp = 44, GLSLstd450SClamp = 45,
   GLSLstd450FMix = 46, GLSLstd450Step = 48, GLSLstd450SmoothStep = 49,
   GLSLstd450Fma = 50, GLSLstd450Frexp = 51, GLSLstd450Ldexp = 53,
   GLSLstd450PackSnorm4x8 = 54, GLSLstd450PackUnorm4x8 = 55,
   GLSLstd450PackSnorm2x16 = 56, GLSLstd450PackUnorm2x16 = 57,
   GLSLstd450PackHalf2x16 = 58, GLSLstd450UnpackSnorm2x16 = 60,
   GLSLstd450UnpackUnorm2x16 = 61, GLSLstd450UnpackHalf2x16 = 62,
   GLSLstd450UnpackSnorm4x8 = 63, GLSLstd450UnpackUnorm4x8 = 64,
   GLSLstd450Length = 66, GLSLstd450Distance = 67, GLSLstd450Cross = 68,
   GLSLstd450Normalize = 69, GLSLstd450FaceForward = 70, GLSLstd450Reflect = 71,
   GLSLstd450Refract = 72, GLSLstd450FindILsb = 73, GLSLstd450FindSMsb = 74,
   GLSLstd450FindUMsb = 75
};

/* Per-type ids: the plain type, and the Function/Private/... pointer
 * to it.  Block member types with layout decorations get their own
 * laid-out ids (arrays carry ArrayStride) keyed by layout. */
struct spv_tinfo
{
   uint32_t id;
   uint32_t img;           /* SAMPLER2D: the OpTypeImage */
   uint32_t laid[2];       /* [0] std430, [1] std140 */
   uint32_t ptr[10];       /* by storage class */
};

struct spv_const
{
   uint32_t type;   /* type id */
   uint32_t bits;
   uint32_t id;
};

struct spv_var
{
   uint32_t id;        /* the OpVariable, or the constant for a folded const */
   uint32_t ptr;       /* folded const indexed dynamically: a variable holding it */
   uint32_t tid;       /* type id of the variable's value */
   uint8_t  sc;        /* storage class */
   bool     is_const;  /* id is a constant value, not a pointer */
   bool     in_block;  /* RSLC_ST_MEMBER / BLOCK: id is the block variable */
};

/* A loop or switch in progress, for break / continue. */
struct spv_ctl
{
   uint32_t merge;
   uint32_t cont;     /* 0 for a switch */
};

struct spv_emit
{
   struct rslc_ctx     *c;
   struct rslc_wbuf     sec[SPV_SEC_COUNT];
   struct spv_tinfo    *tinfo;        /* by type id */
   struct spv_var      *vars;         /* by var id */
   uint32_t            *func_ids;     /* by func id */
   uint32_t            *func_types;
   struct spv_const    *consts;
   struct rslc_wbuf     entry_iface;  /* interface ids for OpEntryPoint */
   struct rslc_wbuf     func_vars;    /* OpVariable Function words, hoisted */
   struct rslc_wbuf     fntypes;      /* interned OpTypeFunction: id, n, words[n] */
   struct spv_ctl       ctl[64];
   uint32_t             nctl;
   uint32_t             nconsts, consts_cap;
   uint32_t             bound;
   uint32_t             ext_id;       /* GLSL.std.450 */
   uint32_t             void_id, bool_id, int_id, uint_id, float_id;
   uint32_t             pervertex_var;/* vertex: the gl_PerVertex block variable */
   uint32_t             block_var[2]; /* uniform / push block variables */
   uint8_t             *member_active[2]; /* per block member, set on use */
   uint32_t             cur_fn;       /* rslc func id being emitted */
   uint32_t             cur_label;    /* current block's label, 0 = terminated */
   uint32_t             entry_label;
   bool                 need_imagequery;
   bool                 in_main;
};

static bool spv_w(struct spv_emit *e, struct rslc_wbuf *b, uint32_t w)
{
   if (b->n == b->cap)
   {
      uint32_t  ncap = b->cap ? b->cap * 2 : 256;
      uint32_t *nv   = (uint32_t*)rslc_arena_alloc(e->c, ncap * sizeof(*nv));
      if (!nv)
         return false;
      if (b->n)
         memcpy(nv, b->v, b->n * sizeof(*nv));
      b->v   = nv;
      b->cap = ncap;
   }
   b->v[b->n++] = w;
   return true;
}

/* Emits an instruction: opcode, then the given words.  n includes
 * nothing for the opcode word. */
static bool spv_inst(struct spv_emit *e, enum spv_section s, enum spv_op op,
      const uint32_t *words, uint32_t n)
{
   struct rslc_wbuf *b = &e->sec[s];
   uint32_t i;
   if (!spv_w(e, b, ((n + 1) << 16) | (uint32_t)op))
      return false;
   for (i = 0; i < n; i++)
      if (!spv_w(e, b, words[i]))
         return false;
   return true;
}

static bool spv_inst1(struct spv_emit *e, enum spv_section s, enum spv_op op, uint32_t a)
{
   return spv_inst(e, s, op, &a, 1);
}

static bool spv_inst2(struct spv_emit *e, enum spv_section s, enum spv_op op, uint32_t a, uint32_t b)
{
   uint32_t w[2];
   w[0] = a; w[1] = b;
   return spv_inst(e, s, op, w, 2);
}

static bool spv_inst3(struct spv_emit *e, enum spv_section s, enum spv_op op, uint32_t a, uint32_t b, uint32_t c)
{
   uint32_t w[3];
   w[0] = a; w[1] = b; w[2] = c;
   return spv_inst(e, s, op, w, 3);
}

static bool spv_inst4(struct spv_emit *e, enum spv_section s, enum spv_op op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
   uint32_t w[4];
   w[0] = a; w[1] = b; w[2] = c; w[3] = d;
   return spv_inst(e, s, op, w, 4);
}

static uint32_t spv_id(struct spv_emit *e)
{
   return e->bound++;
}

/* A string literal as words, NUL padded. */
static bool spv_inst_str(struct spv_emit *e, enum spv_section s, enum spv_op op,
      const uint32_t *pre, uint32_t npre, const char *str, const uint32_t *post, uint32_t npost)
{
   uint32_t len    = (uint32_t)strlen(str);
   uint32_t nwords = len / 4 + 1;
   uint32_t i;
   struct rslc_wbuf *b = &e->sec[s];
   if (!spv_w(e, b, ((npre + nwords + npost + 1) << 16) | (uint32_t)op))
      return false;
   for (i = 0; i < npre; i++)
      if (!spv_w(e, b, pre[i]))
         return false;
   for (i = 0; i < nwords; i++)
   {
      uint32_t w = 0;
      uint32_t k;
      for (k = 0; k < 4; k++)
      {
         uint32_t idx = i * 4 + k;
         if (idx < len)
            w |= (uint32_t)(uint8_t)str[idx] << (8 * k);
      }
      if (!spv_w(e, b, w))
         return false;
   }
   for (i = 0; i < npost; i++)
      if (!spv_w(e, b, post[i]))
         return false;
   return true;
}

static bool spv_name(struct spv_emit *e, uint32_t id, const char *name)
{
   return spv_inst_str(e, SPV_SEC_DEBUG, SpvOpName, &id, 1, name, NULL, 0);
}

static bool spv_member_name(struct spv_emit *e, uint32_t id, uint32_t m, const char *name)
{
   uint32_t pre[2];
   pre[0] = id; pre[1] = m;
   return spv_inst_str(e, SPV_SEC_DEBUG, SpvOpMemberName, pre, 2, name, NULL, 0);
}

static bool spv_decorate(struct spv_emit *e, uint32_t id, uint32_t dec)
{
   return spv_inst2(e, SPV_SEC_DECOR, SpvOpDecorate, id, dec);
}

static bool spv_decorate1(struct spv_emit *e, uint32_t id, uint32_t dec, uint32_t v)
{
   return spv_inst3(e, SPV_SEC_DECOR, SpvOpDecorate, id, dec, v);
}

static bool spv_mdecorate(struct spv_emit *e, uint32_t id, uint32_t m, uint32_t dec)
{
   return spv_inst3(e, SPV_SEC_DECOR, SpvOpMemberDecorate, id, m, dec);
}

static bool spv_mdecorate1(struct spv_emit *e, uint32_t id, uint32_t m, uint32_t dec, uint32_t v)
{
   return spv_inst4(e, SPV_SEC_DECOR, SpvOpMemberDecorate, id, m, dec, v);
}

/* ---- types ---- */

static uint32_t spv_type(struct spv_emit *e, const struct rslc_type *t);

static uint32_t spv_type_laid(struct spv_emit *e, const struct rslc_type *t, bool std140)
{
   struct spv_tinfo *ti = &e->tinfo[t->id];
   uint32_t id;
   unsigned k = std140 ? 1 : 0;

   if (t->kind != RSLC_TY_ARRAY && t->kind != RSLC_TY_STRUCT)
      return spv_type(e, t);
   if (ti->laid[k])
      return ti->laid[k];

   if (t->kind == RSLC_TY_ARRAY)
   {
      uint32_t elem = spv_type_laid(e, t->elem, std140);
      uint32_t len;
      uint32_t a, s, st;
      struct spv_const *cc;
      uint32_t w[3];
      if (!elem)
         return 0;
      /* OpConstant for the length, int typed */
      {
         uint32_t i;
         cc = NULL;
         for (i = 0; i < e->nconsts; i++)
            if (e->consts[i].type == e->int_id && e->consts[i].bits == t->array_len)
               cc = &e->consts[i];
      }
      if (cc)
         len = cc->id;
      else
      {
         len = spv_id(e);
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpConstant, e->int_id, len, t->array_len))
            return 0;
         if (e->nconsts == e->consts_cap)
         {
            uint32_t          ncap = e->consts_cap ? e->consts_cap * 2 : 64;
            struct spv_const *nv   = (struct spv_const*)rslc_arena_alloc(e->c, ncap * sizeof(*nv));
            if (!nv)
               return 0;
            if (e->nconsts)
               memcpy(nv, e->consts, e->nconsts * sizeof(*nv));
            e->consts     = nv;
            e->consts_cap = ncap;
         }
         e->consts[e->nconsts].type = e->int_id;
         e->consts[e->nconsts].bits = t->array_len;
         e->consts[e->nconsts].id   = len;
         e->nconsts++;
      }
      id = spv_id(e);
      w[0] = id; w[1] = elem; w[2] = len;
      if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeArray, w, 3))
         return 0;
      rslc_layout_of(t, std140, &a, &s, &st);
      if (!spv_decorate1(e, id, SPV_DEC_ARRAYSTRIDE, st))
         return 0;
      ti->laid[k] = id;
      return id;
   }
   else
   {
      uint32_t *words = (uint32_t*)rslc_scratch_alloc(e->c, (t->nfields + 1) * sizeof(*words));
      uint32_t  i;
      if (!words)
         return 0;
      for (i = 0; i < t->nfields; i++)
      {
         words[i + 1] = spv_type_laid(e, t->fields[i].type, std140);
         if (!words[i + 1])
            return 0;
      }
      id = spv_id(e);
      words[0] = id;
      if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeStruct, words, t->nfields + 1))
         return 0;
      if (t->name && !spv_name(e, id, t->name->str))
         return 0;
      for (i = 0; i < t->nfields; i++)
      {
         const struct rslc_type *ft = t->fields[i].type;
         const struct rslc_type *bt = ft;
         if (!spv_member_name(e, id, i, t->fields[i].name->str))
            return 0;
         while (bt->kind == RSLC_TY_ARRAY)
            bt = bt->elem;
         if (bt->kind == RSLC_TY_MAT)
         {
            uint32_t a, s, st;
            rslc_layout_of(bt, std140, &a, &s, &st);
            if (!spv_mdecorate(e, id, i, SPV_DEC_COLMAJOR)
                  || !spv_mdecorate1(e, id, i, SPV_DEC_MATRIXSTRIDE, st))
               return 0;
         }
         if (!spv_mdecorate1(e, id, i, SPV_DEC_OFFSET,
                  rslc_layout_field_offset(t, i, std140)))
            return 0;
      }
      ti->laid[k] = id;
      return id;
   }
}

static uint32_t spv_type(struct spv_emit *e, const struct rslc_type *t)
{
   struct spv_tinfo *ti = &e->tinfo[t->id];
   uint32_t id;
   uint32_t w[8];

   if (ti->id)
      return ti->id;
   id = spv_id(e);
   switch (t->kind)
   {
      case RSLC_TY_VOID:
         if (!spv_inst1(e, SPV_SEC_TYPES, SpvOpTypeVoid, id))
            return 0;
         break;
      case RSLC_TY_SCALAR:
         switch (t->base)
         {
            case RSLC_BASE_BOOL:
               if (!spv_inst1(e, SPV_SEC_TYPES, SpvOpTypeBool, id))
                  return 0;
               break;
            case RSLC_BASE_INT:
               if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeInt, id, 32, 1))
                  return 0;
               break;
            case RSLC_BASE_UINT:
               if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeInt, id, 32, 0))
                  return 0;
               break;
            default:
               if (!spv_inst2(e, SPV_SEC_TYPES, SpvOpTypeFloat, id, 32))
                  return 0;
               break;
         }
         break;
      case RSLC_TY_VEC:
      {
         uint32_t s = spv_type(e, rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1));
         if (!s)
            return 0;
         ti = &e->tinfo[t->id];
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeVector, id, s, t->rows))
            return 0;
         break;
      }
      case RSLC_TY_MAT:
      {
         uint32_t col = spv_type(e, rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1));
         if (!col)
            return 0;
         ti = &e->tinfo[t->id];
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeMatrix, id, col, t->cols))
            return 0;
         break;
      }
      case RSLC_TY_SAMPLER2D:
      {
         uint32_t s   = spv_type(e, rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1));
         uint32_t img;
         if (!s)
            return 0;
         ti  = &e->tinfo[t->id];
         img = spv_id(e);
         w[0] = img; w[1] = s; w[2] = 1 /* 2D */; w[3] = 0; w[4] = 0; w[5] = 0; w[6] = 1; w[7] = 0 /* Unknown */;
         if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeImage, w, 8))
            return 0;
         if (!spv_inst2(e, SPV_SEC_TYPES, SpvOpTypeSampledImage, id, img))
            return 0;
         ti->img = img;
         break;
      }
      case RSLC_TY_ARRAY:
      {
         uint32_t elem = spv_type(e, t->elem);
         uint32_t len;
         if (!elem)
            return 0;
         ti  = &e->tinfo[t->id];
         len = spv_id(e);
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpConstant, e->int_id, len, t->array_len))
            return 0;
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeArray, id, elem, len))
            return 0;
         break;
      }
      case RSLC_TY_STRUCT:
      {
         uint32_t *words = (uint32_t*)rslc_scratch_alloc(e->c, (t->nfields + 1) * sizeof(*words));
         uint32_t i;
         if (!words)
            return 0;
         for (i = 0; i < t->nfields; i++)
         {
            words[i + 1] = spv_type(e, t->fields[i].type);
            if (!words[i + 1])
               return 0;
         }
         ti = &e->tinfo[t->id];
         words[0] = id;
         if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeStruct, words, t->nfields + 1))
            return 0;
         if (t->name && !spv_name(e, id, t->name->str))
            return 0;
         for (i = 0; i < t->nfields; i++)
            if (!spv_member_name(e, id, i, t->fields[i].name->str))
               return 0;
         break;
      }
   }
   ti->id = id;
   return id;
}

static uint32_t spv_ptr(struct spv_emit *e, uint32_t tid, unsigned sc)
{
   /* tinfo is indexed by rslc type; pointers are cached on the type
    * whose id this is: find it by scan of the few we make. */
   uint32_t i;
   for (i = 1; i <= e->c->ntypes; i++)
      if (e->tinfo[i].id == tid)
      {
         if (e->tinfo[i].ptr[sc])
            return e->tinfo[i].ptr[sc];
         e->tinfo[i].ptr[sc] = spv_id(e);
         if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypePointer, e->tinfo[i].ptr[sc], sc, tid))
            return 0;
         return e->tinfo[i].ptr[sc];
      }
   /* A laid-out type or an ad-hoc one: make a fresh pointer type. */
   {
      uint32_t p = spv_id(e);
      if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypePointer, p, sc, tid))
         return 0;
      return p;
   }
}

/* Function parameters are pointers; samplers point at the uniform. */
static unsigned spv_param_sc(const struct rslc_type *t)
{
   return t->kind == RSLC_TY_SAMPLER2D ? SPV_SC_UNIFORMCONSTANT : SPV_SC_FUNCTION;
}

static uint32_t spv_ptr_to(struct spv_emit *e, const struct rslc_type *t, unsigned sc)
{
   uint32_t tid = spv_type(e, t);
   if (!tid)
      return 0;
   return spv_ptr(e, tid, sc);
}

/* ---- constants ---- */

static uint32_t spv_const_bits(struct spv_emit *e, const struct rslc_type *t, uint32_t bits)
{
   uint32_t tid = spv_type(e, t);
   uint32_t i, id;
   if (!tid)
      return 0;
   for (i = 0; i < e->nconsts; i++)
      if (e->consts[i].type == tid && e->consts[i].bits == bits)
         return e->consts[i].id;
   id = spv_id(e);
   if (t->base == RSLC_BASE_BOOL)
   {
      if (!spv_inst2(e, SPV_SEC_TYPES, bits ? SpvOpConstantTrue : SpvOpConstantFalse, tid, id))
         return 0;
   }
   else if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpConstant, tid, id, bits))
      return 0;
   if (e->nconsts == e->consts_cap)
   {
      uint32_t          ncap = e->consts_cap ? e->consts_cap * 2 : 64;
      struct spv_const *nv   = (struct spv_const*)rslc_arena_alloc(e->c, ncap * sizeof(*nv));
      if (!nv)
         return 0;
      if (e->nconsts)
         memcpy(nv, e->consts, e->nconsts * sizeof(*nv));
      e->consts     = nv;
      e->consts_cap = ncap;
   }
   e->consts[e->nconsts].type = tid;
   e->consts[e->nconsts].bits = bits;
   e->consts[e->nconsts].id   = id;
   e->nconsts++;
   return id;
}

static uint32_t spv_const_int(struct spv_emit *e, int32_t v)
{
   return spv_const_bits(e, rslc_type_basic(e->c, RSLC_TY_SCALAR, RSLC_BASE_INT, 1, 1), (uint32_t)v);
}

static uint32_t spv_const_uint(struct spv_emit *e, uint32_t v)
{
   return spv_const_bits(e, rslc_type_basic(e->c, RSLC_TY_SCALAR, RSLC_BASE_UINT, 1, 1), v);
}

static uint32_t spv_const_float(struct spv_emit *e, float f)
{
   uint32_t u;
   memcpy(&u, &f, 4);
   return spv_const_bits(e, rslc_type_basic(e->c, RSLC_TY_SCALAR, RSLC_BASE_FLOAT, 1, 1), u);
}

/* A constant composite from a folded value. */
static uint32_t spv_const_composite(struct spv_emit *e, const struct rslc_type *t,
      const uint32_t *bits, uint32_t *consumed)
{
   uint32_t words[20];
   uint32_t n = 0, id, tid;
   *consumed = 0;
   if (t->kind == RSLC_TY_SCALAR)
   {
      *consumed = 1;
      return spv_const_bits(e, t, bits[0]);
   }
   tid = spv_type(e, t);
   if (!tid)
      return 0;
   if (t->kind == RSLC_TY_VEC)
   {
      const struct rslc_type *st = rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1);
      uint32_t i;
      for (i = 0; i < t->rows; i++)
      {
         words[2 + n] = spv_const_bits(e, st, bits[i]);
         if (!words[2 + n])
            return 0;
         n++;
      }
      *consumed = t->rows;
   }
   else if (t->kind == RSLC_TY_MAT)
   {
      const struct rslc_type *col = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
      uint32_t i, used = 0;
      for (i = 0; i < t->cols; i++)
      {
         uint32_t k;
         words[2 + n] = spv_const_composite(e, col, bits + used, &k);
         if (!words[2 + n])
            return 0;
         used += k;
         n++;
      }
      *consumed = used;
   }
   else
      return 0;
   id = spv_id(e);
   words[0] = tid;
   words[1] = id;
   if (!spv_inst(e, SPV_SEC_TYPES, SpvOpConstantComposite, words, 2 + n))
      return 0;
   return id;
}

/* Whether node is a constant: a foldable scalar / vector / matrix
 * expression, or a struct / array constructor of constants. */
static bool spv_node_is_const(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   struct rslc_cval cv;
   if (!n->type)
      return false;
   if (n->type->kind == RSLC_TY_SCALAR || n->type->kind == RSLC_TY_VEC || n->type->kind == RSLC_TY_MAT)
      return rslc_const_eval(e->c, node, &cv);
   if (n->op == RSLC_OP_CONSTRUCT && (n->type->kind == RSLC_TY_STRUCT || n->type->kind == RSLC_TY_ARRAY))
   {
      uint32_t i, cnt = rslc_list_len(e->c, n->b);
      for (i = 0; i < cnt; i++)
         if (!spv_node_is_const(e, rslc_list_item(e->c, n->b, i)))
            return false;
      return true;
   }
   if (n->op == RSLC_OP_VAR)
   {
      const struct rslc_var *v = e->c->prog.vars[n->a];
      return v->storage == RSLC_ST_CONST && v->init && spv_node_is_const(e, v->init);
   }
   return false;
}

/* The constant for a node spv_node_is_const accepted. */
static uint32_t spv_const_node(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   struct rslc_cval cv;
   uint32_t consumed;
   if (n->type->kind == RSLC_TY_SCALAR || n->type->kind == RSLC_TY_VEC || n->type->kind == RSLC_TY_MAT)
   {
      if (!rslc_const_eval(e->c, node, &cv))
         return 0;
      return spv_const_composite(e, n->type, cv.v, &consumed);
   }
   if (n->op == RSLC_OP_VAR)
      return spv_const_node(e, e->c->prog.vars[n->a]->init);
   {
      uint32_t  cnt   = rslc_list_len(e->c, n->b);
      uint32_t *words = (uint32_t*)rslc_scratch_alloc(e->c, (cnt + 2) * sizeof(*words));
      uint32_t  i, tid;
      if (!words)
         return 0;
      for (i = 0; i < cnt; i++)
      {
         words[2 + i] = spv_const_node(e, rslc_list_item(e->c, n->b, i));
         if (!words[2 + i])
            return 0;
      }
      tid = spv_type(e, n->type);
      if (!tid)
         return 0;
      words[0] = tid;
      words[1] = spv_id(e);
      if (!spv_inst(e, SPV_SEC_TYPES, SpvOpConstantComposite, words, cnt + 2))
         return 0;
      return words[1];
   }
}

/* A const variable with a constant initialiser is its constant,
 * made on first use so an unreferenced one leaves nothing behind. */
static uint32_t spv_const_var(struct spv_emit *e, const struct rslc_var *v)
{
   struct spv_var *sv = &e->vars[v->id];
   if (sv->id)
      return sv->id;
   sv->id = spv_const_node(e, v->init);
   if (sv->id && v->name && v->type->kind != RSLC_TY_SCALAR && !spv_name(e, sv->id, v->name->str))
      return 0;
   return sv->id;
}

/* ---- function body emission ---- */

static bool spv_begin_block(struct spv_emit *e, uint32_t label)
{
   if (!spv_inst1(e, SPV_SEC_FUNCS, SpvOpLabel, label))
      return false;
   e->cur_label = label;
   return true;
}

/* After a terminator; a following instruction opens a fresh,
 * unreachable block. */
static bool spv_ensure_block(struct spv_emit *e)
{
   if (e->cur_label)
      return true;
   return spv_begin_block(e, spv_id(e));
}

static bool spv_branch(struct spv_emit *e, uint32_t target)
{
   if (!spv_ensure_block(e))
      return false;
   if (!spv_inst1(e, SPV_SEC_FUNCS, SpvOpBranch, target))
      return false;
   e->cur_label = 0;
   return true;
}

static bool spv_fn_inst(struct spv_emit *e, enum spv_op op, const uint32_t *w, uint32_t n)
{
   if (!spv_ensure_block(e))
      return false;
   return spv_inst(e, SPV_SEC_FUNCS, op, w, n);
}

/* result-typed instruction helper: words = type, id, operands... */
static uint32_t spv_fn_result(struct spv_emit *e, enum spv_op op, uint32_t tid,
      const uint32_t *ops, uint32_t nops, uint8_t prec)
{
   uint32_t i, id;
   struct rslc_wbuf *b = &e->sec[SPV_SEC_FUNCS];
   if (!spv_ensure_block(e))
      return 0;
   id = spv_id(e);
   if (!spv_w(e, b, ((nops + 3) << 16) | (uint32_t)op) || !spv_w(e, b, tid) || !spv_w(e, b, id))
      return 0;
   for (i = 0; i < nops; i++)
      if (!spv_w(e, b, ops[i]))
         return 0;
   if (prec == RSLC_PREC_LOW || prec == RSLC_PREC_MEDIUM)
      if (!spv_decorate(e, id, SPV_DEC_RELAXEDPRECISION))
         return 0;
   return id;
}

static uint32_t spv_r1(struct spv_emit *e, enum spv_op op, uint32_t tid, uint32_t a, uint8_t prec)
{
   return spv_fn_result(e, op, tid, &a, 1, prec);
}

static uint32_t spv_r2(struct spv_emit *e, enum spv_op op, uint32_t tid, uint32_t a, uint32_t b, uint8_t prec)
{
   uint32_t w[2];
   w[0] = a; w[1] = b;
   return spv_fn_result(e, op, tid, w, 2, prec);
}

static uint32_t spv_r3(struct spv_emit *e, enum spv_op op, uint32_t tid, uint32_t a, uint32_t b, uint32_t c, uint8_t prec)
{
   uint32_t w[3];
   w[0] = a; w[1] = b; w[2] = c;
   return spv_fn_result(e, op, tid, w, 3, prec);
}

/* A Function-storage variable, hoisted to the entry block. */
static uint32_t spv_local(struct spv_emit *e, const struct rslc_type *t, const char *name, uint8_t prec)
{
   uint32_t ptr = spv_ptr_to(e, t, SPV_SC_FUNCTION);
   uint32_t id;
   if (!ptr)
      return 0;
   id = spv_id(e);
   if (!spv_w(e, &e->func_vars, (4u << 16) | SpvOpVariable)
         || !spv_w(e, &e->func_vars, ptr)
         || !spv_w(e, &e->func_vars, id)
         || !spv_w(e, &e->func_vars, SPV_SC_FUNCTION))
      return 0;
   if (name && !spv_name(e, id, name))
      return 0;
   if ((prec == RSLC_PREC_LOW || prec == RSLC_PREC_MEDIUM) && !spv_decorate(e, id, SPV_DEC_RELAXEDPRECISION))
      return 0;
   return id;
}

static uint32_t spv_load(struct spv_emit *e, const struct rslc_type *t, uint32_t ptr, uint8_t prec)
{
   uint32_t tid = spv_type(e, t);
   if (!tid)
      return 0;
   return spv_r1(e, SpvOpLoad, tid, ptr, prec);
}

static bool spv_store(struct spv_emit *e, uint32_t ptr, uint32_t val)
{
   uint32_t w[2];
   w[0] = ptr; w[1] = val;
   return spv_fn_inst(e, SpvOpStore, w, 2);
}

static uint32_t spv_rvalue(struct spv_emit *e, uint32_t node);
static uint32_t spv_lvalue(struct spv_emit *e, uint32_t node, unsigned *sc, bool *laid, bool *std140);
static bool     spv_stmt(struct spv_emit *e, uint32_t node);

static const struct rslc_type *spv_scalar_of(struct spv_emit *e, const struct rslc_type *t)
{
   return rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1);
}

/* Splat a scalar to a vector of width n. */
static uint32_t spv_splat(struct spv_emit *e, uint32_t s, const struct rslc_type *vt, uint8_t prec)
{
   uint32_t w[4];
   uint32_t i;
   uint32_t tid = spv_type(e, vt);
   if (!tid)
      return 0;
   for (i = 0; i < vt->rows; i++)
      w[i] = s;
   return spv_fn_result(e, SpvOpCompositeConstruct, tid, w, vt->rows, prec);
}

/* Converts a value between scalar/vector bases of the same width. */
static uint32_t spv_convert(struct spv_emit *e, uint32_t v, const struct rslc_type *from,
      const struct rslc_type *to, uint8_t prec)
{
   uint32_t tid = spv_type(e, to);
   if (!tid)
      return 0;
   if (from->base == to->base)
      return v;
   if (to->base == RSLC_BASE_FLOAT)
      return spv_r1(e, from->base == RSLC_BASE_INT ? SpvOpConvertSToF
            : from->base == RSLC_BASE_UINT ? SpvOpConvertUToF : SpvOpSelect, tid, v, prec);
   if (from->base == RSLC_BASE_FLOAT)
      return spv_r1(e, to->base == RSLC_BASE_INT ? SpvOpConvertFToS : SpvOpConvertFToU, tid, v, prec);
   if ((from->base == RSLC_BASE_INT && to->base == RSLC_BASE_UINT)
         || (from->base == RSLC_BASE_UINT && to->base == RSLC_BASE_INT))
      return spv_r1(e, SpvOpBitcast, tid, v, prec);
   return 0;
}

/* Scalar/vector base conversion including bool, as a constructor does. */
static uint32_t spv_cast(struct spv_emit *e, uint32_t v, const struct rslc_type *from,
      const struct rslc_type *to, uint8_t prec)
{
   uint32_t tid = spv_type(e, to);
   uint32_t n   = to->kind == RSLC_TY_VEC ? to->rows : 1;
   if (!tid)
      return 0;
   if (from->base == to->base)
      return v;
   if (to->base == RSLC_BASE_BOOL)
   {
      const struct rslc_type *zt = rslc_type_basic(e->c, from->kind, (enum rslc_base)from->base, from->rows, 1);
      uint32_t zero = from->base == RSLC_BASE_FLOAT ? spv_const_float(e, 0.0f)
         : from->base == RSLC_BASE_INT ? spv_const_int(e, 0) : spv_const_uint(e, 0);
      if (n > 1)
         zero = spv_splat(e, zero, zt, 0);
      return spv_r2(e, from->base == RSLC_BASE_FLOAT ? SpvOpFOrdNotEqual : SpvOpINotEqual, tid, v, zero, prec);
   }
   if (from->base == RSLC_BASE_BOOL)
   {
      const struct rslc_type *st = spv_scalar_of(e, to);
      uint32_t one  = to->base == RSLC_BASE_FLOAT ? spv_const_float(e, 1.0f)
         : to->base == RSLC_BASE_INT ? spv_const_int(e, 1) : spv_const_uint(e, 1);
      uint32_t zero = to->base == RSLC_BASE_FLOAT ? spv_const_float(e, 0.0f)
         : to->base == RSLC_BASE_INT ? spv_const_int(e, 0) : spv_const_uint(e, 0);
      (void)st;
      if (n > 1)
      {
         one  = spv_splat(e, one, to, 0);
         zero = spv_splat(e, zero, to, 0);
      }
      return spv_r3(e, SpvOpSelect, tid, v, one, zero, prec);
   }
   return spv_convert(e, v, from, to, prec);
}

/* Extracts component i of a vector value. */
static uint32_t spv_extract(struct spv_emit *e, uint32_t v, const struct rslc_type *vt, uint32_t i, uint8_t prec)
{
   uint32_t tid;
   if (vt->kind == RSLC_TY_SCALAR)
      return v;
   tid = spv_type(e, spv_scalar_of(e, vt));
   return spv_r2(e, SpvOpCompositeExtract, tid, v, i, prec);
}

/* Extracts column i of a matrix value. */
static uint32_t spv_column(struct spv_emit *e, uint32_t m, const struct rslc_type *mt, uint32_t i, uint8_t prec)
{
   uint32_t tid = spv_type(e, rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, mt->rows, 1));
   return spv_r2(e, SpvOpCompositeExtract, tid, m, i, prec);
}

/* Builds a composite from constituent ids. */
static uint32_t spv_construct(struct spv_emit *e, const struct rslc_type *t, const uint32_t *parts,
      uint32_t n, uint8_t prec)
{
   uint32_t tid = spv_type(e, t);
   if (!tid)
      return 0;
   return spv_fn_result(e, SpvOpCompositeConstruct, tid, parts, n, prec);
}

/* Matrix component-wise op by columns. */
static uint32_t spv_mat_columnwise(struct spv_emit *e, enum spv_op op, const struct rslc_type *mt,
      uint32_t a, uint32_t b, bool b_is_mat, uint8_t prec)
{
   uint32_t cols[4];
   uint32_t i;
   const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, mt->rows, 1);
   uint32_t ctid = spv_type(e, ct);
   for (i = 0; i < mt->cols; i++)
   {
      uint32_t ca = spv_column(e, a, mt, i, prec);
      uint32_t cb = b_is_mat ? spv_column(e, b, mt, i, prec) : b;
      if (!ca || !cb)
         return 0;
      cols[i] = spv_r2(e, op, ctid, ca, cb, prec);
      if (!cols[i])
         return 0;
   }
   return spv_construct(e, mt, cols, mt->cols, prec);
}

static enum spv_op spv_binop(const struct rslc_type *t, uint16_t op)
{
   bool f = t->base == RSLC_BASE_FLOAT;
   bool s = t->base == RSLC_BASE_INT;
   switch (op)
   {
      case RSLC_BIN_ADD: return f ? SpvOpFAdd : SpvOpIAdd;
      case RSLC_BIN_SUB: return f ? SpvOpFSub : SpvOpISub;
      case RSLC_BIN_MUL: return f ? SpvOpFMul : SpvOpIMul;
      case RSLC_BIN_DIV: return f ? SpvOpFDiv : s ? SpvOpSDiv : SpvOpUDiv;
      case RSLC_BIN_MOD: return s ? SpvOpSMod : SpvOpUMod;
      case RSLC_BIN_LSHIFT: return SpvOpShiftLeftLogical;
      case RSLC_BIN_RSHIFT: return s ? SpvOpShiftRightArithmetic : SpvOpShiftRightLogical;
      case RSLC_BIN_BAND: return SpvOpBitwiseAnd;
      case RSLC_BIN_BOR:  return SpvOpBitwiseOr;
      case RSLC_BIN_BXOR: return SpvOpBitwiseXor;
      case RSLC_BIN_LT:   return f ? SpvOpFOrdLessThan : s ? SpvOpSLessThan : SpvOpULessThan;
      case RSLC_BIN_GT:   return f ? SpvOpFOrdGreaterThan : s ? SpvOpSGreaterThan : SpvOpUGreaterThan;
      case RSLC_BIN_LE:   return f ? SpvOpFOrdLessThanEqual : s ? SpvOpSLessThanEqual : SpvOpULessThanEqual;
      case RSLC_BIN_GE:   return f ? SpvOpFOrdGreaterThanEqual : s ? SpvOpSGreaterThanEqual : SpvOpUGreaterThanEqual;
      case RSLC_BIN_EQ:   return f ? SpvOpFOrdEqual : t->base == RSLC_BASE_BOOL ? SpvOpLogicalEqual : SpvOpIEqual;
      case RSLC_BIN_NE:   return f ? SpvOpFOrdNotEqual : t->base == RSLC_BASE_BOOL ? SpvOpLogicalNotEqual : SpvOpINotEqual;
      case RSLC_BIN_XOR:  return SpvOpLogicalNotEqual;
      default: return SpvOpUndef;
   }
}

/* Equality of two values of any type, reduced to one bool. */
static uint32_t spv_equal(struct spv_emit *e, const struct rslc_type *t, uint32_t a, uint32_t b,
      bool ne, uint8_t prec)
{
   uint32_t btid = e->bool_id;
   if (t->kind == RSLC_TY_SCALAR)
      return spv_r2(e, spv_binop(t, ne ? RSLC_BIN_NE : RSLC_BIN_EQ), btid, a, b, prec);
   if (t->kind == RSLC_TY_VEC)
   {
      const struct rslc_type *bv = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_BOOL, t->rows, 1);
      uint32_t bvt = spv_type(e, bv);
      uint32_t v   = spv_r2(e, spv_binop(t, ne ? RSLC_BIN_NE : RSLC_BIN_EQ), bvt, a, b, prec);
      if (!v)
         return 0;
      return spv_r1(e, ne ? SpvOpAny : SpvOpAll, btid, v, prec);
   }
   {
      /* composite: per element, combined */
      uint32_t acc = 0;
      uint32_t i, n;
      const struct rslc_type *et = NULL;
      if (t->kind == RSLC_TY_MAT)
      {
         n  = t->cols;
         et = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
      }
      else if (t->kind == RSLC_TY_ARRAY)
      {
         n  = t->array_len;
         et = t->elem;
      }
      else
         n = t->nfields;
      for (i = 0; i < n; i++)
      {
         const struct rslc_type *ft = t->kind == RSLC_TY_STRUCT ? t->fields[i].type : et;
         uint32_t ftid = spv_type(e, ft);
         uint32_t ea = spv_r2(e, SpvOpCompositeExtract, ftid, a, i, prec);
         uint32_t eb = spv_r2(e, SpvOpCompositeExtract, ftid, b, i, prec);
         uint32_t r;
         if (!ea || !eb)
            return 0;
         r = spv_equal(e, ft, ea, eb, ne, prec);
         if (!r)
            return 0;
         acc = acc ? spv_r2(e, ne ? SpvOpLogicalOr : SpvOpLogicalAnd, btid, acc, r, prec) : r;
         if (!acc)
            return 0;
      }
      return acc;
   }
}

static bool spv_pervertex(struct spv_emit *e);
static bool spv_builtin_var(struct spv_emit *e, const struct rslc_var *v);

/* A var reference as a pointer (or a constant / block member chain). */
static uint32_t spv_var_ptr(struct spv_emit *e, const struct rslc_var *v, unsigned *sc,
      bool *laid, bool *std140)
{
   struct spv_var *sv = &e->vars[v->id];
   *laid   = false;
   *std140 = false;
   if (v->storage == RSLC_ST_MEMBER)
   {
      const struct rslc_blockdecl *b  = &e->c->prog.blocks[v->block];
      bool     l140 = b->std140;
      uint32_t idx  = spv_const_int(e, (int32_t)v->member);
      uint32_t ptrt;
      uint32_t id;
      const struct rslc_type *ft = b->type->fields[v->member].type;
      uint32_t ftid;
      *sc     = b->push ? SPV_SC_PUSHCONSTANT : SPV_SC_UNIFORM;
      *laid   = true;
      *std140 = l140;
      e->member_active[v->block][v->member] = 1;
      ftid = spv_type_laid(e, ft, l140);
      if (!ftid || !idx)
         return 0;
      ptrt = spv_ptr(e, ftid, *sc);
      if (!ptrt)
         return 0;
      id   = spv_id(e);
      {
         uint32_t ww[4];
         ww[0] = ptrt; ww[1] = id; ww[2] = e->block_var[v->block]; ww[3] = idx;
         if (!spv_fn_inst(e, SpvOpAccessChain, ww, 4))
            return 0;
      }
      return id;
   }
   if (v->storage == RSLC_ST_BLOCK)
   {
      const struct rslc_blockdecl *b = &e->c->prog.blocks[v->block];
      *sc     = b->push ? SPV_SC_PUSHCONSTANT : SPV_SC_UNIFORM;
      *laid   = true;
      *std140 = b->std140;
      /* the whole block as a value: every member is live */
      memset(e->member_active[v->block], 1, b->type->nfields);
      return e->block_var[v->block];
   }
   if (v->storage == RSLC_ST_BUILTIN && !sv->id)
   {
      /* builtins are emitted on first use, so an unreferenced
       * gl_FragDepth never becomes an output */
      if (e->c->stage == RSLC_STAGE_VERTEX
            && (v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_POINTSIZE))
      {
         if (!e->pervertex_var && !spv_pervertex(e))
            return 0;
      }
      else if (!spv_builtin_var(e, v))
         return 0;
   }
   if (v->storage == RSLC_ST_BUILTIN && e->pervertex_var
         && (v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_POINTSIZE))
   {
      uint32_t idx  = spv_const_int(e, v->builtin == RSLC_BI_POSITION ? 0 : 1);
      uint32_t ptrt = spv_ptr_to(e, v->type, SPV_SC_OUTPUT);
      uint32_t ww[4];
      uint32_t id;
      if (!idx || !ptrt)
         return 0;
      id = spv_id(e);
      ww[0] = ptrt; ww[1] = id; ww[2] = e->pervertex_var; ww[3] = idx;
      if (!spv_fn_inst(e, SpvOpAccessChain, ww, 4))
         return 0;
      *sc = SPV_SC_OUTPUT;
      return id;
   }
   *sc = sv->sc;
   return sv->id;
}

/* Materialise an rvalue into a Function temporary so it can be
 * indexed through a pointer. */
static uint32_t spv_temp_of(struct spv_emit *e, uint32_t val, const struct rslc_type *t)
{
   uint32_t tmp = spv_local(e, t, NULL, 0);
   if (!tmp || !spv_store(e, tmp, val))
      return 0;
   return tmp;
}

/* The pointer for an lvalue expression: an access chain from a
 * variable.  sc receives its storage class, laid whether the pointee
 * is a laid-out block type. */
static uint32_t spv_lvalue(struct spv_emit *e, uint32_t node, unsigned *sc, bool *laid, bool *std140)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_VAR:
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         if (e->vars[v->id].is_const)
         {
            /* A folded constant indexed dynamically needs storage.
             * A global gets one Private variable initialised with the
             * constant.  A local gets a fresh Function variable at
             * every access, stored right before it is read, as
             * glslang's "indexable": the store stays inside the branch
             * that reads it, and each variable is written once before
             * its only read, so drivers can still fold it into a
             * constant table. */
            struct spv_var *sv = &e->vars[v->id];
            uint32_t cid = spv_const_var(e, v);
            unsigned vsc = v->global ? SPV_SC_PRIVATE : SPV_SC_FUNCTION;
            *sc = vsc; *laid = false; *std140 = false;
            if (!cid)
               return 0;
            if (!v->global)
            {
               uint32_t ptr = spv_local(e, v->type, v->name ? v->name->str : NULL, 0);
               if (!ptr || !spv_store(e, ptr, cid))
                  return 0;
               return ptr;
            }
            if (!sv->ptr)
            {
               uint32_t ptrt = spv_ptr_to(e, v->type, vsc);
               if (!ptrt)
                  return 0;
               sv->ptr = spv_id(e);
               if (!spv_inst4(e, SPV_SEC_TYPES, SpvOpVariable, ptrt, sv->ptr, vsc, cid))
                  return 0;
               if (v->name && !spv_name(e, sv->ptr, v->name->str))
                  return 0;
            }
            return sv->ptr;
         }
         return spv_var_ptr(e, v, sc, laid, std140);
      }
      case RSLC_OP_MEMBER:
      case RSLC_OP_INDEX:
      {
         const struct rslc_node *bn = rslc_node_at(e->c, n->a);
         uint32_t base;
         uint32_t idx;
         uint32_t tid, ptrt, id;
         uint32_t w[4];
         if (n->op == RSLC_OP_MEMBER && bn->op == RSLC_OP_VAR
               && e->c->prog.vars[bn->a]->storage == RSLC_ST_BLOCK)
         {
            /* instance.member: only that member is live */
            const struct rslc_var *bv = e->c->prog.vars[bn->a];
            const struct rslc_blockdecl *b = &e->c->prog.blocks[bv->block];
            *sc     = b->push ? SPV_SC_PUSHCONSTANT : SPV_SC_UNIFORM;
            *laid   = true;
            *std140 = b->std140;
            base    = e->block_var[bv->block];
            e->member_active[bv->block][n->b] = 1;
         }
         else
            base = spv_lvalue(e, n->a, sc, laid, std140);
         if (!base)
            return 0;
         if (n->op == RSLC_OP_MEMBER)
            idx = spv_const_int(e, (int32_t)n->b);
         else
            idx = spv_rvalue(e, n->b);
         if (!idx)
            return 0;
         tid = *laid ? spv_type_laid(e, n->type, *std140) : spv_type(e, n->type);
         if (!tid)
            return 0;
         ptrt = spv_ptr(e, tid, *sc);
         if (!ptrt)
            return 0;
         id = spv_id(e);
         w[0] = ptrt; w[1] = id; w[2] = base; w[3] = idx;
         if (!spv_fn_inst(e, SpvOpAccessChain, w, 4))
            return 0;
         return id;
      }
      case RSLC_OP_SWIZZLE:
      {
         /* single-component swizzle is an access chain */
         unsigned cnt = n->aux & 3u;
         uint32_t base, idx, tid, ptrt, id;
         uint32_t w[4];
         if (cnt != 1)
            return 0;
         base = spv_lvalue(e, n->a, sc, laid, std140);
         if (!base)
            return 0;
         idx  = spv_const_int(e, (int32_t)((n->aux >> 2) & 3u));
         tid  = spv_type(e, n->type);
         if (!idx || !tid)
            return 0;
         ptrt = spv_ptr(e, tid, *sc);
         if (!ptrt)
            return 0;
         id = spv_id(e);
         w[0] = ptrt; w[1] = id; w[2] = base; w[3] = idx;
         if (!spv_fn_inst(e, SpvOpAccessChain, w, 4))
            return 0;
         return id;
      }
      default:
      {
         /* Not an lvalue: a temporary holding the value. */
         uint32_t v = spv_rvalue(e, node);
         if (!v)
            return 0;
         *sc = SPV_SC_FUNCTION; *laid = false; *std140 = false;
         return spv_temp_of(e, v, n->type);
      }
   }
}

/* Stores val into the lvalue node; multi-component swizzles become a
 * load, insert, store. */
static bool spv_assign_to(struct spv_emit *e, uint32_t node, uint32_t val)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   unsigned sc; bool laid, std140;
   if (n->op == RSLC_OP_SWIZZLE && (n->aux & 3u) != 1)
   {
      const struct rslc_node *base = rslc_node_at(e->c, n->a);
      unsigned cnt = n->aux & 3u, i;
      uint32_t ptr, cur, tid;
      if (!cnt)
         cnt = 4;
      ptr = spv_lvalue(e, n->a, &sc, &laid, &std140);
      if (!ptr)
         return false;
      cur = spv_load(e, base->type, ptr, n->prec);
      tid = spv_type(e, base->type);
      if (!cur || !tid)
         return false;
      for (i = 0; i < cnt; i++)
      {
         uint32_t comp = spv_extract(e, val, n->type, i, n->prec);
         uint32_t w[3];
         if (!comp)
            return false;
         w[0] = comp; w[1] = cur; w[2] = (n->aux >> (2 + 2 * i)) & 3u;
         cur = spv_fn_result(e, SpvOpCompositeInsert, tid, w, 3, n->prec);
         if (!cur)
            return false;
      }
      return spv_store(e, ptr, cur);
   }
   {
      uint32_t ptr = spv_lvalue(e, node, &sc, &laid, &std140);
      if (!ptr)
         return false;
      return spv_store(e, ptr, val);
   }
}

/* Rebuilds a value of a laid-out aggregate type as the plain type:
 * the two differ only in decorations, but SPIR-V treats them as
 * distinct types, so nested aggregates are re-constructed member by
 * member. */
static uint32_t spv_unlay(struct spv_emit *e, const struct rslc_type *t, uint32_t lv,
      bool std140, uint8_t prec)
{
   uint32_t *parts;
   uint32_t  i, n, r;
   struct rslc_mark mark;
   if (t->kind != RSLC_TY_ARRAY && t->kind != RSLC_TY_STRUCT)
      return lv;
   n = t->kind == RSLC_TY_ARRAY ? t->array_len : t->nfields;
   rslc_scratch_mark(e->c, &mark);
   parts = (uint32_t*)rslc_scratch_alloc(e->c, (n ? n : 1) * sizeof(*parts));
   if (!parts)
      return 0;
   for (i = 0; i < n; i++)
   {
      const struct rslc_type *et = t->kind == RSLC_TY_ARRAY ? t->elem : t->fields[i].type;
      uint32_t etid = spv_type_laid(e, et, std140);
      uint32_t ev   = etid ? spv_r2(e, SpvOpCompositeExtract, etid, lv, i, prec) : 0;
      if (!ev)
         return 0;
      parts[i] = spv_unlay(e, et, ev, std140, prec);
      if (!parts[i])
         return 0;
   }
   r = spv_construct(e, t, parts, n, prec);
   rslc_scratch_release(e->c, &mark);
   return r;
}

/* Reads an rvalue through a pointer; from a block, aggregates are
 * converted to their plain type. */
static uint32_t spv_load_from(struct spv_emit *e, const struct rslc_type *t, uint32_t ptr,
      bool laid, bool std140, uint8_t prec)
{
   uint32_t ltid, lv;
   if (!laid || (t->kind != RSLC_TY_ARRAY && t->kind != RSLC_TY_STRUCT))
      return spv_load(e, t, ptr, prec);
   ltid = spv_type_laid(e, t, std140);
   lv   = ltid ? spv_r1(e, SpvOpLoad, ltid, ptr, prec) : 0;
   if (!lv)
      return 0;
   return spv_unlay(e, t, lv, std140, prec);
}

static uint32_t spv_ext(struct spv_emit *e, uint32_t tid, uint32_t inst, const uint32_t *args,
      uint32_t n, uint8_t prec)
{
   uint32_t w[8];
   uint32_t i;
   if (n > 6)
      return 0;
   w[0] = e->ext_id;
   w[1] = inst;
   for (i = 0; i < n; i++)
      w[2 + i] = args[i];
   return spv_fn_result(e, SpvOpExtInst, tid, w, n + 2, prec);
}

/* Short-circuit && / || and ?: via branches and a phi. */
static uint32_t spv_phi2(struct spv_emit *e, uint32_t tid, uint32_t a, uint32_t la,
      uint32_t b, uint32_t lb, uint8_t prec)
{
   uint32_t w[4];
   w[0] = a; w[1] = la; w[2] = b; w[3] = lb;
   return spv_fn_result(e, SpvOpPhi, tid, w, 4, prec);
}

/* glslang's shapes for ?: and && / ||, mirrored so the two modules
 * branch alike and a driver's compiler rounds them alike.  A ?: of
 * scalar or vector type is an OpSelect when each side is a plain
 * variable or a constant expression, control flow otherwise; the right
 * side of && / || is evaluated unconditionally (OpLogicalAnd / Or)
 * when it is a leaf - a variable or a constant - or one comparison,
 * not, member, constant index, any or all over leaves, and short
 * circuits otherwise. */
static bool spv_leaf(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   struct rslc_cval cv;
   if (n->op == RSLC_OP_VAR)
   {
      const struct rslc_var *v = e->c->prog.vars[n->a];
      return !(v->storage == RSLC_ST_PARAM && v->dir != RSLC_DIR_IN);
   }
   return n->op == RSLC_OP_CONST || rslc_const_eval(e->c, node, &cv);
}

static bool spv_trivial_rhs(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   if (spv_leaf(e, node))
      return true;
   switch (n->op)
   {
      case RSLC_OP_UNARY:
         return n->aux == RSLC_UN_NOT && spv_leaf(e, n->a);
      case RSLC_OP_CONVERT:
         return n->type->base == RSLC_BASE_BOOL && spv_leaf(e, n->a);
      case RSLC_OP_BINARY:
         switch (n->aux)
         {
            case RSLC_BIN_EQ: case RSLC_BIN_NE: case RSLC_BIN_LT: case RSLC_BIN_GT:
            case RSLC_BIN_LE: case RSLC_BIN_GE: case RSLC_BIN_XOR:
               return spv_leaf(e, n->a) && spv_leaf(e, n->b);
            default:
               return false;
         }
      case RSLC_OP_MEMBER:
         return spv_leaf(e, n->a);
      case RSLC_OP_INDEX:
         return spv_leaf(e, n->a) && spv_leaf(e, n->b);
      case RSLC_OP_BUILTIN:
         return (n->a == RSLC_BFN_ANY || n->a == RSLC_BFN_ALL)
            && rslc_list_len(e->c, n->b) == 1 && spv_leaf(e, rslc_list_item(e->c, n->b, 0));
      default:
         return false;
   }
}

/* Argument values as scalar components for a vector / matrix
 * constructor. */
static uint32_t spv_flatten(struct spv_emit *e, uint32_t node, const struct rslc_type *want,
      uint32_t *out, uint32_t max, uint8_t prec)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   const struct rslc_type *t = n->type;
   uint32_t v;
   uint32_t i, k = 0;
   if (!max)
      return 0;
   if (t->kind == RSLC_TY_SCALAR && n->op == RSLC_OP_CONST && t->base != want->base)
   {
      /* a literal of another base converts at compile time, as
       * glslang converts it: vec4(x, 0, 1) carries float 0 and 1 */
      struct rslc_cval cv;
      double d;
      if (!rslc_const_eval(e->c, node, &cv))
         return 0;
      d = t->base == RSLC_BASE_FLOAT ? cv.d[0]
         : t->base == RSLC_BASE_INT ? (double)(int32_t)cv.v[0] : (double)cv.v[0];
      if (want->base == RSLC_BASE_FLOAT)
         out[0] = spv_const_float(e, (float)d);
      else if (want->base == RSLC_BASE_BOOL)
         out[0] = spv_const_bits(e, want, d != 0.0);
      else if (want->base == RSLC_BASE_INT)
         out[0] = spv_const_int(e, t->base == RSLC_BASE_FLOAT ? (int32_t)d : (int32_t)cv.v[0]);
      else
         out[0] = spv_const_uint(e, t->base == RSLC_BASE_FLOAT ? (uint32_t)(int32_t)d : cv.v[0]);
      return out[0] ? 1 : 0;
   }
   v = spv_rvalue(e, node);
   if (!v)
      return 0;
   if (t->kind == RSLC_TY_SCALAR)
   {
      out[k++] = spv_cast(e, v, t, want, prec);
      return out[k - 1] ? k : 0;
   }
   if (t->kind == RSLC_TY_VEC)
   {
      for (i = 0; i < t->rows && k < max; i++)
      {
         uint32_t s = spv_extract(e, v, t, i, prec);
         if (!s)
            return 0;
         s = spv_cast(e, s, spv_scalar_of(e, t), want, prec);
         if (!s)
            return 0;
         out[k++] = s;
      }
      return k;
   }
   if (t->kind == RSLC_TY_MAT)
   {
      uint32_t c, r;
      const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
      for (c = 0; c < t->cols && k < max; c++)
      {
         uint32_t col = spv_column(e, v, t, c, prec);
         if (!col)
            return 0;
         for (r = 0; r < t->rows && k < max; r++)
         {
            uint32_t s = spv_extract(e, col, ct, r, prec);
            if (!s)
               return 0;
            out[k++] = spv_cast(e, s, spv_scalar_of(e, ct), want, prec);
            if (!out[k - 1])
               return 0;
         }
      }
      return k;
   }
   return 0;
}

static uint32_t spv_construct_node(struct spv_emit *e, const struct rslc_node *n)
{
   const struct rslc_type *t = n->type;
   uint32_t nargs = rslc_list_len(e->c, n->b);
   uint32_t prec  = n->prec;
   uint32_t comps[16];
   uint32_t nc = 0;

   switch (t->kind)
   {
      case RSLC_TY_SCALAR:
      {
         uint32_t a0 = rslc_list_item(e->c, n->b, 0);
         const struct rslc_type *at = rslc_node_at(e->c, a0)->type;
         uint32_t v = spv_rvalue(e, a0);
         if (!v)
            return 0;
         if (at->kind == RSLC_TY_VEC)
            v = spv_extract(e, v, at, 0, (uint8_t)prec);
         else if (at->kind == RSLC_TY_MAT)
         {
            uint32_t col = spv_column(e, v, at, 0, (uint8_t)prec);
            v = col ? spv_extract(e, col, rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, at->rows, 1), 0, (uint8_t)prec) : 0;
         }
         if (!v)
            return 0;
         return spv_cast(e, v, spv_scalar_of(e, at), t, (uint8_t)prec);
      }
      case RSLC_TY_VEC:
      {
         uint32_t a0 = rslc_list_item(e->c, n->b, 0);
         const struct rslc_type *at = rslc_node_at(e->c, a0)->type;
         const struct rslc_type *st = spv_scalar_of(e, t);
         uint32_t i;
         if (nargs == 1 && rslc_ty_components(at) == 1)
         {
            uint32_t v = spv_rvalue(e, a0);
            if (!v)
               return 0;
            v = spv_cast(e, v, at, st, (uint8_t)prec);
            if (!v)
               return 0;
            return spv_splat(e, v, t, (uint8_t)prec);
         }
         for (i = 0; i < nargs && nc < t->rows; i++)
         {
            uint32_t k = spv_flatten(e, rslc_list_item(e->c, n->b, i), st, comps + nc, t->rows - nc, (uint8_t)prec);
            if (!k)
               return 0;
            nc += k;
         }
         return spv_construct(e, t, comps, nc, (uint8_t)prec);
      }
      case RSLC_TY_MAT:
      {
         uint32_t a0 = rslc_list_item(e->c, n->b, 0);
         const struct rslc_type *at = rslc_node_at(e->c, a0)->type;
         const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
         const struct rslc_type *ft = spv_scalar_of(e, ct);
         uint32_t cols[4];
         uint32_t c, r;
         if (nargs == 1 && rslc_ty_components(at) == 1)
         {
            /* diagonal */
            uint32_t v = spv_rvalue(e, a0);
            uint32_t zero = spv_const_float(e, 0.0f);
            if (!v)
               return 0;
            v = spv_cast(e, v, at, ft, (uint8_t)prec);
            if (!v || !zero)
               return 0;
            for (c = 0; c < t->cols; c++)
            {
               uint32_t parts[4];
               for (r = 0; r < t->rows; r++)
                  parts[r] = (r == c) ? v : zero;
               cols[c] = spv_construct(e, ct, parts, t->rows, (uint8_t)prec);
               if (!cols[c])
                  return 0;
            }
            return spv_construct(e, t, cols, t->cols, (uint8_t)prec);
         }
         if (nargs == 1 && at->kind == RSLC_TY_MAT)
         {
            /* matrix from matrix: copy the overlap, identity elsewhere */
            uint32_t v = spv_rvalue(e, a0);
            uint32_t zero = spv_const_float(e, 0.0f);
            uint32_t one  = spv_const_float(e, 1.0f);
            const struct rslc_type *act = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, at->rows, 1);
            if (!v || !zero || !one)
               return 0;
            for (c = 0; c < t->cols; c++)
            {
               uint32_t parts[4];
               uint32_t src = c < at->cols ? spv_column(e, v, at, c, (uint8_t)prec) : 0;
               if (c < at->cols && !src)
                  return 0;
               for (r = 0; r < t->rows; r++)
               {
                  if (c < at->cols && r < at->rows)
                     parts[r] = spv_extract(e, src, act, r, (uint8_t)prec);
                  else
                     parts[r] = (r == c) ? one : zero;
                  if (!parts[r])
                     return 0;
               }
               cols[c] = spv_construct(e, ct, parts, t->rows, (uint8_t)prec);
               if (!cols[c])
                  return 0;
            }
            return spv_construct(e, t, cols, t->cols, (uint8_t)prec);
         }
         {
            uint32_t i;
            for (i = 0; i < nargs && nc < 16; i++)
            {
               uint32_t k = spv_flatten(e, rslc_list_item(e->c, n->b, i), ft, comps + nc, 16 - nc, (uint8_t)prec);
               if (!k)
                  return 0;
               nc += k;
            }
            for (c = 0; c < t->cols; c++)
            {
               cols[c] = spv_construct(e, ct, comps + c * t->rows, t->rows, (uint8_t)prec);
               if (!cols[c])
                  return 0;
            }
            return spv_construct(e, t, cols, t->cols, (uint8_t)prec);
         }
      }
      case RSLC_TY_STRUCT:
      case RSLC_TY_ARRAY:
      {
         uint32_t *parts = (uint32_t*)rslc_scratch_alloc(e->c, (nargs ? nargs : 1) * sizeof(*parts));
         uint32_t i;
         if (!parts)
            return 0;
         for (i = 0; i < nargs; i++)
         {
            parts[i] = spv_rvalue(e, rslc_list_item(e->c, n->b, i));
            if (!parts[i])
               return 0;
         }
         return spv_construct(e, t, parts, nargs, (uint8_t)prec);
      }
      default:
         return 0;
   }
}

/* ext-inst builtin map; 0 = core op handled elsewhere */
static uint32_t spv_bfn_ext(unsigned bfn, const struct rslc_type *t)
{
   bool f = t->base == RSLC_BASE_FLOAT;
   bool s = t->base == RSLC_BASE_INT;
   switch (bfn)
   {
      case RSLC_BFN_RADIANS: return GLSLstd450Radians;
      case RSLC_BFN_DEGREES: return GLSLstd450Degrees;
      case RSLC_BFN_SIN: return GLSLstd450Sin;
      case RSLC_BFN_COS: return GLSLstd450Cos;
      case RSLC_BFN_TAN: return GLSLstd450Tan;
      case RSLC_BFN_ASIN: return GLSLstd450Asin;
      case RSLC_BFN_ACOS: return GLSLstd450Acos;
      case RSLC_BFN_SINH: return GLSLstd450Sinh;
      case RSLC_BFN_COSH: return GLSLstd450Cosh;
      case RSLC_BFN_TANH: return GLSLstd450Tanh;
      case RSLC_BFN_ASINH: return GLSLstd450Asinh;
      case RSLC_BFN_ACOSH: return GLSLstd450Acosh;
      case RSLC_BFN_ATANH: return GLSLstd450Atanh;
      case RSLC_BFN_POW: return GLSLstd450Pow;
      case RSLC_BFN_EXP: return GLSLstd450Exp;
      case RSLC_BFN_LOG: return GLSLstd450Log;
      case RSLC_BFN_EXP2: return GLSLstd450Exp2;
      case RSLC_BFN_LOG2: return GLSLstd450Log2;
      case RSLC_BFN_SQRT: return GLSLstd450Sqrt;
      case RSLC_BFN_INVERSESQRT: return GLSLstd450InverseSqrt;
      case RSLC_BFN_ABS: return f ? GLSLstd450FAbs : GLSLstd450SAbs;
      case RSLC_BFN_SIGN: return f ? GLSLstd450FSign : GLSLstd450SSign;
      case RSLC_BFN_FLOOR: return GLSLstd450Floor;
      case RSLC_BFN_TRUNC: return GLSLstd450Trunc;
      case RSLC_BFN_ROUND: return GLSLstd450Round;
      case RSLC_BFN_ROUNDEVEN: return GLSLstd450RoundEven;
      case RSLC_BFN_CEIL: return GLSLstd450Ceil;
      case RSLC_BFN_FRACT: return GLSLstd450Fract;
      case RSLC_BFN_MIN: return f ? GLSLstd450FMin : s ? GLSLstd450SMin : GLSLstd450UMin;
      case RSLC_BFN_MAX: return f ? GLSLstd450FMax : s ? GLSLstd450SMax : GLSLstd450UMax;
      case RSLC_BFN_CLAMP: return f ? GLSLstd450FClamp : s ? GLSLstd450SClamp : GLSLstd450UClamp;
      case RSLC_BFN_MIX: return GLSLstd450FMix;
      case RSLC_BFN_STEP: return GLSLstd450Step;
      case RSLC_BFN_SMOOTHSTEP: return GLSLstd450SmoothStep;
      case RSLC_BFN_FMA: return GLSLstd450Fma;
      case RSLC_BFN_LDEXP: return GLSLstd450Ldexp;
      case RSLC_BFN_PACKUNORM2X16: return GLSLstd450PackUnorm2x16;
      case RSLC_BFN_PACKSNORM2X16: return GLSLstd450PackSnorm2x16;
      case RSLC_BFN_PACKUNORM4X8: return GLSLstd450PackUnorm4x8;
      case RSLC_BFN_PACKSNORM4X8: return GLSLstd450PackSnorm4x8;
      case RSLC_BFN_UNPACKUNORM2X16: return GLSLstd450UnpackUnorm2x16;
      case RSLC_BFN_UNPACKSNORM2X16: return GLSLstd450UnpackSnorm2x16;
      case RSLC_BFN_UNPACKUNORM4X8: return GLSLstd450UnpackUnorm4x8;
      case RSLC_BFN_UNPACKSNORM4X8: return GLSLstd450UnpackSnorm4x8;
      case RSLC_BFN_PACKHALF2X16: return GLSLstd450PackHalf2x16;
      case RSLC_BFN_UNPACKHALF2X16: return GLSLstd450UnpackHalf2x16;
      case RSLC_BFN_LENGTH: return GLSLstd450Length;
      case RSLC_BFN_DISTANCE: return GLSLstd450Distance;
      case RSLC_BFN_CROSS: return GLSLstd450Cross;
      case RSLC_BFN_NORMALIZE: return GLSLstd450Normalize;
      case RSLC_BFN_FACEFORWARD: return GLSLstd450FaceForward;
      case RSLC_BFN_REFLECT: return GLSLstd450Reflect;
      case RSLC_BFN_REFRACT: return GLSLstd450Refract;
      case RSLC_BFN_DETERMINANT: return GLSLstd450Determinant;
      case RSLC_BFN_INVERSE: return GLSLstd450MatrixInverse;
      case RSLC_BFN_FINDLSB: return GLSLstd450FindILsb;
      case RSLC_BFN_FINDMSB: return s ? GLSLstd450FindSMsb : GLSLstd450FindUMsb;
      default: return 0;
   }
}

/* Image sampling: sampled image id from a sampler var, coordinates,
 * optional operands. */
static uint32_t spv_image_op(struct spv_emit *e, enum spv_op op, uint32_t tid, uint32_t sampled,
      uint32_t coord, uint32_t extra, uint32_t mask, const uint32_t *ops, uint32_t nops, uint8_t prec)
{
   uint32_t w[10];
   uint32_t n = 0, i;
   w[n++] = sampled;
   w[n++] = coord;
   if (extra)
      w[n++] = extra;   /* gather component / fetch */
   if (mask)
   {
      w[n++] = mask;
      for (i = 0; i < nops; i++)
         w[n++] = ops[i];
   }
   return spv_fn_result(e, op, tid, w, n, prec);
}

#define SPV_IMG_BIAS        1u
#define SPV_IMG_LOD         2u
#define SPV_IMG_GRAD        4u
#define SPV_IMG_CONSTOFFSET 8u

static uint32_t spv_builtin_node(struct spv_emit *e, const struct rslc_node *n)
{
   uint32_t nargs = rslc_list_len(e->c, n->b);
   uint32_t args[6];
   const struct rslc_type *atypes[6];
   uint32_t i;
   uint32_t tid = spv_type(e, n->type);
   uint8_t  prec = n->prec;
   unsigned bfn  = n->a;
   const struct rslc_type *t0;

   if (!tid || nargs > 6)
      return 0;

   /* out-parameter builtins */
   if (bfn == RSLC_BFN_MODF || bfn == RSLC_BFN_FREXP)
   {
      uint32_t x = spv_rvalue(e, rslc_list_item(e->c, n->b, 0));
      unsigned sc; bool laid, std140;
      uint32_t ptr, w[2];
      if (!x)
         return 0;
      ptr = spv_lvalue(e, rslc_list_item(e->c, n->b, 1), &sc, &laid, &std140);
      if (!ptr)
         return 0;
      w[0] = x; w[1] = ptr;
      return spv_ext(e, tid, bfn == RSLC_BFN_MODF ? GLSLstd450Modf : GLSLstd450Frexp, w, 2, prec);
   }
   if (bfn == RSLC_BFN_UADDCARRY || bfn == RSLC_BFN_USUBBORROW || bfn == RSLC_BFN_UMULEXTENDED
         || bfn == RSLC_BFN_IMULEXTENDED)
   {
      const struct rslc_type *at = rslc_node_at(e->c, rslc_list_item(e->c, n->b, 0))->type;
      uint32_t atid = spv_type(e, at);
      uint32_t a = spv_rvalue(e, rslc_list_item(e->c, n->b, 0));
      uint32_t b = spv_rvalue(e, rslc_list_item(e->c, n->b, 1));
      uint32_t stw[3], st, r, lo, hi;
      unsigned sc; bool laid, std140;
      if (!a || !b || !atid)
         return 0;
      st = spv_id(e);
      stw[0] = st; stw[1] = atid; stw[2] = atid;
      if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeStruct, stw, 3))
         return 0;
      r = spv_r2(e, bfn == RSLC_BFN_UADDCARRY ? SpvOpIAddCarry : bfn == RSLC_BFN_USUBBORROW
            ? SpvOpISubBorrow : bfn == RSLC_BFN_UMULEXTENDED ? SpvOpUMulExtended : SpvOpSMulExtended,
            st, a, b, prec);
      if (!r)
         return 0;
      lo = spv_r2(e, SpvOpCompositeExtract, atid, r, 0, prec);
      hi = spv_r2(e, SpvOpCompositeExtract, atid, r, 1, prec);
      if (!lo || !hi)
         return 0;
      if (bfn == RSLC_BFN_UMULEXTENDED || bfn == RSLC_BFN_IMULEXTENDED)
      {
         uint32_t pm = spv_lvalue(e, rslc_list_item(e->c, n->b, 2), &sc, &laid, &std140);
         uint32_t pl = pm ? spv_lvalue(e, rslc_list_item(e->c, n->b, 3), &sc, &laid, &std140) : 0;
         if (!pl || !spv_store(e, pm, hi) || !spv_store(e, pl, lo))
            return 0;
         return lo;
      }
      {
         uint32_t pc = spv_lvalue(e, rslc_list_item(e->c, n->b, 2), &sc, &laid, &std140);
         if (!pc || !spv_store(e, pc, hi))
            return 0;
         return lo;
      }
   }

   for (i = 0; i < nargs; i++)
   {
      uint32_t an = rslc_list_item(e->c, n->b, i);
      atypes[i] = rslc_node_at(e->c, an)->type;
      args[i]   = spv_rvalue(e, an);
      if (!args[i])
         return 0;
   }
   t0 = nargs ? atypes[0] : n->type;

   /* texture family */
   if (bfn >= RSLC_BFN_TEXTURESIZE && bfn <= RSLC_BFN_TEXTUREGATHEROFFSET)
   {
      uint32_t img;
      const struct rslc_type *st = rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t0->base, 1, 1);
      uint32_t stid = spv_type(e, st);
      uint32_t imgt;
      bool     implicit_ok = e->c->stage == RSLC_STAGE_FRAGMENT;
      uint32_t lod0 = implicit_ok ? 0 : spv_const_float(e, 0.0f);
      if (!stid || (!implicit_ok && !lod0))
         return 0;
      /* image type id: the OpTypeImage made with the sampled image */
      imgt = e->tinfo[t0->id].img;
      switch (bfn)
      {
         case RSLC_BFN_TEXTURESIZE:
            e->need_imagequery = true;
            img = spv_r1(e, SpvOpImage, imgt, args[0], 0);
            if (!img)
               return 0;
            return spv_r2(e, SpvOpImageQuerySizeLod, tid, img, args[1], prec);
         case RSLC_BFN_TEXTUREQUERYLEVELS:
            e->need_imagequery = true;
            img = spv_r1(e, SpvOpImage, imgt, args[0], 0);
            if (!img)
               return 0;
            return spv_r1(e, SpvOpImageQueryLevels, tid, img, prec);
         case RSLC_BFN_TEXTUREQUERYLOD:
            e->need_imagequery = true;
            return spv_r2(e, SpvOpImageQueryLod, tid, args[0], args[1], prec);
         case RSLC_BFN_TEXELFETCH:
         case RSLC_BFN_TEXELFETCHOFFSET:
         {
            uint32_t ops[2];
            uint32_t mask = SPV_IMG_LOD;
            uint32_t nops = 1;
            img = spv_r1(e, SpvOpImage, imgt, args[0], 0);
            if (!img)
               return 0;
            ops[0] = args[2];
            if (bfn == RSLC_BFN_TEXELFETCHOFFSET)
            {
               mask |= SPV_IMG_CONSTOFFSET;
               ops[nops++] = args[3];
            }
            return spv_image_op(e, SpvOpImageFetch, tid, img, args[1], 0, mask, ops, nops, prec);
         }
         case RSLC_BFN_TEXTURE:
         case RSLC_BFN_TEXTUREOFFSET:
         {
            /* operand order: Bias, Lod, Grad, ConstOffset */
            uint32_t ops[2];
            uint32_t mask = 0, k = 0;
            bool has_bias = (bfn == RSLC_BFN_TEXTURE && nargs == 3) || (bfn == RSLC_BFN_TEXTUREOFFSET && nargs == 4);
            if (!implicit_ok)
            {
               mask |= SPV_IMG_LOD;
               ops[k++] = lod0;
            }
            else if (has_bias)
            {
               mask |= SPV_IMG_BIAS;
               ops[k++] = args[nargs - 1];
            }
            if (bfn == RSLC_BFN_TEXTUREOFFSET)
            {
               mask |= SPV_IMG_CONSTOFFSET;
               ops[k++] = args[2];
            }
            return spv_image_op(e, implicit_ok ? SpvOpImageSampleImplicitLod : SpvOpImageSampleExplicitLod,
                  tid, args[0], args[1], 0, mask, ops, k, prec);
         }
         case RSLC_BFN_TEXTURELOD:
         case RSLC_BFN_TEXTURELODOFFSET:
         {
            uint32_t ops[2]; uint32_t k = 0;
            uint32_t mask = SPV_IMG_LOD;
            ops[k++] = args[2];
            if (bfn == RSLC_BFN_TEXTURELODOFFSET)
            {
               mask |= SPV_IMG_CONSTOFFSET;
               ops[k++] = args[3];
            }
            return spv_image_op(e, SpvOpImageSampleExplicitLod, tid, args[0], args[1], 0, mask, ops, k, prec);
         }
         case RSLC_BFN_TEXTUREGRAD:
         case RSLC_BFN_TEXTUREGRADOFFSET:
         {
            uint32_t ops[3]; uint32_t k = 0;
            uint32_t mask = SPV_IMG_GRAD;
            ops[k++] = args[2];
            ops[k++] = args[3];
            if (bfn == RSLC_BFN_TEXTUREGRADOFFSET)
            {
               mask |= SPV_IMG_CONSTOFFSET;
               ops[k++] = args[4];
            }
            return spv_image_op(e, SpvOpImageSampleExplicitLod, tid, args[0], args[1], 0, mask, ops, k, prec);
         }
         case RSLC_BFN_TEXTUREPROJ:
         case RSLC_BFN_TEXTUREPROJOFFSET:
         case RSLC_BFN_TEXTUREPROJLOD:
         case RSLC_BFN_TEXTUREPROJLODOFFSET:
         case RSLC_BFN_TEXTUREPROJGRAD:
         case RSLC_BFN_TEXTUREPROJGRADOFFSET:
         {
            uint32_t ops[3]; uint32_t k = 0;
            uint32_t mask = 0;
            enum spv_op op = SpvOpImageSampleProjImplicitLod;
            if (bfn == RSLC_BFN_TEXTUREPROJLOD || bfn == RSLC_BFN_TEXTUREPROJLODOFFSET)
            {
               mask |= SPV_IMG_LOD; ops[k++] = args[2]; op = SpvOpImageSampleProjExplicitLod;
            }
            else if (bfn == RSLC_BFN_TEXTUREPROJGRAD || bfn == RSLC_BFN_TEXTUREPROJGRADOFFSET)
            {
               mask |= SPV_IMG_GRAD; ops[k++] = args[2]; ops[k++] = args[3]; op = SpvOpImageSampleProjExplicitLod;
            }
            else if (!implicit_ok)
            {
               mask |= SPV_IMG_LOD; ops[k++] = lod0; op = SpvOpImageSampleProjExplicitLod;
            }
            if (bfn == RSLC_BFN_TEXTUREPROJOFFSET || bfn == RSLC_BFN_TEXTUREPROJLODOFFSET
                  || bfn == RSLC_BFN_TEXTUREPROJGRADOFFSET)
            {
               mask |= SPV_IMG_CONSTOFFSET;
               ops[k++] = args[nargs - 1 - (bfn == RSLC_BFN_TEXTUREPROJOFFSET && nargs == 4 ? 1 : 0)];
            }
            return spv_image_op(e, op, tid, args[0], args[1], 0, mask, ops, k, prec);
         }
         case RSLC_BFN_TEXTUREGATHER:
         case RSLC_BFN_TEXTUREGATHEROFFSET:
         {
            uint32_t comp = (bfn == RSLC_BFN_TEXTUREGATHER && nargs == 3) ? args[2]
               : (bfn == RSLC_BFN_TEXTUREGATHEROFFSET && nargs == 4) ? args[3] : spv_const_int(e, 0);
            uint32_t ops[1];
            uint32_t mask = 0;
            if (!comp)
               return 0;
            if (bfn == RSLC_BFN_TEXTUREGATHEROFFSET)
            {
               mask = SPV_IMG_CONSTOFFSET;
               ops[0] = args[2];
            }
            return spv_image_op(e, SpvOpImageGather, tid, args[0], args[1], comp, mask, ops, mask ? 1 : 0, prec);
         }
         default:
            return 0;
      }
   }

   switch (bfn)
   {
      case RSLC_BFN_DOT:
         if (t0->kind == RSLC_TY_SCALAR)
            return spv_r2(e, SpvOpFMul, tid, args[0], args[1], prec);
         return spv_r2(e, SpvOpDot, tid, args[0], args[1], prec);
      case RSLC_BFN_MOD:
         if (atypes[1]->kind == RSLC_TY_SCALAR && t0->kind == RSLC_TY_VEC)
         {
            args[1] = spv_splat(e, args[1], t0, prec);
            if (!args[1])
               return 0;
         }
         return spv_r2(e, SpvOpFMod, tid, args[0], args[1], prec);
      case RSLC_BFN_ISNAN: return spv_r1(e, SpvOpIsNan, tid, args[0], prec);
      case RSLC_BFN_ISINF: return spv_r1(e, SpvOpIsInf, tid, args[0], prec);
      case RSLC_BFN_FLOATBITSTOINT: case RSLC_BFN_FLOATBITSTOUINT:
      case RSLC_BFN_INTBITSTOFLOAT: case RSLC_BFN_UINTBITSTOFLOAT:
         return spv_r1(e, SpvOpBitcast, tid, args[0], prec);
      case RSLC_BFN_ANY: return spv_r1(e, SpvOpAny, tid, args[0], prec);
      case RSLC_BFN_ALL: return spv_r1(e, SpvOpAll, tid, args[0], prec);
      case RSLC_BFN_NOT: return spv_r1(e, SpvOpLogicalNot, tid, args[0], prec);
      case RSLC_BFN_LESSTHAN: return spv_r2(e, spv_binop(t0, RSLC_BIN_LT), tid, args[0], args[1], prec);
      case RSLC_BFN_LESSTHANEQUAL: return spv_r2(e, spv_binop(t0, RSLC_BIN_LE), tid, args[0], args[1], prec);
      case RSLC_BFN_GREATERTHAN: return spv_r2(e, spv_binop(t0, RSLC_BIN_GT), tid, args[0], args[1], prec);
      case RSLC_BFN_GREATERTHANEQUAL: return spv_r2(e, spv_binop(t0, RSLC_BIN_GE), tid, args[0], args[1], prec);
      case RSLC_BFN_EQUAL: return spv_r2(e, spv_binop(t0, RSLC_BIN_EQ), tid, args[0], args[1], prec);
      case RSLC_BFN_NOTEQUAL: return spv_r2(e, spv_binop(t0, RSLC_BIN_NE), tid, args[0], args[1], prec);
      case RSLC_BFN_TRANSPOSE: return spv_r1(e, SpvOpTranspose, tid, args[0], prec);
      case RSLC_BFN_OUTERPRODUCT: return spv_r2(e, SpvOpOuterProduct, tid, args[0], args[1], prec);
      case RSLC_BFN_MATRIXCOMPMULT: return spv_mat_columnwise(e, SpvOpFMul, t0, args[0], args[1], true, prec);
      case RSLC_BFN_DFDX: return spv_r1(e, SpvOpDPdx, tid, args[0], prec);
      case RSLC_BFN_DFDY: return spv_r1(e, SpvOpDPdy, tid, args[0], prec);
      case RSLC_BFN_FWIDTH: return spv_r1(e, SpvOpFwidth, tid, args[0], prec);
      case RSLC_BFN_DFDXFINE: return spv_r1(e, SpvOpDPdxFine, tid, args[0], prec);
      case RSLC_BFN_DFDYFINE: return spv_r1(e, SpvOpDPdyFine, tid, args[0], prec);
      case RSLC_BFN_FWIDTHFINE: return spv_r1(e, SpvOpFwidthFine, tid, args[0], prec);
      case RSLC_BFN_DFDXCOARSE: return spv_r1(e, SpvOpDPdxCoarse, tid, args[0], prec);
      case RSLC_BFN_DFDYCOARSE: return spv_r1(e, SpvOpDPdyCoarse, tid, args[0], prec);
      case RSLC_BFN_FWIDTHCOARSE: return spv_r1(e, SpvOpFwidthCoarse, tid, args[0], prec);
      case RSLC_BFN_BITCOUNT: return spv_r1(e, SpvOpBitCount, tid, args[0], prec);
      case RSLC_BFN_BITFIELDREVERSE: return spv_r1(e, SpvOpBitReverse, tid, args[0], prec);
      case RSLC_BFN_BITFIELDEXTRACT:
         return spv_r3(e, t0->base == RSLC_BASE_INT ? SpvOpBitFieldSExtract : SpvOpBitFieldUExtract,
               tid, args[0], args[1], args[2], prec);
      case RSLC_BFN_BITFIELDINSERT:
      {
         uint32_t w[4];
         w[0] = args[0]; w[1] = args[1]; w[2] = args[2]; w[3] = args[3];
         return spv_fn_result(e, SpvOpBitFieldInsert, tid, w, 4, prec);
      }
      case RSLC_BFN_MIX:
         if (atypes[2]->base == RSLC_BASE_BOOL)
            return spv_r3(e, SpvOpSelect, tid, args[2], args[1], args[0], prec);
         break;
      case RSLC_BFN_ATAN:
         if (nargs == 2)
            return spv_ext(e, tid, GLSLstd450Atan2, args, 2, prec);
         return spv_ext(e, tid, GLSLstd450Atan, args, 1, prec);
      default:
         break;
   }

   /* The rest are GLSL.std.450 with every argument at the result's
    * width: scalars in mixed overloads are splatted. */
   {
      uint32_t inst = spv_bfn_ext(bfn, t0);
      if (!inst)
      {
         rslc_err(e->c, "spirv: unsupported builtin", NULL, n->line);
         return 0;
      }
      if (n->type->kind == RSLC_TY_VEC
            && (bfn == RSLC_BFN_MIN || bfn == RSLC_BFN_MAX || bfn == RSLC_BFN_CLAMP
               || bfn == RSLC_BFN_MIX || bfn == RSLC_BFN_STEP || bfn == RSLC_BFN_SMOOTHSTEP))
      {
         for (i = 0; i < nargs; i++)
            if (atypes[i]->kind == RSLC_TY_SCALAR)
            {
               const struct rslc_type *vt = rslc_type_basic(e->c, RSLC_TY_VEC, (enum rslc_base)atypes[i]->base, n->type->rows, 1);
               args[i] = spv_splat(e, args[i], vt, prec);
               if (!args[i])
                  return 0;
            }
      }
      return spv_ext(e, tid, inst, args, nargs, prec);
   }
}

static uint32_t spv_binary_value(struct spv_emit *e, const struct rslc_node *n, uint32_t a)
{
   const struct rslc_node *an = rslc_node_at(e->c, n->a);
   const struct rslc_node *bn = rslc_node_at(e->c, n->b);
   const struct rslc_type *ta = an->type;
   const struct rslc_type *tb = bn->type;
   const struct rslc_type *rt = n->type;
   uint32_t tid = spv_type(e, rt);
   uint8_t  prec = n->prec;
   uint16_t op = n->aux;
   uint32_t b;

   if (!tid)
      return 0;

   if ((op == RSLC_BIN_AND || op == RSLC_BIN_OR) && !spv_trivial_rhs(e, n->b))
   {
      /* short circuit: a && b  ==  a ? b : false */
      uint32_t lhs_label, rhs_label, merge, rhs_end;
      uint32_t w[3];
      uint32_t cv;
      a = spv_rvalue(e, n->a);
      if (!a || !spv_ensure_block(e))
         return 0;
      lhs_label = e->cur_label;
      rhs_label = spv_id(e);
      merge     = spv_id(e);
      if (!spv_inst2(e, SPV_SEC_FUNCS, SpvOpSelectionMerge, merge, 0))
         return 0;
      w[0] = a;
      if (op == RSLC_BIN_AND) { w[1] = rhs_label; w[2] = merge; }
      else                    { w[1] = merge;     w[2] = rhs_label; }
      if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpBranchConditional, w, 3))
         return 0;
      e->cur_label = 0;
      if (!spv_begin_block(e, rhs_label))
         return 0;
      b = spv_rvalue(e, n->b);
      if (!b || !spv_ensure_block(e))
         return 0;
      rhs_end = e->cur_label;
      if (!spv_branch(e, merge))
         return 0;
      if (!spv_begin_block(e, merge))
         return 0;
      cv = spv_const_bits(e, rt, op == RSLC_BIN_AND ? 0 : 1);
      if (!cv)
         return 0;
      return spv_phi2(e, tid, cv, lhs_label, b, rhs_end, prec);
   }

   if (!a)
      a = spv_rvalue(e, n->a);
   b = a ? spv_rvalue(e, n->b) : 0;
   if (!b)
      return 0;

   switch (op)
   {
      case RSLC_BIN_EQ:
      case RSLC_BIN_NE:
         return spv_equal(e, ta, a, b, op == RSLC_BIN_NE, prec);
      case RSLC_BIN_XOR:
         return spv_r2(e, SpvOpLogicalNotEqual, tid, a, b, prec);
      case RSLC_BIN_AND:
         return spv_r2(e, SpvOpLogicalAnd, tid, a, b, prec);
      case RSLC_BIN_OR:
         return spv_r2(e, SpvOpLogicalOr, tid, a, b, prec);
      case RSLC_BIN_LT: case RSLC_BIN_GT: case RSLC_BIN_LE: case RSLC_BIN_GE:
         return spv_r2(e, spv_binop(ta, op), tid, a, b, prec);
      default:
         break;
   }

   /* matrices */
   if (ta->kind == RSLC_TY_MAT || tb->kind == RSLC_TY_MAT)
   {
      if (op == RSLC_BIN_MUL)
      {
         if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_MAT)
            return spv_r2(e, SpvOpMatrixTimesMatrix, tid, a, b, prec);
         if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_VEC)
            return spv_r2(e, SpvOpMatrixTimesVector, tid, a, b, prec);
         if (ta->kind == RSLC_TY_VEC && tb->kind == RSLC_TY_MAT)
            return spv_r2(e, SpvOpVectorTimesMatrix, tid, a, b, prec);
         if (ta->kind == RSLC_TY_MAT)
            return spv_r2(e, SpvOpMatrixTimesScalar, tid, a, b, prec);
         return spv_r2(e, SpvOpMatrixTimesScalar, tid, b, a, prec);
      }
      /* matrix / scalar is the matrix times the reciprocal, as glslang
       * lowers it (there is no OpMatrixDivScalar), so the two modules
       * round alike */
      if (op == RSLC_BIN_DIV && ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_SCALAR)
      {
         uint32_t one = spv_const_float(e, 1.0f);
         uint32_t rb  = one ? spv_r2(e, SpvOpFDiv, spv_type(e, tb), one, b, prec) : 0;
         if (!rb)
            return 0;
         return spv_r2(e, SpvOpMatrixTimesScalar, tid, a, rb, prec);
      }
      /* + - / component-wise by column; a scalar operand is splatted */
      {
         const struct rslc_type *mt = ta->kind == RSLC_TY_MAT ? ta : tb;
         const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, mt->rows, 1);
         uint32_t cols[4];
         uint32_t i, ctid = spv_type(e, ct);
         uint32_t sa = ta->kind == RSLC_TY_SCALAR ? spv_splat(e, a, ct, prec) : 0;
         uint32_t sb = tb->kind == RSLC_TY_SCALAR ? spv_splat(e, b, ct, prec) : 0;
         if ((ta->kind == RSLC_TY_SCALAR && !sa) || (tb->kind == RSLC_TY_SCALAR && !sb))
            return 0;
         for (i = 0; i < mt->cols; i++)
         {
            uint32_t ca = sa ? sa : spv_column(e, a, ta, i, prec);
            uint32_t cb = sb ? sb : spv_column(e, b, tb, i, prec);
            if (!ca || !cb)
               return 0;
            cols[i] = spv_r2(e, spv_binop(ct, op), ctid, ca, cb, prec);
            if (!cols[i])
               return 0;
         }
         return spv_construct(e, mt, cols, mt->cols, prec);
      }
   }

   /* scalar op vector */
   if (ta->kind != tb->kind)
   {
      if (op == RSLC_BIN_MUL && rt->base == RSLC_BASE_FLOAT)
      {
         if (ta->kind == RSLC_TY_VEC)
            return spv_r2(e, SpvOpVectorTimesScalar, tid, a, b, prec);
         return spv_r2(e, SpvOpVectorTimesScalar, tid, b, a, prec);
      }
      if (op == RSLC_BIN_LSHIFT || op == RSLC_BIN_RSHIFT)
      {
         if (tb->kind == RSLC_TY_SCALAR)
         {
            b = spv_splat(e, b, rslc_type_basic(e->c, RSLC_TY_VEC, (enum rslc_base)tb->base, ta->rows, 1), prec);
            if (!b)
               return 0;
         }
         return spv_r2(e, spv_binop(ta, op), tid, a, b, prec);
      }
      if (ta->kind == RSLC_TY_SCALAR)
         a = spv_splat(e, a, rt, prec);
      else
         b = spv_splat(e, b, rt, prec);
      if (!a || !b)
         return 0;
   }
   else if ((op == RSLC_BIN_LSHIFT || op == RSLC_BIN_RSHIFT) && ta->kind == RSLC_TY_VEC && tb->base != ta->base)
   {
      /* shift amount of the other signedness: bitcast */
      uint32_t btid = spv_type(e, rslc_type_basic(e->c, RSLC_TY_VEC, (enum rslc_base)ta->base, ta->rows, 1));
      b = spv_r1(e, SpvOpBitcast, btid, b, prec);
      if (!b)
         return 0;
   }
   return spv_r2(e, spv_binop(rt, op), tid, a, b, prec);
}

static uint32_t spv_binary_node(struct spv_emit *e, const struct rslc_node *n)
{
   return spv_binary_value(e, n, 0);
}

static bool spv_runtime_binary(const struct rslc_node *n)
{
   return n->op == RSLC_OP_BINARY && n->nonconstant
      && n->aux != RSLC_BIN_AND && n->aux != RSLC_BIN_OR;
}

static uint32_t spv_rvalue_impl(struct spv_emit *e, uint32_t node);

static uint32_t spv_rvalue(struct spv_emit *e, uint32_t node)
{
   uint32_t r;
   const struct rslc_node *n = rslc_node_at(e->c, node);
   if (spv_runtime_binary(n) && spv_runtime_binary(rslc_node_at(e->c, n->a)))
   {
      uint32_t at = node, count = 0, i;
      uint32_t *chain;
      while (spv_runtime_binary(rslc_node_at(e->c, at)))
      {
         count++;
         at = rslc_node_at(e->c, at)->a;
      }
      chain = (uint32_t*)rslc_arena_alloc(e->c, (size_t)count * sizeof(*chain));
      if (!chain)
         return 0;
      at = node;
      for (i = 0; i < count; i++)
      {
         chain[i] = at;
         at = rslc_node_at(e->c, at)->a;
      }
      r = spv_rvalue(e, at);
      while (count && r)
      {
         count--;
         r = spv_binary_value(e, rslc_node_at(e->c, chain[count]), r);
      }
   }
   else
      r = spv_rvalue_impl(e, node);
#ifdef RSLC_SPV_TRACE
   if (!r)
   {
      const struct rslc_node *n = rslc_node_at(e->c, node);
      fprintf(stderr, "spv: op %u aux %u a %u kind %u base %u fails at line %u", n->op, n->aux, n->a,
            n->type ? n->type->kind : 99, n->type ? n->type->base : 99, n->line);
      if (n->op == RSLC_OP_VAR)
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         fprintf(stderr, " var %s storage %u global %u", v->name ? v->name->str : "?", v->storage, v->global);
      }
      fputc('\n', stderr);
   }
#endif
   return r;
}

static uint32_t spv_rvalue_impl(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   uint8_t prec = n->prec;

   /* Constant expressions fold to constants, as glslang does; image
    * offsets in particular must be constant objects. */
   if (n->type && n->op != RSLC_OP_CONST && n->op != RSLC_OP_VAR
         && (n->op == RSLC_OP_CONSTRUCT || n->op == RSLC_OP_UNARY || n->op == RSLC_OP_BINARY
            || n->op == RSLC_OP_SWIZZLE || n->op == RSLC_OP_CONVERT || n->op == RSLC_OP_BUILTIN
            || n->op == RSLC_OP_MEMBER || n->op == RSLC_OP_INDEX)
         && (n->type->kind == RSLC_TY_SCALAR || n->type->kind == RSLC_TY_VEC || n->type->kind == RSLC_TY_MAT))
   {
      struct rslc_cval cv;
      if (rslc_const_eval(e->c, node, &cv))
      {
         uint32_t consumed;
         return spv_const_composite(e, n->type, cv.v, &consumed);
      }
   }

   switch (n->op)
   {
      case RSLC_OP_CONST:
         return spv_const_bits(e, n->type, n->a);
      case RSLC_OP_VAR:
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         unsigned sc; bool laid, std140;
         uint32_t ptr;
         if (e->vars[v->id].is_const)
            return spv_const_var(e, v);
         ptr = spv_var_ptr(e, v, &sc, &laid, &std140);
         if (!ptr)
            return 0;
         return spv_load_from(e, n->type, ptr, laid, std140, prec);
      }
      case RSLC_OP_MEMBER:
      case RSLC_OP_INDEX:
      {
         const struct rslc_node *base = rslc_node_at(e->c, n->a);
         /* Constant index on a non-pointer value: extract. */
         bool const_idx = n->op == RSLC_OP_MEMBER
            || rslc_node_at(e->c, n->b)->op == RSLC_OP_CONST;
         bool base_is_ptr = base->op == RSLC_OP_VAR || base->op == RSLC_OP_MEMBER
            || base->op == RSLC_OP_INDEX || (base->op == RSLC_OP_SWIZZLE && (base->aux & 3u) == 1);
         if (base->op == RSLC_OP_VAR && e->vars[e->c->prog.vars[base->a]->id].is_const)
            base_is_ptr = false;
         if (!base_is_ptr && const_idx)
         {
            uint32_t bv  = spv_rvalue(e, n->a);
            uint32_t idx = n->op == RSLC_OP_MEMBER ? n->b : rslc_node_at(e->c, n->b)->a;
            uint32_t tid = spv_type(e, n->type);
            if (!bv || !tid)
               return 0;
            return spv_r2(e, SpvOpCompositeExtract, tid, bv, idx, prec);
         }
         if (!base_is_ptr && base->type->kind == RSLC_TY_VEC)
         {
            uint32_t bv  = spv_rvalue(e, n->a);
            uint32_t idx = spv_rvalue(e, n->b);
            uint32_t tid = spv_type(e, n->type);
            if (!bv || !idx || !tid)
               return 0;
            return spv_r2(e, SpvOpVectorExtractDynamic, tid, bv, idx, prec);
         }
         {
            unsigned sc; bool laid, std140;
            uint32_t ptr = spv_lvalue(e, node, &sc, &laid, &std140);
            if (!ptr)
               return 0;
            return spv_load_from(e, n->type, ptr, laid, std140, prec);
         }
      }
      case RSLC_OP_SWIZZLE:
      {
         const struct rslc_node *base = rslc_node_at(e->c, n->a);
         unsigned cnt = n->aux & 3u, i;
         uint32_t bv = spv_rvalue(e, n->a);
         uint32_t tid = spv_type(e, n->type);
         if (!bv || !tid)
            return 0;
         if (!cnt)
            cnt = 4;
         if (base->type->kind == RSLC_TY_SCALAR)
         {
            uint32_t w[4];
            if (cnt == 1)
               return bv;
            for (i = 0; i < cnt; i++)
               w[i] = bv;
            return spv_fn_result(e, SpvOpCompositeConstruct, tid, w, cnt, prec);
         }
         if (cnt == 1)
            return spv_r2(e, SpvOpCompositeExtract, tid, bv, (n->aux >> 2) & 3u, prec);
         {
            uint32_t w[6];
            w[0] = bv; w[1] = bv;
            for (i = 0; i < cnt; i++)
               w[2 + i] = (n->aux >> (2 + 2 * i)) & 3u;
            return spv_fn_result(e, SpvOpVectorShuffle, tid, w, 2 + cnt, prec);
         }
      }
      case RSLC_OP_CALL:
      {
         const struct rslc_func *f = e->c->prog.funcs[n->a];
         uint32_t nargs = rslc_list_len(e->c, n->b);
         uint32_t temps[64];
         uint32_t words[66];
         uint32_t i, tid, id;
         if (nargs > 64)
            return 0;
         for (i = 0; i < nargs; i++)
         {
            uint32_t an = rslc_list_item(e->c, n->b, i);
            const struct rslc_var *pv = f->params[i];
            if (pv->type->kind == RSLC_TY_SAMPLER2D)
            {
               unsigned sc; bool laid, std140;
               temps[i] = spv_lvalue(e, an, &sc, &laid, &std140);
               if (!temps[i])
                  return 0;
               continue;
            }
            temps[i] = spv_local(e, pv->type, NULL, pv->precision);
            if (!temps[i])
               return 0;
            if (pv->dir != RSLC_DIR_OUT)
            {
               uint32_t v = spv_rvalue(e, an);
               if (!v || !spv_store(e, temps[i], v))
                  return 0;
            }
         }
         tid = spv_type(e, f->ret);
         if (!tid)
            return 0;
         id = spv_id(e);
         words[0] = tid; words[1] = id; words[2] = e->func_ids[f->id];
         for (i = 0; i < nargs; i++)
            words[3 + i] = temps[i];
         if (!spv_fn_inst(e, SpvOpFunctionCall, words, 3 + nargs))
            return 0;
         if (prec == RSLC_PREC_LOW || prec == RSLC_PREC_MEDIUM)
            if (!spv_decorate(e, id, SPV_DEC_RELAXEDPRECISION))
               return 0;
         for (i = 0; i < nargs; i++)
         {
            const struct rslc_var *pv = f->params[i];
            if (pv->dir != RSLC_DIR_IN && pv->type->kind != RSLC_TY_SAMPLER2D)
            {
               uint32_t v = spv_load(e, pv->type, temps[i], pv->precision);
               if (!v || !spv_assign_to(e, rslc_list_item(e->c, n->b, i), v))
                  return 0;
            }
         }
         return id;
      }
      case RSLC_OP_BUILTIN:
         return spv_builtin_node(e, n);
      case RSLC_OP_CONSTRUCT:
         return spv_construct_node(e, n);
      case RSLC_OP_UNARY:
      {
         const struct rslc_node *an = rslc_node_at(e->c, n->a);
         uint32_t tid = spv_type(e, n->type);
         if (!tid)
            return 0;
         switch (n->aux)
         {
            case RSLC_UN_NEG:
            {
               uint32_t v = spv_rvalue(e, n->a);
               if (!v)
                  return 0;
               if (n->type->kind == RSLC_TY_MAT)
               {
                  uint32_t cols[4];
                  uint32_t i;
                  const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, n->type->rows, 1);
                  uint32_t ctid = spv_type(e, ct);
                  for (i = 0; i < n->type->cols; i++)
                  {
                     uint32_t col = spv_column(e, v, n->type, i, prec);
                     cols[i] = col ? spv_r1(e, SpvOpFNegate, ctid, col, prec) : 0;
                     if (!cols[i])
                        return 0;
                  }
                  return spv_construct(e, n->type, cols, n->type->cols, prec);
               }
               return spv_r1(e, n->type->base == RSLC_BASE_FLOAT ? SpvOpFNegate : SpvOpSNegate, tid, v, prec);
            }
            case RSLC_UN_NOT:
            {
               uint32_t v = spv_rvalue(e, n->a);
               return v ? spv_r1(e, SpvOpLogicalNot, tid, v, prec) : 0;
            }
            case RSLC_UN_BITNOT:
            {
               uint32_t v = spv_rvalue(e, n->a);
               return v ? spv_r1(e, SpvOpNot, tid, v, prec) : 0;
            }
            default:
            {
               /* ++ / -- */
               unsigned sc; bool laid, std140;
               uint32_t ptr = spv_lvalue(e, n->a, &sc, &laid, &std140);
               uint32_t old, one, nv;
               const struct rslc_type *t = an->type;
               bool inc = n->aux == RSLC_UN_PREINC || n->aux == RSLC_UN_POSTINC;
               bool post = n->aux == RSLC_UN_POSTINC || n->aux == RSLC_UN_POSTDEC;
               if (!ptr)
                  return 0;
               old = spv_load(e, t, ptr, prec);
               if (!old)
                  return 0;
               if (t->base == RSLC_BASE_FLOAT)
                  one = spv_const_float(e, 1.0f);
               else if (t->base == RSLC_BASE_INT)
                  one = spv_const_int(e, 1);
               else
                  one = spv_const_uint(e, 1);
               if (!one)
                  return 0;
               if (t->kind == RSLC_TY_VEC)
                  one = spv_splat(e, one, t, prec);
               if (t->kind == RSLC_TY_MAT)
               {
                  const struct rslc_type *ct = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
                  one = spv_splat(e, one, ct, prec);
                  nv  = one ? spv_mat_columnwise(e, inc ? SpvOpFAdd : SpvOpFSub, t, old, one, false, prec) : 0;
               }
               else
                  nv = one ? spv_r2(e, spv_binop(t, inc ? RSLC_BIN_ADD : RSLC_BIN_SUB), tid, old, one, prec) : 0;
               if (!nv || !spv_store(e, ptr, nv))
                  return 0;
               return post ? old : nv;
            }
         }
      }
      case RSLC_OP_BINARY:
         return spv_binary_node(e, n);
      case RSLC_OP_ASSIGN:
      {
         uint32_t val;
         if (n->aux == RSLC_BIN_NONE)
            val = spv_rvalue(e, n->b);
         else
         {
            /* a op= b  as  a = a op b, through a BINARY-shaped temp node */
            struct rslc_node tmp;
            const struct rslc_node *lhs = rslc_node_at(e->c, n->a);
            memset(&tmp, 0, sizeof(tmp));
            tmp.op   = RSLC_OP_BINARY;
            tmp.aux  = n->aux;
            tmp.a    = n->a;
            tmp.b    = n->b;
            tmp.type = lhs->type;
            tmp.prec = n->prec;
            tmp.line = n->line;
            if (n->aux == RSLC_BIN_MUL && lhs->type->kind != RSLC_TY_MAT
                  && rslc_node_at(e->c, n->b)->type->kind == RSLC_TY_MAT)
               tmp.type = lhs->type;
            val = spv_binary_node(e, &tmp);
         }
         if (!val || !spv_assign_to(e, n->a, val))
            return 0;
         return val;
      }
      case RSLC_OP_TERNARY:
      {
         uint32_t tid = spv_type(e, n->type);
         uint32_t cond = spv_rvalue(e, n->a);
         if (!tid || !cond)
            return 0;
         if ((n->type->kind == RSLC_TY_SCALAR || n->type->kind == RSLC_TY_VEC)
               && spv_leaf(e, n->b) && spv_leaf(e, n->c))
         {
            uint32_t a = spv_rvalue(e, n->b);
            uint32_t b = a ? spv_rvalue(e, n->c) : 0;
            if (!b)
               return 0;
            if (n->type->kind == RSLC_TY_VEC)
            {
               cond = spv_splat(e, cond, rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_BOOL, n->type->rows, 1), prec);
               if (!cond)
                  return 0;
            }
            return spv_r3(e, SpvOpSelect, tid, cond, a, b, prec);
         }
         {
            uint32_t then_l = spv_id(e), else_l = spv_id(e), merge = spv_id(e);
            uint32_t a, b, la, lb;
            uint32_t w[3];
            if (!spv_ensure_block(e))
               return 0;
            if (!spv_inst2(e, SPV_SEC_FUNCS, SpvOpSelectionMerge, merge, 0))
               return 0;
            w[0] = cond; w[1] = then_l; w[2] = else_l;
            if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpBranchConditional, w, 3))
               return 0;
            e->cur_label = 0;
            if (!spv_begin_block(e, then_l))
               return 0;
            a = spv_rvalue(e, n->b);
            if (!a || !spv_ensure_block(e))
               return 0;
            la = e->cur_label;
            if (!spv_branch(e, merge) || !spv_begin_block(e, else_l))
               return 0;
            b = spv_rvalue(e, n->c);
            if (!b || !spv_ensure_block(e))
               return 0;
            lb = e->cur_label;
            if (!spv_branch(e, merge) || !spv_begin_block(e, merge))
               return 0;
            return spv_phi2(e, tid, a, la, b, lb, prec);
         }
      }
      case RSLC_OP_SEQ:
         if (!spv_rvalue(e, n->a))
            return 0;
         return spv_rvalue(e, n->b);
      case RSLC_OP_CONVERT:
      {
         const struct rslc_node *an = rslc_node_at(e->c, n->a);
         uint32_t v = spv_rvalue(e, n->a);
         if (!v)
            return 0;
         if (an->type->kind == RSLC_TY_ARRAY || an->type->kind == RSLC_TY_STRUCT)
            return v;
         return spv_convert(e, v, an->type, n->type, prec);
      }
      default:
         rslc_err(e->c, "spirv: unsupported expression", NULL, n->line);
         return 0;
   }
}

/* ---- statements ---- */

static bool spv_stmt_list(struct spv_emit *e, uint32_t list)
{
   uint32_t i, n = list ? rslc_list_len(e->c, list) : 0;
   for (i = 0; i < n; i++)
      if (!spv_stmt(e, rslc_list_item(e->c, list, i)))
         return false;
   return true;
}

static bool spv_stmt(struct spv_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   struct rslc_mark mark;
   bool ok;

   rslc_scratch_mark(e->c, &mark);
   switch (n->op)
   {
      case RSLC_OP_BLOCK:
         ok = spv_stmt_list(e, n->b);
         break;
      case RSLC_OP_DECL:
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         struct spv_var *sv = &e->vars[v->id];
         uint32_t val;
         ok = true;
         if (sv->is_const)
            break;   /* a folded local const needs no storage */
         if (v->storage == RSLC_ST_CONST && v->init && !sv->id && spv_node_is_const(e, v->init))
         {
            /* a local const with a constant initialiser is the
             * constant, as glslang treats it: no variable, no store,
             * and nothing at all when it is never read */
            sv->is_const = true;
            break;
         }
         if (!sv->id)
         {
            sv->id = spv_local(e, v->type, v->name ? v->name->str : NULL, v->precision);
            sv->sc = SPV_SC_FUNCTION;
            if (!sv->id)
            {
               ok = false;
               break;
            }
         }
         if (v->init)
         {
            val = spv_rvalue(e, v->init);
            ok  = val && spv_store(e, sv->id, val);
         }
         break;
      }
      case RSLC_OP_EXPR:
         ok = spv_rvalue(e, n->a) != 0;
         break;
      case RSLC_OP_IF:
      {
         uint32_t cond = spv_rvalue(e, n->a);
         uint32_t then_l, else_l, merge;
         uint32_t w[3];
         if (!cond || !spv_ensure_block(e))
         {
            ok = false;
            break;
         }
         then_l = spv_id(e);
         merge  = spv_id(e);
         else_l = n->c ? spv_id(e) : merge;
         if (!spv_inst2(e, SPV_SEC_FUNCS, SpvOpSelectionMerge, merge, 0))
         {
            ok = false;
            break;
         }
         w[0] = cond; w[1] = then_l; w[2] = else_l;
         ok = spv_inst(e, SPV_SEC_FUNCS, SpvOpBranchConditional, w, 3);
         e->cur_label = 0;
         ok = ok && spv_begin_block(e, then_l) && spv_stmt(e, n->b);
         if (ok && e->cur_label)
            ok = spv_branch(e, merge);
         if (ok && n->c)
         {
            ok = spv_begin_block(e, else_l) && spv_stmt(e, n->c);
            if (ok && e->cur_label)
               ok = spv_branch(e, merge);
         }
         ok = ok && spv_begin_block(e, merge);
         break;
      }
      case RSLC_OP_FOR:
      case RSLC_OP_WHILE:
      case RSLC_OP_DO:
      {
         uint32_t init = 0, cond = 0, iter = 0, body;
         uint32_t header = spv_id(e), check = spv_id(e), body_l = spv_id(e);
         uint32_t cont = spv_id(e), merge = spv_id(e);
         uint32_t w[3];
         if (n->op == RSLC_OP_FOR)
         {
            init = rslc_list_item(e->c, n->b, 0);
            cond = rslc_list_item(e->c, n->b, 1);
            iter = rslc_list_item(e->c, n->b, 2);
            body = rslc_list_item(e->c, n->b, 3);
         }
         else if (n->op == RSLC_OP_WHILE)
         {
            cond = n->a; body = n->b;
         }
         else
         {
            body = n->a; cond = n->b;
         }
         ok = true;
         if (init)
            ok = spv_stmt(e, init);
         ok = ok && spv_branch(e, header) && spv_begin_block(e, header);
         if (ok)
         {
            w[0] = merge; w[1] = cont; w[2] = 0;
            ok = spv_inst(e, SPV_SEC_FUNCS, SpvOpLoopMerge, w, 3);
         }
         if (e->nctl == 64)
            ok = false;
         if (!ok)
            break;
         e->ctl[e->nctl].merge = merge;
         e->ctl[e->nctl].cont  = cont;
         e->nctl++;
         if (n->op == RSLC_OP_DO)
         {
            /* header -> body; continue block evaluates the condition */
            ok = spv_branch(e, body_l) && spv_begin_block(e, body_l) && spv_stmt(e, body);
            if (ok && e->cur_label)
               ok = spv_branch(e, cont);
            ok = ok && spv_begin_block(e, cont);
            if (ok)
            {
               uint32_t cv = spv_rvalue(e, cond);
               ok = cv != 0 && spv_ensure_block(e);
               if (ok)
               {
                  w[0] = cv; w[1] = header; w[2] = merge;
                  ok = spv_inst(e, SPV_SEC_FUNCS, SpvOpBranchConditional, w, 3);
                  e->cur_label = 0;
               }
            }
         }
         else
         {
            ok = spv_branch(e, check) && spv_begin_block(e, check);
            if (ok)
            {
               if (cond)
               {
                  uint32_t cv = spv_rvalue(e, cond);
                  ok = cv != 0 && spv_ensure_block(e);
                  if (ok)
                  {
                     w[0] = cv; w[1] = body_l; w[2] = merge;
                     ok = spv_inst(e, SPV_SEC_FUNCS, SpvOpBranchConditional, w, 3);
                     e->cur_label = 0;
                  }
               }
               else
                  ok = spv_branch(e, body_l);
            }
            ok = ok && spv_begin_block(e, body_l) && spv_stmt(e, body);
            if (ok && e->cur_label)
               ok = spv_branch(e, cont);
            ok = ok && spv_begin_block(e, cont);
            if (ok && iter)
               ok = spv_rvalue(e, iter) != 0;
            ok = ok && spv_branch(e, header);
         }
         e->nctl--;
         ok = ok && spv_begin_block(e, merge);
         break;
      }
      case RSLC_OP_SWITCH:
      {
         uint32_t sel = spv_rvalue(e, n->a);
         uint32_t merge = spv_id(e);
         uint32_t nst = n->b ? rslc_list_len(e->c, n->b) : 0;
         uint32_t ncase = 0, i;
         uint32_t *labels;
         uint32_t default_l = merge;
         uint32_t words[2 + 2 * 64];
         uint32_t nw;
         if (!sel || !spv_ensure_block(e))
         {
            ok = false;
            break;
         }
         for (i = 0; i < nst; i++)
            if (rslc_node_at(e->c, rslc_list_item(e->c, n->b, i))->op == RSLC_OP_CASE)
               ncase++;
         if (ncase > 64)
         {
            ok = false;
            break;
         }
         labels = (uint32_t*)rslc_scratch_alloc(e->c, (ncase ? ncase : 1) * sizeof(*labels));
         if (!labels)
         {
            ok = false;
            break;
         }
         nw = 2;
         words[0] = sel;
         {
            uint32_t k = 0;
            for (i = 0; i < nst; i++)
            {
               const struct rslc_node *cs = rslc_node_at(e->c, rslc_list_item(e->c, n->b, i));
               if (cs->op != RSLC_OP_CASE)
                  continue;
               labels[k] = spv_id(e);
               if (cs->a)
               {
                  words[nw++] = rslc_node_at(e->c, cs->a)->a;
                  words[nw++] = labels[k];
               }
               else
                  default_l = labels[k];
               k++;
            }
         }
         words[1] = default_l;
         if (!spv_inst2(e, SPV_SEC_FUNCS, SpvOpSelectionMerge, merge, 0)
               || !spv_inst(e, SPV_SEC_FUNCS, SpvOpSwitch, words, nw))
         {
            ok = false;
            break;
         }
         e->cur_label = 0;
         e->ctl[e->nctl].merge = merge;
         e->ctl[e->nctl].cont  = e->nctl ? e->ctl[e->nctl - 1].cont : 0;
         e->nctl++;
         ok = true;
         {
            uint32_t k = 0;
            for (i = 0; ok && i < nst; i++)
            {
               uint32_t sn = rslc_list_item(e->c, n->b, i);
               const struct rslc_node *cs = rslc_node_at(e->c, sn);
               if (cs->op == RSLC_OP_CASE)
               {
                  if (e->cur_label)
                     ok = spv_branch(e, labels[k]);   /* fallthrough */
                  ok = ok && spv_begin_block(e, labels[k]);
                  k++;
                  continue;
               }
               if (!e->cur_label)
                  continue;   /* unreachable after a break */
               ok = spv_stmt(e, sn);
            }
         }
         e->nctl--;
         if (ok && e->cur_label)
            ok = spv_branch(e, merge);
         ok = ok && spv_begin_block(e, merge);
         break;
      }
      case RSLC_OP_BREAK:
         ok = e->nctl && spv_branch(e, e->ctl[e->nctl - 1].merge);
         break;
      case RSLC_OP_CONTINUE:
      {
         uint32_t i = e->nctl;
         ok = false;
         while (i--)
            if (e->ctl[i].cont)
            {
               ok = spv_branch(e, e->ctl[i].cont);
               break;
            }
         break;
      }
      case RSLC_OP_RETURN:
         if (n->a)
         {
            uint32_t v = spv_rvalue(e, n->a);
            ok = v && spv_fn_inst(e, SpvOpReturnValue, &v, 1);
         }
         else
            ok = spv_fn_inst(e, SpvOpReturn, NULL, 0);
         e->cur_label = 0;
         break;
      case RSLC_OP_DISCARD:
         ok = spv_fn_inst(e, SpvOpKill, NULL, 0);
         e->cur_label = 0;
         break;
      case RSLC_OP_CASE:
         ok = true;   /* handled by SWITCH */
         break;
      default:
         rslc_err(e->c, "spirv: unsupported statement", NULL, n->line);
         ok = false;
         break;
   }
   rslc_scratch_release(e->c, &mark);
   if (!ok && !e->c->failed)
      rslc_err(e->c, "spirv: cannot emit statement", NULL, n->line);
   return ok;
}

/* ---- functions ---- */

/* OpTypeFunction must be unique per signature: sig = return type then
 * parameter pointer types. */
static uint32_t spv_fntype(struct spv_emit *e, const uint32_t *sig, uint32_t n)
{
   uint32_t i = 0, id;
   uint32_t w[70];
   while (i < e->fntypes.n)
   {
      uint32_t fid = e->fntypes.v[i];
      uint32_t fn  = e->fntypes.v[i + 1];
      if (fn == n && !memcmp(e->fntypes.v + i + 2, sig, n * sizeof(*sig)))
         return fid;
      i += 2 + fn;
   }
   if (n > 69)
      return 0;
   id   = spv_id(e);
   w[0] = id;
   memcpy(w + 1, sig, n * sizeof(*sig));
   if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeFunction, w, n + 1))
      return 0;
   if (!spv_w(e, &e->fntypes, id) || !spv_w(e, &e->fntypes, n))
      return 0;
   for (i = 0; i < n; i++)
      if (!spv_w(e, &e->fntypes, sig[i]))
         return 0;
   return id;
}

static bool spv_function(struct spv_emit *e, const struct rslc_func *f)
{
   uint32_t rtid = spv_type(e, f->ret);
   uint32_t ftype;
   uint32_t w[70];
   uint32_t i;
   uint32_t fid = e->func_ids[f->id];
   struct rslc_wbuf *fb = &e->sec[SPV_SEC_FUNCS];
   uint32_t entry_pos;

   if (!rtid || f->nparams > 64)
      return false;
   /* function type */
   w[0] = 0; w[1] = rtid;
   for (i = 0; i < f->nparams; i++)
   {
      w[2 + i] = spv_ptr_to(e, f->params[i]->type, spv_param_sc(f->params[i]->type));
      if (!w[2 + i])
         return false;
   }
   ftype = spv_fntype(e, w + 1, 1 + f->nparams);
   if (!ftype)
      return false;

   w[0] = rtid; w[1] = fid; w[2] = 0; w[3] = ftype;
   if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpFunction, w, 4))
      return false;
   for (i = 0; i < f->nparams; i++)
   {
      const struct rslc_var *pv = f->params[i];
      uint32_t ptr = spv_ptr_to(e, pv->type, spv_param_sc(pv->type));
      uint32_t pid = spv_id(e);
      if (!ptr || !spv_inst2(e, SPV_SEC_FUNCS, SpvOpFunctionParameter, ptr, pid))
         return false;
      e->vars[pv->id].id = pid;
      e->vars[pv->id].sc = (uint8_t)spv_param_sc(pv->type);
      if (pv->name && !spv_name(e, pid, pv->name->str))
         return false;
   }
   e->entry_label = spv_id(e);
   e->cur_fn      = f->id;
   e->func_vars.n = 0;
   if (!spv_begin_block(e, e->entry_label))
      return false;
   /* Function variables are hoisted into the entry block: remember
    * where to splice them. */
   entry_pos = fb->n;

   e->in_main = f == e->c->prog.main_fn;
   if (e->in_main)
   {
      /* global initialisers run at the start of main */
      uint32_t k;
      for (k = 1; k < e->c->prog.nvars; k++)
      {
         const struct rslc_var *v = e->c->prog.vars[k];
         if (v->global && (v->storage == RSLC_ST_GLOBAL || v->storage == RSLC_ST_CONST)
               && v->init && e->vars[k].id && !e->vars[k].is_const)
         {
            uint32_t val = spv_rvalue(e, v->init);
            if (!val || !spv_store(e, e->vars[k].id, val))
               return false;
         }
      }
   }
   if (!spv_stmt(e, f->body))
      return false;
   if (e->cur_label)
   {
      if (f->ret->kind == RSLC_TY_VOID)
      {
         if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpReturn, NULL, 0))
            return false;
      }
      else if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpUnreachable, NULL, 0))
         return false;
      e->cur_label = 0;
   }
   if (!spv_inst(e, SPV_SEC_FUNCS, SpvOpFunctionEnd, NULL, 0))
      return false;

   /* splice the hoisted OpVariables after the entry label */
   if (e->func_vars.n)
   {
      uint32_t n = e->func_vars.n;
      uint32_t tail = fb->n - entry_pos;
      uint32_t *tmp = (uint32_t*)rslc_scratch_alloc(e->c, tail * sizeof(*tmp));
      if (!tmp)
         return false;
      memcpy(tmp, fb->v + entry_pos, tail * sizeof(*tmp));
      fb->n = entry_pos;
      for (i = 0; i < n; i++)
         if (!spv_w(e, fb, e->func_vars.v[i]))
            return false;
      for (i = 0; i < tail; i++)
         if (!spv_w(e, fb, tmp[i]))
            return false;
   }
   return true;
}

/* ---- reachability ---- */

/* Marks every function reachable from node's calls; glslang emits
 * only those, and so does the capability set.  Shared with the
 * printers. */
void rslc_mark_reach(struct rslc_ctx *c, uint32_t node, uint8_t *reach)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return;
      case RSLC_OP_VAR:
         return;
      case RSLC_OP_DECL:
         rslc_mark_reach(c, c->prog.vars[n->a]->init, reach);
         return;
      case RSLC_OP_MEMBER: case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT:
      case RSLC_OP_EXPR: case RSLC_OP_RETURN: case RSLC_OP_CASE:
         rslc_mark_reach(c, n->a, reach);
         return;
      case RSLC_OP_CALL:
         if (!reach[n->a])
         {
            reach[n->a] = 1;
            rslc_mark_reach(c, c->prog.funcs[n->a]->body, reach);
         }
         /* fall through */
      case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT:
      case RSLC_OP_BLOCK: case RSLC_OP_FOR: case RSLC_OP_SWITCH:
         if (n->op == RSLC_OP_SWITCH)
            rslc_mark_reach(c, n->a, reach);
         cnt = n->b ? rslc_list_len(c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            rslc_mark_reach(c, rslc_list_item(c, n->b, i), reach);
         return;
      default:
         rslc_mark_reach(c, n->a, reach);
         rslc_mark_reach(c, n->b, reach);
         rslc_mark_reach(c, n->c, reach);
         return;
   }
}

/* Fills reach[func id] for main and everything it calls, through
 * global initialisers too. */
void rslc_reach_funcs(struct rslc_ctx *c, uint8_t *reach)
{
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;
   memset(reach, 0, pr->nfuncs + 1);
   reach[pr->main_fn->id] = 1;
   rslc_mark_reach(c, pr->main_fn->body, reach);
   for (i = 1; i < pr->nvars; i++)
      if (pr->vars[i]->global && pr->vars[i]->init)
         rslc_mark_reach(c, pr->vars[i]->init, reach);
}

/* Marks used[var] for every variable node reaches, without following
 * calls: the caller walks each reachable function's body. */
static void rslc_mark_used(struct rslc_ctx *c, uint32_t node, uint8_t *used)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return;
      case RSLC_OP_VAR:
         used[n->a] = 1;
         return;
      case RSLC_OP_DECL:
         rslc_mark_used(c, c->prog.vars[n->a]->init, used);
         return;
      case RSLC_OP_MEMBER: case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT:
      case RSLC_OP_EXPR: case RSLC_OP_RETURN: case RSLC_OP_CASE:
         rslc_mark_used(c, n->a, used);
         return;
      case RSLC_OP_CALL: case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT:
      case RSLC_OP_BLOCK: case RSLC_OP_FOR: case RSLC_OP_SWITCH:
         if (n->op == RSLC_OP_SWITCH)
            rslc_mark_used(c, n->a, used);
         cnt = n->b ? rslc_list_len(c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            rslc_mark_used(c, rslc_list_item(c, n->b, i), used);
         return;
      default:
         rslc_mark_used(c, n->a, used);
         rslc_mark_used(c, n->b, used);
         rslc_mark_used(c, n->c, used);
         return;
   }
}

/* Fills used[var] for what the functions reach[] marks and the global
 * initialisers read: the variables SPIRV-Cross lists as active. */
void rslc_reach_vars(struct rslc_ctx *c, const uint8_t *reach, uint8_t *used)
{
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;
   memset(used, 0, pr->nvars + 1);
   for (i = 1; i < pr->nfuncs; i++)
      if (reach[i] && pr->funcs[i]->body)
         rslc_mark_used(c, pr->funcs[i]->body, used);
   for (i = 1; i < pr->nvars; i++)
      if (pr->vars[i]->global && pr->vars[i]->init)
         rslc_mark_used(c, pr->vars[i]->init, used);
}

/* ---- globals and interface ---- */

static bool spv_global_var(struct spv_emit *e, const struct rslc_var *v)
{
   struct spv_var *sv = &e->vars[v->id];
   uint32_t ptr, id;
   unsigned sc;

   if (!v->global)
      return true;
   switch (v->storage)
   {
      case RSLC_ST_IN:      sc = SPV_SC_INPUT; break;
      case RSLC_ST_OUT:     sc = SPV_SC_OUTPUT; break;
      case RSLC_ST_SAMPLER: sc = SPV_SC_UNIFORMCONSTANT; break;
      case RSLC_ST_GLOBAL:  sc = SPV_SC_PRIVATE; break;
      case RSLC_ST_CONST:
      {
         /* fold to a constant when possible */
         if (v->init && spv_node_is_const(e, v->init))
         {
            /* a global const is emitted whether or not it is used, as
             * glslang emits it; a local one waits for its first use */
            sv->is_const = true;
            return spv_const_var(e, v) != 0;
         }
         sc = SPV_SC_PRIVATE;
         break;
      }
      default:
         return true;   /* locals, params, block members; builtins on use */
   }
   ptr = spv_ptr_to(e, v->type, sc);
   if (!ptr)
      return false;
   id = spv_id(e);
   if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpVariable, ptr, id, sc))
      return false;
   sv->id = id;
   sv->sc = (uint8_t)sc;
   if (v->name && !spv_name(e, id, v->name->str))
      return false;
   if (v->location != RSLC_UNSET && !spv_decorate1(e, id, SPV_DEC_LOCATION, v->location))
      return false;
   if (v->storage == RSLC_ST_SAMPLER)
   {
      if (!spv_decorate1(e, id, SPV_DEC_DESCRIPTORSET, v->set == RSLC_UNSET ? 0 : v->set)
            || !spv_decorate1(e, id, SPV_DEC_BINDING, v->binding == RSLC_UNSET ? 0 : v->binding))
         return false;
   }
   if (v->storage == RSLC_ST_IN || v->storage == RSLC_ST_OUT)
   {
      if (v->interp == RSLC_INTERP_FLAT && !spv_decorate(e, id, SPV_DEC_FLAT))
         return false;
      if (v->interp == RSLC_INTERP_NOPERSPECTIVE && !spv_decorate(e, id, SPV_DEC_NOPERSPECTIVE))
         return false;
      if ((v->quals & RSLC_VQ_CENTROID) && !spv_decorate(e, id, SPV_DEC_CENTROID))
         return false;
      if ((v->quals & RSLC_VQ_SAMPLE) && !spv_decorate(e, id, SPV_DEC_SAMPLE))
         return false;
      if ((v->quals & RSLC_VQ_INVARIANT) && !spv_decorate(e, id, SPV_DEC_INVARIANT))
         return false;
      if (!spv_w(e, &e->entry_iface, id))
         return false;
   }
   if ((v->precision == RSLC_PREC_LOW || v->precision == RSLC_PREC_MEDIUM)
         && !spv_decorate(e, id, SPV_DEC_RELAXEDPRECISION))
      return false;
   return true;
}

static bool spv_builtin_var(struct spv_emit *e, const struct rslc_var *v)
{
   struct spv_var *sv = &e->vars[v->id];
   unsigned sc = v->dir == RSLC_DIR_OUT ? SPV_SC_OUTPUT : SPV_SC_INPUT;
   uint32_t ptr = spv_ptr_to(e, v->type, sc);
   uint32_t id;
   uint32_t bi = v->builtin == RSLC_BI_FRAGCOORD ? SPV_BI_FRAGCOORD
      : v->builtin == RSLC_BI_FRAGDEPTH ? SPV_BI_FRAGDEPTH
      : v->builtin == RSLC_BI_VERTEXINDEX ? SPV_BI_VERTEXINDEX
      : v->builtin == RSLC_BI_INSTANCEINDEX ? SPV_BI_INSTANCEINDEX
      : v->builtin == RSLC_BI_FRONTFACING ? SPV_BI_FRONTFACING : SPV_BI_POSITION;
   if (!ptr)
      return false;
   id = spv_id(e);
   if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpVariable, ptr, id, sc))
      return false;
   sv->id = id;
   sv->sc = (uint8_t)sc;
   if (!spv_name(e, id, v->name->str))
      return false;
   if (!spv_decorate1(e, id, SPV_DEC_BUILTIN, bi) || !spv_w(e, &e->entry_iface, id))
      return false;
   if ((v->precision == RSLC_PREC_LOW || v->precision == RSLC_PREC_MEDIUM)
         && !spv_decorate(e, id, SPV_DEC_RELAXEDPRECISION))
      return false;
   return true;
}

static bool spv_block(struct spv_emit *e, unsigned idx)
{
   const struct rslc_blockdecl *b = &e->c->prog.blocks[idx];
   bool std140 = b->std140;
   uint32_t stid, ptr, id;
   unsigned sc = b->push ? SPV_SC_PUSHCONSTANT : SPV_SC_UNIFORM;

   if (!e->c->prog.has_block[idx])
      return true;
   stid = spv_type_laid(e, b->type, std140);
   if (!stid)
      return false;
   if (!spv_decorate(e, stid, SPV_DEC_BLOCK))
      return false;
   ptr = spv_ptr(e, stid, sc);
   if (!ptr)
      return false;
   id = spv_id(e);
   if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpVariable, ptr, id, sc))
      return false;
   e->block_var[idx] = id;
   e->member_active[idx] = (uint8_t*)rslc_arena_zalloc(e->c, b->type->nfields ? b->type->nfields : 1);
   if (!e->member_active[idx])
      return false;
   if (!spv_name(e, id, b->instance ? b->instance->str : ""))
      return false;
   if (!b->push)
   {
      if (!spv_decorate1(e, id, SPV_DEC_DESCRIPTORSET, b->set == RSLC_UNSET ? 0 : b->set)
            || !spv_decorate1(e, id, SPV_DEC_BINDING, b->binding == RSLC_UNSET ? 0 : b->binding))
         return false;
   }
   if (b->instance)
   {
      struct rslc_var *iv = NULL;
      uint32_t k;
      for (k = 1; k < e->c->prog.nvars; k++)
         if (e->c->prog.vars[k]->storage == RSLC_ST_BLOCK && e->c->prog.vars[k]->block == idx)
            iv = e->c->prog.vars[k];
      if (iv)
      {
         e->vars[iv->id].id = id;
         e->vars[iv->id].sc = (uint8_t)sc;
      }
   }
   return true;
}

static bool spv_pervertex(struct spv_emit *e)
{
   uint32_t v4 = spv_type(e, rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, 4, 1));
   uint32_t f  = e->float_id;
   uint32_t one = spv_const_int(e, 1);
   uint32_t arr, st, ptr, id;
   uint32_t w[5];
   if (!v4 || !one)
      return false;
   arr = spv_id(e);
   if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpTypeArray, arr, f, one))
      return false;
   st = spv_id(e);
   w[0] = st; w[1] = v4; w[2] = f; w[3] = arr; w[4] = arr;
   if (!spv_inst(e, SPV_SEC_TYPES, SpvOpTypeStruct, w, 5))
      return false;
   if (!spv_name(e, st, "gl_PerVertex")
         || !spv_member_name(e, st, 0, "gl_Position")
         || !spv_member_name(e, st, 1, "gl_PointSize")
         || !spv_member_name(e, st, 2, "gl_ClipDistance")
         || !spv_member_name(e, st, 3, "gl_CullDistance"))
      return false;
   if (!spv_decorate(e, st, SPV_DEC_BLOCK)
         || !spv_mdecorate1(e, st, 0, SPV_DEC_BUILTIN, SPV_BI_POSITION)
         || !spv_mdecorate1(e, st, 1, SPV_DEC_BUILTIN, SPV_BI_POINTSIZE)
         || !spv_mdecorate1(e, st, 2, SPV_DEC_BUILTIN, SPV_BI_CLIPDISTANCE)
         || !spv_mdecorate1(e, st, 3, SPV_DEC_BUILTIN, SPV_BI_CULLDISTANCE))
      return false;
   ptr = spv_ptr(e, st, SPV_SC_OUTPUT);
   if (!ptr)
      return false;
   id = spv_id(e);
   if (!spv_inst3(e, SPV_SEC_TYPES, SpvOpVariable, ptr, id, SPV_SC_OUTPUT))
      return false;
   if (!spv_name(e, id, ""))
      return false;
   e->pervertex_var = id;
   return spv_w(e, &e->entry_iface, id);
}

/* ---- module ---- */

/* Reflection section size, laid out as the blob wants it. */
static uint32_t rslc_refl_strings_len(struct rslc_ctx *c)
{
   const struct rslc_prog *pr = &c->prog;
   uint32_t len = 1, i, k;
   for (k = 0; k < 2; k++)
   {
      if (!pr->has_block[k])
         continue;
      if (pr->blocks[k].type->name)
         len += pr->blocks[k].type->name->len + 1;
      if (pr->blocks[k].instance)
         len += pr->blocks[k].instance->len + 1;
      for (i = 0; i < pr->blocks[k].type->nfields; i++)
         len += pr->blocks[k].type->fields[i].name->len + 1;
   }
   for (i = 0; i < pr->nsamplers; i++)
      len += pr->samplers[i]->name->len + 1;
   for (i = 0; i < pr->ninputs; i++)
      len += pr->inputs[i]->name->len + 1;
   for (i = 0; i < pr->noutputs; i++)
      len += pr->outputs[i]->name->len + 1;
   return len;
}

static uint32_t refl_put_str(uint8_t *blob, uint32_t *at, const struct rslc_sym *s)
{
   uint32_t off = *at;
   memcpy(blob + off, s->str, s->len + 1);
   *at += s->len + 1;
   return off;
}

static void refl_fill_member(struct rslc_member *m, const struct rslc_type *t)
{
   const struct rslc_type *bt = t;
   m->array_len = 0;
   if (t->kind == RSLC_TY_ARRAY)
   {
      m->array_len = t->array_len;
      bt = t->elem;
   }
   m->base = (uint8_t)bt->base;
   m->cols = (uint8_t)(bt->kind == RSLC_TY_MAT ? bt->cols : 1);
   m->rows = (uint8_t)(bt->kind == RSLC_TY_SCALAR ? 1 : bt->rows);
   if (bt->kind == RSLC_TY_STRUCT)
   {
      m->base = RSLC_BASE_VOID;
      m->cols = m->rows = 0;
   }
}

/* Bytes the reflection section takes. */
uint32_t rslc_reflect_len(struct rslc_ctx *c)
{
   const struct rslc_prog *pr = &c->prog;
   return sizeof(struct rslc_reflect) + rslc_refl_strings_len(c)
      + (pr->has_block[0] ? pr->blocks[0].type->nfields * sizeof(struct rslc_member) : 0)
      + (pr->has_block[1] ? pr->blocks[1].type->nfields * sizeof(struct rslc_member) : 0)
      + pr->nsamplers * sizeof(struct rslc_sampler)
      + (pr->ninputs + pr->noutputs) * sizeof(struct rslc_io);
}

/* Writes the reflection section at blob + off; active[k] holds one
 * byte per member of block k (NULL: none active), used[] one per
 * variable (NULL: none read), for the samplers.  Returns bytes used. */
uint32_t rslc_reflect_write(struct rslc_ctx *c, uint8_t *blob, uint32_t off,
      uint8_t *const active[2], const uint8_t *used)
{
   const struct rslc_prog *pr = &c->prog;
   struct rslc_reflect *r = (struct rslc_reflect*)(blob + off);
   uint32_t at = off + sizeof(*r);
   uint32_t str_at;
   uint32_t i, k;

   memset(r, 0, sizeof(*r));
   /* arrays first (aligned), then strings */
   for (k = 0; k < 2; k++)
   {
      struct rslc_block *b = k ? &r->push : &r->ubo;
      if (!pr->has_block[k])
         continue;
      b->present      = 1;
      b->member_count = pr->blocks[k].type->nfields;
      b->members      = at;
      at += b->member_count * sizeof(struct rslc_member);
   }
   r->samplers = at; r->sampler_count = pr->nsamplers; at += pr->nsamplers * sizeof(struct rslc_sampler);
   r->inputs   = at; r->input_count   = pr->ninputs;   at += pr->ninputs * sizeof(struct rslc_io);
   r->outputs  = at; r->output_count  = pr->noutputs;  at += pr->noutputs * sizeof(struct rslc_io);
   str_at = at;
   blob[str_at] = 0;
   at++;   /* offset 0 = "" */

   for (k = 0; k < 2; k++)
   {
      struct rslc_block *b = k ? &r->push : &r->ubo;
      const struct rslc_blockdecl *bd = &pr->blocks[k];
      struct rslc_member *m;
      bool std140;
      if (!pr->has_block[k])
         continue;
      std140 = bd->std140;
      m = (struct rslc_member*)(blob + b->members);
      b->name     = bd->type->name ? refl_put_str(blob, &at, bd->type->name) : 0;
      b->instance = bd->instance ? refl_put_str(blob, &at, bd->instance) : 0;
      b->binding  = bd->binding;
      b->set      = bd->set;
      b->size     = 0;
      for (i = 0; i < bd->type->nfields; i++)
      {
         const struct rslc_field *f = &bd->type->fields[i];
         uint32_t fa, fs, fst;
         rslc_layout_of(f->type, std140, &fa, &fs, &fst);
         m[i].name   = refl_put_str(blob, &at, f->name);
         m[i].offset = rslc_layout_field_offset(bd->type, i, std140);
         m[i].size   = fs;
         refl_fill_member(&m[i], f->type);
         m[i].active = active[k] ? active[k][i] : 0;
         /* as SPIRV-Cross's get_declared_struct_size: no tail padding */
         b->size     = m[i].offset + fs;
      }
   }
   {
      struct rslc_sampler *s = (struct rslc_sampler*)(blob + r->samplers);
      for (i = 0; i < pr->nsamplers; i++)
      {
         s[i].name    = refl_put_str(blob, &at, pr->samplers[i]->name);
         s[i].binding = pr->samplers[i]->binding;
         s[i].set     = pr->samplers[i]->set;
         s[i].active  = used ? used[pr->samplers[i]->id] : 0;
      }
   }
   {
      struct rslc_io *io = (struct rslc_io*)(blob + r->inputs);
      for (i = 0; i < pr->ninputs; i++)
      {
         const struct rslc_var *v = pr->inputs[i];
         io[i].name     = refl_put_str(blob, &at, v->name);
         io[i].location = v->location;
         io[i].base     = (uint8_t)v->type->base;
         io[i].cols     = (uint8_t)(v->type->kind == RSLC_TY_MAT ? v->type->cols : 1);
         io[i].rows     = (uint8_t)(v->type->kind == RSLC_TY_SCALAR ? 1 : v->type->rows);
         io[i].flat     = v->interp == RSLC_INTERP_FLAT;
      }
      io = (struct rslc_io*)(blob + r->outputs);
      for (i = 0; i < pr->noutputs; i++)
      {
         const struct rslc_var *v = pr->outputs[i];
         io[i].name     = refl_put_str(blob, &at, v->name);
         io[i].location = v->location;
         io[i].base     = (uint8_t)v->type->base;
         io[i].cols     = (uint8_t)(v->type->kind == RSLC_TY_MAT ? v->type->cols : 1);
         io[i].rows     = (uint8_t)(v->type->kind == RSLC_TY_SCALAR ? 1 : v->type->rows);
         io[i].flat     = v->interp == RSLC_INTERP_FLAT;
      }
   }
   return at - off;
}

static bool rslc_emit_spirv(struct rslc_ctx *c, struct rslc_result *result)
{
   struct spv_emit *e = (struct spv_emit*)rslc_arena_zalloc(c, sizeof(*e));
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;
   uint32_t main_id;
   uint8_t *reach;
   uint8_t *used;
   struct rslc_mark mark;

   if (!e)
      return false;
   rslc_scratch_mark(c, &mark);
   e->c     = c;
   e->bound = 1;
   e->tinfo = (struct spv_tinfo*)rslc_arena_zalloc(c, (c->ntypes + 1 + 64) * sizeof(*e->tinfo));
   e->vars  = (struct spv_var*)rslc_arena_zalloc(c, (pr->nvars + 1) * sizeof(*e->vars));
   e->func_ids   = (uint32_t*)rslc_arena_zalloc(c, (pr->nfuncs + 1) * sizeof(uint32_t));
   e->func_types = (uint32_t*)rslc_arena_zalloc(c, (pr->nfuncs + 1) * sizeof(uint32_t));
   if (!e->tinfo || !e->vars || !e->func_ids || !e->func_types)
      return false;

   /* fixed ids first so the header-ish instructions have them */
   e->ext_id = spv_id(e);
   main_id   = spv_id(e);
   e->void_id  = spv_type(e, rslc_type_basic(c, RSLC_TY_VOID, RSLC_BASE_VOID, 0, 0));
   e->bool_id  = spv_type(e, rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_BOOL, 1, 1));
   e->int_id   = spv_type(e, rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_INT, 1, 1));
   e->uint_id  = spv_type(e, rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_UINT, 1, 1));
   e->float_id = spv_type(e, rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_FLOAT, 1, 1));
   if (!e->void_id || !e->bool_id || !e->int_id || !e->uint_id || !e->float_id)
      return false;

   /* function ids, for the functions main reaches */
   reach = (uint8_t*)rslc_scratch_alloc(c, pr->nfuncs + 1);
   used  = (uint8_t*)rslc_scratch_alloc(c, pr->nvars + 1);
   if (!reach || !used || !pr->main_fn)
   {
      if (!pr->main_fn)
         rslc_err(c, "no main() in stage", NULL, 0);
      return false;
   }
   rslc_reach_funcs(c, reach);
   rslc_reach_vars(c, reach, used);
   for (i = 1; i < pr->nfuncs; i++)
   {
      if (!pr->funcs[i]->body || !reach[i])
         continue;
      e->func_ids[i] = pr->funcs[i] == pr->main_fn ? main_id : spv_id(e);
      if (!spv_name(e, e->func_ids[i], pr->funcs[i]->name->str))
         return false;
   }

   if (!spv_block(e, 0) || !spv_block(e, 1))
      return false;
   for (i = 1; i < pr->nvars; i++)
      if (!spv_global_var(e, pr->vars[i]))
         return false;

   for (i = 1; i < pr->nfuncs; i++)
   {
      if (!pr->funcs[i]->body || !reach[i])
         continue;
      if (!spv_function(e, pr->funcs[i]))
         return false;
   }

   /* header-ish sections */
   if (!spv_inst1(e, SPV_SEC_CAPS, SpvOpCapability, SPV_CAP_SHADER))
      return false;
   if (e->need_imagequery && !spv_inst1(e, SPV_SEC_CAPS, SpvOpCapability, SPV_CAP_IMAGEQUERY))
      return false;
   if (!spv_inst_str(e, SPV_SEC_CAPS, SpvOpExtInstImport, &e->ext_id, 1, "GLSL.std.450", NULL, 0))
      return false;
   if (!spv_inst2(e, SPV_SEC_CAPS, SpvOpMemoryModel, 0 /* Logical */, 1 /* GLSL450 */))
      return false;
   {
      uint32_t pre[2];
      pre[0] = c->stage == RSLC_STAGE_VERTEX ? SPV_MODEL_VERTEX : SPV_MODEL_FRAGMENT;
      pre[1] = main_id;
      if (!spv_inst_str(e, SPV_SEC_ENTRY, SpvOpEntryPoint, pre, 2, "main",
               e->entry_iface.v, e->entry_iface.n))
         return false;
   }
   if (c->stage == RSLC_STAGE_FRAGMENT
         && !spv_inst2(e, SPV_SEC_ENTRY, SpvOpExecutionMode, main_id, SPV_MODE_ORIGINUPPERLEFT))
      return false;
   {
      uint32_t src[2];
      src[0] = 2;   /* GLSL */
      src[1] = c->version;
      /* OpSource precedes the names: it closes the entry section */
      if (!spv_inst(e, SPV_SEC_ENTRY, SpvOpSource, src, 2))
         return false;
   }

   /* assemble the blob: header, reflection, SPIR-V words */
   {
      uint32_t words = 5;
      uint32_t s;
      struct rslc_pbuf pb;
      uint32_t *out;
      for (s = 0; s < SPV_SEC_COUNT; s++)
         words += e->sec[s].n;
      if (!rslc_blob_begin(c, &pb, words * 4, e->member_active, used))
         return false;
      out = (uint32_t*)(pb.data + pb.len);
      out[0] = SPV_MAGIC;
      out[1] = SPV_VERSION_1_0;
      out[2] = 0;             /* generator */
      out[3] = e->bound;
      out[4] = 0;
      pb.len += 5 * 4;
      for (s = 0; s < SPV_SEC_COUNT; s++)
         if (e->sec[s].n && !rslc_pbuf_put(c, &pb, e->sec[s].v, e->sec[s].n * 4))
            return false;
      rslc_blob_end(c, &pb, RSLC_DST_SPIRV, result);
      result->spirv     = (const uint32_t*)(pb.data + ((struct rslc_blob_hdr*)pb.data)->payload_off);
      result->spirv_len = words;
   }
   rslc_scratch_release(c, &mark);
   return true;
}

/* Starts a result blob: header space, the reflection section, and a
 * 4-aligned payload area of payload_hint bytes.  pb.len is left at the
 * payload offset. */
bool rslc_blob_begin(struct rslc_ctx *c, struct rslc_pbuf *pb, size_t payload_hint,
      uint8_t *const active[2], const uint8_t *used)
{
   uint32_t refl_len    = rslc_reflect_len(c);
   uint32_t refl_off    = sizeof(struct rslc_blob_hdr);
   uint32_t payload_off = (refl_off + refl_len + 3) & ~3u;
   struct rslc_blob_hdr *h;
   if (!rslc_pbuf_init(c, pb, payload_off, payload_hint))
      return false;
   memset(pb->data, 0, payload_off);
   rslc_reflect_write(c, pb->data, refl_off, active, used);
   h = (struct rslc_blob_hdr*)pb->data;
   h->refl_off    = refl_off;
   h->refl_len    = refl_len;
   h->payload_off = payload_off;
   pb->len        = payload_off;
   return true;
}

/* Finishes the blob and hands it to result. */
void rslc_blob_end(struct rslc_ctx *c, struct rslc_pbuf *pb, enum rslc_dst dst,
      struct rslc_result *result)
{
   struct rslc_blob_hdr *h = (struct rslc_blob_hdr*)pb->data;
   h->magic       = RSLC_BLOB_MAGIC;
   h->version     = RSLC_BLOB_VERSION;
   h->total_len   = (uint32_t)pb->len;
   h->payload_len = (uint32_t)(pb->len - h->payload_off);
   h->stage       = (uint8_t)c->stage;
   h->dst         = (uint8_t)dst;
   memset(result, 0, sizeof(*result));
   result->blob     = pb->data;
   result->blob_len = pb->len;
   result->refl     = (const struct rslc_reflect*)(pb->data + h->refl_off);
}

bool rslc_emit_dst(struct rslc_ctx *c, struct rslc_result *result)
{
   switch (c->opts->dst)
   {
      case RSLC_DST_SPIRV:
      case RSLC_DST_REFLECT:
         if (rslc_emit_spirv(c, result))
            return true;
         if (!c->failed)
            rslc_err(c, "SPIR-V emission failed", NULL, 0);
         return false;
      default:
         break;
   }
#ifdef RSLC_NO_TEXT
   (void)result;
   rslc_err(c, "built without the text printers", NULL, 0);
   return false;
#else
   return rslc_emit_text(c, result);
#endif
}

/* ------------------------------------------------------------------ */
/* SPIR-V ingest                                                       */
/* ------------------------------------------------------------------ */

/* A module this compiler or glslang emitted for the slang stack is
 * read for its interface: the two blocks with their members, the
 * samplers, the stage inputs and outputs, and which block members the
 * code the entry point reaches touches.  That rebuilds the declaration
 * half of the program, which the reflection writer then lays out as
 * it would from GLSL; the layout the module decorates is held against
 * the computed one and a module that differs is refused.  Function
 * bodies are not decompiled: a cached module serves reflection and
 * the Vulkan driver, and a textual driver recompiles its text from
 * the GLSL. */

#define RSLC_SPV_MAGIC 0x07230203u

enum ing_kind
{
   ING_NONE = 0,
   ING_TYPE,      /* type: b set while its rslc type is being built */
   ING_CONST,     /* a: value */
   ING_VAR,       /* a: pointee type id, b: storage class */
   ING_FUNC       /* a: function index */
};

struct ing_id
{
   uint32_t       a;
   uint32_t       b;
   uint32_t       name;      /* word offset of the OpName string, 0 none */
   uint32_t       inst;      /* word offset of the defining instruction */
   uint32_t       location;  /* RSLC_UNSET when absent */
   uint32_t       binding;
   uint32_t       set;
   uint8_t        kind;      /* enum ing_kind */
   uint8_t        block;     /* Block decoration on a struct type */
   uint8_t        flat;
   uint8_t        builtin;   /* BuiltIn on the id or a member of it */
   uint8_t        merge;     /* a merge block or continue target */
   uint8_t        sampler;   /* a sampler variable: its index + 1 */
   const struct rslc_type *type;
};

struct ing
{
   struct rslc_ctx *c;
   const uint32_t  *w;
   uint32_t         n;
   uint32_t         bound;
   struct ing_id   *ids;
   uint32_t         entry;      /* entry point function id */
   uint32_t         model;
   uint32_t         nfuncs;
   uint32_t        *func_at;    /* function index -> word offset of OpFunction */
   uint8_t         *func_reach;
   uint8_t         *active[2];
   uint8_t         *used;           /* by sampler index + 1, as rslc_var.id */
   uint32_t         block_var[2];   /* variable ids of the blocks, 0 none */
};

static bool ing_fail(struct ing *g, const char *msg)
{
   rslc_err(g->c, msg, NULL, 0);
   return false;
}

/* Appends to one of the program's pointer tables. */
static bool ing_push(struct rslc_ctx *c, void ***arr, uint32_t *n, uint32_t *cap, void *item)
{
   if (*n == *cap)
   {
      uint32_t ncap = *cap ? *cap * 2 : 8;
      void **nv = (void**)rslc_arena_alloc(c, ncap * sizeof(*nv));
      if (!nv)
         return false;
      if (*n)
         memcpy(nv, *arr, *n * sizeof(*nv));
      *arr = nv;
      *cap = ncap;
   }
   (*arr)[(*n)++] = item;
   return true;
}

/* Byte i of the literal string at w: SPIR-V packs a string's bytes
 * into its words lowest byte first, whatever the host's byte order. */
#define ING_STR_BYTE(w, i) ((char)(((w)[(i) >> 2] >> (8 * ((i) & 3))) & 0xffu))

/* Whether the literal string at w (words of the instruction left from
 * it) reads s. */
static bool ing_str_is(const uint32_t *w, uint32_t words, const char *s)
{
   uint32_t i;
   for (i = 0; i < words * 4; i++)
   {
      if (ING_STR_BYTE(w, i) != s[i])
         return false;
      if (!s[i])
         return true;
   }
   return false;
}

static struct rslc_sym *ing_name(struct ing *g, uint32_t at)
{
   const uint32_t *w;
   char            buf[256];
   char           *s = buf;
   size_t          len = 0, i;
   if (!at)
      return NULL;
   w = g->w + at;
   /* the instruction was checked to end in a NUL */
   while (ING_STR_BYTE(w, len))
      len++;
   if (!len)
      return NULL;
   if (len > sizeof(buf) && !(s = (char*)rslc_arena_alloc(g->c, len)))
      return NULL;
   for (i = 0; i < len; i++)
      s[i] = ING_STR_BYTE(w, i);
   return rslc_sym(g->c, s, len);
}

/* The rslc type of a type id, built once. */
static const struct rslc_type *ing_type(struct ing *g, uint32_t id)
{
   struct ing_id  *d;
   const uint32_t *w;
   uint32_t        op, len;
   if (id >= g->bound || g->ids[id].kind != ING_TYPE)
      return NULL;
   d = &g->ids[id];
   if (d->type)
      return d->type;
   if (d->b)
      return NULL;   /* a type that contains itself */
   d->b = 1;
   w   = g->w + d->inst;
   op  = w[0] & 0xffffu;
   len = w[0] >> 16;
   switch (op)
   {
      case SpvOpTypeVoid:
         d->type = rslc_type_basic(g->c, RSLC_TY_VOID, RSLC_BASE_VOID, 0, 0);
         break;
      case SpvOpTypeBool:
         d->type = rslc_type_basic(g->c, RSLC_TY_SCALAR, RSLC_BASE_BOOL, 1, 1);
         break;
      case SpvOpTypeInt:
         if (w[2] != 32)
            return NULL;
         d->type = rslc_type_basic(g->c, RSLC_TY_SCALAR, w[3] ? RSLC_BASE_INT : RSLC_BASE_UINT, 1, 1);
         break;
      case SpvOpTypeFloat:
         if (w[2] != 32)
            return NULL;
         d->type = rslc_type_basic(g->c, RSLC_TY_SCALAR, RSLC_BASE_FLOAT, 1, 1);
         break;
      case SpvOpTypeVector:
      {
         const struct rslc_type *e = ing_type(g, w[2]);
         if (!e || e->kind != RSLC_TY_SCALAR || w[3] < 2 || w[3] > 4)
            return NULL;
         d->type = rslc_type_basic(g->c, RSLC_TY_VEC, (enum rslc_base)e->base, w[3], 1);
         break;
      }
      case SpvOpTypeMatrix:
      {
         const struct rslc_type *e = ing_type(g, w[2]);
         if (!e || e->kind != RSLC_TY_VEC || e->base != RSLC_BASE_FLOAT || w[3] < 2 || w[3] > 4)
            return NULL;
         d->type = rslc_type_basic(g->c, RSLC_TY_MAT, RSLC_BASE_FLOAT, e->rows, w[3]);
         break;
      }
      case SpvOpTypeImage:
      {
         const struct rslc_type *e = ing_type(g, w[2]);
         if (!e || e->kind != RSLC_TY_SCALAR || w[3] != 1 /* Dim2D */ || w[7] != 1 /* sampled */)
            return NULL;
         d->type = rslc_type_sampler2d(g->c, (enum rslc_base)e->base);
         break;
      }
      case SpvOpTypeSampledImage:
         d->type = ing_type(g, w[2]);
         break;
      case SpvOpTypeArray:
      {
         const struct rslc_type *e = ing_type(g, w[2]);
         uint32_t lid = w[3];
         if (!e || lid >= g->bound || g->ids[lid].kind != ING_CONST || !g->ids[lid].a)
            return NULL;
         d->type = rslc_type_array(g->c, e, g->ids[lid].a);
         break;
      }
      case SpvOpTypeStruct:
      {
         uint32_t nf = len - 2, i;
         struct rslc_field *f = (struct rslc_field*)rslc_arena_zalloc(g->c, (nf ? nf : 1) * sizeof(*f));
         struct rslc_type  *t;
         if (!f)
            return NULL;
         for (i = 0; i < nf; i++)
         {
            f[i].type = ing_type(g, w[2 + i]);
            if (!f[i].type)
               return NULL;
            f[i].name = NULL;   /* member names come after, from OpMemberName */
         }
         t = rslc_type_struct(g->c, ing_name(g, d->name), f, nf);
         if (t && !ing_push(g->c, (void***)&g->c->prog.structs, &g->c->prog.nstructs,
                  &g->c->prog.structs_cap, t))
            return NULL;
         d->type = t;
         break;
      }
      case SpvOpTypePointer:
         d->type = ing_type(g, w[3]);
         break;
      default:
         return NULL;
   }
   return d->type;
}

/* The member names of a struct, from the OpMemberNames seen on the
 * tape; a struct id used by a block must carry them all. */
static bool ing_member_names(struct ing *g, uint32_t sid, const struct rslc_type *t)
{
   struct rslc_field *f = (struct rslc_field*)t->fields;
   uint32_t at = 5, i;
   while (at < g->n)
   {
      uint32_t op  = g->w[at] & 0xffffu;
      uint32_t len = g->w[at] >> 16;
      if (!len)
         return false;
      if (op == SpvOpMemberName && len >= 4 && g->w[at + 1] == sid && g->w[at + 2] < t->nfields)
         f[g->w[at + 2]].name = ing_name(g, at + 3);
      if (op == SpvOpFunction)
         break;
      at += len;
   }
   for (i = 0; i < t->nfields; i++)
      if (!f[i].name)
         return false;
   return true;
}

static struct rslc_var *ing_var(struct ing *g, uint32_t id, const struct rslc_type *t)
{
   struct rslc_prog *pr = &g->c->prog;
   struct rslc_var  *v  = (struct rslc_var*)rslc_arena_zalloc(g->c, sizeof(*v));
   if (!v)
      return NULL;
   v->name     = ing_name(g, g->ids[id].name);
   v->type     = t;
   v->location = g->ids[id].location;
   v->binding  = g->ids[id].binding;
   v->set      = g->ids[id].set;
   v->global   = 1;
   v->interp   = g->ids[id].flat ? RSLC_INTERP_FLAT : RSLC_INTERP_NONE;
   (void)pr;
   return v;
}

/* A block's decorated member offsets against the layout the reflection
 * writer will compute; the push block may be std430 or std140. */
static bool ing_block_layout(struct ing *g, uint32_t sid, const struct rslc_type *t, bool std140)
{
   uint32_t at = 5;
   uint32_t seen = 0;
   while (at < g->n)
   {
      uint32_t op  = g->w[at] & 0xffffu;
      uint32_t len = g->w[at] >> 16;
      if (!len)
         return false;
      if (op == SpvOpMemberDecorate && len >= 5 && g->w[at + 1] == sid && g->w[at + 3] == SPV_DEC_OFFSET
            && g->w[at + 2] < t->nfields)
      {
         if (g->w[at + 4] != rslc_layout_field_offset(t, g->w[at + 2], std140))
            return false;
         seen++;
      }
      if (op == SpvOpFunction)
         break;
      at += len;
   }
   return seen == t->nfields;
}

/* Which functions the entry point reaches, by their OpFunctionCalls;
 * stack holds nfuncs entries. */
static void ing_reach(struct ing *g, uint32_t entry, uint32_t *stack)
{
   uint32_t top = 0;
   stack[top++]         = entry;
   g->func_reach[entry] = 1;
   while (top)
   {
      uint32_t at = g->func_at[stack[--top]];
      while (at < g->n)
      {
         uint32_t op  = g->w[at] & 0xffffu;
         uint32_t len = g->w[at] >> 16;
         if (!len || op == SpvOpFunctionEnd)
            break;
         if (op == SpvOpFunctionCall && g->w[at + 3] < g->bound && g->ids[g->w[at + 3]].kind == ING_FUNC)
         {
            uint32_t fi = g->ids[g->w[at + 3]].a;
            if (!g->func_reach[fi])
            {
               g->func_reach[fi] = 1;
               stack[top++]      = fi;
            }
         }
         at += len;
      }
   }
}

/* The block members the reachable code touches: the first index of an
 * access chain on a block variable, or the whole block loaded; and the
 * samplers it loads or hands to a call. */
static bool ing_active(struct ing *g)
{
   uint32_t at = 5, fi = 0, k;
   bool in_reach = false;
   while (at < g->n)
   {
      uint32_t op  = g->w[at] & 0xffffu;
      uint32_t len = g->w[at] >> 16;
      if (!len)
         return false;
      if (op == SpvOpFunction)
      {
         in_reach = g->func_reach[fi];
         fi++;
      }
      else if (in_reach && (op == SpvOpAccessChain || op == SpvOpInBoundsAccessChain) && len >= 5)
      {
         uint32_t base = g->w[at + 3], idx = g->w[at + 4];
         for (k = 0; k < 2; k++)
            if (base == g->block_var[k] && g->active[k])
            {
               if (idx >= g->bound || g->ids[idx].kind != ING_CONST
                     || g->ids[idx].a >= g->c->prog.blocks[k].type->nfields)
                  return false;
               g->active[k][g->ids[idx].a] = 1;
            }
      }
      else if (in_reach && (op == SpvOpLoad || (op == SpvOpCopyMemory && len >= 3)))
      {
         uint32_t base = g->w[at + (op == SpvOpLoad ? 3 : 2)];
         for (k = 0; k < 2; k++)
            if (base == g->block_var[k] && g->active[k])
               memset(g->active[k], 1, g->c->prog.blocks[k].type->nfields);
         if (base < g->bound && g->ids[base].sampler)
            g->used[g->ids[base].sampler] = 1;
      }
      else if (in_reach && op == SpvOpFunctionCall)
      {
         for (k = 4; k < len; k++)
         {
            uint32_t arg = g->w[at + k];
            if (arg < g->bound && g->ids[arg].sampler)
               g->used[g->ids[arg].sampler] = 1;
         }
      }
      at += len;
   }
   return true;
}

/* First pass: every id's kind, name and decorations; the entry point;
 * the structured-control-flow check (a conditional branch or switch
 * follows its merge instruction); the capabilities. */
static bool ing_scan(struct ing *g)
{
   uint32_t at = 5;
   uint32_t fi = 0;
   bool merged = false;
   while (at < g->n)
   {
      uint32_t op   = g->w[at] & 0xffffu;
      uint32_t len  = g->w[at] >> 16;
      uint32_t need;
      const uint32_t *w = g->w + at;
      if (!len || at + len > g->n)
         return ing_fail(g, "SPIR-V instruction runs past the module");
      switch (op)
      {
         /* operand counts the reader relies on; a string operand ends
          * inside its instruction */
         case SpvOpCapability: case SpvOpName: case SpvOpTypeVoid: case SpvOpTypeBool:
         case SpvOpTypeStruct: case SpvOpSelectionMerge: case SpvOpSwitch:
            need = 2; break;
         case SpvOpEntryPoint: case SpvOpMemberName: case SpvOpDecorate: case SpvOpExtInstImport:
         case SpvOpTypeInt: case SpvOpTypeFloat: case SpvOpTypeSampledImage:
         case SpvOpLoopMerge: case SpvOpBranchConditional:
            need = 3; break;
         case SpvOpConstant: case SpvOpVariable: case SpvOpTypeVector: case SpvOpTypeMatrix:
         case SpvOpTypeArray: case SpvOpTypePointer: case SpvOpMemberDecorate: case SpvOpLoad:
         case SpvOpFunctionCall:
            need = 4; break;
         case SpvOpFunction:
            need = 5; break;
         case SpvOpTypeImage:
            need = 9; break;
         default:
            need = 1; break;
      }
      if (len < need)
         return ing_fail(g, "SPIR-V instruction is short");
      if ((op == SpvOpName || op == SpvOpMemberName || op == SpvOpExtInstImport)
            && (w[len - 1] >> 24))
         return ing_fail(g, "SPIR-V string runs past its instruction");
      switch (op)
      {
         case SpvOpCapability:
            if (w[1] != SPV_CAP_SHADER && w[1] != SPV_CAP_IMAGEQUERY)
               return ing_fail(g, "SPIR-V capability beyond Shader");
            break;
         case SpvOpExtInstImport:
            if (!ing_str_is(w + 2, len - 2, "GLSL.std.450"))
               return ing_fail(g, "SPIR-V extended instruction set beyond GLSL.std.450");
            break;
         case SpvOpEntryPoint:
            if (g->entry)
               return ing_fail(g, "SPIR-V module with several entry points");
            g->model = w[1];
            g->entry = w[2];
            break;
         case SpvOpName:
            if (w[1] < g->bound)
               g->ids[w[1]].name = at + 2;
            break;
         case SpvOpDecorate:
            if (w[1] < g->bound)
            {
               struct ing_id *d = &g->ids[w[1]];
               switch (w[2])
               {
                  case SPV_DEC_LOCATION:
                  case SPV_DEC_BINDING:
                  case SPV_DEC_DESCRIPTORSET:
                     if (len < 4)
                        return ing_fail(g, "SPIR-V instruction is short");
                     if (w[2] == SPV_DEC_LOCATION)
                        d->location = w[3];
                     else if (w[2] == SPV_DEC_BINDING)
                        d->binding = w[3];
                     else
                        d->set = w[3];
                     break;
                  case SPV_DEC_BLOCK:         d->block    = 1;    break;
                  case SPV_DEC_FLAT:          d->flat     = 1;    break;
                  case SPV_DEC_BUILTIN:       d->builtin  = 1;    break;
                  default: break;
               }
            }
            break;
         case SpvOpMemberDecorate:
            if (w[1] < g->bound && w[3] == SPV_DEC_BUILTIN)
               g->ids[w[1]].builtin = 1;
            break;
         case SpvOpTypeVoid: case SpvOpTypeBool: case SpvOpTypeInt: case SpvOpTypeFloat:
         case SpvOpTypeVector: case SpvOpTypeMatrix: case SpvOpTypeImage:
         case SpvOpTypeSampledImage: case SpvOpTypeArray: case SpvOpTypeStruct:
         case SpvOpTypePointer: case SpvOpTypeFunction:
            if (w[1] >= g->bound)
               return ing_fail(g, "SPIR-V id beyond the bound");
            g->ids[w[1]].kind = ING_TYPE;
            g->ids[w[1]].inst = at;
            break;
         case SpvOpConstant:
            if (w[2] >= g->bound)
               return ing_fail(g, "SPIR-V id beyond the bound");
            g->ids[w[2]].kind = ING_CONST;
            g->ids[w[2]].a    = w[3];
            break;
         case SpvOpVariable:
            if (w[2] >= g->bound)
               return ing_fail(g, "SPIR-V id beyond the bound");
            g->ids[w[2]].kind = ING_VAR;
            g->ids[w[2]].a    = w[1];
            g->ids[w[2]].b    = w[3];
            break;
         case SpvOpFunction:
            if (w[2] >= g->bound)
               return ing_fail(g, "SPIR-V id beyond the bound");
            g->ids[w[2]].kind = ING_FUNC;
            g->ids[w[2]].a    = fi;
            g->func_at[fi]    = at;
            fi++;
            break;
         case SpvOpLabel:
            merged = false;
            break;
         case SpvOpSelectionMerge:
         case SpvOpLoopMerge:
            merged = true;
            if (w[1] < g->bound)
               g->ids[w[1]].merge = 1;
            if (op == SpvOpLoopMerge && w[2] < g->bound)
               g->ids[w[2]].merge = 1;
            break;
         case SpvOpBranchConditional:
         case SpvOpSwitch:
         {
            /* a header's branch follows its merge instruction; any
             * other conditional branch leaves for a merge block or a
             * continue target of an enclosing construct */
            bool structured = merged;
            uint32_t i;
            if (op == SpvOpBranchConditional)
               structured = structured || (w[2] < g->bound && g->ids[w[2]].merge)
                     || (w[3] < g->bound && g->ids[w[3]].merge);
            else
            {
               structured = structured || (w[2] < g->bound && g->ids[w[2]].merge);
               for (i = 4; i < len; i += 2)
                  structured = structured || (w[i] < g->bound && g->ids[w[i]].merge);
            }
            if (!structured)
               return ing_fail(g, "SPIR-V control flow is not structured");
            break;
         }
         default:
            if (op == 29 /* OpTypeRuntimeArray */)
               return ing_fail(g, "SPIR-V runtime array");
            break;
      }
      at += len;
   }
   g->nfuncs = fi;
   return true;
}

bool rslc_compile_spirv(const uint32_t *words, size_t n,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result)
{
   struct rslc_ctx   c;
   struct ing        g;
   struct rslc_prog *pr;
   struct rslc_pbuf  pb;
   uint32_t          id, k;
   bool              ok = false;

   if (!words || !opts || !result)
      return false;
   if (!rslc_ctx_init(&c, opts, stage))
      return false;
   pr = &c.prog;

   memset(&g, 0, sizeof(g));
   g.c = &c;
   g.w = words;
   g.n = (uint32_t)n;
   if (n < 5 || words[0] != RSLC_SPV_MAGIC)
      rslc_err(&c, "not a SPIR-V module", NULL, 0);
   else if ((words[1] >> 8) != 0x100)
      rslc_err(&c, "SPIR-V version not 1.x", NULL, 0);
   else if (words[3] == 0 || words[3] > 0x10000)
      rslc_err(&c, "SPIR-V id bound out of range", NULL, 0);
   else if (opts->dst != RSLC_DST_SPIRV && opts->dst != RSLC_DST_REFLECT)
      rslc_err(&c, "a printed destination needs the GLSL, not a module", NULL, 0);
   else
   {
      uint32_t at, nf = 1;
      for (at = 5; at < n && (words[at] >> 16); at += words[at] >> 16)
         nf += (words[at] & 0xffffu) == SpvOpFunction;
      g.bound      = words[3];
      g.ids        = (struct ing_id*)rslc_arena_zalloc(&c, (size_t)g.bound * sizeof(*g.ids));
      g.func_at    = (uint32_t*)rslc_arena_zalloc(&c, nf * sizeof(uint32_t));
      g.func_reach = (uint8_t*)rslc_arena_zalloc(&c, nf);
      if (g.ids && g.func_at && g.func_reach)
      {
         for (id = 0; id < g.bound; id++)
            g.ids[id].location = g.ids[id].binding = g.ids[id].set = RSLC_UNSET;
         ok = ing_scan(&g);
      }
   }
   if (ok && (!g.entry || g.entry >= g.bound || g.ids[g.entry].kind != ING_FUNC))
      ok = ing_fail(&g, "SPIR-V module without an entry point");
   if (ok && g.model != (stage == RSLC_STAGE_VERTEX ? SPV_MODEL_VERTEX : SPV_MODEL_FRAGMENT))
      ok = ing_fail(&g, "SPIR-V entry point is not this stage");

   /* the interface, in id order as glslang and rslc emit it */
   for (id = 1; ok && id < g.bound; id++)
   {
      struct ing_id *d = &g.ids[id];
      const struct rslc_type *t;
      struct rslc_var *v;
      uint32_t ptr;
      if (d->kind != ING_VAR)
         continue;
      if (d->b != SPV_SC_UNIFORM && d->b != SPV_SC_PUSHCONSTANT && d->b != SPV_SC_UNIFORMCONSTANT
            && d->b != SPV_SC_INPUT && d->b != SPV_SC_OUTPUT)
         continue;   /* Private and Function variables are the body's */
      ptr = d->a;
      if (ptr >= g.bound || g.ids[ptr].kind != ING_TYPE
            || (g.w[g.ids[ptr].inst] & 0xffffu) != SpvOpTypePointer
            || g.w[g.ids[ptr].inst + 3] >= g.bound)
      {
         ok = ing_fail(&g, "SPIR-V variable of no pointer type");
         break;
      }
      t = ing_type(&g, ptr);
      if (!t)
      {
         ok = ing_fail(&g, "SPIR-V variable of a type outside the dialect");
         break;
      }
      switch (d->b)
      {
         case SPV_SC_UNIFORM:
         case SPV_SC_PUSHCONSTANT:
         {
            uint32_t sid = g.w[g.ids[ptr].inst + 3];
            bool push = d->b == SPV_SC_PUSHCONSTANT;
            k = push ? 1 : 0;
            if (t->kind != RSLC_TY_STRUCT || !g.ids[sid].block || pr->has_block[k])
            {
               ok = ing_fail(&g, "SPIR-V uniform that is not the stage's one block");
               break;
            }
            if (!ing_member_names(&g, sid, t))
            {
               ok = ing_fail(&g, "SPIR-V block without member names");
               break;
            }
            pr->has_block[k]        = true;
            pr->blocks[k].type      = t;
            pr->blocks[k].instance  = ing_name(&g, d->name);
            pr->blocks[k].binding   = d->binding;
            pr->blocks[k].set       = d->set;
            pr->blocks[k].push      = push;
            pr->blocks[k].std140    = !push;
            if (push && !ing_block_layout(&g, sid, t, false))
               pr->blocks[k].std140 = true;   /* layout(push_constant, std140) */
            if (!ing_block_layout(&g, sid, t, pr->blocks[k].std140))
            {
               ok = ing_fail(&g, "SPIR-V block layout is not the one this compiler lays out");
               break;
            }
            g.block_var[k] = id;
            g.active[k]    = (uint8_t*)rslc_arena_zalloc(&c, t->nfields ? t->nfields : 1);
            if (!g.active[k])
               ok = false;
            break;
         }
         case SPV_SC_UNIFORMCONSTANT:
            if (t->kind != RSLC_TY_SAMPLER2D)
            {
               ok = ing_fail(&g, "SPIR-V uniform that is not a sampler2D");
               break;
            }
            v = ing_var(&g, id, t);
            if (!v || !v->name)
            {
               ok = ing_fail(&g, "SPIR-V sampler without a name");
               break;
            }
            v->storage = RSLC_ST_SAMPLER;
            if (pr->nsamplers >= 255)
            {
               ok = ing_fail(&g, "SPIR-V module with too many samplers");
               break;
            }
            /* the reflection writer reads used[] by id */
            v->id = pr->nsamplers + 1;
            g.ids[id].sampler = (uint8_t)v->id;
            ok = ing_push(&c, (void***)&pr->samplers, &pr->nsamplers, &pr->samplers_cap, v);
            break;
         case SPV_SC_INPUT:
         case SPV_SC_OUTPUT:
         {
            struct rslc_var ***arr = d->b == SPV_SC_INPUT ? &pr->inputs : &pr->outputs;
            uint32_t *cnt = d->b == SPV_SC_INPUT ? &pr->ninputs : &pr->noutputs;
            uint32_t *cap = d->b == SPV_SC_INPUT ? &pr->inputs_cap : &pr->outputs_cap;
            uint32_t sid  = g.w[g.ids[ptr].inst + 3];
            if (d->builtin || (sid < g.bound && g.ids[sid].builtin))
               break;   /* gl_Position, gl_FragCoord and the rest */
            if (d->location == RSLC_UNSET)
            {
               ok = ing_fail(&g, "SPIR-V stage variable without a location");
               break;
            }
            v = ing_var(&g, id, t);
            if (!v || !v->name)
            {
               ok = ing_fail(&g, "SPIR-V stage variable without a name");
               break;
            }
            v->storage = d->b == SPV_SC_INPUT ? RSLC_ST_IN : RSLC_ST_OUT;
            ok = ing_push(&c, (void***)arr, cnt, cap, v);
            break;
         }
         default:
            break;
      }
   }

   if (ok)
   {
      uint32_t *stack = (uint32_t*)rslc_arena_alloc(&c, g.nfuncs * sizeof(uint32_t));
      g.used = (uint8_t*)rslc_arena_zalloc(&c, pr->nsamplers + 1);
      if (!stack || !g.used)
         ok = false;
      else
         ing_reach(&g, g.ids[g.entry].a, stack);
      if (ok && !ing_active(&g))
         ok = ing_fail(&g, "SPIR-V block access this reader cannot follow");
   }
   if (ok)
   {
      size_t payload = opts->dst == RSLC_DST_SPIRV ? n * 4 : 0;
      if (!rslc_blob_begin(&c, &pb, payload, g.active, g.used))
         ok = false;
      else
      {
         if (payload && !rslc_pbuf_put(&c, &pb, words, payload))
            ok = false;
         else
         {
            rslc_blob_end(&c, &pb, opts->dst, result);
            if (payload)
            {
               result->spirv     = (const uint32_t*)((uint8_t*)pb.data
                     + ((struct rslc_blob_hdr*)pb.data)->payload_off);
               result->spirv_len = n;
            }
         }
         if (!ok)
            rslc_pbuf_free(&c, &pb);
      }
      if (!ok && !c.failed)
         rslc_err(&c, "out of memory", NULL, 0);
   }

   rslc_ctx_close(&c);
   return ok;
}
