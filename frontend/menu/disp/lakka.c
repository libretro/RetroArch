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
#include <png.h>

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

// Category variables
menu_category_t *categories;
int depth = 0;
int num_categories = 0;
int menu_active_category = 0;
int dim = 192;
float all_categories_x = 0;

// Font variables
void *font;
const gl_font_renderer_t *font_ctx;
const font_renderer_driver_t *font_driver;
GLuint font_tex;
GLint max_font_size;
int font_tex_w, font_tex_h;
uint32_t *font_tex_buf;
char font_last_msg[256];
int font_last_width, font_last_height;
struct font_output_list run_label;
struct font_output_list resume_label;

//GL-specific variables
GLfloat font_color[16];
GLfloat font_color_dark[16];
GLuint texture;

GLuint settings_icon;
GLuint arrow_icon;
GLuint run_icon;
GLuint resume_icon;
GLuint savestate_icon;
GLuint loadstate_icon;
GLuint screenshot_icon;
GLuint reload_icon;

typedef float (*easingFunc)(float, float, float, float);

typedef struct
{
   int    alive;
   float  duration;
   float  running_since;
   float  initial_value;
   float  target_value;
   float* subject;
   easingFunc easing;
} tween_t;

static tween_t* tweens = NULL;
int numtweens = 0;

static float inOutQuad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

static void add_tween(float duration, float target_value, float* subject, easingFunc easing)
{
   tween_t *tween;

   numtweens++;

   tweens = (tween_t*)realloc(tweens, numtweens * sizeof(tween_t));
   tween = (tween_t*)&tweens[numtweens-1];

   if (!tween)
      return;

   tween->alive = 1;
   tween->duration = duration;
   tween->running_since = 0;
   tween->initial_value = *subject;
   tween->target_value = target_value;
   tween->subject = subject;
   tween->easing = easing;
}

static void update_tween(void *data, float dt)
{
   tween_t *tween = (tween_t*)data;

   if (!tween)
      return;

#if 0
   RARCH_LOG("delta: %f\n", dt);
   RARCH_LOG("tween running since: %f\n", tween->running_since);
   RARCH_LOG("tween duration: %f\n", tween->duration);
#endif

   if (tween->running_since < tween->duration)
   {
      tween->running_since += dt;
      *tween->subject = tween->easing(
            tween->running_since,
            tween->initial_value,
            tween->target_value - tween->initial_value,
            tween->duration);
      if (tween->running_since >= tween->duration)
         *tween->subject = tween->target_value;
   }
}

static void update_tweens(float dt)
{
   int i, active_tweens;

   active_tweens = 0;
   for(i = 0; i < numtweens; i++)
   {
      update_tween(&tweens[i], dt);
      active_tweens += tweens[i].running_since < tweens[i].duration ? 1 : 0;
   }

   if (numtweens && !active_tweens)
      numtweens = 0;
}

// Move the categories left or right depending on the menu_active_category variable
void lakka_switch_categories(void)
{
   int i, j;

   // translation
   add_tween(DELAY, -menu_active_category * HSPACING, &all_categories_x, &inOutQuad);

   // alpha tweening
   for (i = 0; i < num_categories; i++)
   {
      float ca, cz;
      menu_category_t *category = (menu_category_t*)&categories[i];

      ca = (i == menu_active_category) ? 1.0 : 0.5;
      cz = (i == menu_active_category) ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      add_tween(DELAY, ca, &category->alpha, &inOutQuad);
      add_tween(DELAY, cz, &category->zoom,  &inOutQuad);

      for (j = 0; j < category->num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
            (j == category->active_item) ? 1.0 : 0.5;

         add_tween(DELAY, ia, &category->items[j].alpha, &inOutQuad);
      }
   }
}

void lakka_switch_items(void)
{
   int j;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   for (j = 0; j < active_category->num_items; j++)
   {
      float ia, iz, iy;
      menu_item_t *active_item = (menu_item_t*)&active_category->items[j];

      ia = (j == active_category->active_item) ? 1.0 : 0.5;
      iz = (j == active_category->active_item) ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
      iy = (j == active_category->active_item) ? VSPACING*2.5 :
         (j  < active_category->active_item) ? VSPACING*(j - active_category->active_item - 1) :
         VSPACING*(j - active_category->active_item + 3);

      add_tween(DELAY, ia, &active_item->alpha, &inOutQuad);
      add_tween(DELAY, iz, &active_item->zoom,  &inOutQuad);
      add_tween(DELAY, iy, &active_item->y,     &inOutQuad);
   }
}

