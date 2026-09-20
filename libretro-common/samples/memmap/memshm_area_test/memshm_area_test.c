/* memshm_area_test.c - one shared region mapped into a reservation at
 * several places, and unmapped from the middle.
 *
 * What the area is for: a guest RAM appearing at more than one offset
 * in a window the recompiler addresses directly. So the test is the
 * aliasing through the area -- write at one offset, read at another --
 * and then the case that is easy to get wrong on Windows, where the
 * reservation is a placeholder that Map splits and Unmap must coalesce
 * again: unmap a mapping with a live mapping on each side, and check
 * the neighbours still alias, and that the hole can be mapped again.
 *
 * Every failure mode here is silent corruption rather than a crash: a
 * placeholder split that loses a range makes the next Map fail, and a
 * coalesce that runs over its neighbour unmaps memory that is still in
 * use. The neighbours are read back after every step for that reason.
 *
 * On Windows this exercises whichever path the system supports.
 * Building the library with MEMSHM_AREA_FORCE_LEGACY selects the
 * pre-1803 slot-reservation path on a machine that has the placeholder
 * APIs, which is the only way to test it without a Windows 7 box; the
 * Makefile beside this builds a second binary that way.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <memmap.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(void)
{
   const size_t page = 65536;         /* a granularity Windows accepts */
   const size_t slot = page;
   size_t shm_len    = slot;
   size_t area_len   = slot * 8;
   char name[64];
   void *h;
   memshm_area_t *area;
   unsigned char *base;
   volatile uint32_t *a, *b, *c;
   int ok = 1;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("memshm area\n");

#if defined(_WIN32)
   snprintf(name, sizeof(name), "memshm_area_test_%lu",
         (unsigned long)GetCurrentProcessId());
#else
   snprintf(name, sizeof(name), "/memshm_area_test_%lu",
         (unsigned long)getpid());
#endif

   area = memshm_area_create(area_len);
   if (!area)
   {
      printf("  no area on this platform (Windows before 10 1803, or no mman); nothing to test\n");
      printf("memshm area: ok (skipped)\n");
      return 0;
   }
   base = memshm_area_base(area);
   printf("  reserved %u bytes at %p%s\n", (unsigned)area_len, (void*)base,
#if defined(MEMSHM_AREA_FORCE_LEGACY)
          "  [legacy path forced]"
#else
          ""
#endif
          );

   h = memshm_create(name, shm_len);
   if (!h)
   {
      printf("  FAIL: memshm_create\n");
      return 1;
   }

   /* Three mappings of the same region, at slots 1, 3 and 5: the middle
    * one is what gets unmapped, with live neighbours on both sides. */
   a = (volatile uint32_t*)memshm_area_map(area, h, 0, base + slot * 1, shm_len, PROT_READ | PROT_WRITE);
   b = (volatile uint32_t*)memshm_area_map(area, h, 0, base + slot * 3, shm_len, PROT_READ | PROT_WRITE);
   c = (volatile uint32_t*)memshm_area_map(area, h, 0, base + slot * 5, shm_len, PROT_READ | PROT_WRITE);
   if (!a || !b || !c)
   {
      printf("  FAIL: map (%p %p %p)\n", (void*)a, (void*)b, (void*)c);
      return 1;
   }
   printf("  ok: three mappings of one region at slots 1, 3 and 5\n");

   a[0] = 0x11111111u;
   {
      int good = (b[0] == 0x11111111u && c[0] == 0x11111111u);
      printf("  %s: a write at slot 1 is seen at slots 3 and 5\n", good ? "ok" : "FAIL");
      ok &= good;
   }
   c[1] = 0x22222222u;
   {
      int good = (a[1] == 0x22222222u && b[1] == 0x22222222u);
      printf("  %s: and a write at slot 5 is seen at slots 1 and 3\n", good ? "ok" : "FAIL");
      ok &= good;
   }

   /* The middle one goes: its placeholder must be restored and coalesced
    * with the free ranges either side, without touching 1 or 5. */
   if (!memshm_area_unmap(area, (void*)b, shm_len))
   {
      printf("  FAIL: unmap of the middle mapping\n");
      ok = 0;
   }
   else
   {
      int good = (a[0] == 0x11111111u && c[0] == 0x11111111u && a[1] == 0x22222222u && c[1] == 0x22222222u);
      printf("  %s: after unmapping the middle, the neighbours still alias\n", good ? "ok" : "FAIL");
      ok &= good;
   }

   /* And the hole is usable again -- the case a lost placeholder range
    * would fail. */
   {
      volatile uint32_t *d = (volatile uint32_t*)memshm_area_map(area, h, 0, base + slot * 3, shm_len, PROT_READ | PROT_WRITE);
      int good = (d != NULL) && (d[0] == 0x11111111u);
      printf("  %s: the freed slot maps again and aliases\n", good ? "ok" : "FAIL");
      ok &= good;
      if (d)
         memshm_area_unmap(area, (void*)d, shm_len);
   }

   /* Addresses and lengths the area does not cover. The contract says
    * @at is within the reservation, and on POSIX the map is MAP_FIXED,
    * which replaces whatever is already mapped over the range -- so an
    * unchecked address does not fail, it destroys an unrelated mapping.
    * Every one of these has to be refused at the API. */
   {
      struct { const char *what; unsigned char *at; size_t len; } bad[] = {
         { "an address below the base",            NULL, 0 },
         { "the first address past the end",       NULL, 0 },
         { "a length crossing the end",            NULL, 0 },
         { "a length that would wrap",             NULL, 0 }
      };
      unsigned i;
      bad[0].at  = base - page;        bad[0].len = shm_len;
      bad[1].at  = base + area_len;    bad[1].len = shm_len;
      bad[2].at  = base + area_len - page; bad[2].len = page * 2;
      bad[3].at  = base;               bad[3].len = (size_t)-1;
      for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
      {
         void *r  = memshm_area_map(area, h, 0, bad[i].at, bad[i].len,
               PROT_READ | PROT_WRITE);
         int good = (r == NULL);
         printf("  %s: map refuses %s\n", good ? "ok" : "FAIL", bad[i].what);
         ok &= good;
      }
      /* And unmap, which trusts the same two arguments. */
      {
         int good = !memshm_area_unmap(area, base - page, shm_len);
         printf("  %s: unmap refuses an address below the base\n",
               good ? "ok" : "FAIL");
         ok &= good;
      }
      {
         int good = !memshm_area_unmap(area, base + slot * 1, area_len * 2);
         printf("  %s: unmap refuses a length past the end\n",
               good ? "ok" : "FAIL");
         ok &= good;
      }
   }

