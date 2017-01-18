#pragma once
#include <wut.h>
#include "time.h"

/**
 * \defgroup coreinit_systeminfo System Info
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSSystemInfo OSSystemInfo;

struct OSSystemInfo
{
   uint32_t clockSpeed;
   UNKNOWN(0x4);
   OSTime baseTime;
   UNKNOWN(0x10);
};
CHECK_OFFSET(OSSystemInfo, 0x0, clockSpeed);
CHECK_OFFSET(OSSystemInfo, 0x8, baseTime);
CHECK_SIZE(OSSystemInfo, 0x20);

OSSystemInfo *
OSGetSystemInfo();

#ifdef __cplusplus
}
#endif

/** @} */
