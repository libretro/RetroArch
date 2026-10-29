/* os/ctr/ctr_linear.c, the allocator for the 3DS linear heap that the
 * GPU and DSP buffers come from.
 *
 * Allocations are placed first fit in address order and aligned as
 * asked, freed space joins its neighbours until the heap is one block
 * again, every live allocation is found by its address however many
 * there are, and pages handed back from the end of the heap leave it.
 *
 * An allocation that fits no free block fails. With the whole heap
 * allocated, no free block is left at all: a further
 * allocation must fail rather than start the heap over and hand out
 * its first address a second time, and the free-space queries must
 * answer rather than fault. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#define HEAP      0x14000000ul
#define HEAP_SIZE 0x00400000ul

u32 __linear_heap      = HEAP;
u32 __linear_heap_size = HEAP_SIZE;

u32 ctr_get_linear_free(void);
u32 ctr_get_linear_unused(void);
void ctr_linear_free_pages(u32 pages);
void ctr_linear_get_stats(void);

static int failures;
static unsigned long svc_addr, svc_size, svc_calls;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   failures++; } } while (0)

Result svcControlMemory(u32 *addr_out, u32 addr0, u32 addr1, u32 size,
      MemOp op, MemPerm perm)
{
   (void)addr_out;
   (void)addr1;
   (void)perm;
   if (op == MEMOP_FREE)
   {
      svc_addr = addr0;
      svc_size = size;
      svc_calls++;
   }
   return 0;
}

static unsigned long addr_of(const void *p)
{
   return (unsigned long)(size_t)p;
}

static unsigned long rng = 1;

static unsigned long next_rand(void)
{
   rng = (rng * 1103515245ul + 12345ul) & 0xFFFFFFFFul;
   return rng >> 8;
}

/* The whole heap is free as one block at its start */
static int heap_whole(void)
{
   void *all = linearMemAlign(__linear_heap_size, 16);
   int ok    = addr_of(all) == HEAP && linearSpaceFree() == 0;
   linearFree(all);
   return ok && linearSpaceFree() == __linear_heap_size;
}

#define STRESS 2000

static void *live[STRESS];
static unsigned long live_size[STRESS];

static int by_addr(const void *a, const void *b)
{
   unsigned long x = addr_of(*(void* const*)a);
   unsigned long y = addr_of(*(void* const*)b);
   return x < y ? -1 : x > y;
}

static void stress(void)
{
   static const size_t aligns[] = { 0, 16, 0x40, 0x80, 0x200, 0x1000 };
   unsigned long used = 0;
   void *sorted[STRESS];
   int n = 0, i, overlaps = 0, lost = 0;

   for (i = 0; i < STRESS; i++)
   {
      size_t size  = 1 + next_rand() % 3000;
      size_t align = aligns[next_rand() % 6];
      void *p      = linearMemAlign(size, align);
      if (!p)
         continue;
      if (align && addr_of(p) % align)
         lost++;
      live[n]      = p;
      live_size[n] = linearGetSize(p);
      if (live_size[n] < size)
         lost++;
      n++;
      /* Free about one in three as it goes */
      if (next_rand() % 3 == 0)
      {
         int k = (int)(next_rand() % (unsigned long)n);
         linearFree(live[k]);
         live[k]      = live[n - 1];
         live_size[k] = live_size[n - 1];
         n--;
      }
   }
   CHECK(n > 256);       /* well past the table's first size */
   CHECK(lost == 0);

   memcpy(sorted, live, n * sizeof(*sorted));
   qsort(sorted, n, sizeof(*sorted), by_addr);
   for (i = 0; i < n; i++)
   {
      used += linearGetSize(sorted[i]);
      if (i && addr_of(sorted[i - 1]) + linearGetSize(sorted[i - 1])
            > addr_of(sorted[i]))
         overlaps++;
   }
   CHECK(overlaps == 0);
   CHECK(used + linearSpaceFree() == HEAP_SIZE);

   while (n)
   {
      int k = (int)(next_rand() % (unsigned long)n);
      linearFree(live[k]);
      live[k] = live[--n];
   }
   CHECK(linearSpaceFree() == HEAP_SIZE);
   CHECK(heap_whole());
}

int main(void)
{
   void *a, *b, *c, *d, *all;

   /* Before anything is allocated */
   CHECK(linearSpaceFree() == 0);
   CHECK(ctr_get_linear_free() == 0);
   linearFree(NULL);
   linearFree((void*)(size_t)(HEAP + 0x80));
   CHECK(linearMemAlign(64, 0x30) == NULL);
   CHECK(linearMemAlign(64, 0x300) == NULL);

   /* First fit, aligned */
   a = linearAlloc(100);                     /* 0x80-aligned, 0x80 bytes */
   b = linearMemAlign(10, 0);                /* at least 16-aligned */
   c = linearMemAlign(0x100, 0x1000);
   d = linearAlloc(16);                      /* into the gap before c */
   CHECK(addr_of(a) == HEAP);
   CHECK(addr_of(b) == HEAP + 0x80);
   CHECK(addr_of(c) == HEAP + 0x1000);
   CHECK(addr_of(d) == HEAP + 0x100);
   CHECK(linearGetSize(a) == 0x80);
   CHECK(linearGetSize(b) == 0x10);
   CHECK(linearGetSize(c) == 0x1000);         /* sizes round up to the alignment */
   CHECK(linearGetSize(d) == 0x80);
   CHECK(linearGetSize((void*)(size_t)(HEAP + 0x40)) == 0);
   CHECK(linearSpaceFree() == HEAP_SIZE - 0x80 - 0x10 - 0x1000 - 0x80);
   CHECK(linearRealloc(a, 0x100) == NULL);
   CHECK(linearMemAlign(HEAP_SIZE, 16) == NULL);   /* no block that big */
   CHECK(linearMemAlign(0xFFFFFF00ul, 0x100) == NULL);

   /* Freed space joins back into one block */
   linearFree(b);
   linearFree(c);
   linearFree(a);
   linearFree(a);                            /* already free: ignored */
   linearFree(d);
   CHECK(linearSpaceFree() == HEAP_SIZE);
   CHECK(ctr_get_linear_free() == HEAP_SIZE);
   CHECK(heap_whole());

   /* The whole heap taken: nothing more to give */
   all = linearMemAlign(HEAP_SIZE, 16);
   CHECK(addr_of(all) == HEAP);
   CHECK(linearAlloc(16) == NULL);
   CHECK(linearMemAlign(16, 16) == NULL);
   CHECK(linearSpaceFree() == 0);
   CHECK(ctr_get_linear_free() == 0);
   CHECK(ctr_get_linear_unused() == 0);
   ctr_linear_get_stats();
   linearFree(all);
   CHECK(heap_whole());

   stress();

   /* Pages handed back leave the end of the heap */
   ctr_linear_free_pages(2);
   CHECK(svc_calls == 1);
   CHECK(__linear_heap_size == HEAP_SIZE - 0x2000);
   CHECK(svc_addr == HEAP + HEAP_SIZE - 0x2000 && svc_size == 0x2000);
   CHECK(ctr_get_linear_free() == HEAP_SIZE - 0x2000);
   ctr_linear_free_pages(HEAP_SIZE >> 12);   /* more than there is */
   CHECK(svc_calls == 1);
   CHECK(heap_whole());

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("ctr_linear: ok\n");
   return 0;
}
