/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml.c).
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
#include <stdint.h>

#include <retro_inline.h>
#include <streams/file_stream.h>

#include <formats/rxml.h>

/* Self-contained XML parser.
 *
 * This replaces the previously vendored yxml push parser (deps/yxml)
 * with a direct span scanner over the input string, written from
 * scratch.  The accepted document set and the resulting tree match the
 * yxml-backed implementation byte for byte on every input for which the
 * old code was well defined.  Two old failure modes were undefined
 * behavior and are now well defined instead:
 *   - accumulated text or attribute-value runs longer than 4095 bytes
 *     overflowed a fixed heap buffer; the accumulator now grows,
 *   - element nesting deeper than 32 overflowed a fixed node stack;
 *     the stack now grows.
 * Compatibility details preserved exactly, in particular:
 *   - "\r\n" and "\r" normalize to "\n" (to a single space inside
 *     attribute values, where "\t" and "\n" also become spaces, but
 *     character-reference output is never normalized),
 *   - the total open-name budget of 4095 bytes (elements, plus the
 *     active attribute or processing-instruction name): exceeding it
 *     fails the parse,
 *   - truncated input is not an error: the tree built so far is
 *     returned, completed constructs only,
 *   - an XML declaration is recognized only as the very first
 *     construct, PIs named "xml" (case-insensitive) are rejected, and
 *     "?>" inside a PI body only terminates it when the "?" is not
 *     itself the second byte of a consumed "?" pair,
 *   - element content ("data") is stored only for elements with no
 *     element children, and empty text or attribute values stay NULL,
 *   - the character-reference validity rule matches the old parser bit
 *     for bit, including its idiosyncratic rejection band
 *     U+DFFF..U+E7FD (while accepting U+D800..U+DFFE).
 */

struct rxml_document
{
   struct rxml_node *root_node;
};

/* Keep rarely taken scanners (declarations, comments, PIs, references,
 * cleanup) out of the hot parse loop: smaller code, better icache.
 * Under -Os the compiler already optimizes every function for size and
 * the forced outlining only adds call overhead, so it is disabled. */
#if defined(__OPTIMIZE_SIZE__)
#define RXML_COLD
#define RXML_NOINLINE
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define RXML_COLD     __attribute__((cold, noinline))
#define RXML_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define RXML_COLD     __declspec(noinline)
#define RXML_NOINLINE __declspec(noinline)
#else
#define RXML_COLD
#define RXML_NOINLINE
#endif

#define RXML_NAME_BUDGET 4094 /* open-name byte budget (was yxml's 4096-byte stack) */

/* Byte classes.  bit0: name char, bit1: name start char, bit2: whitespace,
 * bit3: stops a content run, bit4: stops an attribute-value run. */
#define RXML_CN  1
#define RXML_CNS 2
#define RXML_CSP 4
#define RXML_CCS 8
#define RXML_CAS 16

static const unsigned char rxml_cls[256] = {
   /* 00 */ 8|16, 0, 0, 0, 0, 0, 0, 0, 0, 4|16, 4|16, 0, 0, 4|8|16, 0, 0,
   /* 10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   /* 20 */ 4, 0, 16, 0, 0, 0, 8|16, 16, 0, 0, 0, 0, 0, 1, 1, 0,
   /* 30 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1|2, 0, 8|16, 0, 0, 0,
   /* 40 */ 0, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* 50 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 0, 0, 0, 0, 1|2,
   /* 60 */ 0, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* 70 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 0, 0, 0, 0, 0,
   /* 80 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* 90 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* a0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* b0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* c0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* d0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* e0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2,
   /* f0 */ 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2, 1|2
};

#define rxml_is_name(c)      (rxml_cls[c] & RXML_CN)
#define rxml_is_namestart(c) (rxml_cls[c] & RXML_CNS)
#define rxml_is_sp(c)        (rxml_cls[c] & RXML_CSP)

struct rxml_frame
{
   rxml_node_t *node;
   const unsigned char *name;
   size_t len;
};

struct rxml_parser
{
   const unsigned char *p;   /* cursor into NUL-terminated input */
   char *acc;                /* text / attribute value accumulator */
   size_t acc_len;
   size_t acc_cap;
   const unsigned char *val_direct; /* clean attr value taken straight
                                       from the input (no accumulation) */
   size_t val_direct_len;
   struct rxml_frame *frames; /* per-depth scratch (dynamic):
                                 .node is the pushed parent (indices
                                 below stack_i), .name/.len the open
                                 element names (indices below level) */
   size_t frames_cap;
   size_t stack_i;
   size_t level;
   size_t names_len;         /* open-name byte budget accounting */
   rxml_node_t *node;
   struct rxml_attrib_node *attr;
   rxml_document_t *doc;
};

/* Accumulator ------------------------------------------------------- */

