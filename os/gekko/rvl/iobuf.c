/* Wii: buffers for IOS's USB and Bluetooth interfaces, which want
 * them in MEM2 and 32-byte aligned: a pool taken from the top of MEM2
 * on first use, handed out in 64-byte units. */

#include <string.h>

#include <gekko/thread.h>

#include "rvl.h"

#define POOL_SIZE  (256 * 1024)
#define POOL_UNIT  64
#define POOL_UNITS (POOL_SIZE / POOL_UNIT)

static uint8_t   *pool;
static uint8_t    pool_busy[POOL_UNITS / 8];
static gk_mutex_t pool_lock = GK_MUTEX_INIT;
static gk_cond_t  pool_freed = GK_COND_INIT;

void *gk_iobuf_get(uint32_t size)
{
   uint32_t n = (size + POOL_UNIT - 1) / POOL_UNIT, i, run = 0;
   void *p = NULL;
   if (!n || n > POOL_UNITS)
      return NULL;
   gk_mutex_lock(&pool_lock);
   if (!pool && !(pool = (uint8_t*)gk_arena_take_top(&gk_mem2, POOL_SIZE,
               32)))
   {
      gk_mutex_unlock(&pool_lock);
      return NULL;
   }
   while (!p)
   {
      for (i = 0, run = 0; i < POOL_UNITS; i++)
      {
         run = (pool_busy[i / 8] & (1u << (i % 8))) ? 0 : run + 1;
         if (run == n)
            break;
      }
      if (run == n)
      {
         uint32_t start = i + 1 - n, k;
         for (k = start; k <= i; k++)
            pool_busy[k / 8] |= (uint8_t)(1u << (k % 8));
         p = pool + (size_t)start * POOL_UNIT;
      }
      else
         gk_cond_wait(&pool_freed, &pool_lock, GK_WAIT_FOREVER);
   }
   gk_mutex_unlock(&pool_lock);
   memset(p, 0, size);
   return p;
}

void gk_iobuf_put(void *p, uint32_t size)
{
   uint32_t start, n, k;
   if (!p)
      return;
   start = (uint32_t)((uint8_t*)p - pool) / POOL_UNIT;
   n     = (size + POOL_UNIT - 1) / POOL_UNIT;
   gk_mutex_lock(&pool_lock);
   for (k = start; k < start + n; k++)
      pool_busy[k / 8] &= (uint8_t)~(1u << (k % 8));
   gk_cond_broadcast(&pool_freed);
   gk_mutex_unlock(&pool_lock);
}
