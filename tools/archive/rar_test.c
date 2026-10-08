/* libretro-common's RAR reader on real archives.
 *
 *   rar_test <dir with the archives of tools/archive/rar>
 *
 * Every member that the reader says it can unpack has to come out, which
 * means with the CRC-32 the archive gives for it: the reader checks that
 * itself. Sizes, checksums and names are checked here against what they
 * are known to be. Then each archive is cut short and has bytes changed,
 * a few hundred times over: whatever the reader makes of that, it must
 * not read or write where it should not (the test is built with ASan and
 * UBSan), and what it does hand out still has the archive's CRC-32.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rar/rrar_archive.h>
#include <encodings/crc32.h>

static int failures;

#define CHECK(cond, what) \
   do { if (!(cond)) { printf("FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, what); failures++; } } while (0)

static uint8_t *read_file(const char *dir, const char *name, size_t *len)
{
   char     path[1024];
   FILE    *f;
   uint8_t *buf;
   long     n;

   snprintf(path, sizeof(path), "%s/%s", dir, name);
   if (!(f = fopen(path, "rb")))
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (uint8_t *)malloc(n > 0 ? (size_t)n : 1);
   if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   *len = (size_t)n;
   return buf;
}

struct expect
{
   const char *name;
   uint64_t    size;
   uint32_t    crc;
   int         is_dir;
};

/* Opens the archive, unpacks everything in it that can be, and finds
 * each of @want in it. */
static void check(const char *dir, const char *file,
      const struct expect *want, unsigned num_want)
{
   size_t          len = 0;
   uint8_t        *data = read_file(dir, file, &len);
   rrar_archive_t *a = NULL;
   uint32_t        i;
   unsigned        w;

   CHECK(data != NULL, file);
   if (!data)
      return;
   CHECK(rrar_archive_open(&a, data, len) == RRAR_OK, file);
   if (a)
   {
      for (i = 0; i < rrar_archive_num_entries(a); i++)
      {
         const rrar_entry_t *e = rrar_archive_entry(a, i);
         uint8_t *out = NULL;
         size_t   out_len = 0;

         if (e->is_dir)
            continue;
         CHECK(e->supported, e->name);
         CHECK(rrar_archive_extract(a, i, &out, &out_len) == RRAR_OK, e->name);
         CHECK(out != NULL && out_len == e->size, e->name);
         if (out)
            CHECK(encoding_crc32(0, out, out_len) == e->crc, e->name);
         free(out);
      }
      for (w = 0; w < num_want; w++)
      {
         const rrar_entry_t *e = NULL;
         for (i = 0; i < rrar_archive_num_entries(a); i++)
         {
            e = rrar_archive_entry(a, i);
            if (!strcmp(e->name, want[w].name))
               break;
            e = NULL;
         }
         CHECK(e != NULL, want[w].name);
         if (e)
         {
            CHECK((e->is_dir != 0) == (want[w].is_dir != 0), want[w].name);
            if (!e->is_dir)
            {
               CHECK(e->size == want[w].size, want[w].name);
               CHECK(e->crc == want[w].crc, want[w].name);
            }
         }
      }
      CHECK(rrar_archive_entry(a, rrar_archive_num_entries(a)) == NULL, file);
   }
   rrar_archive_close(a);
   free(data);
}

