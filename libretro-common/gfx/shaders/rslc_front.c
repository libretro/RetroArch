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

#include <string.h>

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
      l->pos++;
      for (;;)
      {
         char d = rslc_lex_at(l, 0);
         if ((d == '+' || d == '-')
               && (l->src[l->pos - 1] == 'e' || l->src[l->pos - 1] == 'E'))
            l->pos++;
         else if (rslc_is_ident(d) || d == '.')
            l->pos++;
         else
            break;
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
/* Compile entry                                                       */
/* ------------------------------------------------------------------ */

bool rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len)
{
   if (!rslc_pp_run(c, src, len))
      return false;
   rslc_err(c, "parser not implemented", NULL, 0);
   return false;
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
   if (ok)
      ok = rslc_emit_dst(&c, result);

   rslc_ctx_close(&c);
   return ok;
}
