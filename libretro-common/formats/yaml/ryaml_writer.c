/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ryaml_writer.c).
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

#include <formats/ryaml_writer.h>

/* Output is staged in one buffer: written out through the callback when
 * it fills, or grown and kept when there is no callback. Nothing else is
 * allocated per call; the stack of open collections grows only when the
 * document nests deeper than it has before. */
#define RYW_STAGE        4096
#define RYW_DEPTH_MIN    16

enum ryw_kind { RYW_MAP, RYW_SEQ, RYW_FLOW };

/* How a collection began, which decides where its first entry goes */
enum ryw_open
{
   RYW_ROOT,       /* the document: at the start of the first line     */
   RYW_AFTER_KEY,  /* "key:" written: on the next line, indented       */
   RYW_AFTER_DASH  /* "- " written: on the same line                   */
};

struct ryw_frame
{
   unsigned indent;   /* column of the entries */
   unsigned count;    /* entries written       */
   unsigned char kind;
   unsigned char open;
};

struct ryaml_writer
{
   ryaml_writer_io_t io;
   void             *user;
   char             *buf;
   size_t            len;
   size_t            cap;
   struct ryw_frame *stk;
   int               sp;        /* -1: nothing open */
   int               stk_cap;
   int               after_key; /* a key waits for its value */
   int               done;      /* the document has its value */
   int               err;
};

static void ryw_flush(ryaml_writer_t *w)
{
   if (w->io && w->len)
   {
      if (!w->io(w->buf, w->len, w->user))
         w->err = 1;
      w->len = 0;
   }
}

static void ryw_put(ryaml_writer_t *w, const char *s, size_t n)
{
   if (w->err)
      return;
   if (w->len + n + 1 > w->cap)
   {
      if (w->io)
      {
         ryw_flush(w);
         if (n + 1 > w->cap)
         {
            /* larger than the stage: straight through */
            if (!w->io(s, n, w->user))
               w->err = 1;
            return;
         }
      }
      else
      {
         size_t ncap = w->cap * 2;
         char  *tmp;
         while (ncap < w->len + n + 1)
            ncap *= 2;
         if (!(tmp = (char*)realloc(w->buf, ncap)))
         {
            w->err = 1;
            return;
         }
         w->buf = tmp;
         w->cap = ncap;
      }
   }
   memcpy(w->buf + w->len, s, n);
   w->len += n;
}

static void ryw_spaces(ryaml_writer_t *w, unsigned n)
{
   static const char sp[] = "                                ";
   while (n)
   {
      unsigned k = n < 32 ? n : 32;
      ryw_put(w, sp, k);
      n -= k;
   }
}

/* ---------------------------------------------------------------- */
/* scalars                                                          */
/* ---------------------------------------------------------------- */

/* Whether the reader would take @s back as it is, unquoted */
static int ryw_plain_ok(const char *s, size_t len, int in_flow, int is_key)
{
   size_t i;
   if (!len)
      return 0;
   /* "-1" is a scalar; "- " or "-" alone would be a sequence item */
   if (s[0] == '-' && len > 1 && s[1] != ' ' && s[1] != '\t')
      ;
   else switch (s[0])
   {
      case '-': case '?': case ':': case ',': case '[': case ']':
      case '{': case '}': case '#': case '&': case '*': case '!':
      case '|': case '>': case '\'': case '"': case '%': case '@':
      case '`': case ' ': case '\t':
         return 0;
      default:
         break;
   }
   if (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == ':')
      return 0;
   for (i = 0; i < len; i++)
   {
      unsigned char c = (unsigned char)s[i];
      if (c < 0x20 || c == 0x7f)
         return 0;
      if (c == ':' && i + 1 < len && (s[i + 1] == ' ' || s[i + 1] == '\t'))
         return 0;
      if (c == '#' && i && (s[i - 1] == ' ' || s[i - 1] == '\t'))
         return 0;
      if ((in_flow || is_key) && (c == ',' || c == '[' || c == ']'
               || c == '{' || c == '}'))
         return 0;
      if (in_flow && c == ':')
         return 0;
   }
   return 1;
}

