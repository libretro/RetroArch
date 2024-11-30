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

/* OpenGL 1.x driver.
 *
 * Minimum version : OpenGL 1.1 (1997)
 *
 * We are targeting a minimum of OpenGL 1.1 and the Microsoft
 * "GDI Generic" * software GL implementation.
 * Any additional features added for later 1.x versions should only be
 * enabled if they are detected at runtime. */

#include <stddef.h>
#include <stdlib.h>

#include <encodings/utf.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <gfx/video_frame.h>
#include <gfx/scaler/pixconv.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gl1_defines.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#ifdef VITA
#include <defines/psp_defines.h>
#endif

/* TODO: Move viewport side effects to the caller: it's a source of bugs. */

#define GL1_RASTER_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0]       = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1]       = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0]       = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1]       = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0]       = color[0]; \
   font_color[      4 * (6 * i + c) + 1]       = color[1]; \
   font_color[      4 * (6 * i + c) + 2]       = color[2]; \
   font_color[      4 * (6 * i + c) + 3]       = color[3]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 0] = gl->coords.lut_tex_coord[0]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 1] = gl->coords.lut_tex_coord[1]

#define IS_POT(x) (((x) & (x - 1)) == 0)

#define GET_POT(x) (IS_POT((x)) ? (x) : next_pow2((x)))

#define MAX_MSG_LEN_CHUNK 64

typedef struct
{
   gl1_t *gl;
   GLuint tex;
   unsigned tex_width, tex_height;

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;
} gl1_raster_t;

static const GLfloat gl1_menu_vertexes[8]    = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_menu_tex_coords[8]  = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static struct video_ortho gl1_default_ortho = {0, 1, 0, 1, -1, 1};

