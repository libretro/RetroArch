/* The zip reader's index for reading a deflated member at any offset
 * (rzip_seek): every read must give the bytes a whole extraction gives.
 *
 *   seek_test                        checks an archive it makes itself
 *   seek_test ARCHIVE.zip [span]     checks every deflated member
 *   seek_test -t ARCHIVE.zip         times the largest member
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <zip/rzip_archive.h>
#include <encodings/crc32.h>
#include <encodings/deflate.h>

static int failures;
#define CHECK(c, what) do { if (!(c)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #c, what); } } while (0)

static uint8_t *file_data;
static size_t   file_len;

static int64_t read_cb(void *ud, uint64_t off, void *dst, size_t len)
{
   (void)ud;
   if (off > file_len)
      return -1;
   if (len > file_len - off)
      len = file_len - (size_t)off;
   memcpy(dst, file_data + off, len);
   return (int64_t)len;
}

static uint32_t rng_state = 12345;
static uint32_t rng(void)
{
   rng_state = rng_state * 1664525u + 1013904223u;
   return rng_state >> 8;
}

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }

/* An archive of one deflated member, made here: some of it text-like,
 * some of it runs, some of it noise, so the stream has blocks of every
 * kind and length. */
#define OWN_LEN (900 * 1024)
static int make_archive(void)
{
   static const char name[] = "own.bin";
   const size_t name_len    = sizeof(name) - 1;
   uint8_t *plain = (uint8_t*)malloc(OWN_LEN);
   uint8_t *z;
   size_t   cap   = OWN_LEN + OWN_LEN / 8 + 4096;
   size_t   clen  = 0;
   size_t   i;
   void    *def;
   uint8_t *p;
   uint32_t crc;

   for (i = 0; i < OWN_LEN; i++)
   {
      uint32_t r = rng();
      if ((i >> 16) % 3 == 0)
         plain[i] = (uint8_t)("the quick brown fox "[i % 20] + ((r & 0xff) < 8));
      else if ((i >> 16) % 3 == 1)
         plain[i] = (uint8_t)((i >> 9) & 0xff);
      else
         plain[i] = (uint8_t)(r & ((r >> 9) & 1 ? 0xff : 0x0f));
   }
   crc = encoding_crc32(0, plain, OWN_LEN);
   z   = (uint8_t*)malloc(30 + name_len + cap + 46 + name_len + 22);
   if (!(def = rdeflate_new(6, -15)))
      return 0;
   rdeflate_set_in(def, plain, OWN_LEN);
   rdeflate_set_out(def, z + 30 + name_len, cap);
   rdeflate_finish(def);
   for (;;)
   {
      size_t rd = 0, wr = 0;
      int st = rdeflate_process(def, &rd, &wr);
      clen += wr;
      if (st == RDEFLATE_PROCESS_END)
         break;
      if (st == RDEFLATE_PROCESS_ERROR || (!rd && !wr))
         return 0;
   }
   rdeflate_free(def);
   free(plain);

   p = z;                                   /* local header */
   memset(p, 0, 30);
   put32(p, 0x04034b50); put16(p + 4, 20); put16(p + 8, 8);
   put32(p + 14, crc); put32(p + 18, (uint32_t)clen); put32(p + 22, OWN_LEN);
   put16(p + 26, (uint32_t)name_len);
   memcpy(p + 30, name, name_len);
   p = z + 30 + name_len + clen;            /* central directory */
   memset(p, 0, 46);
   put32(p, 0x02014b50); put16(p + 4, 20); put16(p + 6, 20); put16(p + 10, 8);
   put32(p + 16, crc); put32(p + 20, (uint32_t)clen); put32(p + 24, OWN_LEN);
   put16(p + 28, (uint32_t)name_len);
   memcpy(p + 46, name, name_len);
   p += 46 + name_len;                      /* and its end */
   memset(p, 0, 22);
   put32(p, 0x06054b50); put16(p + 8, 1); put16(p + 10, 1);
   put32(p + 12, (uint32_t)(46 + name_len));
   put32(p + 16, (uint32_t)(30 + name_len + clen));
   file_data = z;
   file_len  = (size_t)(p + 22 - z);
   return 1;
}