#if defined(HAVE_MMAN) && !defined(_WIN32)
   /* The same thing observed rather than inferred: a mapping made
    * outside the area, handed to map as @at. Unchecked, MAP_FIXED
    * replaces it and the byte pattern below is gone. */
   {
      unsigned char *victim = (unsigned char*)mmap(NULL, page,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (victim == MAP_FAILED)
         printf("  ok: (no anonymous page for the outside-the-area check)\n");
      else
      {
         void *r;
         int good;
         memset(victim, 0xAB, page);
         r    = memshm_area_map(area, h, 0, victim, page,
               PROT_READ | PROT_WRITE);
         good = (r == NULL) && (victim[0] == 0xAB);
         printf("  %s: a mapping outside the area is left alone\n",
               good ? "ok" : "FAIL");
         ok &= good;
         munmap(victim, page);
      }
   }
#endif

   /* A name longer than Darwin's 31-character shm limit must still
    * work: Linux takes it as given, Darwin shortens it from the front.
    * A caller that works on one platform should work on the other. */
   {
      char longname[128];
      void *h2;
      memset(longname, 'x', sizeof(longname));
#if defined(_WIN32)
      snprintf(longname, sizeof(longname),
            "memshm_area_test_a_very_long_name_indeed_%lu",
            (unsigned long)GetCurrentProcessId());
#else
      snprintf(longname, sizeof(longname),
            "/memshm_area_test_a_very_long_name_indeed_%lu",
            (unsigned long)getpid());
#endif
      h2 = memshm_create(longname, shm_len);
      printf("  %s: a %u-character region name is accepted\n", h2 ? "ok" : "FAIL", (unsigned)strlen(longname));
      ok &= (h2 != NULL);
      if (h2)
         memshm_destroy(h2);
   }

   memshm_area_unmap(area, (void*)a, shm_len);
   memshm_area_unmap(area, (void*)c, shm_len);

   /* Nothing is mapped now, so one more unmap has nothing to answer to.
    * Taken at face value it wraps the mapping count, and the area's own
    * teardown is what reads it. (An unbalanced unmap while other
    * mappings are live is not distinguishable here -- the count is the
    * only thing to check it against.) */
   {
      int good = !memshm_area_unmap(area, base + slot * 1, shm_len);
      printf("  %s: an unmap with nothing mapped is refused\n",
            good ? "ok" : "FAIL");
      ok &= good;
   }

   memshm_destroy(h);
   memshm_area_free(area);
   printf(ok ? "memshm area: ok\n" : "memshm area: FAILED\n");
   return ok ? 0 : 1;
}
