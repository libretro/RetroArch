/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_front.c).
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

/* rslc TU 1: preprocessor, lexer, parser, typecheck.
 *
 * Owns rslc_compile().  Reads the stage source as a pointer and a
 * length; never copies it, never NUL-terminates it, never reads past
 * len.  Everything it builds goes in the arena.
 *
 * The preprocessor is the GLSL one: object-like and function-like
 * macros with prescanned arguments, #if with integer constant
 * expressions and defined(), #ifdef / #ifndef / #elif / #else / #endif,
 * #undef, #version, #extension, #line N "file", #error, #pragma
 * ignored, ## token paste.  Stringize is refused: no shader in the
 * corpus uses it.  Unknown directives are an error in an active branch
 * and skipped in an inactive one, where glslang also reads "##else"
 * and "# # else" as "#else", and so does this.  Line continuations are
 * spliced by the lexer.
 *
 * Memory: the output stream goes to the main arena in 4096-token
 * chunks.  Everything a macro expansion builds on the way (argument
 * lists, prescans, substituted bodies) is scratch, released when the
 * expansion that started at top level has drained, so a shader that
 * expands into hundreds of thousands of tokens costs the tokens and
 * nothing else. */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef RSLC_SPV_TRACE
#include <stdio.h>
#endif

#include "rslc_share.h"

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

struct rslc_lex
{
   struct rslc_ctx *c;
   const char      *src;
   size_t           len;
   size_t           pos;
   uint32_t         line;
   uint8_t          file;
   bool             bol;      /* next token is first on its line */
};

struct rslc_punct_def
{
   const char *text;
   uint8_t     len;
   uint8_t     kind;
};

/* Longest spellings first so the scan takes the longest match. */
static const struct rslc_punct_def rslc_puncts[] = {
   { "<<=", 3, RSLC_P_LSHIFT_ASSIGN }, { ">>=", 3, RSLC_P_RSHIFT_ASSIGN },
   { "<<", 2, RSLC_P_LSHIFT }, { ">>", 2, RSLC_P_RSHIFT },
   { "++", 2, RSLC_P_INC }, { "--", 2, RSLC_P_DEC },
   { "<=", 2, RSLC_P_LE }, { ">=", 2, RSLC_P_GE },
   { "==", 2, RSLC_P_EQ }, { "!=", 2, RSLC_P_NE },
   { "&&", 2, RSLC_P_AND }, { "||", 2, RSLC_P_OR }, { "^^", 2, RSLC_P_XOR },
   { "+=", 2, RSLC_P_ADD_ASSIGN }, { "-=", 2, RSLC_P_SUB_ASSIGN },
   { "*=", 2, RSLC_P_MUL_ASSIGN }, { "/=", 2, RSLC_P_DIV_ASSIGN },
   { "%=", 2, RSLC_P_MOD_ASSIGN }, { "&=", 2, RSLC_P_BAND_ASSIGN },
   { "|=", 2, RSLC_P_BOR_ASSIGN }, { "^=", 2, RSLC_P_BXOR_ASSIGN },
   { "##", 2, RSLC_P_PASTE },
   { "(", 1, RSLC_P_LPAREN }, { ")", 1, RSLC_P_RPAREN },
   { "[", 1, RSLC_P_LBRACKET }, { "]", 1, RSLC_P_RBRACKET },
   { "{", 1, RSLC_P_LBRACE }, { "}", 1, RSLC_P_RBRACE },
   { ".", 1, RSLC_P_DOT }, { ",", 1, RSLC_P_COMMA },
   { ":", 1, RSLC_P_COLON }, { ";", 1, RSLC_P_SEMI },
   { "=", 1, RSLC_P_ASSIGN }, { "+", 1, RSLC_P_PLUS }, { "-", 1, RSLC_P_MINUS },
   { "*", 1, RSLC_P_STAR }, { "/", 1, RSLC_P_SLASH }, { "%", 1, RSLC_P_PERCENT },
   { "<", 1, RSLC_P_LT }, { ">", 1, RSLC_P_GT },
   { "!", 1, RSLC_P_BANG }, { "~", 1, RSLC_P_TILDE },
   { "&", 1, RSLC_P_AMP }, { "|", 1, RSLC_P_PIPE }, { "^", 1, RSLC_P_CARET },
   { "?", 1, RSLC_P_QUESTION }, { "#", 1, RSLC_P_HASH }
};