RXML_COLD static int rxml_acc_reserve(struct rxml_parser *ps, size_t extra)
{
   size_t need = ps->acc_len + extra + 1;
   size_t cap;
   char *na;
   if (need <= ps->acc_cap)
      return 1;
   cap = ps->acc_cap;
   while (cap < need)
      cap <<= 1;
   na = (char*)realloc(ps->acc, cap);
   if (!na)
      return 0;
   ps->acc     = na;
   ps->acc_cap = cap;
   return 1;
}

static INLINE int rxml_acc_ch(struct rxml_parser *ps, unsigned char c)
{
   if (ps->acc_len + 2 > ps->acc_cap && !rxml_acc_reserve(ps, 1))
      return 0;
   ps->acc[ps->acc_len++] = (char)c;
   return 1;
}

static int rxml_acc_span(struct rxml_parser *ps, const unsigned char *s, size_t len)
{
   if (ps->acc_len + len + 1 > ps->acc_cap && !rxml_acc_reserve(ps, len))
      return 0;
   memcpy(ps->acc + ps->acc_len, s, len);
   ps->acc_len += len;
   return 1;
}

/* strdup() of a counted span */
static char *rxml_span_dup(const unsigned char *s, size_t len)
{
   char *r = (char*)malloc(len + 1);
   if (r)
   {
      memcpy(r, s, len);
      r[len] = '\0';
   }
   return r;
}

/* Scan a name at *pp: first byte must be a name-start (caller checked),
 * following bytes name chars.  Returns length, cursor left on the
 * terminating byte.  *fail is set if the name overflows the shared
 * open-name budget (matching the old parser's stack accounting, which
 * charged length + 1 bytes per name and errored once fewer than two
 * bytes remained). */
static size_t rxml_scan_name(struct rxml_parser *ps, const unsigned char **pp, int *fail)
{
   const unsigned char *s = *pp;
   const unsigned char *q = s + 1;
   size_t len;
   while (rxml_is_name(*q))
      q++;
   len   = (size_t)(q - s);
   *fail = (ps->names_len + len > RXML_NAME_BUDGET);
   *pp   = q;
   return len;
}

/* Character / entity references ------------------------------------- */

/* Decode the reference whose body (between '&' and ';') is the
 * ref_len-byte string at ref, and append its UTF-8 form to the
 * accumulator.  Returns 0 on invalid reference or OOM.  The validity
 * predicate replicates the old parser exactly. */
RXML_COLD static int rxml_ref_emit(struct rxml_parser *ps, const unsigned char *ref, size_t ref_len)
{
   unsigned ch = 0;
   if (ref_len == 0 || ref_len > 7)
      return 0;
   if (ref[0] == '#')
   {
      /* Any invalid or missing digit leaves ch at 0, which the
       * validity check below rejects. */
      size_t i = 1;
      if (ref_len > 1 && ref[1] == 'x')
         for (i = 2; i < ref_len; i++)
         {
            unsigned d = (unsigned)ref[i] - '0';
            if (d > 9)
            {
               d = ((unsigned)ref[i] | 32) - 'a';
               if (d > 5)
               {
                  ch = 0;
                  break;
               }
               d += 10;
            }
            ch = (ch << 4) + d;
         }
      else
         for (; i < ref_len; i++)
         {
            unsigned d = (unsigned)ref[i] - '0';
            if (d > 9)
            {
               ch = 0;
               break;
            }
            ch = ch * 10 + d;
         }
   }
   else
   {
      static const struct
      {
         char name[5];
         char value;
      } named[5] = {
         { "lt",   '<'  },
         { "gt",   '>'  },
         { "amp",  '&'  },
         { "apos", '\'' },
         { "quot", '"'  }
      };
      size_t i;
      /* The longest name is 4 bytes; the bound also keeps the
       * name[ref_len] terminator probe inside the array. */
      if (ref_len <= 4)
         for (i = 0; i < 5; i++)
         {
            if (!named[i].name[ref_len] &&
                  !memcmp(named[i].name, ref, ref_len))
            {
               ch = (unsigned char)named[i].value;
               break;
            }
         }
   }

   /* Old parser's character validity rule, kept verbatim (including
    * the U+DFFF..U+E7FD rejection band). */
   if (!ch || ch > 0x10FFFF || ch == 0xFFFE || ch == 0xFFFF ||
         (ch - 0xDFFF) < 0x7FF)
      return 0;

   if (ch <= 0x7F)
      return rxml_acc_ch(ps, (unsigned char)ch);
   if (!rxml_acc_reserve(ps, 4))
      return 0;
   if (ch <= 0x7FF)
   {
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0xC0 |  (ch >> 6));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 |  (ch & 0x3F));
   }
   else if (ch <= 0xFFFF)
   {
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0xE0 |  (ch >> 12));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 | ((ch >> 6) & 0x3F));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 |  (ch & 0x3F));
   }
   else
   {
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0xF0 |  (ch >> 18));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 | ((ch >> 12) & 0x3F));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 | ((ch >> 6) & 0x3F));
      ps->acc[ps->acc_len++] = (char)(unsigned char)(0x80 |  (ch & 0x3F));
   }
   return 1;
}

