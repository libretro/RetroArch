/* Compile-only stand-in for PSL1GHT's <lv2/cond.h>. */
#ifndef PS3STUB_LV2_COND_H
#define PS3STUB_LV2_COND_H

#include <stdint.h>
#include <lv2/mutex.h>

typedef uint64_t sys_lwcond_t;

typedef struct
{
   char name[8];
} sys_lwcond_attr_t;

int sysLwCondCreate(sys_lwcond_t *cond, sys_lwmutex_t *mutex,
      const sys_lwcond_attr_t *attr);
int sysLwCondDestroy(sys_lwcond_t *cond);
int sysLwCondSignal(sys_lwcond_t *cond);
int sysLwCondWait(sys_lwcond_t *cond, uint32_t timeout);

#endif