static int rslc_is_ident_start(char ch)
{
   return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static int rslc_is_digit(char ch)
{
   return ch >= '0' && ch <= '9';
}

static int rslc_is_ident(char ch)
{
   return rslc_is_ident_start(ch) || rslc_is_digit(ch);
}

/* Byte at pos + off, or 0 past the end. */
static char rslc_lex_at(const struct rslc_lex *l, size_t off)
{
   if (l->pos + off >= l->len)
      return 0;
   return l->src[l->pos + off];
}

/* Skips "\\\n" and "\\\r\n" splices at pos, counting the lines. */
static void rslc_lex_splice(struct rslc_lex *l)
{
   for (;;)
   {
      if (rslc_lex_at(l, 0) != '\\')
         return;
      if (rslc_lex_at(l, 1) == '\n')
      {
         l->pos += 2;
         l->line++;
      }
      else if (rslc_lex_at(l, 1) == '\r' && rslc_lex_at(l, 2) == '\n')
      {
         l->pos += 3;
         l->line++;
      }
      else
         return;
   }
}

/* Skips whitespace and comments, stopping at a newline (which the
 * caller tokenises).  A newline inside a block comment counts as a
 * line but not as a line break. */
static void rslc_lex_skip(struct rslc_lex *l, bool *space)
{
   *space = false;
   for (;;)
   {
      char ch;
      rslc_lex_splice(l);
      ch = rslc_lex_at(l, 0);
      if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v')
      {
         l->pos++;
         *space = true;
      }
      else if (ch == '/' && rslc_lex_at(l, 1) == '/')
      {
         l->pos += 2;
         for (;;)
         {
            rslc_lex_splice(l);
            if (l->pos >= l->len || rslc_lex_at(l, 0) == '\n')
               break;
            l->pos++;
         }
         *space = true;
      }
      else if (ch == '/' && rslc_lex_at(l, 1) == '*')
      {
         l->pos += 2;
         while (l->pos < l->len)
         {
            if (rslc_lex_at(l, 0) == '*' && rslc_lex_at(l, 1) == '/')
            {
               l->pos += 2;
               break;
            }
            if (rslc_lex_at(l, 0) == '\n')
               l->line++;
            l->pos++;
         }
         *space = true;
      }
      else
         return;
   }
}

static void rslc_tok_set(struct rslc_tok *t, uint8_t kind, uint32_t ref,
      uint32_t len)
{
   t->kind = kind;
   t->ref  = ref;
   t->len  = (uint16_t)len;
}

/* Reads one raw token.  Numbers are pp-numbers; the parser converts
 * them.  Returns false only on arena exhaustion. */
static bool rslc_lex_next(struct rslc_lex *l, struct rslc_tok *t)
{
   bool   space;
   char   ch;
   size_t start;

   rslc_lex_skip(l, &space);
   memset(t, 0, sizeof(*t));
   t->line  = l->line;
   t->file  = l->file;
   t->flags = (uint8_t)((l->bol ? RSLC_TF_BOL : 0) | (space ? RSLC_TF_SPACE : 0));

   if (l->pos >= l->len)
   {
      t->kind = RSLC_TK_EOF;
      return true;
   }

   ch    = l->src[l->pos];
   start = l->pos;

   if (ch == '\n')
   {
      l->pos++;
      l->line++;
      l->bol  = true;
      rslc_tok_set(t, RSLC_TK_NEWLINE, (uint32_t)start, 1);
      return true;
   }
   l->bol = false;

   if (rslc_is_ident_start(ch))
   {
      struct rslc_sym *sym;
      while (l->pos < l->len && rslc_is_ident(l->src[l->pos]))
         l->pos++;
      sym = rslc_sym(l->c, l->src + start, l->pos - start);
      if (!sym)
         return false;
      rslc_tok_set(t, RSLC_TK_IDENT, sym->index, (uint32_t)(l->pos - start));
      return true;
   }

   if (rslc_is_digit(ch) || (ch == '.' && rslc_is_digit(rslc_lex_at(l, 1))))
   {
      /* GLSL numbers, not C pp-numbers: "1.0.xxx" is a literal and a
       * swizzle. */
      if (ch == '0' && (rslc_lex_at(l, 1) == 'x' || rslc_lex_at(l, 1) == 'X'))
      {
         l->pos += 2;
         while (rslc_is_ident(rslc_lex_at(l, 0)))
            l->pos++;
      }
      else
      {
         bool dot = false;
         while (rslc_is_digit(rslc_lex_at(l, 0)))
            l->pos++;
         if (rslc_lex_at(l, 0) == '.')
         {
            dot = true;
            l->pos++;
            while (rslc_is_digit(rslc_lex_at(l, 0)))
               l->pos++;
         }
         if (rslc_lex_at(l, 0) == 'e' || rslc_lex_at(l, 0) == 'E')
         {
            size_t save = l->pos;
            l->pos++;
            if (rslc_lex_at(l, 0) == '+' || rslc_lex_at(l, 0) == '-')
               l->pos++;
            if (rslc_is_digit(rslc_lex_at(l, 0)))
            {
               dot = true;
               while (rslc_is_digit(rslc_lex_at(l, 0)))
                  l->pos++;
            }
            else
               l->pos = save;
         }
         if (dot)
         {
            if (rslc_lex_at(l, 0) == 'f' || rslc_lex_at(l, 0) == 'F')
               l->pos++;
            else if ((rslc_lex_at(l, 0) == 'l' || rslc_lex_at(l, 0) == 'L')
                  && (rslc_lex_at(l, 1) == 'f' || rslc_lex_at(l, 1) == 'F'))
               l->pos += 2;
         }
         else if (rslc_lex_at(l, 0) == 'u' || rslc_lex_at(l, 0) == 'U')
            l->pos++;
      }
      rslc_tok_set(t, RSLC_TK_NUMBER, (uint32_t)start, (uint32_t)(l->pos - start));
      return true;
   }

   if (ch == '"')
   {
      l->pos++;
      while (l->pos < l->len && l->src[l->pos] != '"' && l->src[l->pos] != '\n')
         l->pos++;
      if (rslc_lex_at(l, 0) == '"')
         l->pos++;
      rslc_tok_set(t, RSLC_TK_STRING, (uint32_t)start, (uint32_t)(l->pos - start));
      return true;
   }

   {
      unsigned i;
      for (i = 0; i < sizeof(rslc_puncts) / sizeof(rslc_puncts[0]); i++)
      {
         const struct rslc_punct_def *p = &rslc_puncts[i];
         if (l->len - l->pos >= p->len && !memcmp(l->src + l->pos, p->text, p->len))
         {
            l->pos  += p->len;
            rslc_tok_set(t, RSLC_TK_PUNCT, (uint32_t)start, p->len);
            t->punct = p->kind;
            return true;
         }
      }
   }

   l->pos++;
   rslc_tok_set(t, RSLC_TK_OTHER, (uint32_t)start, 1);
   return true;
}

/* ------------------------------------------------------------------ */
/* Token lists                                                          */
/* ------------------------------------------------------------------ */

/* A growable token array in an arena.  Growth re-bumps at twice the
 * size and abandons the old block.  scratch selects the arena. */
struct rslc_tokbuf
{
   struct rslc_tok *v;
   uint32_t         n;
   uint32_t         cap;
   bool             scratch;
};

static void rslc_tokbuf_init(struct rslc_tokbuf *b, bool scratch)
{
   b->v = NULL; b->n = 0; b->cap = 0; b->scratch = scratch;
}

static bool rslc_tokbuf_push(struct rslc_ctx *c, struct rslc_tokbuf *b,
      const struct rslc_tok *t)
{
   if (b->n == b->cap)
   {
      uint32_t         ncap = b->cap ? b->cap * 2 : 32;
      struct rslc_tok *nv   = (struct rslc_tok*)(b->scratch
            ? rslc_scratch_alloc(c, ncap * sizeof(*nv))
            : rslc_arena_alloc(c, ncap * sizeof(*nv)));
      if (!nv)
         return false;
      if (b->n)
         memcpy(nv, b->v, b->n * sizeof(*nv));
      b->v   = nv;
      b->cap = ncap;
   }
   b->v[b->n++] = *t;
   return true;
}

/* Appends a token to the chunked output stream. */
static bool rslc_out_push(struct rslc_ctx *c, const struct rslc_tok *t)
{
   uint32_t chunk = c->ntoks >> RSLC_TOK_CHUNK_SHIFT;
   if (chunk == c->ntok_chunks)
   {
      struct rslc_tok *nc;
      if ((chunk & (chunk + 1)) == 0 || !c->tok_chunks)
      {
         /* The chunk table doubles: 1, 2, 4, ... */
         uint32_t           ncap = chunk ? (chunk + 1) * 2 : 1;
         struct rslc_tok  **nt   = (struct rslc_tok**)rslc_arena_alloc(c,
               ncap * sizeof(*nt));
         if (!nt)
            return false;
         if (chunk)
            memcpy(nt, c->tok_chunks, chunk * sizeof(*nt));
         c->tok_chunks = nt;
      }
      nc = (struct rslc_tok*)rslc_arena_alloc(c, RSLC_TOK_CHUNK * sizeof(*nc));
      if (!nc)
         return false;
      c->tok_chunks[chunk] = nc;
      c->ntok_chunks++;
   }
   c->tok_chunks[chunk][c->ntoks & (RSLC_TOK_CHUNK - 1)] = *t;
   c->ntoks++;
   return true;
}

/* Stores a made-up spelling and returns its synth offset. */
static bool rslc_synth(struct rslc_ctx *c, const char *s, uint32_t len,
      uint32_t *off)
{
   if (c->nsynth + len > c->synth_cap)
   {
      uint32_t ncap = c->synth_cap ? c->synth_cap * 2 : 1024;
      char    *nv;
      while (ncap < c->nsynth + len)
         ncap *= 2;
      nv = (char*)rslc_arena_alloc(c, ncap);
      if (!nv)
         return false;
      if (c->nsynth)
         memcpy(nv, c->synth, c->nsynth);
      c->synth     = nv;
      c->synth_cap = ncap;
   }
   memcpy(c->synth + c->nsynth, s, len);
   *off       = c->nsynth;
   c->nsynth += len;
   return true;
}

/* ------------------------------------------------------------------ */
/* Preprocessor                                                        */
/* ------------------------------------------------------------------ */

/* Token source: the raw lexer, or a macro expansion being read back.
 * Sources stack; a macro's sym is BUSY while its source is on the
 * stack so its own name inside its expansion is never re-expanded. */
struct rslc_pp_src
{
   const struct rslc_tok *v;
   struct rslc_sym       *macro;  /* sym to un-BUSY when exhausted */
   uint32_t               n;
   uint32_t               pos;
};

#define RSLC_PP_MAX_SRC   64
#define RSLC_PP_MAX_COND  64
#define RSLC_PP_MAX_ARGS  32

struct rslc_pp_cond
{
   bool active;      /* this branch is being output */
   bool taken;       /* some branch of this #if has been taken */
   bool parent;      /* enclosing branch active */
   bool in_else;
};

struct rslc_pp
{
   struct rslc_ctx    *c;
   struct rslc_lex     lex;
   struct rslc_pp_src  srcs[RSLC_PP_MAX_SRC];
   struct rslc_pp_cond conds[RSLC_PP_MAX_COND];
   struct rslc_tok     peeked;
   struct rslc_mark    top_mark;    /* scratch state at top level */
   struct rslc_sym    *sym_defined;
   struct rslc_sym    *sym_line;
   uint32_t            nsrc;
   uint32_t            ncond;
   bool                top_marked;   /* top_mark holds a pending release */
   bool                has_peek;
   bool                seen_version;
   bool                seen_token;   /* any token after #version */
};

static bool rslc_pp_active(const struct rslc_pp *pp)
{
   return pp->ncond == 0 || pp->conds[pp->ncond - 1].active;
}

static void rslc_pp_pop_src(struct rslc_pp *pp)
{
   struct rslc_pp_src *s = &pp->srcs[pp->nsrc - 1];
   if (s->macro)
      s->macro->flags &= (uint16_t)~RSLC_SYM_BUSY;
   pp->nsrc--;
}

/* Everything an expansion built in scratch is garbage once the
 * expansion has drained; the main loop calls this between tokens. */
static void rslc_pp_settle(struct rslc_pp *pp)
{
   if (pp->nsrc == 0 && pp->top_marked)
   {
      rslc_scratch_release(pp->c, &pp->top_mark);
      pp->top_marked = false;
   }
}

/* Next raw token from the source stack, popping exhausted expansions. */
static bool rslc_pp_raw(struct rslc_pp *pp, struct rslc_tok *t)
{
   if (pp->has_peek)
   {
      *t = pp->peeked;
      pp->has_peek = false;
      return true;
   }
   while (pp->nsrc)
   {
      struct rslc_pp_src *s = &pp->srcs[pp->nsrc - 1];
      if (s->pos < s->n)
      {
         *t = s->v[s->pos++];
         return true;
      }
      rslc_pp_pop_src(pp);
   }
   return rslc_lex_next(&pp->lex, t);
}

static void rslc_pp_unread(struct rslc_pp *pp, const struct rslc_tok *t)
{
   pp->peeked   = *t;
   pp->has_peek = true;
}

static bool rslc_pp_push_src(struct rslc_pp *pp, const struct rslc_tok *v,
      uint32_t n, struct rslc_sym *macro)
{
   struct rslc_pp_src *s;
   if (pp->nsrc == RSLC_PP_MAX_SRC)
   {
      rslc_err(pp->c, "macro expansion nested too deep", NULL, pp->lex.line);
      return false;
   }
   s        = &pp->srcs[pp->nsrc++];
   s->v     = v;
   s->n     = n;
   s->pos   = 0;
   s->macro = macro;
   if (macro)
      macro->flags |= RSLC_SYM_BUSY;
   return true;
}

static bool rslc_tok_is_punct(const struct rslc_tok *t, enum rslc_punct p)
{
   return t->kind == RSLC_TK_PUNCT && t->punct == p;
}

static bool rslc_tok_is_eol(const struct rslc_tok *t)
{
   return t->kind == RSLC_TK_NEWLINE || t->kind == RSLC_TK_EOF;
}

#define RSLC_TXT(pp, t) rslc_tok_text((pp)->c, (t))
#define RSLC_SYM(pp, t) rslc_tok_sym((pp)->c, (t))

/* Skips the rest of the current line in the raw stream. */
static bool rslc_pp_skip_line(struct rslc_pp *pp)
{
   struct rslc_tok t;
   do
   {
      if (!rslc_pp_raw(pp, &t))
         return false;
   } while (!rslc_tok_is_eol(&t));
   return true;
}

/* Reads the raw tokens of the current line (not expanded) into a
 * token buffer; stops before the newline, which is consumed. */
static bool rslc_pp_read_line(struct rslc_pp *pp, struct rslc_tokbuf *b,
      bool scratch)
{
   struct rslc_tok t;
   rslc_tokbuf_init(b, scratch);
   for (;;)
   {
      if (!rslc_pp_raw(pp, &t))
         return false;
      if (rslc_tok_is_eol(&t))
         return true;
      if (!rslc_tokbuf_push(pp->c, b, &t))
         return false;
   }
}

/* Decimal, octal and hex integers with optional u/U: what #if and
 * #line accept. */
static bool rslc_pp_number(const char *s, uint32_t n, long *out)
{
   uint32_t i = 0;
   long     v = 0;

   if (n > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
   {
      for (i = 2; i < n; i++)
      {
         char ch = s[i];
         int  d;
         if (ch >= '0' && ch <= '9')      d = ch - '0';
         else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
         else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
         else if (ch == 'u' || ch == 'U') { i++; break; }
         else return false;
         v = v * 16 + d;
      }
   }
   else if (n > 1 && s[0] == '0')
   {
      for (i = 1; i < n; i++)
      {
         char ch = s[i];
         if (ch >= '0' && ch <= '7') v = v * 8 + (ch - '0');
         else if (ch == 'u' || ch == 'U') { i++; break; }
         else return false;
      }
   }
   else
   {
      for (i = 0; i < n; i++)
      {
         char ch = s[i];
         if (ch >= '0' && ch <= '9') v = v * 10 + (ch - '0');
         else if (ch == 'u' || ch == 'U') { i++; break; }
         else return false;
      }
   }
   if (i != n)
      return false;
   *out = v;
   return true;
}

/* A number token the preprocessor makes up. */
static bool rslc_pp_make_number(struct rslc_pp *pp, const struct rslc_tok *at,
      long v, struct rslc_tok *out)
{
   char     s[24];
   unsigned k = sizeof(s);
   uint32_t off;
   unsigned long u = v < 0 ? (unsigned long)(-v) : (unsigned long)v;

   do { s[--k] = (char)('0' + u % 10u); u /= 10u; } while (u);
   if (v < 0)
      s[--k] = '-';
   if (!rslc_synth(pp->c, s + k, (uint32_t)(sizeof(s) - k), &off))
      return false;
   *out       = *at;
   out->kind  = RSLC_TK_NUMBER;
   out->ref   = off;
   out->len   = (uint16_t)(sizeof(s) - k);
   out->flags = (uint8_t)((at->flags & (RSLC_TF_SPACE | RSLC_TF_BOL)) | RSLC_TF_SYNTH);
   out->punct = 0;
   return true;
}

/* Pastes two tokens into one: their spellings concatenated and
 * re-lexed.  The result must be a single identifier, number or
 * punctuator. */
static bool rslc_pp_paste(struct rslc_pp *pp, const struct rslc_tok *a,
      const struct rslc_tok *b, struct rslc_tok *out)
{
   const char *as = RSLC_TXT(pp, a);
   const char *bs = RSLC_TXT(pp, b);
   char        buf[256];
   uint32_t    n = (uint32_t)a->len + b->len;
   struct rslc_lex l;
   struct rslc_tok t;
   uint32_t    off;

   if (n >= sizeof(buf))
   {
      rslc_err(pp->c, "pasted token too long", NULL, a->line);
      return false;
   }
   memcpy(buf, as, a->len);
   memcpy(buf + a->len, bs, b->len);
   if (!rslc_synth(pp->c, buf, n, &off))
      return false;
   memset(&l, 0, sizeof(l));
   l.c    = pp->c;
   l.src  = pp->c->synth + off;
   l.len  = n;
   l.line = a->line;
   if (!rslc_lex_next(&l, &t))
      return false;
   if (l.pos != n || t.kind == RSLC_TK_OTHER)
   {
      rslc_err(pp->c, "## does not form a valid token", NULL, a->line);
      return false;
   }
   if (t.kind != RSLC_TK_IDENT)
   {
      t.ref    = off;
      t.flags |= RSLC_TF_SYNTH;
   }
   t.line  = a->line;
   t.file  = a->file;
   t.flags = (uint8_t)((t.flags & RSLC_TF_SYNTH) | (a->flags & (RSLC_TF_SPACE | RSLC_TF_BOL)));
   *out = t;
   return true;
}

static bool rslc_pp_expand_token(struct rslc_pp *pp, const struct rslc_tok *t,
      bool *expanded);

/* Fully macro-expands a token array into another: prescan of a macro
 * argument, and the operand of #if / #line.  Reads only from sources
 * pushed here, never past them. */
static bool rslc_pp_expand_list(struct rslc_pp *pp, const struct rslc_tok *v,
      uint32_t n, struct rslc_tokbuf *out)
{
   uint32_t base = pp->nsrc;

   rslc_tokbuf_init(out, true);
   if (n == 0)
      return true;
   if (!rslc_pp_push_src(pp, v, n, NULL))
      return false;
   for (;;)
   {
      struct rslc_tok t;
      bool expanded;
      while (pp->nsrc > base && pp->srcs[pp->nsrc - 1].pos >= pp->srcs[pp->nsrc - 1].n)
      {
         struct rslc_pp_src *s = &pp->srcs[pp->nsrc - 1];
         if (s->macro)
            s->macro->flags &= (uint16_t)~RSLC_SYM_BUSY;
         pp->nsrc--;
      }
      if (pp->nsrc == base)
         return true;
      t = pp->srcs[pp->nsrc - 1].v[pp->srcs[pp->nsrc - 1].pos++];
      if (t.kind == RSLC_TK_IDENT && !(t.flags & RSLC_TF_NOEXPAND))
      {
         if (!rslc_pp_expand_token(pp, &t, &expanded))
            return false;
         if (expanded)
            continue;
      }
      if (!rslc_tokbuf_push(pp->c, out, &t))
         return false;
   }
}

/* Macro expansion of one identifier token.  Sets *expanded when the
 * token was an invocation whose expansion has been pushed as a
 * source; the caller then reads on.  Function-like macros collect
 * their arguments from the stream, prescan each argument that is not
 * an operand of ##, substitute, then paste. */
static bool rslc_pp_expand_token(struct rslc_pp *pp, const struct rslc_tok *t,
      bool *expanded)
{
   struct rslc_sym   *sym = RSLC_SYM(pp, t);
   struct rslc_macro *m   = sym->macro;
   struct rslc_tok    next;
   struct rslc_tokbuf args[RSLC_PP_MAX_ARGS];
   struct rslc_tokbuf body;
   uint32_t           nargs = 0;
   uint32_t           i;
   uint32_t           line  = t->line;   /* the expansion's line */
   bool               pasting;

   *expanded = false;
   if (pp->nsrc == 0 && !pp->top_marked)
   {
      rslc_scratch_mark(pp->c, &pp->top_mark);
      pp->top_marked = true;
   }

   if (sym == pp->sym_line)
   {
      struct rslc_tok *n = (struct rslc_tok*)rslc_scratch_alloc(pp->c, sizeof(*n));
      if (!n || !rslc_pp_make_number(pp, t, (long)t->line, n))
         return false;
      *expanded = true;
      return rslc_pp_push_src(pp, n, 1, NULL);
   }
   if (!m)
      return true;
   if (sym->flags & RSLC_SYM_BUSY)
      return true;

   if (m->function_like)
   {
      int depth;
      /* Only an invocation when '(' follows; newlines may intervene. */
      for (;;)
      {
         if (!rslc_pp_raw(pp, &next))
            return false;
         if (next.kind != RSLC_TK_NEWLINE)
            break;
      }
      if (!rslc_tok_is_punct(&next, RSLC_P_LPAREN))
      {
         rslc_pp_unread(pp, &next);
         return true;
      }
      for (i = 0; i < RSLC_PP_MAX_ARGS; i++)
         rslc_tokbuf_init(&args[i], true);
      depth = 0;
      for (;;)
      {
         if (!rslc_pp_raw(pp, &next))
            return false;
         if (next.kind == RSLC_TK_EOF)
         {
            rslc_err(pp->c, "unterminated macro invocation of ", m->name->str, t->line);
            return false;
         }
         if (next.kind == RSLC_TK_NEWLINE)
            continue;
         if (rslc_tok_is_punct(&next, RSLC_P_LPAREN))
            depth++;
         else if (rslc_tok_is_punct(&next, RSLC_P_RPAREN))
         {
            if (depth == 0)
            {
               /* glslang places the expansion where the invocation
                * ends. */
               line = next.line;
               break;
            }
            depth--;
         }
         else if (rslc_tok_is_punct(&next, RSLC_P_COMMA) && depth == 0)
         {
            nargs++;
            if (nargs == RSLC_PP_MAX_ARGS)
            {
               rslc_err(pp->c, "too many macro arguments for ", m->name->str, t->line);
               return false;
            }
            continue;
         }
         if (!rslc_tokbuf_push(pp->c, &args[nargs], &next))
            return false;
      }
      if (!(nargs == 0 && args[0].n == 0 && m->nparams == 0))
         nargs++;
      if (nargs != m->nparams)
      {
         rslc_err(pp->c, "wrong number of arguments for macro ", m->name->str, t->line);
         return false;
      }
   }

   /* Substitute into a scratch copy of the body.  An argument that is
    * an operand of ## goes in unexpanded; the others are prescanned. */
   rslc_tokbuf_init(&body, true);
   pasting = false;
   for (i = 0; i < m->nbody; i++)
   {
      const struct rslc_tok *bt = &m->body[i];
      bool paste_next = (i + 1 < m->nbody) && rslc_tok_is_punct(&m->body[i + 1], RSLC_P_PASTE);
      uint32_t p;
      bool     subst = false;

      if (rslc_tok_is_punct(bt, RSLC_P_PASTE))
      {
         pasting = true;
         continue;
      }
      if (bt->kind == RSLC_TK_IDENT && m->function_like)
      {
         struct rslc_sym *bsym = RSLC_SYM(pp, bt);
         for (p = 0; p < m->nparams; p++)
         {
            if (m->params[p] == bsym)
            {
               struct rslc_tokbuf ex;
               uint32_t k;
               if (pasting || paste_next)
                  ex = args[p];
               else if (!rslc_pp_expand_list(pp, args[p].v, args[p].n, &ex))
                  return false;
               for (k = 0; k < ex.n; k++)
               {
                  struct rslc_tok a = ex.v[k];
                  a.line = line;
                  a.file = t->file;
                  if (k == 0)
                     a.flags = (uint8_t)((a.flags & (RSLC_TF_NOEXPAND | RSLC_TF_SYNTH))
                           | (bt->flags & RSLC_TF_SPACE));
                  if (pasting && k == 0 && body.n)
                  {
                     if (!rslc_pp_paste(pp, &body.v[body.n - 1], &a, &body.v[body.n - 1]))
                        return false;
                     continue;
                  }
                  if (!rslc_tokbuf_push(pp->c, &body, &a))
                     return false;
               }
               subst = true;
               break;
            }
         }
      }
      if (!subst)
      {
         struct rslc_tok b = *bt;
         b.line = line;
         b.file = t->file;
         if (i == 0)
            b.flags = (uint8_t)((b.flags & (RSLC_TF_NOEXPAND | RSLC_TF_SYNTH))
                  | (t->flags & (RSLC_TF_SPACE | RSLC_TF_BOL)));
         if (pasting && body.n)
         {
            if (!rslc_pp_paste(pp, &body.v[body.n - 1], &b, &body.v[body.n - 1]))
               return false;
         }
         else if (!rslc_tokbuf_push(pp->c, &body, &b))
            return false;
      }
      pasting = false;
   }
   *expanded = true;
   if (body.n == 0)
      return true;
   return rslc_pp_push_src(pp, body.v, body.n, sym);
}

/* #if expression evaluation over an expanded token list.  C
 * precedence; integers; defined() already resolved. */
struct rslc_pp_expr
{
   struct rslc_pp        *pp;
   const struct rslc_tok *v;
   uint32_t               n;
   uint32_t               pos;
   bool                   bad;
};

static long rslc_pp_cond_expr(struct rslc_pp_expr *e);

static const struct rslc_tok *rslc_pp_expr_peek(struct rslc_pp_expr *e)
{
   static const struct rslc_tok eof = { 0, 0, 0, RSLC_TK_EOF, 0, 0, 0, 0 };
   return e->pos < e->n ? &e->v[e->pos] : &eof;
}

static bool rslc_pp_expr_accept(struct rslc_pp_expr *e, enum rslc_punct p)
{
   if (rslc_tok_is_punct(rslc_pp_expr_peek(e), p))
   {
      e->pos++;
      return true;
   }
   return false;
}

static long rslc_pp_unary(struct rslc_pp_expr *e)
{
   const struct rslc_tok *t = rslc_pp_expr_peek(e);
   long v;
   if (rslc_pp_expr_accept(e, RSLC_P_LPAREN))
   {
      v = rslc_pp_cond_expr(e);
      if (!rslc_pp_expr_accept(e, RSLC_P_RPAREN))
         e->bad = true;
      return v;
   }
   if (rslc_pp_expr_accept(e, RSLC_P_MINUS)) return -rslc_pp_unary(e);
   if (rslc_pp_expr_accept(e, RSLC_P_PLUS))  return rslc_pp_unary(e);
   if (rslc_pp_expr_accept(e, RSLC_P_BANG))  return !rslc_pp_unary(e);
   if (rslc_pp_expr_accept(e, RSLC_P_TILDE)) return ~rslc_pp_unary(e);
   if (t->kind == RSLC_TK_NUMBER)
   {
      e->pos++;
      if (!rslc_pp_number(RSLC_TXT(e->pp, t), t->len, &v))
      {
         e->bad = true;
         return 0;
      }
      return v;
   }
   if (t->kind == RSLC_TK_IDENT)
   {
      /* An identifier that is not a macro evaluates to 0. */
      e->pos++;
      return 0;
   }
   e->bad = true;
   return 0;
}

static long rslc_pp_binary(struct rslc_pp_expr *e, int prec)
{
   long lhs = rslc_pp_unary(e);
   for (;;)
   {
      const struct rslc_tok *t = rslc_pp_expr_peek(e);
      int  p;
      long rhs;
      if (t->kind != RSLC_TK_PUNCT)
         return lhs;
      switch (t->punct)
      {
         case RSLC_P_STAR: case RSLC_P_SLASH: case RSLC_P_PERCENT: p = 10; break;
         case RSLC_P_PLUS: case RSLC_P_MINUS: p = 9; break;
         case RSLC_P_LSHIFT: case RSLC_P_RSHIFT: p = 8; break;
         case RSLC_P_LT: case RSLC_P_GT: case RSLC_P_LE: case RSLC_P_GE: p = 7; break;
         case RSLC_P_EQ: case RSLC_P_NE: p = 6; break;
         case RSLC_P_AMP: p = 5; break;
         case RSLC_P_CARET: p = 4; break;
         case RSLC_P_PIPE: p = 3; break;
         case RSLC_P_AND: p = 2; break;
         case RSLC_P_OR: p = 1; break;
         default: return lhs;
      }
      if (p < prec)
         return lhs;
      e->pos++;
      rhs = rslc_pp_binary(e, p + 1);
      switch (t->punct)
      {
         case RSLC_P_STAR:    lhs = lhs * rhs; break;
         case RSLC_P_SLASH:   if (rhs == 0) { e->bad = true; return 0; } lhs = lhs / rhs; break;
         case RSLC_P_PERCENT: if (rhs == 0) { e->bad = true; return 0; } lhs = lhs % rhs; break;
         case RSLC_P_PLUS:    lhs = lhs + rhs; break;
         case RSLC_P_MINUS:   lhs = lhs - rhs; break;
         case RSLC_P_LSHIFT:  lhs = (long)((unsigned long)lhs << (rhs & 63)); break;
         case RSLC_P_RSHIFT:  lhs = lhs >> (rhs & 63); break;
         case RSLC_P_LT:      lhs = lhs < rhs; break;
         case RSLC_P_GT:      lhs = lhs > rhs; break;
         case RSLC_P_LE:      lhs = lhs <= rhs; break;
         case RSLC_P_GE:      lhs = lhs >= rhs; break;
         case RSLC_P_EQ:      lhs = lhs == rhs; break;
         case RSLC_P_NE:      lhs = lhs != rhs; break;
         case RSLC_P_AMP:     lhs = lhs & rhs; break;
         case RSLC_P_CARET:   lhs = lhs ^ rhs; break;
         case RSLC_P_PIPE:    lhs = lhs | rhs; break;
         case RSLC_P_AND:     lhs = lhs && rhs; break;
         case RSLC_P_OR:      lhs = lhs || rhs; break;
         default: break;
      }
   }
}

static long rslc_pp_cond_expr(struct rslc_pp_expr *e)
{
   long v = rslc_pp_binary(e, 1);
   if (rslc_pp_expr_accept(e, RSLC_P_QUESTION))
   {
      long a = rslc_pp_cond_expr(e);
      long b;
      if (!rslc_pp_expr_accept(e, RSLC_P_COLON))
         e->bad = true;
      b = rslc_pp_cond_expr(e);
      v = v ? a : b;
   }
   return v;
}

/* Evaluates the rest of an #if / #elif line. */
static bool rslc_pp_eval(struct rslc_pp *pp, uint32_t line, bool *result)
{
   struct rslc_tokbuf  raw;
   struct rslc_tokbuf  pre;
   struct rslc_tokbuf  ex;
   struct rslc_pp_expr e;
   struct rslc_mark    mark;
   uint32_t            i;
   bool                ok = false;

   rslc_scratch_mark(pp->c, &mark);
   if (!rslc_pp_read_line(pp, &raw, true))
      return false;

   /* Resolve defined X / defined(X) before expansion. */
   rslc_tokbuf_init(&pre, true);
   for (i = 0; i < raw.n; i++)
   {
      struct rslc_tok t = raw.v[i];
      if (t.kind == RSLC_TK_IDENT && RSLC_SYM(pp, &t) == pp->sym_defined)
      {
         bool paren = false;
         struct rslc_tok r;
         i++;
         if (i < raw.n && rslc_tok_is_punct(&raw.v[i], RSLC_P_LPAREN))
         {
            paren = true;
            i++;
         }
         if (i >= raw.n || raw.v[i].kind != RSLC_TK_IDENT)
         {
            rslc_err(pp->c, "bad defined() in #if", NULL, line);
            return false;
         }
         if (!rslc_pp_make_number(pp, &raw.v[i],
                  RSLC_SYM(pp, &raw.v[i])->macro ? 1 : 0, &r))
            return false;
         if (paren)
         {
            i++;
            if (i >= raw.n || !rslc_tok_is_punct(&raw.v[i], RSLC_P_RPAREN))
            {
               rslc_err(pp->c, "bad defined() in #if", NULL, line);
               return false;
            }
         }
         if (!rslc_tokbuf_push(pp->c, &pre, &r))
            return false;
         continue;
      }
      if (!rslc_tokbuf_push(pp->c, &pre, &t))
         return false;
   }

   if (rslc_pp_expand_list(pp, pre.v, pre.n, &ex))
   {
      e.pp = pp; e.v = ex.v; e.n = ex.n; e.pos = 0; e.bad = false;
      *result = rslc_pp_cond_expr(&e) != 0;
      if (e.bad || e.pos != e.n)
         rslc_err(pp->c, "bad #if expression", NULL, line);
      else
         ok = true;
   }
   if (pp->nsrc == 0)
      rslc_scratch_release(pp->c, &mark);
   return ok;
}

static bool rslc_pp_push_cond(struct rslc_pp *pp, bool take, uint32_t line)
{
   struct rslc_pp_cond *k;
   if (pp->ncond == RSLC_PP_MAX_COND)
   {
      rslc_err(pp->c, "#if nested too deep", NULL, line);
      return false;
   }
   k          = &pp->conds[pp->ncond++];
   k->parent  = pp->ncond == 1 ? true : pp->conds[pp->ncond - 2].active;
   k->active  = k->parent && take;
   k->taken   = k->active;
   k->in_else = false;
   return true;
}

static bool rslc_pp_define(struct rslc_pp *pp, uint32_t line)
{
   struct rslc_tokbuf  l;
   struct rslc_macro  *m;
   uint32_t            i = 0;

   /* A macro body lives for the compile: main arena. */
   if (!rslc_pp_read_line(pp, &l, false))
      return false;
   if (l.n == 0 || l.v[0].kind != RSLC_TK_IDENT)
   {
      rslc_err(pp->c, "#define needs a name", NULL, line);
      return false;
   }
   m = (struct rslc_macro*)rslc_arena_zalloc(pp->c, sizeof(*m));
   if (!m)
      return false;
   m->name = RSLC_SYM(pp, &l.v[0]);
   i = 1;
   if (i < l.n && rslc_tok_is_punct(&l.v[i], RSLC_P_LPAREN)
         && !(l.v[i].flags & RSLC_TF_SPACE))
   {
      m->function_like = true;
      m->params = (struct rslc_sym**)rslc_arena_alloc(pp->c,
            RSLC_PP_MAX_ARGS * sizeof(*m->params));
      if (!m->params)
         return false;
      i++;
      if (i < l.n && rslc_tok_is_punct(&l.v[i], RSLC_P_RPAREN))
         i++;
      else
      {
         for (;;)
         {
            if (i >= l.n || l.v[i].kind != RSLC_TK_IDENT
                  || m->nparams == RSLC_PP_MAX_ARGS)
            {
               rslc_err(pp->c, "bad parameter list for macro ", m->name->str, line);
               return false;
            }
            m->params[m->nparams++] = RSLC_SYM(pp, &l.v[i]);
            i++;
            if (i < l.n && rslc_tok_is_punct(&l.v[i], RSLC_P_COMMA))
            {
               i++;
               continue;
            }
            if (i < l.n && rslc_tok_is_punct(&l.v[i], RSLC_P_RPAREN))
            {
               i++;
               break;
            }
            rslc_err(pp->c, "bad parameter list for macro ", m->name->str, line);
            return false;
         }
      }
   }
   m->body  = l.v + i;
   m->nbody = l.n - i;
   {
      uint32_t k;
      for (k = 0; k < m->nbody; k++)
      {
         const struct rslc_tok *bt = &m->body[k];
         if (rslc_tok_is_punct(bt, RSLC_P_HASH))
         {
            rslc_err(pp->c, "# in macro body is not supported: ", m->name->str, line);
            return false;
         }
         if (rslc_tok_is_punct(bt, RSLC_P_PASTE) && (k == 0 || k + 1 == m->nbody))
         {
            rslc_err(pp->c, "## at the edge of macro body: ", m->name->str, line);
            return false;
         }
      }
      if (m->nbody)
         m->body[0].flags &= (uint8_t)~RSLC_TF_BOL;
   }
   m->name->macro = m;
   return true;
}

static bool rslc_pp_version(struct rslc_pp *pp, uint32_t line)
{
   struct rslc_tokbuf l;
   struct rslc_mark   mark;
   long               v;
   bool               ok = false;

   rslc_scratch_mark(pp->c, &mark);
   if (!rslc_pp_read_line(pp, &l, true))
      return false;
   if (pp->seen_version || pp->seen_token)
      rslc_err(pp->c, "#version must be the first line", NULL, line);
   else if (l.n < 1 || l.v[0].kind != RSLC_TK_NUMBER
         || !rslc_pp_number(RSLC_TXT(pp, &l.v[0]), l.v[0].len, &v))
      rslc_err(pp->c, "bad #version", NULL, line);
   else
   {
      pp->c->version = (uint32_t)v;
      pp->c->es      = false;
      ok = true;
      if (l.n >= 2 && l.v[1].kind == RSLC_TK_IDENT)
      {
         const char *prof = RSLC_TXT(pp, &l.v[1]);
         if (l.v[1].len == 2 && !memcmp(prof, "es", 2))
            pp->c->es = true;
         else if (!((l.v[1].len == 4 && !memcmp(prof, "core", 4))
                  || (l.v[1].len == 13 && !memcmp(prof, "compatibility", 13))))
         {
            rslc_err(pp->c, "bad #version profile", NULL, line);
            ok = false;
         }
      }
      if (ok && pp->c->version != 450 && pp->c->version != 460
            && !(pp->c->es && (pp->c->version == 310 || pp->c->version == 320)))
      {
         rslc_err(pp->c, "unsupported #version", NULL, line);
         ok = false;
      }
   }
   if (ok)
      pp->seen_version = true;
   rslc_scratch_release(pp->c, &mark);
   return ok;
}

static bool rslc_pp_line_directive(struct rslc_pp *pp, uint32_t line)
{
   struct rslc_tokbuf raw;
   struct rslc_tokbuf ex;
   struct rslc_mark   mark;
   long               n;
   uint8_t            file = pp->lex.file;
   bool               ok   = false;

   rslc_scratch_mark(pp->c, &mark);
   if (!rslc_pp_read_line(pp, &raw, true))
      return false;
   if (!rslc_pp_expand_list(pp, raw.v, raw.n, &ex))
      return false;
   if (ex.n < 1 || ex.v[0].kind != RSLC_TK_NUMBER
         || !rslc_pp_number(RSLC_TXT(pp, &ex.v[0]), ex.v[0].len, &n))
      rslc_err(pp->c, "bad #line", NULL, line);
   else if (ex.n >= 2 && ex.v[1].kind == RSLC_TK_STRING)
   {
      const char *s    = RSLC_TXT(pp, &ex.v[1]);
      const char *name = rslc_intern(pp->c, s + 1,
            ex.v[1].len >= 2 ? ex.v[1].len - 2 : 0);
      uint32_t    i;
      if (!name)
         return false;
      for (i = 0; i < pp->c->nfiles; i++)
         if (pp->c->files[i] == name)
            break;
      if (i == pp->c->nfiles)
      {
         if (i == RSLC_MAX_FILES)
            i = RSLC_MAX_FILES - 1;
         else
            pp->c->files[pp->c->nfiles++] = name;
      }
      file = (uint8_t)i;
      ok   = true;
   }
   else if (ex.n >= 2 && ex.v[1].kind == RSLC_TK_NUMBER)
   {
      long f;
      if (rslc_pp_number(RSLC_TXT(pp, &ex.v[1]), ex.v[1].len, &f))
      {
         file = (uint8_t)f;
         ok   = true;
      }
      else
         rslc_err(pp->c, "bad #line", NULL, line);
   }
   else if (ex.n == 1)
      ok = true;
   else
      rslc_err(pp->c, "bad #line", NULL, line);

   if (ok)
   {
      /* The line after the directive is n. */
      pp->lex.line = (uint32_t)n;
      pp->lex.file = file;
   }
   if (pp->nsrc == 0)
      rslc_scratch_release(pp->c, &mark);
   return ok;
}

static bool rslc_pp_directive(struct rslc_pp *pp, const struct rslc_tok *hash)
{
   struct rslc_tok  t;
   uint32_t         line   = hash->line;
   bool             active = rslc_pp_active(pp);
   const char      *d;
   uint32_t         dl;

   if (!rslc_pp_raw(pp, &t))
      return false;
   /* In an inactive branch glslang skips further '#'s before the
    * directive name ("##else"); in an active one that is an error. */
   while (!active && (rslc_tok_is_punct(&t, RSLC_P_HASH) || rslc_tok_is_punct(&t, RSLC_P_PASTE)))
      if (!rslc_pp_raw(pp, &t))
         return false;
   if (rslc_tok_is_eol(&t))
      return true;                       /* a lone '#' */
   if (t.kind != RSLC_TK_IDENT)
   {
      if (!active)
         return rslc_pp_skip_line(pp);
      rslc_err(pp->c, "bad directive", NULL, line);
      return false;
   }
   d  = RSLC_TXT(pp, &t);
   dl = t.len;

#define RSLC_DIR(name) (dl == sizeof(name) - 1 && !memcmp(d, name, dl))

   /* Conditionals are tracked in inactive branches too. */
   if (RSLC_DIR("ifdef") || RSLC_DIR("ifndef"))
   {
      bool neg  = d[2] == 'n';
      bool take = false;
      if (active)
      {
         if (!rslc_pp_raw(pp, &t))
            return false;
         if (t.kind != RSLC_TK_IDENT)
         {
            rslc_err(pp->c, neg ? "#ifndef needs a name" : "#ifdef needs a name", NULL, line);
            return false;
         }
         take = (RSLC_SYM(pp, &t)->macro != NULL) != neg;
      }
      if (!rslc_pp_skip_line(pp))
         return false;
      return rslc_pp_push_cond(pp, take, line);
   }
   if (RSLC_DIR("if"))
   {
      bool take = false;
      if (active)
      {
         if (!rslc_pp_eval(pp, line, &take))
            return false;
      }
      else if (!rslc_pp_skip_line(pp))
         return false;
      return rslc_pp_push_cond(pp, take, line);
   }
   if (RSLC_DIR("elif"))
   {
      struct rslc_pp_cond *k;
      bool take = false;
      if (!pp->ncond)
      {
         rslc_err(pp->c, "#elif without #if", NULL, line);
         return false;
      }
      k = &pp->conds[pp->ncond - 1];
      if (k->in_else)
      {
         rslc_err(pp->c, "#elif after #else", NULL, line);
         return false;
      }
      if (k->parent && !k->taken)
      {
         if (!rslc_pp_eval(pp, line, &take))
            return false;
      }
      else if (!rslc_pp_skip_line(pp))
         return false;
      k->active = k->parent && !k->taken && take;
      if (k->active)
         k->taken = true;
      return true;
   }
   if (RSLC_DIR("else"))
   {
      struct rslc_pp_cond *k;
      if (!pp->ncond)
      {
         rslc_err(pp->c, "#else without #if", NULL, line);
         return false;
      }
      k = &pp->conds[pp->ncond - 1];
      if (k->in_else)
      {
         rslc_err(pp->c, "#else after #else", NULL, line);
         return false;
      }
      k->in_else = true;
      k->active  = k->parent && !k->taken;
      k->taken   = true;
      return rslc_pp_skip_line(pp);
   }
   if (RSLC_DIR("endif"))
   {
      if (!pp->ncond)
      {
         rslc_err(pp->c, "#endif without #if", NULL, line);
         return false;
      }
      pp->ncond--;
      return rslc_pp_skip_line(pp);
   }

   if (!active)
      return rslc_pp_skip_line(pp);

   if (RSLC_DIR("define"))
      return rslc_pp_define(pp, line);
   if (RSLC_DIR("undef"))
   {
      if (!rslc_pp_raw(pp, &t))
         return false;
      if (t.kind != RSLC_TK_IDENT)
      {
         rslc_err(pp->c, "#undef needs a name", NULL, line);
         return false;
      }
      RSLC_SYM(pp, &t)->macro = NULL;
      return rslc_pp_skip_line(pp);
   }
   if (RSLC_DIR("version"))
      return rslc_pp_version(pp, line);
   if (RSLC_DIR("extension"))
   {
      /* Any name with any behaviour is accepted; a construct the
       * extension would enable is refused where it appears, if it is
       * outside the dialect. */
      return rslc_pp_skip_line(pp);
   }
   if (RSLC_DIR("line"))
      return rslc_pp_line_directive(pp, line);
   if (RSLC_DIR("pragma"))
      return rslc_pp_skip_line(pp);
   if (RSLC_DIR("error"))
   {
      /* The message is the rest of the source line, as written. */
      size_t      start = pp->lex.pos;
      size_t      end;
      const char *msg;
      if (!rslc_pp_skip_line(pp))
         return false;
      end = pp->lex.pos;
      while (end > start && (pp->lex.src[end - 1] == '\n' || pp->lex.src[end - 1] == '\r'
               || pp->lex.src[end - 1] == ' ' || pp->lex.src[end - 1] == '\t'))
         end--;
      while (start < end && (pp->lex.src[start] == ' ' || pp->lex.src[start] == '\t'))
         start++;
      msg = rslc_intern(pp->c, pp->lex.src + start, end - start);
      if (!msg)
         return false;
      rslc_err(pp->c, "#error ", msg, line);
      return false;
   }
   if (RSLC_DIR("include"))
   {
      rslc_err(pp->c, "#include reaches the compiler: expand includes first", NULL, line);
      return false;
   }
   rslc_err(pp->c, "unknown directive #", rslc_intern(pp->c, d, dl), line);
   return false;
#undef RSLC_DIR
}

/* Seeds a predefined macro glslang provides under Vulkan rules. */
static bool rslc_pp_predefine(struct rslc_pp *pp, const char *name,
      const char *value)
{
   struct rslc_macro *m = (struct rslc_macro*)rslc_arena_zalloc(pp->c, sizeof(*m));
   struct rslc_tok   *b = (struct rslc_tok*)rslc_arena_zalloc(pp->c, sizeof(*b));
   uint32_t           off;
   if (!m || !b)
      return false;
   m->name = rslc_sym(pp->c, name, strlen(name));
   if (!m->name)
      return false;
   if (!rslc_synth(pp->c, value, (uint32_t)strlen(value), &off))
      return false;
   b->kind  = RSLC_TK_NUMBER;
   b->ref   = off;
   b->len   = (uint16_t)strlen(value);
   b->flags = RSLC_TF_SYNTH;
   m->body  = b;
   m->nbody = 1;
   m->name->macro = m;
   return true;
}

bool rslc_pp_run(struct rslc_ctx *c, const char *src, size_t len)
{
   struct rslc_pp *pp = (struct rslc_pp*)rslc_arena_zalloc(c, sizeof(*pp));
   struct rslc_tok t;

   if (!pp)
      return false;
   if (len > 0xffffffffu)
   {
      rslc_err(c, "source too large", NULL, 0);
      return false;
   }
   c->src       = src;
   c->files[0]  = "";
   c->nfiles    = 1;
   pp->c        = c;
   pp->lex.c    = c;
   pp->lex.src  = src;
   pp->lex.len  = len;
   pp->lex.line = 1;
   pp->lex.bol  = true;

   pp->sym_defined = rslc_sym(c, "defined", 7);
   pp->sym_line    = rslc_sym(c, "__LINE__", 8);
   if (!pp->sym_defined || !pp->sym_line)
      return false;
   if (     !rslc_pp_predefine(pp, "__VERSION__", "450")
         || !rslc_pp_predefine(pp, "GL_core_profile", "1")
         || !rslc_pp_predefine(pp, "VULKAN", "100"))
      return false;

   for (;;)
   {
      bool expanded;
      rslc_pp_settle(pp);
      if (!rslc_pp_raw(pp, &t))
         return false;
      if (t.kind == RSLC_TK_EOF)
         break;
      if (t.kind == RSLC_TK_NEWLINE)
         continue;
      if ((t.flags & RSLC_TF_BOL) && pp->nsrc == 0
            && (rslc_tok_is_punct(&t, RSLC_P_HASH) || rslc_tok_is_punct(&t, RSLC_P_PASTE)))
      {
         if (rslc_tok_is_punct(&t, RSLC_P_PASTE))
         {
            /* "##else": a second '#' before the directive name. */
            struct rslc_tok h = t;
            h.punct = RSLC_P_HASH;
            h.len   = 1;
            h.flags = (uint8_t)(t.flags & ~RSLC_TF_BOL);
            rslc_pp_unread(pp, &h);
         }
         if (!rslc_pp_directive(pp, &t))
            return false;
         continue;
      }
      if (!rslc_pp_active(pp))
         continue;
      if (!pp->seen_version)
      {
         rslc_err(c, "first line must be #version", NULL, t.line);
         return false;
      }
      pp->seen_token = true;
      if (t.kind == RSLC_TK_IDENT && !(t.flags & RSLC_TF_NOEXPAND))
      {
         if (!rslc_pp_expand_token(pp, &t, &expanded))
            return false;
         if (expanded)
            continue;
      }
      if (t.kind == RSLC_TK_OTHER)
      {
         rslc_err(c, "unexpected character", NULL, t.line);
         return false;
      }
      if (t.kind == RSLC_TK_STRING)
      {
         rslc_err(c, "string literal outside #line", NULL, t.line);
         return false;
      }
      if (!rslc_out_push(c, &t))
         return false;
   }
   if (pp->ncond)
   {
      rslc_err(c, "unterminated #if", NULL, t.line);
      return false;
   }
   if (!pp->seen_version)
   {
      rslc_err(c, "first line must be #version", NULL, 1);
      return false;
   }
   memset(&t, 0, sizeof(t));
   t.kind = RSLC_TK_EOF;
   t.line = pp->lex.line;
   return rslc_out_push(c, &t);
}

/* ------------------------------------------------------------------ */
/* Parser and typecheck                                                */
/* ------------------------------------------------------------------ */

/* Declarations are bound straight on the symbol: sym->decl is the
 * innermost visible one and each declaration keeps what it shadowed,
 * so lookup is a pointer read and leaving a scope restores its
 * declarations in order. */
enum rslc_dkind
{
   RSLC_DK_VAR = 1,
   RSLC_DK_FUNC,     /* ptr = first struct rslc_func of the overload chain */
   RSLC_DK_TYPE      /* ptr = struct rslc_type (a struct) */
};

struct rslc_decl
{
   struct rslc_sym  *sym;
   struct rslc_decl *shadow;
   struct rslc_decl *next;    /* within the scope */
   void             *ptr;
   uint8_t           kind;
   uint8_t           pad[7];
};

struct rslc_scope
{
   struct rslc_decl  *decls;
   struct rslc_scope *parent;
   uint8_t            prec_float;  /* default precision in this scope */
   uint8_t            prec_int;
   uint8_t            pad[6];
};

/* Qualifiers collected ahead of a declaration. */
struct rslc_quals
{
   uint32_t location;
   uint32_t binding;
   uint32_t set;
   uint32_t offset;
   uint8_t  storage;      /* RSLC_ST_* chosen from in/out/uniform/const; LOCAL if none */
   uint8_t  dir;          /* params: in/out/inout */
   uint8_t  interp;
   uint8_t  precision;
   uint8_t  vq;           /* RSLC_VQ_* */
   bool     has_storage;
   bool     push_constant;
   bool     std140;
   bool     is_uniform;
   bool     is_const;
   bool     is_in;
   bool     is_out;
   bool     is_inout;
};

struct rslc_parser
{
   struct rslc_ctx        *c;
   struct rslc_scope      *scope;
   struct rslc_func       *cur_fn;
   const struct rslc_type *t_void;
   const struct rslc_type *t_bool;
   const struct rslc_type *t_int;
   const struct rslc_type *t_uint;
   const struct rslc_type *t_float;
   const struct rslc_type *t_s2d;
   uint32_t                pos;
   uint32_t                loop_depth;
   uint32_t                switch_depth;
   uint32_t                var_ids;
   uint32_t                func_ids;
};

#define P_TOK(p)      rslc_tok_at((p)->c, (p)->pos)
#define P_PEEK(p, k)  rslc_tok_at((p)->c, (p)->pos + (k))
#define P_LINE(p)     (P_TOK(p)->line)

static bool rslc_p_fail(struct rslc_parser *p, const char *msg, const char *what)
{
   p->c->err_file = P_TOK(p)->file;
   rslc_err(p->c, msg, what, P_LINE(p));
   return false;
}

/* The current token's spelling, for messages. */
static const char *rslc_p_here(struct rslc_parser *p)
{
   const struct rslc_tok *t = P_TOK(p);
   if (t->kind == RSLC_TK_EOF)
      return "end of source";
   return rslc_intern(p->c, rslc_tok_text(p->c, t), t->len);
}

static bool rslc_p_is(struct rslc_parser *p, enum rslc_punct pc)
{
   return rslc_tok_is_punct(P_TOK(p), pc);
}

static bool rslc_p_is_kw(struct rslc_parser *p, enum rslc_kw kw)
{
   const struct rslc_tok *t = P_TOK(p);
   return t->kind == RSLC_TK_IDENT && rslc_tok_sym(p->c, t)->kw == kw;
}

static void rslc_p_next(struct rslc_parser *p)
{
   if (P_TOK(p)->kind != RSLC_TK_EOF)
      p->pos++;
}

static bool rslc_p_accept(struct rslc_parser *p, enum rslc_punct pc)
{
   if (rslc_p_is(p, pc))
   {
      rslc_p_next(p);
      return true;
   }
   return false;
}

static bool rslc_p_accept_kw(struct rslc_parser *p, enum rslc_kw kw)
{
   if (rslc_p_is_kw(p, kw))
   {
      rslc_p_next(p);
      return true;
   }
   return false;
}

static bool rslc_p_expect(struct rslc_parser *p, enum rslc_punct pc, const char *what)
{
   char msg[96];
   const char *here;
   size_t n;
   if (rslc_p_accept(p, pc))
      return true;
   here = rslc_p_here(p);
   n = strlen(what);
   if (n + 8 >= sizeof(msg))
      return rslc_p_fail(p, "expected ", what);
   memcpy(msg, "expected ", 9);
   memcpy(msg + 9, what, n);
   memcpy(msg + 9 + n, ", got ", 7);
   return rslc_p_fail(p, rslc_intern(p->c, msg, 9 + n + 6), here);
}

/* ---- scopes ---- */

static bool rslc_p_push_scope(struct rslc_parser *p)
{
   struct rslc_scope *s = (struct rslc_scope*)rslc_arena_zalloc(p->c, sizeof(*s));
   if (!s)
      return false;
   s->parent = p->scope;
   if (p->scope)
   {
      s->prec_float = p->scope->prec_float;
      s->prec_int   = p->scope->prec_int;
   }
   p->scope = s;
   return true;
}

static void rslc_p_pop_scope(struct rslc_parser *p)
{
   struct rslc_scope *s = p->scope;
   struct rslc_decl  *d;
   for (d = s->decls; d; d = d->next)
      d->sym->decl = d->shadow;
   p->scope = s->parent;
}

static struct rslc_decl *rslc_p_bind(struct rslc_parser *p, struct rslc_sym *sym,
      enum rslc_dkind kind, void *ptr)
{
   struct rslc_decl *d = (struct rslc_decl*)rslc_arena_zalloc(p->c, sizeof(*d));
   if (!d)
      return NULL;
   d->sym    = sym;
   d->kind   = (uint8_t)kind;
   d->ptr    = ptr;
   d->shadow = sym->decl;
   d->next   = p->scope->decls;
   p->scope->decls = d;
   sym->decl = d;
   return d;
}

/* Is sym already declared in the current scope (not an outer one)? */
static struct rslc_decl *rslc_p_find_here(struct rslc_parser *p, struct rslc_sym *sym)
{
   struct rslc_decl *d;
   for (d = p->scope->decls; d; d = d->next)
      if (d->sym == sym)
         return d;
   return NULL;
}

/* ---- program tables ---- */

/* fail: what the function returns when the arena is out */
#define RSLC_GROW_PTRS(c, arr, n, cap, T, fail) \
   do { \
      if ((n) == (cap)) \
      { \
         uint32_t ncap_ = (cap) ? (cap) * 2 : 16; \
         T **nv_ = (T**)rslc_arena_alloc((c), ncap_ * sizeof(*nv_)); \
         if (!nv_) return fail; \
         if (n) memcpy(nv_, (arr), (n) * sizeof(*nv_)); \
         (arr) = nv_; (cap) = ncap_; \
      } \
   } while (0)

static struct rslc_var *rslc_p_new_var(struct rslc_parser *p, struct rslc_sym *name,
      const struct rslc_type *type, uint32_t line)
{
   struct rslc_prog *pr = &p->c->prog;
   struct rslc_var  *v  = (struct rslc_var*)rslc_arena_zalloc(p->c, sizeof(*v));
   if (!v)
      return NULL;
   if (pr->nvars == 0)
   {
      RSLC_GROW_PTRS(p->c, pr->vars, pr->nvars, pr->vars_cap, struct rslc_var, NULL);
      pr->vars[0] = NULL;
      pr->nvars   = 1;   /* id 0 is "no var" */
   }
   RSLC_GROW_PTRS(p->c, pr->vars, pr->nvars, pr->vars_cap, struct rslc_var, NULL);
   v->name     = name;
   v->type     = type;
   v->line     = line;
   v->id       = pr->nvars;
   v->location = v->binding = v->set = RSLC_UNSET;
   v->global   = p->scope && !p->scope->parent;
   pr->vars[pr->nvars++] = v;
   return v;
}

static struct rslc_func *rslc_p_new_func(struct rslc_parser *p, struct rslc_sym *name,
      const struct rslc_type *ret, uint32_t line)
{
   struct rslc_prog *pr = &p->c->prog;
   struct rslc_func *f  = (struct rslc_func*)rslc_arena_zalloc(p->c, sizeof(*f));
   if (!f)
      return NULL;
   if (pr->nfuncs == 0)
   {
      RSLC_GROW_PTRS(p->c, pr->funcs, pr->nfuncs, pr->funcs_cap, struct rslc_func, NULL);
      pr->funcs[0] = NULL;
      pr->nfuncs   = 1;
   }
   RSLC_GROW_PTRS(p->c, pr->funcs, pr->nfuncs, pr->funcs_cap, struct rslc_func, NULL);
   f->name = name;
   f->ret  = ret;
   f->line = line;
   f->id   = pr->nfuncs;
   pr->funcs[pr->nfuncs++] = f;
   return f;
}

/* ---- type helpers ---- */

static bool rslc_ty_is_numeric(const struct rslc_type *t)
{
   return (t->kind == RSLC_TY_SCALAR || t->kind == RSLC_TY_VEC || t->kind == RSLC_TY_MAT)
      && t->base != RSLC_BASE_BOOL;
}

static bool rslc_ty_is_integer(const struct rslc_type *t)
{
   return (t->kind == RSLC_TY_SCALAR || t->kind == RSLC_TY_VEC)
      && (t->base == RSLC_BASE_INT || t->base == RSLC_BASE_UINT);
}

unsigned rslc_ty_components(const struct rslc_type *t)
{
   switch (t->kind)
   {
      case RSLC_TY_SCALAR: return 1;
      case RSLC_TY_VEC:    return t->rows;
      case RSLC_TY_MAT:    return (unsigned)t->rows * t->cols;
      default:             return 0;
   }
}

/* scalar / vector of the same base and width n (1 = scalar). */
static const struct rslc_type *rslc_ty_vecn(struct rslc_parser *p,
      enum rslc_base base, unsigned n)
{
   if (n == 1)
      return rslc_type_basic(p->c, RSLC_TY_SCALAR, base, 1, 1);
   return rslc_type_basic(p->c, RSLC_TY_VEC, base, n, 1);
}

static const struct rslc_type *rslc_ty_mat(struct rslc_parser *p, unsigned cols,
      unsigned rows)
{
   return rslc_type_basic(p->c, RSLC_TY_MAT, RSLC_BASE_FLOAT, rows, cols);
}

/* The scalar type of a scalar/vector/matrix. */
static const struct rslc_type *rslc_ty_scalar_of(struct rslc_parser *p,
      const struct rslc_type *t)
{
   return rslc_type_basic(p->c, RSLC_TY_SCALAR, t->base, 1, 1);
}

/* Implicit conversion rank: int -> uint -> float.  Returns true when
 * from converts to to implicitly (same shape, base widening). */
static bool rslc_ty_converts(const struct rslc_type *from, const struct rslc_type *to)
{
   if (from == to)
      return true;
   if (from->kind != to->kind || from->rows != to->rows || from->cols != to->cols)
      return false;
   if (from->kind != RSLC_TY_SCALAR && from->kind != RSLC_TY_VEC)
      return false;
   if (to->base == RSLC_BASE_FLOAT)
      return from->base == RSLC_BASE_INT || from->base == RSLC_BASE_UINT;
   if (to->base == RSLC_BASE_UINT)
      return from->base == RSLC_BASE_INT;
   return false;
}

/* ---- nodes ---- */

static struct rslc_node *rslc_p_node(struct rslc_parser *p, uint32_t id)
{
   return rslc_node_at(p->c, id);
}

static uint32_t rslc_p_mk(struct rslc_parser *p, enum rslc_op op,
      const struct rslc_type *type, uint32_t a, uint32_t b, uint32_t cc,
      uint16_t aux, uint32_t line, uint8_t prec)
{
   uint32_t id = rslc_node_new(p->c, op, type, a, b, cc, aux, line);
   if (id)
      rslc_p_node(p, id)->prec = prec;
   return id;
}

static uint8_t rslc_prec_max(uint8_t a, uint8_t b)
{
   return a > b ? a : b;
}

static uint32_t rslc_p_const_int(struct rslc_parser *p, int32_t v, uint32_t line)
{
   return rslc_p_mk(p, RSLC_OP_CONST, p->t_int, (uint32_t)v, 0, 0, 0, line, 0);
}

static uint32_t rslc_p_const_uint(struct rslc_parser *p, uint32_t v, uint32_t line)
{
   return rslc_p_mk(p, RSLC_OP_CONST, p->t_uint, v, 0, 0, 0, line, 0);
}

static uint32_t rslc_p_const_bool(struct rslc_parser *p, bool v, uint32_t line)
{
   return rslc_p_mk(p, RSLC_OP_CONST, p->t_bool, v ? 1u : 0u, 0, 0, 0, line, 0);
}

static uint32_t rslc_float_bits(float f)
{
   uint32_t u;
   memcpy(&u, &f, sizeof(u));
   return u;
}

/* Float constants keep their double value in b / c beside the float
 * bits in a: glslang folds constant expressions in double and rounds
 * once at emission, so (0.587/0.299) must be 1.9632107, not the float
 * quotient 1.96321082, for the module to be the same module. */
static uint32_t rslc_p_const_float(struct rslc_parser *p, double v, uint32_t line)
{
   uint64_t bits;
   memcpy(&bits, &v, sizeof(bits));
   return rslc_p_mk(p, RSLC_OP_CONST, p->t_float, rslc_float_bits((float)v),
         (uint32_t)bits, (uint32_t)(bits >> 32), 0, line, 0);
}

static double rslc_p_const_double(const struct rslc_node *n)
{
   uint64_t bits = ((uint64_t)n->c << 32) | n->b;
   double   d;
   memcpy(&d, &bits, sizeof(d));
   return d;
}

/* Wraps e in an implicit conversion to type when needed. */
static uint32_t rslc_p_convert(struct rslc_parser *p, uint32_t e,
      const struct rslc_type *to)
{
   struct rslc_node *n = rslc_p_node(p, e);
   if (n->type == to)
      return e;
   if (n->op == RSLC_OP_CONST && n->type->kind == RSLC_TY_SCALAR && to->kind == RSLC_TY_SCALAR)
   {
      /* Fold a scalar literal. */
      if (to->base == RSLC_BASE_FLOAT)
      {
         double f = n->type->base == RSLC_BASE_INT ? (double)(int32_t)n->a : (double)n->a;
         return rslc_p_const_float(p, f, n->line);
      }
      if (to->base == RSLC_BASE_UINT)
         return rslc_p_const_uint(p, n->a, n->line);
   }
   return rslc_p_mk(p, RSLC_OP_CONVERT, to, e, 0, 0, 0, n->line, n->prec);
}

/* Converts e to type or fails. */
static bool rslc_p_coerce(struct rslc_parser *p, uint32_t *e, const struct rslc_type *to,
      const char *what)
{
   const struct rslc_type *from = rslc_p_node(p, *e)->type;
   if (from == to)
      return true;
   if (!rslc_ty_converts(from, to))
   {
      rslc_err(p->c, "type mismatch in ", what, rslc_p_node(p, *e)->line);
      return false;
   }
   *e = rslc_p_convert(p, *e, to);
   return *e != 0;
}

/* ---- constant folding ---- */

static bool rslc_p_nonconstant(struct rslc_parser *p, uint32_t e)
{
   while (e)
   {
      const struct rslc_node *n = rslc_p_node(p, e);
      if (n->nonconstant)
         return true;
      switch (n->op)
      {
         case RSLC_OP_VAR:
            return p->c->prog.vars[n->a]->storage != RSLC_ST_CONST;
         case RSLC_OP_UNARY:
         case RSLC_OP_CONVERT:
         case RSLC_OP_SWIZZLE:
            e = n->a;
            break;
         default:
            return false;
      }
   }
   return false;
}

static bool rslc_p_eval(struct rslc_parser *p, uint32_t e, struct rslc_cval *out);

bool rslc_const_eval(struct rslc_ctx *c, uint32_t node, struct rslc_cval *out)
{
   struct rslc_parser p;
   memset(&p, 0, sizeof(p));
   p.c = c;
   return rslc_p_eval(&p, node, out);
}

static bool rslc_p_eval_int(struct rslc_parser *p, uint32_t e, int32_t *out)
{
   struct rslc_cval v;
   if (!rslc_p_eval(p, e, &v))
      return false;
   if (v.type->kind != RSLC_TY_SCALAR || (v.type->base != RSLC_BASE_INT && v.type->base != RSLC_BASE_UINT))
      return false;
   *out = (int32_t)v.v[0];
   return true;
}

/* Float folding runs in double, as glslang's does; *d receives the
 * double result and the return is its float bits (or the bool). */
static uint32_t rslc_fold_scalar(enum rslc_base base, uint16_t op, uint32_t a, uint32_t b,
      double x, double y, double *d, bool *ok)
{
   if (base == RSLC_BASE_FLOAT)
   {
      double r = 0;
      switch (op)
      {
         case RSLC_BIN_ADD: r = x + y; break;
         case RSLC_BIN_SUB: r = x - y; break;
         case RSLC_BIN_MUL: r = x * y; break;
         case RSLC_BIN_DIV: r = y != 0.0 ? x / y : 0.0; break;
         case RSLC_BIN_LT:  return x < y;
         case RSLC_BIN_GT:  return x > y;
         case RSLC_BIN_LE:  return x <= y;
         case RSLC_BIN_GE:  return x >= y;
         case RSLC_BIN_EQ:  return x == y;
         case RSLC_BIN_NE:  return x != y;
         default: *ok = false; return 0;
      }
      *d = r;
      return rslc_float_bits((float)r);
   }
   if (base == RSLC_BASE_BOOL)
   {
      switch (op)
      {
         case RSLC_BIN_AND: return a && b;
         case RSLC_BIN_OR:  return a || b;
         case RSLC_BIN_XOR: return (a != 0) != (b != 0);
         case RSLC_BIN_EQ:  return a == b;
         case RSLC_BIN_NE:  return a != b;
         default: *ok = false; return 0;
      }
   }
   if (base == RSLC_BASE_INT)
   {
      int32_t x = (int32_t)a, y = (int32_t)b;
      switch (op)
      {
         case RSLC_BIN_ADD: return (uint32_t)(x + y);
         case RSLC_BIN_SUB: return (uint32_t)(x - y);
         case RSLC_BIN_MUL: return (uint32_t)(x * y);
         case RSLC_BIN_DIV: return y ? (uint32_t)(x / y) : 0;
         case RSLC_BIN_MOD: return y ? (uint32_t)(x % y) : 0;
         case RSLC_BIN_LSHIFT: return a << (b & 31);
         case RSLC_BIN_RSHIFT: return (uint32_t)(x >> (b & 31));
         case RSLC_BIN_LT:  return x < y;
         case RSLC_BIN_GT:  return x > y;
         case RSLC_BIN_LE:  return x <= y;
         case RSLC_BIN_GE:  return x >= y;
         case RSLC_BIN_EQ:  return x == y;
         case RSLC_BIN_NE:  return x != y;
         case RSLC_BIN_BAND: return a & b;
         case RSLC_BIN_BOR:  return a | b;
         case RSLC_BIN_BXOR: return a ^ b;
         default: *ok = false; return 0;
      }
   }
   switch (op)
   {
      case RSLC_BIN_ADD: return a + b;
      case RSLC_BIN_SUB: return a - b;
      case RSLC_BIN_MUL: return a * b;
      case RSLC_BIN_DIV: return b ? a / b : 0;
      case RSLC_BIN_MOD: return b ? a % b : 0;
      case RSLC_BIN_LSHIFT: return a << (b & 31);
      case RSLC_BIN_RSHIFT: return a >> (b & 31);
      case RSLC_BIN_LT:  return a < b;
      case RSLC_BIN_GT:  return a > b;
      case RSLC_BIN_LE:  return a <= b;
      case RSLC_BIN_GE:  return a >= b;
      case RSLC_BIN_EQ:  return a == b;
      case RSLC_BIN_NE:  return a != b;
      case RSLC_BIN_BAND: return a & b;
      case RSLC_BIN_BOR:  return a | b;
      case RSLC_BIN_BXOR: return a ^ b;
      default: *ok = false; return 0;
   }
}

/* ---- builtin folding ----
 *
 * glslang folds a builtin call whose arguments are all constant, in
 * double, by the GLSL definitions; a module that leaves such a call
 * to the GPU returns different bits, so the same calls fold here.
 * C89 libm lacks a few of the functions, which are written out. */

static double rslc_fexp2(double x) { return pow(2.0, x); }
static double rslc_flog2(double x) { return log(x) / log(2.0); }
static double rslc_ftrunc(double x) { return x < 0.0 ? ceil(x) : floor(x); }
static double rslc_fround(double x) { return x < 0.0 ? ceil(x - 0.5) : floor(x + 0.5); }
static double rslc_fasinh(double x) { return log(x + sqrt(x * x + 1.0)); }
static double rslc_facosh(double x) { return log(x + sqrt(x * x - 1.0)); }
static double rslc_fatanh(double x) { return 0.5 * log((1.0 + x) / (1.0 - x)); }
static double rslc_fsign(double x) { return x > 0.0 ? 1.0 : x < 0.0 ? -1.0 : 0.0; }
static double rslc_ffract(double x) { return x - floor(x); }
static double rslc_fisqrt(double x) { return 1.0 / sqrt(x); }
static double rslc_fradians(double x) { return x * (3.14159265358979323846 / 180.0); }
static double rslc_fdegrees(double x) { return x * (180.0 / 3.14159265358979323846); }

static double rslc_froundeven(double x)
{
   double f = floor(x);
   double r = x - f;
   if (r < 0.5)
      return f;
   if (r > 0.5)
      return f + 1.0;
   return fmod(f, 2.0) == 0.0 ? f : f + 1.0;
}

static double rslc_fmin(double a, double b) { return b < a ? b : a; }
static double rslc_fmax(double a, double b) { return a < b ? b : a; }
static double rslc_fstep(double edge, double x) { return x < edge ? 0.0 : 1.0; }
static double rslc_fmod(double x, double y) { return x - y * floor(x / y); }

static double rslc_fclamp(double x, double lo, double hi)
{
   return rslc_fmin(rslc_fmax(x, lo), hi);
}

static double rslc_fmix(double x, double y, double a)
{
   return x * (1.0 - a) + y * a;
}

static double rslc_fsmoothstep(double e0, double e1, double x)
{
   double t = rslc_fclamp((x - e0) / (e1 - e0), 0.0, 1.0);
   return t * t * (3.0 - 2.0 * t);
}

static double rslc_ffma(double a, double b, double c) { return a * b + c; }

static void rslc_cval_set_float(struct rslc_cval *v, unsigned i, double d)
{
   v->d[i] = d;
   v->v[i] = rslc_float_bits((float)d);
}

static double rslc_cval_dot(const struct rslc_cval *a, const struct rslc_cval *b, unsigned n)
{
   double   r = 0.0;
   unsigned i;
   for (i = 0; i < n; i++)
      r += a->d[i] * b->d[i];
   return r;
}

static void rslc_cval_set_float(struct rslc_cval *v, unsigned i, double d);

/* ---- matrix folding (column-major, d[col * rows + row]) ---- */

static double rslc_mat_det(const double *m, unsigned n)
{
   if (n == 2)
      return m[0] * m[3] - m[2] * m[1];
   if (n == 3)
      return m[0] * (m[4] * m[8] - m[7] * m[5])
           - m[3] * (m[1] * m[8] - m[7] * m[2])
           + m[6] * (m[1] * m[5] - m[4] * m[2]);
   {
      /* 4x4 by expansion along the first row of the transpose, which
       * is the first column here */
      double   det = 0.0;
      unsigned c;
      for (c = 0; c < 4; c++)
      {
         double   sub[9];
         unsigned sc, sr, k = 0;
         for (sc = 0; sc < 4; sc++)
         {
            if (sc == c)
               continue;
            for (sr = 1; sr < 4; sr++)
               sub[k++] = m[sc * 4 + sr];
         }
         det += (c & 1 ? -1.0 : 1.0) * m[c * 4] * rslc_mat_det(sub, 3);
      }
      return det;
   }
}

static bool rslc_p_eval_matrix_binary(const struct rslc_node *n,
      const struct rslc_cval *a, const struct rslc_cval *b, struct rslc_cval *out)
{
   const struct rslc_type *ta = a->type, *tb = b->type;
   unsigned i, r, c, k;
   out->type = n->type;
   if (n->aux == RSLC_BIN_MUL)
   {
      if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_MAT)
      {
         /* (rows_a x cols_a) * (rows_b x cols_b), cols_a == rows_b */
         for (c = 0; c < tb->cols; c++)
            for (r = 0; r < ta->rows; r++)
            {
               double sum = 0.0;
               for (k = 0; k < ta->cols; k++)
                  sum += a->d[k * ta->rows + r] * b->d[c * tb->rows + k];
               rslc_cval_set_float(out, c * ta->rows + r, sum);
            }
         return true;
      }
      if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_VEC)
      {
         for (r = 0; r < ta->rows; r++)
         {
            double sum = 0.0;
            for (k = 0; k < ta->cols; k++)
               sum += a->d[k * ta->rows + r] * b->d[k];
            rslc_cval_set_float(out, r, sum);
         }
         return true;
      }
      if (ta->kind == RSLC_TY_VEC && tb->kind == RSLC_TY_MAT)
      {
         for (c = 0; c < tb->cols; c++)
         {
            double sum = 0.0;
            for (k = 0; k < tb->rows; k++)
               sum += a->d[k] * b->d[c * tb->rows + k];
            rslc_cval_set_float(out, c, sum);
         }
         return true;
      }
   }
   if (n->aux == RSLC_BIN_EQ || n->aux == RSLC_BIN_NE)
   {
      bool eq = true;
      for (i = 0; i < rslc_ty_components(ta); i++)
         eq = eq && a->d[i] == b->d[i];
      out->v[0] = n->aux == RSLC_BIN_EQ ? eq : !eq;
      return true;
   }
   {
      /* component-wise with a scalar broadcast */
      unsigned na = rslc_ty_components(ta), nb = rslc_ty_components(tb);
      unsigned nc = na > nb ? na : nb;
      for (i = 0; i < nc; i++)
      {
         double x = a->d[na == 1 ? 0 : i], y = b->d[nb == 1 ? 0 : i], d;
         switch (n->aux)
         {
            case RSLC_BIN_ADD: d = x + y; break;
            case RSLC_BIN_SUB: d = x - y; break;
            case RSLC_BIN_MUL: d = x * y; break;
            case RSLC_BIN_DIV: d = y != 0.0 ? x / y : 0.0; break;
            default: return false;
         }
         rslc_cval_set_float(out, i, d);
      }
      return true;
   }
}

