/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _WFC_H_
#define _WFC_H_

#include "wfcplatform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENWFC_VERSION_1_0       (1)

#define WFC_NONE                  (0)

#define WFC_INVALID_HANDLE        ((WFCHandle)0)

#define WFC_DEFAULT_DEVICE_ID     (0)

#define WFC_MAX_INT               ((WFCint)16777216)
#define WFC_MAX_FLOAT             ((WFCfloat)16777216)

typedef WFCHandle WFCDevice;
typedef WFCHandle WFCContext;
typedef WFCHandle WFCSource;
typedef WFCHandle WFCMask;
typedef WFCHandle WFCElement;

typedef enum {
    WFC_ERROR_NONE                          = 0,
    WFC_ERROR_OUT_OF_MEMORY                 = 0x7001,
    WFC_ERROR_ILLEGAL_ARGUMENT              = 0x7002,
    WFC_ERROR_UNSUPPORTED                   = 0x7003,
    WFC_ERROR_BAD_ATTRIBUTE                 = 0x7004,
    WFC_ERROR_IN_USE                        = 0x7005,
    WFC_ERROR_BUSY                          = 0x7006,
    WFC_ERROR_BAD_DEVICE                    = 0x7007,
    WFC_ERROR_BAD_HANDLE                    = 0x7008,
    WFC_ERROR_INCONSISTENCY                 = 0x7009,
    WFC_ERROR_FORCE_32BIT                   = 0x7FFFFFFF
} WFCErrorCode;

typedef enum {
    WFC_DEVICE_FILTER_SCREEN_NUMBER         = 0x7020,
    WFC_DEVICE_FILTER_FORCE_32BIT           = 0x7FFFFFFF
} WFCDeviceFilter;

typedef enum {
    /* Read-only */
    WFC_DEVICE_CLASS                        = 0x7030,
    WFC_DEVICE_ID                           = 0x7031,
    WFC_DEVICE_FORCE_32BIT                  = 0x7FFFFFFF
} WFCDeviceAttrib;

typedef enum {
    WFC_DEVICE_CLASS_FULLY_CAPABLE          = 0x7040,
    WFC_DEVICE_CLASS_OFF_SCREEN_ONLY        = 0x7041,
    WFC_DEVICE_CLASS_FORCE_32BIT            = 0x7FFFFFFF
} WFCDeviceClass;

typedef enum {
    /* Read-only */
    WFC_CONTEXT_TYPE                        = 0x7051,
    WFC_CONTEXT_TARGET_HEIGHT               = 0x7052,
    WFC_CONTEXT_TARGET_WIDTH                = 0x7053,
    WFC_CONTEXT_LOWEST_ELEMENT              = 0x7054,

    /* Read-write */
    WFC_CONTEXT_ROTATION                    = 0x7061,
    WFC_CONTEXT_BG_COLOR                    = 0x7062,
    WFC_CONTEXT_FORCE_32BIT                 = 0x7FFFFFFF
} WFCContextAttrib;

typedef enum {
    WFC_CONTEXT_TYPE_ON_SCREEN              = 0x7071,
    WFC_CONTEXT_TYPE_OFF_SCREEN             = 0x7072,
    WFC_CONTEXT_TYPE_FORCE_32BIT            = 0x7FFFFFFF
} WFCContextType;

typedef enum {
    /* Clockwise rotation */
    WFC_ROTATION_0                          = 0x7081,  /* default */
    WFC_ROTATION_90                         = 0x7082,
    WFC_ROTATION_180                        = 0x7083,
    WFC_ROTATION_270                        = 0x7084,
    WFC_ROTATION_FORCE_32BIT                = 0x7FFFFFFF
} WFCRotation;

typedef enum {
    WFC_ELEMENT_DESTINATION_RECTANGLE       = 0x7101,
    WFC_ELEMENT_SOURCE                      = 0x7102,
    WFC_ELEMENT_SOURCE_RECTANGLE            = 0x7103,
    WFC_ELEMENT_SOURCE_FLIP                 = 0x7104,
    WFC_ELEMENT_SOURCE_ROTATION             = 0x7105,
    WFC_ELEMENT_SOURCE_SCALE_FILTER         = 0x7106,
    WFC_ELEMENT_TRANSPARENCY_TYPES          = 0x7107,
    WFC_ELEMENT_GLOBAL_ALPHA                = 0x7108,
    WFC_ELEMENT_MASK                        = 0x7109,
    WFC_ELEMENT_FORCE_32BIT                 = 0x7FFFFFFF
} WFCElementAttrib;

typedef enum {
    WFC_SCALE_FILTER_NONE                   = 0x7151,  /* default */
    WFC_SCALE_FILTER_FASTER                 = 0x7152,
    WFC_SCALE_FILTER_BETTER                 = 0x7153,
    WFC_SCALE_FILTER_FORCE_32BIT            = 0x7FFFFFFF
} WFCScaleFilter;

