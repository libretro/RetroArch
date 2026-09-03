/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ryaml.c).
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

#include <stdlib.h>
#include <string.h>

#include <formats/ryaml.h>

/* The reader walks the document once, a line at a time, and appends to a
 * flat node array. Nesting is tracked by a stack of open containers, so
 * there is no recursion over document depth and no per-node allocation.
 *
 * Scalars borrow the caller's bytes wherever the value is a contiguous
 * subrange of the input, which is the overwhelming majority: a plain
 * scalar, and a quoted one with no escapes, both point straight at the
 * document. Only block scalars and escaped quoted scalars are copied,
 * into a chunked arena. The arena is chunked rather than reallocated
 * because nodes hold pointers into it, and a realloc would leave those
 * dangling.
 *
 * Node links are indices, not pointers, so the node array itself may be
 * reallocated as it grows.
 */

#define RYAML_CHUNK_MIN     (64 * 1024)
#define RYAML_NODES_MIN     256
#define RYAML_STACK_MIN     32

/* Character classes, one table lookup instead of a chain of compares.
 * The hot question in the line loop is "does this byte end a plain
 * scalar or a key", so the flags that matter are packed here. */
#define RYAML_C_SPACE       0x01  /* space or tab                       */
#define RYAML_C_BREAK       0x02  /* CR or LF                           */
#define RYAML_C_DIGIT       0x04
#define RYAML_C_BLANKBREAK  0x03  /* SPACE|BREAK                        */

