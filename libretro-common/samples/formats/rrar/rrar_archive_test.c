/* libretro-common's RAR reader (formats/rar/rrar_archive.c) on real
 * archives.
 *
 *   rrar_archive_test [dir with the archives]
 *
 * Without an argument the archives are looked for in "archives", next to
 * this file, which is where they are when the test is run from here.
 *
 * Every member that the reader says it can unpack has to come out, which
 * means with the CRC-32 the archive gives for it: the reader checks that
 * itself. Sizes, checksums and names are checked here against what they
 * are known to be - for RAR 2.9 archives and for RAR 5 ones. Then each
 * archive is cut short and has bytes changed,
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
   int         unsupported;   /* the reader is to say it cannot unpack it */
};

/* Opens the archive, unpacks everything in it that can be, and finds
 * each of @want in it. */
/* The archive as a file that is read, not one that is in memory. */
typedef struct
{
   const uint8_t *data;
   size_t         len;
} file_t;

static int64_t read_cb(void *ud, uint64_t off, void *dst, size_t len)
{
   const file_t *f = (const file_t*)ud;

   if (off > f->len)
      return -1;
   if (len > f->len - (size_t)off)
      len = f->len - (size_t)off;
   memcpy(dst, f->data + (size_t)off, len);
   return (int64_t)len;
}

/* A watcher of a member being decoded: what it is told is final has to
 * be the member's bytes then and there, and only ever more of them. */
typedef struct
{
   const uint8_t *want;      /* the member, extracted the plain way */
   const uint8_t *dst;       /* where it is being decoded to */
   size_t         size;
   size_t         done;
   unsigned       calls;
   unsigned       stop_after;   /* 0: never */
   int            bad;
} watched_t;

static int watch_cb(void *ud, size_t done)
{
   watched_t *w = (watched_t*)ud;

   w->calls++;
   if (done < w->done || done > w->size)
      w->bad = 1;
   else if (memcmp(w->dst + w->done, w->want + w->done, done - w->done))
      w->bad = 1;
   else
      w->done = done;
   return w->stop_after && w->calls >= w->stop_after;
}

static void check_watched(rrar_archive_t *a, uint32_t i, const uint8_t *want,
      size_t size, const char *name)
{
   uint8_t     *dst = (uint8_t *)malloc(size ? size : 1);
   watched_t    w;
   rrar_watch_t watch;

   if (!dst)
      return;
   memset(&w, 0, sizeof(w));
   memset(dst, 0xA5, size);
   w.want         = want;
   w.dst          = dst;
   w.size         = size;
   watch.progress = watch_cb;
   watch.ud       = &w;
   CHECK(rrar_archive_extract_to(a, i, dst, size, &watch) == RRAR_OK, name);
   CHECK(!w.bad, name);
   CHECK(w.done == size && w.calls > 0, name);
   CHECK(!memcmp(dst, want, size), name);
   /* too little room is refused, and without a watcher it is the same */
   if (size)
      CHECK(rrar_archive_extract_to(a, i, dst, size - 1, NULL) == RRAR_ERROR_PARAM, name);
   memset(dst, 0xA5, size);
   CHECK(rrar_archive_extract_to(a, i, dst, size, NULL) == RRAR_OK
         && !memcmp(dst, want, size), name);
   /* stopped by the watcher at its first call: a member long enough to
    * have more than one says so */
   if (w.calls > 1)
   {
      memset(&w, 0, sizeof(w));
      w.want       = want;
      w.dst        = dst;
      w.size       = size;
      w.stop_after = 1;
      CHECK(rrar_archive_extract_to(a, i, dst, size, &watch) == RRAR_ERROR_CANCELLED, name);
      CHECK(!w.bad && w.calls == 1, name);
   }
   free(dst);
}