static bool rslc_p_eval_matrix_construct(struct rslc_parser *p, const struct rslc_node *n,
      struct rslc_cval *out)
{
   uint32_t nargs = rslc_list_len(p->c, n->b);
   unsigned rows  = n->type->rows, cols = n->type->cols;
   unsigned at    = 0, nc = rows * cols;
   unsigned r, c, k;
   struct rslc_cval a;

   out->type = n->type;
   if (nargs == 1)
   {
      if (!rslc_p_eval(p, rslc_list_item(p->c, n->b, 0), &a))
         return false;
      if (a.type->kind == RSLC_TY_SCALAR)
      {
         double d = a.type->base == RSLC_BASE_FLOAT ? a.d[0]
            : a.type->base == RSLC_BASE_INT ? (double)(int32_t)a.v[0] : (double)a.v[0];
         for (c = 0; c < cols; c++)
            for (r = 0; r < rows; r++)
               rslc_cval_set_float(out, c * rows + r, r == c ? d : 0.0);
         return true;
      }
      if (a.type->kind == RSLC_TY_MAT)
      {
         for (c = 0; c < cols; c++)
            for (r = 0; r < rows; r++)
               rslc_cval_set_float(out, c * rows + r,
                     (c < a.type->cols && r < a.type->rows) ? a.d[c * a.type->rows + r]
                     : r == c ? 1.0 : 0.0);
         return true;
      }
   }
   for (k = 0; k < nargs && at < nc; k++)
   {
      unsigned na, j;
      if (!rslc_p_eval(p, rslc_list_item(p->c, n->b, k), &a))
         return false;
      na = rslc_ty_components(a.type);
      for (j = 0; j < na && at < nc; j++)
      {
         double d = a.type->base == RSLC_BASE_FLOAT ? a.d[j]
            : a.type->base == RSLC_BASE_INT ? (double)(int32_t)a.v[j] : (double)a.v[j];
         rslc_cval_set_float(out, at++, d);
      }
   }
   return at == nc;
}

