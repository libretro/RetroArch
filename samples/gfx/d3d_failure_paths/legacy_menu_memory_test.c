#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <memalign.h>

static unsigned allocations, releases;
static bool fail_alloc;
static void *allocate(size_t alignment, size_t size)
{
   void *p = fail_alloc ? NULL : memalign_alloc(alignment, size);
   if (p) allocations++;
   return p;
}
static void release(void *p)
{ if (p) releases++; memalign_free(p); }
#define memalign_alloc allocate
#define memalign_free release
#include "gfx/common/cpu_menu_memory.h"
#undef memalign_alloc
#undef memalign_free

typedef struct { unsigned freed; } gpu_slots_t;
typedef struct
{
   gfx_cpu_menu_memory_t menu_mem;
   gpu_slots_t sw_fb;
} d3d9_video_t;
typedef d3d9_video_t d3d8_video_t;
typedef d3d9_video_t sdl_video_t;
/* Existing GPU slot IDs are small integers; menu IDs must never release one. */
static void d3d9_frame_mem_free(gpu_slots_t *gpu, uintptr_t id)
{ if (id && id <= 4) gpu->freed++; }
#include "callbacks.inc"

int main(void)
{
   void *(*allocators[])(void*, size_t, uintptr_t*) = {
      sdl_menu_mem_alloc, d3d8_menu_mem_alloc,
      d3d9_hlsl_menu_mem_alloc, d3d9_cg_menu_mem_alloc,
      d3d9_hlsl_menu_mem_alloc, d3d9_cg_menu_mem_alloc
   };
   void (*freers[])(void*, uintptr_t) = {
      sdl_menu_mem_free, d3d8_menu_mem_free,
      d3d9_hlsl_frame_mem_free, d3d9_cg_frame_mem_free,
      d3d9_hlsl_menu_mem_free, d3d9_cg_menu_mem_free
   };
   unsigned backend;
   for (backend = 0; backend < sizeof(allocators) / sizeof(allocators[0]); backend++)
   {
      d3d9_video_t driver;
      uintptr_t ids[GFX_CPU_MENU_SLOTS], extra;
      uint8_t *pixels[GFX_CPU_MENU_SLOTS];
      unsigned i, frame;
      memset(&driver, 0, sizeof(driver));
      assert(!allocators[backend](NULL, 128, &extra) && !extra);
      assert(!allocators[backend](&driver, 0, &extra) && !extra);
      assert(!allocators[backend](&driver, 128, NULL));
      fail_alloc = true;
      assert(!allocators[backend](&driver, 128, &extra) && !extra);
      fail_alloc = false;
      for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
      {
         pixels[i] = (uint8_t*)allocators[backend](&driver, 128, &ids[i]);
         assert(pixels[i] && !((uintptr_t)pixels[i] & 63));
         assert(ids[i] == (uintptr_t)pixels[i] && ids[i] > 4);
      }
      assert(!allocators[backend](&driver, 128, &extra) && !extra);
      for (frame = 0; frame < 48; frame++)
      {
         unsigned slot = frame % GFX_CPU_MENU_SLOTS;
         assert(gfx_cpu_menu_memory_ready(&driver, ids[slot]));
         memset(pixels[slot], (int)frame, 128);
         assert(driver.menu_mem.slots[slot] == pixels[slot]);
         assert(pixels[slot][0] == frame && pixels[slot][127] == frame);
      }
      for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
      {
         freers[backend](&driver, ids[i]);
         assert(!driver.menu_mem.slots[i]);
         freers[backend](&driver, ids[i]);
      }
      assert(!driver.sw_fb.freed && allocations == releases);
      if (backend == 2 || backend == 3)
      {
         freers[backend](&driver, 2);
         assert(driver.sw_fb.freed == 1);
      }
      for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
         assert(allocators[backend](&driver, 257, &ids[i]));
      /* Driver teardown also releases loans not freed by the wrapper. */
      gfx_cpu_menu_memory_destroy(&driver.menu_mem);
      gfx_cpu_menu_memory_destroy(&driver.menu_mem);
      for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
         assert(!driver.menu_mem.slots[i]);
      assert(allocations == releases);
   }
   puts("PASS legacy menu loans: six callback routes, bounded storage, GPU ID isolation and teardown");
   return 0;
}
