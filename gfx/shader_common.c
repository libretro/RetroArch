/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "shader_common.h"
#include "../retroarch_logger.h"

#ifdef HAVE_OPENGL
//  gl_common.c may or may not be a better location for these functions.
void gl_load_texture_data(GLuint obj, const struct texture_image *img,
      GLenum wrap, bool linear, bool mipmap)
{
   glBindTexture(GL_TEXTURE_2D, obj);

#ifdef HAVE_OPENGLES2
   wrap = GL_CLAMP_TO_EDGE;
#endif
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

   GLint filter = linear ? (mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) :
                           (mipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

#ifndef HAVE_PSGL
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32, img->width, img->height,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, img->pixels);
   if (mipmap)
   {
      glGenerateMipmap(GL_TEXTURE_2D);
   }
}

bool gl_load_luts(const struct gfx_shader *generic_shader, GLuint *lut_textures)
{
   unsigned i;
   unsigned num_luts = min(generic_shader->luts, GFX_MAX_TEXTURES);
   if (!generic_shader->luts)
      return true;

   //  Original shader_glsl.c code only generated one texture handle.  I assume
   //  it was a bug, but if not, replace num_luts with 1 when GLSL is used.
   glGenTextures(num_luts, lut_textures);
   for (i = 0; i < num_luts; i++)
   {
      RARCH_LOG("Loading texture image from: \"%s\" ...\n",
            generic_shader->lut[i].path);
      struct texture_image img = {0};
      if (!texture_image_load(generic_shader->lut[i].path, &img))
      {
         RARCH_ERR("Failed to load texture image from: \"%s\"\n", generic_shader->lut[i].path);
         return false;
      }

      gl_load_texture_data(lut_textures[i], &img,
            gl_wrap_type_to_enum(generic_shader->lut[i].wrap),
            generic_shader->lut[i].filter != RARCH_FILTER_NEAREST,
            generic_shader->lut[i].mipmap);
      texture_image_free(&img);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}
#endif // HAVE_OPENGL


