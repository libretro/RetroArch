/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
 *  Copyright (C) 2014      - Jean-André Santoni
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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../backend/menu_common_backend.h"
#include "../menu_common.h"
#include "../file_list.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../gfx/gl_common.h"
#include "../../../gfx/shader_common.h"
#include "../../../config.def.h"
#include "../../../file.h"
#include "../../../dynamic.h"
#include "../../../compat/posix_string.h"
#include "../../../gfx/shader_parse.h"
#include "../../../performance.h"
#include "../../../input/input_common.h"

#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "lakka.h"
#include "tween.h"
#include "png_texture_load.h"

#define HSPACING 300
#define VSPACING 75
#define C_ACTIVE_ZOOM 1.0
#define C_PASSIVE_ZOOM 0.5
#define I_ACTIVE_ZOOM 0.75
#define I_PASSIVE_ZOOM 0.35
#define DELAY 0.02

const GLfloat background_color[] = {
   0.1, 0.74, 0.61, 1.00,
   0.1, 0.74, 0.61, 1.00,
   0.1, 0.74, 0.61, 1.00,
   0.1, 0.74, 0.61, 1.00,
};

menu_category* categories = NULL;

int depth = 0;

GLuint settings_icon;
GLuint arrow_icon;
GLuint run_icon;
GLuint resume_icon;
GLuint savestate_icon;
GLuint loadstate_icon;
GLuint screenshot_icon;
GLuint reload_icon;

struct font_output_list run_label;
struct font_output_list resume_label;

int num_categories = 0;

int menu_active_category = 0;

int dim = 192;

float all_categories_x = 0;

rgui_handle_t *rgui;

// Fonts
void *font;
const gl_font_renderer_t *font_ctx;
const font_renderer_driver_t *font_driver;
GLuint font_tex;
GLint max_font_size;
int font_tex_w, font_tex_h;
uint32_t *font_tex_buf;
char font_last_msg[256];
int font_last_width, font_last_height;
GLfloat font_color[16];
GLfloat font_color_dark[16];
GLuint texture;

// Move the categories left or right depending on the menu_active_category variable
void lakka_switch_categories(void)
{
   // translation
   add_tween(DELAY, -menu_active_category * HSPACING, &all_categories_x, &inOutQuad);

   // alpha tweening
   for (int i = 0; i < num_categories; i++)
   {
      float ca = (i == menu_active_category) ? 1.0 : 0.5;
      float cz = (i == menu_active_category) ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      add_tween(DELAY, ca, &categories[i].alpha, &inOutQuad);
      add_tween(DELAY, cz, &categories[i].zoom,  &inOutQuad);

      for (int j = 0; j < categories[i].num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
            (j == categories[i].active_item) ? 1.0 : 0.5;
         add_tween(DELAY, ia, &categories[i].items[j].alpha, &inOutQuad);
      }
   }
}

void lakka_switch_items(void)
{
   for (int j = 0; j < categories[menu_active_category].num_items; j++)
   {
      float ia, iz, iy;
      ia = (j == categories[menu_active_category].active_item) ? 1.0 : 0.5;
      iz = (j == categories[menu_active_category].active_item) ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
      iy = (j == categories[menu_active_category].active_item) ? VSPACING*2.5 :
         (j  < categories[menu_active_category].active_item) ? VSPACING*(j-categories[menu_active_category].active_item - 1) :
         VSPACING*(j-categories[menu_active_category].active_item + 3);

      add_tween(DELAY, ia, &categories[menu_active_category].items[j].alpha, &inOutQuad);
      add_tween(DELAY, iz, &categories[menu_active_category].items[j].zoom,  &inOutQuad);
      add_tween(DELAY, iy, &categories[menu_active_category].items[j].y,     &inOutQuad);
   }
}

