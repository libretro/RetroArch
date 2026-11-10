/* ryaml_writer_test.c - the YAML writer against its reader.
 *
 * A configuration and a patch file are written and compared byte for
 * byte with the layout they must have. Scalars the reader could misread
 * unquoted (indicators, ": ", " #", blanks at the ends, line breaks,
 * quotes, backslashes, commas and brackets in a flow sequence) are
 * written as keys, values, items and flow items and read back the same.
 * Then 3000 random documents - mappings, block and flow sequences, empty
 * ones, random bytes for keys and values - are written and read back
 * with ryaml_parse() to the same tree, through memory and through a
 * callback that takes the output in small pieces. Last, the calls that
 * do not make a document must be refused.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/ryaml.h>
#include <formats/ryaml_writer.h>

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("  FAIL: %s (line %d)\n", what, __LINE__); fails++; } } while (0)

#define S(w, str) ryaml_writer_scalar((w), (str), strlen(str))
#define K(w, str) ryaml_writer_key((w), (str), strlen(str))

static void layout(void)
{
   static const char want_cfg[] =
      "Core:\n"
      "  PPU Decoder: Recompiler (LLVM)\n"
      "  PPU Threads: 2\n"
      "  Libraries Control: []\n"
      "  Empty: {}\n"
      "Path: C:\\games\n"
      "List:\n"
      "  - one\n"
      "  - key: value\n"
      "    other: \"has: colon\"\n"
      "  -\n"
      "    - nested\n"
      "  - {}\n"
      "Negative: -1\n";
   static const char want_patch[] =
      "Version: 1.2\n"
      "\n"
      "PPU-0123:\n"
      "  60 FPS:\n"
      "    Games:\n"
      "      Some Game:\n"
      "        BLUS00000:\n"
      "          - 01.00\n"
      "    Patch:\n"
      "      - [be32, 0x10000, 0x60000000]\n"
      "      - [load, anchor, [a, \"b,c\"]]\n";
   ryaml_writer_t *w = ryaml_writer_new(NULL, NULL);
   const char     *out;
   size_t          len;

   printf("layout\n");
   ryaml_writer_begin_map(w);
   K(w, "Core");
   ryaml_writer_begin_map(w);
   K(w, "PPU Decoder"); S(w, "Recompiler (LLVM)");
   K(w, "PPU Threads"); S(w, "2");
   K(w, "Libraries Control"); ryaml_writer_begin_seq(w, 0); ryaml_writer_end_seq(w);
   K(w, "Empty"); ryaml_writer_begin_map(w); ryaml_writer_end_map(w);
   ryaml_writer_end_map(w);
   K(w, "Path"); S(w, "C:\\games");
   K(w, "List");
   ryaml_writer_begin_seq(w, 0);
   S(w, "one");
   ryaml_writer_begin_map(w);
   K(w, "key"); S(w, "value");
   K(w, "other"); S(w, "has: colon");
   ryaml_writer_end_map(w);
   ryaml_writer_begin_seq(w, 0); S(w, "nested"); ryaml_writer_end_seq(w);
   ryaml_writer_begin_map(w); ryaml_writer_end_map(w);
   ryaml_writer_end_seq(w);
   K(w, "Negative"); S(w, "-1");
   ryaml_writer_end_map(w);
   out = ryaml_writer_memory(w, &len);
   CHECK(out && len == sizeof(want_cfg) - 1 && !memcmp(out, want_cfg, len), "a configuration");
   if (out && strcmp(out, want_cfg))
      printf("----\n%s----\n", out);
   CHECK(ryaml_writer_free(w), "well formed");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_begin_map(w);
   K(w, "Version"); S(w, "1.2");
   ryaml_writer_blank_line(w);
   K(w, "PPU-0123");
   ryaml_writer_begin_map(w);
   K(w, "60 FPS");
   ryaml_writer_begin_map(w);
   K(w, "Games");
   ryaml_writer_begin_map(w);
   K(w, "Some Game");
   ryaml_writer_begin_map(w);
   K(w, "BLUS00000");
   ryaml_writer_begin_seq(w, 0); S(w, "01.00"); ryaml_writer_end_seq(w);
   ryaml_writer_end_map(w);
   ryaml_writer_end_map(w);
   K(w, "Patch");
   ryaml_writer_begin_seq(w, 0);
   ryaml_writer_begin_seq(w, 1); S(w, "be32"); S(w, "0x10000"); S(w, "0x60000000"); ryaml_writer_end_seq(w);
   ryaml_writer_begin_seq(w, 1); S(w, "load"); S(w, "anchor");
   ryaml_writer_begin_seq(w, 1); S(w, "a"); S(w, "b,c"); ryaml_writer_end_seq(w);
   ryaml_writer_end_seq(w);
   ryaml_writer_end_seq(w);
   ryaml_writer_end_map(w);
   ryaml_writer_end_map(w);
   ryaml_writer_end_map(w);
   out = ryaml_writer_memory(w, &len);
   CHECK(out && len == sizeof(want_patch) - 1 && !memcmp(out, want_patch, len), "a patch file, a blank line and flow sequences");
   if (out && strcmp(out, want_patch))
      printf("----\n%s----\n", out);
   CHECK(ryaml_writer_free(w), "well formed");
}

/* ---- the reader's tree against what was written ---- */