void lakka_switch_subitems(void)
{
   int k;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];
   menu_item_t *item = (menu_item_t*)&active_category->items[active_category->active_item];

   for (k = 0; k < item->num_subitems; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

      if (k < item->active_subitem)
      {
         // Above items
         add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k - item->active_subitem + 2), &subitem->y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad);
      }
      else if (k == item->active_subitem)
      {
         // Active item
         add_tween(DELAY, 1.0, &subitem->alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*2.5, &subitem->y, &inOutQuad);
         add_tween(DELAY, I_ACTIVE_ZOOM, &subitem->zoom, &inOutQuad);
      }
      else if (k > item->active_subitem)
      {
         // Under items
         add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k - item->active_subitem + 3), &subitem->y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad);
      }
   }
}

void lakka_reset_submenu(void)
{
   int i, j, k;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   if (!(
            g_extern.main_is_init
            && !g_extern.libretro_dummy
            && strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0))
   {

      // Keeps active submenu state (do we really want that?)
      active_category->items[active_category->active_item].active_subitem = 0;
      for (i = 0; i < num_categories; i++)
      {
         menu_category_t *category = (menu_category_t*)&categories[i];

         for (j = 0; j < category->num_items; j++)
         {
            for (k = 0; k < category->items[j].num_subitems; k++)
            {
               menu_subitem_t *subitem = (menu_subitem_t*)&category->items[j].subitems[k];

               subitem->alpha = 0;
               subitem->zoom = k == category->items[j].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
               subitem->y = k == 0 ? VSPACING * 2.5 : VSPACING * (3+k);
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
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (i == menu_active_category)
      {
         add_tween(DELAY, 1.0, &category->alpha, &inOutQuad);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               for (k = 0; k < category->items[j].num_subitems; k++)
               {
                  menu_subitem_t *subitem = (menu_subitem_t*)&category->items[j].subitems[k];

                  if (k == category->items[j].active_subitem)
                  {
                     add_tween(DELAY, 1.0, &subitem->alpha, &inOutQuad);
                     add_tween(DELAY, I_ACTIVE_ZOOM, &subitem->zoom, &inOutQuad);
                  }
                  else
                  {
                     add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad);
                     add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad);
                  }
               }
            }
            else
               add_tween(DELAY, 0, &category->items[j].alpha, &inOutQuad);
         }
      }
      else
         add_tween(DELAY, 0, &category->alpha, &inOutQuad);
   }
}

void lakka_close_submenu(void)
{
   int i, j, k;
   add_tween(DELAY, -HSPACING * menu_active_category, &all_categories_x, &inOutQuad);
   
   for (i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (i == menu_active_category)
      {
         add_tween(DELAY, 1.0, &category->alpha, &inOutQuad);
         add_tween(DELAY, C_ACTIVE_ZOOM, &category->zoom, &inOutQuad);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               add_tween(DELAY, 1.0, &category->items[j].alpha, &inOutQuad);

               for (k = 0; k < category->items[j].num_subitems; k++)
                  add_tween(DELAY, 0, &category->items[j].subitems[k].alpha, &inOutQuad);
            }
            else
               add_tween(DELAY, 0.5, &category->items[j].alpha, &inOutQuad);
         }
      }
      else
      {
         add_tween(DELAY, 0.5, &category->alpha, &inOutQuad);
         add_tween(DELAY, C_PASSIVE_ZOOM, &category->zoom, &inOutQuad);

         for (j = 0; j < category->num_items; j++)
            add_tween(DELAY, 0, &category->items[j].alpha, &inOutQuad);
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
   gl_t *gl = (gl_t*)data;
   (void)win_width;
   (void)win_height;
   (void)font_size;

   if (!g_settings.video.font_enable)
      return false;

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
   GLfloat scale_factor, lx, hx, ly, hy, shift_x, shift_y;
   
   scale_factor = scale;
   lx = pos_x;
   hx = (GLfloat)font_last_width * scale_factor / gl->vp.width + lx;
   ly = pos_y;
   hy = (GLfloat)font_last_height * scale_factor / gl->vp.height + ly;

   font_vertex[0] = lx;
   font_vertex[2] = hx;
   font_vertex[4] = lx;
   font_vertex[6] = hx;
   font_vertex[1] = hy;
   font_vertex[3] = hy;
   font_vertex[5] = ly;
   font_vertex[7] = ly;

   shift_x = 2.0f / gl->vp.width;
   shift_y = 2.0f / gl->vp.height;
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

static void lakka_draw_text(void *data, struct font_output_list *out, float x, float y, float scale, float alpha)
{
   int i;
   struct font_output *head;
   struct font_rect geom;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};
   gl_t *gl = (gl_t*)data;

   if (!font || !gl || !out)
      return;

   for (i = 0; i < 4; i++)
   {
      font_color[4 * i + 0] = 1.0;
      font_color[4 * i + 1] = 1.0;
      font_color[4 * i + 2] = 1.0;
      font_color[4 * i + 3] = alpha;
   }

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   glEnable(GL_BLEND);

   GLfloat font_vertex[8]; 
   GLfloat font_vertex_dark[8]; 
   GLfloat font_tex_coords[8];

   glBindTexture(GL_TEXTURE_2D, font_tex);

   gl->coords.tex_coord = font_tex_coords;

   head = (struct font_output*)out->head;

   calculate_msg_geometry(head, &geom);
   adjust_power_of_two(gl, &geom);
   blit_fonts(gl, head, &geom);

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

   gl_set_projection(gl, &ortho, true);
}

