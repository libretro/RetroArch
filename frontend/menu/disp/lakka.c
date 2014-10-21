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
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../menu_driver.h"
#include "../menu_common.h"
#include "menu_display.h"
#include "../../../general.h"
#include <file/file_path.h>
#include "../../../dir_list.h"
#include "../../../gfx/gl_common.h"
#include "../../../gfx/video_thread_wrapper.h"
#include <compat/posix_string.h>

#include "../../../settings_data.h"

#include "lakka.h"
#include "../menu_animation.h"

static const GLfloat lakka_vertex[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

static const GLfloat lakka_tex_coord[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0,
};

static char *str_replace (const char *string,
      const char *substr, const char *replacement)
{
   char *tok, *newstr, *oldstr, *head;

   /* if either substr or replacement is NULL, 
    * duplicate string a let caller handle it. */
   if (!substr || !replacement)
      return strdup (string);

   newstr = strdup (string);
   head = newstr;
   while ( (tok = strstr ( head, substr )))
   {
      oldstr = newstr;
      newstr = (char*)malloc(
            strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);

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

static void lakka_draw_text(lakka_handle_t *lakka,
      const char *str, float x,
      float y, float scale_factor, float alpha)
{
   if (alpha > lakka->global_alpha)
      alpha = lakka->global_alpha;
   uint8_t a8 = 255 * alpha;

   if (!lakka)
      return;

   if (a8 == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -lakka->icon_size || x > gl->win_width + lakka->icon_size
         || y < -lakka->icon_size || y > gl->win_height + lakka->icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   struct font_params params = {0};
   params.x = x / gl->win_width;
   params.y = 1.0f - y / gl->win_height;

   params.scale = scale_factor;
   params.color = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;

   if (driver.video_data && driver.video_poke
       && driver.video_poke->set_osd_msg)
       driver.video_poke->set_osd_msg(driver.video_data,
                                      str, &params);
}

static void lakka_draw_background(bool force_transparency)
{
   float alpha = 0.75f;
   gl_t *gl = NULL;
   lakka_handle_t *lakka = NULL;

   if (!driver.menu)
      return;

   lakka = (lakka_handle_t*)driver.menu->userdata;

   if (!lakka)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, lakka->global_alpha,
      1.0f, 1.0f, 1.0f, lakka->global_alpha,
      1.0f, 1.0f, 1.0f, lakka->global_alpha,
      1.0f, 1.0f, 1.0f, lakka->global_alpha,
   };

   if (alpha > lakka->global_alpha)
      alpha = lakka->global_alpha;

   GLfloat black_color[] = {
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
   };

   gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

   struct gl_coords coords;
   coords.vertices = 4;
   coords.vertex = lakka_vertex;
   coords.tex_coord = lakka_tex_coord;
   coords.lut_tex_coord = lakka_tex_coord;

   if ((g_settings.menu.pause_libretro
      || !g_extern.main_is_init || g_extern.libretro_dummy)
      && !force_transparency
      && lakka->textures[TEXTURE_BG].id)
   {
      coords.color = color;
      glBindTexture(GL_TEXTURE_2D, lakka->textures[TEXTURE_BG].id);
   }
   else
   {
      coords.color = black_color;
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.color = gl->white_color_ptr;
}

static void lakka_draw_icon(lakka_handle_t *lakka,
      GLuint texture, float x, float y,
      float alpha, float rotation,
      float scale_factor)
{
   struct gl_coords coords;
   math_matrix mymat, mrot;
    
   if (!lakka)
      return;

   if (alpha > lakka->global_alpha)
      alpha = lakka->global_alpha;

   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -lakka->icon_size || x > gl->win_width + lakka->icon_size
         || y < -lakka->icon_size || y > gl->win_height + lakka->icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, lakka->icon_size, lakka->icon_size);

   coords.vertices = 4;
   coords.vertex = lakka_vertex;
   coords.tex_coord = lakka_tex_coord;
   coords.lut_tex_coord = lakka_tex_coord;
   coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
}

static void lakka_draw_arrow(lakka_handle_t *lakka)
{
   if (lakka)
      lakka_draw_icon(lakka, lakka->textures[TEXTURE_ARROW].id,
            lakka->margin_left + lakka->hspacing * (lakka->menu_active_category+1) +
            lakka->all_categories_x + lakka->icon_size / 2.0,
            lakka->margin_top + lakka->vspacing * lakka->active_item_factor +
            lakka->icon_size / 2.0, lakka->arrow_alpha, 0, lakka->i_active_zoom);
}

static void lakka_draw_subitems(lakka_handle_t *lakka, int i, int j)
{
   int k;
   menu_category_t *category = (menu_category_t*)&lakka->categories[i];
   menu_item_t *item = (menu_item_t*)&category->items[j];
   menu_category_t *active_category = (menu_category_t*)
      &lakka->categories[lakka->menu_active_category];
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
         lakka_draw_icon(lakka, lakka->textures[TEXTURE_RESUME].id, 
            lakka->margin_left + lakka->hspacing * (i+1) + lakka->icon_size * 2 +
            lakka->all_categories_x - lakka->icon_size / 2.0,
            lakka->margin_top + subitem->y + lakka->icon_size/2.0,
            subitem->alpha,
            0, 
            subitem->zoom);
         lakka_draw_text(lakka, "Resume",
            lakka->margin_left + lakka->hspacing * (i+2.25) +
            lakka->all_categories_x + lakka->label_margin_left,
            lakka->margin_top + subitem->y + lakka->label_margin_top,
            1, 
            subitem->alpha);
      }
      else if (k == 0 ||
            lakka->menu_active_category == 0 ||
            (g_extern.main_is_init && 
            !g_extern.libretro_dummy &&
            strcmp(g_extern.fullpath, active_item->rom) == 0))
      {
         lakka_draw_icon(lakka, subitem->icon, 
               lakka->margin_left + lakka->hspacing * (i+1) + lakka->icon_size * 2 +
               lakka->all_categories_x - lakka->icon_size/2.0, 
               lakka->margin_top + subitem->y + lakka->icon_size/2.0, 
               subitem->alpha, 
               0, 
               subitem->zoom);

         lakka_draw_text(lakka, subitem->name, 
               lakka->margin_left + lakka->hspacing * (i+2.25) +
               lakka->all_categories_x + lakka->label_margin_left, 
               lakka->margin_top + subitem->y + lakka->label_margin_top, 
               1, 
               subitem->alpha);

         if (i && (k == 1 || k == 2))
         {
            char slot[PATH_MAX];

            if (g_settings.state_slot == -1)
               snprintf(slot, sizeof(slot), "%d (auto)", g_settings.state_slot);
            else
               snprintf(slot, sizeof(slot), "%d", g_settings.state_slot);
            lakka_draw_text(lakka, slot, 
                  lakka->margin_left + lakka->hspacing * (i+2.25) +
                  lakka->all_categories_x + lakka->label_margin_left + lakka->setting_margin_left, 
                  lakka->margin_top + subitem->y + lakka->label_margin_top, 
                  1, 
                  subitem->alpha);
         }
      }

      if (subitem->setting)
      {
         char val[PATH_MAX];
         setting_data_get_string_representation(subitem->setting, val,
               sizeof(val));
         lakka_draw_text(lakka, val, 
               lakka->margin_left + lakka->hspacing * (i+2.25) +
               lakka->all_categories_x + lakka->label_margin_left + lakka->setting_margin_left, 
               lakka->margin_top + subitem->y + lakka->label_margin_top, 
               1, 
               subitem->alpha);
      }
   }
}

static void lakka_draw_items(lakka_handle_t *lakka, int i)
{
   int j;
   menu_category_t *category = (menu_category_t*)&lakka->categories[i];
   menu_category_t *active_category = (menu_category_t*)
      &lakka->categories[lakka->menu_active_category];
   menu_item_t *active_item = (menu_item_t*)
      &active_category->items[active_category->active_item];
    
   (void)active_item;

   if (!lakka)
      return;

   for(j = 0; j < category->num_items; j++)
   {
      menu_item_t *item = (menu_item_t*)&category->items[j];

      if (!item)
         continue;

      if ((i >= (lakka->menu_active_category - 1)) &&
         (i <= (lakka->menu_active_category + 1))) /* performance improvement */
      {
         lakka_draw_icon(lakka, category->item_icon,
            lakka->margin_left + lakka->hspacing * (i+1) +
            lakka->all_categories_x - lakka->icon_size / 2.0, 
            lakka->margin_top + item->y + lakka->icon_size / 2.0, 
            item->alpha, 
            0, 
            item->zoom);

         if (lakka->depth == 0)
            lakka_draw_text(lakka, item->name,
               lakka->margin_left + lakka->hspacing * (i+1) +
               lakka->all_categories_x + lakka->label_margin_left, 
               lakka->margin_top + item->y + lakka->label_margin_top, 
               1, 
               item->alpha);
      }

      /* performance improvement */
      if (i == lakka->menu_active_category && j == category->active_item)
         lakka_draw_subitems(lakka, i, j);
   }
}

static void lakka_draw_categories(lakka_handle_t *lakka)
{
   int i;

   if (!lakka)
      return;

   for(i = 0; i < lakka->num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&lakka->categories[i];

      if (!category)
         continue;

      /* draw items */
      lakka_draw_items(lakka, i);

      /* draw category icon */
      lakka_draw_icon(lakka, category->icon, 
            lakka->margin_left + (lakka->hspacing * (i+1)) +
            lakka->all_categories_x - lakka->icon_size/2.0,
            lakka->margin_top + lakka->icon_size/2.0, 
            category->alpha, 
            0, 
            category->zoom);
   }
}

#if defined(HAVE_FBO) && defined(LAKKA_EFFECTS)

static void lakka_check_fb_status(void)
{
    GLenum status;
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status) {
    case GL_FRAMEBUFFER_COMPLETE:
        break;

    case GL_FRAMEBUFFER_UNSUPPORTED:
        break;

    default:
        fputs("Framebuffer Error\n", stderr);
        exit(-1);
    }
}

