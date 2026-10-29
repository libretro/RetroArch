/* The linear heap allocator, in place of libctru's, so that the end of
 * the linear heap can be handed back to the system at runtime
 * (ctr_linear_free_pages).
 *
 * Free space is an address-ordered list of blocks, allocated first fit,
 * the way libctru's MemPool does, so buffers land where they always
 * have. Live allocations are found by address in an open-addressing
 * table. List nodes come from slabs, so a walk of the list stays within
 * a few cache lines, and a node a split or a free gives up is kept for
 * the next one rather than going back to malloc. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <3ds.h>

#include <boolean.h>
#include <retro_inline.h>

#define LINEAR_LIVE_MIN 64u
#define LINEAR_SLAB     64u   /* nodes per slab */
#define LINEAR_LINE     32u   /* the ARM11's L1 line */

#if defined(__GNUC__)
#define LINEAR_COLD __attribute__((noinline, cold))
#else
#define LINEAR_COLD
#endif

/* What a search reads - next, base, size - leads */
typedef struct linear_block
{
   struct linear_block *next;
   u32                  base;
   u32                  size;
   struct linear_block *prev;
} linear_block_t;


/* addr 0 marks a free slot; the linear heap is never at 0 */
typedef struct
{
   u32 addr;
   u32 size;
} linear_live_t;

extern u32 __linear_heap, __linear_heap_size;

/* The free blocks form a ring through end, a block at 0xFFFFFFFF that
 * every allocation fits (a padded size and its alignment waste come to
 * less than its 0xFFFFFFFF bytes) and that lies past every address, so
 * neither a search nor a free needs to test for the end of the list: a
 * search stops at end at the latest, a free goes in before it at the
 * latest, and no chunk ends where it starts to join it. end.next is the
 * first block and end.prev the last, end itself when there is none. */
static struct
{
   linear_block_t  end;
   linear_block_t *spare;   /* nodes kept for reuse, chained by next */
   void           *slabs;   /* each slab's first word is the next */
   linear_live_t  *live;
   u32             live_mask;
   u32             live_count;
   u32             maxaddr;
   bool            ready;
} linear;

#define LINEAR_END (&linear.end)

u32 ctr_get_linear_free(void);
u32 ctr_get_linear_unused(void);
void ctr_linear_free_pages(u32 pages);
void ctr_linear_get_stats(void);

/* ---- free list ---- */

/* A slab of nodes, starting on a cache line so none straddles two,
 * onto the spare list; the slabs last as long as the heap does */
static LINEAR_COLD linear_block_t *linear_slab_new(void)
{
   u32 i;
   linear_block_t *node;
   void **slab = (void**)malloc(sizeof(void*) + LINEAR_LINE - 1
         + LINEAR_SLAB * sizeof(linear_block_t));
   if (!slab)
      return NULL;
   *slab        = linear.slabs;
   linear.slabs = slab;
   node         = (linear_block_t*)(((uintptr_t)(slab + 1) + LINEAR_LINE - 1)
         & ~(uintptr_t)(LINEAR_LINE - 1));
   for (i = 0; i < LINEAR_SLAB - 1; i++)
      node[i].next = &node[i + 1];
   node[LINEAR_SLAB - 1].next = NULL;
   return node;
}

static INLINE linear_block_t *linear_block_new(u32 base, u32 size)
{
   linear_block_t *b = linear.spare;
   if (!b && !(b = linear_slab_new()))
      return NULL;
   linear.spare = b->next;
   b->base      = base;
   b->size      = size;
   return b;
}

static INLINE void linear_block_link(linear_block_t *n, linear_block_t *before)
{
   n->next            = before;
   n->prev            = before->prev;
   before->prev->next = n;
   before->prev       = n;
}

static INLINE void linear_block_del(linear_block_t *b)
{
   b->prev->next = b->next;
   b->next->prev = b->prev;
   b->next       = linear.spare;
   linear.spare  = b;
}

/* Takes size bytes aligned to mask + 1 from the first block they fit
 * in; 0 when none has room. size is a multiple of mask + 1, so the
 * alignment waste added to it cannot overflow. *got is what the
 * allocation holds, which is more than size when no node was to be had
 * for the tail. */