/* Scan "&...;" with the cursor on '&'.  Body chars limited to
 * [0-9A-Za-z#], at most 7 of them.  On success the cursor is on the
 * byte after ';'.  Returns: 1 ok, 0 error, -1 end of input (partial). */
RXML_COLD static int rxml_scan_ref(struct rxml_parser *ps)
{
   const unsigned char *s = ps->p + 1;
   const unsigned char *q = s;
   for (;;)
   {
      unsigned char c = *q;
      if (c == ';')
         break;
      if (!c)
      {
         ps->p = q;
         return -1;
      }
      if (!((unsigned)((c | 32) - 'a') < 26 ||
            (unsigned)(c - '0') < 10 || c == '#'))
         return 0;                       /* not [0-9A-Za-z#] */
      if ((size_t)(q - s) >= 7)
         return 0;                       /* too long */
      q++;
   }
   if (!rxml_ref_emit(ps, s, (size_t)(q - s)))
      return 0;
   ps->p = q + 1;
   return 1;
}

/* Tree building ----------------------------------------------------- */

/* Element start: link a node exactly as the old event loop did.  The
 * node and its name live in one allocation (the name directly after
 * the struct); rxml_free_node() frees the node only. */
static int rxml_on_elemstart(struct rxml_parser *ps,
      const unsigned char *name, size_t name_len)
{
   rxml_node_t *n;
   if (ps->level == ps->frames_cap)
   {
      size_t ncap = ps->frames_cap << 1;
      struct rxml_frame *nf = (struct rxml_frame*)
         realloc(ps->frames, ncap * sizeof(*nf));
      if (!nf)
         return 0;
      ps->frames     = nf;
      ps->frames_cap = ncap;
   }
   n = (rxml_node_t*)malloc(sizeof(*n) + name_len + 1);
   if (!n)
      return 0;
   n->name     = (char*)(n + 1);
   memcpy(n->name, name, name_len);
   n->name[name_len] = '\0';
   n->data     = NULL;
   n->attrib   = NULL;
   n->children = NULL;
   n->next     = NULL;
   if (ps->node)
   {
      if (ps->level > ps->stack_i)
      {
         /* First child of the current node: descend. */
         ps->frames[ps->stack_i++].node = ps->node;
         ps->node->children             = n;
      }
      else /* Sibling of the current node. */
         ps->node->next                 = n;
   }
   else
      ps->doc->root_node           = n;
   ps->node    = n;
   ps->attr    = NULL;
   ps->acc_len = 0;
   ps->frames[ps->level].name = name;
   ps->frames[ps->level].len  = name_len;
   ps->level++;
   return 1;
}

/* Element end: store accumulated text on childless elements only. */
static int rxml_on_elemend(struct rxml_parser *ps)
{
   ps->level--;
   if (ps->acc_len)
   {
      if (ps->level == ps->stack_i)
      {
         if (ps->node->data)
            free(ps->node->data);
         ps->node->data = rxml_span_dup((const unsigned char*)ps->acc, ps->acc_len);
         if (!ps->node->data)
            return 0;
      }
      ps->acc_len = 0;
   }
   if (ps->level < ps->stack_i)
      ps->node = ps->frames[--ps->stack_i].node;
   return 1;
}

static int rxml_on_attrstart(struct rxml_parser *ps,
      const unsigned char *name, size_t name_len)
{
   struct rxml_attrib_node *a = (struct rxml_attrib_node*)
      malloc(sizeof(*a) + name_len + 1);
   if (!a)
      return 0;
   a->attrib   = (char*)(a + 1);
   memcpy(a->attrib, name, name_len);
   a->attrib[name_len] = '\0';
   a->value    = NULL;
   a->next     = NULL;
   if (ps->attr)
      ps->attr->next   = a;
   else
      ps->node->attrib = a;
   ps->attr    = a;
   ps->acc_len = 0;
   return 1;
}

static int rxml_on_attrend(struct rxml_parser *ps)
{
   if (ps->val_direct)
   {
      if (ps->val_direct_len)
      {
         ps->attr->value = rxml_span_dup(ps->val_direct, ps->val_direct_len);
         if (!ps->attr->value)
            return 0;
      }
      ps->val_direct = NULL;
      return 1;
   }
   if (ps->acc_len)
   {
      ps->attr->value = rxml_span_dup((const unsigned char*)ps->acc, ps->acc_len);
      if (!ps->attr->value)
         return 0;
      ps->acc_len = 0;
   }
   return 1;
}

/* Shared sub-scanners.  All return 1 ok / 0 error / -1 end of input. */

/* Skip whitespace ("\r\n" and "\r" count via normalization). */
static INLINE void rxml_skip_sp(struct rxml_parser *ps)
{
   while (rxml_is_sp(*ps->p) || *ps->p == 0x0d)
      ps->p++;
}