static void check_mode(const char *dir, const char *file,
      const struct expect *want, unsigned num_want, int through_reads)
{
   size_t          len = 0;
   uint8_t        *data = read_file(dir, file, &len);
   rrar_archive_t *a = NULL;
   file_t          f;
   uint32_t        i;
   unsigned        w;

   CHECK(data != NULL, file);
   if (!data)
      return;
   f.data = data;
   f.len  = len;
   if (through_reads)
      CHECK(rrar_archive_open_read(&a, len, read_cb, &f) == RRAR_OK, file);
   else
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
         if (!e->supported)
         {
            /* said not to be, and refused as that */
            CHECK(rrar_archive_extract(a, i, &out, &out_len) == RRAR_ERROR_UNSUPPORTED
                  && out == NULL, e->name);
            continue;
         }
         CHECK(rrar_archive_extract(a, i, &out, &out_len) == RRAR_OK, e->name);
         CHECK(out != NULL && out_len == e->size, e->name);
         if (out && e->has_crc)
            CHECK(encoding_crc32(0, out, out_len) == e->crc, e->name);
         if (out)
            check_watched(a, i, out, out_len, e->name);
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
               if (e->has_crc)
                  CHECK(e->crc == want[w].crc, want[w].name);
               else if (e->supported)
               {
                  /* no CRC-32 in the archive: the known one, of what
                   * comes out */
                  uint8_t *out = NULL;
                  size_t   out_len = 0;
                  CHECK(rrar_archive_extract(a, i, &out, &out_len) == RRAR_OK, want[w].name);
                  if (out)
                     CHECK(encoding_crc32(0, out, out_len) == want[w].crc, want[w].name);
                  free(out);
               }
               CHECK((e->supported != 0) == (want[w].unsupported == 0), want[w].name);
            }
         }
      }
      CHECK(rrar_archive_entry(a, rrar_archive_num_entries(a)) == NULL, file);
   }
   rrar_archive_close(a);
   free(data);
}