#define MAXN 4096

struct item
{
   char     key[24];
   char     val[24];
   size_t   klen, vlen;
   int      kind;      /* 0 scalar, 1 map, 2 block seq, 3 flow seq */
   int      first, next;
};

static struct item items[MAXN];
static int         nitems;
static unsigned    rnd_state;

static unsigned rnd(unsigned n)
{
   rnd_state = rnd_state * 1103515245u + 12345u;
   return (rnd_state >> 8) % n;
}

static const char tricky[] = "-?:,[]{}#&*!|>'\"%@` \t\\\n\r\x01\x1b\x7f";

static size_t rnd_text(char *out)
{
   size_t len = rnd(12), i;
   for (i = 0; i < len; i++)
   {
      unsigned r = rnd(10);
      if (r < 3)
         out[i] = tricky[rnd(sizeof(tricky) - 1)];
      else
         out[i] = (char)('a' + rnd(26));
   }
   return len;
}

/* @kind: what this node may be; flow sequences hold scalars and flow
 * sequences only */
static int build(int depth, int in_flow, int is_map_value)
{
   int me = nitems++;
   int r;
   struct item *it = &items[me];
   memset(it, 0, sizeof(*it));
   it->first = it->next = -1;
   r = (depth > 3 || nitems > MAXN - 64) ? 0 : (int)rnd(in_flow ? 3 : 5);
   (void)is_map_value;
   if (in_flow)
      it->kind = (r == 2) ? 3 : 0;
   else
      it->kind = (r >= 4) ? 3 : (r == 0 ? 0 : r);
   if (it->kind == 0)
      it->vlen = rnd_text(it->val);
   else
   {
      int n = (int)rnd(4), i, prev = -1;
      for (i = 0; i < n; i++)
      {
         int c = build(depth + 1, it->kind == 3, it->kind == 1);
         if (items[me].kind == 1)
         {
            items[c].klen = rnd_text(items[c].key);
            /* keys unique within the mapping, so lookups are exact */
            items[c].key[items[c].klen++] = (char)('A' + i);
         }
         if (prev < 0)
            items[me].first = c;
         else
            items[prev].next = c;
         prev = c;
      }
   }
   return me;
}

static void emit(ryaml_writer_t *w, int n)
{
   struct item *it = &items[n];
   int c;
   switch (it->kind)
   {
      case 0:
         ryaml_writer_scalar(w, it->val, it->vlen);
         break;
      case 1:
         ryaml_writer_begin_map(w);
         for (c = it->first; c >= 0; c = items[c].next)
         {
            ryaml_writer_key(w, items[c].key, items[c].klen);
            emit(w, c);
         }
         ryaml_writer_end_map(w);
         break;
      default:
         ryaml_writer_begin_seq(w, it->kind == 3);
         for (c = it->first; c >= 0; c = items[c].next)
            emit(w, c);
         ryaml_writer_end_seq(w);
         break;
   }
}

