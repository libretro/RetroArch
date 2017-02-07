#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   AX_RESULT_SUCCESS                = 0,
   AX_RESULT_INVALID_DEVICE_TYPE    = -1,
   AX_RESULT_INVALID_DRC_VS_MODE    = -13,
   AX_RESULT_VOICE_IS_RUNNING       = -18,
   AX_RESULT_DELAY_TOO_BIG          = -19,
}AXResult;

#ifdef __cplusplus
}
#endif