static bool rslc_p_eval_builtin(struct rslc_parser *p, const struct rslc_node *n,
      struct rslc_cval *out)
{
   struct rslc_cval a[3];
   unsigned na[3];
   uint32_t nargs = rslc_list_len(p->c, n->b);
   unsigned nc, i, k;
   bool     is_float;

   if (nargs < 1 || nargs > 3)
      return false;
   /* glslang leaves inverse() to the GPU; the module must as well */
   if (n->a == RSLC_BFN_TRANSPOSE || n->a == RSLC_BFN_DETERMINANT
         || n->a == RSLC_BFN_MATRIXCOMPMULT)
   {
      const struct rslc_type *mt;
      unsigned r, c;
      if (!rslc_p_eval(p, rslc_list_item(p->c, n->b, 0), &a[0]))
         return false;
      mt        = a[0].type;
      out->type = n->type;
      switch (n->a)
      {
         case RSLC_BFN_TRANSPOSE:
            for (c = 0; c < mt->cols; c++)
               for (r = 0; r < mt->rows; r++)
                  rslc_cval_set_float(out, r * mt->cols + c, a[0].d[c * mt->rows + r]);
            return true;
         case RSLC_BFN_DETERMINANT:
            rslc_cval_set_float(out, 0, rslc_mat_det(a[0].d, mt->rows));
            return true;
         default:
            if (nargs != 2 || !rslc_p_eval(p, rslc_list_item(p->c, n->b, 1), &a[1]))
               return false;
            for (i = 0; i < (unsigned)(mt->rows * mt->cols); i++)
               rslc_cval_set_float(out, i, a[0].d[i] * a[1].d[i]);
            return true;
      }
   }
   if (n->type->kind != RSLC_TY_SCALAR && n->type->kind != RSLC_TY_VEC)
      return false;
   for (k = 0; k < nargs; k++)
   {
      const struct rslc_node *an = rslc_p_node(p, rslc_list_item(p->c, n->b, k));
      if (an->type->kind != RSLC_TY_SCALAR && an->type->kind != RSLC_TY_VEC)
         return false;
      if (!rslc_p_eval(p, rslc_list_item(p->c, n->b, k), &a[k]))
         return false;
      na[k] = rslc_ty_components(a[k].type);
   }
   nc        = rslc_ty_components(n->type);
   out->type = n->type;
   is_float  = a[0].type->base == RSLC_BASE_FLOAT;

#define RSLC_ARG(k, i) a[k].d[na[k] == 1 ? 0 : (i)]
#define RSLC_FOLD1(fn) \
   do { if (!is_float || nargs != 1) return false; \
        for (i = 0; i < nc; i++) rslc_cval_set_float(out, i, fn(RSLC_ARG(0, i))); \
        return true; } while (0)
#define RSLC_FOLD2(fn) \
   do { if (!is_float || nargs != 2) return false; \
        for (i = 0; i < nc; i++) rslc_cval_set_float(out, i, fn(RSLC_ARG(0, i), RSLC_ARG(1, i))); \
        return true; } while (0)
#define RSLC_FOLD3(fn) \
   do { if (!is_float || nargs != 3) return false; \
        for (i = 0; i < nc; i++) rslc_cval_set_float(out, i, fn(RSLC_ARG(0, i), RSLC_ARG(1, i), RSLC_ARG(2, i))); \
        return true; } while (0)

   switch (n->a)
   {
      case RSLC_BFN_RADIANS:     RSLC_FOLD1(rslc_fradians);
      case RSLC_BFN_DEGREES:     RSLC_FOLD1(rslc_fdegrees);
      case RSLC_BFN_SIN:         RSLC_FOLD1(sin);
      case RSLC_BFN_COS:         RSLC_FOLD1(cos);
      case RSLC_BFN_TAN:         RSLC_FOLD1(tan);
      case RSLC_BFN_ASIN:        RSLC_FOLD1(asin);
      case RSLC_BFN_ACOS:        RSLC_FOLD1(acos);
      case RSLC_BFN_ATAN:
         if (nargs == 2)
            RSLC_FOLD2(atan2);
         RSLC_FOLD1(atan);
      case RSLC_BFN_SINH:        RSLC_FOLD1(sinh);
      case RSLC_BFN_COSH:        RSLC_FOLD1(cosh);
      case RSLC_BFN_TANH:        RSLC_FOLD1(tanh);
      case RSLC_BFN_ASINH:       RSLC_FOLD1(rslc_fasinh);
      case RSLC_BFN_ACOSH:       RSLC_FOLD1(rslc_facosh);
      case RSLC_BFN_ATANH:       RSLC_FOLD1(rslc_fatanh);
      case RSLC_BFN_POW:         RSLC_FOLD2(pow);
      case RSLC_BFN_EXP:         RSLC_FOLD1(exp);
      case RSLC_BFN_LOG:         RSLC_FOLD1(log);
      case RSLC_BFN_EXP2:        RSLC_FOLD1(rslc_fexp2);
      case RSLC_BFN_LOG2:        RSLC_FOLD1(rslc_flog2);
      case RSLC_BFN_SQRT:        RSLC_FOLD1(sqrt);
      case RSLC_BFN_INVERSESQRT: RSLC_FOLD1(rslc_fisqrt);
      case RSLC_BFN_FLOOR:       RSLC_FOLD1(floor);
      case RSLC_BFN_TRUNC:       RSLC_FOLD1(rslc_ftrunc);
      case RSLC_BFN_ROUND:       RSLC_FOLD1(rslc_fround);
      case RSLC_BFN_ROUNDEVEN:   RSLC_FOLD1(rslc_froundeven);
      case RSLC_BFN_CEIL:        RSLC_FOLD1(ceil);
      case RSLC_BFN_FRACT:       RSLC_FOLD1(rslc_ffract);
      case RSLC_BFN_MOD:         RSLC_FOLD2(rslc_fmod);
      case RSLC_BFN_STEP:        RSLC_FOLD2(rslc_fstep);
      case RSLC_BFN_SMOOTHSTEP:  RSLC_FOLD3(rslc_fsmoothstep);
      case RSLC_BFN_FMA:         RSLC_FOLD3(rslc_ffma);
      case RSLC_BFN_MIX:
         if (nargs == 3 && a[2].type->base == RSLC_BASE_BOOL)
         {
            if (!is_float)
               return false;
            for (i = 0; i < nc; i++)
               rslc_cval_set_float(out, i, a[2].v[na[2] == 1 ? 0 : i] ? RSLC_ARG(1, i) : RSLC_ARG(0, i));
            return true;
         }
         RSLC_FOLD3(rslc_fmix);
      case RSLC_BFN_ABS:
      case RSLC_BFN_SIGN:
      case RSLC_BFN_MIN:
      case RSLC_BFN_MAX:
      case RSLC_BFN_CLAMP:
         if (is_float)
         {
            switch (n->a)
            {
               case RSLC_BFN_ABS:  RSLC_FOLD1(fabs);
               case RSLC_BFN_SIGN: RSLC_FOLD1(rslc_fsign);
               case RSLC_BFN_MIN:  RSLC_FOLD2(rslc_fmin);
               case RSLC_BFN_MAX:  RSLC_FOLD2(rslc_fmax);
               default:            RSLC_FOLD3(rslc_fclamp);
            }
         }
         for (i = 0; i < nc; i++)
         {
            uint32_t x = a[0].v[na[0] == 1 ? 0 : i];
            uint32_t y = nargs > 1 ? a[1].v[na[1] == 1 ? 0 : i] : 0;
            uint32_t z = nargs > 2 ? a[2].v[na[2] == 1 ? 0 : i] : 0;
            if (a[0].type->base == RSLC_BASE_INT)
            {
               int32_t sx = (int32_t)x, sy = (int32_t)y, sz = (int32_t)z, r;
               switch (n->a)
               {
                  case RSLC_BFN_ABS:  r = sx < 0 ? -sx : sx; break;
                  case RSLC_BFN_SIGN: r = sx > 0 ? 1 : sx < 0 ? -1 : 0; break;
                  case RSLC_BFN_MIN:  r = sy < sx ? sy : sx; break;
                  case RSLC_BFN_MAX:  r = sx < sy ? sy : sx; break;
                  default:            r = sx < sy ? sy : sx; r = sz < r ? sz : r; break;
               }
               out->v[i] = (uint32_t)r;
            }
            else
            {
               uint32_t r;
               switch (n->a)
               {
                  case RSLC_BFN_ABS:  r = x; break;
                  case RSLC_BFN_SIGN: r = x ? 1 : 0; break;
                  case RSLC_BFN_MIN:  r = y < x ? y : x; break;
                  case RSLC_BFN_MAX:  r = x < y ? y : x; break;
                  default:            r = x < y ? y : x; r = z < r ? z : r; break;
               }
               out->v[i] = r;
            }
         }
         return true;
      case RSLC_BFN_LENGTH:
         if (!is_float || nargs != 1)
            return false;
         rslc_cval_set_float(out, 0, sqrt(rslc_cval_dot(&a[0], &a[0], na[0])));
         return true;
      case RSLC_BFN_DISTANCE:
      {
         struct rslc_cval d;
         if (!is_float || nargs != 2)
            return false;
         for (i = 0; i < na[0]; i++)
            d.d[i] = a[0].d[i] - a[1].d[i];
         rslc_cval_set_float(out, 0, sqrt(rslc_cval_dot(&d, &d, na[0])));
         return true;
      }
      case RSLC_BFN_DOT:
         if (!is_float || nargs != 2)
            return false;
         rslc_cval_set_float(out, 0, rslc_cval_dot(&a[0], &a[1], na[0]));
         return true;
      case RSLC_BFN_CROSS:
         if (!is_float || nargs != 2 || na[0] != 3)
            return false;
         rslc_cval_set_float(out, 0, a[0].d[1] * a[1].d[2] - a[1].d[1] * a[0].d[2]);
         rslc_cval_set_float(out, 1, a[0].d[2] * a[1].d[0] - a[1].d[2] * a[0].d[0]);
         rslc_cval_set_float(out, 2, a[0].d[0] * a[1].d[1] - a[1].d[0] * a[0].d[1]);
         return true;
      case RSLC_BFN_NORMALIZE:
      {
         double len;
         if (!is_float || nargs != 1)
            return false;
         len = sqrt(rslc_cval_dot(&a[0], &a[0], na[0]));
         for (i = 0; i < nc; i++)
            rslc_cval_set_float(out, i, a[0].d[i] / len);
         return true;
      }
      case RSLC_BFN_REFLECT:
      {
         double d2;
         if (!is_float || nargs != 2)
            return false;
         d2 = 2.0 * rslc_cval_dot(&a[1], &a[0], na[0]);
         for (i = 0; i < nc; i++)
            rslc_cval_set_float(out, i, a[0].d[i] - d2 * a[1].d[i]);
         return true;
      }
      case RSLC_BFN_REFRACT:
      {
         double eta, dn, k;
         if (!is_float || nargs != 3)
            return false;
         eta = a[2].d[0];
         dn  = rslc_cval_dot(&a[1], &a[0], na[0]);
         k   = 1.0 - eta * eta * (1.0 - dn * dn);
         for (i = 0; i < nc; i++)
            rslc_cval_set_float(out, i, k < 0.0 ? 0.0
                  : eta * a[0].d[i] - (eta * dn + sqrt(k)) * a[1].d[i]);
         return true;
      }
      case RSLC_BFN_FACEFORWARD:
      {
         bool flip;
         if (!is_float || nargs != 3)
            return false;
         flip = rslc_cval_dot(&a[2], &a[1], na[0]) >= 0.0;
         for (i = 0; i < nc; i++)
            rslc_cval_set_float(out, i, flip ? -a[0].d[i] : a[0].d[i]);
         return true;
      }
      case RSLC_BFN_LESSTHAN: case RSLC_BFN_LESSTHANEQUAL: case RSLC_BFN_GREATERTHAN:
      case RSLC_BFN_GREATERTHANEQUAL: case RSLC_BFN_EQUAL: case RSLC_BFN_NOTEQUAL:
      {
         uint16_t op = n->a == RSLC_BFN_LESSTHAN ? RSLC_BIN_LT
            : n->a == RSLC_BFN_LESSTHANEQUAL ? RSLC_BIN_LE
            : n->a == RSLC_BFN_GREATERTHAN ? RSLC_BIN_GT
            : n->a == RSLC_BFN_GREATERTHANEQUAL ? RSLC_BIN_GE
            : n->a == RSLC_BFN_EQUAL ? RSLC_BIN_EQ : RSLC_BIN_NE;
         bool ok = true;
         if (nargs != 2)
            return false;
         for (i = 0; i < nc; i++)
         {
            double dd;
            out->v[i] = rslc_fold_scalar(a[0].type->base, op, a[0].v[i], a[1].v[i],
                  a[0].d[i], a[1].d[i], &dd, &ok);
         }
         return ok;
      }
      case RSLC_BFN_ANY: case RSLC_BFN_ALL:
      {
         bool r = n->a == RSLC_BFN_ALL;
         if (nargs != 1)
            return false;
         for (i = 0; i < na[0]; i++)
            r = n->a == RSLC_BFN_ALL ? (r && a[0].v[i]) : (r || a[0].v[i]);
         out->v[0] = r;
         return true;
      }
      case RSLC_BFN_NOT:
         if (nargs != 1)
            return false;
         for (i = 0; i < nc; i++)
            out->v[i] = !a[0].v[i];
         return true;
      case RSLC_BFN_ISNAN:
         if (!is_float || nargs != 1)
            return false;
         for (i = 0; i < nc; i++)
            out->v[i] = a[0].d[i] != a[0].d[i];
         return true;
      case RSLC_BFN_ISINF:
         if (!is_float || nargs != 1)
            return false;
         for (i = 0; i < nc; i++)
            out->v[i] = a[0].d[i] == a[0].d[i] && a[0].d[i] - a[0].d[i] != 0.0;
         return true;
      default:
         return false;
   }
#undef RSLC_ARG
#undef RSLC_FOLD1
#undef RSLC_FOLD2
#undef RSLC_FOLD3
}

/* Item k of a const struct or array: the node is a const variable
 * initialised by a constructor, or the constructor itself, and the
 * item is that constructor's k-th argument; 0 otherwise. */
static uint32_t rslc_p_const_aggregate_item(struct rslc_parser *p, uint32_t e, uint32_t k)
{
   const struct rslc_node *n = rslc_p_node(p, e);
   if (n->op == RSLC_OP_VAR)
   {
      const struct rslc_var *v = p->c->prog.vars[n->a];
      if (v->storage != RSLC_ST_CONST || !v->init)
         return 0;
      n = rslc_p_node(p, v->init);
   }
   if (n->op != RSLC_OP_CONSTRUCT || !n->b || k >= rslc_list_len(p->c, n->b))
      return 0;
   return rslc_list_item(p->c, n->b, k);
}

/* Evaluates a constant expression over scalars and vectors; false when
 * it is not one.  Enough for array sizes, case labels, const
 * initialisers and layout values. */
static bool rslc_p_eval(struct rslc_parser *p, uint32_t e, struct rslc_cval *out)
{
   struct rslc_node *n = rslc_p_node(p, e);
   unsigned          i, nc;

   memset(out, 0, sizeof(*out));
   /* a failed compile's nodes may name lists and nodes never made */
   if (!e || p->c->failed || n->nonconstant)
      return false;
   switch (n->op)
   {
      case RSLC_OP_CONST:
         out->type = n->type;
         out->v[0] = n->a;
         if (n->type->base == RSLC_BASE_FLOAT)
            out->d[0] = rslc_p_const_double(n);
         return true;
      case RSLC_OP_VAR:
      {
         struct rslc_var *v = p->c->prog.vars[n->a];
         if (v->storage != RSLC_ST_CONST || !v->init)
            return false;
         return rslc_p_eval(p, v->init, out);
      }
      case RSLC_OP_CONVERT:
      {
         struct rslc_cval in;
         if (!rslc_p_eval(p, n->a, &in))
            return false;
         nc = rslc_ty_components(n->type);
         out->type = n->type;
         for (i = 0; i < nc; i++)
         {
            if (n->type->base == RSLC_BASE_FLOAT)
            {
               out->d[i] = in.type->base == RSLC_BASE_INT
                     ? (double)(int32_t)in.v[i] : (double)in.v[i];
               out->v[i] = rslc_float_bits((float)out->d[i]);
            }
            else
               out->v[i] = in.v[i];
         }
         return true;
      }
      case RSLC_OP_UNARY:
      {
         struct rslc_cval in;
         if (!rslc_p_eval(p, n->a, &in))
            return false;
         nc = rslc_ty_components(n->type);
         out->type = n->type;
         for (i = 0; i < nc; i++)
         {
            switch (n->aux)
            {
               case RSLC_UN_NEG:
                  if (n->type->base == RSLC_BASE_FLOAT)
                  {
                     out->d[i] = -in.d[i];
                     out->v[i] = rslc_float_bits((float)out->d[i]);
                  }
                  else
                     out->v[i] = (uint32_t)(-(int32_t)in.v[i]);
                  break;
               case RSLC_UN_NOT:    out->v[i] = !in.v[i]; break;
               case RSLC_UN_BITNOT: out->v[i] = ~in.v[i]; break;
               default: return false;
            }
         }
         return true;
      }
      case RSLC_OP_BINARY:
      {
         struct rslc_cval a, b;
         bool     ok = true;
         unsigned na, nb;
         if (!rslc_p_eval(p, n->a, &a) || !rslc_p_eval(p, n->b, &b))
            return false;
         if (a.type->kind == RSLC_TY_MAT || b.type->kind == RSLC_TY_MAT)
            return rslc_p_eval_matrix_binary(n, &a, &b, out);
         na = rslc_ty_components(a.type);
         nb = rslc_ty_components(b.type);
         nc = na > nb ? na : nb;
         out->type = n->type;
         for (i = 0; i < nc; i++)
            out->v[i] = rslc_fold_scalar(a.type->base, n->aux,
                  a.v[na == 1 ? 0 : i], b.v[nb == 1 ? 0 : i],
                  a.d[na == 1 ? 0 : i], b.d[nb == 1 ? 0 : i], &out->d[i], &ok);
         return ok;
      }
      case RSLC_OP_TERNARY:
      {
         struct rslc_cval k;
         if (!rslc_p_eval(p, n->a, &k))
            return false;
         return rslc_p_eval(p, k.v[0] ? n->b : n->c, out);
      }
      case RSLC_OP_SWIZZLE:
      {
         struct rslc_cval in;
         unsigned cnt = n->aux & 3u;
         if (!rslc_p_eval(p, n->a, &in))
            return false;
         if (cnt == 0)
            cnt = 4;
         out->type = n->type;
         for (i = 0; i < cnt; i++)
         {
            out->v[i] = in.v[(n->aux >> (2 + 2 * i)) & 3u];
            out->d[i] = in.d[(n->aux >> (2 + 2 * i)) & 3u];
         }
         return true;
      }
      case RSLC_OP_CONSTRUCT:
      {
         uint32_t nargs = rslc_list_len(p->c, n->b);
         unsigned at    = 0;
         uint32_t k;
         if (n->type->kind == RSLC_TY_MAT)
            return rslc_p_eval_matrix_construct(p, n, out);
         if (n->type->kind != RSLC_TY_SCALAR && n->type->kind != RSLC_TY_VEC)
            return false;
         nc = rslc_ty_components(n->type);
         out->type = n->type;
         for (k = 0; k < nargs && at < nc; k++)
         {
            struct rslc_cval a;
            unsigned na, j;
            if (!rslc_p_eval(p, rslc_list_item(p->c, n->b, k), &a))
               return false;
            if (a.type->kind == RSLC_TY_MAT && nargs == 1 && n->type->kind == RSLC_TY_SCALAR)
            {
               rslc_cval_set_float(out, 0, a.d[0]);
               return true;
            }
            na = rslc_ty_components(a.type);
            for (j = 0; j < (nargs == 1 && na == 1 ? nc : na) && at < nc; j++)
            {
               unsigned src  = nargs == 1 && na == 1 ? 0 : j;   /* scalar splat */
               uint32_t bits = a.v[src];
               double   d    = a.d[src];
               if (n->type->base != a.type->base)
               {
                  if (n->type->base == RSLC_BASE_FLOAT)
                  {
                     if (a.type->base == RSLC_BASE_INT)
                        d = (double)(int32_t)bits;
                     else if (a.type->base != RSLC_BASE_FLOAT)
                        d = (double)bits;
                     bits = rslc_float_bits((float)d);
                  }
                  else if (a.type->base == RSLC_BASE_FLOAT)
                     bits = n->type->base == RSLC_BASE_INT
                        ? (uint32_t)(int32_t)d : (uint32_t)d;
                  if (n->type->base == RSLC_BASE_BOOL)
                     bits = bits != 0;
               }
               out->d[at]   = d;
               out->v[at++] = bits;
            }
         }
         return at == nc;
      }
      case RSLC_OP_BUILTIN:
         return rslc_p_eval_builtin(p, n, out);
      case RSLC_OP_MEMBER:
      {
         /* field of a const struct: the initialiser's argument */
         uint32_t arg = rslc_p_const_aggregate_item(p, n->a, n->b);
         return arg && rslc_p_eval(p, arg, out);
      }
      case RSLC_OP_INDEX:
      {
         struct rslc_cval base, idx;
         unsigned k, rows;
         if (!rslc_p_eval(p, n->b, &idx))
            return false;
         if (rslc_p_node(p, n->a)->type->kind == RSLC_TY_ARRAY)
         {
            uint32_t arg = rslc_p_const_aggregate_item(p, n->a, idx.v[0]);
            return arg && rslc_p_eval(p, arg, out);
         }
         if (!rslc_p_eval(p, n->a, &base))
            return false;
         if (base.type->kind != RSLC_TY_VEC && base.type->kind != RSLC_TY_MAT)
            return false;
         k = idx.v[0];
         out->type = n->type;
         if (base.type->kind == RSLC_TY_VEC)
         {
            if (k >= base.type->rows)
               return false;
            out->v[0] = base.v[k];
            out->d[0] = base.d[k];
            return true;
         }
         rows = base.type->rows;
         if (k >= base.type->cols)
            return false;
         for (i = 0; i < rows; i++)
         {
            out->v[i] = base.v[k * rows + i];
            out->d[i] = base.d[k * rows + i];
         }
         return true;
      }
      default:
         return false;
   }
}

/* ---- builtin function signatures ---- */

/* One overload: parameter pattern tokens then the result.  Letters:
 *   F I U B   genType of float / int / uint / bool, one width N shared
 *   f i u b   scalar
 *   2 3 4     vec2 vec3 vec4; i2.. ivec, u2.. uvec, b2.. bvec
 *   S         sampler2D
 *   M         any matrix;  Q square matrix;  T transposed M result
 *   X         any scalar or vector, same type as other X
 *   o<tok>    out parameter
 *   @         generic result rules handled in code */
struct rslc_bsig
{
   uint16_t    id;
   const char *name;
   const char *sigs;   /* overloads separated by ';', params space separated, result after ':' */
};

