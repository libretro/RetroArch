#pragma once
#include <wiiu/types.h>
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*AXDeviceFinalMixCallback)(void*);
typedef void(*AXAuxCallback)(void*, void*);

enum AX_DEVICE_MODE
{
   /* Unknown */
   AX_DEVICE_MODE_UNKNOWN
};
typedef uint32_t AXDeviceMode;

enum AX_DEVICE_TYPE
{
   AX_DEVICE_TYPE_TV          = 0,
   AX_DEVICE_TYPE_DRC         = 1,
   AX_DEVICE_TYPE_CONTROLLER  = 2,
};
typedef uint32_t AXDeviceType;

AXResult AXGetDeviceMode(AXDeviceType type, AXDeviceMode *mode);
AXResult AXGetDeviceFinalMixCallback(AXDeviceType type, AXDeviceFinalMixCallback *func);
AXResult AXRegisterDeviceFinalMixCallback(AXDeviceType type, AXDeviceFinalMixCallback func);
AXResult AXGetAuxCallback(AXDeviceType type, uint32_t unk0, uint32_t unk1, AXAuxCallback *callback, void **userData);
AXResult AXRegisterAuxCallback(AXDeviceType type, uint32_t unk0, uint32_t unk1, AXAuxCallback callback, void *userData);
AXResult AXSetDeviceLinearUpsampler(AXDeviceType type, uint32_t unk0, uint32_t unk1);
AXResult AXSetDeviceCompressor(AXDeviceType type, uint32_t unk0);
AXResult AXSetDeviceUpsampleStage(AXDeviceType type, BOOL postFinalMix);
AXResult AXSetDeviceVolume(AXDeviceType type, uint32_t id, uint16_t volume);

#ifdef __cplusplus
}
#endif
