/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <gfx/gl_capabilities.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "gl_common.h"

static void gl_size_format(GLint* internalFormat)
{
#ifndef HAVE_PSGL
   switch (*internalFormat)
   {
      case GL_RGB:
         /* FIXME: PS3 does not support this, neither does it have GL_RGB565_OES. */
         *internalFormat = GL_RGB565;
         break;
      case GL_RGBA:
#ifdef HAVE_OPENGLES2
         *internalFormat = GL_RGBA8_OES;
#else
         *internalFormat = GL_RGBA8;
#endif
         break;
   }
#endif
}

/* This function should only be used without mipmaps
   and when data == NULL */
void gl_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data)
{
#ifndef HAVE_PSGL
#ifdef HAVE_OPENGLES2
   if (gl_check_capability(GL_CAPS_TEX_STORAGE_EXT) && internalFormat != GL_BGRA_EXT)
   {
      gl_size_format(&internalFormat);
      glTexStorage2DEXT(target, 1, internalFormat, width, height);
   }
#else
   if (gl_check_capability(GL_CAPS_TEX_STORAGE) && internalFormat != GL_BGRA_EXT)
   {
      gl_size_format(&internalFormat);
      glTexStorage2D(target, 1, internalFormat, width, height);
   }
#endif
   else
#endif
   {
#ifdef HAVE_OPENGLES
      if (gl_check_capability(GL_CAPS_GLES3_SUPPORTED))
#endif
         gl_size_format(&internalFormat);
      glTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
   }
}