/* Used for the last pass when rendering to the back buffer. */
static const GLfloat gl1_vertexes_flipped[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const GLfloat gl1_vertexes[8]         = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_tex_coords[8]       = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_white_color[16]     = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

/**
 * FORWARD DECLARATIONS
 */
static void gl1_set_viewport(gl1_t *gl1,
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate);

/**
 * DISPLAY DRIVER
 */

static const float *gfx_display_gl1_get_default_vertices(void)
{
   return &gl1_menu_vertexes[0];
}

static const float *gfx_display_gl1_get_default_tex_coords(void)
{
   return &gl1_menu_tex_coords[0];
}

static void *gfx_display_gl1_get_default_mvp(void *data)
{
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return NULL;

   return &gl1->mvp_no_rot;
}

static GLenum gfx_display_prim_to_gl1_enum(
      enum gfx_display_prim_type type)
{
   switch (type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case GFX_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

static void gfx_display_gl1_blend_begin(void *data)
{
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void gfx_display_gl1_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

static void gfx_display_gl1_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   const GLfloat *mvp_matrix;
   gl1_t             *gl1          = (gl1_t*)data;

   if (!gl1 || !draw)
      return;

   if (!draw->coords->vertex)
      draw->coords->vertex         = &gl1_menu_vertexes[0];
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord      = &gl1_menu_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord  = &gl1_menu_tex_coords[0];
   if (!draw->texture)
      return;

   glViewport(draw->x, draw->y, draw->width, draw->height);

   glEnable(GL_TEXTURE_2D);

   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   mvp_matrix = draw->matrix_data ? (const GLfloat*)draw->matrix_data
      : (const GLfloat*)&gl1->mvp_no_rot;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadMatrixf(mvp_matrix);

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef VITA
   {
      unsigned i;
      static float *vertices3 = NULL;

      if (vertices3)
         free(vertices3);
      vertices3 = (float*)malloc(sizeof(float) * 3 * draw->coords->vertices);
      for (i = 0; i < draw->coords->vertices; i++)
      {
         memcpy(&vertices3[i * 3],
               &draw->coords->vertex[i * 2],
               sizeof(float) * 2);
         vertices3[i * 3 + 2]  = 0.0f;
      }
      glVertexPointer(3, GL_FLOAT, 0, vertices3);
   }
#else
   glVertexPointer(2, GL_FLOAT, 0, draw->coords->vertex);
#endif

   glColorPointer(4, GL_FLOAT, 0, draw->coords->color);
   glTexCoordPointer(2, GL_FLOAT, 0, draw->coords->tex_coord);

   glDrawArrays(gfx_display_prim_to_gl1_enum(
            draw->prim_type), 0, draw->coords->vertices);

   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();

   gl1->coords.color = gl1->white_color_ptr;
}

static void gfx_display_gl1_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void gfx_display_gl1_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glScissor(0, 0, video_width, video_height);
   glDisable(GL_SCISSOR_TEST);
}

gfx_display_ctx_driver_t gfx_display_ctx_gl1 = {
   gfx_display_gl1_draw,
   NULL, /* draw_pipeline */
   gfx_display_gl1_blend_begin,
   gfx_display_gl1_blend_end,
   gfx_display_gl1_get_default_mvp,
   gfx_display_gl1_get_default_vertices,
   gfx_display_gl1_get_default_tex_coords,
   FONT_DRIVER_RENDER_OPENGL1_API,
   GFX_VIDEO_DRIVER_OPENGL1,
   "gl1",
   false,
   gfx_display_gl1_scissor_begin,
   gfx_display_gl1_scissor_end
};

/**
 * FONT DRIVER
 */

static void gl1_raster_font_free(void *data,
      bool is_threaded)
{
   gl1_raster_t *font = (gl1_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
      if (
               font->gl
            && font->gl->ctx_driver
            && font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(true);

   glDeleteTextures(1, &font->tex);

   free(font);
}

static void gl1_raster_font_upload_atlas(gl1_raster_t *font)
{
   unsigned i, j;
   GLint  gl_internal = GL_LUMINANCE_ALPHA;
   GLenum gl_format   = GL_LUMINANCE_ALPHA;
   size_t ncomponents = 2;
   uint8_t *tmp       = (uint8_t*)calloc(font->tex_height, font->tex_width * ncomponents);

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
}

static void *gl1_raster_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gl1_raster_t   *font  = (gl1_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl = (gl1_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
               font->gl
            && font->gl->ctx_driver
            && font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(false);

   glGenTextures(1, &font->tex);
   glBindTexture(GL_TEXTURE_2D, font->tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   font->atlas      = font->font_driver->get_atlas(font->font_data);
   font->tex_width  = next_pow2(font->atlas->width);
   font->tex_height = next_pow2(font->atlas->height);

   gl1_raster_font_upload_atlas(font);

   font->atlas->dirty = false;

   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

   return font;
}

static int gl1_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   gl1_raster_t *font  = (gl1_raster_t*)data;
   const char* msg_end = msg + msg_len;
   int delta_x         = 0;

   if (     !font
         || !font->font_driver
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

static void gl1_raster_font_draw_vertices(
      gl1_t *gl,
      gl1_raster_t *font,
      const video_coords_t *coords)
{
#ifdef VITA
   static float *vertices3 = NULL;
#endif

   if (font->atlas->dirty)
   {
      gl1_raster_font_upload_atlas(font);
      font->atlas->dirty   = false;
   }

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadMatrixf(gl->mvp.data);

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef VITA
   if (vertices3)
      free(vertices3);
   vertices3 = (float*)malloc(sizeof(float) * 3 * coords->vertices);
   {
      int i;
      for (i = 0; i < coords->vertices; i++)
      {
         memcpy(&vertices3[i*3], &coords->vertex[i*2], sizeof(float) * 2);
         vertices3[i*3+2] = 0.0f;
      }
   }
   glVertexPointer(3, GL_FLOAT, 0, vertices3);
#else
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
#endif

   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);

   glDrawArrays(GL_TRIANGLES, 0, coords->vertices);

   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
}

static void gl1_raster_font_render_line(gl1_t *gl,
      gl1_raster_t *font,
      const struct font_glyph* glyph_q,
      const char *msg,
      size_t msg_len,
      GLfloat scale,
      const GLfloat color[4],
      GLfloat pos_x,
      GLfloat pos_y,
      int pre_x,
      float inv_tex_size_x,
      float inv_tex_size_y,
      float inv_win_width,
      float inv_win_height,
      unsigned text_align)
{
   int i;
   struct video_coords coords;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_lut_tex_coord[2 * 6 * MAX_MSG_LEN_CHUNK];
   const char* msg_end  = msg + msg_len;
   int x                = pre_x;
   int y                = roundf(pos_y * gl->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gl1_raster_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= gl1_raster_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

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

         GL1_RASTER_FONT_EMIT(0, 0, 1); /* Bottom-left */
         GL1_RASTER_FONT_EMIT(1, 1, 1); /* Bottom-right */
         GL1_RASTER_FONT_EMIT(2, 0, 0); /* Top-left */

         GL1_RASTER_FONT_EMIT(3, 1, 0); /* Top-right */
         GL1_RASTER_FONT_EMIT(4, 0, 0); /* Top-left */
         GL1_RASTER_FONT_EMIT(5, 1, 1); /* Bottom-right */

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
         gl1_raster_font_draw_vertices(gl, font, &coords);
   }
}

static void gl1_raster_font_render_message(gl1_t *gl,
      gl1_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y,
      float inv_tex_size_x,
      float inv_tex_size_y,
      float inv_win_width,
      float inv_win_height,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   int x                                  = roundf(pos_x * gl->vp.width);
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / gl->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      gl1_raster_font_render_line(gl, font, glyph_q,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height,
            x,
            inv_tex_size_x,
            inv_tex_size_y,
            inv_win_width,
            inv_win_height,
            text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gl1_raster_font_setup_viewport(
      gl1_t *gl,
      unsigned width, unsigned height,
      gl1_raster_t *font, bool full_screen)
{
   gl1_set_viewport(gl, width, height, full_screen, false);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, font->tex);
}

static void gl1_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false;
   gl1_raster_t               *font = (gl1_raster_t*)data;
   gl1_t *gl                        = (gl1_t*)userdata;

   if (!font || string_is_empty(msg) || !gl)
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
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      full_screen             = true;
      text_align              = TEXT_ALIGN_LEFT;

      color[0]                = video_msg_color_r;
      color[1]                = video_msg_color_g;
      color[2]                = video_msg_color_b;
      color[3]                = 1.0f;

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   if (font->block)
      font->block->fullscreen = full_screen;

   {
      unsigned width          = gl->video_width;
      unsigned height         = gl->video_height;
      float inv_tex_size_x    = 1.0f / font->tex_width;
      float inv_tex_size_y    = 1.0f / font->tex_height;
      float inv_win_width     = 1.0f / gl->vp.width;
      float inv_win_height    = 1.0f / gl->vp.height;
      if (!font->block)
         gl1_raster_font_setup_viewport(gl, width, height, font, full_screen);

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

            gl1_raster_font_render_message(gl, font, msg, scale, color_dark,
                  x + scale * drop_x / gl->vp.width,
                  y + scale * drop_y / gl->vp.height,
                  inv_tex_size_x,
                  inv_tex_size_y,
                  inv_win_width,
                  inv_win_height,
                  text_align);
         }

         gl1_raster_font_render_message(gl, font, msg, scale, color,
               x, y,
               inv_tex_size_x,
               inv_tex_size_y,
               inv_win_width,
               inv_win_height,
               text_align);
      }

      if (!font->block)
      {
         /* Restore viewport */
         glEnable(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

         glDisable(GL_BLEND);
         gl1_set_viewport(gl, width, height, false, true);
      }
   }
}

static const struct font_glyph *gl1_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl1_raster_t *font = (gl1_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void gl1_raster_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   gl1_raster_t          *font       = (gl1_raster_t*)data;
   video_font_raster_block_t *block  = font ? font->block : NULL;
   gl1_t *gl                         = font ? font->gl    : NULL;

   if (!font || !block || !block->carr.coords.vertices || !gl)
      return;

   gl1_raster_font_setup_viewport(gl, width, height, font, block->fullscreen);
   gl1_raster_font_draw_vertices(gl, font, (video_coords_t*)&block->carr.coords);

   /* Restore viewport */
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
   gl1_set_viewport(gl, width, height, block->fullscreen, true);
}

static void gl1_raster_font_bind_block(void *data, void *userdata)
{
   gl1_raster_t                *font = (gl1_raster_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static bool gl1_raster_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   gl1_raster_t *font = (gl1_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t gl1_raster_font = {
   gl1_raster_font_init,
   gl1_raster_font_free,
   gl1_raster_font_render_msg,
   "gl1",
   gl1_raster_font_get_glyph,
   gl1_raster_font_bind_block,
   gl1_raster_font_flush_block,
   gl1_raster_font_get_message_width,
   gl1_raster_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

#ifdef HAVE_OVERLAY
static void gl1_render_overlay(gl1_t *gl,
      unsigned width,
      unsigned height)
{
   int i;

   glEnable(GL_BLEND);

   if (gl->flags & GL1_FLAG_OVERLAY_FULLSCREEN)
      glViewport(0, 0, width, height);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();

   for (i = 0; i < (int)gl->overlays; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glDrawArrays(GL_TRIANGLE_STRIP, 4 * i, 4);
   }

   glDisable(GL_BLEND);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   if (gl->flags & GL1_FLAG_OVERLAY_FULLSCREEN)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}

static void gl1_free_overlay(gl1_t *gl)
{
   glDeleteTextures(gl->overlays, gl->overlay_tex);

   free(gl->overlay_tex);
   free(gl->overlay_vertex_coord);
   free(gl->overlay_tex_coord);
   free(gl->overlay_color_coord);
   gl->overlay_tex          = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord    = NULL;
   gl->overlay_color_coord  = NULL;
   gl->overlays             = 0;
}

static void gl1_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl1_t *gl        = (gl1_t*)data;

   if (!gl)
      return;

   if (image > gl->overlays)
   {
      RARCH_ERR("[GL]: Invalid overlay id: %u\n", image);
      return;
   }

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl1_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl1_t *gl     = (gl1_t*)data;

   if (!gl)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}
#endif

static void *gl1_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
#ifdef VITA
   static bool vgl_inited               = false;
#endif
   void *ctx_data                       = NULL;
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   unsigned mode_width                  = 0;
   unsigned mode_height                 = 0;
   unsigned win_width = 0, win_height   = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   bool video_smooth                    = settings->bools.video_smooth;
   bool video_font_enable               = settings->bools.video_font_enable;
   const char *video_context_driver     = settings->arrays.video_context_driver;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   const char *extensions               = NULL;
   int interval                         = 0;
   struct retro_hw_render_callback *hwr = NULL;
   gl1_t *gl1                           = (gl1_t*)calloc(1, sizeof(*gl1));

   if (!gl1)
      return NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   gl1->video_width                     = video->width;
   gl1->video_height                    = video->height;

   if (video->rgb32)
   {
      gl1->video_bits                   = 32;
      gl1->video_pitch                  = video->width * 4;
      gl1->flags                       |= GL1_FLAG_RGB32;
   }
   else
   {
      gl1->video_bits                   = 16;
      gl1->video_pitch                  = video->width * 2;
   }

   ctx_driver = video_context_driver_init_first(gl1,
         video_context_driver,
         GFX_CTX_OPENGL_API, 1, 1, false, &ctx_data);

   if (!ctx_driver)
      goto error;

   if (ctx_data)
      gl1->ctx_data = ctx_data;

   gl1->ctx_driver  = ctx_driver;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[GL1]: Found GL1 context: \"%s\".\n", ctx_driver->ident);

   if (gl1->ctx_driver->get_video_size)
      gl1->ctx_driver->get_video_size(gl1->ctx_data,
               &mode_width, &mode_height);

#if defined(__APPLE__) && !defined(IOS)
   /* This is a hack for now to work around a very annoying
    * issue that currently eludes us. */
   if (     !gl1->ctx_driver->set_video_mode
         || !gl1->ctx_driver->set_video_mode(gl1->ctx_data,
            win_width, win_height, video->fullscreen))
      goto error;
#endif

   full_x      = mode_width;
   full_y      = mode_height;
   mode_width  = 0;
   mode_height = 0;
#ifdef VITA
   if (!vgl_inited)
   {
      vglInitExtended(0x1400000, full_x, full_y, RAM_THRESHOLD, SCE_GXM_MULTISAMPLE_4X);
      vglUseVram(GL_TRUE);
      vgl_inited = true;
   }
#endif
   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   RARCH_LOG("[GL1]: Detecting screen resolution: %ux%u.\n", full_x, full_y);
   win_width       = video->width;
   win_height      = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width    = full_x;
      win_height   = full_y;
   }

   mode_width      = win_width;
   mode_height     = win_height;

   interval        = video->swap_interval;

   if (ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval                 = -1;
      ctx_driver->swap_interval(gl1->ctx_data, interval);
   }

   if (     !gl1->ctx_driver->set_video_mode
         || !gl1->ctx_driver->set_video_mode(gl1->ctx_data,
            win_width, win_height, video->fullscreen))
      goto error;

   if (video->fullscreen)
      gl1->flags |= GL1_FLAG_FULLSCREEN;

   mode_width     = 0;
   mode_height    = 0;

   if (gl1->ctx_driver->get_video_size)
      gl1->ctx_driver->get_video_size(gl1->ctx_data,
               &mode_width, &mode_height);

   temp_width     = mode_width;
   temp_height    = mode_height;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("[GL1]: Using resolution %ux%u.\n", temp_width, temp_height);

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);
   extensions = (const char*)glGetString(GL_EXTENSIONS);

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl1->version_major, &gl1->version_minor);

   if (!string_is_empty(extensions))
      gl1->extensions = string_split(extensions, " ");

   RARCH_LOG("[GL1]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL1]: Version: %s.\n", version);
   RARCH_LOG("[GL1]: Extensions: %s\n", extensions);

   if (!string_is_empty(version))
      video_driver_set_gpu_api_version_string(version);

   if (gl1->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      gl1->ctx_driver->input_driver(
            gl1->ctx_data, joypad_name,
            input, input_data);
   }

   if (video_font_enable)
      font_driver_init_osd(gl1,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL1_API);

   if (video_smooth)
      gl1->flags     |= GL1_FLAG_SMOOTH;
   if (string_list_find_elem(gl1->extensions, "GL_EXT_bgra"))
      gl1->flags     |= GL1_FLAG_SUPPORTS_BGRA;

   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif
   glGenTextures(1, &gl1->tex);
   glGenTextures(1, &gl1->menu_tex);

   hwr = video_driver_get_hw_context();

   memcpy(gl1->tex_info.coord, gl1_tex_coords, sizeof(gl1->tex_info.coord));
   gl1->vertex_ptr            = hwr->bottom_left_origin
                              ? gl1_vertexes
                              : gl1_vertexes_flipped;
   gl1->textures              = 4;
   gl1->white_color_ptr       = gl1_white_color;
   gl1->coords.vertex         = gl1->vertex_ptr;
   gl1->coords.tex_coord      = gl1->tex_info.coord;
   gl1->coords.color          = gl1->white_color_ptr;
   gl1->coords.lut_tex_coord  = gl1_tex_coords;
   gl1->coords.vertices       = 4;

   return gl1;

error:
   video_context_driver_free();
   if (gl1)
   {
      if (gl1->extensions)
         string_list_free(gl1->extensions);
      free(gl1);
   }
   return NULL;
}

static void gl1_set_projection(gl1_t *gl1,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(gl1->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl1->mvp = gl1->mvp_no_rot;
      return;
   }

   radians                 = M_PI * gl1->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(gl1->mvp, rot, gl1->mvp_no_rot);
}

static void gl1_set_viewport(gl1_t *gl1,
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate)
{
   settings_t *settings     = config_get_ptr();
   unsigned height          = gl1->video_height;
   int x                    = 0;
   int y                    = 0;
   float device_aspect      = (float)viewport_width / viewport_height;

   if (gl1->ctx_driver->translate_aspect)
      device_aspect         = gl1->ctx_driver->translate_aspect(
            gl1->ctx_data, viewport_width, viewport_height);

   if (settings->bools.video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl1->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(),
            gl1->flags & GL1_FLAG_KEEP_ASPECT, false);
      viewport_width  = gl1->vp.width;
      viewport_height = gl1->vp.height;
   }
   else if ((gl1->flags & GL1_FLAG_KEEP_ASPECT) && !force_full)
   {
      gl1->vp.full_height = gl1->video_height;
      video_viewport_get_scaled_aspect2(&gl1->vp, viewport_width, viewport_height, false, device_aspect, video_driver_get_aspect_ratio());
      viewport_width  = gl1->vp.width;
      viewport_height = gl1->vp.height;
   }
   else
   {
      gl1->vp.x      = gl1->vp.y = 0;
      gl1->vp.width  = viewport_width;
      gl1->vp.height = viewport_height;
   }

   glViewport(gl1->vp.x, gl1->vp.y, gl1->vp.width, gl1->vp.height);
   gl1_set_projection(gl1, &gl1_default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl1->vp_out_width  = viewport_width;
      gl1->vp_out_height = viewport_height;
   }
}

static void gl1_draw_tex(gl1_t *gl1, int pot_width, int pot_height, int width, int height, GLuint tex, const void *frame_to_copy)
{
   uint8_t *frame         = NULL;
   uint8_t *frame_rgba    = NULL;
   /* FIXME: For now, everything is uploaded as BGRA8888, I could not get 444 or 555 to work, and there is no 565 support in GL 1.1 either. */
   GLint internalFormat   = GL_RGB8;
   bool   supports_native = gl1->flags & GL1_FLAG_SUPPORTS_BGRA;
   GLenum format          = supports_native ? GL_BGRA_EXT : GL_RGBA;
#ifdef MSB_FIRST
   GLenum type            = supports_native ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE;
#else
   GLenum type            = GL_UNSIGNED_BYTE;
#endif
   float vertices[]       = {
      -1.0f, -1.0f, 0.0f,
      -1.0f,  1.0f, 0.0f,
       1.0f, -1.0f, 0.0f,
       1.0f,  1.0f, 0.0f,
   };

   float colors[]         = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };

   float norm_width       = (1.0f / (float)pot_width) * (float)width;
   float norm_height      = (1.0f / (float)pot_height) * (float)height;

   float texcoords[]      = {
      0.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, 0.0f
   };

   texcoords[1] = texcoords[5] = norm_height;
   texcoords[4] = texcoords[6] = norm_width;

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
   glEnable(GL_TEXTURE_2D);

   /* Multi-texture not part of GL 1.1 */
   /*glActiveTexture(GL_TEXTURE0);*/

#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, pot_width);
#endif
   glBindTexture(GL_TEXTURE_2D, tex);

   frame = (uint8_t*)frame_to_copy;

   if (!supports_native)
   {
      frame_rgba = (uint8_t*)malloc(pot_width * pot_height * 4);
      if (frame_rgba)
      {
         int x, y;
         for (y = 0; y < pot_height; y++)
         {
            for (x = 0; x < pot_width; x++)
            {
               int index             = (y * pot_width + x) * 4;
#ifdef MSB_FIRST
               frame_rgba[index + 2] = frame[index + 3];
               frame_rgba[index + 1] = frame[index + 2];
               frame_rgba[index + 0] = frame[index + 1];
               frame_rgba[index + 3] = frame[index + 0];
#else
               frame_rgba[index + 2] = frame[index + 0];
               frame_rgba[index + 1] = frame[index + 1];
               frame_rgba[index + 0] = frame[index + 2];
               frame_rgba[index + 3] = frame[index + 3];
#endif
            }
         }
         frame = frame_rgba;
      }
   }

   glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, pot_width, pot_height, 0, format, type, frame);
   if (frame_rgba)
       free(frame_rgba);

   if (tex == gl1->tex)
   {
      if (gl1->flags & GL1_FLAG_SMOOTH)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      else
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }
   }
   else if (tex == gl1->menu_tex)
   {
      if (gl1->flags & GL1_FLAG_MENU_SMOOTH)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      else
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   if (gl1->rotation && tex == gl1->tex)
      glRotatef(gl1->rotation, 0.0f, 0.0f, 1.0f);

   glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glColorPointer(4, GL_FLOAT, 0, colors);
   glVertexPointer(3, GL_FLOAT, 0, vertices);
   glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
}