static const struct rslc_bsig rslc_bsigs[] = {
   { RSLC_BFN_RADIANS, "radians", "F:F" }, { RSLC_BFN_DEGREES, "degrees", "F:F" },
   { RSLC_BFN_SIN, "sin", "F:F" }, { RSLC_BFN_COS, "cos", "F:F" }, { RSLC_BFN_TAN, "tan", "F:F" },
   { RSLC_BFN_ASIN, "asin", "F:F" }, { RSLC_BFN_ACOS, "acos", "F:F" },
   { RSLC_BFN_ATAN, "atan", "F F:F;F:F" },
   { RSLC_BFN_SINH, "sinh", "F:F" }, { RSLC_BFN_COSH, "cosh", "F:F" }, { RSLC_BFN_TANH, "tanh", "F:F" },
   { RSLC_BFN_ASINH, "asinh", "F:F" }, { RSLC_BFN_ACOSH, "acosh", "F:F" }, { RSLC_BFN_ATANH, "atanh", "F:F" },
   { RSLC_BFN_POW, "pow", "F F:F" }, { RSLC_BFN_EXP, "exp", "F:F" }, { RSLC_BFN_LOG, "log", "F:F" },
   { RSLC_BFN_EXP2, "exp2", "F:F" }, { RSLC_BFN_LOG2, "log2", "F:F" },
   { RSLC_BFN_SQRT, "sqrt", "F:F" }, { RSLC_BFN_INVERSESQRT, "inversesqrt", "F:F" },
   { RSLC_BFN_ABS, "abs", "F:F;I:I" }, { RSLC_BFN_SIGN, "sign", "F:F;I:I" },
   { RSLC_BFN_FLOOR, "floor", "F:F" }, { RSLC_BFN_TRUNC, "trunc", "F:F" },
   { RSLC_BFN_ROUND, "round", "F:F" }, { RSLC_BFN_ROUNDEVEN, "roundEven", "F:F" },
   { RSLC_BFN_CEIL, "ceil", "F:F" }, { RSLC_BFN_FRACT, "fract", "F:F" },
   { RSLC_BFN_MOD, "mod", "F F:F;F f:F" }, { RSLC_BFN_MODF, "modf", "F oF:F" },
   { RSLC_BFN_MIN, "min", "F F:F;F f:F;I I:I;I i:I;U U:U;U u:U" },
   { RSLC_BFN_MAX, "max", "F F:F;F f:F;I I:I;I i:I;U U:U;U u:U" },
   { RSLC_BFN_CLAMP, "clamp", "F F F:F;F f f:F;I I I:I;I i i:I;U U U:U;U u u:U" },
   { RSLC_BFN_MIX, "mix", "F F F:F;F F f:F;F F B:F;I I B:I;U U B:U;B B B:B" },
   { RSLC_BFN_STEP, "step", "F F:F;f F:F" },
   { RSLC_BFN_SMOOTHSTEP, "smoothstep", "F F F:F;f f F:F" },
   { RSLC_BFN_ISNAN, "isnan", "F:B" }, { RSLC_BFN_ISINF, "isinf", "F:B" },
   { RSLC_BFN_FLOATBITSTOINT, "floatBitsToInt", "F:I" },
   { RSLC_BFN_FLOATBITSTOUINT, "floatBitsToUint", "F:U" },
   { RSLC_BFN_INTBITSTOFLOAT, "intBitsToFloat", "I:F" },
   { RSLC_BFN_UINTBITSTOFLOAT, "uintBitsToFloat", "U:F" },
   { RSLC_BFN_FMA, "fma", "F F F:F" }, { RSLC_BFN_FREXP, "frexp", "F oI:F" },
   { RSLC_BFN_LDEXP, "ldexp", "F I:F" },
   { RSLC_BFN_PACKUNORM2X16, "packUnorm2x16", "2:u" }, { RSLC_BFN_PACKSNORM2X16, "packSnorm2x16", "2:u" },
   { RSLC_BFN_PACKUNORM4X8, "packUnorm4x8", "4:u" }, { RSLC_BFN_PACKSNORM4X8, "packSnorm4x8", "4:u" },
   { RSLC_BFN_UNPACKUNORM2X16, "unpackUnorm2x16", "u:2" }, { RSLC_BFN_UNPACKSNORM2X16, "unpackSnorm2x16", "u:2" },
   { RSLC_BFN_UNPACKUNORM4X8, "unpackUnorm4x8", "u:4" }, { RSLC_BFN_UNPACKSNORM4X8, "unpackSnorm4x8", "u:4" },
   { RSLC_BFN_PACKHALF2X16, "packHalf2x16", "2:u" }, { RSLC_BFN_UNPACKHALF2X16, "unpackHalf2x16", "u:2" },
   { RSLC_BFN_LENGTH, "length", "F:f" }, { RSLC_BFN_DISTANCE, "distance", "F F:f" },
   { RSLC_BFN_DOT, "dot", "F F:f" }, { RSLC_BFN_CROSS, "cross", "3 3:3" },
   { RSLC_BFN_NORMALIZE, "normalize", "F:F" }, { RSLC_BFN_FACEFORWARD, "faceforward", "F F F:F" },
   { RSLC_BFN_REFLECT, "reflect", "F F:F" }, { RSLC_BFN_REFRACT, "refract", "F F f:F" },
   { RSLC_BFN_MATRIXCOMPMULT, "matrixCompMult", "M M:M" },
   { RSLC_BFN_OUTERPRODUCT, "outerProduct", "@" },
   { RSLC_BFN_TRANSPOSE, "transpose", "M:T" },
   { RSLC_BFN_DETERMINANT, "determinant", "Q:f" }, { RSLC_BFN_INVERSE, "inverse", "Q:Q" },
   { RSLC_BFN_LESSTHAN, "lessThan", "F F:B;I I:B;U U:B" },
   { RSLC_BFN_LESSTHANEQUAL, "lessThanEqual", "F F:B;I I:B;U U:B" },
   { RSLC_BFN_GREATERTHAN, "greaterThan", "F F:B;I I:B;U U:B" },
   { RSLC_BFN_GREATERTHANEQUAL, "greaterThanEqual", "F F:B;I I:B;U U:B" },
   { RSLC_BFN_EQUAL, "equal", "F F:B;I I:B;U U:B;B B:B" },
   { RSLC_BFN_NOTEQUAL, "notEqual", "F F:B;I I:B;U U:B;B B:B" },
   { RSLC_BFN_ANY, "any", "B:b" }, { RSLC_BFN_ALL, "all", "B:b" }, { RSLC_BFN_NOT, "not", "B:B" },
   { RSLC_BFN_UADDCARRY, "uaddCarry", "U U oU:U" }, { RSLC_BFN_USUBBORROW, "usubBorrow", "U U oU:U" },
   { RSLC_BFN_UMULEXTENDED, "umulExtended", "U U oU oU:v" },
   { RSLC_BFN_IMULEXTENDED, "imulExtended", "I I oI oI:v" },
   { RSLC_BFN_BITFIELDEXTRACT, "bitfieldExtract", "I i i:I;U i i:U" },
   { RSLC_BFN_BITFIELDINSERT, "bitfieldInsert", "I I i i:I;U U i i:U" },
   { RSLC_BFN_BITFIELDREVERSE, "bitfieldReverse", "I:I;U:U" },
   { RSLC_BFN_BITCOUNT, "bitCount", "I:I;U:I" },
   { RSLC_BFN_FINDLSB, "findLSB", "I:I;U:I" }, { RSLC_BFN_FINDMSB, "findMSB", "I:I;U:I" },
   { RSLC_BFN_TEXTURESIZE, "textureSize", "S i:i2" },
   { RSLC_BFN_TEXTUREQUERYLOD, "textureQueryLod", "S 2:2" },
   { RSLC_BFN_TEXTUREQUERYLEVELS, "textureQueryLevels", "S:i" },
   { RSLC_BFN_TEXTURE, "texture", "S 2:4;S 2 f:4" },
   { RSLC_BFN_TEXTUREPROJ, "textureProj", "S 3:4;S 4:4;S 3 f:4;S 4 f:4" },
   { RSLC_BFN_TEXTURELOD, "textureLod", "S 2 f:4" },
   { RSLC_BFN_TEXTUREOFFSET, "textureOffset", "S 2 i2:4;S 2 i2 f:4" },
   { RSLC_BFN_TEXELFETCH, "texelFetch", "S i2 i:4" },
   { RSLC_BFN_TEXELFETCHOFFSET, "texelFetchOffset", "S i2 i i2:4" },
   { RSLC_BFN_TEXTUREPROJOFFSET, "textureProjOffset", "S 3 i2:4;S 4 i2:4;S 3 i2 f:4;S 4 i2 f:4" },
   { RSLC_BFN_TEXTURELODOFFSET, "textureLodOffset", "S 2 f i2:4" },
   { RSLC_BFN_TEXTUREPROJLOD, "textureProjLod", "S 3 f:4;S 4 f:4" },
   { RSLC_BFN_TEXTUREPROJLODOFFSET, "textureProjLodOffset", "S 3 f i2:4;S 4 f i2:4" },
   { RSLC_BFN_TEXTUREGRAD, "textureGrad", "S 2 2 2:4" },
   { RSLC_BFN_TEXTUREGRADOFFSET, "textureGradOffset", "S 2 2 2 i2:4" },
   { RSLC_BFN_TEXTUREPROJGRAD, "textureProjGrad", "S 3 2 2:4;S 4 2 2:4" },
   { RSLC_BFN_TEXTUREPROJGRADOFFSET, "textureProjGradOffset", "S 3 2 2 i2:4;S 4 2 2 i2:4" },
   { RSLC_BFN_TEXTUREGATHER, "textureGather", "S 2:4;S 2 i:4" },
   { RSLC_BFN_TEXTUREGATHEROFFSET, "textureGatherOffset", "S 2 i2:4;S 2 i2 i:4" },
   { RSLC_BFN_DFDX, "dFdx", "F:F" }, { RSLC_BFN_DFDY, "dFdy", "F:F" }, { RSLC_BFN_FWIDTH, "fwidth", "F:F" },
   { RSLC_BFN_DFDXFINE, "dFdxFine", "F:F" }, { RSLC_BFN_DFDYFINE, "dFdyFine", "F:F" },
   { RSLC_BFN_FWIDTHFINE, "fwidthFine", "F:F" }, { RSLC_BFN_DFDXCOARSE, "dFdxCoarse", "F:F" },
   { RSLC_BFN_DFDYCOARSE, "dFdyCoarse", "F:F" }, { RSLC_BFN_FWIDTHCOARSE, "fwidthCoarse", "F:F" }
};

static const struct rslc_bsig *rslc_bsig_by_id(unsigned id)
{
   unsigned i;
   for (i = 0; i < sizeof(rslc_bsigs) / sizeof(rslc_bsigs[0]); i++)
      if (rslc_bsigs[i].id == id)
         return &rslc_bsigs[i];
   return NULL;
}

/* The GLSL name of a builtin, for the printers. */
const char *rslc_bfn_name(unsigned id)
{
   const struct rslc_bsig *bs = rslc_bsig_by_id(id);
   return bs ? bs->name : "";
}

static bool rslc_p_seed_builtins(struct rslc_parser *p)
{
   unsigned i;
   for (i = 0; i < sizeof(rslc_bsigs) / sizeof(rslc_bsigs[0]); i++)
   {
      struct rslc_sym *s = rslc_sym(p->c, rslc_bsigs[i].name, strlen(rslc_bsigs[i].name));
      if (!s)
         return false;
      s->bfn = rslc_bsigs[i].id;
   }
   return true;
}

/* Matches one pattern token against an argument type.  n binds the
 * gen width; m the matrix shape (cols<<8|rows); x the X type.  conv
 * counts implicit conversions used.  Returns false on no match. */
static bool rslc_bsig_match_tok(struct rslc_parser *p, const char *tok, unsigned len,
      const struct rslc_type *t, unsigned *n, unsigned *m, const struct rslc_type **x,
      unsigned *conv, bool allow_conv, unsigned *sbase)
{
   enum rslc_base want;
   unsigned        width;
   bool            gen = false;
   const struct rslc_type *target;

   if (len == 1 && (tok[0] == 'F' || tok[0] == 'I' || tok[0] == 'U' || tok[0] == 'B'))
   {
      gen  = true;
      want = tok[0] == 'F' ? RSLC_BASE_FLOAT : tok[0] == 'I' ? RSLC_BASE_INT
           : tok[0] == 'U' ? RSLC_BASE_UINT : RSLC_BASE_BOOL;
      if (t->kind != RSLC_TY_SCALAR && t->kind != RSLC_TY_VEC)
         return false;
      width = t->rows;
      if (*n && *n != width)
         return false;
      *n = width;
   }
   else if (len == 1 && (tok[0] == 'f' || tok[0] == 'i' || tok[0] == 'u' || tok[0] == 'b'))
   {
      want  = tok[0] == 'f' ? RSLC_BASE_FLOAT : tok[0] == 'i' ? RSLC_BASE_INT
            : tok[0] == 'u' ? RSLC_BASE_UINT : RSLC_BASE_BOOL;
      width = 1;
      if (t->kind != RSLC_TY_SCALAR)
         return false;
   }
   else if (len == 1 && tok[0] >= '2' && tok[0] <= '4')
   {
      want  = RSLC_BASE_FLOAT;
      width = (unsigned)(tok[0] - '0');
      if (t->kind != RSLC_TY_VEC || t->rows != width)
         return false;
   }
   else if (len == 2 && (tok[0] == 'i' || tok[0] == 'u' || tok[0] == 'b') && tok[1] >= '2' && tok[1] <= '4')
   {
      want  = tok[0] == 'i' ? RSLC_BASE_INT : tok[0] == 'u' ? RSLC_BASE_UINT : RSLC_BASE_BOOL;
      width = (unsigned)(tok[1] - '0');
      if (t->kind != RSLC_TY_VEC || t->rows != width)
         return false;
   }
   else if (len == 1 && tok[0] == 'S')
   {
      if (t->kind != RSLC_TY_SAMPLER2D)
         return false;
      *sbase = t->base;
      return true;
   }
   else if (len == 1 && (tok[0] == 'M' || tok[0] == 'Q'))
   {
      unsigned shape;
      if (t->kind != RSLC_TY_MAT)
         return false;
      if (tok[0] == 'Q' && t->rows != t->cols)
         return false;
      shape = ((unsigned)t->cols << 8) | t->rows;
      if (*m && *m != shape)
         return false;
      *m = shape;
      return true;
   }
   else if (len == 1 && tok[0] == 'X')
   {
      if (*x && *x != t)
         return false;
      *x = t;
      return true;
   }
   else
      return false;

   (void)gen;
   target = rslc_ty_vecn(p, want, width);
   if (t == target)
      return true;
   if (allow_conv && rslc_ty_converts(t, target))
   {
      (*conv)++;
      return true;
   }
   return false;
}

static const struct rslc_type *rslc_bsig_result(struct rslc_parser *p, const char *tok,
      unsigned len, unsigned n, unsigned m, const struct rslc_type *x, unsigned sbase)
{
   if (len == 1)
   {
      switch (tok[0])
      {
         case '4':
            /* A texel of an integer sampler is an integer vector. */
            if (sbase == RSLC_BASE_INT || sbase == RSLC_BASE_UINT)
               return rslc_ty_vecn(p, (enum rslc_base)sbase, 4);
            return rslc_ty_vecn(p, RSLC_BASE_FLOAT, 4);
         case 'F': return rslc_ty_vecn(p, RSLC_BASE_FLOAT, n ? n : 1);
         case 'I': return rslc_ty_vecn(p, RSLC_BASE_INT, n ? n : 1);
         case 'U': return rslc_ty_vecn(p, RSLC_BASE_UINT, n ? n : 1);
         case 'B': return rslc_ty_vecn(p, RSLC_BASE_BOOL, n ? n : 1);
         case 'f': return p->t_float;
         case 'i': return p->t_int;
         case 'u': return p->t_uint;
         case 'b': return p->t_bool;
         case 'v': return p->t_void;
         case '2': case '3':
            return rslc_ty_vecn(p, RSLC_BASE_FLOAT, (unsigned)(tok[0] - '0'));
         case 'M': return rslc_ty_mat(p, m >> 8, m & 0xff);
         case 'Q': return rslc_ty_mat(p, m >> 8, m & 0xff);
         case 'T': return rslc_ty_mat(p, m & 0xff, m >> 8);
         case 'X': return x;
      }
   }
   if (len == 2 && tok[1] >= '2' && tok[1] <= '4')
      return rslc_ty_vecn(p, tok[0] == 'i' ? RSLC_BASE_INT : tok[0] == 'u'
            ? RSLC_BASE_UINT : RSLC_BASE_BOOL, (unsigned)(tok[1] - '0'));
   return NULL;
}

/* The parameter type a matched token stands for, so arguments can be
 * converted to it. */
static const struct rslc_type *rslc_bsig_param_type(struct rslc_parser *p,
      const char *tok, unsigned len, unsigned n, unsigned m, const struct rslc_type *x,
      const struct rslc_type *actual)
{
   if (len == 1 && tok[0] == 'S')
      return actual;
   return rslc_bsig_result(p, tok, len, n, m, x, 0);
}

/* Resolves a builtin call.  args are node ids; on success the out
 * types are written and arguments converted. */
/* Binds one signature: converts the arguments to its parameter types
 * and derives the result type. */
static bool rslc_p_apply_sig(struct rslc_parser *p, const struct rslc_bsig *bs,
      const char *sig, uint32_t *args, uint32_t nargs, bool allow_conv,
      const struct rslc_type **ret)
{
   const char *q = sig;
   unsigned    n = 0, m = 0, conv = 0, sbase = 0;
   const struct rslc_type *x = NULL;
   const struct rslc_type *ptypes[8];
   bool        outp[8];
   unsigned    np = 0, k;
   const char *rtok;
   unsigned    rlen;

   while (*q && *q != ':')
   {
      const char *tok;
      unsigned    len;
      bool        is_out = false;
      const struct rslc_type *at;
      while (*q == ' ')
         q++;
      if (*q == 'o')
      {
         is_out = true;
         q++;
      }
      tok = q;
      while (*q && *q != ' ' && *q != ':')
         q++;
      len = (unsigned)(q - tok);
      if (!len)
         break;
      if (np >= nargs || np >= 8)
         return false;
      at = rslc_p_node(p, args[np])->type;
      if (!rslc_bsig_match_tok(p, tok, len, at, &n, &m, &x, &conv, allow_conv && !is_out, &sbase))
         return false;
      ptypes[np] = rslc_bsig_param_type(p, tok, len, n, m, x, at);
      outp[np]   = is_out;
      np++;
   }
   if (*q != ':')
      return false;
   q++;
   rtok = q;
   while (*q && *q != ';')
      q++;
   rlen = (unsigned)(q - rtok);
   for (k = 0; k < np; k++)
      if (!outp[k] && !rslc_p_coerce(p, &args[k], ptypes[k], bs->name))
         return false;
   *ret = rslc_bsig_result(p, rtok, rlen, n, m, x, sbase);
   if (!*ret)
      return rslc_p_fail(p, "internal: result of ", bs->name);
   return true;
}

static bool rslc_p_resolve_builtin(struct rslc_parser *p, const struct rslc_bsig *bs,
      uint32_t *args, uint32_t nargs, const struct rslc_type **ret, uint32_t line)
{
   const char *s;
   int         pass;

   if (bs->sigs[0] == '@')
   {
      /* outerProduct(vecC, vecR) -> matCxR...: column vec first. */
      const struct rslc_type *a, *b;
      if (nargs != 2)
         return rslc_p_fail(p, "wrong argument count for ", bs->name);
      a = rslc_p_node(p, args[0])->type;
      b = rslc_p_node(p, args[1])->type;
      if (a->kind != RSLC_TY_VEC || b->kind != RSLC_TY_VEC || a->base != RSLC_BASE_FLOAT
            || b->base != RSLC_BASE_FLOAT)
         return rslc_p_fail(p, "bad arguments for ", bs->name);
      *ret = rslc_ty_mat(p, b->rows, a->rows);
      return true;
   }

   /* Pass 0 wants an exact match.  Pass 1 admits implicit conversions
    * and, among the viable signatures, keeps the one no other beats:
    * A beats B when A converts a subset of the arguments B converts
    * (max(uint, int) is max(uint, uint), not max(float, float)); the
    * first viable signature stands when none dominates. */
   for (pass = 0; pass < 2; pass++)
   {
      const char *best      = NULL;
      uint32_t    best_mask = 0;
      s = bs->sigs;
      while (*s)
      {
         const char *q = s;
         unsigned    n = 0, m = 0, conv = 0, sbase = 0;
         const struct rslc_type *x = NULL;
         uint32_t    ai = 0;
         bool        ok = true;
         uint32_t    mask = 0;

         /* parameters */
         while (*q && *q != ':')
         {
            const char *tok;
            unsigned    len;
            bool        is_out = false;
            while (*q == ' ')
               q++;
            if (*q == 'o')
            {
               is_out = true;
               q++;
            }
            tok = q;
            while (*q && *q != ' ' && *q != ':')
               q++;
            len = (unsigned)(q - tok);
            if (!len)
               break;
            if (ai >= nargs || ai >= 8)
               ok = false;
            else if (ok)
            {
               const struct rslc_type *at = rslc_p_node(p, args[ai])->type;
               unsigned before = conv;
               if (!rslc_bsig_match_tok(p, tok, len, at, &n, &m, &x, &conv, pass == 1 && !is_out, &sbase))
                  ok = false;
               else if (conv != before)
                  mask |= 1u << ai;
            }
            ai++;
         }
         if (ok && ai == nargs)
         {
            if (!best || ((mask & ~best_mask) == 0 && mask != best_mask))
            {
               best      = s;
               best_mask = mask;
            }
         }
         while (*q && *q != ';')
            q++;
         if (*q == ';')
            q++;
         s = q;
      }
      if (best)
         return rslc_p_apply_sig(p, bs, best, args, nargs, pass == 1, ret);
   }
   rslc_err(p->c, "no matching overload for ", bs->name, line);
   return false;
}

/* ---- expressions ---- */

static uint32_t rslc_p_expr(struct rslc_parser *p);
static uint32_t rslc_p_assign_expr(struct rslc_parser *p);
static uint32_t rslc_p_cond_expr(struct rslc_parser *p);
static const struct rslc_type *rslc_p_type_spec(struct rslc_parser *p,
      struct rslc_quals *q, bool allow_struct_def);
static const struct rslc_type *rslc_p_array_suffix(struct rslc_parser *p,
      const struct rslc_type *ty);

static bool rslc_p_is_lvalue(struct rslc_parser *p, uint32_t e)
{
   struct rslc_node *n = rslc_p_node(p, e);
   switch (n->op)
   {
      case RSLC_OP_VAR:
      {
         struct rslc_var *v = p->c->prog.vars[n->a];
         if (v->storage == RSLC_ST_CONST || v->storage == RSLC_ST_IN
               || v->storage == RSLC_ST_SAMPLER || v->storage == RSLC_ST_BLOCK
               || v->storage == RSLC_ST_MEMBER)
            return false;
         if (v->storage == RSLC_ST_PARAM && v->dir == RSLC_DIR_IN)
            return true;   /* in params are writable locals */
         if (v->storage == RSLC_ST_BUILTIN)
            return v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_FRAGDEPTH
               || v->builtin == RSLC_BI_POINTSIZE;
         return true;
      }
      case RSLC_OP_MEMBER:
      case RSLC_OP_INDEX:
         return rslc_p_is_lvalue(p, n->a);
      case RSLC_OP_SWIZZLE:
      {
         unsigned cnt = n->aux & 3u, i, j;
         if (cnt == 0)
            cnt = 4;
         for (i = 0; i < cnt; i++)
            for (j = i + 1; j < cnt; j++)
               if (((n->aux >> (2 + 2 * i)) & 3u) == ((n->aux >> (2 + 2 * j)) & 3u))
                  return false;
         return rslc_p_is_lvalue(p, n->a);
      }
      default:
         return false;
   }
}