static int same(const ryaml_t *y, int node, int n)
{
   struct item *it = &items[n];
   int          c, yc;
   if (it->kind == 0)
   {
      size_t      len;
      const char *v = ryaml_val(y, node, &len);
      return ryaml_type(y, node) == RYAML_TYPE_SCALAR && len == it->vlen
         && (!len || !memcmp(v, it->val, len));
   }
   if (it->kind == 1 ? !ryaml_is_map(y, node) : !ryaml_is_seq(y, node))
      return 0;
   for (c = it->first, yc = ryaml_first_child(y, node); c >= 0;
         c = items[c].next, yc = ryaml_next_sibling(y, yc))
   {
      if (yc < 0)
         return 0;
      if (it->kind == 1)
      {
         size_t      klen;
         const char *k = ryaml_key(y, yc, &klen);
         if (!k || klen != items[c].klen || memcmp(k, items[c].key, klen))
            return 0;
      }
      if (!same(y, yc, c))
         return 0;
   }
   return yc < 0;
}

struct sink { char *buf; size_t len; };

static int sink_io(const void *buf, size_t len, void *user)
{
   struct sink *s = (struct sink*)user;
   char *tmp = (char*)realloc(s->buf, s->len + len + 1);
   if (!tmp)
      return 0;
   s->buf = tmp;
   memcpy(s->buf + s->len, buf, len);
   s->len += len;
   return 1;
}

static void round_trip(void)
{
   int iter, bad = 0, bad_io = 0;
   printf("round trip\n");
   for (iter = 0; iter < 3000; iter++)
   {
      ryaml_writer_t *w;
      ryaml_t        *y;
      const char     *out;
      size_t          len;
      struct sink     sk = { NULL, 0 };
      int             root;

      rnd_state = (unsigned)iter * 2654435761u + 1;
      nitems    = 0;
      root      = build(0, 0, 0);
      /* the document is a mapping, as everything here writes it */
      items[root].kind = 1;
      if (items[root].first < 0)
      {
         int c = build(1, 0, 1);
         items[c].klen = rnd_text(items[c].key);
         items[c].key[items[c].klen++] = 'A';
         items[root].first = c;
      }
      {
         int c, i = 0;
         for (c = items[root].first; c >= 0; c = items[c].next, i++)
            if (!items[c].klen)
               items[c].key[items[c].klen++] = (char)('a' + i);
      }

      w = ryaml_writer_new(NULL, NULL);
      emit(w, root);
      out = ryaml_writer_memory(w, &len);
      y   = out ? ryaml_parse(out, len) : NULL;
      if (!y || !same(y, ryaml_root(y), root))
      {
         if (!bad++)
            printf("  first mismatch, document %d:\n----\n%s----\n", iter, out ? out : "(none)");
      }
      else
      {
         /* the same through a callback, in pieces */
         ryaml_writer_t *wi = ryaml_writer_new(sink_io, &sk);
         emit(wi, root);
         if (!ryaml_writer_free(wi) || sk.len != len || memcmp(sk.buf, out, len))
            bad_io++;
      }
      ryaml_free(y);
      ryaml_writer_free(w);
      free(sk.buf);
   }
   CHECK(!bad, "3000 random documents read back to the same tree");
   CHECK(!bad_io, "and come out the same through a callback");
   if (bad)
      printf("  %d documents differ\n", bad);
}