static const uint8_t ryaml_cls[256] =
{
   /* 0x00 */ 0,0,0,0,0,0,0,0,0,RYAML_C_SPACE,RYAML_C_BREAK,0,0,RYAML_C_BREAK,0,0,
   /* 0x10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x20 */ RYAML_C_SPACE,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x30 */ RYAML_C_DIGIT,RYAML_C_DIGIT,RYAML_C_DIGIT,RYAML_C_DIGIT,
              RYAML_C_DIGIT,RYAML_C_DIGIT,RYAML_C_DIGIT,RYAML_C_DIGIT,
              RYAML_C_DIGIT,RYAML_C_DIGIT,0,0,0,0,0,0,
   /* 0x40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xA0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xB0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xC0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xD0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xE0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0xF0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define RYAML_IS_SPACE(c)  (ryaml_cls[(uint8_t)(c)] & RYAML_C_SPACE)
#define RYAML_IS_BREAK(c)  (ryaml_cls[(uint8_t)(c)] & RYAML_C_BREAK)
#define RYAML_IS_DIGIT(c)  (ryaml_cls[(uint8_t)(c)] & RYAML_C_DIGIT)

struct ryaml_node
{
   const char *key;
   const char *val;
   uint32_t    key_len;
   uint32_t    val_len;
   int32_t     first_child;
   int32_t     last_child;
   int32_t     next_sibling;
   int32_t     parent;
   uint32_t    num_children;
   uint8_t     type;
};

struct ryaml_chunk
{
   struct ryaml_chunk *next;
   size_t              used;
   size_t              cap;
};

struct ryaml
{
   struct ryaml_node  *nodes;
   struct ryaml_chunk *chunks;
   size_t              count;
   size_t              cap;
};

struct ryaml_frame
{
   int32_t node;
   int32_t indent;
};

struct ryaml_parser
{
   const char         *p;
   const char         *end;
   const char         *doc;
   ryaml_t            *y;
   struct ryaml_frame *stk;
   int                 sp;
   int                 stk_cap;
   int32_t             pending;
   int32_t             pending_indent;
   int                 err;
};

/* ---------------------------------------------------------------- */
/* arena                                                            */
/* ---------------------------------------------------------------- */

static char *ryaml_arena_alloc(ryaml_t *y, size_t n)
{
   struct ryaml_chunk *c = y->chunks;
   size_t              want;
   char               *base;

   if (c && (c->cap - c->used) >= n)
   {
      base       = ((char*)(c + 1)) + c->used;
      c->used   += n;
      return base;
   }

   want = RYAML_CHUNK_MIN;
   if (want < n)
      want = n;

   c = (struct ryaml_chunk*)malloc(sizeof(struct ryaml_chunk) + want);
   if (!c)
      return NULL;

   c->next   = y->chunks;
   c->cap    = want;
   c->used   = n;
   y->chunks = c;
   return (char*)(c + 1);
}

/* ---------------------------------------------------------------- */
/* nodes                                                            */
/* ---------------------------------------------------------------- */

static int32_t ryaml_node_new(struct ryaml_parser *ps, int32_t parent, int type)
{
   ryaml_t           *y = ps->y;
   struct ryaml_node *n;
   int32_t            idx;

   if (y->count == y->cap)
   {
      size_t             ncap = y->cap ? (y->cap * 2) : RYAML_NODES_MIN;
      struct ryaml_node *tmp  = (struct ryaml_node*)realloc(y->nodes,
            ncap * sizeof(struct ryaml_node));
      if (!tmp)
      {
         ps->err = 1;
         return RYAML_NONE;
      }
      y->nodes = tmp;
      y->cap   = ncap;
   }

   idx               = (int32_t)y->count++;
   n                 = &y->nodes[idx];
   n->key            = NULL;
   n->val            = NULL;
   n->key_len        = 0;
   n->val_len        = 0;
   n->first_child    = RYAML_NONE;
   n->last_child     = RYAML_NONE;
   n->next_sibling   = RYAML_NONE;
   n->parent         = parent;
   n->num_children   = 0;
   n->type           = (uint8_t)type;

   if (parent >= 0)
   {
      struct ryaml_node *pn = &y->nodes[parent];
      if (pn->last_child >= 0)
         y->nodes[pn->last_child].next_sibling = idx;
      else
         pn->first_child = idx;
      pn->last_child = idx;
      pn->num_children++;
   }
   return idx;
}

static int ryaml_push(struct ryaml_parser *ps, int32_t node, int32_t indent)
{
   if (ps->sp + 1 >= ps->stk_cap)
   {
      int                 ncap = ps->stk_cap ? (ps->stk_cap * 2) : RYAML_STACK_MIN;
      struct ryaml_frame *tmp  = (struct ryaml_frame*)realloc(ps->stk,
            (size_t)ncap * sizeof(struct ryaml_frame));
      if (!tmp)
      {
         ps->err = 1;
         return 0;
      }
      ps->stk     = tmp;
      ps->stk_cap = ncap;
   }
   ps->sp++;
   ps->stk[ps->sp].node   = node;
   ps->stk[ps->sp].indent = indent;
   return 1;
}

/* ---------------------------------------------------------------- */
/* scalars                                                          */
/* ---------------------------------------------------------------- */

/* Trailing spaces are not part of a plain scalar, and neither is a
 * comment introduced by " #". The scan stops at the first of those. */
static void ryaml_plain_extent(const char *s, const char *e,
      const char **out, uint32_t *out_len)
{
   const char *stop = e;
   const char *h    = s;

   /* The end of a plain scalar is either the line end or a " #" that
    * starts a comment. Reaching for the '#' directly beats stepping
    * over the run of ordinary bytes in front of it, and the run is the
    * common case: most values here are followed by a comment. */
   for (;;)
   {
      h = (const char*)memchr(h, '#', (size_t)(e - h));
      if (!h)
         break;
      if (h > s && RYAML_IS_SPACE(h[-1]))
      {
         stop = h;
         break;
      }
      h++;
      if (h >= e)
         break;
   }

   while (stop > s && RYAML_IS_SPACE(stop[-1]))
      stop--;

   *out     = s;
   *out_len = (uint32_t)(stop - s);
}

/* A quoted scalar that contains no escape needs no copy; one that does
 * is rebuilt in the arena. Single quotes only ever escape themselves. */
static int ryaml_quoted(struct ryaml_parser *ps, const char *s, const char *e,
      char quote, const char **out, uint32_t *out_len, const char **after)
{
   const char *q      = s + 1;
   const char *start  = q;
   int         escaped = 0;

   /* The overwhelming case is a quoted scalar with nothing to unescape,
    * and that one can be recognised with two scans and then borrowed
    * whole. Only if an escape turns up does the byte-wise walk below
    * have to run. There is no newline inside [start, e): e is the line
    * end. */
   {
      const char *close = (const char*)memchr(start, quote,
            (size_t)(e - start));
      if (close)
      {
         int has_esc = 0;
         if (quote == '"')
            has_esc = (memchr(start, '\\', (size_t)(close - start)) != NULL);
         else if ((close + 1) < e && close[1] == '\'')
            has_esc = 1;

         if (!has_esc)
         {
            *out     = start;
            *out_len = (uint32_t)(close - start);
            *after   = close + 1;
            return 1;
         }
      }
      else
      {
         ps->err = 1;
         return 0;
      }
   }

   while (q < e)
   {
      char c = *q;
      if (RYAML_IS_BREAK(c))
         break;
      if (quote == '"' && c == '\\')
      {
         escaped = 1;
         q += 2;
         continue;
      }
      if (c == quote)
      {
         if (quote == '\'' && (q + 1) < e && q[1] == '\'')
         {
            escaped = 1;
            q      += 2;
            continue;
         }
         break;
      }
      q++;
   }

   if (q >= e || *q != quote)
   {
      ps->err = 1;
      return 0;
    }

   if (!escaped)
   {
      *out     = start;
      *out_len = (uint32_t)(q - start);
      *after   = q + 1;
      return 1;
   }

   {
      size_t      max = (size_t)(q - start);
      char       *dst = ryaml_arena_alloc(ps->y, max ? max : 1);
      char       *w;
      const char *r   = start;

      if (!dst)
      {
         ps->err = 1;
         return 0;
      }
      w = dst;

      while (r < q)
      {
         if (quote == '\'')
         {
            if (*r == '\'' && (r + 1) < q && r[1] == '\'')
            {
               *w++ = '\'';
               r   += 2;
               continue;
            }
            *w++ = *r++;
            continue;
         }
         if (*r == '\\' && (r + 1) < q)
         {
            char c = r[1];
            r += 2;
            switch (c)
            {
               case 'n':  *w++ = '\n'; break;
               case 't':  *w++ = '\t'; break;
               case 'r':  *w++ = '\r'; break;
               case '0':  *w++ = '\0'; break;
               case 'a':  *w++ = '\a'; break;
               case 'b':  *w++ = '\b'; break;
               case 'f':  *w++ = '\f'; break;
               case 'v':  *w++ = '\v'; break;
               case 'e':  *w++ = 27;   break;
               case '/':  *w++ = '/';  break;
               case '\\': *w++ = '\\'; break;
               case '"':  *w++ = '"';  break;
               case '\'': *w++ = '\''; break;
               default:   *w++ = c;    break;
            }
            continue;
         }
         *w++ = *r++;
      }
      *out     = dst;
      *out_len = (uint32_t)(w - dst);
      *after   = q + 1;
   }
   return 1;
}

/* ---------------------------------------------------------------- */
/* line helpers                                                     */
/* ---------------------------------------------------------------- */

static const char *ryaml_line_end(const char *p, const char *end)
{
   const char *nl = (const char*)memchr(p, '\n', (size_t)(end - p));
   return nl ? nl : end;
}

static const char *ryaml_next_line(const char *le, const char *end)
{
   return (le < end) ? (le + 1) : end;
}

/* Indentation is spaces. A tab inside it is an error rather than a
 * silent width guess. */
static int ryaml_indent_of(const char *p, const char *le, const char **content)
{
   const char *q = p;
   while (q < le && *q == ' ')
      q++;
   *content = q;
   return (int)(q - p);
}

static int ryaml_line_is_blank(const char *c, const char *le)
{
   while (c < le && RYAML_IS_SPACE(*c))
      c++;
   if (c >= le)
      return 1;
   return (*c == '\r');
}

/* ---------------------------------------------------------------- */
/* block scalars                                                    */
/* ---------------------------------------------------------------- */

/* Two passes: measure, then fill. Measuring costs a second walk over
 * the block but avoids a growable temporary and the copy out of it. */
static int ryaml_block_scalar(struct ryaml_parser *ps, const char *hdr,
      const char *le, int key_indent, const char **out, uint32_t *out_len)
{
   int         fold    = (*hdr == '>');
   int         chomp   = 0;      /* -1 strip, 0 clip, +1 keep */
   int         explicit_ind = 0;
   const char *q       = hdr + 1;
   const char *scan;
   const char *body;
   int         content_indent = -1;
   size_t      need    = 0;
   char       *dst;
   char       *w;
   size_t      trailing_nl = 0;

   while (q < le && !RYAML_IS_BREAK(*q))
   {
      if (*q == '-')
         chomp = -1;
      else if (*q == '+')
         chomp = 1;
      else if (RYAML_IS_DIGIT(*q))
         explicit_ind = explicit_ind * 10 + (*q - '0');
      else if (*q == '#' || RYAML_IS_SPACE(*q))
         break;
      else
      {
         ps->err = 1;
         return 0;
      }
      q++;
   }

   body = ryaml_next_line(le, ps->end);

   if (explicit_ind > 0)
      content_indent = key_indent + explicit_ind;

   /* pass one: find the block's indentation and its total size */
   scan = body;
   while (scan < ps->end)
   {
      const char *lend = ryaml_line_end(scan, ps->end);
      const char *c;
      int         ind  = ryaml_indent_of(scan, lend, &c);

      if (ryaml_line_is_blank(c, lend))
      {
         need++;
         scan = ryaml_next_line(lend, ps->end);
         continue;
      }
      if (content_indent < 0)
      {
         if (ind <= key_indent)
            break;
         content_indent = ind;
      }
      if (ind < content_indent)
         break;
      need += (size_t)(lend - scan) - (size_t)content_indent + 1;
      scan  = ryaml_next_line(lend, ps->end);
   }

   if (content_indent < 0)
   {
      /* header with no body: an empty scalar */
      *out     = "";
      *out_len = 0;
      ps->p    = body;
      return 1;
   }

   dst = ryaml_arena_alloc(ps->y, need ? need : 1);
   if (!dst)
   {
      ps->err = 1;
      return 0;
   }
   w = dst;

   /* pass two: copy, dedented */
   scan = body;
   while (scan < ps->end)
   {
      const char *lend = ryaml_line_end(scan, ps->end);
      const char *c;
      const char *cut;
      int         ind  = ryaml_indent_of(scan, lend, &c);
      size_t      n;

      if (ryaml_line_is_blank(c, lend))
      {
         *w++ = '\n';
         scan = ryaml_next_line(lend, ps->end);
         continue;
      }
      if (ind < content_indent)
         break;

      cut = scan + content_indent;
      n   = (size_t)(lend - cut);
      /* a CR belongs to the line ending, not the content */
      if (n && cut[n - 1] == '\r')
         n--;
      memcpy(w, cut, n);
      w += n;
      *w++ = '\n';
      scan = ryaml_next_line(lend, ps->end);
   }

   /* folding turns a single break between two non-empty lines into a
    * space; a run of breaks loses one and keeps the rest */
   if (fold && w > dst)
   {
      char *r    = dst;
      char *wr   = dst;
      char *lim  = w;

      while (r < lim)
      {
         if (*r == '\n')
         {
            size_t runs = 0;
            while (r < lim && *r == '\n')
            {
               runs++;
               r++;
            }
            if (r >= lim)
            {
               while (runs--)
                  *wr++ = '\n';
               break;
            }
            if (runs == 1)
               *wr++ = ' ';
            else
            {
               while (--runs)
                  *wr++ = '\n';
            }
            continue;
         }
         *wr++ = *r++;
      }
      w = wr;
   }

   while (w > dst && w[-1] == '\n')
   {
      trailing_nl++;
      w--;
   }
   if (chomp == 0 && trailing_nl > 0)
      *w++ = '\n';
   else if (chomp > 0)
   {
      while (trailing_nl--)
         *w++ = '\n';
   }

   *out     = dst;
   *out_len = (uint32_t)(w - dst);
   ps->p    = scan;
   return 1;
}

/* ---------------------------------------------------------------- */
/* flow collections                                                 */
/* ---------------------------------------------------------------- */

static const char *ryaml_flow(struct ryaml_parser *ps, int32_t parent,
      const char *s, const char *e);

static const char *ryaml_flow_skip_ws(const char *s, const char *e)
{
   while (s < e && RYAML_IS_SPACE(*s))
      s++;
   return s;
}

/* A flow scalar ends at a comma or at the bracket that closes its
 * collection; inside a mapping it may also end at the ':' of its key. */
static const char *ryaml_flow_scalar(struct ryaml_parser *ps, const char *s,
      const char *e, const char **out, uint32_t *out_len, int stop_colon)
{
   const char *q;
   const char *last;

   if (*s == '"' || *s == '\'')
   {
      const char *after;
      if (!ryaml_quoted(ps, s, e, *s, out, out_len, &after))
         return NULL;
      return after;
   }

   q    = s;
   last = s;
   while (q < e)
   {
      char c = *q;
      if (c == ',' || c == '}' || c == ']')
         break;
      if (stop_colon && c == ':' &&
            ((q + 1) >= e || RYAML_IS_SPACE(q[1]) || q[1] == ','
             || q[1] == '}' || q[1] == ']'))
         break;
      if (RYAML_IS_BREAK(c))
         break;
      if (!RYAML_IS_SPACE(c))
         last = q + 1;
      q++;
   }
   *out     = s;
   *out_len = (uint32_t)(last - s);
   return q;
}

static const char *ryaml_flow(struct ryaml_parser *ps, int32_t parent,
      const char *s, const char *e)
{
   char close = (*s == '{') ? '}' : ']';
   int  ismap = (*s == '{');

   ps->y->nodes[parent].type = (uint8_t)(ismap ? RYAML_TYPE_MAP : RYAML_TYPE_SEQ);
   s++;

   for (;;)
   {
      int32_t     child;
      const char *k;
      uint32_t    klen;

      s = ryaml_flow_skip_ws(s, e);
      if (s >= e)
      {
         ps->err = 1;
         return NULL;
      }
      if (*s == close)
         return s + 1;

      child = ryaml_node_new(ps, parent, RYAML_TYPE_SCALAR);
      if (child < 0)
         return NULL;

      if (ismap)
      {
         s = ryaml_flow_scalar(ps, s, e, &k, &klen, 1);
         if (!s)
            return NULL;
         ps->y->nodes[child].key     = k;
         ps->y->nodes[child].key_len = klen;

         s = ryaml_flow_skip_ws(s, e);
         if (s >= e || *s != ':')
         {
            ps->err = 1;
            return NULL;
         }
         s = ryaml_flow_skip_ws(s + 1, e);
         if (s >= e)
         {
            ps->err = 1;
            return NULL;
         }
      }

      if (*s == '{' || *s == '[')
      {
         s = ryaml_flow(ps, child, s, e);
         if (!s)
            return NULL;
      }
      else
      {
         const char *v;
         uint32_t    vlen;
         s = ryaml_flow_scalar(ps, s, e, &v, &vlen, 0);
         if (!s)
            return NULL;
         ps->y->nodes[child].val     = v;
         ps->y->nodes[child].val_len = vlen;
      }

      s = ryaml_flow_skip_ws(s, e);
      if (s >= e)
      {
         ps->err = 1;
         return NULL;
      }
      if (*s == ',')
      {
         s++;
         continue;
      }
      if (*s == close)
         return s + 1;

      ps->err = 1;
      return NULL;
   }
}

/* ---------------------------------------------------------------- */
/* value dispatch                                                   */
/* ---------------------------------------------------------------- */

/* Attaches whatever follows a "key:" or a "-" to @node. Returns 0 on
 * error. Sets *deferred when the value is absent, which means the node
 * becomes a container if the next line is deeper, and a null scalar if
 * it is not. */
static int ryaml_value(struct ryaml_parser *ps, int32_t node, int indent,
      const char *v, const char *le, int *deferred)
{
   struct ryaml_node *n;

   *deferred = 0;

   while (v < le && RYAML_IS_SPACE(*v))
      v++;

   if (v >= le || *v == '\r' || *v == '#')
   {
      *deferred = 1;
      return 1;
   }

   n = &ps->y->nodes[node];

   if (*v == '|' || *v == '>')
   {
      const char *out;
      uint32_t    outlen;
      if (!ryaml_block_scalar(ps, v, le, indent, &out, &outlen))
         return 0;
      n          = &ps->y->nodes[node];
      n->val     = out;
      n->val_len = outlen;
      n->type    = RYAML_TYPE_SCALAR;
      return -1;   /* cursor already advanced past the block */
   }

   if (*v == '{' || *v == '[')
   {
      if (!ryaml_flow(ps, node, v, le))
         return 0;
      return 1;
   }

   if (*v == '"' || *v == '\'')
   {
      const char *out;
      const char *after;
      uint32_t    outlen;
      if (!ryaml_quoted(ps, v, le, *v, &out, &outlen, &after))
         return 0;
      n          = &ps->y->nodes[node];
      n->val     = out;
      n->val_len = outlen;
      n->type    = RYAML_TYPE_SCALAR;
      return 1;
   }

   {
      const char *out;
      uint32_t    outlen;
      ryaml_plain_extent(v, le, &out, &outlen);
      if (outlen && out[outlen - 1] == '\r')
         outlen--;
      n->val     = out;
      n->val_len = outlen;
      n->type    = RYAML_TYPE_SCALAR;
   }
   return 1;
}

/* Finds the ':' that separates a block mapping key from its value. A
 * colon only counts when a space or the line end follows it, so a key
 * like "12:30" or a URL in a plain value does not split. */
static const char *ryaml_find_key_colon(const char *s, const char *le)
{
   const char *q   = s;
   const char *lim = le;

   if (*q == '"' || *q == '\'')
   {
      char quote = *q;
      q++;
      while (q < le)
      {
         if (quote == '"' && *q == '\\')
         {
            q += 2;
            continue;
         }
         if (*q == quote)
            break;
         q++;
      }
      if (q >= le)
         return NULL;
      q++;
      while (q < le && RYAML_IS_SPACE(*q))
         q++;
      if (q < le && *q == ':')
         return q;
      return NULL;
   }

   /* Anything from a " #" onwards is a comment and cannot hold the
    * key's colon, so the search is bounded once here rather than
    * re-checked at every candidate colon. */
   {
      const char *h = s;
      for (;;)
      {
         h = (const char*)memchr(h, '#', (size_t)(lim - h));
         if (!h)
            break;
         if (h > s && RYAML_IS_SPACE(h[-1]))
         {
            lim = h;
            break;
         }
         h++;
      }
   }

   while (q < lim)
   {
      q = (const char*)memchr(q, ':', (size_t)(lim - q));
      if (!q)
         return NULL;
      if ((q + 1) >= le || RYAML_IS_SPACE(q[1]) || q[1] == '\r')
         return q;
      q++;
   }
   return NULL;
}

/* ---------------------------------------------------------------- */
/* main loop                                                        */
/* ---------------------------------------------------------------- */

static int ryaml_run(struct ryaml_parser *ps)
{
   int32_t root = ryaml_node_new(ps, RYAML_NONE, RYAML_TYPE_NONE);

   if (root < 0)
      return 0;

   ps->sp             = 0;
   ps->stk[0].node    = root;
   ps->stk[0].indent  = -1;
   ps->pending        = RYAML_NONE;
   ps->pending_indent = -1;

   while (ps->p < ps->end)
   {
      const char *le = ryaml_line_end(ps->p, ps->end);
      const char *c;
      int         indent = ryaml_indent_of(ps->p, le, &c);
      int32_t     cur;
      int         is_seq_item;

      if (ryaml_line_is_blank(c, le) || *c == '#')
      {
         ps->p = ryaml_next_line(le, ps->end);
         continue;
      }
      if (c < le && *c == '\t')
         return 0;

      is_seq_item = (*c == '-' && ((c + 1) >= le || RYAML_IS_SPACE(c[1])
               || c[1] == '\r'));

      /* A deferred key becomes a container once something deeper turns
       * up, or a sequence sitting at the key's own indent. */
      if (ps->pending >= 0)
      {
         if (indent > ps->pending_indent
               || (is_seq_item && indent == ps->pending_indent))
         {
            ps->y->nodes[ps->pending].type =
               (uint8_t)(is_seq_item ? RYAML_TYPE_SEQ : RYAML_TYPE_MAP);
            if (!ryaml_push(ps, ps->pending, indent))
               return 0;
         }
         ps->pending = RYAML_NONE;
      }

      while (ps->sp > 0 && indent < ps->stk[ps->sp].indent)
         ps->sp--;

      /* The root's own indentation is whatever its first entry uses;
       * fix it before the check below, so that a line indented past it
       * with nothing to attach to is caught here rather than silently
       * joining the root. */
      if (ps->sp == 0 && ps->y->nodes[root].type == RYAML_TYPE_NONE)
      {
         ps->y->nodes[root].type =
            (uint8_t)(is_seq_item ? RYAML_TYPE_SEQ : RYAML_TYPE_MAP);
         ps->stk[0].indent = indent;
      }

      if (indent > ps->stk[ps->sp].indent)
         return 0;

      cur = ps->stk[ps->sp].node;

      if (is_seq_item)
      {
         int32_t     item;
         const char *rest = c + 1;
         const char *colon;

         if (ps->y->nodes[cur].type != RYAML_TYPE_SEQ)
            return 0;

         item = ryaml_node_new(ps, cur, RYAML_TYPE_SCALAR);
         if (item < 0)
            return 0;

         while (rest < le && RYAML_IS_SPACE(*rest))
            rest++;

         colon = (rest < le && *rest != '{' && *rest != '[')
            ? ryaml_find_key_colon(rest, le) : NULL;

         if (colon)
         {
            /* compact mapping: the dash and the first key share a line,
             * and the mapping's keys line up under that first key */
            int32_t     kv;
            int         kind = (int)(rest - c) + indent;
            const char *kstr;
            uint32_t    klen;
            int         deferred;
            int         r;

            ps->y->nodes[item].type = RYAML_TYPE_MAP;
            if (!ryaml_push(ps, item, kind))
               return 0;

            kv = ryaml_node_new(ps, item, RYAML_TYPE_SCALAR);
            if (kv < 0)
               return 0;

            if (*rest == '"' || *rest == '\'')
            {
               const char *after;
               if (!ryaml_quoted(ps, rest, le, *rest, &kstr, &klen, &after))
                  return 0;
            }
            else
            {
               kstr = rest;
               klen = (uint32_t)(colon - rest);
               while (klen && RYAML_IS_SPACE(kstr[klen - 1]))
                  klen--;
            }
            ps->y->nodes[kv].key     = kstr;
            ps->y->nodes[kv].key_len = klen;

            r = ryaml_value(ps, kv, kind, colon + 1, le, &deferred);
            if (r == 0)
               return 0;
            if (deferred)
            {
               ps->pending        = kv;
               ps->pending_indent = kind;
            }
            if (r < 0)
               continue;
         }
         else if (rest < le && *rest != '\r' && *rest != '#')
         {
            int deferred;
            int r = ryaml_value(ps, item, indent, rest, le, &deferred);
            if (r == 0)
               return 0;
            if (r < 0)
               continue;
         }
         else
         {
            ps->pending        = item;
            ps->pending_indent = indent;
         }

         ps->p = ryaml_next_line(le, ps->end);
         continue;
      }

      /* block mapping entry */
      {
         const char *colon = ryaml_find_key_colon(c, le);
         int32_t     kv;
         const char *kstr;
         uint32_t    klen;
         int         deferred;
         int         r;

         if (!colon)
            return 0;
         if (ps->y->nodes[cur].type != RYAML_TYPE_MAP)
            return 0;

         kv = ryaml_node_new(ps, cur, RYAML_TYPE_SCALAR);
         if (kv < 0)
            return 0;

         if (*c == '"' || *c == '\'')
         {
            const char *after;
            if (!ryaml_quoted(ps, c, le, *c, &kstr, &klen, &after))
               return 0;
         }
         else
         {
            kstr = c;
            klen = (uint32_t)(colon - c);
            while (klen && RYAML_IS_SPACE(kstr[klen - 1]))
               klen--;
         }
         ps->y->nodes[kv].key     = kstr;
         ps->y->nodes[kv].key_len = klen;

         r = ryaml_value(ps, kv, indent, colon + 1, le, &deferred);
         if (r == 0)
            return 0;
         if (deferred)
         {
            ps->pending        = kv;
            ps->pending_indent = indent;
         }
         if (r < 0)
            continue;
      }

      ps->p = ryaml_next_line(le, ps->end);
   }

   return !ps->err;
}

/* ---------------------------------------------------------------- */
/* public entry points                                              */
/* ---------------------------------------------------------------- */

ryaml_t *ryaml_parse_ex(const char *buf, size_t len,
      size_t *err_line, size_t *err_col)
{
   struct ryaml_parser ps;
   ryaml_t            *y;

   if (!buf)
      return NULL;

   y = (ryaml_t*)calloc(1, sizeof(*y));
   if (!y)
      return NULL;

   memset(&ps, 0, sizeof(ps));
   ps.y   = y;
   ps.doc = buf;
   ps.p   = buf;
   ps.end = buf + len;

   /* a UTF-8 byte order mark is not content */
   if (len >= 3 && (uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB
         && (uint8_t)buf[2] == 0xBF)
      ps.p += 3;

   ps.stk = (struct ryaml_frame*)malloc(
         RYAML_STACK_MIN * sizeof(struct ryaml_frame));
   if (!ps.stk)
   {
      free(y);
      return NULL;
   }
   ps.stk_cap = RYAML_STACK_MIN;

   if (!ryaml_run(&ps))
   {
      if (err_line || err_col)
      {
         const char *q    = buf;
         size_t      line = 1;
         const char *ls   = buf;
         while (q < ps.p)
         {
            if (*q == '\n')
            {
               line++;
               ls = q + 1;
            }
            q++;
         }
         if (err_line)
            *err_line = line;
         if (err_col)
            *err_col = (size_t)(ps.p - ls) + 1;
      }
      free(ps.stk);
      ryaml_free(y);
      return NULL;
   }

   free(ps.stk);
   return y;
}

ryaml_t *ryaml_parse(const char *buf, size_t len)
{
   return ryaml_parse_ex(buf, len, NULL, NULL);
}

void ryaml_free(ryaml_t *y)
{
   struct ryaml_chunk *c;

   if (!y)
      return;

   c = y->chunks;
   while (c)
   {
      struct ryaml_chunk *next = c->next;
      free(c);
      c = next;
   }
   free(y->nodes);
   free(y);
}

/* ---------------------------------------------------------------- */
/* navigation                                                       */
/* ---------------------------------------------------------------- */

#define RYAML_VALID(y, n) ((y) && (n) >= 0 && (size_t)(n) < (y)->count)

int ryaml_root(const ryaml_t *y)
{
   return (y && y->count) ? 0 : RYAML_NONE;
}

size_t ryaml_count(const ryaml_t *y)
{
   return y ? y->count : 0;
}

int ryaml_first_child(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? y->nodes[node].first_child : RYAML_NONE;
}

int ryaml_last_child(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? y->nodes[node].last_child : RYAML_NONE;
}

int ryaml_next_sibling(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? y->nodes[node].next_sibling : RYAML_NONE;
}

int ryaml_parent(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? y->nodes[node].parent : RYAML_NONE;
}

size_t ryaml_num_children(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? (size_t)y->nodes[node].num_children : 0;
}

int ryaml_find_child_len(const ryaml_t *y, int node,
      const char *key, size_t key_len)
{
   int32_t n;

   if (!RYAML_VALID(y, node) || !key)
      return RYAML_NONE;

   for (n = y->nodes[node].first_child; n >= 0; n = y->nodes[n].next_sibling)
   {
      const struct ryaml_node *c = &y->nodes[n];
      if (c->key_len == (uint32_t)key_len && c->key
            && memcmp(c->key, key, key_len) == 0)
         return n;
   }
   return RYAML_NONE;
}

int ryaml_find_child(const ryaml_t *y, int node, const char *key)
{
   return key ? ryaml_find_child_len(y, node, key, strlen(key)) : RYAML_NONE;
}

int ryaml_has_child(const ryaml_t *y, int node, const char *key)
{
   return ryaml_find_child(y, node, key) >= 0;
}

int ryaml_type(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) ? (int)y->nodes[node].type : RYAML_TYPE_NONE;
}

int ryaml_is_map(const ryaml_t *y, int node)
{
   return ryaml_type(y, node) == RYAML_TYPE_MAP;
}

int ryaml_is_seq(const ryaml_t *y, int node)
{
   return ryaml_type(y, node) == RYAML_TYPE_SEQ;
}

int ryaml_has_children(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) && y->nodes[node].first_child >= 0;
}

