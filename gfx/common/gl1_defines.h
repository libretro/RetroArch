/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifndef __GL1_DEFINES_H
#define __GL1_DEFINES_H

#include <retro_environment.h>
#include <retro_inline.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef VITA
#include <vitaGL.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

#include "../video_driver.h"

#ifdef VITA
#define GL_RGBA8                    GL_RGBA
#define GL_RGB8                     GL_RGB
#define GL_BGRA_EXT                 GL_RGBA /* Currently unsupported in vitaGL */
#define GL_CLAMP                    GL_CLAMP_TO_EDGE
#endif

#define RARCH_GL1_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL1_TEXTURE_TYPE32    GL_BGRA_EXT
#define RARCH_GL1_FORMAT32          GL_UNSIGNED_BYTE

enum gl1_flags
{
   GL1_FLAG_FULLSCREEN              = (1 << 0),
   GL1_FLAG_MENU_SIZE_CHANGED       = (1 << 1),
   GL1_FLAG_RGB32                   = (1 << 2),
   GL1_FLAG_SUPPORTS_BGRA           = (1 << 3),
   GL1_FLAG_KEEP_ASPECT             = (1 << 4),
   GL1_FLAG_SHOULD_RESIZE           = (1 << 5),
   GL1_FLAG_MENU_TEXTURE_ENABLE     = (1 << 6),
   GL1_FLAG_MENU_TEXTURE_FULLSCREEN = (1 << 7),
   GL1_FLAG_SMOOTH                  = (1 << 8),
   GL1_FLAG_MENU_SMOOTH             = (1 << 9),
   GL1_FLAG_OVERLAY_ENABLE          = (1 << 10),
   GL1_FLAG_OVERLAY_FULLSCREEN      = (1 << 11),
   GL1_FLAG_FRAME_DUPE_LOCK         = (1 << 12)
};

#endif
