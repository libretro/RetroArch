#pragma once
#include <wiiu/types.h>
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   /* Unknown */
   AX_DRC_VS_MODE_UNKNOWN
} AXDRCVSMode;

typedef enum
{
   /* Unknown */
   AX_DRC_VS_OUTPUT_UNKNOWN
} AXDRCVSOutput;

typedef enum
{
   /* Unknown */
   AX_DRC_VS_LC_UNKNOWN
} AXDRCVSLC;

typedef enum
{
   /* Unknown */
   AX_DRC_VS_SPEAKER_POS_UNKNOWN
} AXDRCVSSpeakerPosition;

typedef enum AX_DRC_VS_SURROUND_GAIN
{
   /* Unknown */
   AX_DRC_VS_SURROUND_GAIN_UNKNOWN
} AXDRCVSSurroundLevelGain;

AXResult AXGetDRCVSMode(AXDRCVSMode *mode);
AXResult AXSetDRCVSMode(AXDRCVSMode mode);
AXResult AXSetDRCVSDownmixBalance(AXDRCVSOutput output, float balance);
AXResult AXSetDRCVSLC(AXDRCVSLC lc);
AXResult AXSetDRCVSLimiter(BOOL limit);
AXResult AXSetDRCVSLimiterThreshold(float threshold);
AXResult AXSetDRCVSOutputGain(AXDRCVSOutput output, float gain);
AXResult AXSetDRCVSSpeakerPosition(AXDRCVSOutput output, AXDRCVSSpeakerPosition pos);
AXResult AXSetDRCVSSurroundDepth(AXDRCVSOutput output, float depth);
AXResult AXSetDRCVSSurroundLevelGain(AXDRCVSSurroundLevelGain gain);

#ifdef __cplusplus
}
#endif

/** @} */