/* Match a literal (used for "CDATA[", "OCTYPE", xmldecl keywords...). */
RXML_COLD static int rxml_match(struct rxml_parser *ps, const char *lit)
{
   const unsigned char *q = ps->p;
   while (*lit)
   {
      if (!*q)
      {
         ps->p = q;
         return -1;
      }
      if (*q != (unsigned char)*lit)
         return 0;
      q++;
      lit++;
   }
   ps->p = q;
   return 1;
}

/* "<!--" seen up to "<!-"; cursor on the byte after that.  Body runs to
 * "--", which must be followed by '>' ("--x" is an error). */
RXML_COLD static int rxml_scan_comment(struct rxml_parser *ps)
{
   const unsigned char *q = ps->p;
   if (!*q)      { ps->p = q; return -1; }
   if (*q != '-')
      return 0;
   q++;
   for (;;)
   {
      const unsigned char *d = (const unsigned char*)strchr((const char*)q, '-');
      if (!d)
      {
         ps->p = q + strlen((const char*)q);
         return -1;
      }
      if (d[1] == '-')
      {
         if (d[2] == '>')
         {
            ps->p = d + 3;
            return 1;
         }
         if (!d[2])
         {
            ps->p = d + 2;
            return -1;
         }
         return 0;                       /* "--" not followed by '>' */
      }
      q = d + 1;
   }
}

/* Processing instruction, cursor after "<?".  Handles the name (which
 * must not be "xml" in any case mix) and the body with its pairwise
 * "?"-consumption quirk: a "?" whose successor is not ">" consumes that
 * successor too, so only a "?" reached directly from the body state can
 * terminate the PI. */
RXML_COLD static int rxml_scan_pi(struct rxml_parser *ps)
{
   const unsigned char *q = ps->p;
   const unsigned char *name;
   size_t name_len;
   int fail;
   if (!*q)      { ps->p = q; return -1; }
   if (!rxml_is_namestart(*q))
      return 0;
   name     = q;
   ps->p    = q;
   name_len = rxml_scan_name(ps, &ps->p, &fail);
   if (fail)
      return 0;
   q = ps->p;
   if (!*q)      { ps->p = q; return -1; }
   if (*q != '?' && !(rxml_is_sp(*q) || *q == 0x0d))
      return 0;
   if (name_len == 3 &&
         (name[0] | 32) == 'x' && (name[1] | 32) == 'm' && (name[2] | 32) == 'l')
      return 0;                          /* PI named "xml" is invalid */
   if (*q == '?')
   {
      q++;
      if (!*q)   { ps->p = q; return -1; }
      if (*q != '>')
         return 0;
      ps->p = q + 1;
      return 1;
   }
   /* Body: find the terminating "?>" honoring pair consumption. */
   for (;;)
   {
      const unsigned char *d = (const unsigned char*)strchr((const char*)q, '?');
      if (!d)
      {
         ps->p = q + strlen((const char*)q);
         return -1;
      }
      if (d[1] == '>')
      {
         ps->p = d + 2;
         return 1;
      }
      if (!d[1])
      {
         ps->p = d + 1;
         return -1;
      }
      q = d + 2;                         /* '?' pairs with its successor */
   }
}

/* DOCTYPE internal machinery, cursor after "<!DOCTYPE".  Quotes hide
 * everything to the matching quote; "<" must open "<?pi?>", "<!--..-->"
 * or "<!chatter>"; ">" at bracket depth zero ends the declaration. */
RXML_COLD static int rxml_scan_doctype(struct rxml_parser *ps)
{
   const unsigned char *q = ps->p;
   for (;;)
   {
      unsigned char c = *q;
      if (!c)         { ps->p = q; return -1; }
      if (c == '>')   { ps->p = q + 1; return 1; }
      if (c == '\'' || c == '"')
      {
         const unsigned char *d = (const unsigned char*)strchr((const char*)q + 1, c);
         if (!d)
         {
            ps->p = q + 1 + strlen((const char*)q + 1);
            return -1;
         }
         q = d + 1;
         continue;
      }
      if (c == '<') /* nested construct */
      {
         int r;
         q++;
         c = *q;
         if (!c)      { ps->p = q; return -1; }
         if (c == '?')
         {
            ps->p = q + 1;
            r     = rxml_scan_pi(ps);
            if (r != 1)
               return r;
            q = ps->p;
            continue;
         }
         if (c != '!')
            return 0;
         q++;
         c = *q;
         if (!c)      { ps->p = q; return -1; }
         if (c == '-')
         {
            ps->p = q + 1;
            r     = rxml_scan_comment(ps);
            if (r != 1)
               return r;
            q = ps->p;
            continue;
         }
         /* "<!chatter ...>": the first byte is consumed blindly
          * (so a quote or '>' here is plain chatter); afterwards
          * quoted strings hide their contents and '>' terminates. */
         q++;
         for (;;)
         {
            c = *q;
            if (!c)        { ps->p = q; return -1; }
            if (c == '>')  { q++; break; }
            if (c == '\'' || c == '"')
            {
               const unsigned char *d =
                  (const unsigned char*)strchr((const char*)q + 1, c);
               if (!d)
               {
                  ps->p = q + 1 + strlen((const char*)q + 1);
                  return -1;
               }
               q = d + 1;
               continue;
            }
            q++;
         }
         continue;
      }
      q++;
   }
}


