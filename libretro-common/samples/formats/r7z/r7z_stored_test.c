/* A stored 7z archive (r7z_stored_fixtures.h, two members each a
 * folder of its own) opened from memory: its folders are held where
 * they lie, so a member borrowed is the archive's own octets, and one
 * extracted or detached is a copy of them that is the caller's, with
 * the archive still whole after. Through positioned reads, every
 * member is still right. Done one step at a time and in one call. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#include "r7z_stored_fixtures.h"

static int failures;

static void check(int ok, const char *what, unsigned i)
{
   if (!ok)
   {
      printf("FAIL: %s, member %u\n", what, i);
      failures++;
   }
}

static void member(unsigned i, uint8_t *d, size_t *n)
{
   size_t k;
   *n = i == 0 ? 1500 : 900;
   for (k = 0; k < *n; k++)
      d[k] = (uint8_t)(i == 0 ? ((k * 13) ^ (k >> 3)) : (k * 7 + (k >> 9)));
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

static int inside(const uint8_t *p)
{
   return p >= fx_stored && p < fx_stored + sizeof(fx_stored);
}

static void run(int from_memory, int stepped)
{
   r7z_archive_t *a = NULL;
   struct mem     m;
   unsigned       i, pass;

   m.d = fx_stored;
   m.n = sizeof(fx_stored);
   if ((from_memory ? r7z_archive_open(&a, fx_stored, sizeof(fx_stored))
                    : r7z_archive_open_read(&a, sizeof(fx_stored), read_at, &m))
         != R7Z_OK)
   {
      check(0, "open", 0);
      return;
   }
   check(r7z_archive_num_entries(a) == 2, "entries", 0);
   /* twice over: the archive whole after every member has been taken */
   for (pass = 0; pass < 2; pass++)
      for (i = 0; i < 2; i++)
      {
         uint8_t        want[1500];
         const uint8_t *lent = NULL;
         uint8_t       *got  = NULL;
         size_t         n, len = 0;
         int            r;

         member(i, want, &n);
         if (stepped)
            while ((r = r7z_archive_decode_step(a, i)) == R7Z_PENDING) { }
         r = r7z_archive_entry_borrow(a, i, &lent, &len);
         check(r == R7Z_OK && len == n && !memcmp(lent, want, n), "borrowed", i);
         check(r != R7Z_OK || inside(lent) == from_memory,
               from_memory ? "borrowed where it lies" : "borrowed from a read", i);

         while ((r = r7z_archive_extract_slice(a, i, &got, &len)) == R7Z_PENDING) { }
         check(r == R7Z_OK && len == n && !memcmp(got, want, n) && !inside(got),
               "extracted", i);
         free(got);

         got = NULL;
         r   = r7z_archive_extract_detach(a, i, &got, &len);
         check(r == R7Z_OK && len == n && !memcmp(got, want, n) && !inside(got),
               "detached", i);
         free(got);
      }
   r7z_archive_close(a);
}

int main(void)
{
   run(1, 0);
   run(1, 1);
   run(0, 0);
   run(0, 1);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("r7z_stored_test: stored folders held where they lie, members right every way\n");
   return 0;
}
