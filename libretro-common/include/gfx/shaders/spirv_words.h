/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (spirv_words.h).
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

#ifndef __LIBRETRO_SDK_SPIRV_WORDS_H__
#define __LIBRETRO_SDK_SPIRV_WORDS_H__

/* SPIR-V words: the bottom of a SPIR-V writer, and nothing above it.
 *
 * A module is written as sections of words, each a growing buffer,
 * and joined once, in the order the specification lays them out, into
 * the caller's own storage (a blob, a mapped file, the words handed to
 * vkCreateShaderModule).  What a section holds, how ids are handed
 * out, which types and constants are shared: all the caller's.  rslc
 * (gfx/shaders/rslc_ir.c) writes through it, and so can a program that
 * makes its SPIR-V from its own IR, with no text and no tree between.
 *
 * Header only, every function inline but the cold grow path: an
 * instruction costs one capacity compare and its stores, nothing
 * called.  C89, no platform
 * header, no global state, no lock: re-entrant, any number of writers
 * on any number of threads.  Memory comes from the caller's grow hook,
 * so an arena, a pool reset per module, or realloc all fit; storage
 * sized up front from what the caller knows never grows at all. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The header's own inline keyword, chosen as retro_inline.h chooses
 * INLINE but never taken from outside: a core may define INLINE itself
 * (FBNeo, as "__inline static"), as retro_atomic.h found. */
#if defined(_WIN32) || defined(__INTEL_COMPILER)
#define SPIRV_WORDS_INLINE __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define SPIRV_WORDS_INLINE inline
#elif defined(__GNUC__)
#define SPIRV_WORDS_INLINE __inline__
#else
#define SPIRV_WORDS_INLINE
#endif

/* A host that keeps a word's lowest byte first, as SPIR-V packs a
 * string's bytes into words: there a string is copied as it lies. */
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) \
      || defined(_M_ARM) || defined(_M_ARM64))) \
   || (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) \
      && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define SPIRV_WORDS_LSB_FIRST 1
#endif

/* The five words before the first instruction */
#define SPIRV_WORDS_HEADER 5
#define SPIRV_WORDS_MAGIC  0x07230203u

/* Storage for a buffer of @new_bytes holding the @old_bytes at @old
 * (NULL when there are none): the old words copied over, or the block
 * grown in place.  NULL when there is no memory. */
typedef void *(*spirv_words_grow_t)(void *user, void *old,
      size_t old_bytes, size_t new_bytes);

struct spirv_words_buf
{
   uint32_t *v;
   uint32_t  n;    /* words written */
   uint32_t  cap;  /* words room */
};

struct spirv_words
{
   spirv_words_grow_t grow;
   void              *user;
};

/* The grow path, cold: kept out of line, so that every instruction
 * written carries only its compare and not the loop and the call. */
#if defined(__GNUC__) || defined(__clang__)
#define SPIRV_WORDS_COLD static __attribute__((noinline, unused))
#elif defined(_MSC_VER) && _MSC_VER >= 1300
#define SPIRV_WORDS_COLD static __declspec(noinline)
#else
#define SPIRV_WORDS_COLD static SPIRV_WORDS_INLINE
#endif

/* Room for @more words past b->n.  Grows to 256 words, then by
 * doubling, so that a buffer of n words was copied fewer than n times
 * in all. */
SPIRV_WORDS_COLD bool spirv_words_room_slow(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t more)
{
   uint32_t  need = b->n + more;
   uint32_t  ncap = b->cap ? b->cap : 256;
   uint32_t *nv;
   if (need < b->n)
      return false;  /* wrapped */
   while (ncap < need)
   {
      if (ncap > 0x7fffffffu)
         return false;
      ncap *= 2;
   }
   nv = (uint32_t*)w->grow(w->user, b->v,
         (size_t)b->n * sizeof(*nv), (size_t)ncap * sizeof(*nv));
   if (!nv)
      return false;
   b->v   = nv;
   b->cap = ncap;
   return true;
}

static SPIRV_WORDS_INLINE bool spirv_words_room(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t more)
{
   return b->cap - b->n >= more || spirv_words_room_slow(w, b, more);
}

/* Storage of @n words a caller sizes before writing: what is written
 * there never grows.  The buffer starts empty. */
static SPIRV_WORDS_INLINE bool spirv_words_reserve(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t n)
{
   return n <= b->cap || spirv_words_room_slow(w, b, n - b->n);
}

/* One word. */
static SPIRV_WORDS_INLINE bool spirv_words_put(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t word)
{
   if (!spirv_words_room(w, b, 1))
      return false;
   b->v[b->n++] = word;
   return true;
}

/* @n words. */
static SPIRV_WORDS_INLINE bool spirv_words_put_n(const struct spirv_words *w,
      struct spirv_words_buf *b, const uint32_t *words, uint32_t n)
{
   if (!spirv_words_room(w, b, n))
      return false;
   if (n)
      memcpy(b->v + b->n, words, n * sizeof(*words));
   b->n += n;
   return true;
}

