#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_foreground Foreground Management
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

void
OSEnableForegroundExit();

void
OSReleaseForeground();

void
OSSavesDone_ReadyToRelease();

#ifdef __cplusplus
}
#endif

/** @} */
