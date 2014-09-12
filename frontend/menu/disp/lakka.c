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

#include "../../../settings_data.h"
#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "lakka.h"

// Category variables
menu_category_t *categories;
int depth = 0;
int num_categories = 0;
int menu_active_category = 0;
float all_categories_x = 0;
float global_alpha = 0;
float arrow_alpha = 0;
float hspacing;
float vspacing;
float c_active_zoom;
float c_passive_zoom;
float i_active_zoom;
float i_passive_zoom;
float lakka_font_size;
float margin_left;
float margin_top;
float title_margin_left;
float title_margin_top;
float label_margin_left;
float label_margin_top;
int icon_size;
char icon_dir[4];
float above_subitem_offset;
float above_item_offset;
float active_item_factor;
float under_item_offset;

// Font variables
void *font;
const gl_font_renderer_t *font_driver;
char font_path[PATH_MAX];

static const GLfloat vertex[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

static const GLfloat tex_coord[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0,
};

enum
{
   TEXTURE_MAIN = 0,
   TEXTURE_FONT,
   TEXTURE_BG,
   TEXTURE_SETTINGS,
   TEXTURE_SETTING,
   TEXTURE_SUBSETTING,
   TEXTURE_ARROW,
   TEXTURE_RUN,
   TEXTURE_RESUME,
   TEXTURE_SAVESTATE,
   TEXTURE_LOADSTATE,
   TEXTURE_SCREENSHOT,
   TEXTURE_RELOAD,
   TEXTURE_LAST
};

struct lakka_texture_item
{
   GLuint id;
   char path[PATH_MAX];
};

struct lakka_texture_item textures[TEXTURE_LAST];

static tween_t* tweens = NULL;
int numtweens = 0;

static void lakka_responsive(void)
{
   gl_t *gl = (gl_t*)driver.video_data;
   if (!gl)
      return;

   c_active_zoom = 1.0;
   c_passive_zoom = 0.5;
   i_active_zoom = 1.0;
   i_passive_zoom = 0.5;

   above_subitem_offset = 1.5;
   above_item_offset = -1.0;
   active_item_factor = 2.75;
   under_item_offset = 4.0;

   if (gl->win_width >= 3840)
   {
      icon_size = 256;
      hspacing = 400;
      vspacing = 128;
      lakka_font_size = 42.0;
      margin_left = 672.0;
      margin_top = 512;
      title_margin_left = 20.0;
      title_margin_top = 50.0;
      label_margin_left = 192;
      label_margin_top = 15;
      strcpy(icon_dir, "256");
      return;
   }

   if (gl->win_width >= 2560)
   {
      icon_size = 192;
      hspacing = 300;
      vspacing = 96;
      lakka_font_size = 32.0;
      margin_left = 448.0;
      margin_top = 384;
      title_margin_left = 15.0;
      title_margin_top = 40.0;
      label_margin_left = 144;
      label_margin_top = 11.0;
      strcpy(icon_dir, "192");
      return;
   }

   if (gl->win_width >= 1920)
   {
      icon_size = 128;
      hspacing = 200.0;
      vspacing = 64.0;
      lakka_font_size = 24;
      margin_left = 336.0;
      margin_top = 256;
      title_margin_left = 15.0;
      title_margin_top = 35.0;
      label_margin_left = 85;
      label_margin_top = 8.0;
      strcpy(icon_dir, "128");
      return;
   }

   if (gl->win_width <= 640)
   {
      icon_size = 64;
      hspacing = 100.0;
      vspacing = 32.0;
      lakka_font_size = 16;
      margin_left = 60.0;
      margin_top = 128.0;
      title_margin_left = 10.0;
      title_margin_top = 24.0;
      label_margin_left = 48;
      label_margin_top = 6.0;
      strcpy(icon_dir, "64");
      return;
   }

   icon_size = 96;
   hspacing = 150.0;
   vspacing = 48.0;
   lakka_font_size = 18;
   margin_left = 224;
   margin_top = 192;
   title_margin_left = 15.0;
   title_margin_top = 30.0;
   label_margin_left = 64;
   label_margin_top = 6.0;
   strcpy(icon_dir, "96");
}

static char *str_replace (const char *string, const char *substr, const char *replacement)
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
      memcpy(newstr, oldstr, tok - oldstr );
      memcpy(newstr + (tok - oldstr), replacement, strlen ( replacement ) );
      memcpy(newstr + (tok - oldstr) + strlen( replacement ), tok +
            strlen ( substr ), strlen ( oldstr ) -
            strlen ( substr ) - ( tok - oldstr ) );
      memset(newstr + strlen ( oldstr ) - strlen ( substr ) +
            strlen ( replacement ) , 0, 1 );
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen( replacement );
      free (oldstr);
   }
   return newstr;
}