static double now(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void check_member(rzip_archive_t *a, uint32_t idx, uint32_t span,
      const char *how)
{
   const rzip_entry_t *e = rzip_archive_entry(a, idx);
   uint8_t  *want = NULL;
   uint8_t  *got;
   size_t    want_len = 0;
   rzip_seek_t *s;
   uint64_t  upto;
   unsigned  i;
   int       res = 0;

   if (rzip_archive_extract(a, idx, &want, &want_len) != RZIP_OK)
      return;
   s = rzip_seek_new(a, idx, span);
   if (!e->size || e->csize == e->size)
   {
      /* stored: no index */
      if (s && want_len) { /* a deflated member that did not shrink */ }
   }
   if (!s)
   {
      free(want);
      return;
   }
   got = (uint8_t*)malloc(want_len + 1);
   CHECK(rzip_seek_size(s) == want_len, how);
   CHECK(rzip_seek_covered(s) == 0, how);
   /* nothing can be read before it is indexed */
   if (want_len)
      CHECK(rzip_seek_read(s, 0, got, 1) == RZIP_ERROR_PARAM, how);

   /* indexed a piece at a time; what is covered reads right at once */
   for (upto = span / 2; res == 0; upto += span + span / 3)
   {
      uint64_t covered;

      res     = rzip_seek_build(s, upto);
      covered = rzip_seek_covered(s);
      CHECK(res >= 0, how);
      CHECK(res == 1 || covered >= upto, how);
      CHECK(covered <= want_len, how);
      if (covered)
      {
         size_t len = covered > 5000 ? 5000 : (size_t)covered;
         uint64_t off = covered - len;
         CHECK(rzip_seek_read(s, off, got, len) == RZIP_OK, how);
         CHECK(!memcmp(got, want + off, len), how);
      }
      if (covered < want_len)
         CHECK(rzip_seek_read(s, covered, got, 1) == RZIP_ERROR_PARAM, how);
   }
   CHECK(res == 1, how);
   CHECK(rzip_seek_covered(s) == want_len, how);
   CHECK(rzip_seek_build(s, 0) == 1, how);

   /* all of it, and pieces of it from anywhere */
   CHECK(rzip_seek_read(s, 0, got, want_len) == RZIP_OK, how);
   CHECK(!memcmp(got, want, want_len), how);
   for (i = 0; i < 400 && want_len; i++)
   {
      uint64_t off = rng() % want_len;
      size_t   len = rng() % (i % 8 ? 3000 : 3 * span);
      if (len > want_len - off)
         len = want_len - (size_t)off;
      CHECK(rzip_seek_read(s, off, got, len) == RZIP_OK, how);
      CHECK(!memcmp(got, want + off, len), how);
   }
   /* the ends */
   CHECK(rzip_seek_read(s, want_len, got, 0) == RZIP_OK, how);
   CHECK(rzip_seek_read(s, want_len, got, 1) == RZIP_ERROR_PARAM, how);
   if (want_len)
   {
      CHECK(rzip_seek_read(s, want_len - 1, got, 1) == RZIP_OK, how);
      CHECK(got[0] == want[want_len - 1], how);
      CHECK(rzip_seek_read(s, want_len - 1, got, 2) == RZIP_ERROR_PARAM, how);
   }
   rzip_seek_free(s);
   free(got);
   free(want);
}

int main(int argc, char **argv)
{
   rzip_archive_t *a;
   FILE     *f;
   uint32_t  n, i;
   uint32_t  span;
   int       timing = argc > 1 && !strcmp(argv[1], "-t");
   const char *path = timing ? argv[2] : argv[1];
   unsigned  members = 0;

   if (!path)
   {
      rzip_seek_t *s;

      if (!make_archive())
         return 2;
      path = "an archive made here";
      /* its stream has places to restart from, not one block */
      if (rzip_archive_open(&a, file_data, file_len, NULL, NULL) != RZIP_OK)
         return 2;
      s = rzip_seek_new(a, 0, 32768);
      CHECK(s != NULL, path);
      if (s)
      {
         CHECK(rzip_seek_build(s, 200000) == 0, path);
         CHECK(rzip_seek_covered(s) >= 200000 && rzip_seek_covered(s) < OWN_LEN, path);
         rzip_seek_free(s);
      }
      rzip_archive_close(a);
   }
   else
   {
      if (!(f = fopen(path, "rb")))
      {
         fprintf(stderr, "usage: seek_test [[-t] ARCHIVE.zip [span]]\n");
         return 2;
      }
      fseek(f, 0, SEEK_END);
      file_len  = (size_t)ftell(f);
      fseek(f, 0, SEEK_SET);
      file_data = (uint8_t*)malloc(file_len);
      if (fread(file_data, 1, file_len, f) != file_len)
         return 2;
      fclose(f);
   }
   span = (!timing && argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;

   if (timing)
   {
      const rzip_entry_t *big = NULL;
      uint32_t big_idx = 0;
      rzip_seek_t *s;
      uint8_t *buf = (uint8_t*)malloc(2352);
      double t0, t1;
      unsigned reads = 2000;

      if (rzip_archive_open(&a, file_data, file_len, NULL, NULL) != RZIP_OK)
         return 2;
      n = rzip_archive_num_entries(a);
      for (i = 0; i < n; i++)
      {
         const rzip_entry_t *e = rzip_archive_entry(a, i);
         if (!big || e->size > big->size) { big = e; big_idx = i; }
      }
      s  = rzip_seek_new(a, big_idx, 0);
      if (!s)
         return 2;
      t0 = now();
      if (rzip_seek_build(s, (uint64_t)-1) != 1)
         return 1;
      t1 = now();
      printf("%s: %.0f MB in %.0f MB; indexed in %.2f s (%.0f MB/s)\n", big->name,
            big->size / 1048576.0, big->csize / 1048576.0, t1 - t0, big->size / 1048576.0 / (t1 - t0));
      t0 = now();
      for (i = 0; i < reads; i++)
      {
         uint64_t off = ((uint64_t)rng() * 2352) % (big->size - 2352);
         if (rzip_seek_read(s, off, buf, 2352) != RZIP_OK)
            return 1;
      }
      t1 = now();
      printf("a sector from anywhere: %.2f ms each\n", (t1 - t0) * 1000.0 / reads);
      t0 = now();
      for (i = 0; i < 20000; i++)
      {
         if (rzip_seek_read(s, (uint64_t)i * 2352, buf, 2352) != RZIP_OK)
            return 1;
      }
      t1 = now();
      printf("sectors in order: %.1f MB/s\n", 20000 * 2352 / 1048576.0 / (t1 - t0));
      rzip_seek_free(s);
      return 0;
   }

   /* from memory, and through the read callback */
   if (rzip_archive_open(&a, file_data, file_len, NULL, NULL) != RZIP_OK)
      return 2;
   n = rzip_archive_num_entries(a);
   for (i = 0; i < n; i++)
   {
      const rzip_entry_t *e = rzip_archive_entry(a, i);
      if (!e->is_dir && e->csize != e->size)
         members++;
      check_member(a, i, span ? span : 32768, "memory");
      check_member(a, i, span ? span : 100000, "memory, another span");
   }
   rzip_archive_close(a);
   if (rzip_archive_open(&a, NULL, file_len, read_cb, NULL) != RZIP_OK)
      return 2;
   for (i = 0; i < n; i++)
      check_member(a, i, span ? span : 32768, "callback");
   rzip_archive_close(a);

   /* a member whose bytes are damaged is refused, not read wrong for good:
    * the index says so when it gets there */
   if (file_len > 4096)
   {
      if (rzip_archive_open(&a, file_data, file_len, NULL, NULL) != RZIP_OK)
         return 2;
      for (i = 0; i < n; i++)
      {
         const rzip_entry_t *e = rzip_archive_entry(a, i);
         rzip_seek_t *s;
         size_t at;
         uint8_t old;

         if (e->is_dir || e->csize < 2000 || !(s = rzip_seek_new(a, i, 32768)))
            continue;
         at  = (size_t)(e->data_off + e->csize / 2);
         old = file_data[at];
         file_data[at] ^= 0x5a;
         CHECK(rzip_seek_build(s, (uint64_t)-1) < 0, "damaged");
         CHECK(rzip_seek_covered(s) < e->size, "damaged");
         file_data[at] = old;
         rzip_seek_free(s);
         break;
      }
      rzip_archive_close(a);
   }

   free(file_data);
   if (failures)
   {
      printf("seek_test: %d failure(s) in %s\n", failures, path);
      return 1;
   }
   printf("seek_test: ok, %u deflated member(s) of %s\n", members, path);
   return 0;
}
