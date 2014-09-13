/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

int line_height, glyph_width, glui_term_start_x, glui_term_start_y, 
   glui_term_width, glui_term_height;

const gl_font_renderer_t *font_driver;

static void blit_line(float x, float y, const char *message, bool green)
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

   if (font_driver)
      font_driver->render_msg((void*)driver.menu->font, message, &params);
}

static void glui_render_background(void)
{
   GLfloat color[] = {
      0.0f, 0.0f, 0.0f, 0.9f,
      0.0f, 0.0f, 0.0f, 0.9f,
      0.0f, 0.0f, 0.0f, 0.9f,
      0.0f, 0.0f, 0.0f, 0.9f,
   };

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   glEnable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = (GLfloat*)calloc(4, sizeof(GLfloat));
   gl->coords.color = color;

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

static void glui_render_messagebox(const char *message)
{
}

static void glui_frame(void)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

   size_t begin = 0;
   size_t end;

   if (driver.menu->selection_ptr >= glui_term_height / 2)
      begin = driver.menu->selection_ptr - glui_term_height / 2;
   end   = (driver.menu->selection_ptr + glui_term_height <=
         file_list_get_size(driver.menu->selection_buf)) ?
      driver.menu->selection_ptr + glui_term_height :
      file_list_get_size(driver.menu->selection_buf);

   /* Do not scroll if all items are visible. */
   if (file_list_get_size(driver.menu->selection_buf) <= glui_term_height)
      begin = 0;

   if (end - begin > glui_term_height)
      end = begin + glui_term_height;

   end -= 1;

   glui_render_background();

   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   unsigned menu_type_is = 0;
   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   if (driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->type_is)
      menu_type_is = driver.menu_ctx->backend->type_is(label, menu_type);

#if 0
   RARCH_LOG("Dir is: %s\n", label);
#endif

   get_title(label, dir, menu_type, menu_type_is,
         title, sizeof(title));

   char title_buf[256];
   menu_ticker_line(title_buf, glui_term_width - 3,
         g_extern.frame_count / glui_term_start_x, title, true);
   blit_line(glui_term_start_x + glui_term_start_x, glui_term_start_y, 
         title_buf, true);

   char title_msg[64];
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
   blit_line(
         glui_term_start_x + glui_term_start_x,
         (glui_term_height * line_height) +
         glui_term_start_y + 2, title_msg, true);

   unsigned x, y;
   size_t i;

   x = glui_term_start_x;
   y = glui_term_start_y + line_height;

   for (i = begin; i < end; i++, y += line_height)
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
            setting_data_get_list(),
            driver.menu->selection_buf->list[i].label);
      (void)setting;

      disp_set_label(&w, type, i, label,
            type_str, sizeof(type_str), 
            entry_label, path,
            path_buf, sizeof(path_buf));

      selected = (i == driver.menu->selection_ptr);

      menu_ticker_line(entry_title_buf, glui_term_width - (w + 1 + 2),
            g_extern.frame_count / glui_term_start_x, path_buf, selected);
      menu_ticker_line(type_str_buf, w, 
            g_extern.frame_count / glui_term_start_x, type_str, selected);

      snprintf(message, sizeof(message), "%c %s",
            ' ',
            entry_title_buf);

      blit_line(x, y, message, selected);

      blit_line(gl->win_width - glyph_width * w 
         - glui_term_start_x , y, type_str_buf, selected);
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
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      glui_render_messagebox(msg);
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

   glui_init_core_info(menu);

   return menu;
}

static void glui_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
}

static int glui_input_postprocess(uint64_t old_state)
{
   (void)old_state;

   if ((driver.menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      rarch_main_command(RARCH_CMD_RESUME);
      return -1;
   }

   return 0;
}

static void glui_context_reset(void *data)
{
   char mediapath[256], themepath[256], iconpath[256];
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   driver.gfx_use_rgba = true;

   if (!menu)
      return;

   line_height = g_settings.video.font_size * 4 / 3;
   glyph_width = line_height / 2;
   glui_term_start_x = gl->win_width / 21;
   glui_term_start_y = gl->win_height / 9;
   glui_term_width = (gl->win_width - glui_term_start_x - glui_term_start_x) 
         / glyph_width;
   glui_term_height = ((gl->win_height - glui_term_start_y) 
         / (line_height)) - 1;

   gl_font_init_first(&font_driver, (void*)&menu->font, gl, g_settings.video.font_path,
      g_settings.video.font_size);
}

menu_ctx_driver_t menu_ctx_glui = {
   NULL,
   NULL,
   NULL,
   glui_frame,
   glui_init,
   glui_free,
   glui_context_reset,
   NULL,
   NULL,
   NULL,
   glui_input_postprocess,
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
