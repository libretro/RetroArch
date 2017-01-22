#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

void OSEnableForegroundExit();
void OSReleaseForeground();
void OSSavesDone_ReadyToRelease();

#ifdef __cplusplus
}
#endif
