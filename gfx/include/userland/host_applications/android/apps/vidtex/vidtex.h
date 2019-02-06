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

#ifndef VIDTEX_H
#define VIDTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/** \file Video-on-texture display.
 * Displays video decoded from a media URI or from camera preview onto an EGL surface.
 */
#include <EGL/egl.h>
#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_stdint.h"
#include "svp.h"

/** Max length of a vidtex URI */
#define VIDTEX_MAX_URI  256

/** Test option - create + destroy EGL image every frame */
#define VIDTEX_OPT_IMG_PER_FRAME  (1 << 0)

/** Test option - display the Y' plane as greyscale texture */
#define VIDTEX_OPT_Y_TEXTURE  (1 << 1)

/** Test option - display the U plane as greyscale texture */
#define VIDTEX_OPT_U_TEXTURE  (1 << 2)

/** Test option - display the V plane as greyscale texture */
#define VIDTEX_OPT_V_TEXTURE  (1 << 3)

/** Parameters to vidtex run.
 * N.B. Types need to be bitwise copyable, and between C and C++, so don't use pointers, bool
 *      or anything else liable for problems.
 */
typedef struct VIDTEX_PARAMS_T
{
   /** Duration of playback, in milliseconds. 0 = default, which is full duration for media and
    * a limited duration for camera.
    */
   uint32_t duration_ms;

   /** Media URI, or empty string to use camera preview */
   char uri[VIDTEX_MAX_URI];

   /** Test options; bitmask of VIDTEX_OPT_XXX values */
   uint32_t opts;
} VIDTEX_PARAMS_T;

/** Run video-on-texture.
 * @param params  Parameters to video-on-texture display.
 * @param win     Native window handle.
 * @return 0 on success; -1 on failure.
 *         It is considered successful if the video decode completed without error and at least
 *         one EGL image was shown.
 */
int vidtex_run(const VIDTEX_PARAMS_T *params, EGLNativeWindowType win);

#ifdef __cplusplus
}
#endif

#endif
