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

void gl_ff_vertex(const void *data)
{
#ifndef NO_GL_FF_VERTEX
   const struct gfx_coords *coords = (const struct gfx_coords*)data;

   /* Fall back to fixed function-style if needed and possible. */
   glClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);
   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
}

void gl_ff_matrix(const void *data)
{
#ifndef NO_GL_FF_MATRIX
   const math_matrix_4x4 *mat = (const math_matrix_4x4*)data;

   /* Fall back to fixed function-style if needed and possible. */
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
#endif
}

void gl_load_texture_data(GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool rgb32 = (base_size == (sizeof(uint32_t)));
   driver_t *driver = driver_get_ptr();
   GLenum wrap = gl_wrap_type_to_enum(wrap_type);

   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
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
   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (driver->gfx_use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (driver->gfx_use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

bool gl_load_luts(const struct video_shader *shader,
      GLuint *textures_lut)
{
   unsigned i;
   unsigned num_luts = min(shader->luts, GFX_MAX_TEXTURES);

   if (!shader->luts)
      return true;

   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      struct texture_image img = {0};
      enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

      RARCH_LOG("Loading texture image from: \"%s\" ...\n",
            shader->lut[i].path);

      if (!texture_image_load(&img, shader->lut[i].path))
      {
         RARCH_ERR("Failed to load texture image from: \"%s\"\n",
               shader->lut[i].path);
         return false;
      }

      if (shader->lut[i].filter == RARCH_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_NEAREST;

      if (shader->lut[i].mipmap)
      {
         if (filter_type == TEXTURE_FILTER_NEAREST)
            filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
         else
            filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
      }

      gl_load_texture_data(textures_lut[i],
            shader->lut[i].wrap,
            filter_type, 4,
            img.width, img.height,
            img.pixels, sizeof(uint32_t));
      texture_image_free(&img);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}