float inOutQuad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

void add_tween(float duration, float target_value, float* subject,
      easingFunc easing, tweenCallback callback)
{
   tween_t *tween;
   tween_t *tweens_tmp;

   numtweens++;

   tweens_tmp = (tween_t*)realloc(tweens, numtweens * sizeof(tween_t));
   if (tweens_tmp != NULL)
   {
       tweens = tweens_tmp;
   }
   else // realloc failed
   {
      if (tweens != NULL)
      {
         free(tweens);
         tweens = NULL;
      }

      return;
   }
   
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
   tween->callback = callback;
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

      if (tween->easing)
         *tween->subject = tween->easing(
               tween->running_since,
               tween->initial_value,
               tween->target_value - tween->initial_value,
               tween->duration);

      if (tween->running_since >= tween->duration)
      {
         *tween->subject = tween->target_value;

         if (tween->callback)
            tween->callback();
      }
   }
}

static void update_tweens(float dt)
{
   int i, active_tweens;

   active_tweens = 0;

   for(i = 0; i < numtweens; i++)
   {
      update_tween(&tweens[i], dt);
      active_tweens += tweens[i].running_since <
         tweens[i].duration ? 1 : 0;
   }

   if (numtweens && !active_tweens)
      numtweens = 0;
}

static void lakka_draw_text(const char *str, float x,
      float y, float scale, float alpha)
{
   if (alpha > global_alpha)
      alpha = global_alpha;
   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver.video_data;
   if (!gl)
      return;

   if (x < -icon_size || x > gl->win_width + icon_size
         || y < -icon_size || y > gl->win_height + icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   struct font_params params = {0};
   params.x = x / gl->win_width;
   params.y = 1.0f - y / gl->win_height;

   params.scale = scale;
   params.color = FONT_COLOR_RGBA(255, 255, 255, (uint8_t)(255 * alpha));
   params.full_screen = true;

   if (font_driver)
      font_driver->render_msg(font, str, &params);
}

void lakka_draw_background(void)
{
   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, global_alpha,
      1.0f, 1.0f, 1.0f, global_alpha,
      1.0f, 1.0f, 1.0f, global_alpha,
      1.0f, 1.0f, 1.0f, global_alpha,
   };

   gl_t *gl = (gl_t*)driver.video_data;
   if (!gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

   glEnable(GL_BLEND);

   gl->coords.vertex = vertex;
   gl->coords.tex_coord = tex_coord;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_BG].id);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

void lakka_draw_icon(GLuint texture, float x, float y,
      float alpha, float rotation, float scale)
{
   if (alpha > global_alpha)
      alpha = global_alpha;

   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver.video_data;

   if (!gl)
      return;

   if (x < -icon_size || x > gl->win_width + icon_size
         || y < -icon_size || y > gl->win_height + icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   glViewport(x, gl->win_height - y, icon_size, icon_size);

   glEnable(GL_BLEND);

   gl->coords.vertex = vertex;
   gl->coords.tex_coord = tex_coord;
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

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &mymat);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}