void lakka_switch_subitems(void)
{
   int k;
   menu_item ai = categories[menu_active_category].items[categories[menu_active_category].active_item];

   for (k = 0; k < ai.num_subitems; k++)
   {
      if (k < ai.active_subitem)
      {
         // Above items
         add_tween(DELAY, 0.5, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k-ai.active_subitem + 2), &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      }
      else if (k == ai.active_subitem)
      {
         // Active item
         add_tween(DELAY, 1.0, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*2.5, &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_ACTIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      }
      else if (k > ai.active_subitem)
      {
         // Under items
         add_tween(DELAY, 0.5, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k-ai.active_subitem + 3), &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      }
   }
}

void lakka_reset_submenu(void)
{
   int i, j, k;
   if (!(g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0))
   {
      // Keeps active submenu state (do we really want that?)
      categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem = 0;
      for (i = 0; i < num_categories; i++)
      {
         for (j = 0; j < categories[i].num_items; j++)
         {
            for (k = 0; k < categories[i].items[j].num_subitems; k++)
            {
               categories[i].items[j].subitems[k].alpha = 0;
               categories[i].items[j].subitems[k].zoom = k == categories[i].items[j].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
               categories[i].items[j].subitems[k].y = k == 0 ? VSPACING*2.5 : VSPACING*(3+k);
            }
         }
      }
   }
}

void lakka_open_submenu(void)
{
   int i, j, k;
   add_tween(DELAY, -HSPACING * (menu_active_category+1), &all_categories_x, &inOutQuad);

   // Reset contextual menu style
   lakka_reset_submenu();
   
   for (i = 0; i < num_categories; i++)
   {
      if (i == menu_active_category)
      {
         add_tween(DELAY, 1.0, &categories[i].alpha, &inOutQuad);
         for (j = 0; j < categories[i].num_items; j++)
         {
            if (j == categories[i].active_item)
            {
               for (k = 0; k < categories[i].items[j].num_subitems; k++)
               {
                  if (k == categories[i].items[j].active_subitem)
                  {
                     add_tween(DELAY, 1.0, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
                     add_tween(DELAY, I_ACTIVE_ZOOM, &categories[i].items[j].subitems[k].zoom, &inOutQuad);
                  }
                  else
                  {
                     add_tween(DELAY, 0.5, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
                     add_tween(DELAY, I_PASSIVE_ZOOM, &categories[i].items[j].subitems[k].zoom, &inOutQuad);
                  }
               }
            }
            else
               add_tween(DELAY, 0, &categories[i].items[j].alpha, &inOutQuad);
         }
      }
      else
         add_tween(DELAY, 0, &categories[i].alpha, &inOutQuad);
   }
}

void lakka_close_submenu(void)
{
   int i, j, k;
   add_tween(DELAY, -HSPACING * menu_active_category, &all_categories_x, &inOutQuad);
   
   for (i = 0; i < num_categories; i++)
   {
      if (i == menu_active_category)
      {
         add_tween(DELAY, 1.0, &categories[i].alpha, &inOutQuad);
         add_tween(DELAY, C_ACTIVE_ZOOM, &categories[i].zoom, &inOutQuad);
         for (j = 0; j < categories[i].num_items; j++)
         {
            if (j == categories[i].active_item)
            {
               add_tween(DELAY, 1.0, &categories[i].items[j].alpha, &inOutQuad);
               for (k = 0; k < categories[i].items[j].num_subitems; k++)
                  add_tween(DELAY, 0, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
            }
            else
               add_tween(DELAY, 0.5, &categories[i].items[j].alpha, &inOutQuad);
         }
      }
      else
      {
         add_tween(DELAY, 0.5, &categories[i].alpha, &inOutQuad);
         add_tween(DELAY, C_PASSIVE_ZOOM, &categories[i].zoom, &inOutQuad);
         for (j = 0; j < categories[i].num_items; j++)
            add_tween(DELAY, 0, &categories[i].items[j].alpha, &inOutQuad);
      }
   }
}

struct font_rect
{
   int x, y;
   int width, height;
   int pot_width, pot_height;
};

/* font rendering */

static bool init_font(void *data, const char *font_path, float font_size, unsigned win_width, unsigned win_height)
{
   size_t i, j;
   (void)win_width;
   (void)win_height;

   if (!g_settings.video.font_enable)
      return false;

   (void)font_size;
   gl_t *gl = (gl_t*)data;

   if (font_renderer_create_default(&font_driver, &font))
   {
      glGenTextures(1, &font_tex);
      glBindTexture(GL_TEXTURE_2D, font_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, texture);
      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_font_size);
   }
   else
   {
      RARCH_WARN("Couldn't init font renderer.\n");
      return false;
   }

   for (i = 0; i < 4; i++)
   {
      font_color[4 * i + 0] = g_settings.video.msg_color_r;
      font_color[4 * i + 1] = g_settings.video.msg_color_g;
      font_color[4 * i + 2] = g_settings.video.msg_color_b;
      font_color[4 * i + 3] = 1.0;
   }

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 3; j++)
         font_color_dark[4 * i + j] = 0.3 * font_color[4 * i + j];
      font_color_dark[4 * i + 3] = 1.0;
   }

   return true;
}

static void calculate_msg_geometry(const struct font_output *head, struct font_rect *rect)
{
   int x_min = head->off_x;
   int x_max = head->off_x + head->width+10;
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

   if (geom->pot_width > max_font_size)
      geom->pot_width = max_font_size;
   if (geom->pot_height > max_font_size)
      geom->pot_height = max_font_size;

   if ((geom->pot_width > font_tex_w) || (geom->pot_height > font_tex_h))
   {
      font_tex_buf = (uint32_t*)realloc(font_tex_buf,
            geom->pot_width * geom->pot_height * sizeof(uint32_t));

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geom->pot_width, geom->pot_height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

      font_tex_w = geom->pot_width;
      font_tex_h = geom->pot_height;
   }
}

static void copy_glyph(const struct font_output *head, const struct font_rect *geom, uint32_t *buffer, unsigned width, unsigned height)
{
   int h, w, x, y, font_width, font_height;
   uint8_t *src;
   uint32_t *dst;

   // head has top-left oriented coords.
   x = head->off_x - geom->x;
   y = head->off_y - geom->y;
   y = height - head->height - y - 1;

   src = (uint8_t*)head->output;
   font_width  = head->width  + ((x < 0) ? x : 0);
   font_height = head->height + ((y < 0) ? y : 0);

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

   dst = (uint32_t*)(buffer + y * width + x);
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

static void blit_fonts(gl_t *gl, const struct font_output *head, const struct font_rect *geom)
{
   memset(font_tex_buf, 0, font_tex_w * font_tex_h * sizeof(uint32_t));

   while (head)
   {
      copy_glyph(head, geom, font_tex_buf, font_tex_w, font_tex_h);
      head = head->next;
   }

   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   glTexSubImage2D(GL_TEXTURE_2D,
      0, 0, 0, font_tex_w, font_tex_h,
      GL_RGBA, GL_UNSIGNED_BYTE, font_tex_buf);
}

static void calculate_font_coords(gl_t *gl,
      GLfloat font_vertex[8], GLfloat font_vertex_dark[8], GLfloat font_tex_coords[8], GLfloat scale, GLfloat pos_x, GLfloat pos_y)
{
   unsigned i;
   GLfloat scale_factor = scale;

   GLfloat lx = pos_x;
   GLfloat hx = (GLfloat)font_last_width * scale_factor / gl->vp.width + lx;
   GLfloat ly = pos_y;
   GLfloat hy = (GLfloat)font_last_height * scale_factor / gl->vp.height + ly;

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
   hx = (GLfloat)font_last_width / font_tex_w;
   ly = 1.0f - (GLfloat)font_last_height / font_tex_h; 
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

static void lakka_draw_text(void *data, struct font_output_list out, float x, float y, float scale, float alpha)
{
   gl_t *gl = (gl_t*)data;
   if (!font)
      return;

   for (int i = 0; i < 4; i++)
   {
      font_color[4 * i + 0] = 1.0;
      font_color[4 * i + 1] = 1.0;
      font_color[4 * i + 2] = 1.0;
      font_color[4 * i + 3] = alpha;
   }

   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   glEnable(GL_BLEND);

   GLfloat font_vertex[8]; 
   GLfloat font_vertex_dark[8]; 
   GLfloat font_tex_coords[8];

   glBindTexture(GL_TEXTURE_2D, font_tex);

   gl->coords.tex_coord = font_tex_coords;

   struct font_output *head = out.head;

   struct font_rect geom;
   calculate_msg_geometry(head, &geom);
   adjust_power_of_two(gl, &geom);
   blit_fonts(gl, head, &geom);

   //font_driver->free_output(font, &out);

   font_last_width = geom.width;
   font_last_height = geom.height;

   calculate_font_coords(gl, font_vertex, font_vertex_dark, font_tex_coords, 
         scale, x / gl->win_width, (gl->win_height - y) / gl->win_height);

   gl->coords.vertex = font_vertex;
   gl->coords.color  = font_color;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   // Post - Go back to old rendering path.
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = gl->white_color_ptr;
   glBindTexture(GL_TEXTURE_2D, texture);

   glDisable(GL_BLEND);

   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};
   gl_set_projection(gl, &ortho, true);
}

void lakka_draw_background(void *data)
{
   gl_t *gl = (gl_t*)data;

   glEnable(GL_BLEND);

   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = background_color;
   glBindTexture(GL_TEXTURE_2D, 0);

   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

void lakka_draw_icon(void *data, GLuint texture, float x, float y, float alpha, float rotation, float scale)
{
   gl_t *gl = (gl_t*)data;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   static const GLfloat vtest[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };

   glViewport(x, 900 - y, dim, dim);

   glEnable(GL_BLEND);

   gl->coords.vertex = vtest;
   gl->coords.tex_coord = vtest;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   math_matrix mymat;

   math_matrix mrot;
   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, scale, scale, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl_shader_set_coords(gl, &gl->coords, &mymat);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}

static void lakka_set_texture(void *data, bool enable)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
}

void lakka_render(void *data)
{
   int i, j, k;
   struct font_output_list msg;
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   update_tweens((float)rgui->delta/10000);

   gl_t *gl = (gl_t*)driver.video_data;

   glViewport(0, 0, gl->win_width, gl->win_height);

   lakka_draw_background(gl);

   for(i = 0; i < num_categories; i++)
   {
      // draw items
      for(j = 0; j < categories[i].num_items; j++)
      {
         lakka_draw_icon(gl, 
            categories[i].items[j].icon, 
            156 + HSPACING*(i+1) + all_categories_x - dim/2.0, 
            300 + categories[i].items[j].y + dim/2.0, 
            categories[i].items[j].alpha, 
            0, 
            categories[i].items[j].zoom);

         if (i == menu_active_category && j == categories[i].active_item && depth == 1) // performance improvement
         {
            for(k = 0; k < categories[i].items[j].num_subitems; k++)
            {
               if (k == 0 && g_extern.main_is_init
                     && !g_extern.libretro_dummy
                     && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)
               {
                  lakka_draw_icon(gl, 
                     resume_icon, 
                     156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                     categories[i].items[j].subitems[k].alpha, 
                     0, 
                     categories[i].items[j].subitems[k].zoom);
                  lakka_draw_text(gl, 
                     resume_label, 
                     156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + 15, 
                     1, 
                     categories[i].items[j].subitems[k].alpha);
               }
               else if (k == 0)
               {
                  lakka_draw_icon(gl, 
                        run_icon, 
                        156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                        300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                        categories[i].items[j].subitems[k].alpha, 
                        0, 
                        categories[i].items[j].subitems[k].zoom);
                  lakka_draw_text(gl, 
                        run_label, 
                        156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                        300 + categories[i].items[j].subitems[k].y + 15, 
                        1, 
                        categories[i].items[j].subitems[k].alpha);
               }
               else if (g_extern.main_is_init && 
                     !g_extern.libretro_dummy &&
                     strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)
               {
                  lakka_draw_icon(gl, 
                        categories[i].items[j].subitems[k].icon, 
                        156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                        300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                        categories[i].items[j].subitems[k].alpha, 
                        0, 
                        categories[i].items[j].subitems[k].zoom);
                  /*if category.prefix ~= "settings" and  (k == 2 or k == 3) and item.slot == -1 then
                    love.graphics.print(subitem.name .. " <" .. item.slot .. " (auto)>", 256 + (HSPACING*(i+1)) + all_categories.x, 300-15 + subitem.y)
                    elseif category.prefix ~= "settings" and  (k == 2 or k == 3) then
                    love.graphics.print(subitem.name .. " <" .. item.slot .. ">", 256 + (HSPACING*(i+1)) + all_categories.x, 300-15 + subitem.y)
                    else*/
                  lakka_draw_text(gl, 
                        categories[i].items[j].subitems[k].out, 
                        156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                        300 + categories[i].items[j].subitems[k].y + 15, 
                        1, 
                        categories[i].items[j].subitems[k].alpha);
                  /*end*/
               }
            }
         }

         if (depth == 0)
         {
            if (i == menu_active_category &&
                  j > categories[menu_active_category].active_item - 4 &&
                  j < categories[menu_active_category].active_item + 10) // performance improvement
               lakka_draw_text(gl, 
                     categories[i].items[j].out, 
                     156 + HSPACING*(i+1) + all_categories_x + dim/2.0, 
                     300 + categories[i].items[j].y + 15, 
                     1, 
                     categories[i].items[j].alpha);
         }
         else
         {
            lakka_draw_icon(gl,
                  arrow_icon, 
                  156 + (HSPACING*(i+1)) + all_categories_x + 150 +-dim/2.0, 
                  300 + categories[i].items[j].y + dim/2.0, 
                  categories[i].items[j].alpha,
                  0,
                  categories[i].items[j].zoom);
         }
      }

      // draw category
      lakka_draw_icon(gl, 
            categories[i].icon, 
            156 + (HSPACING*(i+1)) + all_categories_x - dim/2.0, 
            300 + dim/2.0, 
            categories[i].alpha, 
            0, 
            categories[i].zoom);
   }

   msg = (depth == 0) ? categories[menu_active_category].out : categories[menu_active_category].items[categories[menu_active_category].active_item].out;
   lakka_draw_text(gl, msg, 15.0, 40.0, 1, 1.0);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void lakka_init_assets(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   settings_icon = png_texture_load("/usr/share/retroarch/settings.png", &dim, &dim);
   arrow_icon = png_texture_load("/usr/share/retroarch/arrow.png", &dim, &dim);
   run_icon = png_texture_load("/usr/share/retroarch/run.png", &dim, &dim);
   resume_icon = png_texture_load("/usr/share/retroarch/resume.png", &dim, &dim);
   savestate_icon = png_texture_load("/usr/share/retroarch/savestate.png", &dim, &dim);
   loadstate_icon = png_texture_load("/usr/share/retroarch/loadstate.png", &dim, &dim);
   screenshot_icon = png_texture_load("/usr/share/retroarch/screenshot.png", &dim, &dim);
   reload_icon = png_texture_load("/usr/share/retroarch/reload.png", &dim, &dim);

   font_driver->render_msg(font, "Run", &run_label);
   font_driver->render_msg(font, "Resume", &resume_label);
}

void lakka_init_settings(void)
{
   gl_t *gl = (gl_t*)driver.video_data;

   menu_category mcat;
   mcat.name = "Settings";
   mcat.icon = settings_icon;
   mcat.alpha = 1.0;
   mcat.zoom = C_ACTIVE_ZOOM;
   mcat.active_item = 0;
   mcat.num_items   = 0;
   mcat.items       = calloc(mcat.num_items, sizeof(menu_item));
   struct font_output_list out;
   font_driver->render_msg(font, mcat.name, &out);
   mcat.out = out;
   categories[0] = mcat;
}

char * str_replace ( const char *string, const char *substr, const char *replacement)
{
   char *tok, *newstr, *oldstr, *head;

   /* if either substr or replacement is NULL, duplicate string a let caller handle it */
   if ( substr == NULL || replacement == NULL )
      return strdup (string);

   newstr = strdup (string);
   head = newstr;
   while ( (tok = strstr ( head, substr )))
   {
      oldstr = newstr;
      newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
      /*failed to alloc mem, free old string and return NULL */
      if (newstr == NULL)
      {
         free (oldstr);
         return NULL;
      }
      memcpy ( newstr, oldstr, tok - oldstr );
      memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
      memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
      memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen( replacement );
      free (oldstr);
   }
   return newstr;
}

void lakka_init_items(int i, menu_category *mcat, core_info_t corenfo, char* gametexturepath, char* path)
{
   int num_items, j, n;
   gl_t *gl = (gl_t*)driver.video_data;
   struct string_list *list = (struct string_list*)dir_list_new(path, corenfo.supported_extensions, true);

   dir_list_sort(list, true);

   num_items = list ? list->size : 0;

   for (j = 0; j < num_items; j++)
   {
      if (list->elems[j].attr.b) // is a directory
         lakka_init_items(i, mcat, corenfo, gametexturepath, list->elems[j].data);
      else
      {
         struct font_output_list out;

         n = mcat->num_items;
         mcat->num_items++;
         mcat->items = realloc(mcat->items, mcat->num_items * sizeof(menu_item));

         mcat->items[n].name  = path_basename(list->elems[j].data);
         mcat->items[n].rom   = list->elems[j].data;
         mcat->items[n].icon  = png_texture_load(gametexturepath, &dim, &dim);
         mcat->items[n].alpha = i != menu_active_category ? 0 : n ? 0.5 : 1;
         mcat->items[n].zoom  = n ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
         mcat->items[n].y     = n ? VSPACING*(3+n) : VSPACING*2.5;
         mcat->items[n].active_subitem = 0;
         mcat->items[n].num_subitems   = 5;
         mcat->items[n].subitems       = calloc(mcat->items[n].num_subitems, sizeof(menu_subitem));

         for (int k = 0; k < mcat->items[n].num_subitems; k++)
         {
            switch (k)
            {
               case 0:
                  mcat->items[n].subitems[k].name = "Run";
                  mcat->items[n].subitems[k].icon = run_icon;
                  break;
               case 1:
                  mcat->items[n].subitems[k].name = "Save State";
                  mcat->items[n].subitems[k].icon = savestate_icon;
                  break;
               case 2:
                  mcat->items[n].subitems[k].name = "Load State";
                  mcat->items[n].subitems[k].icon = loadstate_icon;
                  break;
               case 3:
                  mcat->items[n].subitems[k].name = "Take Screenshot";
                  mcat->items[n].subitems[k].icon = screenshot_icon;
                  break;
               case 4:
                  mcat->items[n].subitems[k].name = "Reload";
                  mcat->items[n].subitems[k].icon = reload_icon;
                  break;
            }
            mcat->items[n].subitems[k].alpha = 0;
            mcat->items[n].subitems[k].zoom = k == mcat->items[n].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
            mcat->items[n].subitems[k].y = k == 0 ? VSPACING*2.5 : VSPACING*(3+k);
            struct font_output_list out;
            font_driver->render_msg(font, mcat->items[n].subitems[k].name, &out);
            mcat->items[n].subitems[k].out = out;
         }

         font_driver->render_msg(font, mcat->items[n].name, &out);
         mcat->items[n].out = out;
      }
   }
}

static void *lakka_init(void)
{
   int i;
   gl_t *gl;
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));
   if (!rgui)
      return NULL;

   gl = (gl_t*)driver.video_data;

   init_font(gl, g_settings.video.font_path, 6, 1440, 900);

   menu_init_core_info(rgui);

   rgui->core_info = core_info_list_new(*rgui->libretro_dir ? rgui->libretro_dir : "/usr/lib/libretro");

   num_categories = rgui->core_info ? rgui->core_info->count + 1 : 1;

   lakka_init_assets(rgui);

   categories = realloc(categories, num_categories * sizeof(menu_category));

   lakka_init_settings();

   for (i = 0; i < num_categories-1; i++)
   {
      char core_id[256], texturepath[256], gametexturepath[256];
      menu_category mcat;
      struct font_output_list out;
      core_info_t corenfo = rgui->core_info->list[i];

      strcpy(core_id, basename(corenfo.path));
      strcpy(core_id, str_replace(core_id, ".so", ""));
      strcpy(core_id, str_replace(core_id, ".dll", ""));
      strcpy(core_id, str_replace(core_id, ".dylib", ""));
      strcpy(core_id, str_replace(core_id, "-libretro", ""));
      strcpy(core_id, str_replace(core_id, "_libretro", ""));
      strcpy(core_id, str_replace(core_id, "libretro-", ""));
      strcpy(core_id, str_replace(core_id, "libretro_", ""));

      strcpy(texturepath, "/usr/share/retroarch/");
      strcat(texturepath, core_id);
      strcat(texturepath, ".png");

      strcpy(gametexturepath, "/usr/share/retroarch/");
      strcat(gametexturepath, core_id);
      strcat(gametexturepath, "-game.png");

      mcat.name        = corenfo.display_name;
      mcat.libretro    = corenfo.path;
      mcat.icon        = png_texture_load(texturepath, &dim, &dim);
      mcat.alpha       = 0.5;
      mcat.zoom        = C_PASSIVE_ZOOM;
      mcat.active_item = 0;
      mcat.num_items   = 0;
      mcat.items       = calloc(mcat.num_items, sizeof(menu_item));

      font_driver->render_msg(font, mcat.name, &out);
      mcat.out = out;

      lakka_init_items(i+1, &mcat, corenfo, gametexturepath, g_settings.content_directory);

      categories[i+1] = mcat;
   }

   rgui->last_time = rarch_get_time_usec();

   return rgui;
}

static void lakka_free_assets(void *data)
{
}

static void lakka_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (rgui->alloc_font)
      free((uint8_t*)rgui->font);
}

static int lakka_input_postprocess(void *data, uint64_t old_state)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   int ret = 0;

   if ((rgui && rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   return ret;
}


const menu_ctx_driver_t menu_ctx_lakka = {
   lakka_set_texture,
   NULL,
   lakka_render,
   NULL,
   lakka_init,
   lakka_free,
   lakka_init_assets,
   lakka_free_assets,
   NULL,
   NULL,
   lakka_input_postprocess,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   &menu_ctx_backend_lakka,
   "lakka",
};
