/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

extern void gl_load_texture_data(uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size);

void gl_ff_vertex(const struct gfx_coords *coords)
{
#ifndef NO_GL_FF_VERTEX
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

void gl_ff_matrix(const math_matrix_4x4 *mat)
{
#ifndef NO_GL_FF_MATRIX
   math_matrix_4x4 ident;

   /* Fall back to fixed function-style if needed and possible. */
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   matrix_4x4_identity(&ident);
   glLoadMatrixf(ident.data);
#endif
}

bool gl_load_luts(const struct video_shader *shader,
      GLuint *textures_lut)
{
   unsigned i;
   unsigned num_luts = MIN(shader->luts, GFX_MAX_TEXTURES);

   if (!shader->luts)
      return true;

   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      struct texture_image img = {0};
      enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

      RARCH_LOG("Loading texture image from: \"%s\" ...\n",
            shader->lut[i].path);

      if (!video_texture_image_load(&img, shader->lut[i].path))
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
      video_texture_image_free(&img);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}
