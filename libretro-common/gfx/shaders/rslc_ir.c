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

bool rslc_ctx_init(struct rslc_ctx *c, const struct rslc_opts *opts,
      enum rslc_stage stage)
{
   memset(c, 0, sizeof(*c));
   c->opts  = opts;
   c->stage = stage;

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

bool rslc_emit_dst(struct rslc_ctx *c, struct rslc_result *result)
{
   switch (c->opts->dst)
   {
      case RSLC_DST_SPIRV:
      case RSLC_DST_REFLECT:
         rslc_err(c, "SPIR-V emitter not implemented", NULL, 0);
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

/* SPIR-V ingest */

#define RSLC_SPV_MAGIC 0x07230203u

bool rslc_compile_spirv(const uint32_t *words, size_t n,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result)
{
   struct rslc_ctx c;

   if (!words || !opts || !result)
      return false;
   if (!rslc_ctx_init(&c, opts, stage))
      return false;

   if (n < 5 || words[0] != RSLC_SPV_MAGIC)
      rslc_err(&c, "not a SPIR-V module", NULL, 0);
   else if ((words[1] >> 8) != 0x100)
      rslc_err(&c, "SPIR-V version not 1.x", NULL, 0);
   else if (words[3] == 0 || words[3] > 0x10000)
      rslc_err(&c, "SPIR-V id bound out of range", NULL, 0);
   else
      rslc_err(&c, "SPIR-V ingest not implemented", NULL, 0);

   rslc_ctx_close(&c);
   return false;
}