/* Number literal to a CONST node. */
static uint32_t rslc_p_number(struct rslc_parser *p)
{
   const struct rslc_tok *t   = P_TOK(p);
   const char            *s   = rslc_tok_text(p->c, t);
   uint32_t               n   = t->len;
   uint32_t               line = t->line;
   bool                   is_float = false;
   uint32_t               i;
   char                   buf[64];

   if (n >= sizeof(buf))
   {
      rslc_p_fail(p, "number literal too long", NULL);
      return 0;
   }
   for (i = 0; i < n; i++)
   {
      char ch = s[i];
      if (ch == '.' || ch == 'e' || ch == 'E' || ch == 'f' || ch == 'F')
      {
         if ((ch == 'e' || ch == 'E' || ch == 'f' || ch == 'F') && n > 1
               && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            continue;
         is_float = true;
      }
   }
   memcpy(buf, s, n);
   buf[n] = '\0';
   rslc_p_next(p);
   if (is_float)
   {
      char  *end;
      double d;
      /* strip f / F / lf suffixes */
      i = n;
      while (i && (buf[i - 1] == 'f' || buf[i - 1] == 'F' || buf[i - 1] == 'l' || buf[i - 1] == 'L'))
         i--;
      buf[i] = '\0';
      d = strtod(buf, &end);
      if (*end)
      {
         rslc_err(p->c, "bad float literal ", rslc_intern(p->c, s, n), line);
         return 0;
      }
      return rslc_p_const_float(p, d, line);
   }
   else
   {
      long v;
      bool is_uint = n && (s[n - 1] == 'u' || s[n - 1] == 'U');
      if (!rslc_pp_number(s, n, &v))
      {
         rslc_err(p->c, "bad integer literal", NULL, line);
         return 0;
      }
      if (is_uint)
         return rslc_p_const_uint(p, (uint32_t)v, line);
      return rslc_p_const_int(p, (int32_t)v, line);
   }
}

/* A growable id array in scratch, for argument and statement lists
 * that become node lists once complete. */
struct rslc_ids
{
   uint32_t *v;
   uint32_t  n;
   uint32_t  cap;
};

static bool rslc_ids_push(struct rslc_parser *p, struct rslc_ids *a, uint32_t id)
{
   if (a->n == a->cap)
   {
      uint32_t  ncap = a->cap ? a->cap * 2 : 16;
      uint32_t *nv   = (uint32_t*)rslc_scratch_alloc(p->c, ncap * sizeof(*nv));
      if (!nv)
         return false;
      if (a->n)
         memcpy(nv, a->v, a->n * sizeof(*nv));
      a->v   = nv;
      a->cap = ncap;
   }
   a->v[a->n++] = id;
   return true;
}

/* Argument list after '(' has been consumed. */
static bool rslc_p_args(struct rslc_parser *p, struct rslc_ids *args)
{
   args->v = NULL; args->n = 0; args->cap = 0;
   if (rslc_p_accept(p, RSLC_P_RPAREN))
      return true;
   if (rslc_p_is_kw(p, RSLC_KW_VOID) && rslc_tok_is_punct(P_PEEK(p, 1), RSLC_P_RPAREN))
   {
      rslc_p_next(p);
      rslc_p_next(p);
      return true;
   }
   for (;;)
   {
      uint32_t e = rslc_p_assign_expr(p);
      if (!e)
         return false;
      if (!rslc_ids_push(p, args, e))
         return false;
      if (rslc_p_accept(p, RSLC_P_COMMA))
         continue;
      return rslc_p_expect(p, RSLC_P_RPAREN, "')'");
   }
}

/* Constructor: type(args). */
static uint32_t rslc_p_construct(struct rslc_parser *p, const struct rslc_type *ty,
      uint32_t *args, uint32_t nargs, uint32_t line)
{
   uint32_t  i;
   unsigned  need, have = 0;
   uint8_t   prec = 0;

   for (i = 0; i < nargs; i++)
      prec = rslc_prec_max(prec, rslc_p_node(p, args[i])->prec);

   switch (ty->kind)
   {
      case RSLC_TY_SCALAR:
      {
         const struct rslc_type *at;
         if (nargs != 1)
         {
            rslc_err(p->c, "scalar constructor takes one argument", NULL, line);
            return 0;
         }
         at = rslc_p_node(p, args[0])->type;
         if (at->kind != RSLC_TY_SCALAR && at->kind != RSLC_TY_VEC && at->kind != RSLC_TY_MAT)
         {
            rslc_err(p->c, "bad scalar constructor argument", NULL, line);
            return 0;
         }
         if (at == ty)
            return args[0];
         break;
      }
      case RSLC_TY_VEC:
      case RSLC_TY_MAT:
         need = rslc_ty_components(ty);
         if (nargs == 0)
         {
            rslc_err(p->c, "constructor needs arguments", NULL, line);
            return 0;
         }
         if (nargs == 1 && rslc_p_node(p, args[0])->type->kind == RSLC_TY_MAT
               && ty->kind == RSLC_TY_MAT)
            break;   /* matrix from matrix */
         if (nargs == 1 && rslc_ty_components(rslc_p_node(p, args[0])->type) == 1)
            break;   /* splat / diagonal */
         for (i = 0; i < nargs; i++)
         {
            const struct rslc_type *at = rslc_p_node(p, args[i])->type;
            if (at->kind != RSLC_TY_SCALAR && at->kind != RSLC_TY_VEC && at->kind != RSLC_TY_MAT)
            {
               rslc_err(p->c, "bad constructor argument", NULL, line);
               return 0;
            }
            if (have >= need)
            {
               rslc_err(p->c, "too many constructor arguments", NULL, line);
               return 0;
            }
            have += rslc_ty_components(at);
         }
         if (have < need)
         {
            rslc_err(p->c, "too few constructor arguments", NULL, line);
            return 0;
         }
         break;
      case RSLC_TY_STRUCT:
         if (nargs != ty->nfields)
         {
            rslc_err(p->c, "struct constructor argument count", NULL, line);
            return 0;
         }
         for (i = 0; i < nargs; i++)
            if (!rslc_p_coerce(p, &args[i], ty->fields[i].type, "struct constructor"))
               return 0;
         break;
      case RSLC_TY_ARRAY:
         if (ty->array_len && nargs != ty->array_len)
         {
            rslc_err(p->c, "array constructor argument count", NULL, line);
            return 0;
         }
         for (i = 0; i < nargs; i++)
            if (!rslc_p_coerce(p, &args[i], ty->elem, "array constructor"))
               return 0;
         if (!ty->array_len)
            ty = rslc_type_array(p->c, ty->elem, nargs);
         break;
      default:
         rslc_err(p->c, "cannot construct this type", NULL, line);
         return 0;
   }
   return rslc_p_mk(p, RSLC_OP_CONSTRUCT, ty, 0, rslc_list_new(p->c, args, nargs), 0, 0,
         line, prec);
}

static uint32_t rslc_p_call(struct rslc_parser *p, struct rslc_sym *name, uint32_t line)
{
   struct rslc_ids   al;
   struct rslc_mark  mark;
   uint32_t         *args;
   uint32_t          nargs;
   struct rslc_decl *d;
   uint32_t          r = 0;

   rslc_scratch_mark(p->c, &mark);
   if (!rslc_p_args(p, &al))
      return 0;
   args  = al.v;
   nargs = al.n;

   /* User functions shadow builtins. */
   d = name->decl;
   if (d && d->kind == RSLC_DK_FUNC)
   {
      struct rslc_func *f;
      struct rslc_func *best = NULL;
      unsigned          best_conv = 0;
      int               pass;
      uint8_t           prec = 0;
      uint32_t          i;

      for (pass = 0; pass < 2 && !best; pass++)
      {
         for (f = (struct rslc_func*)d->ptr; f; f = f->next)
         {
            unsigned conv = 0;
            bool     ok   = f->nparams == nargs;
            for (i = 0; ok && i < nargs; i++)
            {
               const struct rslc_type *at = rslc_p_node(p, args[i])->type;
               const struct rslc_type *pt = f->params[i]->type;
               if (at == pt)
                  continue;
               if (pass == 1 && f->params[i]->dir == RSLC_DIR_IN && rslc_ty_converts(at, pt))
                  conv++;
               else
                  ok = false;
            }
            if (ok && (!best || conv < best_conv))
            {
               best      = f;
               best_conv = conv;
            }
         }
      }
      if (!best)
      {
         rslc_err(p->c, "no matching overload for ", name->str, line);
         return 0;
      }
      for (i = 0; i < nargs; i++)
      {
         if (best->params[i]->dir != RSLC_DIR_IN)
         {
            if (!rslc_p_is_lvalue(p, args[i]))
            {
               rslc_err(p->c, "out argument is not an lvalue in call to ", name->str, line);
               return 0;
            }
         }
         else if (!rslc_p_coerce(p, &args[i], best->params[i]->type, name->str))
            return 0;
         prec = rslc_prec_max(prec, rslc_p_node(p, args[i])->prec);
      }
      r = rslc_p_mk(p, RSLC_OP_CALL, best->ret, best->id, rslc_list_new(p->c, args, nargs),
            0, 0, line, prec);
      rslc_scratch_release(p->c, &mark);
      return r;
   }
   if (name->bfn)
   {
      const struct rslc_bsig *bs = rslc_bsig_by_id(name->bfn);
      const struct rslc_type *ret = NULL;
      uint8_t  prec = 0;
      uint32_t i;
      if (!rslc_p_resolve_builtin(p, bs, args, nargs, &ret, line))
         return 0;
      for (i = 0; i < nargs; i++)
         prec = rslc_prec_max(prec, rslc_p_node(p, args[i])->prec);
      r = rslc_p_mk(p, RSLC_OP_BUILTIN, ret, name->bfn, rslc_list_new(p->c, args, nargs),
            0, 0, line, prec);
      rslc_scratch_release(p->c, &mark);
      return r;
   }
   rslc_err(p->c, "call of undeclared function ", name->str, line);
   return 0;
}

static unsigned rslc_swizzle_index(char ch)
{
   switch (ch)
   {
      case 'x': case 'r': case 's': return 0;
      case 'y': case 'g': case 't': return 1;
      case 'z': case 'b': case 'p': return 2;
      case 'w': case 'a': case 'q': return 3;
   }
   return 4;
}

static uint32_t rslc_p_postfix_ops(struct rslc_parser *p, uint32_t e);

static uint8_t rslc_p_default_prec(struct rslc_parser *p, const struct rslc_type *t,
      uint8_t explicit_prec);

static uint32_t rslc_p_primary(struct rslc_parser *p)
{
   const struct rslc_tok *t    = P_TOK(p);
   uint32_t               line = t->line;

   if (t->kind == RSLC_TK_NUMBER)
      return rslc_p_number(p);
   if (rslc_tok_is_punct(t, RSLC_P_LPAREN))
   {
      uint32_t e;
      rslc_p_next(p);
      e = rslc_p_expr(p);
      if (!e || !rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
         return 0;
      return e;
   }
   if (t->kind == RSLC_TK_IDENT)
   {
      struct rslc_sym *sym = rslc_tok_sym(p->c, t);
      if (sym->kw == RSLC_KW_TRUE || sym->kw == RSLC_KW_FALSE)
      {
         rslc_p_next(p);
         return rslc_p_const_bool(p, sym->kw == RSLC_KW_TRUE, line);
      }
      /* Type name: constructor, with optional [N] for arrays. */
      if ((sym->kw >= RSLC_KW_VOID && sym->kw <= RSLC_KW_USAMPLER2D)
            || (sym->decl && sym->decl->kind == RSLC_DK_TYPE))
      {
         struct rslc_quals q;
         const struct rslc_type *ty;
         struct rslc_ids   al;
         struct rslc_mark  mark;
         uint32_t          r;
         memset(&q, 0, sizeof(q));
         ty = rslc_p_type_spec(p, &q, false);
         if (!ty)
            return 0;
         ty = rslc_p_array_suffix(p, ty);
         if (!ty)
            return 0;
         if (!rslc_p_expect(p, RSLC_P_LPAREN, "'(' after type"))
            return 0;
         rslc_scratch_mark(p->c, &mark);
         if (!rslc_p_args(p, &al))
            return 0;
         r = rslc_p_construct(p, ty, al.v, al.n, line);
         rslc_scratch_release(p->c, &mark);
         return r;
      }
      rslc_p_next(p);
      if (rslc_p_accept(p, RSLC_P_LPAREN))
         return rslc_p_call(p, sym, line);
      if (sym->decl && sym->decl->kind == RSLC_DK_VAR)
      {
         struct rslc_var *v = (struct rslc_var*)sym->decl->ptr;
         uint32_t e;
         if (v->storage == RSLC_ST_BUILTIN && !v->precision)
            v->precision = rslc_p_default_prec(p, v->type, 0);   /* as glslang: the default applies */
         e = rslc_p_mk(p, RSLC_OP_VAR, v->type, v->id, 0, 0, 0, line, v->precision);
         if (v->storage == RSLC_ST_MEMBER)
         {
            /* A bare block member reads through the block. */
            struct rslc_blockdecl *b = &p->c->prog.blocks[v->block];
            const struct rslc_field *f = &b->type->fields[v->member];
            rslc_p_node(p, e)->type = f->type;
         }
         return e;
      }
      rslc_err(p->c, "undeclared identifier ", sym->str, line);
      return 0;
   }
   if (t->kind == RSLC_TK_EOF)
      return rslc_p_fail(p, "unexpected end of source", NULL), 0;
   return rslc_p_fail(p, "unexpected token in expression: ", rslc_p_here(p)), 0;
}

static uint32_t rslc_p_postfix(struct rslc_parser *p)
{
   uint32_t e = rslc_p_primary(p);
   if (!e)
      return 0;
   return rslc_p_postfix_ops(p, e);
}

static uint32_t rslc_p_postfix_ops(struct rslc_parser *p, uint32_t e)
{
   for (;;)
   {
      struct rslc_node *n    = rslc_p_node(p, e);
      uint32_t          line = P_LINE(p);

      if (rslc_p_accept(p, RSLC_P_LBRACKET))
      {
         uint32_t idx = rslc_p_expr(p);
         const struct rslc_type *bt = n->type;
         const struct rslc_type *rt;
         if (!idx || !rslc_p_expect(p, RSLC_P_RBRACKET, "']'"))
            return 0;
         if (!rslc_ty_is_integer(rslc_p_node(p, idx)->type)
               || rslc_p_node(p, idx)->type->kind != RSLC_TY_SCALAR)
         {
            rslc_err(p->c, "index is not an integer", NULL, line);
            return 0;
         }
         switch (bt->kind)
         {
            case RSLC_TY_ARRAY: rt = bt->elem; break;
            case RSLC_TY_VEC:   rt = rslc_ty_scalar_of(p, bt); break;
            case RSLC_TY_MAT:   rt = rslc_ty_vecn(p, RSLC_BASE_FLOAT, bt->rows); break;
            default:
               rslc_err(p->c, "indexing a non-aggregate", NULL, line);
               return 0;
         }
         e = rslc_p_mk(p, RSLC_OP_INDEX, rt, e, idx, 0, 0, line,
               rslc_prec_max(n->prec, rslc_p_node(p, idx)->prec));
         continue;
      }
      if (rslc_p_accept(p, RSLC_P_DOT))
      {
         const struct rslc_tok *ft = P_TOK(p);
         const struct rslc_type *bt = n->type;
         if (ft->kind != RSLC_TK_IDENT)
            return rslc_p_fail(p, "expected a field name", NULL), 0;
         rslc_p_next(p);
         if (bt->kind == RSLC_TY_STRUCT)
         {
            struct rslc_sym *fs = rslc_tok_sym(p->c, ft);
            uint32_t i;
            for (i = 0; i < bt->nfields; i++)
               if (bt->fields[i].name == fs)
                  break;
            if (i == bt->nfields)
            {
               rslc_err(p->c, "no such field ", fs->str, line);
               return 0;
            }
            e = rslc_p_mk(p, RSLC_OP_MEMBER, bt->fields[i].type, e, i, 0, 0, line,
                  rslc_prec_max(n->prec, bt->fields[i].precision));
            continue;
         }
         if (bt->kind == RSLC_TY_ARRAY && ft->len == 6
               && !memcmp(rslc_tok_text(p->c, ft), "length", 6))
         {
            if (!rslc_p_expect(p, RSLC_P_LPAREN, "'('") || !rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
               return 0;
            e = rslc_p_const_int(p, (int32_t)bt->array_len, line);
            continue;
         }
         if (bt->kind == RSLC_TY_SCALAR || bt->kind == RSLC_TY_VEC)
         {
            const char *s   = rslc_tok_text(p->c, ft);
            unsigned    cnt = ft->len;
            unsigned    i;
            uint16_t    aux;
            unsigned    width = bt->kind == RSLC_TY_SCALAR ? 1 : bt->rows;
            if (cnt < 1 || cnt > 4)
            {
               rslc_err(p->c, "bad swizzle ", rslc_intern(p->c, s, cnt), line);
               return 0;
            }
            aux = (uint16_t)(cnt & 3u);
            for (i = 0; i < cnt; i++)
            {
               unsigned k = rslc_swizzle_index(s[i]);
               if (k >= width)
               {
                  rslc_err(p->c, "bad swizzle ", rslc_intern(p->c, s, cnt), line);
                  return 0;
               }
               aux = (uint16_t)(aux | (k << (2 + 2 * i)));
            }
            e = rslc_p_mk(p, RSLC_OP_SWIZZLE, rslc_ty_vecn(p, bt->base, cnt), e, 0, 0, aux,
                  line, n->prec);
            continue;
         }
         rslc_err(p->c, "field access on a non-aggregate", NULL, line);
         return 0;
      }
      if (rslc_p_is(p, RSLC_P_INC) || rslc_p_is(p, RSLC_P_DEC))
      {
         uint16_t un = rslc_p_is(p, RSLC_P_INC) ? RSLC_UN_POSTINC : RSLC_UN_POSTDEC;
         rslc_p_next(p);
         if (!rslc_p_is_lvalue(p, e) || !rslc_ty_is_numeric(n->type))
         {
            rslc_err(p->c, "++/-- needs a numeric lvalue", NULL, line);
            return 0;
         }
         e = rslc_p_mk(p, RSLC_OP_UNARY, n->type, e, 0, 0, un, line, n->prec);
         continue;
      }
      return e;
   }
}

static uint32_t rslc_p_unary(struct rslc_parser *p)
{
   uint32_t line = P_LINE(p);
   uint16_t un   = 0;

   if (rslc_p_accept(p, RSLC_P_MINUS))     un = RSLC_UN_NEG;
   else if (rslc_p_accept(p, RSLC_P_PLUS)) return rslc_p_unary(p);
   else if (rslc_p_accept(p, RSLC_P_BANG)) un = RSLC_UN_NOT;
   else if (rslc_p_accept(p, RSLC_P_TILDE)) un = RSLC_UN_BITNOT;
   else if (rslc_p_accept(p, RSLC_P_INC))  un = RSLC_UN_PREINC;
   else if (rslc_p_accept(p, RSLC_P_DEC))  un = RSLC_UN_PREDEC;
   else
      return rslc_p_postfix(p);

   {
      uint32_t          e = rslc_p_unary(p);
      struct rslc_node *n;
      if (!e)
         return 0;
      n = rslc_p_node(p, e);
      switch (un)
      {
         case RSLC_UN_NEG:
            if (!rslc_ty_is_numeric(n->type))
               return rslc_p_fail(p, "unary - on a non-numeric", NULL), 0;
            if (n->op == RSLC_OP_CONST && n->type->kind == RSLC_TY_SCALAR)
            {
               if (n->type->base == RSLC_BASE_FLOAT)
                  return rslc_p_const_float(p, -rslc_p_const_double(n), line);
               if (n->type->base == RSLC_BASE_INT)
                  return rslc_p_const_int(p, -(int32_t)n->a, line);
               return rslc_p_const_uint(p, (uint32_t)(-(int32_t)n->a), line);
            }
            break;
         case RSLC_UN_NOT:
            if (n->type != p->t_bool)
               return rslc_p_fail(p, "! on a non-bool", NULL), 0;
            break;
         case RSLC_UN_BITNOT:
            if (!rslc_ty_is_integer(n->type))
               return rslc_p_fail(p, "~ on a non-integer", NULL), 0;
            break;
         default:
            if (!rslc_p_is_lvalue(p, e) || !rslc_ty_is_numeric(n->type))
               return rslc_p_fail(p, "++/-- needs a numeric lvalue", NULL), 0;
            break;
      }
      return rslc_p_mk(p, RSLC_OP_UNARY, n->type, e, 0, 0, un, line, n->prec);
   }
}

/* Result type of a binary arithmetic / comparison, with the operand
 * conversions applied.  NULL on a type error. */
static const struct rslc_type *rslc_p_binary_type(struct rslc_parser *p, uint16_t op,
      uint32_t *a, uint32_t *b, uint32_t line)
{
   const struct rslc_type *ta = rslc_p_node(p, *a)->type;
   const struct rslc_type *tb = rslc_p_node(p, *b)->type;

   switch (op)
   {
      case RSLC_BIN_AND: case RSLC_BIN_OR: case RSLC_BIN_XOR:
         if (ta != p->t_bool || tb != p->t_bool)
         {
            rslc_err(p->c, "logical operator on non-bool", NULL, line);
            return NULL;
         }
         return p->t_bool;
      case RSLC_BIN_EQ: case RSLC_BIN_NE:
         if (ta != tb)
         {
            if (rslc_ty_converts(ta, tb))
               *a = rslc_p_convert(p, *a, tb);
            else if (rslc_ty_converts(tb, ta))
               *b = rslc_p_convert(p, *b, ta);
            else
            {
               rslc_err(p->c, "comparison of different types", NULL, line);
               return NULL;
            }
         }
         return p->t_bool;
      case RSLC_BIN_LT: case RSLC_BIN_GT: case RSLC_BIN_LE: case RSLC_BIN_GE:
         if (ta->kind != RSLC_TY_SCALAR || tb->kind != RSLC_TY_SCALAR
               || ta->base == RSLC_BASE_BOOL || tb->base == RSLC_BASE_BOOL)
         {
            rslc_err(p->c, "relational operator needs numeric scalars", NULL, line);
            return NULL;
         }
         if (ta != tb)
         {
            if (rslc_ty_converts(ta, tb))
               *a = rslc_p_convert(p, *a, tb);
            else if (rslc_ty_converts(tb, ta))
               *b = rslc_p_convert(p, *b, ta);
            else
            {
               rslc_err(p->c, "comparison of different types", NULL, line);
               return NULL;
            }
         }
         return p->t_bool;
      default:
         break;
   }

   if (!rslc_ty_is_numeric(ta) || !rslc_ty_is_numeric(tb))
   {
      rslc_err(p->c, "arithmetic on a non-numeric", NULL, line);
      return NULL;
   }
   if (op == RSLC_BIN_MOD || op == RSLC_BIN_LSHIFT || op == RSLC_BIN_RSHIFT
         || op == RSLC_BIN_BAND || op == RSLC_BIN_BOR || op == RSLC_BIN_BXOR)
   {
      if (!rslc_ty_is_integer(ta) || !rslc_ty_is_integer(tb))
      {
         rslc_err(p->c, "bitwise operator on a non-integer", NULL, line);
         return NULL;
      }
      if (op == RSLC_BIN_LSHIFT || op == RSLC_BIN_RSHIFT)
      {
         /* Shift: result has the left type; the right may be any
          * integer scalar or a vector of the same width. */
         if (tb->kind == RSLC_TY_VEC && (ta->kind != RSLC_TY_VEC || ta->rows != tb->rows))
         {
            rslc_err(p->c, "shift width mismatch", NULL, line);
            return NULL;
         }
         return ta;
      }
   }

   /* Base unification: int -> uint -> float. */
   if (ta->base != tb->base)
   {
      const struct rslc_type *ca, *cb;
      enum rslc_base base = (ta->base == RSLC_BASE_FLOAT || tb->base == RSLC_BASE_FLOAT)
         ? RSLC_BASE_FLOAT : RSLC_BASE_UINT;
      ca = rslc_type_basic(p->c, ta->kind, base, ta->rows, ta->cols);
      cb = rslc_type_basic(p->c, tb->kind, base, tb->rows, tb->cols);
      if (!rslc_ty_converts(ta, ca) || !rslc_ty_converts(tb, cb))
      {
         rslc_err(p->c, "operand types do not convert", NULL, line);
         return NULL;
      }
      *a = rslc_p_convert(p, *a, ca);
      *b = rslc_p_convert(p, *b, cb);
      ta = ca;
      tb = cb;
   }

   /* Shapes. */
   if (ta == tb)
   {
      if (ta->kind == RSLC_TY_MAT && op == RSLC_BIN_MUL)
         return rslc_ty_mat(p, tb->cols, ta->rows);
      return ta;
   }
   if (ta->kind == RSLC_TY_SCALAR)
      return tb;
   if (tb->kind == RSLC_TY_SCALAR)
      return ta;
   if (op == RSLC_BIN_MUL)
   {
      if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_VEC && ta->cols == tb->rows)
         return rslc_ty_vecn(p, RSLC_BASE_FLOAT, ta->rows);
      if (ta->kind == RSLC_TY_VEC && tb->kind == RSLC_TY_MAT && ta->rows == tb->rows)
         return rslc_ty_vecn(p, RSLC_BASE_FLOAT, tb->cols);
      if (ta->kind == RSLC_TY_MAT && tb->kind == RSLC_TY_MAT && ta->cols == tb->rows)
         return rslc_ty_mat(p, tb->cols, ta->rows);
   }
   rslc_err(p->c, "operand shapes do not match", NULL, line);
   return NULL;
}

static uint32_t rslc_p_fold_binary(struct rslc_parser *p, uint32_t e)
{
   struct rslc_node *n = rslc_p_node(p, e);
   struct rslc_cval  v;
   if (!e || p->c->failed)
      return 0;
   if (rslc_p_nonconstant(p, n->a) || rslc_p_nonconstant(p, n->b))
   {
      n->nonconstant = 1;
      return e;
   }
   if (n->type->kind != RSLC_TY_SCALAR)
      return e;
   if (rslc_p_node(p, n->a)->op != RSLC_OP_CONST || rslc_p_node(p, n->b)->op != RSLC_OP_CONST)
      return e;
   if (!rslc_p_eval(p, e, &v))
      return e;
   if (n->type->base == RSLC_BASE_FLOAT)
      return rslc_p_const_float(p, v.d[0], n->line);
   return rslc_p_mk(p, RSLC_OP_CONST, n->type, v.v[0], 0, 0, 0, n->line, 0);
}

static int rslc_p_binop_prec(const struct rslc_tok *t, uint16_t *op)
{
   if (t->kind != RSLC_TK_PUNCT)
      return 0;
   switch (t->punct)
   {
      case RSLC_P_STAR:    *op = RSLC_BIN_MUL;    return 11;
      case RSLC_P_SLASH:   *op = RSLC_BIN_DIV;    return 11;
      case RSLC_P_PERCENT: *op = RSLC_BIN_MOD;    return 11;
      case RSLC_P_PLUS:    *op = RSLC_BIN_ADD;    return 10;
      case RSLC_P_MINUS:   *op = RSLC_BIN_SUB;    return 10;
      case RSLC_P_LSHIFT:  *op = RSLC_BIN_LSHIFT; return 9;
      case RSLC_P_RSHIFT:  *op = RSLC_BIN_RSHIFT; return 9;
      case RSLC_P_LT:      *op = RSLC_BIN_LT;     return 8;
      case RSLC_P_GT:      *op = RSLC_BIN_GT;     return 8;
      case RSLC_P_LE:      *op = RSLC_BIN_LE;     return 8;
      case RSLC_P_GE:      *op = RSLC_BIN_GE;     return 8;
      case RSLC_P_EQ:      *op = RSLC_BIN_EQ;     return 7;
      case RSLC_P_NE:      *op = RSLC_BIN_NE;     return 7;
      case RSLC_P_AMP:     *op = RSLC_BIN_BAND;   return 6;
      case RSLC_P_CARET:   *op = RSLC_BIN_BXOR;   return 5;
      case RSLC_P_PIPE:    *op = RSLC_BIN_BOR;    return 4;
      case RSLC_P_AND:     *op = RSLC_BIN_AND;    return 3;
      case RSLC_P_XOR:     *op = RSLC_BIN_XOR;    return 2;
      case RSLC_P_OR:      *op = RSLC_BIN_OR;     return 1;
      default: return 0;
   }
}

static uint32_t rslc_p_binary(struct rslc_parser *p, int min_prec)
{
   uint32_t lhs = rslc_p_unary(p);
   if (!lhs)
      return 0;
   for (;;)
   {
      uint16_t op;
      int      prec = rslc_p_binop_prec(P_TOK(p), &op);
      uint32_t line = P_LINE(p);
      uint32_t rhs;
      const struct rslc_type *rt;
      if (!prec || prec < min_prec)
         return lhs;
      rslc_p_next(p);
      rhs = rslc_p_binary(p, prec + 1);
      if (!rhs)
         return 0;
      rt = rslc_p_binary_type(p, op, &lhs, &rhs, line);
      if (!rt)
         return 0;
      lhs = rslc_p_mk(p, RSLC_OP_BINARY, rt, lhs, rhs, 0, op, line,
            rslc_prec_max(rslc_p_node(p, lhs)->prec, rslc_p_node(p, rhs)->prec));
      lhs = rslc_p_fold_binary(p, lhs);
   }
}

static uint32_t rslc_p_cond_expr(struct rslc_parser *p)
{
   uint32_t cond = rslc_p_binary(p, 1);
   uint32_t line = P_LINE(p);
   if (!cond)
      return 0;
   if (rslc_p_accept(p, RSLC_P_QUESTION))
   {
      uint32_t a, b;
      const struct rslc_type *ta, *tb;
      if (rslc_p_node(p, cond)->type != p->t_bool)
         return rslc_p_fail(p, "?: condition is not a bool", NULL), 0;
      a = rslc_p_expr(p);
      if (!a || !rslc_p_expect(p, RSLC_P_COLON, "':'"))
         return 0;
      b = rslc_p_assign_expr(p);
      if (!b)
         return 0;
      ta = rslc_p_node(p, a)->type;
      tb = rslc_p_node(p, b)->type;
      if (ta != tb)
      {
         if (rslc_ty_converts(ta, tb))
            a = rslc_p_convert(p, a, tb);
         else if (rslc_ty_converts(tb, ta))
            b = rslc_p_convert(p, b, ta);
         else
         {
            rslc_err(p->c, "?: branches differ in type", NULL, line);
            return 0;
         }
      }
      return rslc_p_mk(p, RSLC_OP_TERNARY, rslc_p_node(p, a)->type, cond, a, b, 0, line,
            rslc_prec_max(rslc_p_node(p, a)->prec, rslc_p_node(p, b)->prec));
   }
   return cond;
}

static bool rslc_p_assign_op(const struct rslc_tok *t, uint16_t *op)
{
   if (t->kind != RSLC_TK_PUNCT)
      return false;
   switch (t->punct)
   {
      case RSLC_P_ASSIGN:        *op = RSLC_BIN_NONE;   return true;
      case RSLC_P_ADD_ASSIGN:    *op = RSLC_BIN_ADD;    return true;
      case RSLC_P_SUB_ASSIGN:    *op = RSLC_BIN_SUB;    return true;
      case RSLC_P_MUL_ASSIGN:    *op = RSLC_BIN_MUL;    return true;
      case RSLC_P_DIV_ASSIGN:    *op = RSLC_BIN_DIV;    return true;
      case RSLC_P_MOD_ASSIGN:    *op = RSLC_BIN_MOD;    return true;
      case RSLC_P_LSHIFT_ASSIGN: *op = RSLC_BIN_LSHIFT; return true;
      case RSLC_P_RSHIFT_ASSIGN: *op = RSLC_BIN_RSHIFT; return true;
      case RSLC_P_BAND_ASSIGN:   *op = RSLC_BIN_BAND;   return true;
      case RSLC_P_BOR_ASSIGN:    *op = RSLC_BIN_BOR;    return true;
      case RSLC_P_BXOR_ASSIGN:   *op = RSLC_BIN_BXOR;   return true;
      default: return false;
   }
}

static uint32_t rslc_p_assign_expr(struct rslc_parser *p)
{
   uint32_t lhs = rslc_p_cond_expr(p);
   uint16_t op;
   uint32_t line = P_LINE(p);
   uint32_t rhs;
   const struct rslc_type *lt;

   if (!lhs)
      return 0;
   if (!rslc_p_assign_op(P_TOK(p), &op))
      return lhs;
   rslc_p_next(p);
   if (!rslc_p_is_lvalue(p, lhs))
      return rslc_p_fail(p, "assignment to a non-lvalue", NULL), 0;
   rhs = rslc_p_assign_expr(p);
   if (!rhs)
      return 0;
   lt = rslc_p_node(p, lhs)->type;
   if (op == RSLC_BIN_NONE)
   {
      if (!rslc_p_coerce(p, &rhs, lt, "assignment"))
         return 0;
   }
   else
   {
      uint32_t tmp = lhs;
      const struct rslc_type *rt = rslc_p_binary_type(p, op, &tmp, &rhs, line);
      if (!rt)
         return 0;
      if (tmp != lhs)
      {
         rslc_err(p->c, "compound assignment would convert the left side", NULL, line);
         return 0;
      }
      if (rt != lt && !(lt->kind == RSLC_TY_MAT && rt->kind == RSLC_TY_MAT
               && op == RSLC_BIN_MUL && rt->rows == lt->rows && rt->cols == lt->cols))
      {
         rslc_err(p->c, "compound assignment result type mismatch", NULL, line);
         return 0;
      }
   }
   return rslc_p_mk(p, RSLC_OP_ASSIGN, lt, lhs, rhs, 0, op, line,
         rslc_prec_max(rslc_p_node(p, lhs)->prec, rslc_p_node(p, rhs)->prec));
}

static uint32_t rslc_p_expr(struct rslc_parser *p)
{
   uint32_t e = rslc_p_assign_expr(p);
   if (!e)
      return 0;
   while (rslc_p_is(p, RSLC_P_COMMA))
   {
      uint32_t line = P_LINE(p);
      uint32_t r;
      rslc_p_next(p);
      r = rslc_p_assign_expr(p);
      if (!r)
         return 0;
      e = rslc_p_mk(p, RSLC_OP_SEQ, rslc_p_node(p, r)->type, e, r, 0, 0, line,
            rslc_p_node(p, r)->prec);
   }
   return e;
}

/* ---- qualifiers and types ---- */

static bool rslc_p_layout(struct rslc_parser *p, struct rslc_quals *q)
{
   if (!rslc_p_expect(p, RSLC_P_LPAREN, "'(' after layout"))
      return false;
   for (;;)
   {
      const struct rslc_tok *t = P_TOK(p);
      const char            *s;
      uint32_t               len;
      uint32_t              *slot = NULL;
      if (t->kind != RSLC_TK_IDENT)
         return rslc_p_fail(p, "bad layout qualifier", NULL);
      s   = rslc_tok_text(p->c, t);
      len = t->len;
      rslc_p_next(p);
      if (len == 8 && !memcmp(s, "location", 8))      slot = &q->location;
      else if (len == 7 && !memcmp(s, "binding", 7))  slot = &q->binding;
      else if (len == 3 && !memcmp(s, "set", 3))      slot = &q->set;
      else if (len == 6 && !memcmp(s, "offset", 6))   slot = &q->offset;
      else if (len == 13 && !memcmp(s, "push_constant", 13)) q->push_constant = true;
      else if (len == 6 && !memcmp(s, "std140", 6))   q->std140 = true;
      else if (len == 6 && !memcmp(s, "std430", 6))   { }
      else if (len == 12 && !memcmp(s, "column_major", 12)) { }
      else if (len == 9 && !memcmp(s, "row_major", 9))
         return rslc_p_fail(p, "row_major is not supported", NULL);
      else if (len == 7 && !memcmp(s, "index", 5))    { }
      else
         return rslc_p_fail(p, "unknown layout qualifier ", rslc_intern(p->c, s, len));
      if (rslc_p_accept(p, RSLC_P_ASSIGN))
      {
         uint32_t e = rslc_p_cond_expr(p);
         int32_t  v;
         if (!e)
            return false;
         if (!rslc_p_eval_int(p, e, &v) || v < 0)
            return rslc_p_fail(p, "layout value must be a non-negative constant", NULL);
         if (slot)
            *slot = (uint32_t)v;
      }
      else if (slot)
         return rslc_p_fail(p, "layout qualifier needs a value", NULL);
      if (rslc_p_accept(p, RSLC_P_COMMA))
         continue;
      return rslc_p_expect(p, RSLC_P_RPAREN, "')'");
   }
}

/* Reads every qualifier ahead of a type.  Returns false on error. */
static bool rslc_p_quals(struct rslc_parser *p, struct rslc_quals *q)
{
   memset(q, 0, sizeof(*q));
   q->location = q->binding = q->set = q->offset = RSLC_UNSET;
   for (;;)
   {
      const struct rslc_tok *t = P_TOK(p);
      struct rslc_sym       *s;
      if (t->kind != RSLC_TK_IDENT)
         return true;
      s = rslc_tok_sym(p->c, t);
      switch (s->kw)
      {
         case RSLC_KW_LAYOUT:
            rslc_p_next(p);
            if (!rslc_p_layout(p, q))
               return false;
            continue;
         case RSLC_KW_CONST:   q->is_const = true; break;
         case RSLC_KW_IN:      q->is_in = true; break;
         case RSLC_KW_OUT:     q->is_out = true; break;
         case RSLC_KW_INOUT:   q->is_inout = true; break;
         case RSLC_KW_UNIFORM: q->is_uniform = true; break;
         case RSLC_KW_FLAT:    q->interp = RSLC_INTERP_FLAT; break;
         case RSLC_KW_SMOOTH:  q->interp = RSLC_INTERP_SMOOTH; break;
         case RSLC_KW_NOPERSPECTIVE: q->interp = RSLC_INTERP_NOPERSPECTIVE; break;
         case RSLC_KW_CENTROID:  q->vq |= RSLC_VQ_CENTROID; break;
         case RSLC_KW_SAMPLE:    q->vq |= RSLC_VQ_SAMPLE; break;
         case RSLC_KW_INVARIANT: q->vq |= RSLC_VQ_INVARIANT; break;
         case RSLC_KW_PRECISE:   q->vq |= RSLC_VQ_PRECISE; break;
         case RSLC_KW_LOWP:    q->precision = RSLC_PREC_LOW; break;
         case RSLC_KW_MEDIUMP: q->precision = RSLC_PREC_MEDIUM; break;
         case RSLC_KW_HIGHP:   q->precision = RSLC_PREC_HIGH; break;
         default:
            return true;
      }
      rslc_p_next(p);
   }
}

static const struct rslc_type *rslc_p_struct_def(struct rslc_parser *p);

/* A type specifier: keyword type or struct name, or an inline struct
 * definition when allowed.  Array suffixes belong to the declarator
 * and are handled by the caller, except in constructors. */
static const struct rslc_type *rslc_p_type_spec(struct rslc_parser *p,
      struct rslc_quals *q, bool allow_struct_def)
{
   const struct rslc_tok *t = P_TOK(p);
   struct rslc_sym       *s;
   (void)q;

   if (t->kind != RSLC_TK_IDENT)
   {
      rslc_p_fail(p, "expected a type", NULL);
      return NULL;
   }
   s = rslc_tok_sym(p->c, t);
   if (s->kw == RSLC_KW_STRUCT)
   {
      if (!allow_struct_def)
      {
         rslc_p_fail(p, "struct definition not allowed here", NULL);
         return NULL;
      }
      return rslc_p_struct_def(p);
   }
   if (s->kw >= RSLC_KW_VOID && s->kw <= RSLC_KW_USAMPLER2D)
   {
      const struct rslc_type *ty = NULL;
      unsigned k = s->kw;
      rslc_p_next(p);
      if (k == RSLC_KW_VOID)          ty = p->t_void;
      else if (k == RSLC_KW_BOOL)     ty = p->t_bool;
      else if (k == RSLC_KW_INT)      ty = p->t_int;
      else if (k == RSLC_KW_UINT)     ty = p->t_uint;
      else if (k == RSLC_KW_FLOAT)    ty = p->t_float;
      else if (k >= RSLC_KW_BVEC2 && k <= RSLC_KW_BVEC4)
         ty = rslc_ty_vecn(p, RSLC_BASE_BOOL, k - RSLC_KW_BVEC2 + 2);
      else if (k >= RSLC_KW_IVEC2 && k <= RSLC_KW_IVEC4)
         ty = rslc_ty_vecn(p, RSLC_BASE_INT, k - RSLC_KW_IVEC2 + 2);
      else if (k >= RSLC_KW_UVEC2 && k <= RSLC_KW_UVEC4)
         ty = rslc_ty_vecn(p, RSLC_BASE_UINT, k - RSLC_KW_UVEC2 + 2);
      else if (k >= RSLC_KW_VEC2 && k <= RSLC_KW_VEC4)
         ty = rslc_ty_vecn(p, RSLC_BASE_FLOAT, k - RSLC_KW_VEC2 + 2);
      else if (k >= RSLC_KW_MAT2 && k <= RSLC_KW_MAT4)
         ty = rslc_ty_mat(p, k - RSLC_KW_MAT2 + 2, k - RSLC_KW_MAT2 + 2);
      else if (k >= RSLC_KW_MAT2X2 && k <= RSLC_KW_MAT4X4)
      {
         unsigned i = k - RSLC_KW_MAT2X2;
         ty = rslc_ty_mat(p, i / 3 + 2, i % 3 + 2);
      }
      else if (k == RSLC_KW_SAMPLER2D)
         ty = p->t_s2d;
      else if (k == RSLC_KW_ISAMPLER2D)
         ty = rslc_type_sampler2d(p->c, RSLC_BASE_INT);
      else if (k == RSLC_KW_USAMPLER2D)
         ty = rslc_type_sampler2d(p->c, RSLC_BASE_UINT);
      return ty;
   }
   if (s->decl && s->decl->kind == RSLC_DK_TYPE)
   {
      rslc_p_next(p);
      return (const struct rslc_type*)s->decl->ptr;
   }
   rslc_p_fail(p, "expected a type, got ", s->str);
   return NULL;
}

/* Array suffixes after a declarator name: [N] [M] ...; outer first. */
static const struct rslc_type *rslc_p_array_suffix(struct rslc_parser *p,
      const struct rslc_type *ty)
{
   uint32_t dims[8];
   unsigned nd = 0, i;
   while (rslc_p_is(p, RSLC_P_LBRACKET))
   {
      uint32_t e;
      int32_t  len = 0;
      rslc_p_next(p);
      if (nd == 8)
      {
         rslc_p_fail(p, "too many array dimensions", NULL);
         return NULL;
      }
      if (!rslc_p_is(p, RSLC_P_RBRACKET))
      {
         e = rslc_p_cond_expr(p);
         if (!e)
            return NULL;
         if (!rslc_p_eval_int(p, e, &len) || len <= 0)
         {
            rslc_p_fail(p, "array size must be a positive constant", NULL);
            return NULL;
         }
      }
      if (!rslc_p_expect(p, RSLC_P_RBRACKET, "']'"))
         return NULL;
      dims[nd++] = (uint32_t)len;
   }
   for (i = nd; i > 0; i--)
      ty = rslc_type_array(p->c, ty, dims[i - 1]);
   return ty;
}

static const struct rslc_type *rslc_p_struct_def(struct rslc_parser *p)
{
   struct rslc_sym   *name = NULL;
   struct rslc_field *fields = NULL;
   uint32_t           nfields = 0, cap = 0;
   struct rslc_type  *st;
   uint32_t           line = P_LINE(p);

   rslc_p_next(p);   /* struct */
   if (P_TOK(p)->kind == RSLC_TK_IDENT && rslc_tok_sym(p->c, P_TOK(p))->kw == RSLC_KW_NONE)
   {
      name = rslc_tok_sym(p->c, P_TOK(p));
      rslc_p_next(p);
   }
   if (!rslc_p_expect(p, RSLC_P_LBRACE, "'{' after struct"))
      return NULL;
   while (!rslc_p_accept(p, RSLC_P_RBRACE))
   {
      struct rslc_quals q;
      const struct rslc_type *ft;
      if (!rslc_p_quals(p, &q))
         return NULL;
      ft = rslc_p_type_spec(p, &q, false);
      if (!ft)
         return NULL;
      for (;;)
      {
         const struct rslc_tok *nt = P_TOK(p);
         const struct rslc_type *mt;
         uint32_t i;
         if (nt->kind != RSLC_TK_IDENT)
         {
            rslc_p_fail(p, "expected a field name", NULL);
            return NULL;
         }
         rslc_p_next(p);
         mt = rslc_p_array_suffix(p, ft);
         if (!mt)
            return NULL;
         for (i = 0; i < nfields; i++)
            if (fields[i].name == rslc_tok_sym(p->c, nt))
            {
               rslc_err(p->c, "duplicate field ", rslc_tok_sym(p->c, nt)->str, nt->line);
               return NULL;
            }
         if (nfields == cap)
         {
            uint32_t           ncap = cap ? cap * 2 : 8;
            struct rslc_field *nf   = (struct rslc_field*)rslc_arena_zalloc(p->c,
                  ncap * sizeof(*nf));
            if (!nf)
               return NULL;
            if (nfields)
               memcpy(nf, fields, nfields * sizeof(*nf));
            fields = nf;
            cap    = ncap;
         }
         fields[nfields].type      = mt;
         fields[nfields].name      = rslc_tok_sym(p->c, nt);
         fields[nfields].line      = nt->line;
         fields[nfields].precision = q.precision ? q.precision
            : (mt->base == RSLC_BASE_FLOAT || mt->kind == RSLC_TY_MAT) ? p->scope->prec_float
            : rslc_ty_is_integer(mt) ? p->scope->prec_int : 0;
         nfields++;
         if (rslc_p_accept(p, RSLC_P_COMMA))
            continue;
         break;
      }
      if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
         return NULL;
   }
   if (!nfields)
   {
      rslc_err(p->c, "empty struct", NULL, line);
      return NULL;
   }
   st = rslc_type_struct(p->c, name, fields, nfields);
   if (!st)
      return NULL;
   if (name)
   {
      if (rslc_p_find_here(p, name))
      {
         rslc_err(p->c, "redeclaration of ", name->str, line);
         return NULL;
      }
      if (!rslc_p_bind(p, name, RSLC_DK_TYPE, st))
         return NULL;
   }
   {
      struct rslc_prog *pr = &p->c->prog;
      RSLC_GROW_PTRS(p->c, pr->structs, pr->nstructs, pr->structs_cap, struct rslc_type, NULL);
      pr->structs[pr->nstructs++] = st;
   }
   return st;
}

/* ---- statements ---- */

static uint32_t rslc_p_statement(struct rslc_parser *p);
static uint32_t rslc_p_declaration(struct rslc_parser *p, bool global);

static bool rslc_p_starts_declaration(struct rslc_parser *p)
{
   const struct rslc_tok *t = P_TOK(p);
   struct rslc_sym       *s;
   if (t->kind != RSLC_TK_IDENT)
      return false;
   s = rslc_tok_sym(p->c, t);
   if (s->kw >= RSLC_KW_CONST && s->kw <= RSLC_KW_STRUCT)
      return true;
   if (s->kw >= RSLC_KW_VOID && s->kw <= RSLC_KW_USAMPLER2D)
   {
      /* A type followed by '(' is a constructor expression; a type
       * followed by [..] is one when the brackets are followed by '('
       * and a declaration when a name follows. */
      const struct rslc_tok *n = P_PEEK(p, 1);
      if (rslc_tok_is_punct(n, RSLC_P_LBRACKET))
      {
         uint32_t k = 1;
         int depth = 0;
         for (;;)
         {
            n = P_PEEK(p, k);
            if (n->kind == RSLC_TK_EOF)
               return false;
            if (rslc_tok_is_punct(n, RSLC_P_LBRACKET))
               depth++;
            else if (rslc_tok_is_punct(n, RSLC_P_RBRACKET))
            {
               depth--;
               if (depth == 0 && !rslc_tok_is_punct(P_PEEK(p, k + 1), RSLC_P_LBRACKET))
                  return P_PEEK(p, k + 1)->kind == RSLC_TK_IDENT;
            }
            k++;
         }
      }
      return !(rslc_tok_is_punct(n, RSLC_P_LPAREN) || rslc_tok_is_punct(n, RSLC_P_DOT));
   }
   if (s->decl && s->decl->kind == RSLC_DK_TYPE)
   {
      const struct rslc_tok *n = P_PEEK(p, 1);
      return n->kind == RSLC_TK_IDENT;
   }
   return false;
}

static uint32_t rslc_p_block(struct rslc_parser *p, bool new_scope)
{
   uint32_t         line = P_LINE(p);
   struct rslc_ids  items;
   struct rslc_mark mark;
   uint32_t         list;

   items.v = NULL; items.n = 0; items.cap = 0;
   if (!rslc_p_expect(p, RSLC_P_LBRACE, "'{'"))
      return 0;
   if (new_scope && !rslc_p_push_scope(p))
      return 0;
   rslc_scratch_mark(p->c, &mark);
   while (!rslc_p_is(p, RSLC_P_RBRACE))
   {
      uint32_t s;
      if (P_TOK(p)->kind == RSLC_TK_EOF)
         return rslc_p_fail(p, "unexpected end of source in block", NULL), 0;
      s = rslc_p_statement(p);
      if (!s)
         return 0;
      if (!rslc_ids_push(p, &items, s))
         return 0;
   }
   rslc_p_next(p);
   if (new_scope)
      rslc_p_pop_scope(p);
   list = rslc_list_new(p->c, items.v, items.n);
   rslc_scratch_release(p->c, &mark);
   return rslc_p_mk(p, RSLC_OP_BLOCK, NULL, 0, list, 0, 0, line, 0);
}

static uint32_t rslc_p_cond_in_parens(struct rslc_parser *p)
{
   uint32_t e;
   if (!rslc_p_expect(p, RSLC_P_LPAREN, "'('"))
      return 0;
   e = rslc_p_expr(p);
   if (!e || !rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
      return 0;
   if (rslc_p_node(p, e)->type != p->t_bool)
      return rslc_p_fail(p, "condition is not a bool", NULL), 0;
   return e;
}

static uint32_t rslc_p_statement(struct rslc_parser *p)
{
   const struct rslc_tok *t    = P_TOK(p);
   uint32_t               line = t->line;
   struct rslc_sym       *s    = t->kind == RSLC_TK_IDENT ? rslc_tok_sym(p->c, t) : NULL;

   if (rslc_p_is(p, RSLC_P_LBRACE))
      return rslc_p_block(p, true);
   if (rslc_p_accept(p, RSLC_P_SEMI))
      return rslc_p_mk(p, RSLC_OP_BLOCK, NULL, 0, 0, 0, RSLC_BLOCK_GROUP, line, 0);

   if (s)
   {
      switch (s->kw)
      {
         case RSLC_KW_IF:
         {
            uint32_t cond, then, els = 0;
            rslc_p_next(p);
            cond = rslc_p_cond_in_parens(p);
            if (!cond)
               return 0;
            then = rslc_p_statement(p);
            if (!then)
               return 0;
            if (rslc_p_accept_kw(p, RSLC_KW_ELSE))
            {
               els = rslc_p_statement(p);
               if (!els)
                  return 0;
            }
            return rslc_p_mk(p, RSLC_OP_IF, NULL, cond, then, els, 0, line, 0);
         }
         case RSLC_KW_FOR:
         {
            uint32_t parts[4] = { 0, 0, 0, 0 };
            uint32_t list;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_LPAREN, "'(' after for"))
               return 0;
            if (!rslc_p_push_scope(p))
               return 0;
            if (!rslc_p_accept(p, RSLC_P_SEMI))
            {
               if (rslc_p_starts_declaration(p))
                  parts[0] = rslc_p_declaration(p, false);
               else
               {
                  uint32_t e = rslc_p_expr(p);
                  if (!e || !rslc_p_expect(p, RSLC_P_SEMI, "';'"))
                     return 0;
                  parts[0] = rslc_p_mk(p, RSLC_OP_EXPR, NULL, e, 0, 0, 0, line, 0);
               }
               if (!parts[0])
                  return 0;
            }
            if (!rslc_p_is(p, RSLC_P_SEMI))
            {
               parts[1] = rslc_p_expr(p);
               if (!parts[1])
                  return 0;
               if (rslc_p_node(p, parts[1])->type != p->t_bool)
                  return rslc_p_fail(p, "for condition is not a bool", NULL), 0;
            }
            if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            if (!rslc_p_is(p, RSLC_P_RPAREN))
            {
               parts[2] = rslc_p_expr(p);
               if (!parts[2])
                  return 0;
            }
            if (!rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
               return 0;
            p->loop_depth++;
            parts[3] = rslc_p_statement(p);
            p->loop_depth--;
            rslc_p_pop_scope(p);
            if (!parts[3])
               return 0;
            list = rslc_list_new(p->c, parts, 4);
            return rslc_p_mk(p, RSLC_OP_FOR, NULL, 0, list, 0, 0, line, 0);
         }
         case RSLC_KW_WHILE:
         {
            uint32_t cond, body;
            rslc_p_next(p);
            cond = rslc_p_cond_in_parens(p);
            if (!cond)
               return 0;
            p->loop_depth++;
            body = rslc_p_statement(p);
            p->loop_depth--;
            if (!body)
               return 0;
            return rslc_p_mk(p, RSLC_OP_WHILE, NULL, cond, body, 0, 0, line, 0);
         }
         case RSLC_KW_DO:
         {
            uint32_t cond, body;
            rslc_p_next(p);
            p->loop_depth++;
            body = rslc_p_statement(p);
            p->loop_depth--;
            if (!body)
               return 0;
            if (!rslc_p_accept_kw(p, RSLC_KW_WHILE))
               return rslc_p_fail(p, "expected while after do body", NULL), 0;
            cond = rslc_p_cond_in_parens(p);
            if (!cond || !rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_DO, NULL, body, cond, 0, 0, line, 0);
         }
         case RSLC_KW_SWITCH:
         {
            uint32_t e, body;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_LPAREN, "'('"))
               return 0;
            e = rslc_p_expr(p);
            if (!e || !rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
               return 0;
            if (rslc_p_node(p, e)->type->kind != RSLC_TY_SCALAR
                  || !rslc_ty_is_integer(rslc_p_node(p, e)->type))
               return rslc_p_fail(p, "switch needs an integer", NULL), 0;
            p->switch_depth++;
            body = rslc_p_block(p, true);
            p->switch_depth--;
            if (!body)
               return 0;
            return rslc_p_mk(p, RSLC_OP_SWITCH, NULL, e, rslc_p_node(p, body)->b, 0, 0, line, 0);
         }
         case RSLC_KW_CASE:
         {
            uint32_t e;
            int32_t  v;
            if (!p->switch_depth)
               return rslc_p_fail(p, "case outside switch", NULL), 0;
            rslc_p_next(p);
            e = rslc_p_expr(p);
            if (!e || !rslc_p_expect(p, RSLC_P_COLON, "':'"))
               return 0;
            if (!rslc_p_eval_int(p, e, &v))
               return rslc_p_fail(p, "case label is not a constant", NULL), 0;
            return rslc_p_mk(p, RSLC_OP_CASE, NULL, rslc_p_const_int(p, v, line), 0, 0, 0, line, 0);
         }
         case RSLC_KW_DEFAULT:
            if (!p->switch_depth)
               return rslc_p_fail(p, "default outside switch", NULL), 0;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_COLON, "':'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_CASE, NULL, 0, 0, 0, 0, line, 0);
         case RSLC_KW_BREAK:
            if (!p->loop_depth && !p->switch_depth)
               return rslc_p_fail(p, "break outside loop or switch", NULL), 0;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_BREAK, NULL, 0, 0, 0, 0, line, 0);
         case RSLC_KW_CONTINUE:
            if (!p->loop_depth)
               return rslc_p_fail(p, "continue outside loop", NULL), 0;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_CONTINUE, NULL, 0, 0, 0, 0, line, 0);
         case RSLC_KW_DISCARD:
            if (p->c->stage != RSLC_STAGE_FRAGMENT)
               return rslc_p_fail(p, "discard outside a fragment shader", NULL), 0;
            rslc_p_next(p);
            if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_DISCARD, NULL, 0, 0, 0, 0, line, 0);
         case RSLC_KW_RETURN:
         {
            uint32_t e = 0;
            rslc_p_next(p);
            if (!rslc_p_is(p, RSLC_P_SEMI))
            {
               e = rslc_p_expr(p);
               if (!e)
                  return 0;
               if (p->cur_fn->ret == p->t_void)
                  return rslc_p_fail(p, "return with a value in a void function", NULL), 0;
               if (!rslc_p_coerce(p, &e, p->cur_fn->ret, "return"))
                  return 0;
            }
            else if (p->cur_fn->ret != p->t_void)
               return rslc_p_fail(p, "return without a value", NULL), 0;
            if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
               return 0;
            return rslc_p_mk(p, RSLC_OP_RETURN, NULL, e, 0, 0, 0, line, 0);
         }
         default:
            break;
      }
   }
   if (rslc_p_starts_declaration(p))
      return rslc_p_declaration(p, false);
   {
      uint32_t e = rslc_p_expr(p);
      if (!e || !rslc_p_expect(p, RSLC_P_SEMI, "';'"))
         return 0;
      return rslc_p_mk(p, RSLC_OP_EXPR, NULL, e, 0, 0, 0, line, 0);
   }
}

