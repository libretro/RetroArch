/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

void gl_init_font(gl_t *gl, const char *font_path, unsigned font_size)
{
#ifdef HAVE_FREETYPE
   if (!g_settings.video.font_enable)
      return;

   const char *path = font_path;
   if (!*path)
      path = font_renderer_get_default_font();

   if (path)
   {
      gl->font = font_renderer_new(path, font_size);
      if (gl->font)
      {
         glGenTextures(1, &gl->font_tex);
         glBindTexture(GL_TEXTURE_2D, gl->font_tex);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      }
      else
         RARCH_WARN("Couldn't init font renderer with font \"%s\"...\n", font_path);
   }
   else
      RARCH_LOG("Did not find default font.\n");

   for (unsigned i = 0; i < 4; i++)
   {
      gl->font_color[4 * i + 0] = g_settings.video.msg_color_r;
      gl->font_color[4 * i + 1] = g_settings.video.msg_color_g;
      gl->font_color[4 * i + 2] = g_settings.video.msg_color_b;
      gl->font_color[4 * i + 3] = 1.0;
   }

   for (unsigned i = 0; i < 4; i++)
   {
      for (unsigned j = 0; j < 3; j++)
         gl->font_color_dark[4 * i + j] = 0.3 * gl->font_color[4 * i + j];
      gl->font_color_dark[4 * i + 3] = 1.0;
   }
#else
   (void)gl;
   (void)font_path;
   (void)font_size;
#endif
}

void gl_deinit_font(gl_t *gl)
{
#ifdef HAVE_FREETYPE
   if (gl->font)
   {
      font_renderer_free(gl->font);
      glDeleteTextures(1, &gl->font_tex);

      if (gl->font_tex_empty_buf)
         free(gl->font_tex_empty_buf);
   }
#else
   (void)gl;
#endif
}

#ifdef HAVE_FREETYPE
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
   geom->pot_width = next_pow2(geom->width);
   geom->pot_height = next_pow2(geom->height);

   if ((geom->pot_width > gl->font_tex_w) || (geom->pot_height > gl->font_tex_h))
   {
      gl->font_tex_empty_buf = realloc(gl->font_tex_empty_buf, geom->pot_width * geom->pot_height);
      memset(gl->font_tex_empty_buf, 0, geom->pot_width * geom->pot_height);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, geom->pot_width);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY8, geom->pot_width, geom->pot_height,
            0, GL_LUMINANCE, GL_UNSIGNED_BYTE, gl->font_tex_empty_buf);

      gl->font_tex_w = geom->pot_width;
      gl->font_tex_h = geom->pot_height;
   }
}

// Old style "blitting", so we can render all the fonts in one go.
// TODO: Is it possible that fonts could overlap if we blit without alpha blending?
static void blit_fonts(gl_t *gl, const struct font_output *head, const struct font_rect *geom)
{
   // Clear out earlier fonts.
   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->font_tex_w);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, gl->font_tex_w, gl->font_tex_h,
         GL_LUMINANCE, GL_UNSIGNED_BYTE, gl->font_tex_empty_buf);

   while (head)
   {
      // head has top-left oriented coords.
      int x = head->off_x - geom->x;
      int y = head->off_y - geom->y;
      y = gl->font_tex_h - head->height - y - 1;

      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(head->pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, head->pitch);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, x, y, head->width, head->height,
            GL_LUMINANCE, GL_UNSIGNED_BYTE, head->output);

      head = head->next;
   }
}

static void calculate_font_coords(gl_t *gl,
      GLfloat font_vertex[8], GLfloat font_vertex_dark[8], GLfloat font_tex_coords[8])
{
   GLfloat scale_factor = g_settings.video.font_scale ?
      (GLfloat)gl->full_x / (GLfloat)gl->vp_width :
      1.0f;

   GLfloat lx = g_settings.video.msg_pos_x;
   GLfloat hx = (GLfloat)gl->font_last_width / (gl->vp_width * scale_factor) + lx;
   GLfloat ly = g_settings.video.msg_pos_y;
   GLfloat hy = (GLfloat)gl->font_last_height / (gl->vp_height * scale_factor) + ly;

   font_vertex[0] = lx;
   font_vertex[1] = ly;
   font_vertex[2] = lx;
   font_vertex[3] = hy;
   font_vertex[4] = hx;
   font_vertex[5] = hy;
   font_vertex[6] = hx;
   font_vertex[7] = ly;

   GLfloat shift_x = 2.0f / gl->vp_width;
   GLfloat shift_y = 2.0f / gl->vp_height;
   for (unsigned i = 0; i < 4; i++)
   {
      font_vertex_dark[2 * i + 0] = font_vertex[2 * i + 0] - shift_x;
      font_vertex_dark[2 * i + 1] = font_vertex[2 * i + 1] - shift_y;
   }

   lx = 0.0f;
   hx = (GLfloat)gl->font_last_width / gl->font_tex_w;
   ly = 1.0f - (GLfloat)gl->font_last_height / gl->font_tex_h; 
   hy = 1.0f;

   font_tex_coords[0] = lx;
   font_tex_coords[1] = hy;
   font_tex_coords[2] = lx;
   font_tex_coords[3] = ly;
   font_tex_coords[4] = hx;
   font_tex_coords[5] = ly;
   font_tex_coords[6] = hx;
   font_tex_coords[7] = hy;
}

extern const GLfloat vertexes_flipped[];
extern const GLfloat white_color[];

#endif

void gl_render_msg(void *data, const char *msg)
{
#ifdef HAVE_FREETYPE
   gl_t *gl = (gl_t*)data;
   if (!gl->font)
      return;

   gl_shader_use(0);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
   glEnable(GL_BLEND);

   GLfloat font_vertex[8]; 
   GLfloat font_vertex_dark[8]; 
   GLfloat font_tex_coords[8];

   glBindTexture(GL_TEXTURE_2D, gl->font_tex);
   glTexCoordPointer(2, GL_FLOAT, 0, font_tex_coords);

   struct font_output_list out;

   // If we get the same message, there's obviously no need to render fonts again ...
   if (strcmp(gl->font_last_msg, msg) != 0)
   {
      font_renderer_msg(gl->font, msg, &out);
      struct font_output *head = out.head;

      struct font_rect geom;
      calculate_msg_geometry(head, &geom);
      adjust_power_of_two(gl, &geom);
      blit_fonts(gl, head, &geom);

      font_renderer_free_output(&out);
      strlcpy(gl->font_last_msg, msg, sizeof(gl->font_last_msg));

      gl->font_last_width = geom.width;
      gl->font_last_height = geom.height;
   }
   calculate_font_coords(gl, font_vertex, font_vertex_dark, font_tex_coords);
   
   glVertexPointer(2, GL_FLOAT, 0, font_vertex_dark);
   glColorPointer(4, GL_FLOAT, 0, gl->font_color_dark);
   glDrawArrays(GL_QUADS, 0, 4);
   glVertexPointer(2, GL_FLOAT, 0, font_vertex);
   glColorPointer(4, GL_FLOAT, 0, gl->font_color);
   glDrawArrays(GL_QUADS, 0, 4);

   // Post - Go back to old rendering path.
   glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
   glVertexPointer(2, GL_FLOAT, 0, vertexes_flipped);
   glColorPointer(4, GL_FLOAT, 0, white_color);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);

   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};
   gl_set_projection(gl, &ortho, true);
#else
   (void)gl;
   (void)msg;
#endif
}

