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

#include "../menu_common.h"
#include "../menu_driver.h"
#include "menu_display.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../gfx/shader/shader_gl_common.h"
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

unsigned line_height, glyph_width, glui_margin, glui_term_width, glui_term_height;
GLuint glui_bg = 0;
char box_message[PATH_MAX];

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

static void glui_render_background(void)
{
   GLfloat black_color[] = {
      0.0f, 0.0f, 0.0f, 0.8f,
      0.0f, 0.0f, 0.0f, 0.8f,
      0.0f, 0.0f, 0.0f, 0.8f,
      0.0f, 0.0f, 0.0f, 0.8f,
   };

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

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

   glEnable(GL_BLEND);

   gl->coords.vertex = vertex;
   gl->coords.tex_coord = tex_coord;
   gl->coords.color = glui_bg ? gl->white_color_ptr : black_color;
   glBindTexture(GL_TEXTURE_2D, glui_bg);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

static void glui_get_message(const char *message)
{
   size_t i;
   (void)i;

   if (!driver.menu || !message || !*message)
      return;

   strlcpy(box_message, message, sizeof(box_message));
}

static void glui_render_messagebox(const char *message)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   unsigned i;
   int x = gl->win_width / 2 - strlen(list->elems[0].data) * glyph_width / 2;
   int y = gl->win_height / 2 - list->size * line_height / 2;
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      glui_blit_line(x, y + i * line_height, msg, false);
   }

   string_list_free(list);
}

static void glui_frame(void)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   line_height = g_settings.video.font_size * 4 / 3;
   glyph_width = line_height / 2;
   glui_margin = gl->win_width / 20 ;
   glui_term_width = (gl->win_width - glui_margin * 2) / glyph_width;
   glui_term_height = (gl->win_height - glui_margin * 2) / line_height - 2;

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

   glui_render_background();

   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, title, sizeof(title));

   char title_buf[256];
   menu_ticker_line(title_buf, glui_term_width - 3,
         g_extern.frame_count / glui_margin, title, true);
   glui_blit_line(glui_margin * 2, glui_margin + line_height,
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
   glui_blit_line(
         glui_margin * 2,
         glui_margin + glui_term_height * line_height + line_height * 2, title_msg, true);

   unsigned x, y;
   size_t i;

   x = glui_margin;
   y = glui_margin + line_height*2;

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
            g_extern.frame_count / glui_margin, path_buf, selected);
      menu_ticker_line(type_str_buf, w, 
            g_extern.frame_count / glui_margin, type_str, selected);

      snprintf(message, sizeof(message), "%s", entry_title_buf);

      glui_blit_line(x, y, message, selected);

      glui_blit_line(gl->win_width - glyph_width * w - glui_margin , 
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
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      glui_render_messagebox(msg);
   }

   if (box_message[0] != '\0')
   {
      glui_render_background();
      glui_render_messagebox(box_message);
      box_message[0] = '\0';
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

static GLuint glui_png_texture_load(const char * file_name)
{
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

static void glui_context_reset(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
    
   (void)gl;

   driver.gfx_use_rgba = true;

   if (!menu)
      return;

   char bgpath[PATH_MAX];

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "glui", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   if (path_file_exists(bgpath))
      glui_bg = glui_png_texture_load(bgpath);
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