/* ---- declarations ---- */

static uint8_t rslc_p_default_prec(struct rslc_parser *p, const struct rslc_type *t,
      uint8_t explicit_prec)
{
   if (explicit_prec)
      return explicit_prec;
   if (t->kind == RSLC_TY_ARRAY)
      return rslc_p_default_prec(p, t->elem, 0);
   if (t->kind == RSLC_TY_SAMPLER2D)
      return 0;   /* sampler precision is its own default, never float's */
   if (t->kind == RSLC_TY_MAT || t->base == RSLC_BASE_FLOAT)
      return p->scope->prec_float;
   if (t->base == RSLC_BASE_INT || t->base == RSLC_BASE_UINT)
      return p->scope->prec_int;
   return 0;
}

/* { a, b, ... } for an array or struct of type ty; nests. */
static uint32_t rslc_p_init_list(struct rslc_parser *p, const struct rslc_type *ty)
{
   uint32_t         line = P_LINE(p);
   struct rslc_ids  al;
   struct rslc_mark mark;
   uint32_t         r;
   const struct rslc_type *rt = ty;

   if (ty->kind != RSLC_TY_ARRAY && ty->kind != RSLC_TY_STRUCT
         && ty->kind != RSLC_TY_VEC && ty->kind != RSLC_TY_MAT)
      return rslc_p_fail(p, "initializer list for a scalar", NULL), 0;
   rslc_p_next(p);   /* '{' */
   al.v = NULL; al.n = 0; al.cap = 0;
   rslc_scratch_mark(p->c, &mark);
   while (!rslc_p_is(p, RSLC_P_RBRACE))
   {
      uint32_t e;
      const struct rslc_type *et;
      if (ty->kind == RSLC_TY_ARRAY)
         et = ty->elem;
      else if (ty->kind == RSLC_TY_STRUCT)
         et = al.n < ty->nfields ? ty->fields[al.n].type : NULL;
      else if (ty->kind == RSLC_TY_MAT)
         et = rslc_ty_vecn(p, RSLC_BASE_FLOAT, ty->rows);
      else
         et = rslc_ty_scalar_of(p, ty);
      if (!et)
         return rslc_p_fail(p, "too many initializers", NULL), 0;
      if (rslc_p_is(p, RSLC_P_LBRACE))
         e = rslc_p_init_list(p, et);
      else
         e = rslc_p_assign_expr(p);
      if (!e)
         return 0;
      if (!rslc_ids_push(p, &al, e))
         return 0;
      if (rslc_p_accept(p, RSLC_P_COMMA))
         continue;
      break;
   }
   if (!rslc_p_expect(p, RSLC_P_RBRACE, "'}'"))
      return 0;
   if (ty->kind == RSLC_TY_ARRAY && !ty->array_len)
      rt = rslc_type_array(p->c, ty->elem, al.n);
   r = rslc_p_construct(p, rt, al.v, al.n, line);
   rslc_scratch_release(p->c, &mark);
   return r;
}

