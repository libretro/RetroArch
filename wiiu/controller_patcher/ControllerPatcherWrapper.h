#ifndef _CONTROLLER_PATCHER_WRAPPER_H_
#define _CONTROLLER_PATCHER_WRAPPER_H_

#include "wiiu/vpad.h"

/* Main */
#ifdef __cplusplus
extern "C" {
#endif

#include "./patcher/ControllerPatcherDefs.h"

//! C wrapper for our C++ functions
void ControllerPatcherInit(void);
void ControllerPatcherDeInit(void);
CONTROLLER_PATCHER_RESULT_OR_ERROR setControllerDataFromHID(VPADStatus * data);
CONTROLLER_PATCHER_RESULT_OR_ERROR gettingInputAllDevices(InputData * output,s32 array_size);

#ifdef __cplusplus
}
#endif

#endif
