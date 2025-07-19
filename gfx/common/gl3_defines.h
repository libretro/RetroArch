/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2019 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
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

#ifndef __GL3_DEFINES_H
#define __GL3_DEFINES_H

#include <boolean.h>
#include <string.h>
#include <libretro.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../video_driver.h"
#include "../drivers_shader/shader_gl3.h"

RETRO_BEGIN_DECLS

#define GL_CORE_NUM_TEXTURES 4
#define GL_CORE_NUM_PBOS 4
#define GL_CORE_NUM_VBOS 256
#define GL_CORE_NUM_FENCES 8

enum gl3_flags
{
   GL3_FLAG_PBO_READBACK_ENABLE    = (1 <<  0),
   GL3_FLAG_HW_RENDER_BOTTOM_LEFT  = (1 <<  1),
   GL3_FLAG_HW_RENDER_ENABLE       = (1 <<  2),
   GL3_FLAG_USE_SHARED_CONTEXT     = (1 <<  3),
   GL3_FLAG_OVERLAY_ENABLE         = (1 <<  4),
   GL3_FLAG_OVERLAY_FULLSCREEN     = (1 <<  5),
   GL3_FLAG_MENU_TEXTURE_ENABLE    = (1 <<  6),
   GL3_FLAG_MENU_TEXTURE_FULLSCREEN= (1 <<  7),
   GL3_FLAG_VSYNC                  = (1 <<  8),
   GL3_FLAG_FULLSCREEN             = (1 <<  9),
   GL3_FLAG_QUITTING               = (1 << 10),
   GL3_FLAG_SHOULD_RESIZE          = (1 << 11),
   GL3_FLAG_KEEP_ASPECT            = (1 << 12),
   GL3_FLAG_FRAME_DUPE_LOCK        = (1 << 13)
};

RETRO_END_DECLS

#endif