/* The archive cut short and with bytes changed. */
static void mangle(const char *dir, const char *file)
{
   size_t   len = 0;
   uint8_t *data = read_file(dir, file, &len);
   uint8_t *copy;
   unsigned round;
   uint32_t seed = 12345;

   if (!data || !(copy = (uint8_t *)malloc(len)))
   {
      free(data);
      return;
   }
   for (round = 0; round < 300; round++)
   {
      rrar_archive_t *a = NULL;
      size_t          use = len;
      uint32_t        i;

      memcpy(copy, data, len);
      seed = seed * 1103515245u + 12345u;
      if (round % 3 == 0)
         use = (seed >> 8) % (len + 1);            /* cut short */
      else
      {
         unsigned k, changes = 1 + (seed >> 28);
         for (k = 0; k < changes; k++)
         {
            seed = seed * 1103515245u + 12345u;
            copy[(seed >> 8) % len] ^= (uint8_t)(1u << (seed & 7));
         }
      }
      if (rrar_archive_open(&a, copy, use) != RRAR_OK)
         continue;
      for (i = 0; i < rrar_archive_num_entries(a); i++)
      {
         const rrar_entry_t *e = rrar_archive_entry(a, i);
         uint8_t *out = NULL;
         size_t   out_len = 0;

         /* (a size that was changed into gigabytes is only refused for
          * want of memory: not what this is after) */
         if (e->is_dir || e->size > 64u * 1024 * 1024)
            continue;
         if (rrar_archive_extract(a, i, &out, &out_len) == RRAR_OK)
         {
            CHECK(out_len == e->size, file);
            CHECK(encoding_crc32(0, out, out_len) == e->crc, file);
         }
         else
            CHECK(out == NULL, file);
         free(out);
      }
      rrar_archive_close(a);
   }
   free(copy);
   free(data);
}

int main(int argc, char **argv)
{
   static const struct expect lowdist[] = {
      { "lowdist-reset.bin", 64, 0x6ff838dcu, 0 },
   };
   static const struct expect best[] = {
      { "LibarchiveAddingTest.html", 20111, 0x5e05a663u, 0 },
      { "testlink", 25, 0x11fcd3f1u, 0 },
      { "testdir/test.txt", 20, 0xbec8a242u, 0 },
      { "testdir/LibarchiveAddingTest.html", 20111, 0x5e05a663u, 0 },
      { "testdir", 0, 0, 1 },
      { "testemptydir", 0, 0, 1 },
   };
   static const struct expect filter[] = {
      { "bsdcat.exe", 204288, 0x4db10349u, 0 },
   };
   static const struct expect unicode[] = {
      { "\xe8\xa1\xa8\xe3\x81\xa0\xe3\x82\x88/\xe6\x96\xb0\xe3\x81\x97\xe3\x81\x84\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80/"
        "\xe6\x96\xb0\xe8\xa6\x8f\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88 \xe3\x83\x89\xe3\x82\xad\xe3\x83\xa5\xe3\x83\xa1\xe3\x83\xb3\xe3\x83\x88.txt",
        0, 0, 0 },
      { "\xe8\xa1\xa8\xe3\x81\xa0\xe3\x82\x88/\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab", 54, 0x5dfb8225u, 0 },
      { "abcdefghijklmnopqrs\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt", 16, 0x15b6d005u, 0 },
      { "\xe8\xa1\xa8\xe3\x81\xa0\xe3\x82\x88", 0, 0, 1 },
   };
   const char *dir;

   if (argc != 2)
   {
      fprintf(stderr, "usage: rar_test <dir>\n");
      return 2;
   }
   dir = argv[1];

   check(dir, "lowdist_reset.rar", lowdist, sizeof(lowdist) / sizeof(lowdist[0]));
   check(dir, "compress_best.rar", best, sizeof(best) / sizeof(best[0]));
   check(dir, "filter.rar", filter, sizeof(filter) / sizeof(filter[0]));
   check(dir, "unicode.rar", unicode, sizeof(unicode) / sizeof(unicode[0]));

   mangle(dir, "lowdist_reset.rar");
   mangle(dir, "compress_best.rar");
   mangle(dir, "filter.rar");
   mangle(dir, "unicode.rar");

   {
      /* what is no RAR, and what is one of a kind not read */
      rrar_archive_t *a = (rrar_archive_t *)1;
      static const uint8_t rar5[] = { 'R', 'a', 'r', '!', 0x1a, 0x07, 0x01, 0x00, 0, 0, 0, 0 };
      CHECK(rrar_archive_open(&a, (const uint8_t *)"PK\3\4....", 8) == RRAR_ERROR_DATA && !a, "zip");
      CHECK(rrar_archive_open(&a, rar5, sizeof(rar5)) == RRAR_ERROR_UNSUPPORTED && !a, "rar5");
      CHECK(rrar_archive_open(&a, rar5, 3) == RRAR_ERROR_PARAM && !a, "short");
   }

   if (failures)
   {
      printf("%d checks failed\n", failures);
      return 1;
   }
   printf("rar_test: PASS\n");
   return 0;
}
