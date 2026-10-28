/* Entries streamed out of a solid folder with r7z_archive_stream_*():
 * the three of the LZMA2 and LZMA archives in
 * ../../file/archive_7z/archive_7z_fixtures.h, and of each converter's
 * in r7z_filter_fixtures.h, from memory and through positioned reads.
 * Streamed in order, each after the first goes on from where the one
 * before ended - r7z_archive.c is built with its allocations counted,
 * and the later ones allocate nothing - and out of order the stream
 * starts again; every way they come out as r7z_archive_extract() gives
 * them, and the converters', the other coders' and BCJ2's as
 * code_like() made them. r7z_archive.c is built here to make and read
 * a few KiB a step, so every coder takes many. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#include "../../file/archive_7z/archive_7z_fixtures.h"
#include "r7z_filter_fixtures.h"
/* fx_big_bzip2 from there; its fx_big_deflate is another than the one
 * above */
#define fx_big_deflate methods_big_deflate
#include "r7z_methods_fixtures.h"
#undef fx_big_deflate

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

/* @n octets of something like code, file @which's: a few plain words
 * over and over, and every so often one of the call, branch and jump
 * forms the converters rewrite, each 4 octets so the RISC ones stay on
 * their words, x86's call and jump padded out to 8 and an IA-64 bundle
 * of 16 with a branch in its third slot */
static void code_like(uint8_t *d, size_t n, unsigned which)
{
   static const uint8_t forms[13][16] = {
      { 0xE8, 0x10, 0, 0, 0, 0x90, 0x90, 0x90 },  /* x86 call */
      { 0xE9, 0x20, 0, 0, 0, 0x90, 0x90, 0x90 },  /* x86 jmp */
      { 0x10, 0, 0, 0xEB },                       /* ARM BL */
      { 0, 0xF0, 0x10, 0xF8 },                    /* Thumb BL pair */
      { 0x10, 0, 0, 0x94 },                       /* ARM64 BL */
      { 0x10, 0, 0, 0x90 },                       /* ARM64 ADRP */
      { 0x48, 0, 0x10, 0x01 },                    /* PPC bl */
      { 0x40, 0, 0, 0x10 },                       /* SPARC call */
      { 0x55, 0x48, 0x89, 0xE5 },
      { 0x8B, 0x45, 0xFC, 0x90 },
      { 0xC3, 0x90, 0x90, 0x90 },
      { 0x00, 0x00, 0xA0, 0xE1 },
      { 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0x50 } };
   static const unsigned lens[13] = { 8, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      16 };
   uint32_t x = 2463534242u + which * 977u;
   size_t   at = 0;
   while (at < n)
   {
      unsigned f, k;
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      /* a form one time in four, else a plain word */
      f = (x & 3) ? 8 + ((x >> 4) & 3) : (x >> 8) % 8;
      if (f == 7 && (x & 0x100000))
         f = 12;
      for (k = 0; k < lens[f] && at < n; k++, at++)
         d[at] = forms[f][k];
   }
}

/* An archive of code_like()'s three files, or of s.bin, the first of
 * them cut short: each entry streamed in order, then out of it, as
 * code_like() made it */
