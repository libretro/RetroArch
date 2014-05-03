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

#include "../include/export/PSGL/psgl.h"
#include "../include/PSGL/Types.h"
#include "../include/PSGL/TypeUtils.h"
#include "../include/PSGL/private.h"
#include "../include/PSGL/Utils.h"
#include <string.h>
#include <math.h>
#include <limits.h>

#include <Cg/CgCommon.h>

// endian swapping of the fragment uniforms, if necessary
#define SWAP_IF_BIG_ENDIAN(arg) endianSwapWordByHalf(arg)

#ifdef __cplusplus
extern "C" {
#endif

void rglPsglPlatformInit (void *data);
void rglPsglPlatformExit (void);

RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode );
RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceExtended (const void *data);
RGL_EXPORT GLfloat rglPlatformGetDeviceAspectRatio (const void *data);


GLboolean rglPlatformBufferObjectUnmapTextureReference (void *data);
GLboolean rglpCreateBufferObject (void *data);
GLboolean rglGcmFifoReferenceInUse (void *data, GLuint reference);
GLuint rglGcmFifoPutReference (void *data);
void rglGcmGetTileRegionInfo (void *data, GLuint *address, GLuint *size);
GLboolean rglGcmTryResizeTileRegion( GLuint address, GLuint size, void *data);

int32_t rglOutOfSpaceCallback (void *data, uint32_t spaceInWords);
void rglGcmFifoGlSetRenderTarget (const void *args);
void rglCreatePushBuffer (void *data);
void rglGcmFreeTiledSurface (GLuint bufferId);

#ifdef __cplusplus
}
#endif

#endif
