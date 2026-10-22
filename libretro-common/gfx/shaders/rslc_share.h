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
   struct rslc_chunk *spare;  /* released chunks kept for reuse (scratch) */
   uint8_t           *cur;
   uint8_t           *end;
   size_t             used;   /* bytes handed out, all chunks */
   size_t             held;   /* bytes allocated from the caller */
   size_t             cap;
   bool               fixed;
};

/* A point in a scratch arena to release back to. */
struct rslc_mark
{
   struct rslc_chunk *chunk;
   uint8_t           *cur;
   size_t             used;
};

/* Interned identifier.  One record per distinct spelling, allocated
 * once in the arena and never moved, so a pointer to it is the
 * identifier: tokens, macros and symbols compare by address.  kw is
 * the keyword code when the spelling is one, RSLC_KW_NONE otherwise;
 * macro is the current #define, if any. */
struct rslc_sym
{
   const char        *str;
   struct rslc_macro *macro;
   uint32_t           hash;
   uint32_t           len;
   uint32_t           index;   /* position in ctx->syms */
   uint16_t           kw;
   uint16_t           flags;   /* RSLC_SYM_* */
};

#define RSLC_SYM_BUSY     1u   /* macro being expanded: do not re-expand */

struct rslc_intern
{
   struct rslc_sym **slots;
   uint32_t          mask;   /* slot count - 1 */
   uint32_t          count;
};

/* Output buffer that becomes the result blob.  The backend reserves
 * the header and reflection section by starting len at that size and
 * sizes the whole buffer from the IR up front, so the blob is one
 * allocation and the buffer is the blob: no final copy.  Growth by
 * doubling exists for a misestimate only; it counts as a defect when
 * it fires on a corpus shader. */
struct rslc_pbuf
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
   unsigned grows;
};

/* Tokens.  Sixteen bytes: a stage expands to hundreds of thousands of
 * them in the worst corpus shaders.  ref is the sym index for an
 * identifier, else the byte offset of the spelling in the source, or
 * in ctx->synth when RSLC_TF_SYNTH is set (text the preprocessor made
 * up: __LINE__, defined()).  rslc_tok_text / rslc_tok_sym resolve it.
 * The preprocessor leaves the stream in 4096-token chunks
 * (ctx->tok_chunks); rslc_tok_at indexes them. */
enum rslc_tk
{
   RSLC_TK_EOF = 0,
   RSLC_TK_IDENT,
   RSLC_TK_NUMBER,
   RSLC_TK_STRING,     /* "..." - only #line uses it */
   RSLC_TK_PUNCT,      /* kind in punct, see RSLC_P_* */
   RSLC_TK_NEWLINE,    /* preprocessing only; never reaches the parser */
   RSLC_TK_OTHER       /* a byte the language does not use */
};

/* Punctuators, longest match first in the lexer table. */
enum rslc_punct
{
   RSLC_P_NONE = 0,
   RSLC_P_LSHIFT_ASSIGN, RSLC_P_RSHIFT_ASSIGN,
   RSLC_P_LSHIFT, RSLC_P_RSHIFT, RSLC_P_INC, RSLC_P_DEC,
   RSLC_P_LE, RSLC_P_GE, RSLC_P_EQ, RSLC_P_NE,
   RSLC_P_AND, RSLC_P_OR, RSLC_P_XOR,
   RSLC_P_ADD_ASSIGN, RSLC_P_SUB_ASSIGN, RSLC_P_MUL_ASSIGN,
   RSLC_P_DIV_ASSIGN, RSLC_P_MOD_ASSIGN, RSLC_P_BAND_ASSIGN,
   RSLC_P_BOR_ASSIGN, RSLC_P_BXOR_ASSIGN,
   RSLC_P_LPAREN, RSLC_P_RPAREN, RSLC_P_LBRACKET, RSLC_P_RBRACKET,
   RSLC_P_LBRACE, RSLC_P_RBRACE, RSLC_P_DOT, RSLC_P_COMMA,
   RSLC_P_COLON, RSLC_P_SEMI, RSLC_P_ASSIGN, RSLC_P_PLUS, RSLC_P_MINUS,
   RSLC_P_STAR, RSLC_P_SLASH, RSLC_P_PERCENT, RSLC_P_LT, RSLC_P_GT,
   RSLC_P_BANG, RSLC_P_TILDE, RSLC_P_AMP, RSLC_P_PIPE, RSLC_P_CARET,
   RSLC_P_QUESTION, RSLC_P_HASH, RSLC_P_PASTE,
   RSLC_P_COUNT
};

