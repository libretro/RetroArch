/* Compile-only stand-in for PSL1GHT's <lv2/mutex.h>. */
#ifndef PS3STUB_LV2_MUTEX_H
#define PS3STUB_LV2_MUTEX_H

#include <stdint.h>

typedef uint64_t sys_lwmutex_t;

typedef struct
{
   uint32_t attr_protocol;
   uint32_t attr_recursive;
   char     name[8];
} sys_lwmutex_attr_t;

#define SYS_LWMUTEX_ATTR_PROTOCOL  0
#define SYS_LWMUTEX_ATTR_RECURSIVE 0

int sysLwMutexCreate(sys_lwmutex_t *mutex, const sys_lwmutex_attr_t *attr);
int sysLwMutexDestroy(sys_lwmutex_t *mutex);
int sysLwMutexLock(sys_lwmutex_t *mutex, uint32_t timeout);
int sysLwMutexUnlock(sys_lwmutex_t *mutex);

#endif
