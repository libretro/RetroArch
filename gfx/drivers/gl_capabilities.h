/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifndef _GL_CAPABILITIES_H
#define _GL_CAPABILITIES_H

#include <boolean.h>
#include <retro_common_api.h>

enum gl_capability_enum
{
   GL_CAPS_NONE = 0,
   GL_CAPS_EGLIMAGE,
   GL_CAPS_SYNC,
   GL_CAPS_MIPMAP,
   GL_CAPS_VAO,
   GL_CAPS_FBO,
   GL_CAPS_ARGB8,
   GL_CAPS_DEBUG,
   GL_CAPS_PACKED_DEPTH_STENCIL,
   GL_CAPS_ES2_COMPAT,
   GL_CAPS_UNPACK_ROW_LENGTH,
   GL_CAPS_FULL_NPOT_SUPPORT,
   GL_CAPS_SRGB_FBO,
   GL_CAPS_SRGB_FBO_ES3,
   GL_CAPS_FP_FBO,
   GL_CAPS_BGRA8888,
   GL_CAPS_GLES3_SUPPORTED
};

RETRO_BEGIN_DECLS

bool gl_check_error(char **error_string);

bool gl_query_core_context_in_use(void);

void gl_query_core_context_set(bool set);

void gl_query_core_context_unset(void);

bool gl_check_capability(enum gl_capability_enum enum_idx);

RETRO_END_DECLS

#endif
