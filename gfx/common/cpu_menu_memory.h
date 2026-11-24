/* Cached menu sources consumed synchronously by a backend upload. */
#ifndef GFX_CPU_MENU_MEMORY_H
#define GFX_CPU_MENU_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>
#include <memalign.h>

#define GFX_CPU_MENU_SLOTS 6
typedef struct gfx_cpu_menu_memory
{
   void *slots[GFX_CPU_MENU_SLOTS];
} gfx_cpu_menu_memory_t;

/* IDs are aligned source addresses, disjoint from small GPU slot IDs. */
static INLINE void *gfx_cpu_menu_memory_alloc(gfx_cpu_menu_memory_t *pool,
      size_t size, uintptr_t *id)
{
   unsigned i;
   if (!id)
      return NULL;
   *id = 0;
   if (!pool || !size)
      return NULL;
   for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
      if (!pool->slots[i])
      {
         if (!(pool->slots[i] = memalign_alloc(64, size)))
            return NULL;
         *id = (uintptr_t)pool->slots[i];
         return pool->slots[i];
      }
   return NULL;
}

/* The wrapper calls this after the upload consumed the source. */
static INLINE bool gfx_cpu_menu_memory_ready(void *data, uintptr_t id)
{
   (void)data;
   (void)id;
   return true;
}

static INLINE bool gfx_cpu_menu_memory_free(gfx_cpu_menu_memory_t *pool,
      uintptr_t id)
{
   unsigned i;
   if (!pool || !id)
      return false;
   for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
      if (pool->slots[i] && (uintptr_t)pool->slots[i] == id)
      {
         memalign_free(pool->slots[i]);
         pool->slots[i] = NULL;
         return true;
      }
   return false;
}

static INLINE void gfx_cpu_menu_memory_destroy(gfx_cpu_menu_memory_t *pool)
{
   unsigned i;
   if (!pool)
      return;
   for (i = 0; i < GFX_CPU_MENU_SLOTS; i++)
   {
      memalign_free(pool->slots[i]);
      pool->slots[i] = NULL;
   }
}
#endif
