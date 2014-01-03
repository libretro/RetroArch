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

static bool gl_init_font(void *data, const char *font_path, float font_size, unsigned win_width, unsigned win_height)
{
   size_t i, j;
   (void)win_width;
   (void)win_height;

   if (!g_settings.video.font_enable)
      return false;

   (void)font_size;
   gl_t *gl = (gl_t*)data;

   if (font_renderer_create_default(&gl->font_driver, &gl->font))
   {
      glGenTextures(1, &gl->font_tex);
      glBindTexture(GL_TEXTURE_2D, gl->font_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl->max_font_size);
   }
   else
   {
      RARCH_WARN("Couldn't init font renderer.\n");
      return false;
   }

   for (i = 0; i < 4; i++)
   {
      gl->font_color[4 * i + 0] = g_settings.video.msg_color_r;
      gl->font_color[4 * i + 1] = g_settings.video.msg_color_g;
      gl->font_color[4 * i + 2] = g_settings.video.msg_color_b;
      gl->font_color[4 * i + 3] = 1.0;
   }

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 3; j++)
         gl->font_color_dark[4 * i + j] = 0.3 * gl->font_color[4 * i + j];
      gl->font_color_dark[4 * i + 3] = 1.0;
   }

   return true;
}

void gl_deinit_font(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl->font)
   {
      gl->font_driver->free(gl->font);
      glDeleteTextures(1, &gl->font_tex);

      if (gl->font_tex_buf)
         free(gl->font_tex_buf);
   }
}

// Somewhat overwhelming code just to render some damn fonts.
// We aim to use NPOT textures for compatibility with old and shitty cards.
// Also, we want to avoid reallocating a texture for each glyph (performance dips), so we
// contruct the whole texture using one call, and copy straight to it with
// glTexSubImage.

struct font_rect
{
   int x, y;
   int width, height;
   int pot_width, pot_height;
};

static void calculate_msg_geometry(const struct font_output *head, struct font_rect *rect)
{
   int x_min = head->off_x;
   int x_max = head->off_x + head->width;
   int y_min = head->off_y;
   int y_max = head->off_y + head->height;

   while ((head = head->next))
   {
      int left = head->off_x;
      int right = head->off_x + head->width;
      int bottom = head->off_y;
      int top = head->off_y + head->height;

      if (left < x_min)
         x_min = left;
      if (right > x_max)
         x_max = right;

      if (bottom < y_min)
         y_min = bottom;
      if (top > y_max)
         y_max = top;
   }

   rect->x = x_min;
   rect->y = y_min;
   rect->width = x_max - x_min;
   rect->height = y_max - y_min;
}

static void adjust_power_of_two(gl_t *gl, struct font_rect *geom)
{
   // Some systems really hate NPOT textures.
   geom->pot_width  = next_pow2(geom->width);
   geom->pot_height = next_pow2(geom->height);

   if (geom->pot_width > gl->max_font_size)
      geom->pot_width = gl->max_font_size;
   if (geom->pot_height > gl->max_font_size)
      geom->pot_height = gl->max_font_size;

   if ((geom->pot_width > gl->font_tex_w) || (geom->pot_height > gl->font_tex_h))
   {
      gl->font_tex_buf = (uint32_t*)realloc(gl->font_tex_buf,
            geom->pot_width * geom->pot_height * sizeof(uint32_t));

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geom->pot_width, geom->pot_height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

      gl->font_tex_w = geom->pot_width;
      gl->font_tex_h = geom->pot_height;
   }
}

static void copy_glyph(const struct font_output *head, const struct font_rect *geom, uint32_t *buffer, unsigned width, unsigned height)
{
   int h, w;
   // head has top-left oriented coords.
   int x = head->off_x - geom->x;
   int y = head->off_y - geom->y;
   y     = height - head->height - y - 1;

   const uint8_t *src = head->output;
   int font_width  = head->width  + ((x < 0) ? x : 0);
   int font_height = head->height + ((y < 0) ? y : 0);

   if (x < 0)
   {
      src += -x;
      x    = 0;
   }

   if (y < 0)
   {
      src += -y * head->pitch;
      y    = 0;
   }

   if (x + font_width > (int)width)
      font_width = width - x;

   if (y + font_height > (int)height)
      font_height = height - y;

   uint32_t *dst = buffer + y * width + x;
   for (h = 0; h < font_height; h++, dst += width, src += head->pitch)
   {
      uint8_t *d = (uint8_t*)dst;
      for (w = 0; w < font_width; w++)
      {
         *d++ = 0xff;
         *d++ = 0xff;
         *d++ = 0xff;
         *d++ = src[w];
      }
   }
}

