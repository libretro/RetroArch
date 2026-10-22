/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_share.h).
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

#ifndef __LIBRETRO_SDK_RSLC_SHARE_H__
#define __LIBRETRO_SDK_RSLC_SHARE_H__

/* Internal to the three rslc translation units.  Not installed, not
 * part of the ABI.
 *
 * Memory model: one bump arena per compile holds every intermediate
 * (tokens, IR, intern table, reflection staging).  Nothing in it is
 * freed individually; it dies with the compile.  The only other
 * allocation is the output blob, which the caller owns.  The arena is
 * either a block the caller handed in (fixed: fail when it runs out)
 * or a chain of RSLC_ARENA_CHUNK chunks from the caller's allocator
 * under opts->arena_cap.  The implementations live in rslc_ir.c so
 * each TU carries one copy of the code, not three. */

#include <gfx/shaders/rslc.h>

RETRO_BEGIN_DECLS

/* The largest alignment any arena object needs. */
#define RSLC_ARENA_ALIGN 8

struct rslc_chunk
{
   struct rslc_chunk *next;
   size_t             len;    /* usable bytes after this header */
};

struct rslc_arena
{
   struct rslc_chunk *chunks; /* newest first; NULL with a fixed block */
   uint8_t           *cur;
   uint8_t           *end;
   size_t             used;   /* bytes handed out, all chunks */
   size_t             held;   /* bytes allocated from the caller */
   size_t             cap;
   bool               fixed;
};

/* Open-addressing string intern table.  Entries and strings live in
 * the arena.  Interning a string already present returns the same
 * pointer, so identifiers compare by address after the lexer. */
struct rslc_intern_entry
{
   const char *str;
   uint32_t    hash;
   uint32_t    len;
};

struct rslc_intern
{
   struct rslc_intern_entry *slots;
   uint32_t                  mask;   /* slot count - 1 */
   uint32_t                  count;
};

/* Growing output buffer that becomes the result blob.  The caller
 * reserves the header and reflection section up front by starting
 * len at that size; the backend appends the payload; the blob is the
 * buffer, so there is no final copy. */
struct rslc_pbuf
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
};

/* Per-compile state shared by the three TUs. */
struct rslc_ctx
{
   const struct rslc_opts *opts;
   struct rslc_arena       arena;
   struct rslc_intern      intern;
   enum rslc_stage         stage;
   bool                    failed;
};

/* Allocator wrappers: opts->alloc / free_fn or malloc / free. */
void *rslc_mem_alloc(const struct rslc_opts *opts, size_t len);
void  rslc_mem_free(const struct rslc_opts *opts, void *ptr);

/* Context lifetime.  rslc_ctx_init validates opts, attaches or opens
 * the arena and seeds the intern table; rslc_ctx_close releases
 * arena chunks (never a caller-supplied block). */
bool  rslc_ctx_init(struct rslc_ctx *c, const struct rslc_opts *opts,
      enum rslc_stage stage);
void  rslc_ctx_close(struct rslc_ctx *c);

/* Arena.  Returns NULL and marks the compile failed when the cap or
 * the fixed block is exhausted; callers propagate NULL upward. */
void *rslc_arena_alloc(struct rslc_ctx *c, size_t len);
void *rslc_arena_zalloc(struct rslc_ctx *c, size_t len);

/* Intern.  Both return a NUL-terminated copy owned by the arena, or
 * NULL on arena exhaustion. */
const char *rslc_intern(struct rslc_ctx *c, const char *s, size_t len);
const char *rslc_intern_cstr(struct rslc_ctx *c, const char *s);
uint32_t    rslc_hash(const char *s, size_t len);

/* Print buffer.  rslc_pbuf_reserve guarantees extra free bytes;
 * rslc_pbuf_put / _putc append.  All return false and mark the compile
 * failed when the allocator refuses.  The buffer is released by
 * rslc_pbuf_free, or detached into a result by the backend. */
bool  rslc_pbuf_init(struct rslc_ctx *c, struct rslc_pbuf *b, size_t prefix);
bool  rslc_pbuf_reserve(struct rslc_ctx *c, struct rslc_pbuf *b, size_t extra);
bool  rslc_pbuf_put(struct rslc_ctx *c, struct rslc_pbuf *b,
      const void *src, size_t len);
bool  rslc_pbuf_puts(struct rslc_ctx *c, struct rslc_pbuf *b, const char *s);
bool  rslc_pbuf_putc(struct rslc_ctx *c, struct rslc_pbuf *b, char ch);
bool  rslc_pbuf_putu(struct rslc_ctx *c, struct rslc_pbuf *b, uint32_t v);
void  rslc_pbuf_free(struct rslc_ctx *c, struct rslc_pbuf *b);

/* Error sink.  Writes "a" then "b" then, when line is non-zero,
 * " (line N)" into opts->err, truncating to err_cap, and marks the
 * compile failed.  Only the first message of a compile is kept.  No
 * stdio anywhere in the compiler. */
void  rslc_err(struct rslc_ctx *c, const char *a, const char *b,
      unsigned line);

/* Entry points across the TUs.  rslc_front_compile (TU 1) builds the
 * IR; rslc_emit_dst (TU 2) fills result for opts->dst, handling SPIR-V
 * and reflection itself and calling rslc_emit_text (TU 3) for the
 * printed languages unless RSLC_NO_TEXT is defined. */
bool  rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len);
bool  rslc_emit_dst(struct rslc_ctx *c, struct rslc_result *result);
bool  rslc_emit_text(struct rslc_ctx *c, struct rslc_result *result);

RETRO_END_DECLS

#endif
