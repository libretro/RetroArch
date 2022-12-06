/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdlib.h>

#include <encodings/utf.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include "../common/gl3_common.h"
#include "../font_driver.h"
#include "../../configuration.h"

/* TODO: Move viewport side effects to the caller: it's a source of bugs. */

#define GL_CORE_RASTER_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]

#define MAX_MSG_LEN_CHUNK 64

typedef struct
{
   gl3_t *gl;
   GLuint tex;

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;
} gl3_raster_t;

static void gl3_raster_font_free(void *data,
      bool is_threaded)
{
   gl3_raster_t *font = (gl3_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
      if (
            font->gl && 
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(true);

   glDeleteTextures(1, &font->tex);

   free(font);
}

static bool gl3_raster_font_upload_atlas(gl3_raster_t *font)
{
   if (font->tex)
      glDeleteTextures(1, &font->tex);
   glGenTextures(1, &font->tex);
   glBindTexture(GL_TEXTURE_2D, font->tex);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, font->atlas->width, font->atlas->height);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   font->atlas->width, font->atlas->height, GL_RED, GL_UNSIGNED_BYTE, font->atlas->buffer);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glBindTexture(GL_TEXTURE_2D, 0);

   return true;
}

static void *gl3_raster_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gl3_raster_t *font = (gl3_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl = (gl3_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
            font->gl && 
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(false);

   font->atlas      = font->font_driver->get_atlas(font->font_data);

   if (!gl3_raster_font_upload_atlas(font))
      goto error;

   font->atlas->dirty = false;
   return font;

error:
   gl3_raster_font_free(font, is_threaded);
   return NULL;
}

static int gl3_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   gl3_raster_t *font   = (gl3_raster_t*)data;
   const char* msg_end = msg + msg_len;
   int delta_x         = 0;

   if (     !font
         || !font->font_driver
         || !font->font_driver->get_glyph
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code                  = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
            font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void gl3_raster_font_draw_vertices(gl3_raster_t *font,
      const video_coords_t *coords)
{
   if (font->atlas->dirty)
   {
      gl3_raster_font_upload_atlas(font);
      font->atlas->dirty   = false;
   }

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, font->tex);

   if (  font->gl &&
         font->gl->pipelines.font_loc.flat_ubo_vertex >= 0)
      glUniform4fv(font->gl->pipelines.font_loc.flat_ubo_vertex,
                   4, font->gl->mvp_no_rot.data);

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   gl3_bind_scratch_vbo(font->gl, coords->vertex, 2 * sizeof(float) * coords->vertices);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)(uintptr_t)0);

   gl3_bind_scratch_vbo(font->gl, coords->tex_coord, 2 * sizeof(float) * coords->vertices);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)(uintptr_t)0);

   gl3_bind_scratch_vbo(font->gl, coords->color, 4 * sizeof(float) * coords->vertices);
   glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(uintptr_t)0);

   glDrawArrays(GL_TRIANGLES, 0, coords->vertices);

   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void gl3_raster_font_render_line(gl3_t *gl,
      gl3_raster_t *font, const char *msg, size_t msg_len,
      GLfloat scale, const GLfloat color[4], GLfloat pos_x,
      GLfloat pos_y, unsigned text_align)
{
   int i;
   struct video_coords coords;
   const struct font_glyph* glyph_q = NULL;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   const char* msg_end  = msg + msg_len;
   int x                = roundf(pos_x * gl->vp.width);
   int y                = roundf(pos_y * gl->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;
   float inv_tex_size_x = 1.0f / font->atlas->width;
   float inv_tex_size_y = 1.0f / font->atlas->height;
   float inv_win_width  = 1.0f / gl->vp.width;
   float inv_win_height = 1.0f / gl->vp.height;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gl3_raster_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= gl3_raster_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      i = 0;
      while ((i < MAX_MSG_LEN_CHUNK) && (msg < msg_end))
      {
         const struct font_glyph *glyph;
         int off_x, off_y, tex_x, tex_y, width, height;
         unsigned                  code = utf8_walk(&msg);

         /* Do something smarter here ... */
         if (!(glyph = font->font_driver->get_glyph(
               font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         GL_CORE_RASTER_FONT_EMIT(0, 0, 1); /* Bottom-left */
         GL_CORE_RASTER_FONT_EMIT(1, 1, 1); /* Bottom-right */
         GL_CORE_RASTER_FONT_EMIT(2, 0, 0); /* Top-left */

         GL_CORE_RASTER_FONT_EMIT(3, 1, 0); /* Top-right */
         GL_CORE_RASTER_FONT_EMIT(4, 0, 0); /* Top-left */
         GL_CORE_RASTER_FONT_EMIT(5, 1, 1); /* Bottom-right */

         i++;

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord     = font_tex_coords;
      coords.vertex        = font_vertex;
      coords.color         = font_color;
      coords.vertices      = i * 6;
      coords.lut_tex_coord = font_tex_coords;

      if (font->block)
         video_coord_array_append(&font->block->carr, &coords, coords.vertices);
      else
         gl3_raster_font_draw_vertices(font, &coords);
   }
}

static void gl3_raster_font_render_message(
      gl3_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y,
      unsigned text_align)
{
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      gl3_raster_font_render_line(font->gl, font,
            msg, strlen(msg), scale, color, pos_x,
            pos_y, text_align);
      return;
   }

   line_height = line_metrics->height * scale / font->gl->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim
         ? (delim - msg) : strlen(msg);

      /* Draw the line */
      gl3_raster_font_render_line(font->gl, font,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gl3_raster_font_setup_viewport(unsigned width, unsigned height,
      gl3_raster_t *font, bool full_screen)
{
   video_driver_set_viewport(width, height, full_screen, false);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);
   if (font->gl)
      glUseProgram(font->gl->pipelines.font);
}

static void gl3_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false ;
   gl3_raster_t           *font = (gl3_raster_t*)data;
   unsigned width                   = font->gl->video_width;
   unsigned height                  = font->gl->video_height;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x                    = video_msg_pos_x;
      y                    = video_msg_pos_y;
      scale                = 1.0f;
      full_screen          = true;
      text_align           = TEXT_ALIGN_LEFT;

      color[0]             = video_msg_color_r;
      color[1]             = video_msg_color_g;
      color[2]             = video_msg_color_b;
      color[3]             = 1.0f;

      drop_x               = -2;
      drop_y               = -2;
      drop_mod             = 0.3f;
      drop_alpha           = 1.0f;
   }

   if (font->block)
      font->block->fullscreen = full_screen;
   else
      gl3_raster_font_setup_viewport(width, height, font, full_screen);

   if (font->gl)
   {
      if (!string_is_empty(msg)
            && font->font_data  && font->font_driver)
      {
         if (drop_x || drop_y)
         {
            GLfloat color_dark[4];

            color_dark[0] = color[0] * drop_mod;
            color_dark[1] = color[1] * drop_mod;
            color_dark[2] = color[2] * drop_mod;
            color_dark[3] = color[3] * drop_alpha;

            gl3_raster_font_render_message(font, msg, scale, color_dark,
                  x + scale * drop_x / font->gl->vp.width, y +
                      scale * drop_y / font->gl->vp.height, text_align);
         }

         gl3_raster_font_render_message(font, msg, scale, color,
               x, y, text_align);
      }

      if (!font->block)
      {
         glDisable(GL_BLEND);
         video_driver_set_viewport(width, height, false, true);
      }
   }
}

static const struct font_glyph *gl3_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl3_raster_t *font = (gl3_raster_t*)data;
   if (font && font->font_driver && font->font_driver->ident)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void gl3_raster_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   gl3_raster_t          *font       = (gl3_raster_t*)data;
   video_font_raster_block_t *block = font ? font->block : NULL;

   if (!font || !block || !block->carr.coords.vertices)
      return;

   gl3_raster_font_setup_viewport(width, height, font, block->fullscreen);
   gl3_raster_font_draw_vertices(font, (video_coords_t*)&block->carr.coords);

   if (font->gl)
   {
      glDisable(GL_BLEND);
      video_driver_set_viewport(width, height, block->fullscreen, true);
   }
}

static void gl3_raster_font_bind_block(void *data, void *userdata)
{
   gl3_raster_t                *font = (gl3_raster_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static bool gl3_raster_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   gl3_raster_t *font   = (gl3_raster_t*)data;
   if (font && font->font_driver && font->font_data)
      return font->font_driver->get_line_metrics(font->font_data, metrics);
   return false;
}

font_renderer_t gl3_raster_font = {
   gl3_raster_font_init,
   gl3_raster_font_free,
   gl3_raster_font_render_msg,
   "gl3_raster_font",
   gl3_raster_font_get_glyph,
   gl3_raster_font_bind_block,
   gl3_raster_font_flush_block,
   gl3_raster_font_get_message_width,
   gl3_raster_font_get_line_metrics
};