// Old style "blitting", so we can render all the fonts in one go.
// TODO: Is it possible that fonts could overlap if we blit without alpha blending?
static void blit_fonts(gl_t *gl, const struct font_output *head, const struct font_rect *geom)
{
   memset(gl->font_tex_buf, 0, gl->font_tex_w * gl->font_tex_h * sizeof(uint32_t));

   while (head)
   {
      copy_glyph(head, geom, gl->font_tex_buf, gl->font_tex_w, gl->font_tex_h);
      head = head->next;
   }

   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   glTexSubImage2D(GL_TEXTURE_2D,
      0, 0, 0, gl->font_tex_w, gl->font_tex_h,
      GL_RGBA, GL_UNSIGNED_BYTE, gl->font_tex_buf);
}

static void calculate_font_coords(gl_t *gl,
      GLfloat font_vertex[8], GLfloat font_vertex_dark[8], GLfloat font_tex_coords[8], GLfloat scale, GLfloat pos_x, GLfloat pos_y)
{
   unsigned i;
   GLfloat scale_factor = scale;

   GLfloat lx = pos_x;
   GLfloat hx = (GLfloat)gl->font_last_width * scale_factor / gl->vp.width + lx;
   GLfloat ly = pos_y;
   GLfloat hy = (GLfloat)gl->font_last_height * scale_factor / gl->vp.height + ly;

   font_vertex[0] = lx;
   font_vertex[2] = hx;
   font_vertex[4] = lx;
   font_vertex[6] = hx;
   font_vertex[1] = hy;
   font_vertex[3] = hy;
   font_vertex[5] = ly;
   font_vertex[7] = ly;

   GLfloat shift_x = 2.0f / gl->vp.width;
   GLfloat shift_y = 2.0f / gl->vp.height;
   for (i = 0; i < 4; i++)
   {
      font_vertex_dark[2 * i + 0] = font_vertex[2 * i + 0] - shift_x;
      font_vertex_dark[2 * i + 1] = font_vertex[2 * i + 1] - shift_y;
   }

   lx = 0.0f;
   hx = (GLfloat)gl->font_last_width / gl->font_tex_w;
   ly = 1.0f - (GLfloat)gl->font_last_height / gl->font_tex_h; 
   hy = 1.0f;

   font_tex_coords[0] = lx;
   font_tex_coords[2] = hx;
   font_tex_coords[4] = lx;
   font_tex_coords[6] = hx;
   font_tex_coords[1] = ly;
   font_tex_coords[3] = ly;
   font_tex_coords[5] = hy;
   font_tex_coords[7] = hy;
}

static void setup_font(void *data, const char *msg, GLfloat scale, GLfloat pos_x, GLfloat pos_y)
{
   gl_t *gl = (gl_t*)data;
   if (!gl->font)
      return;

   if (gl->shader)
      gl->shader->use(GL_SHADER_STOCK_BLEND);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   glEnable(GL_BLEND);

   GLfloat font_vertex[8]; 
   GLfloat font_vertex_dark[8]; 
   GLfloat font_tex_coords[8];

   glBindTexture(GL_TEXTURE_2D, gl->font_tex);

   gl->coords.tex_coord = font_tex_coords;

   struct font_output_list out;

   // If we get the same message, there's obviously no need to render fonts again ...
   if (strcmp(gl->font_last_msg, msg) != 0)
   {
      gl->font_driver->render_msg(gl->font, msg, &out);
      struct font_output *head = out.head;

      struct font_rect geom;
      calculate_msg_geometry(head, &geom);
      adjust_power_of_two(gl, &geom);
      blit_fonts(gl, head, &geom);

      gl->font_driver->free_output(gl->font, &out);
      strlcpy(gl->font_last_msg, msg, sizeof(gl->font_last_msg));

      gl->font_last_width = geom.width;
      gl->font_last_height = geom.height;
   }
   calculate_font_coords(gl, font_vertex, font_vertex_dark, font_tex_coords, 
         scale, pos_x, pos_y);
   
   gl->coords.vertex = font_vertex_dark;
   gl->coords.color  = gl->font_color_dark;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.vertex = font_vertex;
   gl->coords.color  = gl->font_color;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   // Post - Go back to old rendering path.
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = gl->white_color_ptr;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);

   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};
   gl_set_projection(gl, &ortho, true);
}

static void gl_render_msg(void *data, const char *msg, void *parms)
{
   (void)data;
   (void)msg;
   GLfloat x, y, scale;
   
   gl_t *gl = (gl_t*)data;
   font_params_t *params = (font_params_t*)parms;

   if (params)
   {
      x = params->x;
      y = params->y;
      scale = params->scale;
   }
   else
   {
      x = g_settings.video.msg_pos_x;
      y = g_settings.video.msg_pos_y;
      scale = g_settings.video.font_scale ? (GLfloat)gl->vp.width / (GLfloat)gl->full_x : 1.0f;
   }

   setup_font(data, msg, scale, x, y);
}

const gl_font_renderer_t gl_raster_font = {
   gl_init_font,
   gl_deinit_font,
   gl_render_msg,
   "GL raster",
};

