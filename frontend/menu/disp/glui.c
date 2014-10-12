/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../menu_common.h"
#include "../menu_driver.h"
#include "menu_display.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../gfx/gl_common.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../config.def.h"
#include "../../../file.h"
#include "../../../dynamic.h"
#include "../../../compat/posix_string.h"
#include "../../../performance.h"
#include "../../../input/input_common.h"

#include "../../../settings_data.h"
#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "shared.h"

typedef struct glui_handle
{
   unsigned line_height;
   unsigned glyph_width;
   unsigned margin;
   unsigned term_width;
   unsigned term_height;
   char box_message[PATH_MAX];
   GLuint bg;
} glui_handle_t;

static void glui_blit_line(float x, float y, const char *message, bool green)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   struct font_params params = {0};
   params.x = x / gl->win_width;
   params.y = 1.0f - y / gl->win_height;

   params.scale = 1.0;
   params.color = green ? FONT_COLOR_RGBA(100, 255, 100, 255)
      : FONT_COLOR_RGBA(255, 255, 255, 255);
   params.full_screen = true;
    
   if (driver.video_data && driver.video_poke
       && driver.video_poke->set_osd_msg)
       driver.video_poke->set_osd_msg(driver.video_data,
                                      message, &params);
}

static void glui_render_background(bool force_transparency)
{
   float alpha = 0.75f;
   gl_t *gl = NULL;
   glui_handle_t *glui = NULL;

   if (!driver.menu)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (!glui)
      return;

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

   gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

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
   coords.vertices = 4;
   coords.vertex = vertex;
   coords.tex_coord = tex_coord;
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

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.color = gl->white_color_ptr;
}

static void glui_get_message(const char *message)
{
   size_t i;
   glui_handle_t *glui = NULL;
   (void)i;

   if (!driver.menu || !message || !*message)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (!glui)
      return;

   strlcpy(glui->box_message, message, sizeof(glui->box_message));
}

static void glui_render_messagebox(const char *message)
{
   unsigned i;
   int x, y;
   struct string_list *list = NULL;
   glui_handle_t *glui = NULL;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   glui = (glui_handle_t*)driver.menu->userdata;

   if (!glui)
      return;

   list = (struct string_list*)string_split(message, "\n");

   if (!list)
      return;

   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   x = gl->win_width / 2 - strlen(list->elems[0].data) * glui->glyph_width / 2;
   y = gl->win_height / 2 - list->size * glui->line_height / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      glui_blit_line(x, y + i * glui->line_height, msg, false);
   }

   string_list_free(list);
}