static void lakka_fbo_reset(lakka_handle_t *lakka)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   if (!gl)
      return;

   glGenFramebuffers(1, &lakka->fbo);
   glGenTextures(1, &lakka->fbo_color);
   glGenRenderbuffers(1, &lakka->fbo_depth);

   glBindFramebuffer(GL_FRAMEBUFFER, lakka->fbo);

   glBindTexture(GL_TEXTURE_2D, lakka->fbo_color);
   glTexImage2D(GL_TEXTURE_2D, 
         0, 
         GL_RGBA, 
         gl->win_width, gl->win_height,
         0, 
         GL_RGBA, 
         GL_UNSIGNED_BYTE, 
         NULL);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lakka->fbo_color, 0);

   glBindRenderbuffer(GL_RENDERBUFFER, lakka->fbo_depth);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, gl->win_width, gl->win_height);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, lakka->fbo_depth);

   lakka_check_fb_status();

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void lakka_draw_fbo(lakka_handle_t *lakka)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   if (!gl)
      return;

   struct gl_coords coords;
   coords.vertices = 4;
   coords.vertex = lakka_vertex;
   coords.tex_coord = lakka_vertex;
   coords.color = gl->white_color_ptr;
   glBindTexture(GL_TEXTURE_2D, lakka->fbo_color);

   math_matrix mymat;

   math_matrix mrot;
   matrix_rotate_z(&mrot, 0);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, lakka->global_scale, lakka->global_scale, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);

   // shadow
   glViewport(2, -2, gl->win_width, gl->win_height);
   glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glViewport(0, 0, gl->win_width, gl->win_height);

   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);
}