static void gl1_readback(gl1_t *gl1,
      unsigned alignment, unsigned fmt, unsigned type,
      unsigned video_width, unsigned video_height,
      void *src)
{
#ifndef VITA
   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);
#endif

   glReadPixels(
         (gl1->vp.x > 0) ? gl1->vp.x : 0,
         (gl1->vp.y > 0) ? gl1->vp.y : 0,
         (gl1->vp.width  > video_width)  ? video_width  : gl1->vp.width,
         (gl1->vp.height > video_height) ? video_height : gl1->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
}

static bool gl1_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   const void *frame_to_copy        = NULL;
   unsigned mode_width              = 0;
   unsigned mode_height             = 0;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;
   bool draw                        = true;
   bool do_swap                     = false;
   gl1_t *gl1                       = (gl1_t*)data;
   unsigned bits                    = gl1->video_bits;
   unsigned pot_width               = 0;
   unsigned pot_height              = 0;
   unsigned video_width             = video_info->width;
   unsigned video_height            = video_info->height;
   int bfi_light_frames;
   unsigned n;
#ifdef HAVE_MENU
   bool menu_is_alive               = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active              = video_info->widgets_active;
#endif
   bool hard_sync                   = video_info->hard_sync;
   struct font_params *osd_params   = (struct font_params*)
      &video_info->osd_stat_params;
   bool overlay_behind_menu         = video_info->overlay_behind_menu;

   /* FIXME: Force these settings off as they interfere with the rendering */
   video_info->xmb_shadows_enable   = false;
   video_info->menu_shader_pipeline = 0;

   if (gl1->flags & GL1_FLAG_SHOULD_RESIZE)
   {
      gfx_ctx_mode_t mode;

      gl1->flags       &= ~GL1_FLAG_SHOULD_RESIZE;

      mode.width        = width;
      mode.height       = height;

      if (gl1->ctx_driver->set_resize)
         gl1->ctx_driver->set_resize(gl1->ctx_data,
               mode.width, mode.height);

      gl1_set_viewport(gl1,
            video_width, video_height, false, true);
   }

   if (     !frame
         || (frame == RETRO_HW_FRAME_BUFFER_VALID)
         || (
            (frame_width  == 4)
         && (frame_height == 4)
         && (frame_width < width && frame_height < height))
      )
      draw = false;

   do_swap = frame || draw;

   if (     (gl1->video_width  != frame_width)
         || (gl1->video_height != frame_height)
         || (gl1->video_pitch  != pitch))
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gl1->video_width  = frame_width;
         gl1->video_height = frame_height;
         gl1->video_pitch  = pitch;

         pot_width         = GET_POT(frame_width);
         pot_height        = GET_POT(frame_height);

         if (draw)
         {
            if (gl1->video_buf)
               free(gl1->video_buf);

            gl1->video_buf = (unsigned char*)malloc(pot_width * pot_height * 4);
         }
      }
   }

   width         = gl1->video_width;
   height        = gl1->video_height;
   pitch         = gl1->video_pitch;

   pot_width     = GET_POT(width);
   pot_height    = GET_POT(height);

   if (draw && gl1->video_buf)
   {
      if (bits == 32)
      {
         int y;
         /* copy lines into top-left portion of larger (power-of-two) buffer */
         for (y = 0; y < (int)height; y++)
            memcpy(gl1->video_buf + ((pot_width * (bits / 8)) * y),
                  (const unsigned char*)frame + (pitch * y),
                  width * (bits / 8));
      }
      else if (bits == 16)
         conv_rgb565_argb8888(gl1->video_buf, frame, width, height, pot_width * sizeof(unsigned), pitch);

      frame_to_copy = gl1->video_buf;
   }

   if (gl1->video_width != width || gl1->video_height != height)
   {
      gl1->video_width  = width;
      gl1->video_height = height;
   }

   if (gl1->ctx_driver->get_video_size)
      gl1->ctx_driver->get_video_size(gl1->ctx_data,
               &mode_width, &mode_height);

   gl1->screen_width           = mode_width;
   gl1->screen_height          = mode_height;

   if (draw)
   {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      if (frame_to_copy)
         gl1_draw_tex(gl1, pot_width, pot_height,
               width, height, gl1->tex, frame_to_copy);
   }