typedef enum {
    WFC_TRANSPARENCY_NONE                   = 0,       /* default */
    WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA   = (1 << 0),
    WFC_TRANSPARENCY_SOURCE                 = (1 << 1),
    WFC_TRANSPARENCY_MASK                   = (1 << 2),
    WFC_TRANSPARENCY_FORCE_32BIT            = 0x7FFFFFFF
} WFCTransparencyType;

typedef enum {
    WFC_VENDOR                              = 0x7200,
    WFC_RENDERER                            = 0x7201,
    WFC_VERSION                             = 0x7202,
    WFC_EXTENSIONS                          = 0x7203,
    WFC_STRINGID_FORCE_32BIT                = 0x7FFFFFFF
} WFCStringID;


/* Function Prototypes */

/* Device */
WFC_API_CALL WFCint WFC_APIENTRY
    wfcEnumerateDevices(WFCint *deviceIds, WFCint deviceIdsCount,
        const WFCint *filterList) WFC_APIEXIT;
WFC_API_CALL WFCDevice WFC_APIENTRY
    wfcCreateDevice(WFCint deviceId, const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL WFCErrorCode WFC_APIENTRY
    wfcGetError(WFCDevice dev) WFC_APIEXIT;
WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetDeviceAttribi(WFCDevice dev, WFCDeviceAttrib attrib) WFC_APIEXIT;
WFC_API_CALL WFCErrorCode WFC_APIENTRY
    wfcDestroyDevice(WFCDevice dev) WFC_APIEXIT;

/* Context */
WFC_API_CALL WFCContext WFC_APIENTRY
    wfcCreateOnScreenContext(WFCDevice dev,
        WFCint screenNumber,
        const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL WFCContext WFC_APIENTRY
    wfcCreateOffScreenContext(WFCDevice dev,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcCommit(WFCDevice dev, WFCContext ctx, WFCboolean wait) WFC_APIEXIT;
WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetContextAttribi(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcGetContextAttribfv(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib, WFCint count, WFCfloat *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetContextAttribi(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib, WFCint value) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetContextAttribfv(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib,
        WFCint count, const WFCfloat *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcDestroyContext(WFCDevice dev, WFCContext ctx) WFC_APIEXIT;

/* Source */
WFC_API_CALL WFCSource WFC_APIENTRY
    wfcCreateSourceFromStream(WFCDevice dev, WFCContext ctx,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcDestroySource(WFCDevice dev, WFCSource src) WFC_APIEXIT;

/* Mask */
WFC_API_CALL WFCMask WFC_APIENTRY
    wfcCreateMaskFromStream(WFCDevice dev, WFCContext ctx,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcDestroyMask(WFCDevice dev, WFCMask mask) WFC_APIEXIT;

/* Element */
WFC_API_CALL WFCElement WFC_APIENTRY
    wfcCreateElement(WFCDevice dev, WFCContext ctx,
        const WFCint *attribList) WFC_APIEXIT;
WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetElementAttribi(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib) WFC_APIEXIT;
WFC_API_CALL WFCfloat WFC_APIENTRY
    wfcGetElementAttribf(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcGetElementAttribiv(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib, WFCint count, WFCint *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcGetElementAttribfv(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib, WFCint count, WFCfloat *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribi(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib, WFCint value) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribf(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib, WFCfloat value) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribiv(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib,
        WFCint count, const WFCint *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribfv(WFCDevice dev, WFCElement element,
        WFCElementAttrib attrib,
        WFCint count, const WFCfloat *values) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcInsertElement(WFCDevice dev, WFCElement element,
        WFCElement subordinate) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcRemoveElement(WFCDevice dev, WFCElement element) WFC_APIEXIT;
WFC_API_CALL WFCElement WFC_APIENTRY
    wfcGetElementAbove(WFCDevice dev, WFCElement element) WFC_APIEXIT;
WFC_API_CALL WFCElement WFC_APIENTRY
    wfcGetElementBelow(WFCDevice dev, WFCElement element) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcDestroyElement(WFCDevice dev, WFCElement element) WFC_APIEXIT;

/* Rendering */
WFC_API_CALL void WFC_APIENTRY
    wfcActivate(WFCDevice dev, WFCContext ctx) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcDeactivate(WFCDevice dev, WFCContext ctx) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcCompose(WFCDevice dev, WFCContext ctx, WFCboolean wait) WFC_APIEXIT;
WFC_API_CALL void WFC_APIENTRY
    wfcFence(WFCDevice dev, WFCContext ctx, WFCEGLDisplay dpy,
        WFCEGLSync sync) WFC_APIEXIT;

/* Renderer and extension information */
WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetStrings(WFCDevice dev,
        WFCStringID name,
        const char **strings,
        WFCint stringsCount) WFC_APIEXIT;
WFC_API_CALL WFCboolean WFC_APIENTRY
    wfcIsExtensionSupported(WFCDevice dev, const char *string) WFC_APIEXIT;

#ifdef __cplusplus
}
#endif

#endif /* _WFC_H_ */
