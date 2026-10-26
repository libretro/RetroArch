#pragma once
#include <wiiu/types.h>
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*AXFrameCallback)(void);

enum AX_INIT_RENDERER
{
  AX_INIT_RENDERER_32KHZ = 0,
  AX_INIT_RENDERER_48KHZ = 1,
};
typedef uint32_t AXInitRenderer;

enum AX_INIT_PIPELINE
{
  AX_INIT_PIPELINE_SINGLE = 0,
  AX_INIT_PIPELINE_FOUR_STAGE = 1,
};
typedef uint32_t AXInitPipeline;

typedef struct AXProfile
{
  uint32_t __unknown[0x22];
}AXProfile;

typedef struct AXInitParams
{
   AXInitRenderer renderer;
   uint32_t __unknown;
   AXInitPipeline pipeline;
}AXInitParams;

void AXInit();
void AXQuit();
void AXInitWithParams(AXInitParams *params);
BOOL AXIsInit();
void AXInitProfile(AXProfile *profile, u32 count);
uint32_t AXGetSwapProfile(AXProfile *profile, u32 count);
AXResult AXSetDefaultMixerSelect(u32 unk0);
AXResult AXRegisterAppFrameCallback(AXFrameCallback callback);
uint32_t AXGetInputSamplesPerFrame();
uint32_t AXGetInputSamplesPerSec();

#ifdef __cplusplus
}
#endif
