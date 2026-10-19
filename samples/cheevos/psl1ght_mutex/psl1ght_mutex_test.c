#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Load host headers before selecting the simulated target. */
#undef _WIN32
#define __PSL1GHT__ 1
#include "../../../deps/rcheevos/src/rc_compat.c"

static unsigned int created;
static unsigned int destroyed;
static unsigned int locks;
static unsigned int unlocks;
static unsigned int errors;

int sysLwMutexCreate(sys_lwmutex_t *mutex, const sys_lwmutex_attr_t *attr)
{
   unsigned int i;
   if (attr->attr_protocol != SYS_LWMUTEX_PROTOCOL_PRIO ||
       attr->attr_recursive != SYS_LWMUTEX_ATTR_RECURSIVE)
      errors++;
   for (i = 0; i < sizeof(attr->name); i++)
      if (attr->name[i])
         errors++;
   mutex->depth = 0;
   created++;
   return 0;
}

int sysLwMutexDestroy(sys_lwmutex_t *mutex)
{
   if (mutex->depth)
      errors++;
   destroyed++;
   return 0;
}

int sysLwMutexLock(sys_lwmutex_t *mutex, uint64_t timeout)
{
   if (timeout)
      errors++;
   mutex->depth++;
   locks++;
   return 0;
}

int sysLwMutexUnlock(sys_lwmutex_t *mutex)
{
   if (!mutex->depth)
      errors++;
   mutex->depth--;
   unlocks++;
   return 0;
}

int main(void)
{
   rc_mutex_t mutex;
   rc_mutex_init(&mutex);
   rc_mutex_lock(&mutex);
   rc_mutex_lock(&mutex);
   if (mutex.depth != 2)
      errors++;
   rc_mutex_unlock(&mutex);
   rc_mutex_unlock(&mutex);
   rc_mutex_destroy(&mutex);
   if (errors || created != 1 || destroyed != 1 || locks != 2 || unlocks != 2)
      return 1;
   puts("PASS: rcheevos PSL1GHT recursive mutex attributes and lifecycle");
   return 0;
}