static void lakka_draw_subitems(int i, int j)
{
   int k;
   menu_category_t *category = (menu_category_t*)&categories[i];
   menu_item_t *item = (menu_item_t*)&category->items[j];
   menu_category_t *active_category = (menu_category_t*)
      &categories[menu_active_category];
   menu_item_t *active_item = (menu_item_t*)
      &active_category->items[active_category->active_item];

   for(k = 0; k < item->num_subitems; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

      if (!subitem)
         continue;

      if (i && k == 0 && g_extern.main_is_init
            && !g_extern.libretro_dummy
            && strcmp(g_extern.fullpath, active_item->rom) == 0)
      {
         lakka_draw_icon(textures[TEXTURE_RESUME].id, 
            margin_left + hspacing*(i+2.25) +
            all_categories_x - icon_size/2.0,
            margin_top + subitem->y + icon_size/2.0,
            subitem->alpha,
            0, 
            subitem->zoom);
         lakka_draw_text("Resume",
            margin_left + hspacing*(i+2.25) +
            all_categories_x + label_margin_left,
            margin_top + subitem->y + label_margin_top,
            1, 
            subitem->alpha);
      }
      else if (k == 0 ||
            menu_active_category == 0 ||
            (g_extern.main_is_init && 
            !g_extern.libretro_dummy &&
            strcmp(g_extern.fullpath, active_item->rom) == 0))
      {
         lakka_draw_icon(subitem->icon, 
               margin_left + hspacing*(i+2.25) +
               all_categories_x - icon_size/2.0, 
               margin_top + subitem->y + icon_size/2.0, 
               subitem->alpha, 
               0, 
               subitem->zoom);
         lakka_draw_text(subitem->name, 
               margin_left + hspacing * (i+2.25) +
               all_categories_x + label_margin_left, 
               margin_top + subitem->y + label_margin_top, 
               1, 
               subitem->alpha);

         if (i && (k == 1 || k == 2))
         {
            char slot[256];
            if (g_settings.state_slot == -1)
               snprintf(slot, sizeof(slot), "%d (auto)", g_settings.state_slot);
            else
               snprintf(slot, sizeof(slot), "%d", g_settings.state_slot);
            lakka_draw_text(slot, 
                  margin_left + hspacing * (i+2.25) +
                  all_categories_x + label_margin_left + 400, 
                  margin_top + subitem->y + label_margin_top, 
                  1, 
                  subitem->alpha);
         }
      }

      if (subitem->setting)
      {
         char val[256];
         setting_data_get_string_representation(subitem->setting, val,
               sizeof(val));
         lakka_draw_text(val, 
               margin_left + hspacing * (i+2.25) +
               all_categories_x + label_margin_left + 400, 
               margin_top + subitem->y + label_margin_top, 
               1, 
               subitem->alpha);
      }
   }
}

static void lakka_draw_items(int i)
{
   int j;
   menu_category_t *category = (menu_category_t*)&categories[i];
   menu_category_t *active_category = (menu_category_t*)
      &categories[menu_active_category];
   menu_item_t *active_item = (menu_item_t*)
      &active_category->items[active_category->active_item];
    
   (void)active_item;

   for(j = 0; j < category->num_items; j++)
   {
      menu_item_t *item = (menu_item_t*)&category->items[j];

      if (!item)
         continue;

      if (i >= menu_active_category - 1 &&
         i <= menu_active_category + 1) /* performance improvement */
      {
         lakka_draw_icon(category->item_icon,
            margin_left + hspacing*(i+1) +
            all_categories_x - icon_size/2.0, 
            margin_top + item->y + icon_size/2.0, 
            item->alpha, 
            0, 
            item->zoom);

         if (depth == 0)
            lakka_draw_text(item->name,
               margin_left + hspacing * (i+1) +
               all_categories_x + label_margin_left, 
               margin_top + item->y + label_margin_top, 
               1, 
               item->alpha);
      }

      /* performance improvement */
      if (i == menu_active_category
            && j == category->active_item && depth == 1) 
         lakka_draw_subitems(i, j);
   }
}

static void lakka_draw_categories(void)
{
   int i;

   for(i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      /* draw items */
      lakka_draw_items(i);

      /* draw category icon */
      lakka_draw_icon(category->icon, 
            margin_left + (hspacing*(i+1)) +
            all_categories_x - icon_size/2.0,
            margin_top + icon_size/2.0, 
            category->alpha, 
            0, 
            category->zoom);
   }
}

static void lakka_frame(void)
{
   gl_t *gl = (gl_t*)driver.video_data;
   menu_category_t *active_category = (menu_category_t*)
      &categories[menu_active_category];
   menu_item_t *active_item;

   if (!driver.menu || !gl || !active_category)
      return;

   active_item = (menu_item_t*)
      &active_category->items[active_category->active_item];

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   lakka_draw_background();

   lakka_draw_categories();

   if (depth == 0)
      lakka_draw_text(active_category->name,
            title_margin_left, title_margin_top, 1, 1.0);
   else if (active_item)
      lakka_draw_text(active_item->name,
            title_margin_left, title_margin_top, 1, 1.0);

   lakka_draw_icon(textures[TEXTURE_ARROW].id,
        margin_left + hspacing*(menu_active_category+1) +
        all_categories_x + icon_size/2.0,
        margin_top + vspacing*active_item_factor +
        icon_size/2.0, arrow_alpha, 0, i_active_zoom);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static GLuint png_texture_load(const char * file_name)
{
   struct texture_image ti = {0};
   texture_image_load(&ti, file_name);

   /* Generate the OpenGL texture object */
   GLuint texture;
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ti.width, ti.height, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, ti.pixels);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   free(ti.pixels);

   return texture;
}

