#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2ShadowState
{
   uint32_t config[0xB00];
   uint32_t context[0x400];
   uint32_t alu[0x800];
   uint32_t loop[0x60];
   uint32_t __unk_0[0x20];
   uint32_t resource[0xD9E];
   uint32_t __unk_1[0x22];
   uint32_t sampler[0xA2];
   uint32_t __unk_2[0x3C];
} GX2ShadowState;

typedef struct GX2ContextState
{
   GX2ShadowState shadowState;
   uint32_t __unk_0;
   uint32_t shadowDisplayListSize;
   uint32_t __unk_1[0x2FC];
   uint32_t shadowDisplayList[192];
} GX2ContextState;

void GX2SetupContextStateEx(GX2ContextState *state, BOOL unk1);
void GX2GetContextStateDisplayList(GX2ContextState *state, void *outDisplayList, uint32_t *outSize);
void GX2SetContextState(GX2ContextState *state);
void GX2SetDefaultState();

#ifdef __cplusplus
}
#endif