/* Keyword tail, then SP* '=' SP* quote: shared by the XML declaration
 * pseudo-attributes.  Returns the quote char, 0 on error, or
 * (unsigned)-1 at end of input. */
RXML_COLD static unsigned rxml_xmldecl_eq(struct rxml_parser *ps, const char *lit)
{
   unsigned char quote;
   int r = rxml_match(ps, lit);
   if (r != 1)
      return (unsigned)r;
   rxml_skip_sp(ps);
   if (*ps->p != '=')
      return *ps->p ? 0 : (unsigned)-1;
   ps->p++;
   rxml_skip_sp(ps);
   quote = *ps->p;
   if (quote != '\'' && quote != '"')
      return quote ? 0 : (unsigned)-1;
   ps->p++;
   return quote;
}

/* Close quote of a pseudo-attribute, then SP or '?' must follow. */
RXML_COLD static int rxml_xmldecl_close(struct rxml_parser *ps, unsigned char quote)
{
   if (!*ps->p)   return -1;
   if (*ps->p != quote)
      return 0;
   ps->p++;
   if (!*ps->p)   return -1;
   if (*ps->p != '?' && !(rxml_is_sp(*ps->p) || *ps->p == 0x0d))
      return 0;
   rxml_skip_sp(ps);
   return 1;
}

/* XML declaration, cursor after "<?xml" + one whitespace byte.
 * version="1.N+" required; then optional encoding, then optional
 * standalone=yes|no; ends at "?>". */
RXML_COLD static int rxml_scan_xmldecl(struct rxml_parser *ps)
{
   unsigned q;
   int r;

   rxml_skip_sp(ps);
   if (*ps->p != 'v')
      return *ps->p ? 0 : -1;
   ps->p++;
   q = rxml_xmldecl_eq(ps, "ersion");
   if (q + 1 <= 1)
      return (int)q;
   if ((r = rxml_match(ps, "1.")) != 1)
      return r;
   if (!*ps->p)  return -1;
   if ((unsigned)(*ps->p - '0') > 9)
      return 0;
   do
   {
      ps->p++;
   } while ((unsigned)(*ps->p - '0') <= 9);
   if ((r = rxml_xmldecl_close(ps, (unsigned char)q)) != 1)
      return r;

   if (*ps->p == 'e')
   {
      ps->p++;
      q = rxml_xmldecl_eq(ps, "ncoding");
      if (q + 1 <= 1)
         return (int)q;
      if (!*ps->p)  return -1;
      if (!((unsigned)((*ps->p | 32) - 'a') < 26))
         return 0;
      for (;;)
      {
         unsigned char c = *++ps->p;
         if (!((unsigned)((c | 32) - 'a') < 26 || (unsigned)(c - '0') < 10 ||
               c == '.' || c == '_' || c == '-'))
            break;
      }
      if ((r = rxml_xmldecl_close(ps, (unsigned char)q)) != 1)
         return r;
   }
   if (*ps->p == 's')
   {
      ps->p++;
      q = rxml_xmldecl_eq(ps, "tandalone");
      if (q + 1 <= 1)
         return (int)q;
      if (*ps->p == 'y')
      {
         ps->p++;
         if ((r = rxml_match(ps, "es")) != 1)
            return r;
      }
      else if (*ps->p == 'n')
      {
         ps->p++;
         if ((r = rxml_match(ps, "o")) != 1)
            return r;
      }
      else if (!*ps->p)
         return -1;
      else
         return 0;
      if (!*ps->p)  return -1;
      if (*ps->p != (unsigned char)q)
         return 0;
      ps->p++;
      rxml_skip_sp(ps);
   }
   if (*ps->p != '?')

      return *ps->p ? 0 : -1;
   ps->p++;
   if (*ps->p != '>')

      return *ps->p ? 0 : -1;
   ps->p++;
   return 1;
}

/* CDATA body, cursor after "<![CDATA[".  Content is everything before
 * the first "]]>"; at end of input, up to two pending "]" bytes of an
 * unfinished terminator are dropped. */
/* CDATA body, cursor after "<![CDATA[".  Content is everything before
 * the first "]]>".  End of input inside the section needs no
 * accumulator flush: the enclosing element can never close, so its
 * pending text is unobservable in the returned partial tree (the old
 * parser withheld up to two pending "]" bytes here, equally
 * unobservable). */
RXML_COLD static int rxml_scan_cdata(struct rxml_parser *ps)
{
   const unsigned char *q = ps->p;
   const unsigned char *run = q;
   for (;;)
   {
      unsigned char c = *q;
      if (c == ']')
      {
         if (q[1] == ']' && q[2] == '>')
         {
            if (q > run && !rxml_acc_span(ps, run, (size_t)(q - run)))
               return 0;
            ps->p = q + 3;
            return 1;
         }
         if (q[1] && (q[1] != ']' || q[2]))
         {
            q++;
            continue;
         }
      }
      else if (c == 0x0d)
      {
         if (q > run && !rxml_acc_span(ps, run, (size_t)(q - run)))
            return 0;
         if (!rxml_acc_ch(ps, 0x0a))
            return 0;
         q++;
         if (*q == 0x0a)
            q++;
         run = q;
         continue;
      }
      else if (c)
      {
         q++;
         continue;
      }
      ps->p = q;
      return -1;
   }
}