void lakka_draw_background(void *data)
{
   gl_t *gl = (gl_t*)data;

   glEnable(GL_BLEND);

   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = background_color;
   glBindTexture(GL_TEXTURE_2D, 0);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

void lakka_draw_icon(void *data, GLuint texture, float x, float y, float alpha, float rotation, float scale)
{
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

   gl_t *gl = (gl_t*)data;

   glViewport(x, gl->win_height - y, dim, dim);

   glEnable(GL_BLEND);

   gl->coords.vertex = vtest;
   gl->coords.tex_coord = vtest;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   if (gl->shader && gl->shader->use)
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

static void lakka_frame(void)
{
   int i, j, k;
   struct font_output_list *msg;
   gl_t *gl = (gl_t*)driver.video_data;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   if (!driver.menu || !gl)
      return;

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   lakka_draw_background(gl);

   for(i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      // draw items
      for(j = 0; j < category->num_items; j++)
      {
         menu_item_t *item = (menu_item_t*)&category->items[j];

         lakka_draw_icon(gl, 
            item->icon, 
            156 + HSPACING*(i+1) + all_categories_x - dim/2.0, 
            300 + item->y + dim/2.0, 
            item->alpha, 
            0, 
            item->zoom);

         if (i == menu_active_category && j == category->active_item && depth == 1) // performance improvement
         {
            for(k = 0; k < item->num_subitems; k++)
            {
               menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

               if (k == 0 && g_extern.main_is_init
                     && !g_extern.libretro_dummy
                     && strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0)
               {
                  lakka_draw_icon(gl, 
                     resume_icon, 
                     156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                     300 + subitem->y + dim/2.0, 
                     subitem->alpha, 
                     0, 
                     subitem->zoom);
                  lakka_draw_text(gl, 
                     &resume_label, 
                     156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                     300 + subitem->y + 15, 
                     1, 
                     subitem->alpha);
               }
               else if (k == 0)
               {
                  lakka_draw_icon(gl, 
                        run_icon, 
                        156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                        300 + subitem->y + dim/2.0, 
                        subitem->alpha, 
                        0, 
                        subitem->zoom);
                  lakka_draw_text(gl, 
                        &run_label, 
                        156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                        300 + subitem->y + 15, 
                        1, 
                        subitem->alpha);
               }
               else if (g_extern.main_is_init && 
                     !g_extern.libretro_dummy &&
                     strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0)
               {
                  lakka_draw_icon(gl, 
                        subitem->icon, 
                        156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                        300 + subitem->y + dim/2.0, 
                        subitem->alpha, 
                        0, 
                        subitem->zoom);
                  lakka_draw_text(gl, 
                        &subitem->out, 
                        156 + HSPACING * (i+2) + all_categories_x + dim/2.0, 
                        300 + subitem->y + 15, 
                        1, 
                        subitem->alpha);
                  /*end*/
               }
            }
         }

         if (depth == 0)
         {
            if (i == menu_active_category &&
                  j > active_category->active_item - 4 &&
                  j < active_category->active_item + 10) // performance improvement
               lakka_draw_text(gl, 
                     &item->out, 
                     156 + HSPACING * (i+1) + all_categories_x + dim/2.0, 
                     300 + item->y + 15, 
                     1, 
                     item->alpha);
         }
         else
         {
            lakka_draw_icon(gl,
                  arrow_icon, 
                  156 + (HSPACING*(i+1)) + all_categories_x + 150 +-dim/2.0, 
                  300 + item->y + dim/2.0, 
                  item->alpha,
                  0,
                  item->zoom);
         }
      }

      // draw category
      lakka_draw_icon(gl, 
            category->icon, 
            156 + (HSPACING*(i+1)) + all_categories_x - dim/2.0, 
            300 + dim/2.0, 
            category->alpha, 
            0, 
            category->zoom);
   }

   if ((depth == 0))
      lakka_draw_text(gl, &active_category->out, 15.0, 40.0, 1, 1.0);
   else
      lakka_draw_text(gl, &active_category->items[active_category->active_item].out, 15.0, 40.0, 1, 1.0);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

// thanks to https://github.com/DavidEGrayson/ahrs-visualizer/blob/master/png_texture.cpp
static GLuint png_texture_load(const char * file_name, int * width, int * height)
{
    png_byte header[8];

    FILE *fp = fopen(file_name, "rb");
    if (fp == 0)
    {
        perror(file_name);
        return 0;
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8))
    {
        fprintf(stderr, "error: %s is not a PNG.\n", file_name);
        fclose(fp);
        return 0;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "error: png_create_read_struct returned 0.\n");
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        return 0;
    }

    // the code in this if statement gets called if libpng encounters an error
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "error from libpng\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // init png reading
    png_init_io(png_ptr, fp);

    // let libpng know you already read the first 8 bytes
    png_set_sig_bytes(png_ptr, 8);

    // read all the info up to the image data
    png_read_info(png_ptr, info_ptr);

    // variables to pass to get info
    int bit_depth, color_type;
    png_uint_32 temp_width, temp_height;

    // get info about png
    png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth, &color_type,
        NULL, NULL, NULL);

    if (width){ *width = temp_width; }
    if (height){ *height = temp_height; }

    //printf("%s: %lux%lu %d\n", file_name, temp_width, temp_height, color_type);

    if (bit_depth != 8)
    {
        fprintf(stderr, "%s: Unsupported bit depth %d.  Must be 8.\n", file_name, bit_depth);
        return 0;
    }

    GLint format;
    switch(color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        format = GL_RGB;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        format = GL_RGBA;
        break;
    default:
        fprintf(stderr, "%s: Unknown libpng color type %d.\n", file_name, color_type);
        return 0;
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes-1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    png_byte * image_data = (png_byte *)malloc(rowbytes * temp_height * sizeof(png_byte)+15);
    if (image_data == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG image data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    png_byte ** row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
    if (row_pointers == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return 0;
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (unsigned int i = 0; i < temp_height; i++)
    {
        row_pointers[temp_height - 1 - i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    png_read_image(png_ptr, row_pointers);

    // Generate the OpenGL texture object
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, temp_width, temp_height, 0, format, GL_UNSIGNED_BYTE, image_data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);
    fclose(fp);
    return texture;
}

static void lakka_free_assets(void *data)
{
   (void)data;

   glDeleteTextures(1, &settings_icon);
   glDeleteTextures(1, &arrow_icon);
   glDeleteTextures(1, &run_icon);
   glDeleteTextures(1, &resume_icon);
   glDeleteTextures(1, &savestate_icon);
   glDeleteTextures(1, &loadstate_icon);
   glDeleteTextures(1, &screenshot_icon);
   glDeleteTextures(1, &reload_icon);

   if (numtweens)
      free(tweens);
}

static void lakka_init_assets(void *data)
{
   char path[256], dirpath[256];;
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   fill_pathname_join(dirpath, g_settings.assets_directory, "lakka", sizeof(dirpath));
   fill_pathname_slash(dirpath, sizeof(dirpath));

   fill_pathname_join(path, dirpath, "settings.png", sizeof(path));
   settings_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "arrow.png", sizeof(path));
   arrow_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "run.png", sizeof(path));
   run_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "resume.png", sizeof(path));
   resume_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "savestate.png", sizeof(path));
   savestate_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "loadstate.png", sizeof(path));
   loadstate_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "screenshot.png", sizeof(path));
   screenshot_icon = png_texture_load(path, &dim, &dim);
   fill_pathname_join(path, dirpath, "reload.png", sizeof(path));
   reload_icon = png_texture_load(path, &dim, &dim);

   font_driver->render_msg(font, "Run", &run_label);
   font_driver->render_msg(font, "Resume", &resume_label);
}

