/* BCJ2 folders, which 7-Zip writes for programs: four inputs, the main
 * stream and the call and jump targets each from a coder or stored,
 * and the range coder's stream stored. Two archives 7-Zip wrote
 * (r7z_bcj2_fixtures.h) are extracted from memory and through
 * positioned reads, whole, in slices and streamed, and must come out
 * as the octets they were made from. From memory the packed streams are read
 * where they lie: an extraction from memory allocates less than one
 * through reads by more than half the archive, where it would be the
 * same if they were copied. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#include "r7z_bcj2_fixtures.h"

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
   if (!p)
      return;
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

/* prog.bin, as gen.py makes it: calls and jumps to a few targets
 * among other instructions */
static size_t gen(uint8_t *d, size_t n)
{
   static const uint8_t push[3] = { 0x55, 0x89, 0xe5 };
   static const uint8_t jz[6]   = { 0x0f, 0x84, 0x10, 0x00, 0x00, 0x00 };
   static const uint8_t ret[4]  = { 0x8b, 0x45, 0x08, 0xc3 };
   uint32_t x = 1, j = 0;
   size_t   at = 0;
   while (at < n)
   {
      uint8_t  ins[6];
      size_t   len, k;
      uint32_t kind;
      j++;
      kind = j % 5;
      if (j % 61 == 0)
      {
         x    = x * 1103515245u + 12345u;
         kind = (x >> 16) % 5;
      }
      if (kind < 2)
      {
         uint32_t tgt = kind == 0 ? 0x1000u * ((j / 5) % 3) : 0x800u;
         uint32_t rel = tgt - (uint32_t)(at + 5);
         ins[0] = kind == 0 ? 0xe8 : 0xe9;
         ins[1] = (uint8_t)rel;
         ins[2] = (uint8_t)(rel >> 8);
         ins[3] = (uint8_t)(rel >> 16);
         ins[4] = (uint8_t)(rel >> 24);
         len    = 5;
      }
      else if (kind == 2)
      {
         memcpy(ins, push, 3);
         len = 3;
      }
      else if (kind == 3)
      {
         memcpy(ins, jz, 6);
         len = 6;
      }
      else
      {
         memcpy(ins, ret, 4);
         len = 4;
      }
      for (k = 0; k < len && at < n; k++)
         d[at++] = ins[k];
   }
   return n;
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

static void check(const char *what, const char *how, int ok)
{
   if (!ok)
   {
      printf("FAIL: %s, %s\n", what, how);
      failures++;
   }
}

static void one(const char *what, const uint8_t *fx, size_t fx_len,
      size_t n)
{
   uint8_t       *want = (uint8_t*)malloc(n);
   size_t         spent[4];
   struct mem     m;
   unsigned       way;

   gen(want, n);
   m.d = fx;
   m.n = fx_len;
   for (way = 0; way < 2; way++)
   {
      r7z_archive_t *a = NULL;
      const uint8_t *d;
      size_t         got, at = 0;
      int            r, ok = 1;
      const char    *how = way ? "streamed through reads"
                           : "streamed from memory";
      r = way ? r7z_archive_open_read(&a, fx_len, read_at, &m)
              : r7z_archive_open(&a, fx, fx_len);
      if (r != R7Z_OK || r7z_archive_stream_begin(a, 0) != R7Z_OK)
      {
         check(what, how, 0);
         r7z_archive_close(a);
         continue;
      }
      while ((r = r7z_archive_stream_step(a, &d, &got)) == R7Z_PENDING)
      {
         if (got > n - at || (got && memcmp(d, want + at, got)))
            ok = 0;
         at += got;
      }
      check(what, how, ok && r == R7Z_OK && at == n);
      r7z_archive_close(a);
   }
   for (way = 0; way < 4; way++)
   {
      r7z_archive_t *a   = NULL;
      uint8_t       *out = NULL;
      size_t         len = 0;
      int            r;
      const char    *how = way == 0 ? "from memory"
                         : way == 1 ? "through reads"
                         : way == 2 ? "from memory, in slices"
                         : "through reads, in slices";
      r = (way & 1) ? r7z_archive_open_read(&a, fx_len, read_at, &m)
                    : r7z_archive_open(&a, fx, fx_len);
      check(what, how, r == R7Z_OK && r7z_archive_num_entries(a) == 1);
      if (r != R7Z_OK)
         continue;
      total = 0;
      if (way < 2)
         r = r7z_archive_extract(a, 0, &out, &len);
      else
         while ((r = r7z_archive_extract_slice(a, 0, &out, &len)) == R7Z_PENDING) { }
      check(what, how, r == R7Z_OK && len == n && !memcmp(out, want, n));
      spent[way] = total;
      counted_free(out);   /* the archive's allocation */
      r7z_archive_close(a);
   }
   if (     spent[1] < spent[0] + fx_len / 2
         || spent[3] < spent[2] + fx_len / 2)
   {
      printf("  %s: %lu and %lu octets allocated from memory, %lu and %lu"
            " through reads\n", what, (unsigned long)spent[0],
            (unsigned long)spent[2], (unsigned long)spent[1],
            (unsigned long)spent[3]);
      check(what, "the packed streams read where they lie", 0);
   }
   free(want);
}

int main(void)
{
   one("BCJ2 with LZMA", fx_bcj2_lzma, sizeof(fx_bcj2_lzma), 60000);
   one("BCJ2 stored", fx_bcj2_copy, sizeof(fx_bcj2_copy), 6000);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("r7z_bcj2_test: both archives extracted and streamed every way\n");
   return 0;
}