/* Attribute value, cursor after the opening quote. */
static int rxml_scan_attrvalue(struct rxml_parser *ps, unsigned char quote)
{
   const unsigned char *q = ps->p;
   const unsigned char *run = q;
   /* Fast path: a value with no character to translate, reference to
    * expand or error to raise is used straight from the input. */
   while (!(rxml_cls[*q] & RXML_CAS))
      q++;
   if (*q == quote)
   {
      ps->val_direct     = run;
      ps->val_direct_len = (size_t)(q - run);
      ps->p              = q + 1;
      return 1;
   }
   for (;;)
   {
      unsigned char c = *q;
      if (!(rxml_cls[c] & RXML_CAS) && c != quote)
      {
         q++;
         continue;
      }
      if (q > run && !rxml_acc_span(ps, run, (size_t)(q - run)))
         return 0;
      if (c == quote)
      {
         ps->p = q + 1;
         return 1;
      }
      switch (c)
      {
         case 0:
            ps->p = q;
            return -1;
         case '<':
            return 0;
         case '&':
         {
            int r;
            ps->p = q;
            r     = rxml_scan_ref(ps);
            if (r != 1)
               return r;
            q = ps->p;
            break;
         }
         case 0x0d:
            q++;
            if (*q == 0x0a)
               q++;
            if (!rxml_acc_ch(ps, 0x20))
               return 0;
            break;
         default: /* 0x09, 0x0a -> space; '\'' or '"' != quote -> literal */
            if (!rxml_acc_ch(ps, (c == 0x09 || c == 0x0a) ? 0x20 : c))
               return 0;
            q++;
            break;
      }
      run = q;
   }
}


/* Misc items around the root element.  In the prolog (mode > 0):
 * comments, PIs, DOCTYPEs and, as the very first construct (mode == 2),
 * an XML declaration; stops with the cursor on a root-element name
 * start.  In the epilog (mode == 0): comments and PIs only.
 * Returns 1 with the cursor on the name start (prolog only), else
 * 0 error / -1 end of input. */
RXML_NOINLINE static int rxml_scan_misc(struct rxml_parser *ps, int mode)
{
   int r;
   for (;;)
   {
      unsigned char c;
      rxml_skip_sp(ps);
      c = *ps->p;
      if (c != '<')
         return c ? 0 : -1;
      ps->p++;
      c = *ps->p;
      if (!c)
         return -1;
      if (c == '!')
      {
         ps->p++;
         c = *ps->p;
         if (!c)
            return -1;
         if (c == '-')
         {
            ps->p++;
            if ((r = rxml_scan_comment(ps)) != 1)
               return r;
         }
         else if (mode && c == 'D')
         {
            ps->p++;
            if ((r = rxml_match(ps, "OCTYPE")) != 1)
               return r;
            if ((r = rxml_scan_doctype(ps)) != 1)
               return r;
         }
         else
            return 0;
      }
      else if (c == '?')
      {
         ps->p++;
         if (mode == 2 && ps->p[0] == 'x' && ps->p[1] == 'm' &&
               ps->p[2] == 'l' &&
               (rxml_is_sp(ps->p[3]) || ps->p[3] == 0x0d))
         {
            ps->p += 4;
            if (*(ps->p - 1) == 0x0d && *ps->p == 0x0a)
               ps->p++;
            if ((r = rxml_scan_xmldecl(ps)) != 1)
               return r;
         }
         else if ((r = rxml_scan_pi(ps)) != 1)
            return r;
      }
      else if (mode && rxml_is_namestart(c))
         return 1;
      else
         return 0;
      if (mode)
         mode = 1;
   }
}

/* The main parse ----------------------------------------------------- */