#ifdef HAVE_MENU
   if (gl1->menu_frame && menu_is_alive)
   {
      frame_to_copy = NULL;
      width         = gl1->menu_width;
      height        = gl1->menu_height;
      pitch         = gl1->menu_pitch;
      bits          = gl1->menu_bits;

      pot_width     = GET_POT(width);
      pot_height    = GET_POT(height);

      do_swap       = true;

      if (gl1->flags & GL1_FLAG_MENU_SIZE_CHANGED)
      {
         gl1->flags &= ~GL1_FLAG_MENU_SIZE_CHANGED;

         if (gl1->menu_video_buf)
            free(gl1->menu_video_buf);
         gl1->menu_video_buf = NULL;
      }

      if (!gl1->menu_video_buf)
         gl1->menu_video_buf = (unsigned char*)
            malloc(pot_width * pot_height * 4);

      if (bits == 16 && gl1->menu_video_buf)
      {
         conv_rgba4444_argb8888(gl1->menu_video_buf,
               gl1->menu_frame, width, height,
               pot_width * sizeof(unsigned), pitch);

         frame_to_copy = gl1->menu_video_buf;

         if (gl1->flags & GL1_FLAG_MENU_TEXTURE_FULLSCREEN)
         {
            glViewport(0, 0, video_width, video_height);
            gl1_draw_tex(gl1, pot_width, pot_height,
                  width, height, gl1->menu_tex, frame_to_copy);
            glViewport(gl1->vp.x, gl1->vp.y, gl1->vp.width, gl1->vp.height);
         }
         else
            gl1_draw_tex(gl1, pot_width, pot_height,
                  width, height, gl1->menu_tex, frame_to_copy);
      }
   }