#endif

static void lakka_frame(void)
{
   menu_item_t *active_item = NULL;
   menu_category_t *active_category = NULL;
   lakka_handle_t *lakka = NULL;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (!driver.menu)
      return;

   lakka = (lakka_handle_t*)driver.menu->userdata;

   if (!lakka)
      return;

   active_category = (menu_category_t*)
      &lakka->categories[lakka->menu_active_category];

   if (!active_category)
      return;

   active_item = (menu_item_t*)
      &active_category->items[active_category->active_item];

   update_tweens(0.002);

#if defined(HAVE_FBO) && defined(LAKKA_EFFECTS)
   glBindFramebuffer(GL_FRAMEBUFFER, lakka->fbo);
   glClearColor(0.0, 0.0, 0.0, 0.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   lakka_draw_categories(lakka);
   lakka_draw_arrow(lakka);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glViewport(0, 0, gl->win_width, gl->win_height);
   lakka_draw_background(false);
   lakka_draw_fbo(lakka);
#else
   glViewport(0, 0, gl->win_width, gl->win_height);
   lakka_draw_background(false);
   lakka_draw_categories(lakka);
   lakka_draw_arrow(lakka);
#endif

   if (lakka->depth == 0)
      lakka_draw_text(lakka, active_category->name,
            lakka->title_margin_left, lakka->title_margin_top, 1, 1.0);
   else if (active_item)
      lakka_draw_text(lakka, active_item->name,
            lakka->title_margin_left, lakka->title_margin_top, 1, 1.0);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   glDisable(GL_BLEND);
}

static GLuint lakka_png_texture_load_(const char * file_name)
{
   if (! path_file_exists(file_name))
      return 0;

   struct texture_image ti = {0};
   texture_image_load(&ti, file_name);

   /* Generate the OpenGL texture object */
   GLuint texture = 0;
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ti.width, ti.height, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, ti.pixels);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glGenerateMipmap(GL_TEXTURE_2D);

   free(ti.pixels);

   return texture;
}
static int lakka_png_texture_load_wrap(void *data)
{
   const char *filename = (const char*)data;
   return lakka_png_texture_load_(filename);
}