static int rxml_parse_document(struct rxml_parser *ps)
{
   int r;

   /* Optional UTF-8 byte order mark, only at the very first byte. */
   if (*ps->p == 0xEF)
   {
      int r2;
      ps->p++;
      if ((r2 = rxml_match(ps, "\xbb\xbf")) != 1)
         return r2;
   }

   {
      int r2 = rxml_scan_misc(ps, 2);
      if (r2 != 1)
         return r2;
   }

   /* Element tree. */
   for (;;)
   {
      /* Cursor on the first byte of an element name. */
      const unsigned char *name = ps->p;
      size_t name_len;
      int fail;
      unsigned char c;

      name_len = rxml_scan_name(ps, &ps->p, &fail);
      if (fail)
         return 0;
      c = *ps->p;
      if (!(rxml_is_sp(c) || c == 0x0d) && c != '/' && c != '>')
         return c ? 0 : -1;
      if (!rxml_on_elemstart(ps, name, name_len))
         return 0;
      ps->names_len += name_len + 1;

      /* Attributes. */
      for (;;)
      {
         rxml_skip_sp(ps);
         c = *ps->p;
         if (c == '>')
         {
            ps->p++;
            break;                       /* into content */
         }
         if (c == '/')
         {
            ps->p++;
            c = *ps->p;
            if (c != '>')
               return c ? 0 : -1;
            ps->p++;
            if (!rxml_on_elemend(ps))
               return 0;
            ps->names_len -= name_len + 1;
            goto element_closed;
         }
         if (!rxml_is_namestart(c))
            return c ? 0 : -1;
         {
            const unsigned char *aname = ps->p;
            size_t aname_len = rxml_scan_name(ps, &ps->p, &fail);
            unsigned char quote;
            if (fail)
               return 0;
            c = *ps->p;
            if (!(rxml_is_sp(c) || c == 0x0d) && c != '=')
               return c ? 0 : -1;
            if (!rxml_on_attrstart(ps, aname, aname_len))
               return 0;
            rxml_skip_sp(ps);
            c = *ps->p;
            if (c != '=')
               return c ? 0 : -1;
            ps->p++;
            rxml_skip_sp(ps);
            quote = *ps->p;
            if (quote != '\'' && quote != '"')
               return quote ? 0 : -1;
            ps->p++;
            /* The attribute name occupies name-budget space while its
             * value parses; it was already length-checked above and
             * nothing pushes during the value, so no live accounting
             * is needed beyond the check in rxml_scan_name(). */
            r = rxml_scan_attrvalue(ps, quote);
            if (r != 1)
               return r;
            if (!rxml_on_attrend(ps))
               return 0;
            /* After the closing quote: '/', '>' or whitespace. */
            c = *ps->p;
            if (!(rxml_is_sp(c) || c == 0x0d) && c != '/' && c != '>')
               return c ? 0 : -1;
         }
      }

      /* Content of the just-opened element. */
content_loop:
      for (;;)
      {
         const unsigned char *q = ps->p;
         const unsigned char *run = q;
         for (;;)
         {
            while (!(rxml_cls[*q] & RXML_CCS))
               q++;
            if (q > run && !rxml_acc_span(ps, run, (size_t)(q - run)))
               return 0;
            c = *q;
            if (c == '<')
               break;
            if (!c)
            {
               ps->p = q;
               return -1;
            }
            if (c == '&')
            {
               ps->p = q;
               r     = rxml_scan_ref(ps);
               if (r != 1)
                  return r;
               q = ps->p;
            }
            else /* 0x0d */
            {
               q++;
               if (*q == 0x0a)
                  q++;
               if (!rxml_acc_ch(ps, 0x0a))
                  return 0;
            }
            run = q;
         }
         ps->p = q + 1;
         c     = *ps->p;
         if (c == '/')
         {
            /* Closing tag: must match the innermost open element. */
            const unsigned char *open  = ps->frames[ps->level - 1].name;
            size_t open_len            = ps->frames[ps->level - 1].len;
            const unsigned char *tail;
            size_t k = 0;
            ps->p++;
            c = *ps->p;
            if (!rxml_is_namestart(c))
               return c ? 0 : -1;
            tail = ps->p;
            for (;;)
            {
               c = tail[k];
               if (k < open_len)
               {
                  if (!c)
                  {
                     ps->p = tail + k;
                     return -1;
                  }
                  if (c != open[k])
                     return 0;
                  k++;
                  continue;
               }
               /* Name fully matched: expect whitespace* '>'. */
               if (rxml_is_name(c))
                  return 0;
               ps->p = tail + k;
               break;
            }
            c = *ps->p;
            if (!c)
               return -1;
            if (c == '>')
               ps->p++;
            else if (rxml_is_sp(c) || c == 0x0d)
            {
               /* The end-of-element event fires at this whitespace
                * byte.  For the root element that also switches
                * straight to the epilog, so the pending '>' is never
                * accepted there ("</root >" fails, while "</root "
                * followed by nothing, comments or PIs succeeds); for
                * any other element, whitespace then '>' follow. */
               ps->p++;
               if (c == 0x0d && *ps->p == 0x0a)
                  ps->p++;
               if (ps->level > 1)
               {
                  rxml_skip_sp(ps);
                  c = *ps->p;
                  if (!c)
                  {
                     if (!rxml_on_elemend(ps))
                        return 0;
                     return -1;
                  }
                  if (c != '>')
                     return 0;
                  ps->p++;
               }
            }
            else
               return 0;
            if (!rxml_on_elemend(ps))
               return 0;
            ps->names_len -= open_len + 1;
element_closed:
            if (ps->level == 0)
               goto epilog;
            goto content_loop;
         }
         if (c == '!')
         {
            ps->p++;
            c = *ps->p;
            if (c == '-')
            {
               ps->p++;
               if ((r = rxml_scan_comment(ps)) != 1)
                  return r;
               continue;
            }
            if (c == '[')
            {
               ps->p++;
               if ((r = rxml_match(ps, "CDATA[")) != 1)
                  return r;
               if ((r = rxml_scan_cdata(ps)) != 1)
                  return r;
               continue;
            }
            return c ? 0 : -1;
         }
         if (c == '?')
         {
            ps->p++;
            if ((r = rxml_scan_pi(ps)) != 1)
               return r;
            continue;
         }
         if (rxml_is_namestart(c))
            break;                       /* child element */
         return c ? 0 : -1;
      }
   }

epilog:
   /* After the root element: whitespace, comments and PIs only. */
   return rxml_scan_misc(ps, 0);
}

