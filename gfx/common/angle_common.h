/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ANGLE_COMMON_H
#define __ANGLE_COMMON_H

#ifdef HAVE_GBM
/* presense or absense of this include makes egl.h change NativeWindowType between gbm_device* and _XDisplay* */
#include <gbm.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "../../retroarch.h"

#define EGL_PLATFORM_ANGLE_ANGLE                           0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE                      0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE         0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE         0x3205
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE               0x3209

/* currently we use ANGLE only on Windows (including UWP), that might change. */
#ifdef _WIN32
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE                 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE                0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE      0x320B
#endif

RETRO_BEGIN_DECLS

bool angle_init_context(egl_ctx_data_t* egl,
      void* display_data, EGLint* major, EGLint* minor,
      EGLint* count, const EGLint* attrib_ptr, egl_accept_config_cb_t cb);

RETRO_END_DECLS

#endif
