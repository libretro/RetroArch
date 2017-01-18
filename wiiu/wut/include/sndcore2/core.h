#pragma once
#include <wut.h>
#include "result.h"

/**
 * \defgroup sndcore2_core Core
 * \ingroup sndcore2
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AXProfile AXProfile;
typedef struct AXInitParams AXInitParams;

typedef void(*AXFrameCallback)(void);

//! A value from enum AX_INIT_RENDERER.
typedef uint32_t AXInitRenderer;

//! A value from enum AX_INIT_PIPELINE.
typedef uint32_t AXInitPipeline;

enum AX_INIT_RENDERER
{
  AX_INIT_RENDERER_32KHZ = 0,
  AX_INIT_RENDERER_48KHZ = 1,
};

enum AX_INIT_PIPELINE
{
  AX_INIT_PIPELINE_SINGLE = 0,
  AX_INIT_PIPELINE_FOUR_STAGE = 1,
};

struct AXProfile
{
  // Unknown
};

struct AXInitParams
{
   AXInitRenderer renderer;
   UNKNOWN(4);
   AXInitPipeline pipeline;
};
CHECK_OFFSET(AXInitParams, 0x00, renderer);
CHECK_OFFSET(AXInitParams, 0x08, pipeline);
CHECK_SIZE(AXInitParams, 0x0C);

void
AXInit();

void
AXQuit();

void
AXInitWithParams(AXInitParams *params);

BOOL
AXIsInit();

void
AXInitProfile(AXProfile *profile,
              uint32_t count);

uint32_t
AXGetSwapProfile(AXProfile *profile,
                 uint32_t count);

AXResult
AXSetDefaultMixerSelect(uint32_t unk0);

AXResult
AXRegisterAppFrameCallback(AXFrameCallback callback);

uint32_t
AXGetInputSamplesPerFrame();

uint32_t
AXGetInputSamplesPerSec();

#ifdef __cplusplus
}
#endif

/** @} */