static void filtered(const char *what, const uint8_t *fx, size_t len,
      int read)
{
   static const size_t   sizes[3] = { 6001, 4500, 7003 };
   static const uint32_t order[6] = { 0, 1, 2, 1, 2, 0 };
   static uint8_t        want[7003];
   unsigned              files = fx == fx_m_copy ? 1 : 3;
   struct mem     m;
   r7z_archive_t *a = NULL;
   unsigned       k;

   m.d = fx;
   m.n = len;
   if ((read ? r7z_archive_open_read(&a, len, read_at, &m)
             : r7z_archive_open(&a, fx, len)) != R7Z_OK)
   {
      check(0, what, 0);
      return;
   }
   for (k = 0; k < 6; k++)
   {
      uint32_t       i  = files == 1 ? 0 : order[k];
      size_t         at = 0, n, before = total, size = files == 1 ? 3001
                       : sizes[i];
      const uint8_t *d;
      int            res, ok = 1;
      code_like(want, sizes[i], i);
      if (r7z_archive_stream_begin(a, i) != R7Z_OK)
      {
         check(0, what, i);
         continue;
      }
      check(files == 3 && (k == 1 || k == 2) ? total == before : 1,
            "in order, going on", i);
      while ((res = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
      {
         if (n > size - at || (n && memcmp(d, want + at, n)))
            ok = 0;
         at += n;
      }
      check(ok && res == R7Z_OK && at == size, what, i);
   }
   r7z_archive_stream_end(a);
   r7z_archive_close(a);
}

/* @n octets of x86 calls and jumps and of E8 and E9 that are not, close
 * together, so some are always about where a piece ends */
static void x86_dense(uint8_t *d, size_t n)
{
   uint32_t x = 13u;
   size_t   at;
   for (at = 0; at < n; at++)
   {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      d[at] = (x & 1) ? (uint8_t)(0xE8 | ((x >> 1) & 1))
            : (uint8_t)((x >> 2) & 3);
   }
}

/* x86 calls across where the pieces end, which the converter's state
 * carries over */
static void dense(int read)
{
   static uint8_t want[9000];
   struct mem     m;
   r7z_archive_t *a = NULL;
   const uint8_t *d;
   size_t         at = 0, n;
   int            res, ok = 1;

   m.d = fx_f_dense;
   m.n = sizeof(fx_f_dense);
   x86_dense(want, sizeof(want));
   if (     (read ? r7z_archive_open_read(&a, m.n, read_at, &m)
                  : r7z_archive_open(&a, m.d, m.n)) != R7Z_OK
         || r7z_archive_stream_begin(a, 0) != R7Z_OK)
   {
      check(0, "x86, dense", 0);
      r7z_archive_close(a);
      return;
   }
   while ((res = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
   {
      if (n > sizeof(want) - at || (n && memcmp(d, want + at, n)))
         ok = 0;
      at += n;
   }
   check(ok && res == R7Z_OK && at == sizeof(want), "x86, dense", 0);
   r7z_archive_close(a);
}

/* @n octets that do not compress: PPMd's input for them is about as
 * long as they are */
static void noise(uint8_t *d, size_t n)
{
   uint32_t x = 362436069u;
   size_t   at;
   for (at = 0; at < n; at++)
   {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      d[at] = (uint8_t)(x >> 11);
   }
}

/* One entry from @fx, streamed from memory and through reads, against
 * @want: PPMd over noise, its input read a few octets past where each
 * step stops short of the end of what is fetched; BZip2 in blocks that
 * go round its ring */
static void streamed_one(const char *what, const uint8_t *fx, size_t len,
      uint32_t i, const uint8_t *want, size_t want_len)
{
   int read;
   for (read = 0; read < 2; read++)
   {
      struct mem     m;
      r7z_archive_t *a = NULL;
      const uint8_t *d;
      size_t         at = 0, n;
      int            res, ok = 1;
      m.d = fx;
      m.n = len;
      if (     (read ? r7z_archive_open_read(&a, len, read_at, &m)
                     : r7z_archive_open(&a, fx, len)) != R7Z_OK
            || r7z_archive_stream_begin(a, i) != R7Z_OK)
      {
         check(0, what, i);
         r7z_archive_close(a);
         continue;
      }
      while ((res = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
      {
         if (n > want_len - at || (n && memcmp(d, want + at, n)))
            ok = 0;
         at += n;
      }
      check(ok && res == R7Z_OK && at == want_len, what, i);
      r7z_archive_close(a);
   }
}

static void noisy_and_blocks(void)
{
   static uint8_t want[6000];
   r7z_archive_t *a = NULL;
   uint32_t       i;
   noise(want, sizeof(want));
   streamed_one("PPMd over noise", fx_m_noise, sizeof(fx_m_noise), 0,
         want, sizeof(want));
#ifdef HAVE_RBZIP2
   if (r7z_archive_open(&a, fx_big_bzip2, sizeof(fx_big_bzip2)) != R7Z_OK)
   {
      check(0, "BZip2 blocks", 0);
      return;
   }
   for (i = 0; i < r7z_archive_num_entries(a); i++)
   {
      uint8_t *w = NULL;
      size_t   w_len = 0;
      if (     r7z_archive_entry(a, i)->is_dir
            || !r7z_archive_entry(a, i)->size)
         continue;
      if (r7z_archive_extract(a, i, &w, &w_len) != R7Z_OK)
         check(0, "BZip2 blocks, whole", i);
      else
         streamed_one("BZip2 blocks", fx_big_bzip2, sizeof(fx_big_bzip2),
               i, w, w_len);
      counted_free(w);
   }
   r7z_archive_close(a);
#endif
}

int main(void)
{
   int r;
   run("LZMA2", fx_big_lzma2, sizeof(fx_big_lzma2), 0);
   run("LZMA2, read", fx_big_lzma2, sizeof(fx_big_lzma2), 1);
   run("LZMA", fx_big_lzma, sizeof(fx_big_lzma), 0);
   run("LZMA, read", fx_big_lzma, sizeof(fx_big_lzma), 1);
   for (r = 0; r < 2; r++)
   {
      filtered("BCJ", fx_f_bcj, sizeof(fx_f_bcj), r);
      filtered("BCJ, LZMA2", fx_f_bcjl2, sizeof(fx_f_bcjl2), r);
      filtered("ARM", fx_f_arm, sizeof(fx_f_arm), r);
      filtered("ARMT", fx_f_armt, sizeof(fx_f_armt), r);
      filtered("ARM64", fx_f_arm64, sizeof(fx_f_arm64), r);
      filtered("PPC", fx_f_ppc, sizeof(fx_f_ppc), r);
      filtered("SPARC", fx_f_sparc, sizeof(fx_f_sparc), r);
      filtered("IA64", fx_f_ia64, sizeof(fx_f_ia64), r);
      filtered("Delta", fx_f_delta, sizeof(fx_f_delta), r);
      dense(r);
      filtered("ARM and Delta", fx_f_two, sizeof(fx_f_two), r);
      filtered("PPMd", fx_m_ppmd, sizeof(fx_m_ppmd), r);
      filtered("BZip2", fx_m_bzip2, sizeof(fx_m_bzip2), r);
      filtered("Deflate", fx_m_deflate, sizeof(fx_m_deflate), r);
      filtered("Deflate64", fx_m_deflate64, sizeof(fx_m_deflate64), r);
      filtered("BCJ2", fx_m_bcj2, sizeof(fx_m_bcj2), r);
      filtered("BCJ and Copy", fx_m_copy, sizeof(fx_m_copy), r);
   }
   noisy_and_blocks();
   if (failures)
      return 1;
   printf("r7z_stream_test: solid folders' entries streamed, in order once, "
         "through every coder\n");
   return 0;
}