void lakka_init_settings(void)
{
   menu_category_t *category = (menu_category_t*)&categories[0];

   strlcpy(category->name, "Settings", sizeof(category->name));
   category->icon = settings_icon;
   category->alpha = 1.0;
   category->zoom = C_ACTIVE_ZOOM;
   category->active_item = 0;
   category->num_items   = 0;
   category->items       = (menu_item_t*)calloc(category->num_items, sizeof(menu_item_t));

   font_driver->render_msg(font, category->name, &category->out);
}

static char * str_replace ( const char *string, const char *substr, const char *replacement)
{
   char *tok, *newstr, *oldstr, *head;

   /* if either substr or replacement is NULL, duplicate string a let caller handle it */
   if (!substr || !replacement)
      return strdup (string);

   newstr = strdup (string);
   head = newstr;
   while ( (tok = strstr ( head, substr )))
   {
      oldstr = newstr;
      newstr = (char*)malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);

      if (!newstr)
      {
         /*failed to alloc mem, free old string and return NULL */
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

static void lakka_init_items(int i, menu_category_t *category, core_info_t *info, const char* gametexturepath, const char* path)
{
   int num_items, j, n, k;
   struct string_list *list = (struct string_list*)dir_list_new(path, info->supported_extensions, true);

   dir_list_sort(list, true);

   num_items = list ? list->size : 0;

   for (j = 0; j < num_items; j++)
   {
      if (list->elems[j].attr.b) // is a directory
         lakka_init_items(i, category, info, gametexturepath, list->elems[j].data);
      else
      {
         struct font_output_list out;
         menu_item_t *item;

         n = category->num_items;

         category->num_items++;
         category->items = (menu_item_t*)realloc(category->items, category->num_items * sizeof(menu_item_t));
         item = (menu_item_t*)&category->items[n];

         strlcpy(item->name, path_basename(list->elems[j].data), sizeof(item->name));
         strlcpy(item->rom, list->elems[j].data, sizeof(item->rom));
         item->icon           = png_texture_load(gametexturepath, &dim, &dim);
         item->alpha          = i != menu_active_category ? 0 : n ? 0.5 : 1;
         item->zoom           = n ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
         item->y              = n ? VSPACING*(3+n) : VSPACING*2.5;
         item->active_subitem = 0;
         item->num_subitems   = 5;
         item->subitems       = (menu_subitem_t*)calloc(item->num_subitems, sizeof(menu_subitem_t));

         for (k = 0; k < item->num_subitems; k++)
         {
            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

            switch (k)
            {
               case 0:
                  strlcpy(subitem->name, "Run", sizeof(subitem->name));
                  subitem->icon = run_icon;
                  break;
               case 1:
                  strlcpy(subitem->name, "Save State", sizeof(subitem->name));
                  subitem->icon = savestate_icon;
                  break;
               case 2:
                  strlcpy(item->subitems[k].name, "Load State", sizeof(item->subitems[k].name));
                  item->subitems[k].icon = loadstate_icon;
                  break;
               case 3:
                  strlcpy(item->subitems[k].name, "Take Screenshot", sizeof(item->subitems[k].name));
                  item->subitems[k].icon = screenshot_icon;
                  break;
               case 4:
                  strlcpy(item->subitems[k].name, "Reset", sizeof(item->subitems[k].name));
                  item->subitems[k].icon = reload_icon;
                  break;
            }
            subitem->alpha = 0;
            subitem->zoom = k == item->active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
            subitem->y = k == 0 ? VSPACING * 2.5 : VSPACING * (3 + k);
            font_driver->render_msg(font, subitem->name, &out);
            memcpy(&subitem->out, &out, sizeof(struct font_output_list));
         }

         font_driver->render_msg(font, item->name, &out);
         memcpy(&item->out, &out, sizeof(struct font_output_list));
      }
   }
}



static void lakka_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   lakka_free_assets(rgui);

   if (rgui->alloc_font)
      free((uint8_t*)rgui->font);
}

static int lakka_input_postprocess(uint64_t old_state)
{
   int ret = 0;

   if ((driver.menu && driver.menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   return ret;
}

static void lakka_init_core_info(void *data)
{
   core_info_list_t *core;
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;

   rgui->core_info = (core_info_list_t*)core_info_list_new(*g_settings.libretro_directory ? g_settings.libretro_directory : "/usr/lib/libretro");

   if (rgui->core_info)
   {
      core = (core_info_list_t*)rgui->core_info;
      num_categories = rgui->core_info ? core->count + 1 : 1;
   }
}

static void *lakka_init(void)
{
   int i, j;
   gl_t *gl;
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));
   if (!rgui)
      return NULL;

   gl = (gl_t*)driver.video_data;

   init_font(gl, g_settings.video.font_path, g_settings.video.font_size, gl->win_width, gl->win_height);

   lakka_init_core_info(rgui);
   lakka_init_assets(rgui);

   if (categories)
   {
      for (i = 1; i < num_categories; i++)
      {
         menu_category_t *category = (menu_category_t*)&categories[i];

         char core_id[256], texturepath[256], gametexturepath[256], dirpath[256];
         core_info_t *info;
         core_info_list_t *info_list;

         fill_pathname_join(dirpath, g_settings.assets_directory, "lakka", sizeof(dirpath));
         fill_pathname_slash(dirpath, sizeof(dirpath));

         info_list = (core_info_list_t*)rgui->core_info;
         info = NULL;

         if (info_list)
            info = (core_info_t*)&info_list->list[i-1];

         strlcpy(core_id, basename(info->path), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, ".so", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, ".dll", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, ".dylib", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, "-libretro", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, "_libretro", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, "libretro-", ""), sizeof(core_id));
         strlcpy(core_id, str_replace(core_id, "libretro_", ""), sizeof(core_id));

         strlcpy(texturepath, dirpath, sizeof(texturepath));
         strlcat(texturepath, core_id, sizeof(texturepath));
         strlcat(texturepath, ".png", sizeof(texturepath));

         strlcpy(gametexturepath, dirpath, sizeof(gametexturepath));
         strlcat(gametexturepath, core_id, sizeof(gametexturepath));
         strlcat(gametexturepath, "-content.png", sizeof(gametexturepath));

         category->icon = png_texture_load(texturepath, &dim, &dim);
         font_driver->render_msg(font, category->name, &category->out);

         for (j = 0; j < category->num_items; j++)
         {
            menu_item_t *item = (menu_item_t*)&category->items[j];
            item->icon = png_texture_load(gametexturepath, &dim, &dim);
            font_driver->render_msg(font, item->name, &item->out);
         }
      }
      return rgui;
   }

   categories = (menu_category_t*)calloc(num_categories, sizeof(menu_category_t));

   lakka_init_settings();

   for (i = 1; i < num_categories; i++)
   {
      char core_id[256], texturepath[256], gametexturepath[256], dirpath[256];
      core_info_t *info;
      core_info_list_t *info_list;
      menu_category_t *category = (menu_category_t*)&categories[i];

      fill_pathname_join(dirpath, g_settings.assets_directory, "lakka", sizeof(dirpath));
      fill_pathname_slash(dirpath, sizeof(dirpath));

      info_list = (core_info_list_t*)rgui->core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      strlcpy(core_id, basename(info->path), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".so", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".dll", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, ".dylib", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "-libretro", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "_libretro", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "libretro-", ""), sizeof(core_id));
      strlcpy(core_id, str_replace(core_id, "libretro_", ""), sizeof(core_id));

      strlcpy(texturepath, dirpath, sizeof(texturepath));
      strlcat(texturepath, core_id, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      strlcpy(gametexturepath, dirpath, sizeof(gametexturepath));
      strlcat(gametexturepath, core_id, sizeof(gametexturepath));
      strlcat(gametexturepath, "-content.png", sizeof(gametexturepath));

      strlcpy(category->name, info->display_name, sizeof(category->name));
      strlcpy(category->libretro, info->path, sizeof(category->libretro));
      category->icon        = png_texture_load(texturepath, &dim, &dim);
      category->alpha       = 0.5;
      category->zoom        = C_PASSIVE_ZOOM;
      category->active_item = 0;
      category->num_items   = 0;
      category->items       = (menu_item_t*)calloc(category->num_items, sizeof(menu_item_t));

      font_driver->render_msg(font, category->name, &category->out);

      lakka_init_items(i, category, info, gametexturepath, g_settings.content_directory);
   }

   return rgui;
}

const menu_ctx_driver_t menu_ctx_lakka = {
   NULL,
   NULL,
   NULL,
   lakka_frame,
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
   lakka_init_core_info,
   &menu_ctx_backend_lakka,
   "lakka",
};