static void lakka_context_destroy(void *data)
{
   int i, j, k;
   gl_t *gl = (gl_t*)driver.video_data;

   (void)gl;

   for (i = 0; i < TEXTURE_LAST; i++)
      glDeleteTextures(1, &textures[i].id);

   for (i = 1; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      glDeleteTextures(1, &category->icon);
      glDeleteTextures(1, &category->item_icon);

      for (j = 0; j < category->num_items; j++)
      {
         menu_item_t *item;
         menu_subitem_t *subitem;

         item = (menu_item_t*)&category->items[j];

         if (!item)
            continue;

         for (k = 0; k < item->num_subitems; k++ )
         {
            subitem = (menu_subitem_t*)&item->subitems[k];

            if (subitem)
               glDeleteTextures(1, &subitem->icon);
         }
      }
   }

   if (font_driver)
   {
      font_driver->free(font);
      font_driver = NULL;
   }

   //if (numtweens)
   //   free(tweens);
}

void lakka_init_settings(void)
{
   rarch_setting_t *setting_data = (rarch_setting_t*)setting_data_get_list();

   menu_category_t *category = (menu_category_t*)&categories[0];

   strlcpy(category->name, "Settings", sizeof(category->name));
   category->alpha = 1.0;
   category->zoom = c_active_zoom;
   category->active_item = 0;
   category->num_items   = 0;
   category->items       = (menu_item_t*)
      calloc(category->num_items, sizeof(menu_item_t));

   int j, k, jj, kk;
   jj = 0;
   for (j = 0; j <= 512; j++)
   {
      rarch_setting_t group = (rarch_setting_t)setting_data[j];

      if (group.type == ST_GROUP)
      {
         category->num_items++;
         category->items = (menu_item_t*)
            realloc(category->items, category->num_items * sizeof(menu_item_t));

         menu_item_t *item  = (menu_item_t*)&category->items[jj];

         strlcpy(item->name, group.name, sizeof(item->name));
         item->alpha = jj ? 0.5 : 1.0;
         item->zoom = jj ? i_passive_zoom : i_active_zoom;
         item->y = jj ?
            vspacing*(under_item_offset+jj) : vspacing * active_item_factor;
         item->active_subitem = 0;
         item->num_subitems = 0;
         item->subitems = NULL;

         kk = 0;
         for (k = 0; k <= 512; k++)
         {
            rarch_setting_t setting = (rarch_setting_t)setting_data[k];

            if (setting.type != ST_SUB_GROUP && setting.group == group.name)
            {
               item->num_subitems++;

               item->subitems = (menu_subitem_t*)
                  realloc(item->subitems, 
                     item->num_subitems * sizeof(menu_subitem_t));

               menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[kk];

               strlcpy(subitem->name, setting.short_description, 
                     sizeof(subitem->name));
               subitem->alpha = kk ? 1.0 : 0.5;
               subitem->zoom = kk ? i_active_zoom : i_passive_zoom;
               subitem->y = kk ? vspacing * (kk + under_item_offset)
                  : vspacing * active_item_factor;

               subitem->setting = &setting_data[k];

               kk++;
            }
         }

         jj++;
      }
   }

   category->num_items++;
   category->items = (menu_item_t*)
      realloc(category->items, category->num_items * sizeof(menu_item_t));

   menu_item_t *itemq = (menu_item_t*)&category->items[jj];

   strlcpy(itemq->name, "Quit RetroArch", sizeof(itemq->name));
   itemq->alpha          = jj ? 0.5 : 1.0;
   itemq->zoom           = jj ? i_passive_zoom : i_active_zoom;
   itemq->y              = jj ? vspacing*(under_item_offset+jj) :
      vspacing * active_item_factor;
   itemq->active_subitem = 0;
   itemq->num_subitems   = 0;
}

void lakka_settings_context_reset(void)
{
   menu_item_t *item;
   int j, k;
   menu_category_t *category = (menu_category_t*)&categories[0];

   if (!category)
      return;

   category->icon = textures[TEXTURE_SETTINGS].id;
   category->item_icon = textures[TEXTURE_SETTING].id;

   for  (j = 0; j < category->num_items; j++)
   {
      item = (menu_item_t*)&category->items[j];

      for (k = 0; k < item->num_subitems; k++)
      {
         menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];
         subitem->icon = textures[TEXTURE_SUBSETTING].id;
      }
   }
}


