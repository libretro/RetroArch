/* Regression test for the libretrodb reader: malformed-input
 * handling and the sliding-window cursor stream.
 *
 * Every case here corresponds to a defect that was live at some
 * point.  A .rdb is content the frontend downloads, so the reader
 * has to survive anything in that file rather than trusting it.
 *
 *   missing metadata key    rmsgpack_dom_read_into() fed the NULL
 *                           from a failed map lookup straight into
 *                           "switch (value->type)".
 *   metadata key mistyped   the same function switched on the type
 *                           found in the *file* and consumed a
 *                           different number of va_args per case,
 *                           sliding the caller's list out of step.
 *   deep nesting            the readers recursed once per level of
 *                           container nesting with no depth limit,
 *                           so a run of 0x91 exhausted the stack.
 *   no nil sentinel         intfstream_read() reports EOF as a
 *                           short read, not -1, so the reader took
 *                           its zero-initialised type byte for a
 *                           positive fixint and returned a
 *                           fabricated record forever.
 *   truncated record        rmsgpack_dom_read_with() freed an
 *                           output value it had never initialised;
 *                           in the cursor loop that slot still held
 *                           the previous record, so the free walked
 *                           a dangling map.
 *   empty containers        a zero-length map or array is legal
 *                           MsgPack, but calloc(0) may return NULL
 *                           and that was read as failure.
 *   index headers           binsearch() read before its count == 0
 *                           test, recursed without shrinking on a
 *                           one-element range, and loaded the
 *                           record offset with an unaligned cast;
 *                           nothing cross-checked count against the
 *                           payload the header reserved.
 *   window spanning         the cursor walks through a fixed
 *                           sliding buffer; records have to survive
 *                           crossing its boundary.
 *   oversized field         a field wider than the window cannot be
 *                           served from it, and returning failure
 *                           silently dropped the record.
 *   query slices            libretrodb_query_compile() takes a
 *                           (pointer, length) pair, but the parser
 *                           handed identifier slices to strlcpy(),
 *                           which strlen()s its source, and indexed
 *                           buff.data[buff.offset] after chomping
 *                           trailing space without checking the
 *                           offset against the length.
 *
 * Self-contained: builds each .rdb in a temp directory, exercises
 * it, and reports.  Exits non-zero if any case regresses.
 *
 * Run with: make SANITIZER=address,undefined && ./libretrodb_parser_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../libretrodb.h"
#include "../../rmsgpack_dom.h"
#include "../../query.h"

/* A record count no legitimate test file reaches; hitting it means
 * the cursor is not terminating. */
#define RUNAWAY_LIMIT 100000

static int failures = 0;
static int checks   = 0;

/* Printed before the case runs, so a crash still says which one. */
static void begin(const char *what)
{
   printf("  .... %-34s", what);
}

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("\r");
   if (ok)
      printf("  ok    %-34s %s\n", what, detail ? detail : "");
   else
   {
      printf("  FAIL  %-34s %s\n", what, detail ? detail : "");
      failures++;
   }
}

/* ------------------------------------------------------------------
 * Minimal MsgPack writers, so the test does not depend on the
 * library it is testing to produce its inputs.
 * ------------------------------------------------------------------ */

typedef struct
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
} buf_t;

static void bput(buf_t *b, const void *p, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t want = (b->cap ? b->cap * 2 : 256);
      while (want < b->len + n)
         want *= 2;
      b->data = (uint8_t*)realloc(b->data, want);
      b->cap  = want;
   }
   memcpy(b->data + b->len, p, n);
   b->len += n;
}

static void bbyte(buf_t *b, uint8_t v)          { bput(b, &v, 1); }
static void bfixmap(buf_t *b, unsigned n)       { bbyte(b, (uint8_t)(0x80 | n)); }
static void bfixstr(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, (uint8_t)(0xa0 | n));
   bput(b, s, n);
}
static void bstr32(buf_t *b, const void *s, uint32_t n)
{
   uint8_t h[5];
   h[0] = 0xdb;
   h[1] = (uint8_t)(n >> 24); h[2] = (uint8_t)(n >> 16);
   h[3] = (uint8_t)(n >> 8);  h[4] = (uint8_t)n;
   bput(b, h, 5);
   bput(b, s, n);
}
static void bbin(buf_t *b, const void *s, uint8_t n)
{
   bbyte(b, 0xc4);
   bbyte(b, n);
   bput(b, s, n);
}
static void buint8(buf_t *b, uint8_t v)         { bbyte(b, 0xcc); bbyte(b, v); }
static void bnil(buf_t *b)                      { bbyte(b, 0xc0); }