static void ryw_text(ryaml_writer_t *w, const char *s, size_t len,
      int in_flow, int is_key)
{
   const char *run;
   size_t      i;

   if (ryw_plain_ok(s, len, in_flow, is_key))
   {
      ryw_put(w, s, len);
      return;
   }

   /* double-quoted, escaping only what the reader unescapes */
   ryw_put(w, "\"", 1);
   run = s;
   for (i = 0; i < len; i++)
   {
      static const char hex[] = "0123456789abcdef";
      unsigned char c   = (unsigned char)s[i];
      const char   *esc = NULL;
      char          xb[4];
      size_t        elen = 2;
      switch (c)
      {
         case '"':  esc = "\\\""; break;
         case '\\': esc = "\\\\"; break;
         case '\n': esc = "\\n";  break;
         case '\r': esc = "\\r";  break;
         case '\t': esc = "\\t";  break;
         case '\0': esc = "\\0";  break;
         default:
            if (c < 0x20 || c == 0x7f)
            {
               /* any other control byte as \xNN */
               xb[0] = '\\';
               xb[1] = 'x';
               xb[2] = hex[c >> 4];
               xb[3] = hex[c & 15];
               esc   = xb;
               elen  = 4;
            }
            break;
      }
      if (esc)
      {
         ryw_put(w, run, (size_t)(s + i - run));
         ryw_put(w, esc, elen);
         run = s + i + 1;
      }
   }
   ryw_put(w, run, (size_t)(s + len - run));
   ryw_put(w, "\"", 1);
}

/* ---------------------------------------------------------------- */
/* layout                                                           */
/* ---------------------------------------------------------------- */

/* Where the next entry of @f starts: its first entry after a key goes on
 * a new line, after a dash on the same one (a sequence's first item on
 * a new line too, as "- - a" does not read back), and every later entry
 * at the start of its own line. */
static void ryw_entry(ryaml_writer_t *w, struct ryw_frame *f)
{
   if (!f->count && (f->open == RYW_AFTER_KEY
            || (f->open == RYW_AFTER_DASH && f->kind == RYW_SEQ)))
      ryw_put(w, "\n", 1);
   else if (!f->count && f->open != RYW_ROOT)
      return;
   ryw_spaces(w, f->indent);
}

static struct ryw_frame *ryw_top(ryaml_writer_t *w)
{
   return w->sp >= 0 ? &w->stk[w->sp] : NULL;
}

/* Where a value goes in the current collection, before it is written:
 * @what is 0 for a scalar or flow sequence, 1 for a block mapping, 2 for
 * a block sequence (whose first item goes on the line below the dash).
 * Returns 0 when no value is due there. */
static int ryw_value_slot(ryaml_writer_t *w, int what)
{
   struct ryw_frame *f = ryw_top(w);
   if (w->err)
      return 0;
   if (!f)
   {
      if (w->done)
         return 0;
      w->done = 1;
      return 1;
   }
   switch (f->kind)
   {
      case RYW_MAP:
         if (!w->after_key)
            return 0;
         w->after_key = 0;
         if (!what)
            ryw_put(w, " ", 1);
         return 1;
      case RYW_SEQ:
         ryw_entry(w, f);
         ryw_put(w, "- ", what == 2 ? 1 : 2);
         f->count++;
         return 1;
      default:
         if (f->count)
            ryw_put(w, ", ", 2);
         f->count++;
         return 1;
   }
}

static int ryw_push(ryaml_writer_t *w, int kind, int open, unsigned indent)
{
   if (w->sp + 1 >= w->stk_cap)
   {
      int               ncap = w->stk_cap * 2;
      struct ryw_frame *tmp  = (struct ryw_frame*)realloc(w->stk,
            (size_t)ncap * sizeof(*tmp));
      if (!tmp)
      {
         w->err = 1;
         return 0;
      }
      w->stk     = tmp;
      w->stk_cap = ncap;
   }
   w->sp++;
   w->stk[w->sp].indent = indent;
   w->stk[w->sp].count  = 0;
   w->stk[w->sp].kind   = (unsigned char)kind;
   w->stk[w->sp].open   = (unsigned char)open;
   return 1;
}

static void ryw_begin_block(ryaml_writer_t *w, int kind)
{
   struct ryw_frame *f = ryw_top(w);
   int               open;
   unsigned          indent;

   if (f && f->kind == RYW_FLOW)
   {
      w->err = 1;   /* no block collection inside a flow one */
      return;
   }
   if (!ryw_value_slot(w, kind == RYW_SEQ ? 2 : 1))
   {
      w->err = 1;
      return;
   }
   if (!f)
   {
      open   = RYW_ROOT;
      indent = 0;
   }
   else
   {
      open   = (f->kind == RYW_MAP) ? RYW_AFTER_KEY : RYW_AFTER_DASH;
      indent = f->indent + 2;
   }
   ryw_push(w, kind, open, indent);
}