static GLuint lakka_png_texture_load(const char* file_name)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;
      thr->cmd_data.custom_command.method = lakka_png_texture_load_wrap;
      thr->cmd_data.custom_command.data   = (void*)file_name;
      thr->send_cmd_func(thr, CMD_CUSTOM_COMMAND);
      thr->wait_reply_func(thr, CMD_CUSTOM_COMMAND);

      return thr->cmd_data.custom_command.return_value;

   }

   return lakka_png_texture_load_(file_name);
}

static void lakka_context_destroy(void *data)
{
   int i, j, k;
   menu_handle_t *menu = NULL;
   lakka_handle_t *lakka = NULL;

   menu = (menu_handle_t*)data;

   if (!menu)
      return;

   lakka = (lakka_handle_t*)menu->userdata;

   if (!lakka)
      return;

#if defined(HAVE_FBO) && defined(LAKKA_EFFECTS)
   glDeleteFramebuffers(1, &lakka->fbo);
   glDeleteTextures(1, &lakka->fbo_color);
   glDeleteTextures(1, &lakka->fbo_depth);
#endif

   for (i = 0; i < TEXTURE_LAST; i++)
      glDeleteTextures(1, &lakka->textures[i].id);

   for (i = 1; i < lakka->num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&lakka->categories[i];

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
}

static bool lakka_init_settings(menu_handle_t *menu)
{
   int k, jj = 0, kk;
   lakka_handle_t *lakka = NULL;
   menu_category_t *category = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t*)menu->list_settings;

   lakka = (lakka_handle_t*)menu->userdata;

   if (!lakka)
      return false;

   category = (menu_category_t*)&lakka->categories[0];

   if (!setting_data || !category)
      return false;

   strlcpy(category->name, "Settings", sizeof(category->name));
   category->alpha       = lakka->c_active_alpha;
   category->zoom        = lakka->c_active_zoom;
   category->active_item = 0;
   category->num_items   = 0;
   category->items       = (menu_item_t*)
      calloc(category->num_items, sizeof(menu_item_t));

   rarch_setting_t *group = (rarch_setting_t*)setting_data_find_setting(driver.menu->list_settings,
         "Driver Options");

   for (; group->type != ST_NONE; group++)
   {
      if (group->type != ST_GROUP)
         continue;

      category->num_items++;
      category->items = (menu_item_t*)
         realloc(category->items, category->num_items * sizeof(menu_item_t));

      if (!category->items)
         return false;

      menu_item_t *item  = (menu_item_t*)&category->items[jj];

      if (!item)
         return false;

      strlcpy(item->name, group->name, sizeof(item->name));
      item->alpha = jj ? lakka->i_passive_alpha : lakka->i_active_alpha;
      item->zoom  = jj ? lakka->i_passive_zoom  : lakka->i_active_zoom;
      item->y = jj ?
         lakka->vspacing * (lakka->under_item_offset + jj) 
         : lakka->vspacing * lakka->active_item_factor;
      item->active_subitem = 0;
      item->num_subitems = 0;
      item->subitems = NULL;

      kk = 0;
      for (k = 0; k <= 512; k++)
      {
         rarch_setting_t *setting = (rarch_setting_t*)&setting_data[k];

         if (setting
               && setting->type != ST_SUB_GROUP 
               && setting->group == group->name)
         {
            item->num_subitems++;

            item->subitems = (menu_subitem_t*)
               realloc(item->subitems, 
                     item->num_subitems * sizeof(menu_subitem_t));

            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[kk];

            strlcpy(subitem->name, setting->short_description, 
                  sizeof(subitem->name));
            subitem->alpha = 0.0;
            subitem->zoom = kk ? lakka->i_passive_zoom : lakka->i_active_zoom;
            subitem->y = kk ? lakka->vspacing * (kk + lakka->under_item_offset)
               : lakka->vspacing * lakka->active_item_factor;

            subitem->setting = (rarch_setting_t*)&setting_data[k];

            kk++;
         }
      }
      jj++;
   }

   category->num_items++;
   category->items = (menu_item_t*)
      realloc(category->items, category->num_items * sizeof(menu_item_t));

   menu_item_t *itemq = (menu_item_t*)&category->items[jj];

   if (!itemq)
      return false;

   strlcpy(itemq->name, "Quit RetroArch", sizeof(itemq->name));
   itemq->alpha          = jj ? lakka->i_passive_alpha : lakka->i_active_alpha;
   itemq->zoom           = jj ? lakka->i_passive_zoom  : lakka->i_active_zoom;
   itemq->y              = jj ? lakka->vspacing * (lakka->under_item_offset + jj) :
      lakka->vspacing * lakka->active_item_factor;
   itemq->active_subitem = 0;
   itemq->num_subitems   = 0;

   return true;
}