int ryaml_has_val(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) && y->nodes[node].val != NULL;
}

int ryaml_has_key(const ryaml_t *y, int node)
{
   return RYAML_VALID(y, node) && y->nodes[node].key != NULL;
}

const char *ryaml_key(const ryaml_t *y, int node, size_t *len)
{
   if (!RYAML_VALID(y, node) || !y->nodes[node].key)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   if (len)
      *len = y->nodes[node].key_len;
   return y->nodes[node].key;
}

const char *ryaml_val(const ryaml_t *y, int node, size_t *len)
{
   if (!RYAML_VALID(y, node) || !y->nodes[node].val)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   if (len)
      *len = y->nodes[node].val_len;
   return y->nodes[node].val;
}

/* ---------------------------------------------------------------- */
/* scalar conversions                                               */
/* ---------------------------------------------------------------- */

static int ryaml_to_u64(const char *s, size_t len, uint32_t *out)
{
   uint32_t acc  = 0;
   size_t   i    = 0;
   int      base = 10;

   if (!len)
      return 0;

   if (len > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
   {
      base = 16;
      i    = 2;
   }

   if (i >= len)
      return 0;

   for (; i < len; i++)
   {
      char     c = s[i];
      uint32_t d;

      if (RYAML_IS_DIGIT(c))
         d = (uint32_t)(c - '0');
      else if (base == 16 && c >= 'a' && c <= 'f')
         d = (uint32_t)(c - 'a' + 10);
      else if (base == 16 && c >= 'A' && c <= 'F')
         d = (uint32_t)(c - 'A' + 10);
      else
         return 0;

      acc = acc * (uint32_t)base + d;
   }
   *out = acc;
   return 1;
}

int ryaml_val_uint(const ryaml_t *y, int node, uint32_t *out)
{
   size_t      len;
   const char *v = ryaml_val(y, node, &len);
   uint32_t    tmp;

   if (!v || !out)
      return 0;
   if (!ryaml_to_u64(v, len, &tmp))
      return 0;
   *out = tmp;
   return 1;
}

int ryaml_val_int(const ryaml_t *y, int node, int *out)
{
   size_t      len;
   const char *v = ryaml_val(y, node, &len);
   uint32_t    tmp;
   int         neg = 0;

   if (!v || !out || !len)
      return 0;

   if (*v == '-' || *v == '+')
   {
      neg = (*v == '-');
      v++;
      len--;
   }
   if (!ryaml_to_u64(v, len, &tmp))
      return 0;

   *out = neg ? -(int)tmp : (int)tmp;
   return 1;
}

int ryaml_val_bool(const ryaml_t *y, int node, int *out)
{
   size_t      len;
   const char *v = ryaml_val(y, node, &len);
   char        buf[8];
   size_t      i;

   if (!v || !out || !len || len >= sizeof(buf))
      return 0;

   for (i = 0; i < len; i++)
   {
      char c = v[i];
      if (c >= 'A' && c <= 'Z')
         c = (char)(c - 'A' + 'a');
      buf[i] = c;
   }
   buf[len] = '\0';

   if (!strcmp(buf, "true") || !strcmp(buf, "yes") || !strcmp(buf, "on"))
   {
      *out = 1;
      return 1;
   }
   if (!strcmp(buf, "false") || !strcmp(buf, "no") || !strcmp(buf, "off"))
   {
      *out = 0;
      return 1;
   }
   return 0;
}

int ryaml_val_equals(const ryaml_t *y, int node, const char *str)
{
   size_t      len;
   const char *v = ryaml_val(y, node, &len);
   size_t      slen;

   if (!v || !str)
      return 0;
   slen = strlen(str);
   return (slen == len) && (memcmp(v, str, len) == 0);
}
