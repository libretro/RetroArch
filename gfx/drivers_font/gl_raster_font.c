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

#include "../gl_common.h"
#include "../video_shader_driver.h"

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

#define MAX_MSG_LEN_CHUNK 64

typedef struct gl_raster_block {
   bool fullscreen;
   gl_coord_array_t carr;
} gl_raster_block_t;

typedef struct
{
   gl_t *gl;
   GLuint tex;
   unsigned tex_width, tex_height;

   const font_renderer_driver_t *font_driver;
   void *font_data;

   gl_raster_block_t *block;
} gl_raster_t;

static void *gl_raster_font_init_font(void *gl_data,
      const char *font_path, float font_size)
{
   unsigned width, height;
   uint8_t *tmp_buffer;
   const struct font_atlas *atlas = NULL;
   gl_raster_t *font = (gl_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl = (gl_t*)gl_data;

   if (!font_renderer_create_default(&font->font_driver,
            &font->font_data, font_path, font_size))
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

   atlas = font->font_driver->get_atlas(font->font_data);

   width = next_pow2(atlas->width);
   height = next_pow2(atlas->height);

   /* Ideally, we'd use single component textures, but the 
    * difference in ways to do that between core GL and GLES/legacy GL
    * is too great to bother going down that route. */
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
         0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   tmp_buffer = (uint8_t*)malloc(atlas->width * atlas->height * 4);

   if (tmp_buffer)
   {
      unsigned i;
      uint8_t       *dst = tmp_buffer;
      const uint8_t *src = atlas->buffer;

      for (i = 0; i < atlas->width * atlas->height; i++)
      {
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = *src++;
      }

      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, atlas->width,
            atlas->height, GL_RGBA, GL_UNSIGNED_BYTE, tmp_buffer);
      free(tmp_buffer);
   }

   font->tex_width  = width;
   font->tex_height = height;

   glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);
   return font;
}

static void gl_raster_font_free_font(void *data)
{
   gl_raster_t *font = (gl_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   glDeleteTextures(1, &font->tex);
   free(font);
}

static int get_message_width(gl_raster_t *font, const char *msg)
{
   int delta_x;
   unsigned i, msg_len_full, msg_len;

   msg_len_full   = strlen(msg);
   msg_len        = min(msg_len_full, MAX_MSG_LEN_CHUNK);

   delta_x        = 0;

   while (msg_len_full)
   {
      for (i = 0; i < msg_len; i++)
      {
         const struct font_glyph *glyph = 
            font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);
         if (!glyph)
            glyph = font->font_driver->get_glyph(font->font_data, '?'); /* Do something smarter here ... */
         if (!glyph)
            continue;

         delta_x += glyph->advance_x;
      }

      msg_len_full -= msg_len;
      msg += msg_len;
      msg_len = min(msg_len_full, MAX_MSG_LEN_CHUNK);
   }

   return delta_x;
}

static void draw_vertices(gl_t *gl, const gl_coords_t *coords)
{
   gl->shader->set_coords(coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLES, 0, coords->vertices);
}

static void render_message(gl_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y, bool align_right)
{
   int x, y, delta_x, delta_y;
   float inv_tex_size_x, inv_tex_size_y, inv_win_width, inv_win_height;
   unsigned i, msg_len_full, msg_len;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK]; 
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   gl_t *gl = font->gl;
   struct gl_coords coords;

   msg_len_full   = strlen(msg);
   msg_len        = min(msg_len_full, MAX_MSG_LEN_CHUNK);

   x              = roundf(pos_x * gl->vp.width);
   y              = roundf(pos_y * gl->vp.height);
   delta_x        = 0;
   delta_y        = 0;

   if (align_right)
      x -= get_message_width(font, msg);

   inv_tex_size_x = 1.0f / font->tex_width;
   inv_tex_size_y = 1.0f / font->tex_height;
   inv_win_width  = 1.0f / font->gl->vp.width;
   inv_win_height = 1.0f / font->gl->vp.height;

   while (msg_len_full)
   {
      for (i = 0; i < msg_len; i++)
      {
         int off_x, off_y, tex_x, tex_y, width, height;
         const struct font_glyph *glyph =
            font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);
         if (!glyph)
            glyph = font->font_driver->get_glyph(font->font_data, '?'); /* Do something smarter here ... */
         if (!glyph)
            continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         emit(0, 0, 1); /* Bottom-left */
         emit(1, 1, 1); /* Bottom-right */
         emit(2, 0, 0); /* Top-left */

         emit(3, 1, 0); /* Top-right */
         emit(4, 0, 0); /* Top-left */
         emit(5, 1, 1); /* Bottom-right */
#undef emit

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord = font_tex_coords;
      coords.vertex    = font_vertex;
      coords.color     = font_color;
      coords.vertices  = 6 * msg_len;
      coords.lut_tex_coord = gl->coords.lut_tex_coord;

      if (font->block)
         gl_coord_array_add(&font->block->carr, &coords, coords.vertices);
      else
         draw_vertices(gl, &coords);

      msg_len_full -= msg_len;
      msg += msg_len;
      msg_len = min(msg_len_full, MAX_MSG_LEN_CHUNK);
   }
}