static void lakka_settings_context_reset(void)
{
   int j, k;
   lakka_handle_t *lakka = NULL;
   menu_item_t *item = NULL;
   menu_category_t *category = NULL;

   if (!driver.menu)
      return;

   lakka = (lakka_handle_t*)driver.menu->userdata;

   if (!lakka)
      return;

   category = (menu_category_t*)&lakka->categories[0];

   if (!category)
      return;

   category->icon      = lakka->textures[TEXTURE_SETTINGS].id;
   category->item_icon = lakka->textures[TEXTURE_SETTING].id;

   for  (j = 0; j < category->num_items; j++)
   {
      item = (menu_item_t*)&category->items[j];

      for (k = 0; k < item->num_subitems; k++)
      {
         menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];
         subitem->icon = lakka->textures[TEXTURE_SUBSETTING].id;
      }
   }
}

static void lakka_context_reset(void *data)
{
   int i, j, k;
   char mediapath[PATH_MAX], themepath[PATH_MAX], iconpath[PATH_MAX];
   lakka_handle_t *lakka = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   lakka = (lakka_handle_t*)menu->userdata;

   if (!lakka)
      return;

#if defined(HAVE_FBO) && defined(LAKKA_EFFECTS)
   lakka_fbo_reset(lakka);
#endif

   driver.gfx_use_rgba = true;

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, lakka->icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));
   
   fill_pathname_join(lakka->textures[TEXTURE_BG].path, iconpath,
         "bg.png", sizeof(lakka->textures[TEXTURE_BG].path));
   fill_pathname_join(lakka->textures[TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(lakka->textures[TEXTURE_SETTINGS].path));
   fill_pathname_join(lakka->textures[TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(lakka->textures[TEXTURE_SETTING].path));
   fill_pathname_join(lakka->textures[TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(lakka->textures[TEXTURE_SUBSETTING].path));
   fill_pathname_join(lakka->textures[TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(lakka->textures[TEXTURE_ARROW].path));
   fill_pathname_join(lakka->textures[TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(lakka->textures[TEXTURE_RUN].path));
   fill_pathname_join(lakka->textures[TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(lakka->textures[TEXTURE_RESUME].path));
   fill_pathname_join(lakka->textures[TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(lakka->textures[TEXTURE_SAVESTATE].path));
   fill_pathname_join(lakka->textures[TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(lakka->textures[TEXTURE_LOADSTATE].path));
   fill_pathname_join(lakka->textures[TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(lakka->textures[TEXTURE_SCREENSHOT].path));
   fill_pathname_join(lakka->textures[TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(lakka->textures[TEXTURE_RELOAD].path));

   for (k = 0; k < TEXTURE_LAST; k++)
      lakka->textures[k].id = lakka_png_texture_load(lakka->textures[k].path);

   lakka_settings_context_reset();

   for (i = 1; i < lakka->num_categories; i++)
   {
      char core_id[PATH_MAX], texturepath[PATH_MAX], content_texturepath[PATH_MAX];
      menu_category_t *category = (menu_category_t*)&lakka->categories[i];
      core_info_t *info = NULL;
      core_info_list_t *info_list = NULL;

      fill_pathname_join(mediapath, g_settings.assets_directory,
            "lakka", sizeof(mediapath));
      fill_pathname_join(themepath, mediapath, THEME, sizeof(themepath));
      fill_pathname_join(iconpath, themepath, lakka->icon_dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      info_list = (core_info_list_t*)g_extern.core_info;
      info = NULL;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      if (info && info->systemname)
      {
         char *tmp = str_replace(info->systemname, "/", " ");
         strlcpy(core_id, tmp, sizeof(core_id));
         free(tmp);
      }
      else
         strlcpy(core_id, "default", sizeof(core_id));

      strlcpy(texturepath, iconpath, sizeof(texturepath));
      strlcat(texturepath, core_id, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      strlcpy(content_texturepath, iconpath, sizeof(content_texturepath));
      strlcat(content_texturepath, core_id, sizeof(content_texturepath));
      strlcat(content_texturepath, "-content.png", sizeof(content_texturepath));

      category->icon = lakka_png_texture_load(texturepath);
      category->item_icon = lakka_png_texture_load(content_texturepath);
      
      for (j = 0; j < category->num_items; j++)
      {
         menu_item_t *item = (menu_item_t*)&category->items[j];

         for (k = 0; k < item->num_subitems; k++)
         {
            menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

            switch (k)
            {
               case 0:
                  subitem->icon = lakka->textures[TEXTURE_RUN].id;
                  break;
               case 1:
                  subitem->icon = lakka->textures[TEXTURE_SAVESTATE].id;
                  break;
               case 2:
                  subitem->icon = lakka->textures[TEXTURE_LOADSTATE].id;
                  break;
               case 3:
                  subitem->icon = lakka->textures[TEXTURE_SCREENSHOT].id;
                  break;
               case 4:
                  subitem->icon = lakka->textures[TEXTURE_RELOAD].id;
                  break;
            }
         }
      }
   }
}

static void lakka_init_subitems(lakka_handle_t *lakka, menu_item_t *item)
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
      subitem->zoom = k ? lakka->i_passive_zoom : lakka->i_active_zoom;
      subitem->y = k ? lakka->vspacing * (k + lakka->under_item_offset) :
         lakka->vspacing * lakka->active_item_factor;
   }
}

static void lakka_init_item(lakka_handle_t *lakka,
      int i, int j, menu_category_t *category,
      core_info_t *info, struct string_list *list, const char * name)
{
   menu_item_t *item = NULL;
   int n = category->num_items;

   category->num_items++;
   category->items = (menu_item_t*)realloc(category->items,
         category->num_items * sizeof(menu_item_t));
   item = (menu_item_t*)&category->items[n];

   strlcpy(item->name, name, sizeof(item->name));
   if (list != NULL)
      strlcpy(item->rom, list->elems[j].data, sizeof(item->rom));
   item->alpha          = (i != lakka->menu_active_category) ? 0 :
                          n ? lakka->i_passive_alpha : lakka->i_active_alpha;
   item->zoom           = n ? lakka->i_passive_zoom  : lakka->i_active_zoom;
   item->y              = n ? (lakka->vspacing * (lakka->under_item_offset + n)) :
      (lakka->vspacing * lakka->active_item_factor);
   item->active_subitem = 0;
   item->num_subitems   = 5;
   item->subitems       = (menu_subitem_t*)
      calloc(item->num_subitems, sizeof(menu_subitem_t));

   lakka_init_subitems(lakka, item);
}

static void lakka_init_items(lakka_handle_t *lakka,
      int i, menu_category_t *category,
      core_info_t *info, const char* path)
{
   int num_items, j;
   struct string_list *list = NULL;

   if (category == NULL || info == NULL)
      return;

   list = (struct string_list*)dir_list_new(path, info->supported_extensions, true);

   dir_list_sort(list, true);

   num_items = list ? list->size : 0;

   for (j = 0; j < num_items; j++)
   {
      if (list->elems[j].attr.i == RARCH_DIRECTORY) // is a directory
         lakka_init_items(lakka, i, category, info, list->elems[j].data);
      else
      {
         lakka_init_item(lakka, i, j, category, info, list, 
               path_basename(list->elems[j].data));
      }
   }

   string_list_free(list);
}

static void lakka_free_userdata(void *data)
{
   lakka_handle_t *lakka = (lakka_handle_t*)data;

   if (!lakka)
      return;

   if (lakka->categories)
      free(lakka->categories);

   free(lakka);
}

static void lakka_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (menu->userdata)
      lakka_free_userdata(menu->userdata);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
}

static int lakka_input_postprocess(retro_input_t state,
      retro_input_t old_state)
{
   lakka_handle_t *lakka = NULL;
   if (!driver.menu)
      return 0;

   lakka = (lakka_handle_t*)driver.menu->userdata;

   if (!lakka)
      return 0;

   if (lakka->global_alpha == 0.0f)
      add_tween(LAKKA_DELAY, 1.0f, &lakka->global_alpha, &inOutQuad, NULL);

   if (lakka->global_scale == 2.0f)
      add_tween(LAKKA_DELAY, 1.0f, &lakka->global_scale, &inQuad, NULL);

   return 0;
}

static void lakka_init_core_info(void *data)
{
   (void)data;

   core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
   if (*g_settings.libretro_directory)
      g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
}

static void *lakka_init(void)
{
   menu_handle_t *menu = NULL;
   lakka_handle_t *lakka = NULL;
   const video_driver_t *video_driver = NULL;
   gl_t *gl = (gl_t*)driver_video_resolve(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize Lakka menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   if (!menu)
      return NULL;

   menu->userdata = (lakka_handle_t*)calloc(1, sizeof(lakka_handle_t));
   if (!menu->userdata)
   {
      free(menu);
      return NULL;
   }

   lakka_init_core_info(menu);

   lakka = (lakka_handle_t*)menu->userdata;
   lakka->num_categories       = g_extern.core_info ? (g_extern.core_info->count + 1) : 1;
   lakka->categories = (menu_category_t*)
      calloc(lakka->num_categories, sizeof(menu_category_t));

   if (!lakka->categories)
   {
      free(menu->userdata);
      free(menu);
      
      return NULL;
   }

   lakka->c_active_zoom   = 1.0;
   lakka->c_passive_zoom  = 0.5;
   lakka->i_active_zoom   = 1.0;
   lakka->i_passive_zoom  = 0.5;

   lakka->c_active_alpha  = 1.0;
   lakka->c_passive_alpha = 0.5;
   lakka->i_active_alpha  = 1.0;
   lakka->i_passive_alpha = 0.5;

   lakka->above_subitem_offset = 1.5;
   lakka->above_item_offset    = -1.0;
   lakka->active_item_factor   = 3.0;
   lakka->under_item_offset    = 5.0;

   float scale_factor = 1;
   if      (gl->win_width >= 3840) scale_factor = 2.0;
   else if (gl->win_width >= 2560) scale_factor = 1.5;
   else if (gl->win_width >= 1920) scale_factor = 1.0;
   else if (gl->win_width >= 1280) scale_factor = 0.75;
   else if (gl->win_width >=  640) scale_factor = 0.5;
   else if (gl->win_width >=  320) scale_factor = 0.25;

   strlcpy(lakka->icon_dir, "256", sizeof(lakka->icon_dir));

   lakka->icon_size = 128.0 * scale_factor;
   lakka->hspacing = 200.0 * scale_factor;
   lakka->vspacing = 64.0 * scale_factor;
   lakka->margin_left = 336.0 * scale_factor;
   lakka->margin_top = (256+32) * scale_factor;
   lakka->title_margin_left = 60 * scale_factor;
   lakka->title_margin_top = 60 * scale_factor + g_settings.video.font_size/3;
   lakka->label_margin_left = 85.0 * scale_factor;
   lakka->label_margin_top = g_settings.video.font_size/3.0;
   lakka->setting_margin_left = 600.0 * scale_factor;

   lakka->depth                = 0;
   lakka->menu_active_category = 0;
   lakka->all_categories_x     = 0;
   lakka->global_alpha         = 0.0f;
   lakka->global_scale         = 2.0f;
   lakka->arrow_alpha          = 0;

   lakka->fbo_depth            = 0;

   return menu;
}

static bool lakka_init_lists(void *data)
{
   int i;
   menu_handle_t *menu = (menu_handle_t*)data;
   lakka_handle_t *lakka = NULL;

   if (!menu)
      return false;

   lakka = (lakka_handle_t*)menu->userdata;

   if (!lakka)
      return false;

   if (!lakka_init_settings(menu))
      return false;

   for (i = 1; i < lakka->num_categories; i++)
   {
      core_info_t *info = NULL;
      menu_category_t *category = (menu_category_t*)&lakka->categories[i];
      core_info_list_t *info_list = (core_info_list_t*)g_extern.core_info;

      if (info_list)
         info = (core_info_t*)&info_list->list[i-1];

      if (!info)
         return false;

      strlcpy(category->name, info->display_name, sizeof(category->name));
      strlcpy(category->libretro, info->path, sizeof(category->libretro));
      category->alpha       = lakka->i_passive_alpha;
      category->zoom        = lakka->c_passive_zoom;
      category->active_item = 0;
      category->num_items   = 0;
      category->items       = (menu_item_t*)
         calloc(category->num_items + 1, sizeof(menu_item_t));

      if (! info->supports_no_game)
         lakka_init_items(lakka, i, category, info, g_settings.content_directory);
      else
         lakka_init_item(lakka, i, 0, category, info, NULL, 
               info->display_name);
   }

   return true;
}

menu_ctx_driver_t menu_ctx_lakka = {
   NULL,
   NULL,
   NULL,
   lakka_frame,
   lakka_init,
   lakka_init_lists,
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
