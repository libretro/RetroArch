#pragma once
#include <wut.h>
#include "result.h"

/**
 * \defgroup sndcore2_device Device
 * \ingroup sndcore2
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*AXDeviceFinalMixCallback)(void*);
typedef void(*AXAuxCallback)(void*, void*);

//! A value from enum AX_DEVICE_MODE.
typedef uint32_t AXDeviceMode;

//! A value from enum AX_DEVICE_TYPE.
typedef uint32_t AXDeviceType;

enum AX_DEVICE_MODE
{
   // Unknown
   AX_DEVICE_MODE_UNKNOWN
};

enum AX_DEVICE_TYPE
{
   AX_DEVICE_TYPE_TV          = 0,
   AX_DEVICE_TYPE_DRC         = 1,
   AX_DEVICE_TYPE_CONTROLLER  = 2,
};

AXResult
AXGetDeviceMode(AXDeviceType type,
                AXDeviceMode *mode);

AXResult
AXGetDeviceFinalMixCallback(AXDeviceType type,
                            AXDeviceFinalMixCallback *func);

AXResult
AXRegisterDeviceFinalMixCallback(AXDeviceType type,
                                 AXDeviceFinalMixCallback func);

AXResult
AXGetAuxCallback(AXDeviceType type,
                 uint32_t unk0,
                 uint32_t unk1,
                 AXAuxCallback *callback,
                 void **userData);

AXResult
AXRegisterAuxCallback(AXDeviceType type,
                      uint32_t unk0,
                      uint32_t unk1,
                      AXAuxCallback callback,
                      void *userData);

AXResult
AXSetDeviceLinearUpsampler(AXDeviceType type,
                           uint32_t unk0,
                           uint32_t unk1);

AXResult
AXSetDeviceCompressor(AXDeviceType type,
                      uint32_t unk0);

AXResult
AXSetDeviceUpsampleStage(AXDeviceType type,
                         BOOL postFinalMix);

AXResult
AXSetDeviceVolume(AXDeviceType type,
                  uint32_t id,
                  uint16_t volume);

#ifdef __cplusplus
}
#endif

/** @} */
