/* r7z_archive_decode_peek() on one thread while another decodes with
 * r7z_archive_decode_step(), a folder of two at a time (BZip2 and LZMA2):
 * what it gives is the entry's bytes and never the other folder's, also
 * as the decoded folder is checked and held. Run under ThreadSanitizer. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "r7z_peek_fixtures.h"

#define ROUNDS 8

static int failures;

static void check(int ok, const char *what, const char *how)
{
   if (!ok)
   {
      printf("FAIL: %s, %s\n", what, how);
      failures++;
   }
}

/* a.txt (seed 1) and b.txt (seed 2): numbered lines, each unlike any
 * other */
static void text(uint8_t *out, size_t n, unsigned seed)
{
   static const char words[] =
      " folder slice peek decode held bytes archive thread\n";
   unsigned k   = 0;
   size_t   at  = 0;
   while (at < n)
   {
      char   line[80];
      size_t len;
      sprintf(line, "%c%07u%s", '0' + seed, k++, words);
      len = strlen(line);
      if (len > n - at)
         len = n - at;
      memcpy(out + at, line, len);
      at += len;
   }
}

struct decoder
{
   r7z_archive_t     *a;
   uint32_t           index;
   int                result;
   retro_atomic_int_t done;
};

static void decode(void *ud)
{
   struct decoder *d = (struct decoder*)ud;
   do
      d->result = r7z_archive_decode_step(d->a, d->index);
   while (d->result == R7Z_PENDING);
   retro_atomic_store_release_int(&d->done, 1);
}

static void archive(const char *what, const uint8_t *fx, size_t fx_len)
{
   uint8_t *want[2];
   size_t   size[2];
   unsigned round, k;

   size[0] = 393216;
   size[1] = 327680;
   for (k = 0; k < 2; k++)
   {
      want[k] = (uint8_t*)malloc(size[k]);
      text(want[k], size[k], k + 1);
   }

   for (round = 0; round < ROUNDS; round++)
   {
      for (k = 0; k < 2; k++)
      {
         struct decoder     d;
         sthread_t         *t;
         const r7z_entry_t *e;
         unsigned           seen = 0;
         int                wrong = 0, other = 0, last;

         if (r7z_archive_open(&d.a, fx, fx_len) != R7Z_OK)
         {
            check(0, what, "opens");
            continue;
         }
         e = r7z_archive_entry(d.a, k);
         check(e && e->size == size[k], what, "entry sizes");
         check(r7z_archive_entry(d.a, 0)->folder
               != r7z_archive_entry(d.a, 1)->folder, what, "two folders");
         d.index  = k;
         d.result = R7Z_PENDING;
         retro_atomic_int_init(&d.done, 0);
         if (!(t = sthread_create(decode, &d)))
         {
            check(0, what, "thread");
            r7z_archive_close(d.a);
            continue;
         }
         do
         {
            const uint8_t *data  = NULL;
            size_t         avail = 0;
            last = retro_atomic_load_acquire_int(&d.done);
            if (r7z_archive_decode_peek(d.a, k, &data, &avail) == R7Z_OK
                  && avail)
            {
               seen++;
               if (avail > size[k] || memcmp(data, want[k], avail))
                  wrong = 1;
            }
            /* (the folder not being decoded is not there) */
            if (r7z_archive_decode_peek(d.a, k ^ 1, &data, &avail) == R7Z_OK
                  && avail)
               other = 1;
         } while (!last);
         sthread_join(t);
         check(d.result == R7Z_OK, what, "decodes");
         check(seen > 0, what, "seen");
         check(!wrong, what, "the entry's bytes, as far as decoded");
         check(!other, what, "nothing of the other folder");
         {
            const uint8_t *data  = NULL;
            size_t         avail = 0;
            check(r7z_archive_decode_peek(d.a, k, &data, &avail) == R7Z_OK
                  && avail == size[k] && !memcmp(data, want[k], avail),
                  what, "held whole");
         }
         r7z_archive_close(d.a);
      }
   }
   free(want[0]);
   free(want[1]);
}

int main(void)
{
   archive("BZip2", r7z_peek_bzip2, sizeof(r7z_peek_bzip2));
   archive("LZMA2", r7z_peek_lzma2, sizeof(r7z_peek_lzma2));
   if (failures)
   {
      printf("r7z_peek_threads_test: %d failed\n", failures);
      return 1;
   }
   printf("r7z_peek_threads_test: OK\n");
   return 0;
}