static void setup_viewport(gl_raster_t *font, bool full_screen)
{
   gl_t *gl = font->gl;

   gl_set_viewport(gl, gl->win_width, gl->win_height, full_screen, false);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   glBindTexture(GL_TEXTURE_2D, font->tex);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
}

static void restore_viewport(gl_t *gl)
{
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

static void gl_raster_font_render_msg(void *data, const char *msg,
      const struct font_params *params)
{
   GLfloat x, y, scale, drop_mod;
   GLfloat color[4], color_dark[4];
   int drop_x, drop_y;
   bool full_screen;
   bool align_right;
   gl_t *gl = NULL;
   gl_raster_t *font = (gl_raster_t*)data;
   settings_t *settings = config_get_ptr();

   if (!font)
      return;

   gl = font->gl;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      align_right = params->align_right;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;

      color[0]    = FONT_COLOR_GET_RED(params->color) / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color) / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x           = settings->video.msg_pos_x;
      y           = settings->video.msg_pos_y;
      scale       = 1.0f;
      full_screen = false;
      align_right = false;

      color[0]    = settings->video.msg_color_r;
      color[1]    = settings->video.msg_color_g;
      color[2]    = settings->video.msg_color_b;
      color[3] = 1.0f;

      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
   }

   if (font->block)
      font->block->fullscreen = true;
   else
      setup_viewport(font, full_screen);

   if (drop_x || drop_y)
   {
      color_dark[0] = color[0] * drop_mod;
      color_dark[1] = color[1] * drop_mod;
      color_dark[2] = color[2] * drop_mod;
      color_dark[3] = color[3];

      render_message(font, msg, scale, color_dark,
            x + scale * drop_x / gl->vp.width, y + 
            scale * drop_y / gl->vp.height, align_right);
   }

   render_message(font, msg, scale, color, x, y, align_right);

   if (!font->block)
      restore_viewport(gl);
}

static const struct font_glyph *gl_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl_raster_t *font = (gl_raster_t*)data;

   if (!font)
      return NULL;
   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static void gl_flush_block(void *data)
{
   gl_raster_t       *font  = (gl_raster_t*)data;
   gl_raster_block_t *block = font->block;

   if (block->carr.coords.vertices)
   {
      setup_viewport(font, block->fullscreen);

      draw_vertices(font->gl, (gl_coords_t*)&block->carr.coords);

      restore_viewport(font->gl);
   }

   block->carr.coords.vertices = 0;
}

static void gl_end_block(void *data)
{
   gl_raster_t *font = (gl_raster_t*)data;
   gl_t *gl = font->gl;

   gl_flush_block(data);

   gl_coord_array_release(&font->block->carr);
   free(font->block);
   font->block = NULL;
}

static void gl_begin_block(void *data)
{
   gl_raster_t *font = (gl_raster_t*)data;
   unsigned i = 0;

   if (font->block)
      return;

   font->block = calloc(1, sizeof(gl_raster_block_t));
}

gl_font_renderer_t gl_raster_font = {
   gl_raster_font_init_font,
   gl_raster_font_free_font,
   gl_raster_font_render_msg,
   "GL raster",
   gl_raster_font_get_glyph,
   gl_begin_block,
   gl_flush_block,
   gl_end_block
};