#define RSLC_TF_BOL      1u  /* first token on its line */
#define RSLC_TF_SPACE    2u  /* whitespace precedes it */
#define RSLC_TF_NOEXPAND 4u  /* identifier painted: never a macro again */

#define RSLC_TF_SYNTH    8u  /* spelling lives in ctx->synth, not the source */

struct rslc_tok
{
   uint32_t ref;
   uint32_t line;
   uint16_t len;
   uint8_t  kind;    /* enum rslc_tk */
   uint8_t  punct;   /* enum rslc_punct when kind is PUNCT */
   uint8_t  flags;   /* RSLC_TF_* */
   uint8_t  file;    /* index into ctx files, from #line */
   uint16_t pad;
};

#define RSLC_TOK_CHUNK_SHIFT 12
#define RSLC_TOK_CHUNK       (1u << RSLC_TOK_CHUNK_SHIFT)
#define rslc_tok_at(c, i) \
   (&(c)->tok_chunks[(i) >> RSLC_TOK_CHUNK_SHIFT][(i) & (RSLC_TOK_CHUNK - 1)])

/* A #define.  Parameters and body are token arrays in the arena; a
 * parameter reference in the body is an IDENT whose sym is one of the
 * params. */
struct rslc_macro
{
   struct rslc_sym  *name;
   struct rslc_sym **params;
   struct rslc_tok  *body;
   uint32_t          nparams;
   uint32_t          nbody;
   bool              function_like;
};

/* Keyword codes, assigned to syms when the context is seeded. */
enum rslc_kw
{
   RSLC_KW_NONE = 0,
   /* storage and qualifiers */
   RSLC_KW_CONST, RSLC_KW_IN, RSLC_KW_OUT, RSLC_KW_INOUT, RSLC_KW_UNIFORM,
   RSLC_KW_FLAT, RSLC_KW_SMOOTH, RSLC_KW_NOPERSPECTIVE, RSLC_KW_CENTROID,
   RSLC_KW_SAMPLE, RSLC_KW_INVARIANT, RSLC_KW_PRECISE, RSLC_KW_LAYOUT,
   RSLC_KW_PRECISION, RSLC_KW_LOWP, RSLC_KW_MEDIUMP, RSLC_KW_HIGHP,
   RSLC_KW_STRUCT,
   /* control */
   RSLC_KW_IF, RSLC_KW_ELSE, RSLC_KW_FOR, RSLC_KW_WHILE, RSLC_KW_DO,
   RSLC_KW_SWITCH, RSLC_KW_CASE, RSLC_KW_DEFAULT, RSLC_KW_BREAK,
   RSLC_KW_CONTINUE, RSLC_KW_RETURN, RSLC_KW_DISCARD,
   /* literals */
   RSLC_KW_TRUE, RSLC_KW_FALSE,
   /* types: the order within each family is width 1..4 */
   RSLC_KW_VOID, RSLC_KW_BOOL, RSLC_KW_INT, RSLC_KW_UINT, RSLC_KW_FLOAT,
   RSLC_KW_BVEC2, RSLC_KW_BVEC3, RSLC_KW_BVEC4,
   RSLC_KW_IVEC2, RSLC_KW_IVEC3, RSLC_KW_IVEC4,
   RSLC_KW_UVEC2, RSLC_KW_UVEC3, RSLC_KW_UVEC4,
   RSLC_KW_VEC2, RSLC_KW_VEC3, RSLC_KW_VEC4,
   RSLC_KW_MAT2, RSLC_KW_MAT3, RSLC_KW_MAT4,
   RSLC_KW_MAT2X2, RSLC_KW_MAT2X3, RSLC_KW_MAT2X4,
   RSLC_KW_MAT3X2, RSLC_KW_MAT3X3, RSLC_KW_MAT3X4,
   RSLC_KW_MAT4X2, RSLC_KW_MAT4X3, RSLC_KW_MAT4X4,
   RSLC_KW_SAMPLER2D,
   RSLC_KW_COUNT
};

/* Source file names seen through #line, for diagnostics. */
#define RSLC_MAX_FILES 64

