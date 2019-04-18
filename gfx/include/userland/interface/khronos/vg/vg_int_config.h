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

#ifndef VG_INT_CONFIG_H
#define VG_INT_CONFIG_H

#include "interface/khronos/include/VG/vgext.h"
#include "middleware/khronos/vg/2708/vg_config_filler_4.h" /* should #define VG_CONFIG_RENDERER */

#define VG_CONFIG_VENDOR "Broadcom"
/* VG_CONFIG_RENDERER is platform-specific */
#if VG_KHR_EGL_image
   #define VG_CONFIG_EXTENSIONS_KHR_EGL_IMAGE "VG_KHR_EGL_image "
#else
   #define VG_CONFIG_EXTENSIONS_KHR_EGL_IMAGE ""
#endif
#define VG_CONFIG_EXTENSIONS \
   VG_CONFIG_EXTENSIONS_KHR_EGL_IMAGE

#define VG_CONFIG_SCREEN_LAYOUT              VG_PIXEL_LAYOUT_UNKNOWN
#define VG_CONFIG_MAX_SCISSOR_RECTS                               32 /* must be >= 32 */
#define VG_CONFIG_MAX_DASH_COUNT                                  16 /* must be >= 16 */
#define VG_CONFIG_MAX_KERNEL_SIZE                                 15 /* must be >= 7 */
#define VG_CONFIG_MAX_SEPARABLE_KERNEL_SIZE                       33 /* must be >= 15 */
#define VG_CONFIG_MAX_COLOR_RAMP_STOPS                            32 /* must be >= 32 */
#define VG_CONFIG_MAX_IMAGE_WIDTH                               2048 /* must be >= 256 */
#define VG_CONFIG_MAX_IMAGE_HEIGHT                              2048 /* must be >= 256 */
#define VG_CONFIG_MAX_IMAGE_PIXELS                           4194304 /* must be >= 65536 */
#define VG_CONFIG_MAX_IMAGE_BYTES                           16777216 /* must be >= 65536 */
#define VG_CONFIG_MAX_FLOAT                            3.4028234e38f /* must be >= 10.0e10f */
#define VG_CONFIG_MAX_GAUSSIAN_STD_DEVIATION                   16.0f /* must be >= 16.0f */

#endif
