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

bool gl_add_lut(
      const char *lut_path,
      bool lut_mipmap,
      unsigned lut_filter,
      enum gfx_wrap_type lut_wrap_type,
      unsigned i, void *textures_data)
{
   struct texture_image img;
   GLuint *textures_lut                 = (GLuint*)textures_data;
   enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

   img.width         = 0;
   img.height        = 0;
   img.pixels        = NULL;
   img.supports_rgba = video_driver_supports_rgba();

   if (!image_texture_load(&img, lut_path))
   {
      RARCH_ERR("[GL]: Failed to load texture image from: \"%s\"\n",
            lut_path);
      return false;
   }

   RARCH_LOG("[GL]: Loaded texture image from: \"%s\" ...\n",
         lut_path);

   if (lut_filter == RARCH_FILTER_NEAREST)
      filter_type = TEXTURE_FILTER_NEAREST;

   if (lut_mipmap)
   {
      if (filter_type == TEXTURE_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
      else
         filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
   }

   gl_load_texture_data(
         textures_lut[i],
         lut_wrap_type,
         filter_type, 4,
         img.width, img.height,
         img.pixels, sizeof(uint32_t));
   image_texture_free(&img);

   return true;
}
