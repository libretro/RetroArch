/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Daniel De Matteis
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

#ifndef _RGL_H
#define _RGL_H

#include "../include/export/RGL/rgl.h"
#include "../include/RGL/Types.h"
#include "../include/RGL/TypeUtils.h"
#include "../include/RGL/private.h"
#include "../include/RGL/Utils.h"
#include <string.h>
#include <math.h>
#include <limits.h>

#include <Cg/CgCommon.h>

// endian swapping of the fragment uniforms, if necessary
#if RGL_ENDIAN == RGL_BIG_ENDIAN
#define SWAP_IF_BIG_ENDIAN(arg) endianSwapWordByHalf(arg)
#elif RGL_ENDIAN == RGL_LITTLE_ENDIAN
#define SWAP_IF_BIG_ENDIAN(arg) arg
#else
#error include missing for endianness
#endif

void rglPsglPlatformInit (void *data);
void rglPsglPlatformExit (void);

RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode );
RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceExtended (const void *data);
RGL_EXPORT GLfloat rglPlatformGetDeviceAspectRatio (const void *data);

#endif
