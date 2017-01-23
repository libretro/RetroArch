#pragma once
#include <wiiu/types.h>
#include <wiiu/os/time.h>
#include "enum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GX2EventCallbackFunction)(GX2EventType, void *);

typedef struct GX2DisplayListOverrunData
{
   void *oldList;
   uint32_t oldSize;
   void *newList;
   uint32_t newSize;
   uint32_t __unk[0x2];
} GX2DisplayListOverrunData;

BOOL GX2DrawDone();
void GX2WaitForVsync();
void GX2WaitForFlip();
void GX2SetEventCallback(GX2EventType type, GX2EventCallbackFunction func, void *userData);
void GX2GetEventCallback(GX2EventType type, GX2EventCallbackFunction *funcOut, void **userDataOut);
OSTime GX2GetRetiredTimeStamp();
OSTime GX2GetLastSubmittedTimeStamp();
BOOL GX2WaitTimeStamp(OSTime time);
void GX2GetSwapStatus(uint32_t *swapCount, uint32_t *flipCount, OSTime *lastFlip, OSTime *lastVsync);

#ifdef __cplusplus
}
#endif

/** @} */
