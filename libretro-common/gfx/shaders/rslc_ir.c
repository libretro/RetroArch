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
      char     num[12];
      unsigned n = sizeof(num) - 1;

      num[n] = '\0';
      do
      {
         num[--n] = (char)('0' + (line % 10u));
         line    /= 10u;
      } while (line && n);
      at = rslc_err_copy(dst, cap, at, " (line ");
      at = rslc_err_copy(dst, cap, at, num + n);
      at = rslc_err_copy(dst, cap, at, ")");
   }
   dst[at] = '\0';
}

/* Arena */

static bool rslc_arena_grow(struct rslc_ctx *c, size_t need)
{
   struct rslc_arena *a = &c->arena;
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
   if (a->held + len > a->cap)
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

void *rslc_arena_alloc(struct rslc_ctx *c, size_t len)
{
   struct rslc_arena *a = &c->arena;
   void              *p;

   if (c->failed)
      return NULL;
   len = (len + (RSLC_ARENA_ALIGN - 1)) & ~(size_t)(RSLC_ARENA_ALIGN - 1);
   if ((size_t)(a->end - a->cur) < len)
      if (!rslc_arena_grow(c, len))
         return NULL;
   p        = a->cur;
   a->cur  += len;
   a->used += len;
   return p;
}

void *rslc_arena_zalloc(struct rslc_ctx *c, size_t len)
{
   void *p = rslc_arena_alloc(c, len);
   if (p)
      memset(p, 0, len);
   return p;
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
   struct rslc_intern       *t    = &c->intern;
   uint32_t                  n    = t->mask ? (t->mask + 1) * 2 : RSLC_INTERN_INITIAL;
   struct rslc_intern_entry *old  = t->slots;
   uint32_t                  oldn = t->mask ? t->mask + 1 : 0;
   struct rslc_intern_entry *nw;
   uint32_t                  i;

   nw = (struct rslc_intern_entry*)rslc_arena_zalloc(c, n * sizeof(*nw));
   if (!nw)
      return false;
   for (i = 0; i < oldn; i++)
   {
      uint32_t j;
      if (!old[i].str)
         continue;
      j = old[i].hash & (n - 1);
      while (nw[j].str)
         j = (j + 1) & (n - 1);
      nw[j] = old[i];
   }
   t->slots = nw;
   t->mask  = n - 1;
   return true;
}

const char *rslc_intern(struct rslc_ctx *c, const char *s, size_t len)
{
   struct rslc_intern *t = &c->intern;
   uint32_t            h = rslc_hash(s, len);
   uint32_t            j;
   char               *copy;

   if (c->failed)
      return NULL;
   if (!t->slots || (t->count + 1) * 4 > (t->mask + 1) * 3)
      if (!rslc_intern_rehash(c))
         return NULL;

   j = h & t->mask;
   for (;;)
   {
      struct rslc_intern_entry *e = &t->slots[j];
      if (!e->str)
         break;
      if (e->hash == h && e->len == len && !memcmp(e->str, s, len))
         return e->str;
      j = (j + 1) & t->mask;
   }

   copy = (char*)rslc_arena_alloc(c, len + 1);
   if (!copy)
      return NULL;
   memcpy(copy, s, len);
   copy[len]         = '\0';
   t->slots[j].str   = copy;
   t->slots[j].hash  = h;
   t->slots[j].len   = (uint32_t)len;
   t->count++;
   return copy;
}

const char *rslc_intern_cstr(struct rslc_ctx *c, const char *s)
{
   return rslc_intern(c, s, strlen(s));
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
      uintptr_t p   = (uintptr_t)opts->arena;
      uintptr_t pad = (RSLC_ARENA_ALIGN - (p & (RSLC_ARENA_ALIGN - 1)))
         & (RSLC_ARENA_ALIGN - 1);
      if (opts->arena_len <= pad)
      {
         rslc_err(c, "caller arena too small", NULL, 0);
         return false;
      }
      c->arena.fixed = true;
      c->arena.cur   = (uint8_t*)opts->arena + pad;
      c->arena.end   = (uint8_t*)opts->arena + opts->arena_len;
      c->arena.cap   = opts->arena_len;
   }
   else
      c->arena.cap   = opts->arena_cap ? opts->arena_cap : RSLC_ARENA_CAP_DEFAULT;

   /* Seed the intern table so the first identifier never rehashes. */
   if (!rslc_intern_rehash(c))
      return false;
   return true;
}

void rslc_ctx_close(struct rslc_ctx *c)
{
   struct rslc_chunk *k = c->arena.chunks;
   while (k)
   {
      struct rslc_chunk *next = k->next;
      rslc_mem_free(c->opts, k);
      k = next;
   }
   c->arena.chunks = NULL;
   c->arena.cur    = c->arena.end = NULL;
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
