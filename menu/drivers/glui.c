/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../menu.h"
#include "../menu_texture.h"
#include "../menu_input.h"

#include <file/file_path.h>
#include "../../gfx/gl_common.h"
#include <compat/posix_string.h>

#include "shared.h"

typedef struct glui_handle
{
   unsigned line_height;
   unsigned glyph_width;
   unsigned margin;
   unsigned term_width;
   unsigned term_height;
   char box_message[PATH_MAX_LENGTH];
   GLuint bg;
} glui_handle_t;

static int glui_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, NULL);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);
   
   return -1;
}

static void glui_blit_line(gl_t *gl, float x, float y, const char *message, bool green)
{
   struct font_params params = {0};

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   params.x           = x / gl->win_width;
   params.y           = 1.0f - y / gl->win_height;
   params.scale       = 1.0;
   params.color       = FONT_COLOR_RGBA(255, 255, 255, 255);
   params.full_screen = true;

   if (green)
      params.color = FONT_COLOR_RGBA(100, 255, 100, 255);
    
   if (!driver.video_data)
      return;
   if (!driver.video_poke)
      return;
   if (!driver.video_poke->set_osd_msg)
      return;

   driver.video_poke->set_osd_msg(driver.video_data,
         message, &params, NULL);
}

static void glui_render_background(gl_t *gl, glui_handle_t *glui,
      bool force_transparency)
{
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
   struct gl_coords coords;
   float alpha = 0.75f;
   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   GLfloat black_color[] = {
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
   };

   glViewport(0, 0, gl->win_width, gl->win_height);

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;

   if ((g_settings.menu.pause_libretro
      || !g_extern.main_is_init || g_extern.libretro_dummy)
      && !force_transparency
      && glui->bg)
   {
      coords.color = color;
      glBindTexture(GL_TEXTURE_2D, glui->bg);
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

static void glui_draw_cursor(gl_t *gl, glui_handle_t *glui, float x, float y)
{
   struct gl_coords coords;
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
   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   glViewport(x - 5, gl->win_height - y, 11, 11);

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;

   coords.color = color;
   glBindTexture(GL_TEXTURE_2D, 0);

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.color = gl->white_color_ptr;
}

static void glui_get_message(const char *message)
{
   glui_handle_t *glui = NULL;

   if (!driver.menu || !message || !*message)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (glui)
      strlcpy(glui->box_message, message, sizeof(glui->box_message));
}

static void glui_render_messagebox(const char *message)
{
   unsigned i;
   int x, y;
   struct string_list *list = NULL;
   glui_handle_t *glui = NULL;
   gl_t *gl = (gl_t*)video_driver_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (!glui)
      return;

   list = (struct string_list*)string_split(message, "\n");

   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   x = gl->win_width  / 2 - strlen(list->elems[0].data) * glui->glyph_width / 2;
   y = gl->win_height / 2 - list->size * glui->line_height / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         glui_blit_line(gl, x, y + i * glui->line_height, msg, false);
   }

end:
   string_list_free(list);
}

static void glui_frame(void)
{
   unsigned x, y;
   size_t i;
   char title[PATH_MAX_LENGTH], title_buf[PATH_MAX_LENGTH], 
        title_msg[PATH_MAX_LENGTH];
   char timedate[PATH_MAX_LENGTH];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   size_t end;
   gl_t *gl = (gl_t*)video_driver_resolve(NULL);
   glui_handle_t *glui = NULL;
   const char *core_name = NULL;
   const char *core_version = NULL;

   if (!driver.menu || !gl)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (!glui)
      return;

   if (driver.menu->need_refresh
         && g_extern.is_menu
         && !driver.menu->msg_force)
      return;

   glui->line_height = g_settings.video.font_size * 4 / 3;
   glui->glyph_width = glui->line_height / 2;
   glui->margin      = gl->win_width / 20 ;
   glui->term_width  = (gl->win_width - glui->margin * 2) / glui->glyph_width;
   glui->term_height = (gl->win_height - glui->margin * 2) / glui->line_height - 2;

   driver.menu->mouse.ptr = (driver.menu->mouse.y - glui->margin) /
         glui->line_height - 2 + driver.menu->begin;

   glViewport(0, 0, gl->win_width, gl->win_height);

   if (driver.menu->mouse.wheeldown && driver.menu->begin
         < menu_list_get_size(driver.menu->menu_list) - glui->term_height)
      driver.menu->begin++;

   if (driver.menu->mouse.wheelup && driver.menu->begin > 0)
      driver.menu->begin--;

   /* Do not scroll if all items are visible. */
   if (menu_list_get_size(driver.menu->menu_list) <= glui->term_height)
      driver.menu->begin = 0;

   end = (driver.menu->begin + glui->term_height <=
         menu_list_get_size(driver.menu->menu_list)) ?
      driver.menu->begin + glui->term_height :
      menu_list_get_size(driver.menu->menu_list);

   glui_render_background(gl, glui, false);

   menu_list_get_last_stack(driver.menu->menu_list, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, title, sizeof(title));

   menu_animation_ticker_line(title_buf, glui->term_width - 3,
         g_extern.frame_count / glui->margin, title, true);
   glui_blit_line(gl, glui->margin * 2, glui->margin + glui->line_height,
         title_buf, true);

   core_name = g_extern.menu.info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   core_version = g_extern.menu.info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);

   disp_timedate_set_label(timedate, sizeof(timedate), 0);

   glui_blit_line(gl,
         glui->margin * 2,
         glui->margin + glui->term_height * glui->line_height 
         + glui->line_height * 2, title_msg, true);

   if (g_settings.menu.timedate_enable)
      glui_blit_line(gl,
            glui->margin * 14,
            glui->margin + glui->term_height * glui->line_height 
            + glui->line_height * 2, timedate, true);


   x = glui->margin;
   y = glui->margin + glui->line_height * 2;

   for (i = driver.menu->begin; i < end; i++, y += glui->line_height)
   {
      char message[PATH_MAX_LENGTH], type_str[PATH_MAX_LENGTH],
           entry_title_buf[PATH_MAX_LENGTH], type_str_buf[PATH_MAX_LENGTH],
           path_buf[PATH_MAX_LENGTH];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      bool selected = false;
      menu_file_list_cbs_t *cbs = NULL;

      menu_list_get_at_offset(driver.menu->menu_list->selection_buf, i, &path,
            &entry_label, &type);

      cbs = (menu_file_list_cbs_t*)
         menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
               i);

      if (cbs && cbs->action_get_representation)
         cbs->action_get_representation(driver.menu->menu_list->selection_buf,
               &w, type, i, label,
               type_str, sizeof(type_str), 
               entry_label, path,
               path_buf, sizeof(path_buf));

      selected = (i == driver.menu->selection_ptr);

      menu_animation_ticker_line(entry_title_buf, glui->term_width - (w + 1 + 2),
            g_extern.frame_count / glui->margin, path_buf, selected);
      menu_animation_ticker_line(type_str_buf, w, 
            g_extern.frame_count / glui->margin, type_str, selected);

      strlcpy(message, entry_title_buf, sizeof(message));

      glui_blit_line(gl, x, y, message, selected);

      glui_blit_line(gl, gl->win_width - glui->glyph_width * w - glui->margin , 
         y, type_str_buf, selected);
   }