/* Header is "RARCHDB\0" plus a big-endian metadata offset. */
static int write_rdb(const char *path, const buf_t *body, const buf_t *meta)
{
   FILE   *f;
   uint8_t hdr[16];
   uint64_t off = 16 + (uint64_t)body->len;
   int      i;

   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(hdr, 1, sizeof(hdr), f);
   if (body->len)
      fwrite(body->data, 1, body->len, f);
   if (meta && meta->len)
      fwrite(meta->data, 1, meta->len, f);
   fclose(f);
   return 1;
}

static void meta_count(buf_t *m, uint8_t n)
{
   bfixmap(m, 1);
   bfixstr(m, "count");
   buint8(m, n);
}

static void bfree(buf_t *b) { free(b->data); memset(b, 0, sizeof(*b)); }

/* ------------------------------------------------------------------
 * Harness
 * ------------------------------------------------------------------ */

/* Walk a database. Returns record count, -1 if it could not be
 * opened, -2 if the cursor would not open, -3 if it ran away. */
static long walk(const char *path)
{
   libretrodb_t        *db  = libretrodb_new();
   libretrodb_cursor_t *cur = libretrodb_cursor_new();
   struct rmsgpack_dom_value item;
   long n = 0;
   long rv;

   if (!db || !cur)
      return -1;

   if (libretrodb_open(path, db, false) != 0)
   {
      rv = -1;
      goto done_nocursor;
   }
   if (libretrodb_cursor_open(db, cur, NULL) != 0)
   {
      rv = -2;
      goto done_db;
   }

   while (libretrodb_cursor_read_item(cur, &item) == 0)
   {
      rmsgpack_dom_value_free(&item);
      if (++n > RUNAWAY_LIMIT)
      {
         rv = -3;
         libretrodb_cursor_close(cur);
         goto done_db;
      }
   }
   libretrodb_cursor_close(cur);
   rv = n;

done_db:
   libretrodb_close(db);
done_nocursor:
   libretrodb_free(db);
   libretrodb_cursor_free(cur);
   return rv;
}

/* Collect the "name" of every record, joined by '|', so a walk can
 * be compared byte for byte against what was written. */
static char *walk_names(const char *path)
{
   libretrodb_t        *db  = libretrodb_new();
   libretrodb_cursor_t *cur = libretrodb_cursor_new();
   struct rmsgpack_dom_value item;
   buf_t out;
   unsigned i;

   memset(&out, 0, sizeof(out));

   if (!db || !cur || libretrodb_open(path, db, false) != 0)
   {
      libretrodb_free(db);
      libretrodb_cursor_free(cur);
      return NULL;
   }
   if (libretrodb_cursor_open(db, cur, NULL) != 0)
   {
      libretrodb_close(db);
      libretrodb_free(db);
      libretrodb_cursor_free(cur);
      return NULL;
   }

   while (libretrodb_cursor_read_item(cur, &item) == 0)
   {
      if (item.type == RDT_MAP)
      {
         for (i = 0; i < item.val.map.len; i++)
         {
            struct rmsgpack_dom_value *k = &item.val.map.items[i].key;
            struct rmsgpack_dom_value *v = &item.val.map.items[i].value;
            if (   k->type == RDT_STRING && k->val.string.buff
                && !strcmp(k->val.string.buff, "name")
                && v->type == RDT_STRING && v->val.string.buff)
            {
               bput(&out, v->val.string.buff,
                     strlen(v->val.string.buff));
               bbyte(&out, '|');
            }
         }
      }
      rmsgpack_dom_value_free(&item);
   }
   bbyte(&out, 0);

   libretrodb_cursor_close(cur);
   libretrodb_close(db);
   libretrodb_free(db);
   libretrodb_cursor_free(cur);
   return (char*)out.data;
}

