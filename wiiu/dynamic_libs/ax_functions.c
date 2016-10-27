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
#include "common/common.h"
#include "os_functions.h"
#include "ax_functions.h"

unsigned int sound_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, AXInitWithParams, u32 * params);
EXPORT_DECL(void, AXInit, void);
EXPORT_DECL(void, AXQuit, void);
EXPORT_DECL(u32, AXGetInputSamplesPerSec, void);
EXPORT_DECL(u32, AXGetInputSamplesPerFrame, void);
EXPORT_DECL(s32, AXVoiceBegin, void *v);
EXPORT_DECL(s32, AXVoiceEnd, void *v);
EXPORT_DECL(void, AXSetVoiceType, void *v, u16 type);
EXPORT_DECL(void, AXSetVoiceOffsets, void *v, const void *buf);
EXPORT_DECL(void, AXSetVoiceSrcType, void *v, u32 type);
EXPORT_DECL(void, AXSetVoiceVe, void *v, const void *vol);
EXPORT_DECL(s32, AXSetVoiceDeviceMix, void *v, s32 device, u32 id, void *mix);
EXPORT_DECL(void, AXSetVoiceState, void *v, u16 state);
EXPORT_DECL(void, AXSetVoiceSrc, void *v, const void *src);
EXPORT_DECL(s32, AXSetVoiceSrcRatio, void *v,f32 ratio)
EXPORT_DECL(void *, AXAcquireVoice, u32 prio, void * callback, u32 arg);
EXPORT_DECL(void, AXFreeVoice, void *v);
EXPORT_DECL(void, AXRegisterFrameCallback, void * callback);
EXPORT_DECL(u32, AXGetVoiceLoopCount, void *v);
EXPORT_DECL(void, AXSetVoiceEndOffset, void *v, u32 offset);
EXPORT_DECL(void, AXSetVoiceLoopOffset, void *v, u32 offset);

void InitAcquireAX(void)
{
    unsigned int *funcPointer = 0;

    if(OS_FIRMWARE >= 400)
    {
        AXInit = 0;

        OSDynLoad_Acquire("sndcore2.rpl", &sound_handle);
        OS_FIND_EXPORT(sound_handle, AXInitWithParams);
        OS_FIND_EXPORT(sound_handle, AXGetInputSamplesPerSec);
    }
    else
    {
        AXInitWithParams = 0;
        AXGetInputSamplesPerSec = 0;

        OSDynLoad_Acquire("snd_core.rpl", &sound_handle);
        OS_FIND_EXPORT(sound_handle, AXInit);
    }
}

void InitAXFunctionPointers(void)
{
    unsigned int *funcPointer = 0;

    InitAcquireAX();

    OS_FIND_EXPORT(sound_handle, AXQuit);
    OS_FIND_EXPORT(sound_handle, AXVoiceBegin);
    OS_FIND_EXPORT(sound_handle, AXVoiceEnd);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceType);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceOffsets);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceSrcType);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceVe);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceDeviceMix);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceState);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceSrc);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceSrcRatio);
    OS_FIND_EXPORT(sound_handle, AXAcquireVoice);
    OS_FIND_EXPORT(sound_handle, AXFreeVoice);
    OS_FIND_EXPORT(sound_handle, AXRegisterFrameCallback);
    OS_FIND_EXPORT(sound_handle, AXGetVoiceLoopCount);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceEndOffset);
    OS_FIND_EXPORT(sound_handle, AXSetVoiceLoopOffset);
}

void ProperlyEndTransitionAudio(void)
{
    bool (* check_os_audio_transition_flag_old)(void);
    void (* AXInit_old)(void);
    void (* AXQuit_old)(void);

    unsigned int *funcPointer = 0;
    unsigned int sound_handle;
    OSDynLoad_Acquire("snd_core.rpl", &sound_handle);

    OS_FIND_EXPORT_EX(sound_handle, check_os_audio_transition_flag, check_os_audio_transition_flag_old);
    OS_FIND_EXPORT_EX(sound_handle, AXInit, AXInit_old);
    OS_FIND_EXPORT_EX(sound_handle, AXQuit, AXQuit_old);

    if (check_os_audio_transition_flag_old())
    {
        AXInit_old();
        AXQuit_old();
    }
}
