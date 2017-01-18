#pragma once
#include <wut.h>

/**
 * \defgroup gx2_context Context State
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2ShadowState GX2ShadowState;
typedef struct GX2ContextState GX2ContextState;

struct GX2ShadowState
{
   uint32_t config[0xB00];
   uint32_t context[0x400];
   uint32_t alu[0x800];
   uint32_t loop[0x60];
   PADDING((0x80 - 0x60) * 4);
   uint32_t resource[0xD9E];
   PADDING((0xDC0 - 0xD9E) * 4);
   uint32_t sampler[0xA2];
   PADDING((0xC0 - 0xA2) * 4);
};
CHECK_OFFSET(GX2ShadowState, 0x0000, config);
CHECK_OFFSET(GX2ShadowState, 0x2C00, context);
CHECK_OFFSET(GX2ShadowState, 0x3C00, alu);
CHECK_OFFSET(GX2ShadowState, 0x5C00, loop);
CHECK_OFFSET(GX2ShadowState, 0x5E00, resource);
CHECK_OFFSET(GX2ShadowState, 0x9500, sampler);
CHECK_SIZE(GX2ShadowState, 0x9800);

struct GX2ContextState
{
   GX2ShadowState shadowState;
   UNKNOWN(4);
   uint32_t shadowDisplayListSize;
   UNKNOWN(0x9e00 - 0x9808);
   uint32_t shadowDisplayList[192];
};
CHECK_OFFSET(GX2ContextState, 0x0000, shadowState);
CHECK_OFFSET(GX2ContextState, 0x9804, shadowDisplayListSize);
CHECK_OFFSET(GX2ContextState, 0x9e00, shadowDisplayList);
CHECK_SIZE(GX2ContextState, 0xa100);

void
GX2SetupContextStateEx(GX2ContextState *state,
                       BOOL unk1);

void
GX2GetContextStateDisplayList(GX2ContextState *state,
                              void *outDisplayList,
                              uint32_t *outSize);

void
GX2SetContextState(GX2ContextState *state);

void
GX2SetDefaultState();

#ifdef __cplusplus
}
#endif

/** @} */
