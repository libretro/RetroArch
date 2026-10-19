#ifndef RC_PSL1GHT_TEST_MUTEX_H
#define RC_PSL1GHT_TEST_MUTEX_H

#include <stdint.h>

#define SYS_LWMUTEX_PROTOCOL_PRIO 2
#define SYS_LWMUTEX_ATTR_RECURSIVE 0x10

typedef struct
{
   unsigned int depth;
} sys_lwmutex_t;

typedef struct
{
   unsigned int attr_protocol;
   unsigned int attr_recursive;
   char name[8];
} sys_lwmutex_attr_t;

int sysLwMutexCreate(sys_lwmutex_t *mutex, const sys_lwmutex_attr_t *attr);
int sysLwMutexDestroy(sys_lwmutex_t *mutex);
int sysLwMutexLock(sys_lwmutex_t *mutex, uint64_t timeout);
int sysLwMutexUnlock(sys_lwmutex_t *mutex);

#endif
