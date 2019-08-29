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

#include "../common/gl_common.h"
#include "../font_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/* TODO: Move viewport side effects to the caller: it's a source of bugs. */

#define gl_raster_font_emit(c, vx, vy) do { \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 0] = gl->coords.lut_tex_coord[0]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 1] = gl->coords.lut_tex_coord[1]; \
} while(0)

#define MAX_MSG_LEN_CHUNK 64

typedef struct
{
   gl_t *gl;
   GLuint tex;
   unsigned tex_width, tex_height;

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;
} gl_raster_t;

static void gl_raster_font_free_font(void *data,
      bool is_threaded)
{
   gl_raster_t *font = (gl_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
   {
      if (
            font->gl && 
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(true);
   }

   if (font->tex)
   {
      glDeleteTextures(1, &font->tex);
      font->tex = 0;
   }

   free(font);
}

#if 0
static bool gl_raster_font_upload_atlas_components_4(gl_raster_t *font)
{
   unsigned i, j;
   GLint  gl_internal                   = GL_RGBA;
   GLenum gl_format                     = GL_RGBA;
   size_t ncomponents                   = 4;
   uint8_t       *tmp                   = NULL;

   tmp = (uint8_t*)calloc(font->tex_height, font->tex_width * ncomponents);

   for (i = 0; i < font->atlas->height; ++i)
   {
      const uint8_t *src = &font->atlas->buffer[i * font->atlas->width];
      uint8_t       *dst = &tmp[i * font->tex_width * ncomponents];

      for (j = 0; j < font->atlas->width; ++j)
      {
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = 0xff;
         *dst++ = *src++;
      }
      break;
   }

   glTexImage2D(GL_TEXTURE_2D, 0, gl_internal, font->tex_width, font->tex_height,
         0, gl_format, GL_UNSIGNED_BYTE, tmp);

   free(tmp);

   return true;
}
#endif

static bool gl_raster_font_upload_atlas(gl_raster_t *font)
{
   unsigned i, j;
   GLint  gl_internal                   = GL_LUMINANCE_ALPHA;
   GLenum gl_format                     = GL_LUMINANCE_ALPHA;
   size_t ncomponents                   = 2;
   uint8_t       *tmp                   = NULL;
#if defined(GL_VERSION_3_0)
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();

    if (font->gl->core_context_in_use ||
        (hwr->context_type == RETRO_HW_CONTEXT_OPENGL &&
         hwr->version_major >= 3))
   {
      GLint swizzle[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

      gl_internal = GL_R8;
      gl_format   = GL_RED;
      ncomponents = 1;
   }
#endif

   tmp = (uint8_t*)calloc(font->tex_height, font->tex_width * ncomponents);

   switch (ncomponents)
   {
      case 1:
         for (i = 0; i < font->atlas->height; ++i)
         {
            const uint8_t *src = &font->atlas->buffer[i * font->atlas->width];
            uint8_t       *dst = &tmp[i * font->tex_width * ncomponents];

            memcpy(dst, src, font->atlas->width);
         }
         break;
      case 2:
         for (i = 0; i < font->atlas->height; ++i)
         {
            const uint8_t *src = &font->atlas->buffer[i * font->atlas->width];
            uint8_t       *dst = &tmp[i * font->tex_width * ncomponents];

            for (j = 0; j < font->atlas->width; ++j)
            {
               *dst++ = 0xff;
               *dst++ = *src++;
            }
         }
         break;
   }

   glTexImage2D(GL_TEXTURE_2D, 0, gl_internal, font->tex_width, font->tex_height,
         0, gl_format, GL_UNSIGNED_BYTE, tmp);

   free(tmp);

   return true;
}

static void *gl_raster_font_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gl_raster_t   *font  = (gl_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl = (gl_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
            font->gl && 
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(false);

   glGenTextures(1, &font->tex);

   gl_bind_texture(font->tex, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);

   font->atlas      = font->font_driver->get_atlas(font->font_data);
   font->tex_width  = next_pow2(font->atlas->width);
   font->tex_height = next_pow2(font->atlas->height);

   if (!gl_raster_font_upload_atlas(font))
      goto error;

   font->atlas->dirty = false;

   glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

   return font;

error:
   gl_raster_font_free_font(font, is_threaded);
   font = NULL;

   return NULL;
}

static int gl_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   gl_raster_t *font   = (gl_raster_t*)data;
   const char* msg_end = msg + msg_len;
   int delta_x         = 0;

   if (     !font
         || !font->font_driver
         || !font->font_driver->get_glyph
         || !font->font_data )
      return 0;

   while (msg < msg_end)
   {
      unsigned code                  = utf8_walk(&msg);
      const struct font_glyph *glyph = font->font_driver->get_glyph(
            font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (!glyph)
         continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void gl_raster_font_draw_vertices(gl_raster_t *font,
      const video_coords_t *coords,
      video_frame_info_t *video_info)
{
   if (font->atlas->dirty)
   {
      gl_raster_font_upload_atlas(font);
      font->atlas->dirty   = false;
   }

   font->gl->shader->set_coords(font->gl->shader_data, coords);
   font->gl->shader->set_mvp(font->gl->shader_data,
         &font->gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLES, 0, coords->vertices);
}

static void gl_raster_font_render_line(
      gl_raster_t *font, const char *msg, unsigned msg_len,
      GLfloat scale, const GLfloat color[4], GLfloat pos_x,
      GLfloat pos_y, unsigned text_align,
      video_frame_info_t *video_info)
{
   unsigned i;
   struct video_coords coords;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_lut_tex_coord[2 * 6 * MAX_MSG_LEN_CHUNK];
   gl_t      *gl        = font->gl;
   const char* msg_end  = msg + msg_len;
   int x                = roundf(pos_x * gl->vp.width);
   int y                = roundf(pos_y * gl->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;
   float inv_tex_size_x = 1.0f / font->tex_width;
   float inv_tex_size_y = 1.0f / font->tex_height;
   float inv_win_width  = 1.0f / font->gl->vp.width;
   float inv_win_height = 1.0f / font->gl->vp.height;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gl_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= gl_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

   while (msg < msg_end)
   {
      i = 0;
      while ((i < MAX_MSG_LEN_CHUNK) && (msg < msg_end))
      {
         int off_x, off_y, tex_x, tex_y, width, height;
         unsigned                  code = utf8_walk(&msg);
         const struct font_glyph *glyph = font->font_driver->get_glyph(
               font->font_data, code);

         if (!glyph) /* Do something smarter here ... */
            glyph = font->font_driver->get_glyph(font->font_data, '?');

         if (!glyph)
            continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         gl_raster_font_emit(0, 0, 1); /* Bottom-left */
         gl_raster_font_emit(1, 1, 1); /* Bottom-right */
         gl_raster_font_emit(2, 0, 0); /* Top-left */

         gl_raster_font_emit(3, 1, 0); /* Top-right */
         gl_raster_font_emit(4, 0, 0); /* Top-left */
         gl_raster_font_emit(5, 1, 1); /* Bottom-right */

         i++;

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord     = font_tex_coords;
      coords.vertex        = font_vertex;
      coords.color         = font_color;
      coords.vertices      = i * 6;
      coords.lut_tex_coord = font_lut_tex_coord;

      if (font->block)
         video_coord_array_append(&font->block->carr, &coords, coords.vertices);
      else
         gl_raster_font_draw_vertices(font, &coords, video_info);
   }
}

static void gl_raster_font_render_message(
      gl_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y,
      unsigned text_align,
      video_frame_info_t *video_info)
{
   float line_height;
   int lines = 0;

   /* If the font height is not supported just draw as usual */
   if (!font->font_driver->get_line_height)
   {
      gl_raster_font_render_line(font,
            msg, (unsigned)strlen(msg), scale, color, pos_x,
            pos_y, text_align,
            video_info);
      return;
   }

   line_height = (float) font->font_driver->get_line_height(font->font_data) *
                     scale / font->gl->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      unsigned msg_len  = delim
         ? (unsigned)(delim - msg) : (unsigned)strlen(msg);

      /* Draw the line */
      gl_raster_font_render_line(font,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height, text_align,
            video_info);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gl_raster_font_setup_viewport(unsigned width, unsigned height,
      gl_raster_t *font, bool full_screen)
{
   video_driver_set_viewport(width, height, full_screen, false);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   glBindTexture(GL_TEXTURE_2D, font->tex);

   if (font->gl->shader && font->gl->shader->use)
      font->gl->shader->use(font->gl,
            font->gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);
}

static void gl_raster_font_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false ;
   gl_raster_t                *font = (gl_raster_t*)data;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;

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
      x                    = video_info->font_msg_pos_x;
      y                    = video_info->font_msg_pos_y;
      scale                = 1.0f;
      full_screen          = true;
      text_align           = TEXT_ALIGN_LEFT;

      color[0]             = video_info->font_msg_color_r;
      color[1]             = video_info->font_msg_color_g;
      color[2]             = video_info->font_msg_color_b;
      color[3]             = 1.0f;

      drop_x               = -2;
      drop_y               = -2;
      drop_mod             = 0.3f;
      drop_alpha           = 1.0f;
   }

   if (font->block)
      font->block->fullscreen = full_screen;
   else
      gl_raster_font_setup_viewport(width, height, font, full_screen);

   if (!string_is_empty(msg) && font->gl
         && font->font_data  && font->font_driver)
   {
      if (drop_x || drop_y)
      {
         GLfloat color_dark[4];

         color_dark[0] = color[0] * drop_mod;
         color_dark[1] = color[1] * drop_mod;
         color_dark[2] = color[2] * drop_mod;
         color_dark[3] = color[3] * drop_alpha;

         if (font->gl)
            gl_raster_font_render_message(font, msg, scale, color_dark,
                  x + scale * drop_x / font->gl->vp.width, y +
                  scale * drop_y / font->gl->vp.height, text_align,
                  video_info);
      }

      if (font->gl)
         gl_raster_font_render_message(font, msg, scale, color,
               x, y, text_align, video_info);
   }

   if (!font->block && font->gl)
   {
      /* restore viewport */
      glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

      glDisable(GL_BLEND);
      video_driver_set_viewport(width, height, false, true);
   }
}

static const struct font_glyph *gl_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl_raster_t *font = (gl_raster_t*)data;

   if (!font || !font->font_driver)
      return NULL;
   if (!font->font_driver->ident)
       return NULL;
   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static void gl_raster_font_flush_block(unsigned width, unsigned height,
      void *data, video_frame_info_t *video_info)
{
   gl_raster_t          *font       = (gl_raster_t*)data;
   video_font_raster_block_t *block = font ? font->block : NULL;

   if (!font || !block || !block->carr.coords.vertices)
      return;

   gl_raster_font_setup_viewport(width, height, font, block->fullscreen);
   gl_raster_font_draw_vertices(font, (video_coords_t*)&block->carr.coords,
         video_info);

   if (font->gl)
   {
      /* restore viewport */
      glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

      glDisable(GL_BLEND);
      video_driver_set_viewport(width, height, block->fullscreen, true);
   }
}

static void gl_raster_font_bind_block(void *data, void *userdata)
{
   gl_raster_t                *font = (gl_raster_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static int gl_get_line_height(void *data)
{
   gl_raster_t *font   = (gl_raster_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_height(font->font_data);
}

font_renderer_t gl_raster_font = {
   gl_raster_font_init_font,
   gl_raster_font_free_font,
   gl_raster_font_render_msg,
   "GL raster",
   gl_raster_font_get_glyph,
   gl_raster_font_bind_block,
   gl_raster_font_flush_block,
   gl_get_message_width,
   gl_get_line_height
};