#ifdef HAVE_OVERLAY
   if ((gl1->flags & GL1_FLAG_OVERLAY_ENABLE) && overlay_behind_menu)
      gl1_render_overlay(gl1, video_width, video_height);
#endif

   if (gl1->flags & GL1_FLAG_MENU_TEXTURE_ENABLE)
   {
      do_swap = true;
#ifdef VITA
      glUseProgram(0);
      bool enabled = glIsEnabled(GL_DEPTH_TEST);
      if (enabled)
         glDisable(GL_DEPTH_TEST);
#endif
      menu_driver_frame(menu_is_alive, video_info);
#ifdef VITA
      if (enabled)
         glEnable(GL_DEPTH_TEST);
#endif
   }
   else
#endif
      if (video_info->statistics_show)
      {
         if (osd_params)
            font_driver_render_msg(gl1, video_info->stat_text,
                  osd_params, NULL);
      }

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

#ifdef HAVE_OVERLAY
   if ((gl1->flags & GL1_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu)
      gl1_render_overlay(gl1, video_width, video_height);
#endif

   if (msg)
      font_driver_render_msg(gl1, msg, NULL, NULL);

   if (gl1->ctx_driver->update_window_title)
      gl1->ctx_driver->update_window_title(
            gl1->ctx_data);

   /* Screenshots. */
   if (gl1->readback_buffer_screenshot)
      gl1_readback(gl1,
            4,
            GL_RGBA,
#ifdef MSB_FIRST
            GL_UNSIGNED_INT_8_8_8_8_REV,
#else
            GL_UNSIGNED_BYTE,
#endif
            video_width, video_height,
            gl1->readback_buffer_screenshot);


   if (do_swap && gl1->ctx_driver->swap_buffers)
      gl1->ctx_driver->swap_buffers(gl1->ctx_data);

 /* Emscripten has to do black frame insertion in its main loop */
#ifndef EMSCRIPTEN
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused
         && !(gl1->flags & GL1_FLAG_MENU_TEXTURE_ENABLE))
   {

      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
      video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(gl1->flags & GL1_FLAG_FRAME_DUPE_LOCK))
      {
         gl1->flags |= GL1_FLAG_FRAME_DUPE_LOCK;

         while (bfi_light_frames > 0)
         {
            if (!(gl1_frame(gl1, frame, 0, 0, frame_count, 0, msg, video_info)))
            {
               gl1->flags &= ~GL1_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         gl1->flags &= ~GL1_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(gl1->flags & GL1_FLAG_FRAME_DUPE_LOCK))
         {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (gl1->ctx_driver->swap_buffers)
               gl1->ctx_driver->swap_buffers(gl1->ctx_data);
         }
      }
   }
