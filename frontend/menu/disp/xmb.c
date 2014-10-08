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

#define THEME "monochrome"

unsigned line_height, glyph_width, xmb_margin, xmb_term_width, xmb_term_height;
GLuint xmb_bg = 0;
char box_message[PATH_MAX];

float xmb_alpha = 1.0f;

char icon_dir[4];

int xmb_icon_size = 96;
float xmb_hspacing = 150.0;
float xmb_vspacing = 48.0;
float xmb_font_size = 18;
float xmb_margin_left = 224;
float xmb_margin_top = 192;
float xmb_title_margin_left = 15.0;
float xmb_title_margin_top = 30.0;
float xmb_label_margin_left = 64;
float xmb_label_margin_top = 6.0;
float xmb_setting_margin_left = 400;

float xmb_above_item_offset = -1.0;
float xmb_active_item_factor = 2.75;
float xmb_under_item_offset = 4.0;

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

struct xmb_texture_item
{
   GLuint id;
   char path[PATH_MAX];
};

struct xmb_texture_item textures[TEXTURE_LAST];

static void xmb_draw_icon(GLuint texture, float x, float y,
      float alpha, float rotation, float scale)
{
   if (alpha > xmb_alpha)
      alpha = xmb_alpha;

   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb_icon_size || x > gl->win_width + xmb_icon_size
         || y < -xmb_icon_size || y > gl->win_height + xmb_icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, xmb_icon_size, xmb_icon_size);

   struct gl_coords coords;
   coords.vertices = 4;
   coords.vertex = vertex;
   coords.tex_coord = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   math_matrix mymat;

   math_matrix mrot;
   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, scale, scale, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
}

static void xmb_draw_text(const char *str, float x,
      float y, float scale, float alpha)
{
   if (alpha > xmb_alpha)
      alpha = xmb_alpha;
   uint8_t a8 = 255 * alpha;
   if (a8 == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb_icon_size || x > gl->win_width + xmb_icon_size
         || y < -xmb_icon_size || y > gl->win_height + xmb_icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   struct font_params params = {0};
   params.x = x / gl->win_width;
   params.y = 1.0f - y / gl->win_height;

   params.scale = scale;
   params.color = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;

   if (driver.video_data && driver.video_poke
       && driver.video_poke->set_osd_msg)
       driver.video_poke->set_osd_msg(driver.video_data,
                                      str, &params);
}

static void xmb_render_background(void)
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
   gl->coords.color = xmb_bg ? gl->white_color_ptr : black_color;
   glBindTexture(GL_TEXTURE_2D, xmb_bg);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   gl->coords.vertices = 4;
   gl->shader->set_coords(&gl->coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

static void xmb_get_message(const char *message)
{
   size_t i;
   (void)i;

   if (!driver.menu || !message || !*message)
      return;

   strlcpy(box_message, message, sizeof(box_message));
}

static void xmb_render_messagebox(const char *message)
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
      xmb_draw_text(msg, x, y + i * xmb_vspacing, 1, 1);
   }

   string_list_free(list);
}

