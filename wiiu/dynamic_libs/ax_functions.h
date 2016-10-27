/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef __AX_FUNCTIONS_H_
#define __AX_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

void InitAXFunctionPointers(void);

extern void (* AXInitWithParams)(u32 * params);
extern void (* AXQuit)(void);
extern u32 (* AXGetInputSamplesPerSec)(void);
extern s32 (* AXVoiceBegin)(void *v);
extern s32 (* AXVoiceEnd)(void *v);
extern void (* AXSetVoiceType)(void *v, u16 type);
extern void (* AXSetVoiceOffsets)(void *v, const void *buf);
extern void (* AXSetVoiceSrcType)(void *v, u32 type);
extern void (* AXSetVoiceVe)(void *v, const void *vol);
extern s32 (* AXSetVoiceDeviceMix)(void *v, s32 device, u32 id, void *mix);
extern void (* AXSetVoiceState)(void *v, u16 state);
extern void (* AXSetVoiceSrc)(void *v, const void *src);
extern s32 (* AXSetVoiceSrcRatio)(void *v, f32 ratio);
extern void * (* AXAcquireVoice)(u32 prio, void * callback, u32 arg);
extern void (* AXFreeVoice)(void *v);
extern void (* AXRegisterFrameCallback)(void * callback);
extern u32 (* AXGetVoiceLoopCount)(void * v);
extern void (* AXSetVoiceEndOffset)(void * v, u32 offset);
extern void (* AXSetVoiceLoopOffset)(void * v, u32 offset);

#ifdef __cplusplus
}
#endif

#endif // __VPAD_FUNCTIONS_H_
