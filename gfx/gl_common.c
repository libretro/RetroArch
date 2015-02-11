/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "gl_common.h"

void gl_load_texture_data(GLuint id,
      const struct texture_image *img,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type)
{
   GLint mag_filter, min_filter;
   GLenum wrap;
   bool want_mipmap = false;

   glBindTexture(GL_TEXTURE_2D, id);
   
   wrap = driver.video->wrap_type_to_enum(wrap_type);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_LINEAR;
         mag_filter = GL_LINEAR;
#ifndef HAVE_PSGL
         want_mipmap = true;
#endif
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
#ifndef HAVE_PSGL
         want_mipmap = true;
#endif
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

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

#ifndef HAVE_PSGL
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0,
         driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         img->width, img->height, 0,
         driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, img->pixels);

   if (want_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

bool gl_load_luts(const struct video_shader *generic_shader,
      GLuint *textures_lut)
{
   unsigned i;
   unsigned num_luts = min(generic_shader->luts, GFX_MAX_TEXTURES);

   if (!generic_shader->luts)
      return true;

   /*  Original shader_glsl.c code only generated one 
    *  texture handle.  I assume it was a bug, but if not, 
    *  replace num_luts with 1 when GLSL is used. */
   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      struct texture_image img = {0};
      enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

      RARCH_LOG("Loading texture image from: \"%s\" ...\n",
            generic_shader->lut[i].path);

      if (!texture_image_load(&img, generic_shader->lut[i].path))
      {
         RARCH_ERR("Failed to load texture image from: \"%s\"\n",
               generic_shader->lut[i].path);
         return false;
      }

      if (generic_shader->lut[i].filter == RARCH_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_NEAREST;

      if (generic_shader->lut[i].mipmap)
      {
         if (filter_type == TEXTURE_FILTER_NEAREST)
            filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
         else
            filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
      }

      gl_load_texture_data(textures_lut[i], &img,
            generic_shader->lut[i].wrap,
            filter_type);
      texture_image_free(&img);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}