static void ryw_end(ryaml_writer_t *w, int seq)
{
   struct ryw_frame *f = ryw_top(w);
   struct ryw_frame *parent;

   if (w->err)
      return;
   if (!f || w->after_key || (seq ? f->kind == RYW_MAP : f->kind != RYW_MAP))
   {
      w->err = 1;
      return;
   }
   if (f->kind == RYW_FLOW)
      ryw_put(w, "]", 1);
   else if (!f->count)
   {
      /* after "key:", or after the "-" a block sequence is opened with */
      if (f->open == RYW_AFTER_KEY
            || (f->open == RYW_AFTER_DASH && f->kind == RYW_SEQ))
         ryw_put(w, " ", 1);
      ryw_put(w, seq ? "[]" : "{}", 2);
   }
   w->sp--;
   parent = ryw_top(w);
   /* a block collection's entries end their own lines; a flow one or an
    * empty one ends its line here, unless it sits in a flow sequence */
   if ((f->kind == RYW_FLOW || !f->count) && (!parent || parent->kind != RYW_FLOW))
      ryw_put(w, "\n", 1);
}

/* ---------------------------------------------------------------- */
/* public                                                           */
/* ---------------------------------------------------------------- */

ryaml_writer_t *ryaml_writer_new(ryaml_writer_io_t io, void *user_data)
{
   ryaml_writer_t *w = (ryaml_writer_t*)calloc(1, sizeof(*w));
   if (!w)
      return NULL;
   w->io      = io;
   w->user    = user_data;
   w->cap     = RYW_STAGE;
   w->buf     = (char*)malloc(w->cap);
   w->stk_cap = RYW_DEPTH_MIN;
   w->stk     = (struct ryw_frame*)malloc((size_t)w->stk_cap * sizeof(*w->stk));
   w->sp      = -1;
   if (!w->buf || !w->stk)
   {
      free(w->buf);
      free(w->stk);
      free(w);
      return NULL;
   }
   return w;
}

int ryaml_writer_free(ryaml_writer_t *w)
{
   int ok;
   if (!w)
      return 0;
   ryw_flush(w);
   ok = !w->err && w->sp < 0 && !w->after_key;
   free(w->buf);
   free(w->stk);
   free(w);
   return ok;
}

const char *ryaml_writer_memory(ryaml_writer_t *w, size_t *len)
{
   if (!w || w->io || w->err)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   w->buf[w->len] = '\0';
   if (len)
      *len = w->len;
   return w->buf;
}

int ryaml_writer_error(const ryaml_writer_t *w)
{
   return !w || w->err;
}

void ryaml_writer_begin_map(ryaml_writer_t *w)
{
   if (w)
      ryw_begin_block(w, RYW_MAP);
}

void ryaml_writer_end_map(ryaml_writer_t *w)
{
   if (w)
      ryw_end(w, 0);
}

void ryaml_writer_begin_seq(ryaml_writer_t *w, int flow)
{
   struct ryw_frame *f;
   if (!w)
      return;
   if (!flow)
   {
      ryw_begin_block(w, RYW_SEQ);
      return;
   }
   f = ryw_top(w);
   if (!ryw_value_slot(w, 0))
   {
      w->err = 1;
      return;
   }
   ryw_put(w, "[", 1);
   ryw_push(w, RYW_FLOW, RYW_AFTER_DASH, f ? f->indent : 0);
}

void ryaml_writer_end_seq(ryaml_writer_t *w)
{
   if (w)
      ryw_end(w, 1);
}

void ryaml_writer_key(ryaml_writer_t *w, const char *key, size_t len)
{
   struct ryw_frame *f;
   if (!w || w->err)
      return;
   f = ryw_top(w);
   if (!f || f->kind != RYW_MAP || w->after_key || !key)
   {
      w->err = 1;
      return;
   }
   ryw_entry(w, f);
   ryw_text(w, key, len, 0, 1);
   ryw_put(w, ":", 1);
   f->count++;
   w->after_key = 1;
}

void ryaml_writer_scalar(ryaml_writer_t *w, const char *val, size_t len)
{
   struct ryw_frame *f;
   if (!w)
      return;
   f = ryw_top(w);
   if (!ryw_value_slot(w, 0))
   {
      w->err = 1;
      return;
   }
   ryw_text(w, val ? val : "", val ? len : 0, f && f->kind == RYW_FLOW, 0);
   if (!f || f->kind != RYW_FLOW)
      ryw_put(w, "\n", 1);
}

void ryaml_writer_blank_line(ryaml_writer_t *w)
{
   struct ryw_frame *f;
   if (!w || w->err)
      return;
   f = ryw_top(w);
   if (w->after_key || (f && f->kind == RYW_FLOW))
   {
      w->err = 1;
      return;
   }
   /* the next entry starts a line of its own, so this one can be empty;
    * a collection's first entry after a key starts with its line break,
    * so it gets its own here */
   if (f && !f->count && f->open == RYW_AFTER_KEY)
   {
      w->err = 1;
      return;
   }
   ryw_put(w, "\n", 1);
}
