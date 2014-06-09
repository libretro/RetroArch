/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../gfx_common.h"
#include "../gl_common.h"
#include "../shader_common.h"

typedef struct
{
   gl_t *gl;
   GLuint tex;
   unsigned tex_width, tex_height;

   const font_renderer_driver_t *font_driver;
   void *font_data;
} gl_raster_t;

static void *gl_init_font(void *gl_data, const char *font_path, float font_size)
{
   gl_raster_t *font = (gl_raster_t*)calloc(1, sizeof(*font));
   if (!font)
      return NULL;

   font->gl = (gl_t*)gl_data;

   if (!font_renderer_create_default(&font->font_driver, &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't init font renderer.\n");
      free(font);
      return NULL;
   }

   glGenTextures(1, &font->tex);
   glBindTexture(GL_TEXTURE_2D, font->tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   const struct font_atlas *atlas = font->font_driver->get_atlas(font->font_data);

   unsigned width = next_pow2(atlas->width);
   unsigned height = next_pow2(atlas->height);
   // Ideally, we'd use single component textures, but the difference in ways to do that between core GL and GLES/legacy GL
   // is too great to bother going down that route.
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   uint8_t *tmp_buffer = (uint8_t*)malloc(atlas->width * atlas->height * 4);
   if (tmp_buffer)
   {
      unsigned i;
      uint8_t *dst = tmp_buffer;
      const uint8_t *src = atlas->buffer;
      for (i = 0; i < atlas->width * atlas->height; i++)
      {
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = *src++;
      }
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, atlas->width, atlas->height, GL_RGBA, GL_UNSIGNED_BYTE, tmp_buffer);
      free(tmp_buffer);
   }

   font->tex_width  = width;
   font->tex_height = height;

   glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);
   return font;
}

void gl_free_font(void *data)
{
   gl_raster_t *font = (gl_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   glDeleteTextures(1, &font->tex);
   free(font);
}

#define emit(c, vx, vy) do { \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]; \
} while(0)

static void render_message(gl_raster_t *font, const char *msg, GLfloat scale, const GLfloat color[4], GLfloat pos_x, GLfloat pos_y)
{
   unsigned i;
   gl_t *gl = font->gl;

   glBindTexture(GL_TEXTURE_2D, font->tex);

#define MAX_MSG_LEN_CHUNK 64
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK]; 
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];

   unsigned msg_len_full = strlen(msg);
   unsigned msg_len = min(msg_len_full, MAX_MSG_LEN_CHUNK);

   int x = roundf(pos_x * gl->vp.width);
   int y = roundf(pos_y * gl->vp.height);
   int delta_x = 0;
   int delta_y = 0;

   float inv_tex_size_x = 1.0f / font->tex_width;
   float inv_tex_size_y = 1.0f / font->tex_height;
   float inv_win_width  = 1.0f / font->gl->vp.width;
   float inv_win_height = 1.0f / font->gl->vp.height;

   while (msg_len_full)
   {
      // Rebind shaders so attrib cache gets reset.
      if (gl->shader && gl->shader->use)
         gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

      for (i = 0; i < msg_len; i++)
      {
         const struct font_glyph *gly = font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);
         if (!gly)
            gly = font->font_driver->get_glyph(font->font_data, '?'); // Do something smarter here ...
         if (!gly)
            continue;

         int off_x  = gly->draw_offset_x;
         int off_y  = gly->draw_offset_y;
         int tex_x  = gly->atlas_offset_x;
         int tex_y  = gly->atlas_offset_y;
         int width  = gly->width;
         int height = gly->height;

         emit(0, 0, 1); // Bottom-left
         emit(1, 1, 1); // Bottom-right
         emit(2, 0, 0); // Top-left

         emit(3, 1, 0); // Top-right
         emit(4, 0, 0); // Top-left
         emit(5, 1, 1); // Bottom-right
#undef emit

         delta_x += gly->advance_x;
         delta_y -= gly->advance_y;
      }

      gl->coords.tex_coord = font_tex_coords;
      gl->coords.vertex    = font_vertex;
      gl->coords.color     = font_color;
      gl->coords.vertices  = 6 * msg_len;
      gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);
      glDrawArrays(GL_TRIANGLES, 0, 6 * msg_len);

      msg_len_full -= msg_len;
      msg += msg_len;
      msg_len = min(msg_len_full, MAX_MSG_LEN_CHUNK);
   }

   // Post - Go back to old rendering path.
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void gl_render_msg(void *data, const char *msg, const struct font_params *params)
{
   GLfloat x, y, scale, drop_mod;
   GLfloat color[4], color_dark[4];
   int drop_x, drop_y;
   bool full_screen;

   gl_raster_t *font = (gl_raster_t*)data;
   if (!font)
      return;

   gl_t *gl = font->gl;

   if (params)
   {
      x = params->x;
      y = params->y;
      scale = params->scale;
      full_screen = params->full_screen;
      drop_x = params->drop_x;
      drop_y = params->drop_y;
      drop_mod = params->drop_mod;

      color[0] = FONT_COLOR_GET_RED(params->color) / 255.0f;
      color[1] = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2] = FONT_COLOR_GET_BLUE(params->color) / 255.0f;
      color[3] = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      // If alpha is 0.0f, turn it into default 1.0f
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x = g_settings.video.msg_pos_x;
      y = g_settings.video.msg_pos_y;
      scale = 1.0f;
      full_screen = false;

      color[0] = g_settings.video.msg_color_r;
      color[1] = g_settings.video.msg_color_g;
      color[2] = g_settings.video.msg_color_b;
      color[3] = 1.0f;

      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, full_screen, false);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (drop_x || drop_y)
   {
      color_dark[0] = color[0] * drop_mod;
      color_dark[1] = color[1] * drop_mod;
      color_dark[2] = color[2] * drop_mod;
      color_dark[3] = color[3];

      render_message(font, msg, scale, color_dark,
            x + scale * drop_x / gl->vp.width, y + scale * drop_y / gl->vp.height);
   }
   render_message(font, msg, scale, color, x, y);

   glDisable(GL_BLEND);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

const gl_font_renderer_t gl_raster_font = {
   gl_init_font,
   gl_free_font,
   gl_render_msg,
   "GL raster",
};