/* Per-compile state shared by the three TUs. */
struct rslc_ctx
{
   const struct rslc_opts *opts;
   struct rslc_arena       arena;      /* lives for the compile */
   struct rslc_arena       scratch;    /* mark / release transients */
   struct rslc_intern      intern;
   struct rslc_sym       **syms;       /* dense, by sym->index */
   struct rslc_tok       **tok_chunks; /* preprocessed stream, EOF-terminated */
   const char             *src;        /* the stage source, for token text */
   char                   *synth;      /* preprocessor-made spellings */
   const char             *files[RSLC_MAX_FILES];
   uint32_t                nsyms;
   uint32_t                syms_cap;
   uint32_t                ntoks;
   uint32_t                ntok_chunks;
   uint32_t                nsynth;
   uint32_t                synth_cap;
   uint32_t                nfiles;
   uint32_t                version;    /* from #version */
   enum rslc_stage         stage;
   bool                    es;         /* #version ... es */
   bool                    failed;
};

/* Allocator wrappers: opts->alloc / free_fn or malloc / free. */
void *rslc_mem_alloc(const struct rslc_opts *opts, size_t len);
void  rslc_mem_free(const struct rslc_opts *opts, void *ptr);

/* Context lifetime.  rslc_ctx_init validates opts, attaches or opens
 * the arena and seeds the intern table with the keywords;
 * rslc_ctx_close releases arena chunks (never a caller-supplied
 * block). */
bool  rslc_ctx_init(struct rslc_ctx *c, const struct rslc_opts *opts,
      enum rslc_stage stage);
void  rslc_ctx_close(struct rslc_ctx *c);

/* Arena.  Returns NULL and marks the compile failed when the cap or
 * the fixed block is exhausted; callers propagate NULL upward.  The
 * scratch arena shares the cap with the main one and gives memory
 * back on release, keeping its chunks for the next mark. */
void *rslc_arena_alloc(struct rslc_ctx *c, size_t len);
void *rslc_arena_zalloc(struct rslc_ctx *c, size_t len);
void *rslc_scratch_alloc(struct rslc_ctx *c, size_t len);
void  rslc_scratch_mark(struct rslc_ctx *c, struct rslc_mark *m);
void  rslc_scratch_release(struct rslc_ctx *c, const struct rslc_mark *m);

/* Token text and symbol. */
const char      *rslc_tok_text(const struct rslc_ctx *c, const struct rslc_tok *t);
struct rslc_sym *rslc_tok_sym(const struct rslc_ctx *c, const struct rslc_tok *t);

/* Intern.  rslc_sym returns the record for a spelling, creating it on
 * first sight; rslc_intern is the string of that record.  NULL on
 * arena exhaustion. */
struct rslc_sym *rslc_sym(struct rslc_ctx *c, const char *s, size_t len);
const char *rslc_intern(struct rslc_ctx *c, const char *s, size_t len);
const char *rslc_intern_cstr(struct rslc_ctx *c, const char *s);
uint32_t    rslc_hash(const char *s, size_t len);

/* Print buffer.  rslc_pbuf_init allocates prefix + hint bytes once;
 * rslc_pbuf_reserve guarantees extra free bytes; rslc_pbuf_put /
 * _putc append.  All return false and mark the compile failed when
 * the allocator refuses.  The buffer is released by rslc_pbuf_free, or
 * detached into a result by the backend.  grows counts the times the
 * hint was wrong. */
bool  rslc_pbuf_init(struct rslc_ctx *c, struct rslc_pbuf *b,
      size_t prefix, size_t hint);
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

/* Preprocessor (TU 1).  Lexes src and runs the directives and macros
 * over it, leaving c->toks / c->ntoks as the parser's input: IDENT,
 * NUMBER, PUNCT and a final EOF, with the #version and #extension
 * lines consumed into c->version / c->es and the extension list.
 * Returns false with the error set. */
bool  rslc_pp_run(struct rslc_ctx *c, const char *src, size_t len);

/* Entry points across the TUs.  rslc_front_compile (TU 1) builds the
 * IR; rslc_emit_dst (TU 2) fills result for opts->dst, handling SPIR-V
 * and reflection itself and calling rslc_emit_text (TU 3) for the
 * printed languages unless RSLC_NO_TEXT is defined. */
bool  rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len);
bool  rslc_emit_dst(struct rslc_ctx *c, struct rslc_result *result);
bool  rslc_emit_text(struct rslc_ctx *c, struct rslc_result *result);

RETRO_END_DECLS

#endif