static uint32_t rslc_p_var_init(struct rslc_parser *p, struct rslc_var *v)
{
   uint32_t e;
   if (rslc_p_is(p, RSLC_P_LBRACE))
      e = rslc_p_init_list(p, v->type);
   else
      e = rslc_p_assign_expr(p);
   if (!e)
      return 0;
   if (v->type->kind == RSLC_TY_ARRAY && !v->type->array_len)
   {
      const struct rslc_type *it = rslc_p_node(p, e)->type;
      if (it->kind == RSLC_TY_ARRAY && it->elem == v->type->elem)
         v->type = it;
   }
   if (!rslc_p_coerce(p, &e, v->type, "initialiser"))
      return 0;
   return e;
}

/* Function parameter list after '(' ; fills f->params. */
static bool rslc_p_params(struct rslc_parser *p, struct rslc_func *f)
{
   struct rslc_var *params[64];
   uint32_t         n = 0;

   if (rslc_p_accept(p, RSLC_P_RPAREN))
      goto done;
   if (rslc_p_is_kw(p, RSLC_KW_VOID) && rslc_tok_is_punct(P_PEEK(p, 1), RSLC_P_RPAREN))
   {
      rslc_p_next(p);
      rslc_p_next(p);
      goto done;
   }
   for (;;)
   {
      struct rslc_quals q;
      const struct rslc_type *ty;
      struct rslc_sym  *name = NULL;
      struct rslc_var  *v;
      uint32_t          line = P_LINE(p);
      if (n == 64)
         return rslc_p_fail(p, "too many parameters", NULL);
      if (!rslc_p_quals(p, &q))
         return false;
      ty = rslc_p_type_spec(p, &q, false);
      if (!ty)
         return false;
      ty = rslc_p_array_suffix(p, ty);   /* vec3[2] p */
      if (!ty)
         return false;
      if (P_TOK(p)->kind == RSLC_TK_IDENT && rslc_tok_sym(p->c, P_TOK(p))->kw == RSLC_KW_NONE)
      {
         name = rslc_tok_sym(p->c, P_TOK(p));
         rslc_p_next(p);
      }
      ty = rslc_p_array_suffix(p, ty);
      if (!ty)
         return false;
      v = rslc_p_new_var(p, name, ty, line);
      if (!v)
         return false;
      v->storage   = RSLC_ST_PARAM;
      v->dir       = q.is_inout ? RSLC_DIR_INOUT : q.is_out ? RSLC_DIR_OUT : RSLC_DIR_IN;
      v->precision = rslc_p_default_prec(p, ty, q.precision);
      if (q.is_const)
         v->quals |= RSLC_VQ_PRECISE;   /* const in: read-only, marked for printers */
      params[n++] = v;
      if (rslc_p_accept(p, RSLC_P_COMMA))
         continue;
      if (!rslc_p_expect(p, RSLC_P_RPAREN, "')'"))
         return false;
      break;
   }
done:
   f->nparams = n;
   f->params  = (struct rslc_var**)rslc_arena_alloc(p->c, (n ? n : 1) * sizeof(*f->params));
   if (!f->params)
      return false;
   if (n)
      memcpy(f->params, params, n * sizeof(*f->params));
   return true;
}

static bool rslc_p_same_params(const struct rslc_func *a, const struct rslc_func *b)
{
   uint32_t i;
   if (a->nparams != b->nparams)
      return false;
   for (i = 0; i < a->nparams; i++)
      if (a->params[i]->type != b->params[i]->type)
         return false;
   return true;
}

static uint32_t rslc_p_function(struct rslc_parser *p, struct rslc_quals *q,
      const struct rslc_type *ret, struct rslc_sym *name, uint32_t line)
{
   struct rslc_func *f = rslc_p_new_func(p, name, ret, line);
   struct rslc_decl *d;
   struct rslc_func *existing = NULL;
   uint32_t          i;
   (void)q;

   if (!f)
      return 0;
   /* Parameters are declared in the body scope. */
   if (!rslc_p_push_scope(p))
      return 0;
   if (!rslc_p_params(p, f))
      return 0;

   d = name->decl;
   if (d && d->kind == RSLC_DK_FUNC)
   {
      struct rslc_func *g;
      for (g = (struct rslc_func*)d->ptr; g; g = g->next)
         if (rslc_p_same_params(g, f))
            existing = g;
   }
   else if (d && p->scope->parent && rslc_p_find_here(p, name))
   {
      rslc_err(p->c, "redeclaration of ", name->str, line);
      return 0;
   }

   if (existing)
   {
      if (existing->ret != ret)
      {
         rslc_err(p->c, "overload differs only by return type: ", name->str, line);
         return 0;
      }
      /* Reuse the earlier record; the new parameter vars replace the
       * prototype's so the body refers to names.  A prototype that
       * follows the definition leaves the body's params alone. */
      if (!existing->body)
      {
         existing->params  = f->params;
         existing->nparams = f->nparams;
      }
      f = existing;
   }
   else if (d && d->kind == RSLC_DK_FUNC)
   {
      /* New overload: append to the chain. */
      struct rslc_func *g = (struct rslc_func*)d->ptr;
      while (g->next)
         g = g->next;
      g->next = f;
   }
   else
   {
      struct rslc_scope *saved = p->scope;
      p->scope = saved->parent;   /* bind at global scope */
      if (name->bfn && name->decl == NULL)
      {
         /* A user function shadowing a builtin name is allowed. */
      }
      if (!rslc_p_bind(p, name, RSLC_DK_FUNC, f))
         return 0;
      p->scope = saved;
   }

   if (rslc_p_accept(p, RSLC_P_SEMI))
   {
      rslc_p_pop_scope(p);
      return 1;   /* prototype */
   }
   if (!rslc_p_is(p, RSLC_P_LBRACE))
      return rslc_p_fail(p, "expected '{' or ';' after function header", NULL), 0;
   if (f->body)
   {
      rslc_err(p->c, "function defined twice: ", name->str, line);
      return 0;
   }
   for (i = 0; i < f->nparams; i++)
      if (f->params[i]->name && !rslc_p_bind(p, f->params[i]->name, RSLC_DK_VAR, f->params[i]))
         return 0;
   p->cur_fn = f;
   f->body   = rslc_p_block(p, false);
   p->cur_fn = NULL;
   rslc_p_pop_scope(p);
   if (!f->body)
      return 0;
   if (name->len == 4 && !memcmp(name->str, "main", 4) && f->nparams == 0)
      p->c->prog.main_fn = f;
   return 1;
}

/* A uniform / push block: qualifiers already read, name is the block
 * type name. */
static bool rslc_p_block_decl(struct rslc_parser *p, struct rslc_quals *q,
      struct rslc_sym *name, uint32_t line)
{
   struct rslc_prog      *pr  = &p->c->prog;
   unsigned               idx = q->push_constant ? 1 : 0;
   struct rslc_blockdecl *b   = &pr->blocks[idx];
   struct rslc_field     *fields = NULL;
   uint32_t               nfields = 0, cap = 0, i;
   struct rslc_type      *st;
   struct rslc_sym       *instance = NULL;

   if (!q->is_uniform)
      return rslc_p_fail(p, "only uniform blocks are supported", NULL);
   if (pr->has_block[idx])
      return rslc_p_fail(p, idx ? "more than one push_constant block"
            : "more than one uniform block", NULL);
   rslc_p_next(p);   /* '{' */
   while (!rslc_p_accept(p, RSLC_P_RBRACE))
   {
      struct rslc_quals fq;
      const struct rslc_type *ft;
      if (!rslc_p_quals(p, &fq))
         return false;
      ft = rslc_p_type_spec(p, &fq, false);
      if (!ft)
         return false;
      for (;;)
      {
         const struct rslc_tok  *nt = P_TOK(p);
         const struct rslc_type *mt;
         if (nt->kind != RSLC_TK_IDENT)
            return rslc_p_fail(p, "expected a member name", NULL);
         rslc_p_next(p);
         mt = rslc_p_array_suffix(p, ft);
         if (!mt)
            return false;
         if (nfields == cap)
         {
            uint32_t           ncap = cap ? cap * 2 : 16;
            struct rslc_field *nf   = (struct rslc_field*)rslc_arena_zalloc(p->c,
                  ncap * sizeof(*nf));
            if (!nf)
               return false;
            if (nfields)
               memcpy(nf, fields, nfields * sizeof(*nf));
            fields = nf;
            cap    = ncap;
         }
         fields[nfields].type      = mt;
         fields[nfields].name      = rslc_tok_sym(p->c, nt);
         fields[nfields].line      = nt->line;
         fields[nfields].precision = rslc_p_default_prec(p, mt, fq.precision);
         nfields++;
         if (rslc_p_accept(p, RSLC_P_COMMA))
            continue;
         break;
      }
      if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
         return false;
   }
   if (P_TOK(p)->kind == RSLC_TK_IDENT)
   {
      instance = rslc_tok_sym(p->c, P_TOK(p));
      rslc_p_next(p);
      if (rslc_p_is(p, RSLC_P_LBRACKET))
         return rslc_p_fail(p, "block instance arrays are not supported", NULL);
   }
   if (!rslc_p_expect(p, RSLC_P_SEMI, "';' after block"))
      return false;

   st = rslc_type_struct(p->c, name, fields, nfields);
   if (!st)
      return false;
   b->type     = st;
   b->instance = instance;
   b->binding  = q->binding;
   b->set      = q->set;
   b->line     = line;
   b->push     = idx == 1;
   b->std140   = idx == 0 || q->std140;
   pr->has_block[idx] = true;

   if (instance)
   {
      struct rslc_var *v = rslc_p_new_var(p, instance, st, line);
      if (!v)
         return false;
      v->storage = RSLC_ST_BLOCK;
      v->block   = idx;
      v->binding = q->binding;
      v->set     = q->set;
      if (rslc_p_find_here(p, instance) || !rslc_p_bind(p, instance, RSLC_DK_VAR, v))
         return rslc_p_fail(p, "redeclaration of ", instance->str);
   }
   else
   {
      for (i = 0; i < nfields; i++)
      {
         struct rslc_var *v = rslc_p_new_var(p, fields[i].name, fields[i].type, fields[i].line);
         if (!v)
            return false;
         v->storage   = RSLC_ST_MEMBER;
         v->block     = idx;
         v->member    = i;
         v->precision = fields[i].precision;
         if (rslc_p_find_here(p, fields[i].name))
         {
            rslc_err(p->c, "redeclaration of ", fields[i].name->str, fields[i].line);
            return false;
         }
         if (!rslc_p_bind(p, fields[i].name, RSLC_DK_VAR, v))
            return false;
      }
   }
   return true;
}

static bool rslc_p_add_interface(struct rslc_parser *p, struct rslc_var *v)
{
   struct rslc_prog *pr = &p->c->prog;
   switch (v->storage)
   {
      case RSLC_ST_SAMPLER:
         RSLC_GROW_PTRS(p->c, pr->samplers, pr->nsamplers, pr->samplers_cap, struct rslc_var, false);
         pr->samplers[pr->nsamplers++] = v;
         return true;
      case RSLC_ST_IN:
         RSLC_GROW_PTRS(p->c, pr->inputs, pr->ninputs, pr->inputs_cap, struct rslc_var, false);
         pr->inputs[pr->ninputs++] = v;
         return true;
      case RSLC_ST_OUT:
         RSLC_GROW_PTRS(p->c, pr->outputs, pr->noutputs, pr->outputs_cap, struct rslc_var, false);
         pr->outputs[pr->noutputs++] = v;
         return true;
      default:
         return true;
   }
}

/* A declaration: variables, a function, a struct, a block, or a
 * precision statement.  Returns a DECL / BLOCK node (local) or 1
 * (global, nothing to emit as a statement); 0 on error. */
static uint32_t rslc_p_declaration(struct rslc_parser *p, bool global)
{
   struct rslc_quals       q;
   const struct rslc_type *ty;
   uint32_t                line = P_LINE(p);
   uint32_t                decls[32];
   uint32_t                ndecls = 0;

   /* precision statement */
   if (rslc_p_accept_kw(p, RSLC_KW_PRECISION))
   {
      uint8_t prec;
      if (rslc_p_accept_kw(p, RSLC_KW_LOWP))         prec = RSLC_PREC_LOW;
      else if (rslc_p_accept_kw(p, RSLC_KW_MEDIUMP)) prec = RSLC_PREC_MEDIUM;
      else if (rslc_p_accept_kw(p, RSLC_KW_HIGHP))   prec = RSLC_PREC_HIGH;
      else
         return rslc_p_fail(p, "expected a precision", NULL), 0;
      if (rslc_p_accept_kw(p, RSLC_KW_FLOAT))
         p->scope->prec_float = prec;
      else if (rslc_p_accept_kw(p, RSLC_KW_INT))
         p->scope->prec_int = prec;
      else if (rslc_p_accept_kw(p, RSLC_KW_SAMPLER2D))
         ;
      else
         return rslc_p_fail(p, "precision statement needs float, int or sampler2D", NULL), 0;
      if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
         return 0;
      return global ? 1 : rslc_p_mk(p, RSLC_OP_BLOCK, NULL, 0, 0, 0, RSLC_BLOCK_GROUP, line, 0);
   }

   if (!rslc_p_quals(p, &q))
      return 0;

   /* uniform Block { ... }; */
   if (q.is_uniform && P_TOK(p)->kind == RSLC_TK_IDENT
         && rslc_tok_sym(p->c, P_TOK(p))->kw == RSLC_KW_NONE
         && rslc_tok_is_punct(P_PEEK(p, 1), RSLC_P_LBRACE))
   {
      struct rslc_sym *bname = rslc_tok_sym(p->c, P_TOK(p));
      if (!global)
         return rslc_p_fail(p, "block inside a function", NULL), 0;
      rslc_p_next(p);
      if (!rslc_p_block_decl(p, &q, bname, line))
         return 0;
      return 1;
   }

   ty = rslc_p_type_spec(p, &q, true);
   if (!ty)
      return 0;
   ty = rslc_p_array_suffix(p, ty);   /* vec3[2] name */
   if (!ty)
      return 0;

   /* struct S { ... }; with no declarator */
   if (rslc_p_accept(p, RSLC_P_SEMI))
      return global ? 1 : rslc_p_mk(p, RSLC_OP_BLOCK, NULL, 0, 0, 0, 0, line, 0);

   for (;;)
   {
      const struct rslc_tok  *nt = P_TOK(p);
      struct rslc_sym        *name;
      const struct rslc_type *vt;
      struct rslc_var        *v;
      uint32_t                vline = nt->line;

      if (nt->kind != RSLC_TK_IDENT || rslc_tok_sym(p->c, nt)->kw != RSLC_KW_NONE)
         return rslc_p_fail(p, "expected a name in declaration", NULL), 0;
      name = rslc_tok_sym(p->c, nt);
      rslc_p_next(p);

      if (rslc_p_accept(p, RSLC_P_LPAREN))
      {
         if (!global || ndecls)
            return rslc_p_fail(p, "function declaration not allowed here", NULL), 0;
         return rslc_p_function(p, &q, ty, name, line);
      }

      vt = rslc_p_array_suffix(p, ty);
      if (!vt)
         return 0;
      if (vt == p->t_void)
         return rslc_p_fail(p, "variable of type void", NULL), 0;

      v = rslc_p_new_var(p, name, vt, vline);
      if (!v)
         return 0;
      v->precision = rslc_p_default_prec(p, vt, q.precision);
      v->interp    = q.interp;
      v->quals     = q.vq;
      v->location  = q.location;
      v->binding   = q.binding;
      v->set       = q.set;

      if (vt->kind == RSLC_TY_SAMPLER2D)
      {
         if (!global || !q.is_uniform)
            return rslc_p_fail(p, "sampler2D must be a global uniform", NULL), 0;
         v->storage = RSLC_ST_SAMPLER;
      }
      else if (q.is_uniform)
         return rslc_p_fail(p, "loose uniforms are not supported; use a block", NULL), 0;
      else if (q.is_in && global)
         v->storage = RSLC_ST_IN;
      else if (q.is_out && global)
         v->storage = RSLC_ST_OUT;
      else if (q.is_const)
         v->storage = RSLC_ST_CONST;
      else
         v->storage = global ? RSLC_ST_GLOBAL : RSLC_ST_LOCAL;

      if (v->storage == RSLC_ST_IN && p->c->stage == RSLC_STAGE_FRAGMENT
            && (vt->base == RSLC_BASE_INT || vt->base == RSLC_BASE_UINT)
            && v->interp == RSLC_INTERP_NONE)
         v->interp = RSLC_INTERP_FLAT;

      if (rslc_p_accept(p, RSLC_P_ASSIGN))
      {
         if (v->storage == RSLC_ST_IN || v->storage == RSLC_ST_SAMPLER)
            return rslc_p_fail(p, "initialiser on an input", NULL), 0;
         v->init = rslc_p_var_init(p, v);
         if (!v->init)
            return 0;
         if (v->storage == RSLC_ST_CONST)
         {
            struct rslc_cval cv;
            if (!rslc_p_eval(p, v->init, &cv) && vt->kind != RSLC_TY_ARRAY
                  && vt->kind != RSLC_TY_STRUCT && vt->kind != RSLC_TY_MAT)
            {
               /* Not foldable: keep it as an ordinary initialised
                * variable, as glslang's "const" of non-constant does
                * in practice. */
               v->storage = global ? RSLC_ST_GLOBAL : RSLC_ST_LOCAL;
            }
         }
      }
      else if (v->storage == RSLC_ST_CONST)
         return rslc_p_fail(p, "const without an initialiser: ", name->str), 0;

      if (v->type->kind == RSLC_TY_ARRAY && !v->type->array_len)
         return rslc_p_fail(p, "unsized array: ", name->str), 0;

      if (rslc_p_find_here(p, name))
      {
         rslc_err(p->c, "redeclaration of ", name->str, vline);
         return 0;
      }
      if (!rslc_p_bind(p, name, RSLC_DK_VAR, v))
         return 0;
      if (!rslc_p_add_interface(p, v))
         return 0;

      if (!global)
      {
         if (ndecls == 32)
            return rslc_p_fail(p, "too many declarators", NULL), 0;
         decls[ndecls++] = rslc_p_mk(p, RSLC_OP_DECL, NULL, v->id, 0, 0, 0, vline, 0);
      }

      if (rslc_p_accept(p, RSLC_P_COMMA))
         continue;
      if (!rslc_p_expect(p, RSLC_P_SEMI, "';'"))
         return 0;
      break;
   }
   if (global)
      return 1;
   if (ndecls == 1)
      return decls[0];
   /* one declaration, several declarators: a group, not a scope */
   return rslc_p_mk(p, RSLC_OP_BLOCK, NULL, 0, rslc_list_new(p->c, decls, ndecls), 0, RSLC_BLOCK_GROUP, line, 0);
}

/* ---- builtin variables ---- */

static bool rslc_p_builtin_var(struct rslc_parser *p, const char *name,
      const struct rslc_type *ty, uint8_t which, uint8_t storage)
{
   struct rslc_sym *s = rslc_sym(p->c, name, strlen(name));
   struct rslc_var *v;
   if (!s)
      return false;
   v = rslc_p_new_var(p, s, ty, 0);
   if (!v)
      return false;
   v->storage = RSLC_ST_BUILTIN;
   v->builtin = which;
   v->dir     = storage;   /* RSLC_DIR_IN: readable, RSLC_DIR_OUT: writable */
   return rslc_p_bind(p, s, RSLC_DK_VAR, v) != NULL;
}

/* ---- for the remap (rslc_emit.c) ---- */

#ifndef RSLC_NO_TEXT

struct rslc_var *rslc_front_global(struct rslc_ctx *c, struct rslc_sym *name,
      const struct rslc_type *type)
{
   struct rslc_parser *p = c->parser;
   struct rslc_var    *v = rslc_p_new_var(p, name, type, 0);
   if (!v)
      return NULL;
   v->global = 1;
   return rslc_p_bind(p, name, RSLC_DK_VAR, v) ? v : NULL;
}

struct rslc_var *rslc_front_var_copy(struct rslc_ctx *c, const struct rslc_var *src)
{
   struct rslc_prog *pr = &c->prog;
   struct rslc_var  *v  = (struct rslc_var*)rslc_arena_alloc(c, sizeof(*v));
   if (!v)
      return NULL;
   RSLC_GROW_PTRS(c, pr->vars, pr->nvars, pr->vars_cap, struct rslc_var, NULL);
   memcpy(v, src, sizeof(*v));
   v->id                 = pr->nvars;
   pr->vars[pr->nvars++] = v;
   return v;
}

/* More declarations at file scope, from @src: in highp whatever the
 * stage's defaults, with none of its macros, and leaving its #version
 * as it was. */
bool rslc_front_parse(struct rslc_ctx *c, const char *src, size_t len)
{
   struct rslc_parser *p      = c->parser;
   uint8_t             prec_f = p->scope->prec_float;
   uint8_t             prec_i = p->scope->prec_int;
   uint32_t            ver    = c->version;
   uint32_t            eof    = c->ntoks - 1;
   bool                es     = c->es;
   bool                ok;
   uint32_t            i;

   for (i = 0; i < c->nsyms; i++)
      c->syms[i]->macro = NULL;
   p->scope->prec_float = RSLC_PREC_HIGH;
   p->scope->prec_int   = RSLC_PREC_HIGH;
   ok     = rslc_pp_run(c, src, len);
   p->pos = eof + 1;
   while (ok && P_TOK(p)->kind != RSLC_TK_EOF)
   {
      if (rslc_p_accept(p, RSLC_P_SEMI))
         continue;
      ok = rslc_p_declaration(p, true) != 0;
   }
   c->version           = ver;
   c->es                = es;
   p->scope->prec_float = prec_f;
   p->scope->prec_int   = prec_i;
   return ok;
}
#endif

/* ---- translation unit ---- */

static bool rslc_p_run(struct rslc_ctx *c)
{
   struct rslc_parser *p = (struct rslc_parser*)rslc_arena_zalloc(c, sizeof(*p));
   if (!p)
      return false;
   p->c       = c;
   p->t_void  = rslc_type_basic(c, RSLC_TY_VOID, RSLC_BASE_VOID, 0, 0);
   p->t_bool  = rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_BOOL, 1, 1);
   p->t_int   = rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_INT, 1, 1);
   p->t_uint  = rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_UINT, 1, 1);
   p->t_float = rslc_type_basic(c, RSLC_TY_SCALAR, RSLC_BASE_FLOAT, 1, 1);
   p->t_s2d   = rslc_type_sampler2d(c, RSLC_BASE_FLOAT);
   if (!p->t_void || !p->t_bool || !p->t_int || !p->t_uint || !p->t_float || !p->t_s2d)
      return false;
   if (!rslc_p_seed_builtins(p))
      return false;
   if (!rslc_p_push_scope(p))
      return false;

   if (c->stage == RSLC_STAGE_VERTEX)
   {
      if (     !rslc_p_builtin_var(p, "gl_Position", rslc_ty_vecn(p, RSLC_BASE_FLOAT, 4), RSLC_BI_POSITION, RSLC_DIR_OUT)
            || !rslc_p_builtin_var(p, "gl_PointSize", p->t_float, RSLC_BI_POINTSIZE, RSLC_DIR_OUT)
            || !rslc_p_builtin_var(p, "gl_VertexIndex", p->t_int, RSLC_BI_VERTEXINDEX, RSLC_DIR_IN)
            || !rslc_p_builtin_var(p, "gl_InstanceIndex", p->t_int, RSLC_BI_INSTANCEINDEX, RSLC_DIR_IN))
         return false;
   }
   else
   {
      if (     !rslc_p_builtin_var(p, "gl_FragCoord", rslc_ty_vecn(p, RSLC_BASE_FLOAT, 4), RSLC_BI_FRAGCOORD, RSLC_DIR_IN)
            || !rslc_p_builtin_var(p, "gl_FragDepth", p->t_float, RSLC_BI_FRAGDEPTH, RSLC_DIR_OUT)
            || !rslc_p_builtin_var(p, "gl_FrontFacing", p->t_bool, RSLC_BI_FRONTFACING, RSLC_DIR_IN))
         return false;
   }

   while (P_TOK(p)->kind != RSLC_TK_EOF)
   {
      if (rslc_p_accept(p, RSLC_P_SEMI))
         continue;
      if (!rslc_p_declaration(p, true))
         return false;
   }
   if (!c->prog.main_fn)
   {
      rslc_err(c, "no main() in stage", NULL, 0);
      return false;
   }
   if (c->prog.main_fn->ret != p->t_void)
   {
      rslc_err(c, "main() must return void", NULL, c->prog.main_fn->line);
      return false;
   }
   if (!c->opts->remap)
      return true;
#ifdef RSLC_NO_TEXT
   /* the remap lives with the printers: the caller rewrites SPIR-V */
   c->remap = RSLC_REMAP_UNSUPPORTED;
   return true;
#else
   c->parser = p;
   return rslc_remap_run(c);
#endif
}

/* ------------------------------------------------------------------ */
/* Compile entry                                                       */
/* ------------------------------------------------------------------ */

bool rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len)
{
   if (!rslc_pp_run(c, src, len))
      return false;
   return rslc_p_run(c);
}

bool rslc_compile(const char *glsl, size_t len,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result)
{
   struct rslc_ctx c;
   bool            ok;

   if (!glsl || !opts || !result)
      return false;
   if (!rslc_ctx_init(&c, opts, stage))
      return false;

   ok = rslc_front_compile(&c, glsl, len);
#ifdef RSLC_SPV_TRACE
   fprintf(stderr, "front: arena used %lu held %lu scratch used %lu held %lu toks %u nodes %u lists %u syms %u types %u synth %u\n",
         (unsigned long)c.arena.used, (unsigned long)c.arena.held,
         (unsigned long)c.scratch.used, (unsigned long)c.scratch.held,
         c.ntoks, c.nnodes, c.nlists, c.nsyms, c.ntypes, c.nsynth);
#endif
   if (ok)
      ok = rslc_emit_dst(&c, result);
   if (ok)
      result->remap = c.remap;
#ifdef RSLC_SPV_TRACE
   fprintf(stderr, "emit: arena used %lu held %lu scratch used %lu held %lu\n",
         (unsigned long)c.arena.used, (unsigned long)c.arena.held,
         (unsigned long)c.scratch.used, (unsigned long)c.scratch.held);
#endif

   rslc_ctx_close(&c);
   return ok;
}