#endif


   /* check if we are fast forwarding or in menu,
      if we are ignore hard sync */
   if (      hard_sync
         && !video_info->input_driver_nonblock_state

      )
   {
      glClear(GL_COLOR_BUFFER_BIT);
      glFinish();
   }

   if (draw)
   {
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }
   return true;
}

static void gl1_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   int interval                = 0;
   gl1_t             *gl1      = (gl1_t*)data;

   if (!gl1)
      return;

   if (!state)
      interval = swap_interval;

   if (gl1->ctx_driver->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl1->ctx_driver->swap_interval(gl1->ctx_data, interval);
   }
}

static bool gl1_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool ret             = false;
   gl1_t *gl1           = (gl1_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   gl1->ctx_driver->check_window(gl1->ctx_data,
            &quit, &resize, &temp_width, &temp_height);

   if (resize)
      gl1->flags        |= GL1_FLAG_SHOULD_RESIZE;

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

static bool gl1_focus(void *data)
{
   gl1_t *gl        = (gl1_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->has_focus)
      return gl->ctx_driver->has_focus(gl->ctx_data);
   return true;
}

static bool gl1_suppress_screensaver(void *data, bool enable) { return false; }

static void gl1_free(void *data)
{
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return;

   if (gl1->menu_frame)
      free(gl1->menu_frame);
   gl1->menu_frame = NULL;

   if (gl1->video_buf)
      free(gl1->video_buf);
   gl1->video_buf = NULL;

   if (gl1->menu_video_buf)
      free(gl1->menu_video_buf);
   gl1->menu_video_buf = NULL;

   if (gl1->tex)
   {
      glDeleteTextures(1, &gl1->tex);
      gl1->tex = 0;
   }

   if (gl1->menu_tex)
   {
      glDeleteTextures(1, &gl1->menu_tex);
      gl1->menu_tex = 0;
   }

#ifdef HAVE_OVERLAY
   gl1_free_overlay(gl1);
#endif

   if (gl1->extensions)
      string_list_free(gl1->extensions);
   gl1->extensions = NULL;

   font_driver_free_osd();
   if (gl1->ctx_driver && gl1->ctx_driver->destroy)
      gl1->ctx_driver->destroy(gl1->ctx_data);
   video_context_driver_free();
   free(gl1);
}

static bool gl1_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void gl1_set_rotation(void *data,
      unsigned rotation)
{
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return;

   gl1->rotation = 90 * rotation;
   gl1_set_projection(gl1, &gl1_default_ortho, true);
}

static void gl1_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl1_t *gl1      = (gl1_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl1->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl1_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned num_pixels = 0;
   gl1_t *gl1          = (gl1_t*)data;

   if (!gl1)
      return false;

   num_pixels                      = gl1->vp.width * gl1->vp.height;
   gl1->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

   if (!gl1->readback_buffer_screenshot)
      return false;

   if (!is_idle)
      video_driver_cached_frame();

   video_frame_convert_rgba_to_bgr(
         (const void*)gl1->readback_buffer_screenshot,
         buffer,
         num_pixels);

   free(gl1->readback_buffer_screenshot);
   gl1->readback_buffer_screenshot = NULL;

   return true;
}

static void gl1_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   settings_t *settings      = config_get_ptr();
   bool menu_linear_filter   = settings->bools.menu_linear_filter;
   unsigned       pitch      = width * 2;
   gl1_t              *gl1   = (gl1_t*)data;

   if (!gl1)
      return;

   if (menu_linear_filter)
      gl1->flags            |=  GL1_FLAG_MENU_SMOOTH;
   else
      gl1->flags            &= ~GL1_FLAG_MENU_SMOOTH;

   if (rgb32)
      pitch                  = width * 4;

   if (gl1->menu_frame)
      free(gl1->menu_frame);
   gl1->menu_frame           = NULL;

   if (     (!gl1->menu_frame)
         || (gl1->menu_width  != width)
         || (gl1->menu_height != height)
         || (gl1->menu_pitch  != pitch))
   {
      if (pitch && height)
      {
         if (gl1->menu_frame)
            free(gl1->menu_frame);

         /* FIXME? We have to assume the pitch has no
          * extra padding in it because that will
          * mess up the POT calculation when we don't
          * know how many bpp there are. */
         gl1->menu_frame = (unsigned char*)malloc(pitch * height);
      }
   }

   if (gl1->menu_frame && frame && pitch && height)
   {
      memcpy(gl1->menu_frame, frame, pitch * height);
      gl1->menu_width        = width;
      gl1->menu_height       = height;
      gl1->menu_pitch        = pitch;
      gl1->menu_bits         = rgb32 ? 32 : 16;
      gl1->flags            |= GL1_FLAG_MENU_SIZE_CHANGED;
   }
}