/* ------------------------------------------------------------------
 * Cases
 * ------------------------------------------------------------------ */

static void case_metadata(const char *dir)
{
   char  path[512];
   buf_t body, meta;

   /* metadata map keyed "kount" instead of "count" */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bnil(&body);
   bfixmap(&meta, 1); bfixstr(&meta, "kount"); buint8(&meta, 1);
   sprintf(path, "%s/meta_missing_key.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("metadata key absent");
   check(walk(path) == -1, "metadata key absent", "rejected, no NULL deref");
   bfree(&body); bfree(&meta);

   /* "count" present but stored as a string */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bnil(&body);
   bfixmap(&meta, 1); bfixstr(&meta, "count"); bfixstr(&meta, "AAAAAAAA");
   sprintf(path, "%s/meta_wrong_type.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("metadata key mistyped");
   check(walk(path) == -1, "metadata key mistyped", "rejected, va_list intact");
   bfree(&body); bfree(&meta);

   /* "count" as a positive fixint rather than uint8: a different but
    * legal encoding, and must still be accepted */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bfixmap(&body, 1); bfixstr(&body, "name"); bfixstr(&body, "One");
   bnil(&body);
   bfixmap(&meta, 1); bfixstr(&meta, "count"); bbyte(&meta, 1);
   sprintf(path, "%s/meta_fixint.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("metadata count as fixint");
   check(walk(path) == 1, "metadata count as fixint", "accepted");
   bfree(&body); bfree(&meta);
}

static void case_nesting(const char *dir)
{
   char  path[512];
   buf_t body, meta;
   int   i;

   /* shallow nesting stays legal */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   for (i = 0; i < 8; i++)
      bbyte(&body, 0x91);          /* fixarray of one */
   bbyte(&body, 0x00);
   bnil(&body);
   meta_count(&meta, 1);
   sprintf(path, "%s/nest_shallow.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("nesting within the limit");
   check(walk(path) == 1, "nesting within the limit", "accepted");
   bfree(&body); bfree(&meta);

   /* a long run of container headers must be refused rather than
    * recursed into until the stack runs out */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   for (i = 0; i < 100000; i++)
      bbyte(&body, 0x91);
   bbyte(&body, 0x00);
   bnil(&body);
   meta_count(&meta, 1);
   sprintf(path, "%s/nest_deep.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("nesting past the limit");
   check(walk(path) == 0, "nesting past the limit", "refused, stack intact");
   bfree(&body); bfree(&meta);
}

static void case_truncation(const char *dir)
{
   char  path[512];
   buf_t body, meta;

   /* no trailing nil: the walk has to end at EOF, not spin */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bfixmap(&body, 1); bfixstr(&body, "name"); bfixstr(&body, "One");
   meta_count(&meta, 1);
   sprintf(path, "%s/no_sentinel.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("sentinel missing");
   check(walk(path) >= 0, "sentinel missing", "terminates at EOF");
   bfree(&body); bfree(&meta);

   /* a complete record followed by a bare map header: the second
    * read fails, and the value it fails on is the one the previous
    * iteration left behind */
   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bfixmap(&body, 1); bfixstr(&body, "name"); bfixstr(&body, "AAAAAAAAAAAAAAAA");
   bbyte(&body, 0x81);
   meta_count(&meta, 2);
   sprintf(path, "%s/trunc_after_record.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("record truncated mid-map");
   check(walk(path) >= 0, "record truncated mid-map", "no use-after-free");
   bfree(&body); bfree(&meta);
}

static void case_empty_containers(const char *dir)
{
   char  path[512];
   buf_t body, meta;

   memset(&body, 0, sizeof(body)); memset(&meta, 0, sizeof(meta));
   bbyte(&body, 0x80);                              /* empty map      */
   bfixmap(&body, 1); bfixstr(&body, "tags");
   bbyte(&body, 0x90);                              /* empty array    */
   bfixmap(&body, 1); bfixstr(&body, "name"); bfixstr(&body, "Real");
   bnil(&body);
   meta_count(&meta, 3);
   sprintf(path, "%s/empty_containers.rdb", dir);
   write_rdb(path, &body, &meta);
   begin("empty map and array records");
   check(walk(path) == 3, "empty map and array records", "all three read");
   bfree(&body); bfree(&meta);
}

/* The cursor reads through a fixed sliding buffer.  Build a database
 * comfortably larger than it, and one carrying a field wider than
 * it, and require both to read back exactly. */
static void case_window(const char *dir)
{
   char   path[512];
   buf_t  body, meta, expect;
   char  *got;
   char   name[64];
   int    i;
   const int records = 4000;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));
   memset(&expect, 0, sizeof(expect));

   for (i = 0; i < records; i++)
   {
      sprintf(name, "Record %06d With Padding To Widen The Entry", i);
      bfixmap(&body, 2);
      bfixstr(&body, "name");
      bstr32(&body, name, (uint32_t)strlen(name));
      bfixstr(&body, "crc");
      bbin(&body, "\xde\xad\xbe\xef", 4);
      bput(&expect, name, strlen(name));
      bbyte(&expect, '|');
   }
   bnil(&body);
   bbyte(&expect, 0);
   meta_count(&meta, 1);

   sprintf(path, "%s/window_span.rdb", dir);
   write_rdb(path, &body, &meta);

   begin("database spanning many windows");
   check(walk(path) == records, "database spanning many windows",
         "record count matches");
   begin("records across window edges");
   got = walk_names(path);
   check(got && !strcmp(got, (char*)expect.data),
         "records across window edges", "names byte-identical");
   free(got);
   bfree(&body); bfree(&meta); bfree(&expect);

   /* One field larger than any plausible window.  Reading it needs a
    * single call wider than the buffer; failing that call drops the
    * record without any error reaching the caller. */
   {
      char *big = (char*)malloc(200000);
      memset(big, 'A', 200000);
      memset(&body, 0, sizeof(body));
      memset(&meta, 0, sizeof(meta));
      bfixmap(&body, 2);
      bfixstr(&body, "crc");
      bbin(&body, "\xde\xad\xbe\xef", 4);
      bfixstr(&body, "name");
      bstr32(&body, big, 200000);
      bnil(&body);
      meta_count(&meta, 1);
      sprintf(path, "%s/window_bigfield.rdb", dir);
      write_rdb(path, &body, &meta);
      begin("field wider than the window");
      check(walk(path) == 1, "field wider than the window",
            "record still read");
      free(big);
      bfree(&body); bfree(&meta);
   }
}

/* Index headers are three independent file-supplied numbers with no
 * relationship enforced between them. */
static void case_index(const char *dir)
{
   char  path[512];
   buf_t body, meta, tail;
   struct { const char *name; uint8_t key_size; uint8_t next;
            uint8_t count; int payload; int expect_found;
            const char *what; } cases[] = {
      { "crc", 4, 12, 1,   12, 1, "index well formed"          },
      { "crc", 4, 12, 200, 12, 0, "index count exceeds payload" },
      { "crc", 4,  0, 50,   0, 0, "index payload empty"         },
      { "crc", 0, 64,  8,  64, 0, "index key_size zero"         },
      { "zzz", 4,  0,  1,   0, 0, "index chain does not advance" }
   };
   unsigned c;

   for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
   {
      libretrodb_t *db;
      struct rmsgpack_dom_value out;
      unsigned char key[4];
      int found;

      key[0] = 0xde; key[1] = 0xad; key[2] = 0xbe; key[3] = 0xef;

      memset(&body, 0, sizeof(body));
      memset(&meta, 0, sizeof(meta));
      memset(&tail, 0, sizeof(tail));

      bfixmap(&body, 2);
      bfixstr(&body, "name"); bfixstr(&body, "Rec");
      bfixstr(&body, "crc");  bbin(&body, "\xde\xad\xbe\xef", 4);
      bnil(&body);
      meta_count(&meta, 1);

      /* index header, then its payload */
      bfixmap(&tail, 4);
      bfixstr(&tail, "name");     bfixstr(&tail, cases[c].name);
      bfixstr(&tail, "key_size"); buint8(&tail, cases[c].key_size);
      bfixstr(&tail, "next");     buint8(&tail, cases[c].next);
      bfixstr(&tail, "count");    buint8(&tail, cases[c].count);
      if (cases[c].payload)
      {
         int j;
         bput(&tail, "\xde\xad\xbe\xef", 4);
         for (j = 0; j < 8; j++)
            bbyte(&tail, j == 0 ? 16 : 0);
         for (j = 12; j < cases[c].payload; j++)
            bbyte(&tail, 0);
      }
      /* metadata and the index chain both live past the records */
      bput(&meta, tail.data, tail.len);

      sprintf(path, "%s/index_%u.rdb", dir, c);
      write_rdb(path, &body, &meta);

      begin(cases[c].what);
      db    = libretrodb_new();
      found = 0;
      if (db && libretrodb_open(path, db, false) == 0)
      {
         if (libretrodb_find_entry(db, "crc", key, &out) == 0)
         {
            found = 1;
            rmsgpack_dom_value_free(&out);
         }
         libretrodb_close(db);
      }
      libretrodb_free(db);

      check(found == cases[c].expect_found, cases[c].what,
            cases[c].expect_found ? "entry located" : "rejected cleanly");

      bfree(&body); bfree(&meta); bfree(&tail);
   }
}

/* libretrodb_query_compile() is documented by its signature to take
 * a pointer and a length.  Feed it exactly-sized heap buffers with no
 * terminator so that any read past the length is a heap overflow the
 * sanitizer can see, rather than landing on a NUL that happens to be
 * there. */
static void case_query_slices(const char *dir)
{
   static const struct
   {
      const char *text;
      const char *what;
   } queries[] = {
      { "{crc:or(b\"DEADBEEF\",b\"00000000\")}", "query with identifiers"   },
      { "{'serial': b'414243'}",                "query with binary string" },
      { "{'a':",                                "query ending at a value"  },
      { "{'a': ",                               "query ending after space" },
      { "   ",                                  "query that is all space"  },
      { "{crc:99999999999999999999999}",        "query with huge integer"  },
      { "{'a': b\"\"}",                           "query with empty binary"  },
      { "{nosuchfunction(1)}",                  "query naming unknown func" }
   };
   char   path[512];
   buf_t  body, meta;
   unsigned i;
   libretrodb_t *db = libretrodb_new();

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));
   bfixmap(&body, 1); bfixstr(&body, "name"); bfixstr(&body, "One");
   bnil(&body);
   meta_count(&meta, 1);
   sprintf(path, "%s/query_host.rdb", dir);
   write_rdb(path, &body, &meta);
   bfree(&body); bfree(&meta);

   if (!db || libretrodb_open(path, db, false) != 0)
   {
      check(0, "query host database", "could not be opened");
      libretrodb_free(db);
      return;
   }

   for (i = 0; i < sizeof(queries) / sizeof(queries[0]); i++)
   {
      size_t      n     = strlen(queries[i].text);
      char       *exact = (char*)malloc(n ? n : 1);
      const char *err   = NULL;
      void       *q;

      memcpy(exact, queries[i].text, n);
      begin(queries[i].what);
      q = libretrodb_query_compile(db, exact, n, &err);
      /* Compiling or rejecting are both fine; reading out of bounds
       * while deciding is not, and that is what the sanitizer sees. */
      check(1, queries[i].what, (q && !err) ? "compiled" : "rejected");
      if (q && !err)
         libretrodb_query_free((libretrodb_query_t*)q);
      free(exact);
   }

   libretrodb_close(db);
   libretrodb_free(db);
}

int main(int argc, char **argv)
{
   const char *dir = (argc > 1) ? argv[1] : "/tmp";

   /* Unbuffered: several of these cases crash outright when the
    * defect they cover is present, and the line already printed is
    * what identifies which one. */
   setvbuf(stdout, NULL, _IONBF, 0);

   printf("libretrodb parser regression test (dir: %s)\n\n", dir);

   case_metadata(dir);
   case_nesting(dir);
   case_truncation(dir);
   case_empty_containers(dir);
   case_window(dir);
   case_index(dir);
   case_query_slices(dir);

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