/* In memory, and read through the callback. */
static void check(const char *dir, const char *file,
      const struct expect *want, unsigned num_want)
{
   check_mode(dir, file, want, num_want, 0);
   check_mode(dir, file, want, num_want, 1);
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
      {
         /* Whatever it is taken for in memory, it is taken for the same
          * when it is read: the same verdict, the same members, the same
          * bytes or the same refusal for each. */
         rrar_archive_t *b = NULL;
         file_t          f;
         int             res_a, res_b;

         f.data = copy;
         f.len  = use;
         res_a  = rrar_archive_open(&a, copy, use);
         res_b  = rrar_archive_open_read(&b, use, read_cb, &f);
         CHECK(res_a == res_b, file);
         CHECK((a != NULL) == (b != NULL), file);
         if (a && b)
         {
            CHECK(rrar_archive_num_entries(a) == rrar_archive_num_entries(b), file);
            for (i = 0; i < rrar_archive_num_entries(a)
                  && i < rrar_archive_num_entries(b); i++)
            {
               const rrar_entry_t *ea = rrar_archive_entry(a, i);
               const rrar_entry_t *eb = rrar_archive_entry(b, i);
               uint8_t *oa = NULL, *ob = NULL;
               size_t   la = 0, lb = 0;
               int      xa, xb;

               CHECK(!strcmp(ea->name, eb->name) && ea->size == eb->size
                     && ea->supported == eb->supported && ea->is_dir == eb->is_dir, file);
               if (ea->is_dir || ea->size > 64u * 1024 * 1024)
                  continue;
               xa = rrar_archive_extract(a, i, &oa, &la);
               xb = rrar_archive_extract(b, i, &ob, &lb);
               CHECK(xa == xb && la == lb, file);
               if (oa && ob && la == lb)
                  CHECK(!memcmp(oa, ob, la), file);
               free(oa);
               free(ob);
            }
         }
         rrar_archive_close(b);
         if (res_a != RRAR_OK)
         {
            rrar_archive_close(a);
            continue;
         }
      }
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
            /* (an archive with no CRC-32s has nothing to hold it to) */
            if (e->has_crc)
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
   static const struct expect conversion[] = {
      /* PPMd blocks between LZ blocks, the reading handed over each way */
      { "ppmd_lzss_conversion_test.txt", 241647978, 0xf72b4477u, 0 },
   };
   /* RAR 5 */
   static const struct expect five_stored[] = {
      { "helloworld.txt", 29, 0x95a043b4u, 0, 0 },
   };
   static const struct expect five_compressed[] = {
      { "test.bin", 1200, 0x7cca70cdu, 0, 0 },
   };
   static const struct expect five_arm[] = {
      { "elf-Linux-ARMv7-ls", 90808, 0x886f91ebu, 0, 0 },
   };
   static const struct expect five_solid[] = {
      /* the first member of a solid archive stands by itself */
      { "test1.bin", 4096, 0x7e13b2c6u, 0, 0 },
      { "test2.bin", 4096, 0xf166afcbu, 0, 1 },
      { "test4.bin", 4096, 0x10c43ed4u, 0, 1 },
   };
   static const struct expect five_blake2[] = {
      { "cebula.txt", 814, 0x7e5ec49eu, 0, 0 },
   };
   const char *dir;

   if (argc > 2)
   {
      fprintf(stderr, "usage: rrar_archive_test [dir]\n");
      return 2;
   }
   dir = argc == 2 ? argv[1] : "archives";

   check(dir, "lowdist_reset.rar", lowdist, sizeof(lowdist) / sizeof(lowdist[0]));
   check(dir, "compress_best.rar", best, sizeof(best) / sizeof(best[0]));
   check(dir, "filter.rar", filter, sizeof(filter) / sizeof(filter[0]));
   check(dir, "unicode.rar", unicode, sizeof(unicode) / sizeof(unicode[0]));
   check(dir, "ppmd_lzss_conversion.rar", conversion, sizeof(conversion) / sizeof(conversion[0]));

   check(dir, "rar5_stored.rar", five_stored, sizeof(five_stored) / sizeof(five_stored[0]));
   check(dir, "rar5_compressed.rar", five_compressed, sizeof(five_compressed) / sizeof(five_compressed[0]));
   check(dir, "rar5_arm.rar", five_arm, sizeof(five_arm) / sizeof(five_arm[0]));
   check(dir, "rar5_multiple_files_solid.rar", five_solid, sizeof(five_solid) / sizeof(five_solid[0]));
   check(dir, "rar5_blake2.rar", five_blake2, sizeof(five_blake2) / sizeof(five_blake2[0]));

   mangle(dir, "rar5_stored.rar");
   mangle(dir, "rar5_compressed.rar");
   mangle(dir, "rar5_arm.rar");
   mangle(dir, "rar5_multiple_files_solid.rar");
   mangle(dir, "rar5_blake2.rar");
   mangle(dir, "lowdist_reset.rar");
   mangle(dir, "compress_best.rar");
   mangle(dir, "filter.rar");
   mangle(dir, "unicode.rar");

   {
      /* what is no RAR, and what is one of a kind not read */
      rrar_archive_t *a = (rrar_archive_t *)1;
      static const uint8_t rar6[] = { 'R', 'a', 'r', '!', 0x1a, 0x07, 0x02, 0x00, 0, 0, 0, 0 };
      static const uint8_t rar5[] = { 'R', 'a', 'r', '!', 0x1a, 0x07, 0x01, 0x00, 0, 0, 0, 0 };
      CHECK(rrar_archive_open(&a, (const uint8_t *)"PK\3\4....", 8) == RRAR_ERROR_DATA && !a, "zip");
      CHECK(rrar_archive_open(&a, rar6, sizeof(rar6)) == RRAR_ERROR_UNSUPPORTED && !a, "a later container");
      CHECK(rrar_archive_open(&a, rar5, sizeof(rar5)) == RRAR_ERROR_DATA && !a, "rar5 with no headers");
      CHECK(rrar_archive_open(&a, rar5, 3) == RRAR_ERROR_PARAM && !a, "short");
   }

   if (failures)
   {
      printf("%d checks failed\n", failures);
      return 1;
   }
   printf("rrar_archive_test: PASS\n");
   return 0;
}