static void gl1_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   gl1_t *gl         = (gl1_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_size)
      gl->ctx_driver->get_video_output_size(
            gl->ctx_data,
            width, height, desc, desc_len);
}

static void gl1_get_video_output_prev(void *data)
{
   gl1_t *gl         = (gl1_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_prev)
      gl->ctx_driver->get_video_output_prev(gl->ctx_data);
}

static void gl1_get_video_output_next(void *data)
{
   gl1_t *gl         = (gl1_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_next)
      gl->ctx_driver->get_video_output_next(gl->ctx_data);
}

static void gl1_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gl1_t               *gl = (gl1_t*)data;
   if (gl->ctx_driver->set_video_mode)
      gl->ctx_driver->set_video_mode(gl->ctx_data,
            width, height, fullscreen);
}

static unsigned gl1_wrap_type_to_enum(enum gfx_wrap_type type)
{
   /* Mirrored not actually supported */
   if (type == RARCH_WRAP_REPEAT || type == RARCH_WRAP_MIRRORED_REPEAT)
      return GL_REPEAT;
   return GL_CLAMP;
}

static void gl1_load_texture_data(
      GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint filter;
   bool use_rgba    = video_driver_supports_rgba();
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl1_wrap_type_to_enum(wrap_type);

   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

   /* GL1.x does not have mipmapping support. */
   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_NEAREST:
      case TEXTURE_FILTER_NEAREST:
         filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_MIPMAP_LINEAR:
      case TEXTURE_FILTER_LINEAR:
      default:
         filter = GL_LINEAR;
         break;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32)
         ? GL_RGBA
         : RARCH_GL1_INTERNAL_FORMAT32,
         width,
         height,
         0,
         (use_rgba || !rgb32)
         ? GL_RGBA
         : RARCH_GL1_TEXTURE_TYPE32,
#ifdef MSB_FIRST
         GL_UNSIGNED_INT_8_8_8_8_REV,
#else
         rgb32
         ? RARCH_GL1_FORMAT32
         : GL_UNSIGNED_BYTE,
#endif
         frame);
}

