/* Compile-only stand-in for PSL1GHT's <sysutil/sysutil.h>: the system
 * utility callback calls defines/ps3_defines.h maps the Cell SDK's
 * onto under HAVE_SYSUTILS, in PSL1GHT's signatures and values. */
#ifndef PS3STUB_SYSUTIL_SYSUTIL_H
#define PS3STUB_SYSUTIL_SYSUTIL_H

#include <stdint.h>

#define SYSUTIL_EXIT_GAME 0x0101

typedef void (*sysutilCallback)(uint64_t status, uint64_t param,
      void *usrdata);

int32_t sysUtilCheckCallback(void);
int32_t sysUtilRegisterCallback(int32_t slot, sysutilCallback cb,
      void *usrdata);

#endif