#ifdef GEKKO
   const char *message_queue;

   if (driver.menu->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      driver.menu->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   glui_render_messagebox(message_queue);
#endif

   if (driver.menu->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      glui_render_background(gl, glui, true);
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      glui_render_messagebox(msg);
   }

   if (glui->box_message[0] != '\0')
   {
      glui_render_background(gl, glui, true);
      glui_render_messagebox(glui->box_message);
      glui->box_message[0] = '\0';
   }

   if (driver.menu->mouse.enable)
      glui_draw_cursor(gl, glui, driver.menu->mouse.x, driver.menu->mouse.y);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void *glui_init(void)
{
   menu_handle_t *menu;
   glui_handle_t *glui = NULL;
   const video_driver_t *video_driver = NULL;
   gl_t *gl = (gl_t*)video_driver_resolve(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize GLUI menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   menu->userdata = (glui_handle_t*)calloc(1, sizeof(glui_handle_t));

   if (!menu->userdata)
      goto error;

   glui     = (glui_handle_t*)menu->userdata;
   glui->bg = 0;

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void glui_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (menu->userdata)
      free(menu->userdata);
}


static void glui_context_reset(void *data)
{
   char bgpath[PATH_MAX_LENGTH];
   glui_handle_t *glui = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)video_driver_resolve(NULL);
    
   (void)gl;

   if (!menu)
      return;

   glui = (glui_handle_t*)menu->userdata;

   if (!glui)
      return;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "glui", sizeof(bgpath));

   if (*g_settings.menu.wallpaper)
      strlcpy(bgpath, g_settings.menu.wallpaper, sizeof(bgpath));
   else
      fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   if (path_file_exists(bgpath))
      glui->bg = (GLuint)menu_texture_load(bgpath,
            TEXTURE_BACKEND_OPENGL,
            TEXTURE_FILTER_LINEAR);
}

static void glui_navigation_clear(void *data, bool pending_push)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (menu)
      menu->begin = 0;
}

static void glui_navigation_set(void *data, bool scroll)
{
   glui_handle_t *glui = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;
   
   glui = (glui_handle_t*)menu->userdata;

   if (!glui)
      return;
   if (!scroll)
      return;

   if (menu->selection_ptr < glui->term_height/2)
      menu->begin = 0;
   else if (menu->selection_ptr >= glui->term_height/2
         && menu->selection_ptr <
         menu_list_get_size(menu->menu_list) - glui->term_height/2)
      menu->begin = menu->selection_ptr - glui->term_height/2;
   else if (menu->selection_ptr >=
         menu_list_get_size(menu->menu_list) - glui->term_height/2)
      menu->begin = menu_list_get_size(menu->menu_list)
            - glui->term_height;
}

static void glui_navigation_set_last(void *data)
{
   glui_navigation_set(data, true);
}

static void glui_navigation_descend_alphabet(void *data, size_t *unused)
{
   glui_navigation_set(data, true);
}

static void glui_navigation_ascend_alphabet(void *data, size_t *unused)
{
   glui_navigation_set(data, true);
}

menu_ctx_driver_t menu_ctx_glui = {
   NULL,
   glui_get_message,
   NULL,
   glui_frame,
   glui_init,
   glui_free,
   glui_context_reset,
   NULL,
   NULL,
   NULL,
   glui_navigation_clear,
   NULL,
   NULL,
   glui_navigation_set,
   glui_navigation_set_last,
   glui_navigation_descend_alphabet,
   glui_navigation_ascend_alphabet,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   glui_entry_iterate,
   "glui",
};
