#pragma once
#include <wut.h>
#include <coreinit/time.h>
#include "enum.h"

/**
 * \defgroup gx2_event Event
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2DisplayListOverrunData GX2DisplayListOverrunData;
typedef void (*GX2EventCallbackFunction)(GX2EventType, void *);

struct GX2DisplayListOverrunData
{
   //! Pointer to overrun display list
   void *oldList;

   //! Size of overrun display list
   uint32_t oldSize;

   //! Pointer to new display list
   void *newList;

   //! Size of new display list
   uint32_t newSize;

   UNKNOWN(8);
};
CHECK_OFFSET(GX2DisplayListOverrunData, 0x00, oldList);
CHECK_OFFSET(GX2DisplayListOverrunData, 0x04, oldSize);
CHECK_OFFSET(GX2DisplayListOverrunData, 0x08, newList);
CHECK_OFFSET(GX2DisplayListOverrunData, 0x0C, newSize);
CHECK_SIZE(GX2DisplayListOverrunData, 0x18);

BOOL
GX2DrawDone();

void
GX2WaitForVsync();

void
GX2WaitForFlip();

void
GX2SetEventCallback(GX2EventType type,
                    GX2EventCallbackFunction func,
                    void *userData);

void
GX2GetEventCallback(GX2EventType type,
                    GX2EventCallbackFunction *funcOut,
                    void **userDataOut);

OSTime
GX2GetRetiredTimeStamp();

OSTime
GX2GetLastSubmittedTimeStamp();

BOOL
GX2WaitTimeStamp(OSTime time);

void
GX2GetSwapStatus(uint32_t *swapCount,
                 uint32_t *flipCount,
                 OSTime *lastFlip,
                 OSTime *lastVsync);

#ifdef __cplusplus
}
#endif

/** @} */
