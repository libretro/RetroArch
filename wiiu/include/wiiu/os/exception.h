#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (*OSExceptionCallbackFn)(OSContext *context);

typedef enum OSExceptionType
{
   OS_EXCEPTION_TYPE_SYSTEM_RESET         = 0,
   OS_EXCEPTION_TYPE_MACHINE_CHECK        = 1,
   OS_EXCEPTION_TYPE_DSI                  = 2,
   OS_EXCEPTION_TYPE_ISI                  = 3,
   OS_EXCEPTION_TYPE_EXTERNAL_INTERRUPT   = 4,
   OS_EXCEPTION_TYPE_ALIGNMENT            = 5,
   OS_EXCEPTION_TYPE_PROGRAM              = 6,
   OS_EXCEPTION_TYPE_FLOATING_POINT       = 7,
   OS_EXCEPTION_TYPE_DECREMENTER          = 8,
   OS_EXCEPTION_TYPE_SYSTEM_CALL          = 9,
   OS_EXCEPTION_TYPE_TRACE                = 10,
   OS_EXCEPTION_TYPE_PERFORMANCE_MONITOR  = 11,
   OS_EXCEPTION_TYPE_BREAKPOINT           = 12,
   OS_EXCEPTION_TYPE_SYSTEM_INTERRUPT     = 13,
   OS_EXCEPTION_TYPE_ICI                  = 14,
} OSExceptionType;

OSExceptionCallbackFn OSSetExceptionCallback(OSExceptionType exceptionType,
      OSExceptionCallbackFn callback);
OSExceptionCallbackFn OSSetExceptionCallbackEx(int unknown, OSExceptionType exceptionType,
      OSExceptionCallbackFn callback);

#ifdef __cplusplus
}
#endif