static u32 linear_take(u32 size, u32 mask, u32 *got)
{
   linear_block_t *b = linear.end.next;
   u32 waste, addr, rest;

   while ((waste = (0u - b->base) & mask) + size > b->size)
      b = b->next;
   if (b == LINEAR_END)
      return 0;

   addr = b->base + waste;
   rest = b->size - waste - size;
   *got = size;

   if (!waste)
   {
      b->base += size;
      b->size  = rest;
      if (!rest)
         linear_block_del(b);
   }
   else
   {
      b->size = waste;
      if (rest)
      {
         /* The tail goes back on the list after b */
         linear_block_t *n = linear_block_new(addr + size, rest);
         if (n)
            linear_block_link(n, b->next);
         else
            *got += rest; /* no choice but to waste the space */
      }
   }
   return addr;
}

static void linear_give(u32 addr, u32 size)
{
   linear_block_t *b, *n;

   for (b = linear.end.next; ; b = b->next)
   {
      if (b->base > addr)
      {
         /* Joins the block it ends at, or goes in before it - before
          * end when it lies past the last block */
         if (addr + size == b->base)
         {
            b->base  = addr;
            b->size += size;
         }
         else if ((n = linear_block_new(addr, size)))
            linear_block_link(n, b);
         return;
      }

      if (b->base + b->size == addr)
      {
         /* Joins the block before it, and then those it now reaches */
         u32 end  = addr + size;
         b->size += size;
         while ((n = b->next)->base == end)
         {
            b->size += n->size;
            end     += n->size;
            linear_block_del(n);
         }
         return;
      }
   }
}

/* ---- live allocations ---- */

static INLINE u32 linear_live_home(u32 addr, u32 mask)
{
   u32 h = (addr >> 4) * 0x9E3779B1u;
   return (h ^ (h >> 15)) & mask;
}

/* The slot holding addr, or the free one it would go in */
static INLINE u32 linear_live_slot(const linear_live_t *live, u32 mask,
      u32 addr)
{
   u32 i = linear_live_home(addr, mask);
   while (live[i].addr && live[i].addr != addr)
      i = (i + 1) & mask;
   return i;
}

static LINEAR_COLD bool linear_live_grow(void)
{
   u32 i;
   u32 old_mask = linear.live_mask;
   u32 mask     = old_mask * 2 + 1;
   linear_live_t *old  = linear.live;
   linear_live_t *live = (linear_live_t*)calloc(mask + 1, sizeof(*live));

   if (!live)
      return false;
   for (i = 0; i <= old_mask; i++)
      if (old[i].addr)
         live[linear_live_slot(live, mask, old[i].addr)] = old[i];
   free(old);
   linear.live      = live;
   linear.live_mask = mask;
   return true;
}

/* An address already live keeps what it holds */
static bool linear_live_add(u32 addr, u32 size)
{
   u32 i;
   if ((linear.live_count + 1) * 2 > linear.live_mask + 1
         && !linear_live_grow())
      return false;
   i = linear_live_slot(linear.live, linear.live_mask, addr);
   if (!linear.live[i].addr)
   {
      linear.live[i].addr = addr;
      linear.live[i].size = size;
      linear.live_count++;
   }
   return true;
}

static void linear_live_del(u32 i)
{
   linear_live_t *live = linear.live;
   u32 mask            = linear.live_mask;
   u32 j               = i;

   /* Pulls back each entry after it that would no longer be found */
   for (;;)
   {
      u32 k;
      j = (j + 1) & mask;
      if (!live[j].addr)
         break;
      k = linear_live_home(live[j].addr, mask);
      if (i <= j ? (i < k && k <= j) : (i < k || k <= j))
         continue;
      live[i] = live[j];
      i       = j;
   }
   live[i].addr = 0;
   linear.live_count--;
}

/* ---- libctru's linear heap interface ---- */

