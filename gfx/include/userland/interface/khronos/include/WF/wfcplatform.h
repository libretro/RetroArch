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

#ifndef _WFCPLATFORM_H_
#define _WFCPLATFORM_H_

#include "../KHR/khrplatform.h"
#include "../EGL/egl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WFC_API_CALL
#define WFC_API_CALL KHRONOS_APICALL
#endif
#ifndef WFC_APIENTRY
#define WFC_APIENTRY KHRONOS_APIENTRY
#endif
#ifndef WFC_APIEXIT
#define WFC_APIEXIT KHRONOS_APIATTRIBUTES
#endif

#ifndef WFC_DEFAULT_SCREEN_NUMBER
#define WFC_DEFAULT_SCREEN_NUMBER (0)
#endif

typedef enum {
    WFC_FALSE               = 0,
    WFC_TRUE                = 1,
    WFC_BOOLEAN_FORCE_32BIT = 0x7FFFFFFF
} WFCboolean;

typedef khronos_int32_t   WFCint;
typedef khronos_float_t   WFCfloat;
typedef khronos_uint32_t  WFCbitfield;
typedef khronos_uint32_t  WFCHandle;

typedef EGLDisplay   WFCEGLDisplay;
typedef void         *WFCEGLSync;   /* An opaque handle to an EGLSyncKHR */
typedef WFCHandle    WFCNativeStreamType;

#ifdef __cplusplus
}
#endif

#endif /* _WFCPLATFORM_H_ */
