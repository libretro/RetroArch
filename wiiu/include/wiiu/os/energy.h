#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 IMEnableDim(void);
s32 IMDisableDim(void);
s32 IMIsDimEnabled(s32 * result);
s32 IMEnableAPD(void);
s32 IMDisableAPD(void);
s32 IMIsAPDEnabled(s32 * result);
s32 IMIsAPDEnabledBySysSettings(s32 * result);

#ifdef __cplusplus
}
#endif