static LINEAR_COLD bool linear_init(void)
{
   linear_block_t *b;

   if (!(linear.live = (linear_live_t*)calloc(LINEAR_LIVE_MIN,
               sizeof(*linear.live))))
      return false;
   if (!(b = linear_block_new(__linear_heap, __linear_heap_size)))
   {
      free(linear.live);
      linear.live = NULL;
      return false;
   }
   linear.end.base  = 0xFFFFFFFFu;
   linear.end.size  = 0xFFFFFFFFu;
   linear.end.next  = LINEAR_END;
   linear.end.prev  = LINEAR_END;
   linear_block_link(b, LINEAR_END);
   linear.live_mask = LINEAR_LIVE_MIN - 1;
   linear.maxaddr   = __linear_heap;
   linear.ready     = true;
   return true;
}

void *linearMemAlign(size_t len, size_t alignment)
{
   u32 mask, size, got, addr;

   if (alignment < 16)
      alignment = 16;
   else if ((alignment & (alignment - 1)) || alignment > 0x80000000u)
      return NULL;
   mask = (u32)alignment - 1;

   if ((size_t)(u32)len != len)
      return NULL;
   size = (u32)len;
   if (size & mask)
   {
      if (size > 0xFFFFFFFFu - mask)
         return NULL;
      size = (size + mask) & ~mask;
   }

   if (!linear.ready && !linear_init())
      return NULL;

   if (!(addr = linear_take(size, mask, &got)))
      return NULL;

   if (!linear_live_add(addr, got))
   {
      linear_give(addr, got);
      return NULL;
   }

   if (linear.end.prev == LINEAR_END)
      linear.maxaddr = __linear_heap + __linear_heap_size;
   else if (linear.maxaddr < linear.end.prev->base)
      linear.maxaddr = linear.end.prev->base;

   return (void*)(uintptr_t)addr;
}

void *linearAlloc(size_t len)
{
   return linearMemAlign(len, 0x80);
}

void *linearRealloc(void *mem, size_t len)
{
   (void)mem;
   (void)len;
   return NULL;
}

size_t linearGetSize(void *mem)
{
   u32 i, addr = (u32)(uintptr_t)mem;
   if (!linear.ready || !addr)
      return 0;
   i = linear_live_slot(linear.live, linear.live_mask, addr);
   return linear.live[i].addr ? linear.live[i].size : 0;
}

void linearFree(void *mem)
{
   u32 i, addr = (u32)(uintptr_t)mem;
   linear_live_t chunk;

   if (!linear.ready || !addr)
      return;
   i = linear_live_slot(linear.live, linear.live_mask, addr);
   if (!linear.live[i].addr)
      return;
   chunk = linear.live[i];
   linear_live_del(i);
   linear_give(chunk.addr, chunk.size);
}

u32 linearSpaceFree(void)
{
   u32 total = 0;
   const linear_block_t *b;
   if (!linear.ready)
      return 0;
   for (b = linear.end.next; b != LINEAR_END; b = b->next)
      total += b->size;
   return total;
}

/* ---- the end of the heap ---- */

u32 ctr_get_linear_free(void)
{
   const linear_block_t *b = linear.end.prev;
   if (!linear.ready || b == LINEAR_END || b->base + b->size != __linear_heap + __linear_heap_size)
      return 0;
   return b->size;
}

u32 ctr_get_linear_unused(void)
{
   return __linear_heap + __linear_heap_size - linear.maxaddr;
}

void ctr_linear_free_pages(u32 pages)
{
   u32 tmp, size;
   linear_block_t *b = linear.end.prev;

   if (!linear.ready || b == LINEAR_END || b->base + b->size != __linear_heap + __linear_heap_size)
      return;

   size = pages << 12;
   if (size > b->size)
      return;

   b->size           -= size;
   __linear_heap_size -= size;
   svcControlMemory(&tmp, __linear_heap + __linear_heap_size, 0x0, size,
         MEMOP_FREE, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));
}

void ctr_linear_get_stats(void)
{
   const linear_block_t *b = linear.end.prev;
   printf("last:\n");
   if (linear.ready && b != LINEAR_END)
      printf("0x%08X --> 0x%08X (0x%08X) \n", (unsigned)b->base,
            (unsigned)(b->base + b->size), (unsigned)b->size);
   printf("free: 0x%08X unused: 0x%08X \n",
         (unsigned)ctr_get_linear_unused(), (unsigned)ctr_get_linear_free());
}