static void xmb_frame(void)
{
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   if (driver.menu->need_refresh
         && g_extern.is_menu
         && !driver.menu->msg_force)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);

   xmb_render_background();

   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, title, sizeof(title));
   xmb_draw_text(title, 30, 40, 1, 1);

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
   xmb_draw_text(title_msg, 30, gl->win_height - 30, 1, 1);

   unsigned x, y = 0;
   int selptr = driver.menu->selection_ptr;
   int i;

   for (i = 0; i < file_list_get_size(driver.menu->selection_buf); i++)
   {
      char message[PATH_MAX], type_str[PATH_MAX],
           entry_title_buf[PATH_MAX], type_str_buf[PATH_MAX],
           path_buf[PATH_MAX];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;

      file_list_get_at_offset(driver.menu->selection_buf, i, &path,
            &entry_label, &type);
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
            setting_data_get_list(SL_FLAG_ALL_SETTINGS, false),
            driver.menu->selection_buf->list[i].label);
      (void)setting;

      disp_set_label(&w, type, i, label,
            type_str, sizeof(type_str), 
            entry_label, path,
            path_buf, sizeof(path_buf));

      bool selected = (i == selptr);

      snprintf(message, sizeof(message), "%s", path_buf);

      float iy;
      float ia = 0.5;
      float iz = 0.5;

      iy = i < selptr ? xmb_vspacing * (i - selptr + xmb_above_item_offset) :
         xmb_vspacing * (i - selptr + xmb_under_item_offset);

      if (i == selptr)
      {
         ia = 1.0;
         iz = 1.0;
         iy = xmb_vspacing * xmb_active_item_factor;
      }

      xmb_draw_icon(textures[TEXTURE_SETTING].id,
            xmb_margin_left + xmb_hspacing - xmb_icon_size/2.0, 
            xmb_margin_top + iy + xmb_icon_size/2.0, 
            ia, 
            0, 
            iz);

      xmb_draw_text(message,
            xmb_margin_left + xmb_hspacing + xmb_label_margin_left, 
            xmb_margin_top + iy + xmb_label_margin_top, 
            1, 
            ia);

      xmb_draw_text(type_str,
            xmb_margin_left + xmb_hspacing + xmb_label_margin_left + xmb_setting_margin_left, 
            xmb_margin_top + iy + xmb_label_margin_top, 
            1, 
            ia);
   }

   xmb_draw_icon(textures[TEXTURE_SETTINGS].id, 
         xmb_margin_left + xmb_hspacing - xmb_icon_size/2.0,
         xmb_margin_top + xmb_icon_size/2.0, 
         1.0, 
         0, 
         1.0);

#ifdef GEKKO
   const char *message_queue;

   if (driver.menu->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      driver.menu->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   xmb_render_messagebox(message_queue);
#endif

   if (driver.menu->keyboard.display)
   {
      char msg[PATH_MAX];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      xmb_render_messagebox(msg);
   }

   if (box_message[0] != '\0')
   {
      xmb_render_background();
      xmb_render_messagebox(box_message);
      box_message[0] = '\0';
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void xmb_init_core_info(void *data)
{
   (void)data;

   core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
   if (*g_settings.libretro_directory)
   {
      g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
   }
}

static void *xmb_init(void)
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

   xmb_init_core_info(menu);

   strcpy(icon_dir, "96");

   return menu;
}

static void xmb_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
}

static GLuint xmb_png_texture_load_(const char * file_name)
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
static int xmb_png_texture_load_wrap(void * file_name)
{
   return xmb_png_texture_load_(file_name);
}

static GLuint xmb_png_texture_load(const char* file_name)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;
      thr->cmd_data.custom_command.method = xmb_png_texture_load_wrap;
      thr->cmd_data.custom_command.data   = (void*)file_name;
      thr->send_cmd_func(thr, CMD_CUSTOM_COMMAND);
      thr->wait_reply_func(thr, CMD_CUSTOM_COMMAND);

      return thr->cmd_data.custom_command.return_value;

   }

   return xmb_png_texture_load_(file_name);
}

static void xmb_context_reset(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
    
   (void)gl;

   driver.gfx_use_rgba = true;

   if (!menu)
      return;

   int k;
   char bgpath[PATH_MAX];
   char mediapath[PATH_MAX], themepath[PATH_MAX], iconpath[PATH_MAX];

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   if (path_file_exists(bgpath))
      xmb_bg = xmb_png_texture_load(bgpath);

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

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
      textures[k].id = xmb_png_texture_load(textures[k].path);
}

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_get_message,
   NULL,
   xmb_frame,
   xmb_init,
   xmb_free,
   xmb_context_reset,
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
   xmb_init_core_info,
   &menu_ctx_backend_common,
   "xmb",
};