static void tricky_scalars(void)
{
   static const char *vals[] = {
      "", " lead", "trail ", "a: b", "a #b", "#x", "-", "- x", "-1", "&a",
      "*a", "[x]", "x,y", "{y}", "\"q\"", "it's", "back\\slash", "tab\tx",
      "nl\nx", "cr\rx", "key:", ":", "?", "? x", "true", "~", "0x10", "|",
      ">", "%x", "@x", "`x", "!x", "a:b", "http://x", "\xc3\xa9t\xc3\xa9",
   };
   size_t i;
   int    bad = 0;
   printf("tricky scalars\n");
   for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
   {
      ryaml_writer_t *w = ryaml_writer_new(NULL, NULL);
      const char     *out, *v;
      size_t          len, vl, n = strlen(vals[i]);
      ryaml_t        *y;
      int             root, m;

      ryaml_writer_begin_map(w);
      ryaml_writer_key(w, vals[i], n);           /* as a key */
      ryaml_writer_scalar(w, vals[i], n);        /* and its value */
      K(w, "seq");
      ryaml_writer_begin_seq(w, 0);
      ryaml_writer_scalar(w, vals[i], n);        /* an item */
      ryaml_writer_begin_seq(w, 1);
      ryaml_writer_scalar(w, vals[i], n);        /* a flow item */
      ryaml_writer_scalar(w, vals[i], n);
      ryaml_writer_end_seq(w);
      ryaml_writer_end_seq(w);
      ryaml_writer_end_map(w);
      out  = ryaml_writer_memory(w, &len);
      y    = out ? ryaml_parse(out, len) : NULL;
      root = ryaml_root(y);
      m    = ryaml_first_child(y, root);
      v    = ryaml_key(y, m, &vl);
      if (!y || !v || vl != n || memcmp(v, vals[i], n)) { bad++; printf("  key %u:\n%s", (unsigned)i, out ? out : ""); }
      v    = ryaml_val(y, m, &vl);
      if (!v || vl != n || memcmp(v, vals[i], n)) { bad++; printf("  value %u:\n%s", (unsigned)i, out ? out : ""); }
      m    = ryaml_find_child(y, root, "seq");
      v    = ryaml_val(y, ryaml_first_child(y, m), &vl);
      if (!v || vl != n || memcmp(v, vals[i], n)) { bad++; printf("  item %u:\n%s", (unsigned)i, out ? out : ""); }
      m    = ryaml_last_child(y, m);
      v    = ryaml_val(y, ryaml_last_child(y, m), &vl);
      if (ryaml_num_children(y, m) != 2 || !v || vl != n || memcmp(v, vals[i], n)) { bad++; printf("  flow item %u:\n%s", (unsigned)i, out ? out : ""); }
      ryaml_free(y);
      ryaml_writer_free(w);
   }
   CHECK(!bad, "every one read back as written");
}

static void refused(void)
{
   ryaml_writer_t *w;
   printf("refused\n");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_begin_map(w);
   S(w, "no key");
   CHECK(ryaml_writer_error(w) && !ryaml_writer_free(w), "a scalar where a key is due");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_end_map(w);
   CHECK(ryaml_writer_error(w) && !ryaml_writer_free(w), "an end without a begin");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_begin_seq(w, 0);
   ryaml_writer_end_map(w);
   CHECK(ryaml_writer_error(w) && !ryaml_writer_free(w), "a sequence ended as a mapping");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_begin_map(w);
   K(w, "a");
   CHECK(!ryaml_writer_free(w), "a mapping left open, its key without a value");

   w = ryaml_writer_new(NULL, NULL);
   ryaml_writer_begin_seq(w, 1);
   ryaml_writer_begin_map(w);
   CHECK(ryaml_writer_error(w) && !ryaml_writer_free(w), "a block mapping inside a flow sequence");

   w = ryaml_writer_new(NULL, NULL);
   S(w, "one");
   S(w, "two");
   CHECK(ryaml_writer_error(w) && !ryaml_writer_free(w), "two values for one document");
}

int main(void)
{
   layout();
   tricky_scalars();
   round_trip();
   refused();
   printf(fails ? "ryaml_writer: %d FAILED\n" : "ryaml_writer: ok\n", fails);
   return fails ? 1 : 0;
}
