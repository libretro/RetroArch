#pragma once
#include <wut.h>
#include "result.h"

/**
 * \defgroup sndcore2_drcvs DRC VS
 * \ingroup sndcore2
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

//! A value from enum AX_DRC_VS_MODE.
typedef uint32_t AXDRCVSMode;

//! A value from enum AX_DRC_VS_OUTPUT.
typedef uint32_t AXDRCVSOutput;

//! A value from enum AX_DRC_VS_LC.
typedef uint32_t AXDRCVSLC;

//! A value from enum AX_DRC_VS_SPEAKER_POS.
typedef uint32_t AXDRCVSSpeakerPosition;

//! A value from enum AX_DRC_VS_SURROUND_GAIN.
typedef uint32_t AXDRCVSSurroundLevelGain;

enum AX_DRC_VS_MODE
{
   // Unknown
   AX_DRC_VS_MODE_UNKNOWN
};

enum AX_DRC_VS_OUTPUT
{
   // Unknown
   AX_DRC_VS_OUTPUT_UNKNOWN
};

enum AX_DRC_VS_LC
{
   // Unknown
   AX_DRC_VS_LC_UNKNOWN
};

enum AX_DRC_VS_SPEAKER_POS
{
   // Unknown
   AX_DRC_VS_SPEAKER_POS_UNKNOWN
};

enum AX_DRC_VS_SURROUND_GAIN
{
   // Unknown
   AX_DRC_VS_SURROUND_GAIN_UNKNOWN
};

AXResult
AXGetDRCVSMode(AXDRCVSMode *mode);

AXResult
AXSetDRCVSMode(AXDRCVSMode mode);

AXResult
AXSetDRCVSDownmixBalance(AXDRCVSOutput output,
                         float balance);

AXResult
AXSetDRCVSLC(AXDRCVSLC lc);

AXResult
AXSetDRCVSLimiter(BOOL limit);

AXResult
AXSetDRCVSLimiterThreshold(float threshold);

AXResult
AXSetDRCVSOutputGain(AXDRCVSOutput output,
                     float gain);

AXResult
AXSetDRCVSSpeakerPosition(AXDRCVSOutput output,
                          AXDRCVSSpeakerPosition pos);

AXResult
AXSetDRCVSSurroundDepth(AXDRCVSOutput output,
                        float depth);

AXResult
AXSetDRCVSSurroundLevelGain(AXDRCVSSurroundLevelGain gain);

#ifdef __cplusplus
}
#endif

/** @} */
