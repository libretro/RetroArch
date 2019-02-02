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
#if !defined(HAVE_PSGL) && !defined(ORBIS)
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
      glTexImage2D(target, level, internalFormat, width,
            height, border, format, type, data);
   }
}

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_supports_rgba();
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl_wrap_type_to_enum(wrap_type);
   GLuint id        = (GLuint)id_data;
   bool have_mipmap = gl_check_capability(GL_CAPS_MIPMAP);

   if (!have_mipmap)
   {
      /* Assume no mipmapping support. */
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
            filter_type = TEXTURE_FILTER_LINEAR;
            break;
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            filter_type = TEXTURE_FILTER_NEAREST;
            break;
         default:
            break;
      }
   }

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         mag_filter = GL_LINEAR;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   gl_bind_texture(id, wrap, mag_filter, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap && have_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}