static void video_texture_load_gl1(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *idptr)
{
   GLuint id;
   unsigned width     = 0;
   unsigned height    = 0;
   const void *pixels = NULL;

   /* Generate the OpenGL texture object */
   glGenTextures(1, &id);
   *idptr             = id;

   if (ti)
   {
      width           = ti->width;
      height          = ti->height;
      pixels          = ti->pixels;
   }

   gl1_load_texture_data(
         id,
         RARCH_WRAP_EDGE,
         filter_type,
         4 /* TODO/FIXME - dehardcode */,
         width,
         height,
         pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl1(void *data)
{
   uintptr_t id = 0;
   gl1_t   *gl1 = (gl1_t*)video_driver_get_ptr();

   if (gl1->ctx_driver->make_current)
      gl1->ctx_driver->make_current(false);

   if (data)
      video_texture_load_gl1((struct texture_image*)data,
            TEXTURE_FILTER_NEAREST, &id);
   return (int)id;
}

static int video_texture_unload_wrap_gl1(void *data)
{
   GLuint  glid;
   uintptr_t id = (uintptr_t)data;
   gl1_t   *gl1 = (gl1_t*)video_driver_get_ptr();

   if (gl1 && gl1->ctx_driver->make_current)
      gl1->ctx_driver->make_current(false);

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
   return 0;
}
#endif

static uintptr_t gl1_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      gl1_t                   *gl1 = (gl1_t*)video_data;
      custom_command_method_t func = video_texture_load_wrap_gl1;

      return video_thread_texture_handle(data, func);
   }
#endif

   video_texture_load_gl1((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl1_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl1_t *gl1     = (gl1_t*)data;
   if (gl1)
      gl1->flags |= (GL1_FLAG_KEEP_ASPECT | GL1_FLAG_SHOULD_RESIZE);
}

static void gl1_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_unload_wrap_gl1;
      video_thread_texture_handle((void *)id, func);
      return;
   }
#endif

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static float gl1_get_refresh_rate(void *data)
{
   float refresh_rate = 0.0f;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
      return refresh_rate;
   return 0.0f;
}

static void gl1_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl1_t *gl1       = (gl1_t*)data;

   if (!gl1)
      return;

   if (state)
      gl1->flags   |=  GL1_FLAG_MENU_TEXTURE_ENABLE;
   else
      gl1->flags   &= ~GL1_FLAG_MENU_TEXTURE_ENABLE;
   if (full_screen)
      gl1->flags   |=  GL1_FLAG_MENU_TEXTURE_FULLSCREEN;
   else
      gl1->flags   &= ~GL1_FLAG_MENU_TEXTURE_FULLSCREEN;
}

static uint32_t gl1_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);

   return flags;
}

static const video_poke_interface_t gl1_poke_interface = {
   gl1_get_flags,
   gl1_load_texture,
   gl1_unload_texture,
   gl1_set_video_mode,
   gl1_get_refresh_rate,
   NULL, /* set_filtering */
   gl1_get_video_output_size,
   gl1_get_video_output_prev,
   gl1_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gl1_set_aspect_ratio,
   NULL, /* apply_state_changes */
   gl1_set_texture_frame,
   gl1_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void gl1_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &gl1_poke_interface; }
#ifdef HAVE_GFX_WIDGETS
static bool gl1_widgets_enabled(void *data) { return true; }
#endif

static void gl1_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   gl1_t               *gl1 = (gl1_t*)data;
   gl1_set_viewport(gl1,
         viewport_width, viewport_height, force_full, allow_rotate);
}

#ifdef HAVE_OVERLAY
static unsigned gl1_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

static bool gl1_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   size_t i;
   int j;
   gl1_t *gl = (gl1_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   gl1_free_overlay(gl);
   gl->overlay_tex = (GLuint*)
      calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
      return false;

   gl->overlay_vertex_coord = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)
      calloc(4 * 4 * num_images, sizeof(GLfloat));

   if (     !gl->overlay_vertex_coord
         || !gl->overlay_tex_coord
         || !gl->overlay_color_coord)
      return false;

   gl->overlays             = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      unsigned alignment = gl1_get_alignment(images[i].width
            * sizeof(uint32_t));

      gl1_load_texture_data(gl->overlay_tex[i],
            RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
            alignment,
            images[i].width, images[i].height, images[i].pixels,
            sizeof(uint32_t));

      /* Default. Stretch to whole screen. */
      gl1_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl1_overlay_vertex_geom(gl, i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   return true;
}

static void gl1_overlay_enable(void *data, bool state)
{
   gl1_t *gl     = (gl1_t*)data;

   if (!gl)
      return;

   if (state)
      gl->flags |=  GL1_FLAG_OVERLAY_ENABLE;
   else
      gl->flags &= ~GL1_FLAG_OVERLAY_ENABLE;

   if ((gl->flags & GL1_FLAG_FULLSCREEN) && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl1_overlay_full_screen(void *data, bool enable)
{
   gl1_t *gl = (gl1_t*)data;

   if (gl)
   {
      if (enable)
         gl->flags |=  GL1_FLAG_OVERLAY_FULLSCREEN;
      else
         gl->flags &= ~GL1_FLAG_OVERLAY_FULLSCREEN;
   }
}

static void gl1_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color = NULL;
   gl1_t *gl      = (gl1_t*)data;
   if (!gl)
      return;

   color          = (GLfloat*)&gl->overlay_color_coord[image * 16];

   color[ 0 + 3]  = mod;
   color[ 4 + 3]  = mod;
   color[ 8 + 3]  = mod;
   color[12 + 3]  = mod;
}

static const video_overlay_interface_t gl1_overlay_interface = {
   gl1_overlay_enable,
   gl1_overlay_load,
   gl1_overlay_tex_geom,
   gl1_overlay_vertex_geom,
   gl1_overlay_full_screen,
   gl1_overlay_set_alpha,
};

static void gl1_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &gl1_overlay_interface;
}

#endif

static bool gl1_has_windowed(void *data)
{
   gl1_t *gl        = (gl1_t*)data;
   if (gl && gl->ctx_driver)
      return gl->ctx_driver->has_windowed;
   return false;
}

video_driver_t video_gl1 = {
   gl1_init,
   gl1_frame,
   gl1_set_nonblock_state,
   gl1_alive,
   gl1_focus,
   gl1_suppress_screensaver,
   gl1_has_windowed,
   gl1_set_shader,
   gl1_free,
   "gl1",
   gl1_set_viewport_wrapper,
   gl1_set_rotation,
   gl1_viewport_info,
   gl1_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gl1_get_overlay_interface,
#endif
   gl1_get_poke_interface,
   gl1_wrap_type_to_enum,
#ifdef HAVE_GFX_WIDGETS
   gl1_widgets_enabled
#endif
};
