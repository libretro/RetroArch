#pragma once
#include <wut.h>

/**
 * \defgroup sndcore2_result Result
 * \ingroup sndcore2
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

//! A value from enum AX_RESULT.
typedef int32_t AXResult;

enum AX_RESULT
{
   AX_RESULT_SUCCESS                = 0,
   AX_RESULT_INVALID_DEVICE_TYPE    = -1,
   AX_RESULT_INVALID_DRC_VS_MODE    = -13,
   AX_RESULT_VOICE_IS_RUNNING       = -18,
   AX_RESULT_DELAY_TOO_BIG          = -19,
};

#ifdef __cplusplus
}
#endif

/** @} */
