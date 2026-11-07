/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* A GLSL preset's first pass reading its frame where it lies in a larger
 * texture, as it would read a texture holding just that rectangle: the
 * same texels, filtering and wrap at the rectangle's edges, with its top
 * row first as a software frame's. Included by shader_glsl.c, and by its
 * test; no GL here.
 *
 * glsl_rect_rewrite() rewrites the source text. Every read of a frame
 * sampler - by its names, object-like macros naming them, or a function's
 * sampler parameter every call gives the frame - must be the first
 * argument of texture2D(), texture(), texture2DLod() or textureLod(), or
 * of a macro naming or forwarding to one of those, and becomes
 * RARCH_frame(coord), defined after the sampler's declaration at global
 * scope. Anything else - another built-in, the frame in an expression, a
 * macro that means a frame in one branch and something else in another,
 * an overloaded function given it - is unsupported, and the pass keeps
 * its copy. A lod or bias argument is dropped: neither texture has
 * levels.
 *
 * RARCH_frame() reads through three vec4 uniforms glsl_rect_values()
 * fills. Linear, clamped to edge: the coordinate mapped into the
 * rectangle and clamped to just past its first and last texel centres
 * (slang_rect.h's rule), where the filter gives the texel beyond no
 * weight. Nearest: the texel a sampler would pick, floor(u * size),
 * wrapped as it would, read at its centre. A coordinate exactly on a
 * texel edge goes the way the shader's arithmetic puts it, which a GPU
 * compiling another program may round the other way; filter weights at
 * such points may differ by one step.
 * Linear filtering with repeat, mirror or border is unsupported.
 * Without highp in the fragment stage on GLES the rewritten source does
 * not compile, and the pass keeps its copy. */

#ifndef GLSL_RECT_H__
#define GLSL_RECT_H__

#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#define GLSL_RECT_NAME_RECT   "RARCH_FrameRect"
#define GLSL_RECT_NAME_CLAMP  "RARCH_FrameClamp"
#define GLSL_RECT_NAME_TEXELS "RARCH_FrameTexels"

/* In texels, as SLANG_RECT_CLAMP_BIAS */
#define GLSL_RECT_CLAMP_BIAS (1.0f / 1024.0f)


enum glsl_rect_mode
{
   GLSL_RECT_LINEAR_EDGE = 0,
   GLSL_RECT_NEAREST_EDGE,
   GLSL_RECT_NEAREST_REPEAT,
   GLSL_RECT_NEAREST_MIRROR,
   GLSL_RECT_NEAREST_BORDER
};

/* Up to this many defines of one name are followed */
#define GLSL_RECT_ALIAS_DEPTH 8

enum glsl_rect_tok
{
   GLSL_RECT_TOK_IDENT = 0,
   GLSL_RECT_TOK_PUNCT,
   GLSL_RECT_TOK_OTHER
};

struct glsl_rect_token
{
   size_t start;
   size_t len;
   /* 0 for code; a define's body has its own, counted from 1 */
   unsigned region;
   unsigned char type;
   /* An object-like define's single-identifier body: not a use */
   unsigned char alias_body;
   /* GLSL_RECT_FLAG_*: set by glsl_rect_frame_params() */
   unsigned char flags;
   /* { } depth before it, code only */
   unsigned short brace;
};

/* A function's sampler parameter every call gives the frame: the
 * parameter's declaration and the frames passed are not reads, and
 * inside the function the parameter is the frame */
#define GLSL_RECT_FLAG_SKIP  1
#define GLSL_RECT_FLAG_FRAME 2

struct glsl_rect_alias
{
   size_t name;
   size_t name_len;
   size_t body;
   size_t body_len;
};

/* A name an #undef took away */
struct glsl_rect_undef
{
   size_t name;
   size_t len;
};

struct glsl_rect_scan
{
   const char *src;
   struct glsl_rect_token *tok;
   size_t num_tok;
   size_t cap_tok;
   struct glsl_rect_alias *alias;
   size_t num_alias;
   size_t cap_alias;
   struct glsl_rect_undef undef[64];
   size_t num_undef;
   bool failed;
};

static bool glsl_rect_is_ident_start(char c)
{
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool glsl_rect_is_ident(char c)
{
   return glsl_rect_is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool glsl_rect_tok_is(const struct glsl_rect_scan *s, size_t i,
      const char *str)
{
   size_t n = strlen(str);
   return i < s->num_tok && s->tok[i].len == n
      && !memcmp(s->src + s->tok[i].start, str, n);
}

static bool glsl_rect_span_is(const char *src, size_t start, size_t len,
      const char *str)
{
   return strlen(str) == len && !memcmp(src + start, str, len);
}

static void glsl_rect_push_tok(struct glsl_rect_scan *s, size_t start,
      size_t len, unsigned char type, unsigned region)
{
   if (s->failed)
      return;
   if (s->num_tok == s->cap_tok)
   {
      size_t cap                  = s->cap_tok ? s->cap_tok * 2 : 256;
      struct glsl_rect_token *tok = (struct glsl_rect_token*)
         realloc(s->tok, cap * sizeof(*tok));
      if (!tok)
      {
         s->failed = true;
         return;
      }
      s->tok     = tok;
      s->cap_tok = cap;
   }
   s->tok[s->num_tok].start      = start;
   s->tok[s->num_tok].len        = len;
   s->tok[s->num_tok].type       = type;
   s->tok[s->num_tok].region     = region;
   s->tok[s->num_tok].alias_body = 0;
   s->tok[s->num_tok].flags      = 0;
   s->tok[s->num_tok].brace      = 0;
   s->num_tok++;
}

static void glsl_rect_push_alias(struct glsl_rect_scan *s, size_t name,
      size_t name_len, size_t body, size_t body_len)
{
   if (s->failed)
      return;
   if (s->num_alias == s->cap_alias)
   {
      size_t cap                 = s->cap_alias ? s->cap_alias * 2 : 32;
      struct glsl_rect_alias *a  = (struct glsl_rect_alias*)
         realloc(s->alias, cap * sizeof(*a));
      if (!a)
      {
         s->failed = true;
         return;
      }
      s->alias     = a;
      s->cap_alias = cap;
   }
   s->alias[s->num_alias].name     = name;
   s->alias[s->num_alias].name_len = name_len;
   s->alias[s->num_alias].body     = body;
   s->alias[s->num_alias].body_len = body_len;
   s->num_alias++;
}

/* Comments blanked in place, newlines kept */
static void glsl_rect_blank_comments(char *buf)
{
   char *p = buf;
   while (*p)
   {
      if (p[0] == '/' && p[1] == '/')
      {
         while (*p && *p != '\n')
            *p++ = ' ';
      }
      else if (p[0] == '/' && p[1] == '*')
      {
         *p++ = ' ';
         *p++ = ' ';
         while (*p && !(p[0] == '*' && p[1] == '/'))
         {
            if (*p != '\n')
               *p = ' ';
            p++;
         }
         if (*p)
         {
            *p++ = ' ';
            *p++ = ' ';
         }
      }
      else
         p++;
   }
}

/* Tokens of [p, end) into @s; @region as glsl_rect_token's. A line
 * continuation is whitespace. */
static void glsl_rect_lex(struct glsl_rect_scan *s, size_t p, size_t end,
      unsigned region)
{
   const char *src = s->src;
   while (p < end)
   {
      char c = src[p];
      if (c == '\\' && p + 1 < end && src[p + 1] == '\n')
         p += 2;
      else if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'
            || c == '\v')
         p++;
      else if (glsl_rect_is_ident_start(c))
      {
         size_t q = p;
         while (q < end && glsl_rect_is_ident(src[q]))
            q++;
         glsl_rect_push_tok(s, p, q - p, GLSL_RECT_TOK_IDENT, region);
         p = q;
      }
      else if ((c >= '0' && c <= '9') || (c == '.' && p + 1 < end
               && src[p + 1] >= '0' && src[p + 1] <= '9'))
      {
         size_t q = p;
         while (q < end && (glsl_rect_is_ident(src[q]) || src[q] == '.'
                  || ((src[q] == '+' || src[q] == '-')
                     && (src[q - 1] == 'e' || src[q - 1] == 'E'))))
            q++;
         glsl_rect_push_tok(s, p, q - p, GLSL_RECT_TOK_OTHER, region);
         p = q;
      }
      else
      {
         glsl_rect_push_tok(s, p, 1, GLSL_RECT_TOK_PUNCT, region);
         p++;
      }
   }
}

/* The end of the logical line starting at @p: past its newline, line
 * continuations included */
static size_t glsl_rect_line_end(const char *src, size_t p)
{
   while (src[p])
   {
      if (src[p] == '\\' && src[p + 1] == '\n')
         p += 2;
      else if (src[p] == '\n')
         return p + 1;
      else
         p++;
   }
   return p;
}

/* Whether a function-like define's parameters, tokens [@first, @k), and
 * body, tokens from @k on, are NAME(a, b[, c]) R(a, b[, c]) */
static bool glsl_rect_forwards(const struct glsl_rect_scan *s,
      size_t first, size_t k)
{
   size_t np = 0, i;
   size_t params[3];
   const struct glsl_rect_token *t = s->tok;
   /* a , b [, c] */
   for (i = first; i < k; i++)
   {
      if ((i - first) % 2 == 0)
      {
         if (t[i].type != GLSL_RECT_TOK_IDENT || np == 3)
            return false;
         params[np++] = i;
      }
      else if (!glsl_rect_tok_is(s, i, ","))
         return false;
   }
   if (np < 2 || (k - first) % 2 == 0)
      return false;
   /* R ( a , b [, c] ) and nothing after */
   if (     s->num_tok - k != 2 + 2 * np
         || t[k].type != GLSL_RECT_TOK_IDENT
         || !glsl_rect_tok_is(s, k + 1, "(")
         || !glsl_rect_tok_is(s, s->num_tok - 1, ")"))
      return false;
   for (i = 0; i < np; i++)
   {
      const struct glsl_rect_token *a = &t[k + 2 + 2 * i];
      const struct glsl_rect_token *p = &t[params[i]];
      if (     a->len != p->len
            || memcmp(s->src + a->start, s->src + p->start, a->len))
         return false;
      if (i + 1 < np && !glsl_rect_tok_is(s, k + 3 + 2 * i, ","))
         return false;
   }
   return true;
}

/* Code tokens and define bodies of the blanked source, and the
 * object-like defines whose body is one identifier */
static void glsl_rect_scan_source(struct glsl_rect_scan *s)
{
   const char *src = s->src;
   size_t p        = 0;
   unsigned region = 0;
   while (src[p] && !s->failed)
   {
      size_t q   = p;
      size_t end = glsl_rect_line_end(src, p);
      while (q < end && (src[q] == ' ' || src[q] == '\t'))
         q++;
      if (q < end && src[q] == '#')
      {
         size_t d = q + 1;
         while (d < end && (src[d] == ' ' || src[d] == '\t'))
            d++;
         if (     end - d > 6 && !memcmp(src + d, "define", 6)
               && (src[d + 6] == ' ' || src[d + 6] == '\t'))
         {
            size_t n = d + 6, n_end, first;
            while (n < end && (src[n] == ' ' || src[n] == '\t'))
               n++;
            n_end = n;
            while (n_end < end && glsl_rect_is_ident(src[n_end]))
               n_end++;
            region++;
            if (n_end < end && src[n_end] == '(')
            {
               /* Function-like: its body after the parameters */
               size_t b = n_end, first = s->num_tok, k;
               while (b < end && src[b] != ')')
                  b++;
               if (b < end)
               {
                  bool fwd;
                  glsl_rect_lex(s, n_end + 1, b, region);
                  k   = s->num_tok;
                  glsl_rect_lex(s, b + 1, end, region);
                  /* NAME(a, b[, c]) R(a, b[, c]): forwards to R, as an
                   * object-like define would name it */
                  if (s->failed)
                     return;
                  if ((fwd = glsl_rect_forwards(s, first, k)))
                     glsl_rect_push_alias(s, n, n_end - n,
                           s->tok[k].start, s->tok[k].len);
                  /* The parameters are not code */
                  memmove(s->tok + first, s->tok + k,
                        (s->num_tok - k) * sizeof(*s->tok));
                  s->num_tok -= k - first;
                  /* Its body names R: not a use */
                  if (fwd)
                     s->tok[first].alias_body = 1;
               }
            }
            else
            {
               first = s->num_tok;
               glsl_rect_lex(s, n_end, end, region);
               if (     s->num_tok == first + 1
                     && s->tok[first].type == GLSL_RECT_TOK_IDENT)
               {
                  s->tok[first].alias_body = 1;
                  glsl_rect_push_alias(s, n, n_end - n, s->tok[first].start,
                        s->tok[first].len);
               }
            }
         }
         else if (end - d > 5 && !memcmp(src + d, "undef", 5))
         {
            size_t n = d + 5, n_end;
            while (n < end && (src[n] == ' ' || src[n] == '\t'))
               n++;
            n_end = n;
            while (n_end < end && glsl_rect_is_ident(src[n_end]))
               n_end++;
            if (s->num_undef < sizeof(s->undef) / sizeof(s->undef[0]))
            {
               s->undef[s->num_undef].name = n;
               s->undef[s->num_undef].len  = n_end - n;
               s->num_undef++;
            }
            else
               s->failed = true;
         }
      }
      else
         glsl_rect_lex(s, p, end, 0);
      p = end;
   }
}

static const char *const glsl_rect_reads[] = {
   "texture2D", "texture", "texture2DLod", "textureLod"
};

/* 1: every meaning of @name is one of @names; 0: none is; -1: some
 * are and some are not, or too deep. A read's name ends the search: a
 * define of it that comes back to it, as `#define texture(c, d)
 * COMPAT_TEXTURE(c, d)` does, still reads. */
static int glsl_rect_resolves_to(const struct glsl_rect_scan *s,
      size_t name, size_t len, const char *const *names, unsigned count,
      unsigned depth)
{
   unsigned i;
   size_t a;
   int result  = -2;
   for (i = 0; i < count; i++)
      if (glsl_rect_span_is(s->src, name, len, names[i]))
         return 1;
   for (i = 0; i < 4; i++)
      if (glsl_rect_span_is(s->src, name, len, glsl_rect_reads[i]))
         return 0;
   if (depth >= GLSL_RECT_ALIAS_DEPTH)
      return -1;
   for (a = 0; a < s->num_alias; a++)
   {
      int r;
      if (     s->alias[a].name_len != len
            || memcmp(s->src + s->alias[a].name, s->src + name, len))
         continue;
      r = glsl_rect_resolves_to(s, s->alias[a].body, s->alias[a].body_len,
            names, count, depth + 1);
      if (r < 0)
         return -1;
      if (result == -2)
         result = r;
      else if (result != r)
         return -1;
   }
   return result == -2 ? 0 : result;
}


struct glsl_rect_func
{
   size_t name;  /* token indices */
   size_t open;
   size_t close;
   size_t body;
   size_t end;
   bool overloaded;
};

struct glsl_rect_param
{
   unsigned func;
   unsigned pos;
   size_t tok;
   bool alive;
};

/* The token closing the bracket at @open, or num_tok; a define's lines
 * between are not counted */
static size_t glsl_rect_match(const struct glsl_rect_scan *s, size_t open,
      char o, char c)
{
   size_t k;
   int depth = 0;
   for (k = open; k < s->num_tok; k++)
   {
      if (     s->tok[k].region != s->tok[open].region
            || s->tok[k].type != GLSL_RECT_TOK_PUNCT)
         continue;
      if (s->src[s->tok[k].start] == o)
         depth++;
      else if (s->src[s->tok[k].start] == c && !--depth)
         return k;
   }
   return s->num_tok;
}

/* The single identifier argument @pos of the call whose '(' is @open, or
 * num_tok when it is anything else */
static size_t glsl_rect_call_arg(const struct glsl_rect_scan *s,
      size_t open, size_t close, unsigned pos)
{
   size_t k, start = open + 1;
   unsigned n = 0;
   int depth  = 0;
   for (k = open + 1; k <= close; k++)
   {
      char c = s->tok[k].type == GLSL_RECT_TOK_PUNCT
         ? s->src[s->tok[k].start] : 0;
      /* A define inside a call: not followed */
      if (s->tok[k].region != s->tok[open].region)
         return s->num_tok;
      if (c == '(' || c == '[')
         depth++;
      else if ((c == ')' || c == ']') && k != close)
         depth--;
      if ((c == ',' && !depth) || k == close)
      {
         if (n == pos)
            return (k == start + 1
                  && s->tok[start].type == GLSL_RECT_TOK_IDENT)
               ? start : s->num_tok;
         n++;
         start = k + 1;
      }
   }
   return s->num_tok;
}

/* Marks the tokens of functions' sampler parameters every call gives the
 * frame, by name in @frame or as such a parameter of its own caller */
static void glsl_rect_frame_params(struct glsl_rect_scan *s,
      const char *const *frame, unsigned num_frame)
{
   /* At most one function in five tokens, one parameter in two */
   size_t cap_f = s->num_tok / 5 + 1, cap_p = s->num_tok / 2 + 1;
   struct glsl_rect_func *func   = (struct glsl_rect_func*)
      malloc(cap_f * sizeof(*func));
   struct glsl_rect_param *param = (struct glsl_rect_param*)
      malloc(cap_p * sizeof(*param));
   unsigned nf = 0, np = 0, f, p;
   size_t i;
   int brace = 0;
   bool changed = true;

   if (!func || !param)
   {
      s->failed = true;
      goto done;
   }
   for (i = 0; i < s->num_tok; i++)
   {
      struct glsl_rect_token *t = &s->tok[i];
      t->brace = (unsigned short)(brace < 0 ? 0 : brace);
      if (t->region || t->type != GLSL_RECT_TOK_PUNCT)
         continue;
      if (s->src[t->start] == '{')
         brace++;
      else if (s->src[t->start] == '}')
         brace--;
   }
   /* Definitions: name ( ... ) { ... } at global scope */
   for (i = 0; i + 1 < s->num_tok; i++)
   {
      size_t close, end, k, start;
      unsigned pos = 0;
      int depth    = 0;
      if (     s->tok[i].region || s->tok[i].brace
            || s->tok[i].type != GLSL_RECT_TOK_IDENT
            || !glsl_rect_tok_is(s, i + 1, "("))
         continue;
      close = glsl_rect_match(s, i + 1, '(', ')');
      if (close + 1 >= s->num_tok || !glsl_rect_tok_is(s, close + 1, "{"))
         continue;
      end = glsl_rect_match(s, close + 1, '{', '}');
      if (end == s->num_tok || nf == cap_f)
      {
         s->failed = true;
         goto done;
      }
      for (f = 0; f < nf; f++)
         if (     s->tok[func[f].name].len == s->tok[i].len
               && !memcmp(s->src + s->tok[func[f].name].start,
                  s->src + s->tok[i].start, s->tok[i].len))
            func[f].overloaded = true;
      func[nf].name       = i;
      func[nf].open       = i + 1;
      func[nf].close      = close;
      func[nf].body       = close + 1;
      func[nf].end        = end;
      func[nf].overloaded = false;
      for (f = 0; f < nf; f++)
         if (func[f].overloaded && s->tok[func[f].name].len == s->tok[i].len
               && !memcmp(s->src + s->tok[func[f].name].start,
                  s->src + s->tok[i].start, s->tok[i].len))
            func[nf].overloaded = true;
      start = i + 2;
      for (k = i + 2; k <= close; k++)
      {
         char c = s->tok[k].type == GLSL_RECT_TOK_PUNCT
            ? s->src[s->tok[k].start] : 0;
         if (c == '(' || c == '[')
            depth++;
         else if ((c == ')' || c == ']') && k != close)
            depth--;
         if ((c == ',' && !depth) || k == close)
         {
            size_t j;
            for (j = start; j + 1 < k; j++)
               if (     glsl_rect_tok_is(s, j, "sampler2D")
                     && s->tok[j + 1].type == GLSL_RECT_TOK_IDENT)
               {
                  if (np == cap_p)
                  {
                     s->failed = true;
                     goto done;
                  }
                  param[np].func  = nf;
                  param[np].pos   = pos;
                  param[np].tok   = j + 1;
                  param[np].alive = true;
                  np++;
               }
            pos++;
            start = k + 1;
         }
      }
      nf++;
      i = end;
   }
   /* A define naming the parameter reads it wherever it is expanded,
    * out of sight of the function's body; unless the name is the frame
    * everywhere, as a parameter shadowing it must be */
   for (p = 0; p < np; p++)
   {
      const struct glsl_rect_token *pt = &s->tok[param[p].tok];
      if (func[param[p].func].overloaded)
         param[p].alive = false;
      if (glsl_rect_resolves_to(s, pt->start, pt->len, frame, num_frame,
               0) == 1)
         continue;
      for (i = 0; param[p].alive && i < s->num_tok; i++)
         if (     s->tok[i].region
               && s->tok[i].type == GLSL_RECT_TOK_IDENT
               && s->tok[i].len == pt->len
               && !memcmp(s->src + s->tok[i].start, s->src + pt->start,
                  pt->len))
            param[p].alive = false;
   }
   /* Each call must give the frame, or a caller's own such parameter */
   while (changed)
   {
      changed = false;
      for (p = 0; p < np; p++)
      {
         const struct glsl_rect_func *fn = &func[param[p].func];
         unsigned calls = 0;
         bool ok        = param[p].alive;
         for (i = 0; ok && i + 1 < s->num_tok; i++)
         {
            size_t close, a;
            unsigned q;
            bool good;
            if (     i == fn->name || s->tok[i].len != s->tok[fn->name].len
                  || memcmp(s->src + s->tok[i].start,
                     s->src + s->tok[fn->name].start, s->tok[i].len)
                  || !glsl_rect_tok_is(s, i + 1, "("))
               continue;
            /* Called from a define, or outside a function: not followed */
            if (s->tok[i].region || !s->tok[i].brace)
            {
               ok = false;
               break;
            }
            close = glsl_rect_match(s, i + 1, '(', ')');
            a     = close == s->num_tok ? close
               : glsl_rect_call_arg(s, i + 1, close, param[p].pos);
            good  = a < s->num_tok && glsl_rect_resolves_to(s,
                  s->tok[a].start, s->tok[a].len, frame, num_frame, 0) == 1;
            for (q = 0; !good && a < s->num_tok && q < np; q++)
            {
               const struct glsl_rect_func *caller = &func[param[q].func];
               good = param[q].alive && i > caller->body && i < caller->end
                  && s->tok[param[q].tok].len == s->tok[a].len
                  && !memcmp(s->src + s->tok[param[q].tok].start,
                     s->src + s->tok[a].start, s->tok[a].len);
            }
            ok = good;
            calls++;
         }
         if (param[p].alive && (!ok || !calls))
         {
            param[p].alive = false;
            changed        = true;
         }
      }
   }
   for (p = 0; p < np; p++)
   {
      const struct glsl_rect_func *fn = &func[param[p].func];
      const struct glsl_rect_token *pt = &s->tok[param[p].tok];
      if (!param[p].alive)
         continue;
      s->tok[param[p].tok].flags |= GLSL_RECT_FLAG_SKIP;
      for (i = fn->body; i < fn->end; i++)
         if (     s->tok[i].type == GLSL_RECT_TOK_IDENT
               && s->tok[i].len == pt->len
               && !memcmp(s->src + s->tok[i].start, s->src + pt->start,
                  pt->len))
            s->tok[i].flags |= GLSL_RECT_FLAG_FRAME;
      for (i = 0; i + 1 < s->num_tok; i++)
      {
         size_t close, a;
         if (     i == fn->name
               || s->tok[i].len != s->tok[fn->name].len
               || memcmp(s->src + s->tok[i].start,
                  s->src + s->tok[fn->name].start, s->tok[i].len)
               || !glsl_rect_tok_is(s, i + 1, "("))
            continue;
         close = glsl_rect_match(s, i + 1, '(', ')');
         a     = glsl_rect_call_arg(s, i + 1, close, param[p].pos);
         if (a < s->num_tok)
            s->tok[a].flags |= GLSL_RECT_FLAG_SKIP;
      }
   }
done:
   free(func);
   free(param);
}

/* A read replaced, or the helper put after a declaration */
struct glsl_rect_edit
{
   size_t start;
   size_t end;
   /* The coordinate's span; for a declaration, the frame's name */
   size_t arg;
   size_t arg_len;
   bool decl;
};

static const char *glsl_rect_helper(enum glsl_rect_mode mode)
{
   /* Nearest: the texel a sampler would pick, i = floor(u * size),
    * wrapped as it would, read at its centre */
#define GLSL_RECT_TEXEL \
   "   return RARCH_FRAME_READ(%s, t * " GLSL_RECT_NAME_RECT ".xy\n" \
   "         + " GLSL_RECT_NAME_RECT ".zw);\n"
   switch (mode)
   {
      case GLSL_RECT_LINEAR_EDGE:
         return
            "   return RARCH_FRAME_READ(%s, clamp(c * " GLSL_RECT_NAME_RECT ".xy\n"
            "         + " GLSL_RECT_NAME_RECT ".zw, " GLSL_RECT_NAME_CLAMP ".xy,\n"
            "         " GLSL_RECT_NAME_CLAMP ".zw));\n";
      case GLSL_RECT_NEAREST_EDGE:
         return
            "   RARCH_HP vec2 s = " GLSL_RECT_NAME_TEXELS ".xy;\n"
            "   RARCH_HP vec2 t = clamp(floor(c * s), vec2(0.0), s - 1.0);\n"
            GLSL_RECT_TEXEL;
      case GLSL_RECT_NEAREST_REPEAT:
         return
            "   RARCH_HP vec2 s = " GLSL_RECT_NAME_TEXELS ".xy;\n"
            "   RARCH_HP vec2 t = mod(floor(c * s), s);\n"
            GLSL_RECT_TEXEL;
      case GLSL_RECT_NEAREST_MIRROR:
         return
            "   RARCH_HP vec2 s = " GLSL_RECT_NAME_TEXELS ".xy;\n"
            "   RARCH_HP vec2 m = mod(floor(c * s), 2.0 * s);\n"
            "   RARCH_HP vec2 t = mix(m, 2.0 * s - 1.0 - m, step(s, m));\n"
            GLSL_RECT_TEXEL;
      case GLSL_RECT_NEAREST_BORDER:
         return
            "   RARCH_HP vec2 s = " GLSL_RECT_NAME_TEXELS ".xy;\n"
            "   RARCH_HP vec2 t = floor(c * s);\n"
            "   if (any(lessThan(t, vec2(0.0))) || any(greaterThanEqual(t, s)))\n"
            "      return vec4(0.0);\n"
            GLSL_RECT_TEXEL;
   }
#undef GLSL_RECT_TEXEL
   return NULL;
}

/* Once per stage: two frame names declared in one stage both hold the
 * frame, so the first's helper serves the second's reads too */
#define GLSL_RECT_PRELUDE \
   "\n#ifndef RARCH_FRAME_HELPER\n" \
   "#define RARCH_FRAME_HELPER\n" \
   "#ifndef RARCH_HP\n" \
   "#ifdef GL_ES\n" \
   "#if defined(GL_FRAGMENT_PRECISION_HIGH) || defined(VERTEX)\n" \
   "#define RARCH_HP highp\n" \
   "#else\n" \
   "#error The frame is not read in place without highp\n" \
   "#endif\n" \
   "#else\n" \
   "#define RARCH_HP\n" \
   "#endif\n" \
   "#endif\n" \
   "#ifndef RARCH_FRAME_READ\n" \
   "#if __VERSION__ >= 130\n" \
   "#define RARCH_FRAME_READ texture\n" \
   "#else\n" \
   "#define RARCH_FRAME_READ texture2D\n" \
   "#endif\n" \
   "#endif\n" \
   "uniform RARCH_HP vec4 " GLSL_RECT_NAME_RECT ";\n" \
   "uniform RARCH_HP vec4 " GLSL_RECT_NAME_CLAMP ";\n" \
   "uniform RARCH_HP vec4 " GLSL_RECT_NAME_TEXELS ";\n" \
   "vec4 RARCH_frame(RARCH_HP vec2 c)\n" \
   "{\n"
#define GLSL_RECT_POSTLUDE "}\n#endif\n"

static int glsl_rect_edit_cmp(const void *a, const void *b)
{
   const struct glsl_rect_edit *x = (const struct glsl_rect_edit*)a;
   const struct glsl_rect_edit *y = (const struct glsl_rect_edit*)b;
   return x->start < y->start ? -1 : x->start > y->start ? 1 : 0;
}

/* The source with every read of a frame sampler named in @frame going
 * through RARCH_frame(), malloc'd; NULL when it can't be. */
static char *glsl_rect_rewrite(const char *src, const char *const *frame,
      unsigned num_frame, enum glsl_rect_mode mode)
{
   struct glsl_rect_scan s;
   struct glsl_rect_edit *edit = NULL;
   size_t num_edit = 0, i, out_len, o, at;
   const char *helper = glsl_rect_helper(mode);
   char *buf = NULL, *out = NULL;
   int brace = 0, paren = 0;
   bool decl = false;

   if (!src || !helper || !num_frame)
      return NULL;
   memset(&s, 0, sizeof(s));
   i = strlen(src) + 1;
   if (!(buf = (char*)malloc(i)))
      return NULL;
   memcpy(buf, src, i);
   glsl_rect_blank_comments(buf);
   s.src = buf;
   glsl_rect_scan_source(&s);
   if (s.failed || !(edit = (struct glsl_rect_edit*)
            calloc(s.num_tok + 1, sizeof(*edit))))
      goto fail;
   glsl_rect_frame_params(&s, frame, num_frame);
   if (s.failed)
      goto fail;
   /* A frame or a read taken away again is followed nowhere */
   for (i = 0; i < s.num_undef; i++)
      if (     glsl_rect_resolves_to(&s, s.undef[i].name, s.undef[i].len,
                  frame, num_frame, 0)
            || glsl_rect_resolves_to(&s, s.undef[i].name, s.undef[i].len,
                  glsl_rect_reads, 4, 0))
         goto fail;

   for (i = 0; i < s.num_tok; i++)
   {
      const struct glsl_rect_token *t = &s.tok[i];
      int r;
      if (t->type == GLSL_RECT_TOK_PUNCT && !t->region)
      {
         char c = s.src[t->start];
         if (c == '{')
            brace++;
         else if (c == '}')
            brace--;
         else if (c == '(')
            paren++;
         else if (c == ')')
            paren--;
      }
      if (     t->type != GLSL_RECT_TOK_IDENT || t->alias_body
            || (t->flags & GLSL_RECT_FLAG_SKIP))
         continue;
      r = (t->flags & GLSL_RECT_FLAG_FRAME) ? 1
         : glsl_rect_resolves_to(&s, t->start, t->len, frame, num_frame, 0);
      if (r < 0)
         goto fail;
      if (!r)
         continue;
      /* Its declaration, at global scope, by a name for it */
      if (     i > 0 && glsl_rect_tok_is(&s, i - 1, "sampler2D"))
      {
         size_t k;
         if (t->region || brace || paren)
            goto fail;
         for (k = i + 1; k < s.num_tok && !glsl_rect_tok_is(&s, k, ";"); k++);
         if (k == s.num_tok || s.tok[k].region)
            goto fail;
         edit[num_edit].start   = s.tok[k].start + 1;
         edit[num_edit].end     = s.tok[k].start + 1;
         edit[num_edit].arg     = t->start;
         edit[num_edit].arg_len = t->len;
         edit[num_edit].decl    = true;
         num_edit++;
         decl = true;
         continue;
      }
      /* The first argument of a read */
      if (     i >= 2 && glsl_rect_tok_is(&s, i - 1, "(")
            && glsl_rect_tok_is(&s, i + 1, ",")
            && s.tok[i - 2].type == GLSL_RECT_TOK_IDENT
            && s.tok[i - 1].region == t->region
            && s.tok[i - 2].region == t->region
            && glsl_rect_resolves_to(&s, s.tok[i - 2].start, s.tok[i - 2].len,
               glsl_rect_reads, 4, 0) == 1)
      {
         size_t k, arg_end = 0;
         int depth = 0;
         for (k = i + 2; k < s.num_tok; k++)
         {
            if (s.tok[k].region != t->region)
               goto fail;
            if (s.tok[k].type != GLSL_RECT_TOK_PUNCT)
               continue;
            if (s.src[s.tok[k].start] == '(' || s.src[s.tok[k].start] == '[')
               depth++;
            else if (s.src[s.tok[k].start] == ']')
               depth--;
            else if (s.src[s.tok[k].start] == ')')
            {
               if (!depth)
                  break;
               depth--;
            }
            else if (s.src[s.tok[k].start] == ',' && !depth && !arg_end)
               arg_end = s.tok[k].start;
         }
         if (k == s.num_tok || k == i + 2)
            goto fail;
         if (!arg_end)
            arg_end = s.tok[k].start;
         edit[num_edit].start   = s.tok[i - 2].start;
         edit[num_edit].end     = s.tok[k].start + 1;
         edit[num_edit].arg     = s.tok[i + 2].start;
         edit[num_edit].arg_len = arg_end - s.tok[i + 2].start;
         edit[num_edit].decl    = false;
         num_edit++;
         continue;
      }
      goto fail;
   }
   if (!decl || brace || paren)
      goto fail;

   qsort(edit, num_edit, sizeof(*edit), glsl_rect_edit_cmp);
   out_len = strlen(buf) + 1;
   for (i = 0; i < num_edit; i++)
   {
      if (i && edit[i].start < edit[i - 1].end)
         goto fail;
      /* A coordinate spanning a directive line */
      if (!edit[i].decl)
      {
         size_t k;
         for (k = edit[i].start; k < edit[i].end; k++)
            if (buf[k] == '\n')
            {
               size_t j = k + 1;
               while (buf[j] == ' ' || buf[j] == '\t')
                  j++;
               if (buf[j] == '#' && (k == 0 || buf[k - 1] != '\\'))
                  goto fail;
            }
      }
      out_len += edit[i].decl
         ? strlen(GLSL_RECT_PRELUDE) + strlen(helper) + edit[i].arg_len
            + strlen(GLSL_RECT_POSTLUDE)
         : edit[i].arg_len + 16;
   }
   if (!(out = (char*)malloc(out_len)))
      goto fail;
   o  = 0;
   at = 0;
   for (i = 0; i < num_edit; i++)
   {
      memcpy(out + o, buf + at, edit[i].start - at);
      o += edit[i].start - at;
      if (edit[i].decl)
      {
         const char *pct = strstr(helper, "%s");
         size_t pre_len  = strlen(GLSL_RECT_PRELUDE);
         memcpy(out + o, GLSL_RECT_PRELUDE, pre_len);
         o += pre_len;
         memcpy(out + o, helper, (size_t)(pct - helper));
         o += (size_t)(pct - helper);
         memcpy(out + o, buf + edit[i].arg, edit[i].arg_len);
         o += edit[i].arg_len;
         memcpy(out + o, pct + 2, strlen(pct + 2));
         o += strlen(pct + 2);
         memcpy(out + o, GLSL_RECT_POSTLUDE, strlen(GLSL_RECT_POSTLUDE));
         o += strlen(GLSL_RECT_POSTLUDE);
      }
      else
      {
         memcpy(out + o, "RARCH_frame(", 12);
         o += 12;
         memcpy(out + o, buf + edit[i].arg, edit[i].arg_len);
         o += edit[i].arg_len;
         out[o++] = ')';
      }
      at = edit[i].end;
   }
   memcpy(out + o, buf + at, strlen(buf + at) + 1);
   free(edit);
   free(s.tok);
   free(s.alias);
   free(buf);
   return out;

fail:
   free(out);
   free(edit);
   free(s.tok);
   free(s.alias);
   free(buf);
   return NULL;
}

/* The three uniforms, RECT, CLAMP and TEXELS, into @out[12] for @mode
 * and the view at (@x, @y), @w by @h, from the top left of a frame
 * @frame_h rows high, in a @tex_w by @tex_h texture that holds the frame
 * from its first row with @flipped false, or from its last, as a
 * hardware frame's, with it true */
static void glsl_rect_values(enum glsl_rect_mode mode, bool flipped,
      unsigned x, unsigned y, unsigned w, unsigned h, unsigned frame_h,
      unsigned tex_w, unsigned tex_h, float *out)
{
   float W  = (float)tex_w;
   float H  = (float)tex_h;
   float x0 = (float)x;
   float y0 = flipped ? (float)frame_h - (float)y - (float)h : (float)y;
   float d  = GLSL_RECT_CLAMP_BIAS;
   memset(out, 0, 12 * sizeof(float));
   if (mode == GLSL_RECT_LINEAR_EDGE)
   {
      /* The coordinate into the rectangle */
      out[0] = (float)w / W;
      out[1] = (flipped ? -(float)h : (float)h) / H;
      out[2] = x0 / W;
      out[3] = (flipped ? y0 + (float)h : y0) / H;
      out[4] = (x0 + 0.5f + d) / W;
      out[5] = (y0 + 0.5f + d) / H;
      out[6] = (x0 + (float)w - 0.5f + d) / W;
      out[7] = (y0 + (float)h - 0.5f + d) / H;
   }
   else
   {
      /* A texel of the rectangle, its top row 0, to its centre */
      out[0] = 1.0f / W;
      out[1] = (flipped ? -1.0f : 1.0f) / H;
      out[2] = (x0 + 0.5f) / W;
      out[3] = (flipped ? y0 + (float)h - 0.5f : y0 + 0.5f) / H;
   }
   out[8] = (float)w;
   out[9] = (float)h;
}

#endif