static void lakka_context_reset(void *data)
{
   int i, j, k;
   char mediapath[256], themepath[256], iconpath[256];
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver.video_data;

   driver.gfx_use_rgba = true;

   if (!menu)
      return;

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));
   
   fill_pathname_join(font_path, themepath, "font.ttf", sizeof(font_path));

   gl_font_init_first(&font_driver, &font, gl, font_path, lakka_font_size);

   fill_pathname_join(textures[TEXTURE_BG].path, iconpath,
         "bg.png", sizeof(textures[TEXTURE_BG].path));
   fill_pathname_join(textures[TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(textures[TEXTURE_SETTINGS].path));
   fill_pathname_join(textures[TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(textures[TEXTURE_SETTING].path));
   fill_pathname_join(textures[TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(textures[TEXTURE_SUBSETTING].path));
   fill_pathname_join(textures[TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(textures[TEXTURE_ARROW].path));
   fill_pathname_join(textures[TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(textures[TEXTURE_RUN].path));
   fill_pathname_join(textures[TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(textures[TEXTURE_RESUME].path));
   fill_pathname_join(textures[TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(textures[TEXTURE_SAVESTATE].path));
   fill_pathname_join(textures[TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(textures[TEXTURE_LOADSTATE].path));
   fill_pathname_join(textures[TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(textures[TEXTURE_SCREENSHOT].path));
   fill_pathname_join(textures[TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(textures[TEXTURE_RELOAD].path));

   for (k = 0; k < TEXTURE_LAST; k++)
      textures[k].id = png_texture_load(textures[k].path);

   lakka_settings_context_reset();
   for (i = 1; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      char core_id[256], texturepath[256], content_texturepath[256],
           mediapath[256], themepath[256];
      core_info_t *info;
      core_info_list_t *info_list;

      fill_pathname_join(mediapath, g_settings.assets_directory,
            "lakka", sizeof(mediapath));
      fill_pathname_join(themepath, mediapath, THEME, sizeof(themepath));
      fill_pathname_join(iconpath, themepath, icon_dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      info_list = (core_info_list_t*)g_extern.core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      if (info != NULL && info->systemname)
      {
         char *tmp = str_replace(info->systemname, "/", " ");
         strlcpy(core_id, tmp, sizeof(core_id));
         free(tmp);
      }
      else
      {
         strlcpy(core_id, "default", sizeof(core_id));
      }

      strlcpy(texturepath, iconpath, sizeof(texturepath));
      strlcat(texturepath, core_id, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      strlcpy(content_texturepath, iconpath, sizeof(content_texturepath));
      strlcat(content_texturepath, core_id, sizeof(content_texturepath));
      strlcat(content_texturepath, "-content.png", sizeof(content_texturepath));

      category->icon = png_texture_load(texturepath);
      category->item_icon = png_texture_load(content_texturepath);
      
      for (j = 0; j < category->num_items; j++)
      {
         menu_item_t *item = (menu_item_t*)&category->items[j];

         for (k = 0; k < item->num_subitems; k++)
         {
            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

            switch (k)
            {
               case 0:
                  subitem->icon = textures[TEXTURE_RUN].id;
                  break;
               case 1:
                  subitem->icon = textures[TEXTURE_SAVESTATE].id;
                  break;
               case 2:
                  subitem->icon = textures[TEXTURE_LOADSTATE].id;
                  break;
               case 3:
                  subitem->icon = textures[TEXTURE_SCREENSHOT].id;
                  break;
               case 4:
                  subitem->icon = textures[TEXTURE_RELOAD].id;
                  break;
            }
         }
      }
   }
}

static void lakka_init_subitems(menu_item_t *item)
{
   int k;
   for (k = 0; k < item->num_subitems; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

      if (!subitem)
         continue;

      switch (k)
      {
         case 0:
            strlcpy(subitem->name, "Run", sizeof(subitem->name));
            break;
         case 1:
            strlcpy(subitem->name, "Save State", sizeof(subitem->name));
            break;
         case 2:
            strlcpy(subitem->name, "Load State", sizeof(subitem->name));
            break;
         case 3:
            strlcpy(subitem->name, "Take Screenshot", sizeof(subitem->name));
            break;
         case 4:
            strlcpy(subitem->name, "Reset", sizeof(subitem->name));
            break;
      }
      subitem->alpha = 0;
      subitem->zoom = k ? i_passive_zoom : i_active_zoom;
      subitem->y = k ? vspacing * (k+under_item_offset) :
         vspacing * active_item_factor;
   }
}

static void lakka_init_item(int i, int j, menu_category_t *category,
      core_info_t *info, struct string_list *list, const char * name)
{
   menu_item_t *item;

   int n = category->num_items;

   category->num_items++;
   category->items = (menu_item_t*)realloc(category->items,
         category->num_items * sizeof(menu_item_t));
   item = (menu_item_t*)&category->items[n];

   strlcpy(item->name, name, sizeof(item->name));
   if (list != NULL)
      strlcpy(item->rom, list->elems[j].data, sizeof(item->rom));
   item->alpha          = i != menu_active_category ? 0 : n ? 0.5 : 1;
   item->zoom           = n ? i_passive_zoom : i_active_zoom;
   item->y              = n ? vspacing*(under_item_offset+n) :
      vspacing*active_item_factor;
   item->active_subitem = 0;
   item->num_subitems   = 5;
   item->subitems       = (menu_subitem_t*)
      calloc(item->num_subitems, sizeof(menu_subitem_t));

   lakka_init_subitems(item);
}

static void lakka_init_items(int i, menu_category_t *category,
      core_info_t *info, const char* path)
{
   int num_items, j;
   struct string_list *list;

   if (category == NULL || info == NULL)
      return;

   list = (struct string_list*)dir_list_new(path, info->supported_extensions, true);

   dir_list_sort(list, true);

   num_items = list ? list->size : 0;

   for (j = 0; j < num_items; j++)
   {
      if (list->elems[j].attr.i == RARCH_DIRECTORY) // is a directory
         lakka_init_items(i, category, info, list->elems[j].data);
      else
      {
         lakka_init_item(i, j, category, info, list, 
               path_basename(list->elems[j].data));
      }
   }

   string_list_free(list);
}

static void lakka_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
}

static int lakka_input_postprocess(uint64_t old_state)
{
   if ((driver.menu && driver.menu->trigger_state
            & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      global_alpha = 0;
      rarch_main_command(RARCH_CMD_RESUME);
      return -1;
   }

   if (! global_alpha)
      add_tween(LAKKA_DELAY, 1.0, &global_alpha, &inOutQuad, NULL);

   return 0;
}

static void lakka_init_core_info(void *data)
{
   (void)data;

   core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
   if (*g_settings.libretro_directory)
   {
      g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
   }

   if (g_extern.core_info)
   {
      num_categories = g_extern.core_info->count + 1;
   }
   else
   {
     num_categories = 1;
   }
}

static void *lakka_init(void)
{
   int i;
   menu_handle_t *menu;
   gl_t *gl;

   if (driver.video != &video_gl)
   {
      RARCH_ERR("Cannot initialize Lakka menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   gl = (gl_t*)driver.video_data;

   if (!menu || !gl)
      return NULL;

   lakka_responsive();

   lakka_init_core_info(menu);
   categories = (menu_category_t*)
      calloc(num_categories, sizeof(menu_category_t));

   lakka_init_settings();

   for (i = 1; i < num_categories; i++)
   {
      core_info_t *info;
      core_info_list_t *info_list;
      menu_category_t *category = (menu_category_t*)&categories[i];

      info_list = (core_info_list_t*)g_extern.core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      if (info == NULL)
         return NULL;

      strlcpy(category->name, info->display_name, sizeof(category->name));
      strlcpy(category->libretro, info->path, sizeof(category->libretro));
      category->alpha       = 0.5;
      category->zoom        = c_passive_zoom;
      category->active_item = 0;
      category->num_items   = 0;
      category->items       = (menu_item_t*)
         calloc(category->num_items + 1, sizeof(menu_item_t));

      if (! info->supports_no_game)
         lakka_init_items(i, category, info, g_settings.content_directory);
      else
         lakka_init_item(i, 0, category, info, NULL, 
               info->display_name);
   }

   return menu;
}

menu_ctx_driver_t menu_ctx_lakka = {
   NULL,
   NULL,
   NULL,
   lakka_frame,
   lakka_init,
   lakka_free,
   lakka_context_reset,
   lakka_context_destroy,
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
