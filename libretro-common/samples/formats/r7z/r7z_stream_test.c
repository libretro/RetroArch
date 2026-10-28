/* Entries streamed out of a solid folder with r7z_archive_stream_*():
 * the three of the LZMA2 and LZMA archives in
 * ../../file/archive_7z/archive_7z_fixtures.h, from memory and through
 * positioned reads. Streamed in order, each after the first goes on
 * from where the one before ended - r7z_archive.c is built with its
 * allocations counted, and the later ones allocate nothing - and out of
 * order the stream starts again; every way they come out as
 * r7z_archive_extract() gives them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#include "../../file/archive_7z/archive_7z_fixtures.h"

static int    failures;
static size_t total;

/* the counting allocator r7z_archive.c is built with here */
void *counted_malloc(size_t n)
{
   size_t *p = (size_t*)malloc(n + sizeof(size_t) * 2);
   if (!p)
      return NULL;
   p[0]   = n;
   total += n;
   return p + 2;
}

void *counted_calloc(size_t n, size_t size)
{
   void *p = counted_malloc(n * size);
   if (p)
      memset(p, 0, n * size);
   return p;
}

void counted_free(void *v)
{
   size_t *p = (size_t*)v;
   if (p)
      free(p - 2);
}

void *counted_realloc(void *v, size_t n)
{
   size_t *p = (size_t*)v;
   void   *q;
   if (!p)
      return counted_malloc(n);
   if (!(q = counted_malloc(n)))
      return NULL;
   memcpy(q, p, p[-2] < n ? p[-2] : n);
   counted_free(p);
   return q;
}

static void check(int ok, const char *what, unsigned i)
{
   if (!ok)
   {
      printf("FAIL: %s, entry %u\n", what, i);
      failures++;
   }
}

struct mem
{
   const uint8_t *d;
   size_t         n;
};

static int64_t read_at(void *ud, uint64_t off, void *dst, size_t len)
{
   const struct mem *m = (const struct mem*)ud;
   if (off >= m->n)
      return 0;
   if (len > m->n - off)
      len = (size_t)(m->n - off);
   memcpy(dst, m->d + off, len);
   return (int64_t)len;
}

/* Entry @i streamed: as extracted, and how much its start allocated */
static int streamed(r7z_archive_t *a, uint32_t i, size_t *alloc)
{
   uint8_t       *want = NULL;
   size_t         want_len = 0, at = 0, n;
   size_t         before = total;
   const uint8_t *d;
   int            res, ok;

   if (r7z_archive_extract(a, i, &want, &want_len) != R7Z_OK)
      return 0;
   before = total;
   if (r7z_archive_stream_begin(a, i) != R7Z_OK)
   {
      counted_free(want);
      return 0;
   }
   *alloc = total - before;
   ok     = 1;
   while ((res = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
   {
      if (n > want_len - at || (n && memcmp(d, want + at, n)))
         ok = 0;
      at += n;
   }
   counted_free(want);
   return ok && res == R7Z_OK && at == want_len;
}

static void run(const char *what, const uint8_t *fx, size_t len, int read)
{
   struct mem     m;
   r7z_archive_t *a = NULL;
   size_t         alloc;
   uint32_t       i;

   m.d = fx;
   m.n = len;
   if ((read ? r7z_archive_open_read(&a, len, read_at, &m)
             : r7z_archive_open(&a, fx, len)) != R7Z_OK)
   {
      check(0, what, 0);
      return;
   }
   /* in order: the first starts the folder, the rest go on */
   for (i = 0; i < 3; i++)
   {
      check(streamed(a, i, &alloc), what, i);
      check(i ? alloc == 0 : alloc > 0, "in order, going on", i);
   }
   /* the last ends the folder, and out of order starts it again */
   check(streamed(a, 2, &alloc) && alloc > 0, "again", 2);
   check(streamed(a, 0, &alloc) && alloc > 0, "back to the start", 0);
   check(streamed(a, 1, &alloc) && alloc == 0, "on from the first", 1);
   r7z_archive_stream_end(a);
   r7z_archive_close(a);
}

int main(void)
{
   run("LZMA2", fx_big_lzma2, sizeof(fx_big_lzma2), 0);
   run("LZMA2, read", fx_big_lzma2, sizeof(fx_big_lzma2), 1);
   run("LZMA", fx_big_lzma, sizeof(fx_big_lzma), 0);
   run("LZMA, read", fx_big_lzma, sizeof(fx_big_lzma), 1);
   if (failures)
      return 1;
   printf("r7z_stream_test: a solid folder's entries streamed, in order once\n");
   return 0;
}