static void glui_frame(void)
{
   unsigned x, y;
   size_t i;
   char title[PATH_MAX], title_buf[PATH_MAX], 
        title_msg[PATH_MAX];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   size_t begin = 0, end;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   glui_handle_t *glui = NULL;

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
   glui->margin = gl->win_width / 20 ;
   glui->term_width = (gl->win_width - glui->margin * 2) / glui->glyph_width;
   glui->term_height = (gl->win_height - glui->margin * 2) / glui->line_height - 2;

   glViewport(0, 0, gl->win_width, gl->win_height);


   if (driver.menu->selection_ptr >= glui->term_height / 2)
      begin = driver.menu->selection_ptr - glui->term_height / 2;
   end   = (driver.menu->selection_ptr + glui->term_height <=
         file_list_get_size(driver.menu->selection_buf)) ?
      driver.menu->selection_ptr + glui->term_height :
      file_list_get_size(driver.menu->selection_buf);

   /* Do not scroll if all items are visible. */
   if (file_list_get_size(driver.menu->selection_buf) <= glui->term_height)
      begin = 0;

   if (end - begin > glui->term_height)
      end = begin + glui->term_height;

   glui_render_background(false);

   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, title, sizeof(title));

   menu_ticker_line(title_buf, glui->term_width - 3,
         g_extern.frame_count / glui->margin, title, true);
   glui_blit_line(glui->margin * 2, glui->margin + glui->line_height,
         title_buf, true);

   const char *core_name = g_extern.menu.info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = g_extern.menu.info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
   glui_blit_line(
         glui->margin * 2,
         glui->margin + glui->term_height * glui->line_height 
         + glui->line_height * 2, title_msg, true);


   x = glui->margin;
   y = glui->margin + glui->line_height * 2;

   for (i = begin; i < end; i++, y += glui->line_height)
   {
      char message[PATH_MAX], type_str[PATH_MAX],
           entry_title_buf[PATH_MAX], type_str_buf[PATH_MAX],
           path_buf[PATH_MAX];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      bool selected = false;

      file_list_get_at_offset(driver.menu->selection_buf, i, &path,
            &entry_label, &type);
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
            driver.menu->list_settings,
            driver.menu->selection_buf->list[i].label);
      (void)setting;

      disp_set_label(&w, type, i, label,
            type_str, sizeof(type_str), 
            entry_label, path,
            path_buf, sizeof(path_buf));

      selected = (i == driver.menu->selection_ptr);

      menu_ticker_line(entry_title_buf, glui->term_width - (w + 1 + 2),
            g_extern.frame_count / glui->margin, path_buf, selected);
      menu_ticker_line(type_str_buf, w, 
            g_extern.frame_count / glui->margin, type_str, selected);

      strlcpy(message, entry_title_buf, sizeof(message));

      glui_blit_line(x, y, message, selected);

      glui_blit_line(gl->win_width - glui->glyph_width * w - glui->margin , 
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
      char msg[PATH_MAX];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      glui_render_background(true);
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      glui_render_messagebox(msg);
   }

   if (glui->box_message[0] != '\0')
   {
      glui_render_background(true);
      glui_render_messagebox(glui->box_message);
      glui->box_message[0] = '\0';
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void glui_init_core_info(void *data)
{
   (void)data;

   core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
   if (*g_settings.libretro_directory)
   {
      g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
   }
}

static void *glui_init(void)
{
   menu_handle_t *menu;
   glui_handle_t *glui = NULL;
   const video_driver_t *video_driver = NULL;
   gl_t *gl = (gl_t*)driver_video_resolve(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize GLUI menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu->userdata = (glui_handle_t*)calloc(1, sizeof(glui_handle_t));

   if (!menu->userdata)
   {
      free(menu);
      return NULL;
   }

   glui = (glui_handle_t*)menu->userdata;
   glui->bg = 0;

   glui_init_core_info(menu);

   return menu;
}

static void glui_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (menu->userdata)
      free(menu->userdata);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
}

static GLuint glui_png_texture_load_(const char * file_name)
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
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   free(ti.pixels);

   return texture;
}
static int glui_png_texture_load_wrap(void * file_name)
{
   return glui_png_texture_load_(file_name);
}


static GLuint glui_png_texture_load(const char* file_name)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;
      thr->cmd_data.custom_command.method = glui_png_texture_load_wrap;
      thr->cmd_data.custom_command.data   = (void*)file_name;
      thr->send_cmd_func(thr, CMD_CUSTOM_COMMAND);
      thr->wait_reply_func(thr, CMD_CUSTOM_COMMAND);

      return thr->cmd_data.custom_command.return_value;

   }

   return glui_png_texture_load_(file_name);
}


static void glui_context_reset(void *data)
{
   char bgpath[PATH_MAX];
   glui_handle_t *glui = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
    
   (void)gl;

   driver.gfx_use_rgba = true;

   if (!menu)
      return;

   glui = (glui_handle_t*)menu->userdata;

   if (!glui)
      return;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "glui", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   if (path_file_exists(bgpath))
      glui->bg = glui_png_texture_load(bgpath);

   printf("%d\n", glui->bg);
}

menu_ctx_driver_t menu_ctx_glui = {
   NULL,
   glui_get_message,
   NULL,
   glui_frame,
   glui_init,
   NULL,
   glui_free,
   glui_context_reset,
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
   NULL,
   NULL,
   NULL,
   NULL,
   glui_init_core_info,
   &menu_ctx_backend_common,
   "glui",
};
