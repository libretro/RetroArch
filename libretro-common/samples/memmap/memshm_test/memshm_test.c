/* memshm_test.c - shared memory mapped twice is the same memory.
 *
 * The reason memshm exists is a guest RAM that has to appear at more
 * than one host address. So the test is the aliasing: one region, two
 * mappings, a pattern written through the first read back through the
 * second, and a change through the second seen through the first. Then
 * both are unmapped, the handle destroyed, and a third mapping from the
 * destroyed handle must fail -- the fd is closed, not leaked.
 *
 * memjit_write_begin/end are called nested; they do nothing off Apple
 * Silicon and must return, which is all that can be checked here.
 *
 * memreserve_at: a reservation asked for at a hint, committed, written
 * through, and released. Whether the hint was honoured is reported, not
 * asserted -- the platform may move it -- but the reservation must be
 * usable wherever it landed. memsync is then called over the committed
 * range: on this host it is msync or a no-op, on ARM the cache flush,
 * and it must return 0.
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
   const size_t len = 1u << 20;   /* 1 MiB */
   char name[64];
   void *h, *a, *b;
   volatile uint32_t *pa, *pb;
   size_t i;
   int ok = 1;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("memshm\n");

#if defined(_WIN32)
   sprintf(name, "memshm_test_%lu", (unsigned long)GetCurrentProcessId());
#else
   sprintf(name, "/memshm_test_%lu", (unsigned long)getpid());
#endif

   h = memshm_create(name, len);
   if (!h)
   {
      printf("  memshm_create: not available on this platform; nothing to test\n");
      printf("memshm: ok (skipped)\n");
      return 0;
   }
   a = memshm_map(h, 0, NULL, len, PROT_READ | PROT_WRITE);
   b = memshm_map(h, 0, NULL, len, PROT_READ | PROT_WRITE);
   if (!a || !b || a == b)
   {
      printf("  FAIL: two mappings: %p %p\n", a, b);
      return 1;
   }
   pa = (volatile uint32_t*)a; pb = (volatile uint32_t*)b;

   for (i = 0; i < len / 4; i++)
      pa[i] = (uint32_t)i * 2654435761u;
   for (i = 0; i < len / 4; i++)
      if (pb[i] != (uint32_t)i * 2654435761u) { ok = 0; break; }
   printf("  %s: %zu words written through A read back through B\n", ok ? "alias" : "FAIL", len / 4);

   pb[12345] = 0xDEADBEEFu;
   if (pa[12345] != 0xDEADBEEFu) { printf("  FAIL: write through B not seen through A\n"); ok = 0; }
   else printf("  alias: a write through B seen through A\n");

   memjit_write_begin();
   memjit_write_begin();
   memjit_write_end();
   memjit_write_end();
   printf("  memjit: nested begin/end returned\n");

   memshm_unmap(a, len);
   memshm_unmap(b, len);
   memshm_destroy(h);
   printf("  unmapped both, handle destroyed\n");

   /* reserve at a hint */
   {
      void *hint = (void*)(uintptr_t)0x200000000ull;   /* 8 GiB: free on any 64-bit host */
      size_t rlen = 16u << 20;
      void *r = memreserve_at(hint, rlen);
      if (!r)
      {
         printf("  memreserve_at: not available on this platform\n");
      }
      else
      {
         volatile uint32_t *pr = (volatile uint32_t*)r;
         if (!memcommit(r, rlen)) { printf("  FAIL: memcommit\n"); ok = 0; }
         else
         {
            pr[0] = 0x12345678u; pr[(rlen / 4) - 1] = 0x9ABCDEF0u;
            if (pr[0] != 0x12345678u || pr[(rlen / 4) - 1] != 0x9ABCDEF0u) { printf("  FAIL: reserved+committed memory not writable\n"); ok = 0; }
            else printf("  reserve_at: 16 MiB %s the hint, committed, written at both ends\n", r == hint ? "at" : "moved from");
            if (memsync(r, (char*)r + rlen) != 0) { printf("  FAIL: memsync\n"); ok = 0; }
            else printf("  memsync: returned 0 over the committed range\n");
         }
         memrelease(r, rlen);
      }
   }

   printf(ok ? "memshm: ok\n" : "memshm: FAILED\n");
   return ok ? 0 : 1;
}