/* An instruction's first word: its length in words, the opcode word
 * included, and its opcode. */
static SPIRV_WORDS_INLINE uint32_t spirv_words_opword(uint32_t op, uint32_t len)
{
   return (len << 16) | op;
}

/* The instruction @op with its @n operand words: one room check. */
static SPIRV_WORDS_INLINE bool spirv_words_op(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t op,
      const uint32_t *words, uint32_t n)
{
   uint32_t *p;
   if (!spirv_words_room(w, b, n + 1))
      return false;
   p    = b->v + b->n;
   p[0] = spirv_words_opword(op, n + 1);
   if (n)
      memcpy(p + 1, words, n * sizeof(*words));
   b->n += n + 1;
   return true;
}

/* The instruction @op, its @n operand words left for the caller to
 * store where the pointer returned says, with no staging array; NULL
 * when there is no memory. */
static SPIRV_WORDS_INLINE uint32_t *spirv_words_op_at(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t op, uint32_t n)
{
   uint32_t *p;
   if (!spirv_words_room(w, b, n + 1))
      return NULL;
   p     = b->v + b->n;
   p[0]  = spirv_words_opword(op, n + 1);
   b->n += n + 1;
   return p + 1;
}

/* Words a NUL-terminated literal string of @len bytes takes: its
 * bytes and at least one NUL, padded to a whole word. */
static SPIRV_WORDS_INLINE uint32_t spirv_words_str_len(size_t len)
{
   return (uint32_t)(len / 4 + 1);
}

/* The literal @str at @out, spirv_words_str_len words: its bytes,
 * the first in the lowest bits of its word on any host, NUL padded. */
static SPIRV_WORDS_INLINE void spirv_words_str_store(uint32_t *out,
      const char *str, size_t len)
{
   uint32_t nwords = spirv_words_str_len(len);
#ifdef SPIRV_WORDS_LSB_FIRST
   /* The bytes as they lie: the last word zeroed first is the padding */
   out[nwords - 1] = 0;
   memcpy(out, str, len);
#else
   uint32_t i;
   for (i = 0; i < nwords; i++)
   {
      uint32_t w = 0;
      uint32_t k;
      for (k = 0; k < 4; k++)
      {
         size_t idx = (size_t)i * 4 + k;
         if (idx < len)
            w |= (uint32_t)(uint8_t)str[idx] << (8 * k);
      }
      out[i] = w;
   }
#endif
}

/* The instruction @op: the @npre words, the literal @str, then the
 * @npost words. */
static SPIRV_WORDS_INLINE bool spirv_words_op_str(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t op,
      const uint32_t *pre, uint32_t npre, const char *str,
      const uint32_t *post, uint32_t npost)
{
   size_t    len    = strlen(str);
   uint32_t  nwords = spirv_words_str_len(len);
   uint32_t *p      = spirv_words_op_at(w, b, op, npre + nwords + npost);
   if (!p)
      return false;
   if (npre)
      memcpy(p, pre, npre * sizeof(*pre));
   spirv_words_str_store(p + npre, str, len);
   if (npost)
      memcpy(p + npre + nwords, post, npost * sizeof(*post));
   return true;
}

/* @n words put in at word @pos, what was from there on moved up after
 * them in one move. */
static SPIRV_WORDS_INLINE bool spirv_words_insert(const struct spirv_words *w,
      struct spirv_words_buf *b, uint32_t pos,
      const uint32_t *words, uint32_t n)
{
   if (pos > b->n || !spirv_words_room(w, b, n))
      return false;
   if (n)
   {
      memmove(b->v + pos + n, b->v + pos,
            (size_t)(b->n - pos) * sizeof(*words));
      memcpy(b->v + pos, words, n * sizeof(*words));
   }
   b->n += n;
   return true;
}

/* Words of the module the @count sections make, the header with them. */
static SPIRV_WORDS_INLINE size_t spirv_words_total(const struct spirv_words_buf *secs,
      unsigned count)
{
   size_t   words = SPIRV_WORDS_HEADER;
   unsigned s;
   for (s = 0; s < count; s++)
      words += secs[s].n;
   return words;
}

/* The module at @out (spirv_words_total words): the header, then each
 * section in turn.  @bound is one more than the highest id used. */
static SPIRV_WORDS_INLINE void spirv_words_join(uint32_t *out,
      const struct spirv_words_buf *secs, unsigned count,
      uint32_t version, uint32_t generator, uint32_t bound)
{
   unsigned s;
   out[0] = SPIRV_WORDS_MAGIC;
   out[1] = version;
   out[2] = generator;
   out[3] = bound;
   out[4] = 0;
   out   += SPIRV_WORDS_HEADER;
   for (s = 0; s < count; s++)
   {
      if (secs[s].n)
         memcpy(out, secs[s].v, secs[s].n * sizeof(*out));
      out += secs[s].n;
   }
}

RETRO_END_DECLS

#endif