/* Public API --------------------------------------------------------- */

struct rxml_node *rxml_root_node(rxml_document_t *doc)
{
   if (doc)
      return doc->root_node;
   return NULL;
}

RXML_COLD static void rxml_free_node(struct rxml_node *node)
{
   struct rxml_node *head = NULL;
   struct rxml_attrib_node *attrib_node_head = NULL;

   if (!node)
      return;

   for (head = node->children; head; )
   {
      struct rxml_node *next_node = (struct rxml_node*)head->next;
      rxml_free_node(head);
      head = next_node;
   }

   for (attrib_node_head = node->attrib; attrib_node_head; )
   {
      struct rxml_attrib_node *next_attrib =
            (struct rxml_attrib_node*)attrib_node_head->next;

      /* ->attrib lives inside the same allocation */
      if (attrib_node_head->value)
         free(attrib_node_head->value);
      free(attrib_node_head);

      attrib_node_head = next_attrib;
   }

   /* ->name lives inside the same allocation */
   if (node->data)
      free(node->data);
   free(node);
}

rxml_document_t *rxml_load_document_string(const char *str)
{
   struct rxml_parser ps;
   int r;

   memset(&ps, 0, sizeof(ps));
   ps.p          = (const unsigned char*)str;
   ps.acc_cap    = 4096;
   ps.acc        = (char*)malloc(ps.acc_cap);
   ps.frames_cap = 32;
   ps.frames     = (struct rxml_frame*)malloc(ps.frames_cap * sizeof(*ps.frames));
   ps.doc        = (rxml_document_t*)malloc(sizeof(*ps.doc));

   if (!ps.acc || !ps.frames || !ps.doc)
      r = 0;
   else
   {
      ps.doc->root_node = NULL;
      r = rxml_parse_document(&ps);
   }

   free(ps.acc);
   free(ps.frames);

   /* r > 0: fully parsed; r < 0: input ended mid-construct, which the
    * old implementation also accepted (it never signalled EOF to the
    * parser), returning the tree built so far; r == 0: parse error. */
   if (r == 0)
   {
      rxml_free_document(ps.doc);
      return NULL;
   }
   return ps.doc;
}

rxml_document_t *rxml_load_document(const char *path)
{
   rxml_document_t *doc    = NULL;
   char *memory_buffer     = NULL;
   int64_t len             = 0;
   RFILE *file             = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return NULL;

   len                     = filestream_get_size(file);
   /* filestream_get_size returns -1 on error.  Pre-patch this
    * flowed through (size_t)(len + 1) as malloc(0) on 64-bit
    * (returning a tiny non-NULL block) or as a wrapped value on
    * 32-bit; either way memory_buffer[len] = '\0' wrote far
    * out-of-bounds.  Reject negative sizes and any size that
    * would not fit in size_t on this platform. */
   if (len < 0 || (uint64_t)len >= (uint64_t)((size_t)-1))
      goto error;
   memory_buffer           = (char*)malloc((size_t)(len + 1));
   if (!memory_buffer)
      goto error;

   memory_buffer[len]      = '\0';
   if (filestream_read(file, memory_buffer, len) != len)
      goto error;

   filestream_close(file);
   file                    = NULL;

   doc                     = rxml_load_document_string(memory_buffer);

   free(memory_buffer);
   return doc;

error:
   free(memory_buffer);
   if (file)
      filestream_close(file);
   return NULL;
}

void rxml_free_document(rxml_document_t *doc)
{
   if (!doc)
      return;

   rxml_free_node(doc->root_node);

   free(doc);
}

const char *rxml_node_attrib(struct rxml_node *node, const char *attrib)
{
   struct rxml_attrib_node *attribs = NULL;
   for (attribs = node->attrib; attribs; attribs = attribs->next)
   {
      if (strcmp(attrib, attribs->attrib) == 0)
         return attribs->value;
   }

   return NULL;
}

/* Keep the single-translation-unit (griffin) namespace clean. */
#undef RXML_CN
#undef RXML_CNS
#undef RXML_CSP
#undef RXML_CCS
#undef RXML_CAS
#undef RXML_NAME_BUDGET
#undef RXML_COLD
#undef RXML_NOINLINE
#undef rxml_is_name
#undef rxml_is_namestart
#undef rxml_is_sp
