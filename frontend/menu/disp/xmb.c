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
#include "tween.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 0.02
#endif

typedef struct
{
   char  name[256];
   float alpha;
   float zoom;
   float y;
} xmb_node_t;


enum
{
   RMB_TEXTURE_MAIN = 0,
   RMB_TEXTURE_FONT,
   RMB_TEXTURE_BG,
   RMB_TEXTURE_SETTINGS,
   RMB_TEXTURE_SETTING,
   RMB_TEXTURE_SUBSETTING,
   RMB_TEXTURE_ARROW,
   RMB_TEXTURE_RUN,
   RMB_TEXTURE_RESUME,
   RMB_TEXTURE_SAVESTATE,
   RMB_TEXTURE_LOADSTATE,
   RMB_TEXTURE_SCREENSHOT,
   RMB_TEXTURE_RELOAD,
   RMB_TEXTURE_LAST
};

struct xmb_texture_item
{
   GLuint id;
   char path[PATH_MAX];
};

typedef struct xmb_handle
{
   int xmb_depth;
   GLuint xmb_bg;
   unsigned line_height;
   unsigned glyph_width;
   unsigned xmb_margin;
   unsigned xmb_term_width;
   unsigned xmb_term_height;
   char icon_dir[4];
   char box_message[PATH_MAX];
   char xmb_title[PATH_MAX];
   struct xmb_texture_item textures[RMB_TEXTURE_LAST];
   int xmb_icon_size;
   float xmb_alpha;
   float xmb_hspacing;
   float xmb_vspacing;
   float xmb_font_size;
   float xmb_margin_left;
   float xmb_margin_top;
   float xmb_title_margin_left;
   float xmb_title_margin_top;
   float xmb_label_margin_left;
   float xmb_label_margin_top;
   float xmb_setting_margin_left;
   float xmb_above_item_offset;
   float xmb_active_item_factor;
   float xmb_under_item_offset;
   xmb_node_t *xmb_nodes;
} xmb_handle_t;

static xmb_handle_t *xmb_menu_data;

static const GLfloat rmb_vertex[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

static const GLfloat rmb_tex_coord[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0,
};

static void xmb_draw_icon(GLuint texture, float x, float y,
      float alpha, float rotation, float scale)
{
   struct gl_coords coords;
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!xmb)
      return;

   if (alpha > xmb->xmb_alpha)
      alpha = xmb->xmb_alpha;

   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->xmb_icon_size || x > gl->win_width + xmb->xmb_icon_size
         || y < -xmb->xmb_icon_size || y > gl->win_height + xmb->xmb_icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, xmb->xmb_icon_size, xmb->xmb_icon_size);

   coords.vertices = 4;
   coords.vertex = rmb_vertex;
   coords.tex_coord = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
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
   uint8_t a8 = 0;
   struct font_params params = {0};
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!xmb)
      return;

   if (alpha > xmb->xmb_alpha)
      alpha = xmb->xmb_alpha;
   a8 = 255 * alpha;
   if (a8 == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->xmb_icon_size || x > gl->win_width + xmb->xmb_icon_size
         || y < -xmb->xmb_icon_size || y > gl->win_height + xmb->xmb_icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

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
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!xmb)
      return;

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
   gl->coords.color = xmb->xmb_bg ? gl->white_color_ptr : black_color;
   glBindTexture(GL_TEXTURE_2D, xmb->xmb_bg);

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
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!driver.menu || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_messagebox(const char *message)
{
   unsigned i;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!driver.menu || !gl || !xmb)
      return;

   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   int x = gl->win_width / 2 - strlen(list->elems[0].data) * xmb->glyph_width / 2;
   int y = gl->win_height / 2 - list->size * xmb->line_height / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      xmb_draw_text(msg, x, y + i * xmb->xmb_vspacing, 1, 1);
   }

   string_list_free(list);
}

static void xmb_selection_pointer_changed(void)
{
   int i;
   int current = driver.menu->selection_ptr;
   int num_nodes = file_list_get_size(driver.menu->selection_buf);
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!xmb)
      return;

   for (i = 0; i < num_nodes; i++)
   {
      float iy;
      float ia = 0.5;
      float iz = 0.5;
      xmb_node_t *node = (xmb_node_t*)&xmb->xmb_nodes[i];

      if (!node)
         continue;

      iy = (i < current) ? xmb->xmb_vspacing *
         (i - current + xmb->xmb_above_item_offset) :
         xmb->xmb_vspacing * (i - current + xmb->xmb_under_item_offset);

      if (i == current)
      {
         ia = 1.0;
         iz = 1.0;
         iy = xmb->xmb_vspacing * xmb->xmb_active_item_factor;
      }

      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, iz, &node->zoom,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, iy, &node->y,     &inOutQuad, NULL);
   }
}

static void xmb_populate_entries(void *data, const char *path,
      const char *labell, unsigned ii)
{
   int i;
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   int num_nodes = file_list_get_size(driver.menu->selection_buf);
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;

   if (!xmb)
      return;

   xmb->xmb_nodes = (xmb_node_t*)
      realloc(xmb->xmb_nodes, num_nodes * sizeof(xmb_node_t));

   if (!xmb->xmb_nodes)
      return;

   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   for (i = 0; i < num_nodes; i++)
   {
      char name[PATH_MAX], value[PATH_MAX], path_buf[PATH_MAX];
      float iy;
      xmb_node_t *node = NULL;
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      int current = driver.menu->selection_ptr;

      file_list_get_at_offset(driver.menu->selection_buf, i, &path,
            &entry_label, &type);
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
            driver.menu->list_settings,
            driver.menu->selection_buf->list[i].label);
      (void)setting;

      disp_set_label(&w, type, i, label,
            value, sizeof(value), 
            entry_label, path,
            path_buf, sizeof(path_buf));

      strlcpy(name, path_buf, sizeof(name));

      iy = (i < current) ? xmb->xmb_vspacing *
         (i - current + xmb->xmb_above_item_offset) :
         xmb->xmb_vspacing * (i - current + xmb->xmb_under_item_offset);

      if (i == current)
         iy = xmb->xmb_vspacing * xmb->xmb_active_item_factor;

      node = (xmb_node_t*)&xmb->xmb_nodes[i];

      if (!xmb)
         continue;

      strlcpy(node->name, name, sizeof(node->name));
      node->alpha = (i  == current) ? 1.0 : 0.5;
      node->zoom  = (i  == current) ? 1.0 : 0.5;
      node->y = iy;
   }
}

static void xmb_frame(void)
{
   int i;
   char title_msg[64];
   size_t end;
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!driver.menu || !gl)
      return;

   if (driver.menu->need_refresh
         && g_extern.is_menu
         && !driver.menu->msg_force)
      return;

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   xmb_render_background();

   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, xmb->xmb_title, sizeof(xmb->xmb_title));

   xmb_draw_text(xmb->xmb_title, 30, 40, 1, 1);

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

   end = file_list_get_size(driver.menu->selection_buf);

   for (i = 0; i < end; i++)
   {
      char value[PATH_MAX], path_buf[PATH_MAX];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;

      xmb_node_t *node = (xmb_node_t*)&xmb->xmb_nodes[i];

      file_list_get_at_offset(driver.menu->selection_buf, i, &path,
            &entry_label, &type);
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
            driver.menu->list_settings,
            driver.menu->selection_buf->list[i].label);
      (void)setting;

      disp_set_label(&w, type, i, label,
            value, sizeof(value),
            entry_label, path,
            path_buf, sizeof(path_buf));

      xmb_draw_icon(xmb->textures[RMB_TEXTURE_SETTING].id,
            xmb->xmb_margin_left + xmb->xmb_hspacing - xmb->xmb_icon_size/2.0, 
            xmb->xmb_margin_top + node->y + xmb->xmb_icon_size/2.0, 
            node->alpha, 
            0, 
            node->zoom);

      xmb_draw_text(node->name,
            xmb->xmb_margin_left + xmb->xmb_hspacing + xmb->xmb_label_margin_left, 
            xmb->xmb_margin_top + node->y + xmb->xmb_label_margin_top, 
            1, 
            node->alpha);

      xmb_draw_text(value,
            xmb->xmb_margin_left + xmb->xmb_hspacing + 
            xmb->xmb_label_margin_left + xmb->xmb_setting_margin_left, 
            xmb->xmb_margin_top + node->y + xmb->xmb_label_margin_top, 
            1, 
            node->alpha);
   }

   xmb_draw_icon(xmb->textures[RMB_TEXTURE_SETTINGS].id, 
         xmb->xmb_margin_left + xmb->xmb_hspacing - xmb->xmb_icon_size / 2.0,
         xmb->xmb_margin_top + xmb->xmb_icon_size / 2.0, 
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
      snprintf(msg, sizeof(msg), "%s\n%s",
            driver.menu->keyboard.label, str);
      xmb_render_messagebox(msg);
   }

   if (xmb->box_message[0] != '\0')
   {
      xmb_render_background();
      xmb_render_messagebox(xmb->box_message);
      xmb->box_message[0] = '\0';
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
   menu_handle_t *menu = NULL;
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

   xmb_menu_data = (xmb_handle_t*)calloc(1, sizeof(*xmb_menu_data));

   if (!xmb_menu_data)
      return NULL;

   xmb_menu_data->xmb_icon_size           = 96;
   xmb_menu_data->xmb_hspacing            = 150.0;
   xmb_menu_data->xmb_vspacing            = 48.0;
   xmb_menu_data->xmb_font_size           = 18;
   xmb_menu_data->xmb_margin_left         = 224;
   xmb_menu_data->xmb_margin_top          = 192;
   xmb_menu_data->xmb_title_margin_left   = 15.0;
   xmb_menu_data->xmb_title_margin_top    = 30.0;
   xmb_menu_data->xmb_label_margin_left   = 64;
   xmb_menu_data->xmb_label_margin_top    = 6.0;
   xmb_menu_data->xmb_setting_margin_left = 400;
   xmb_menu_data->xmb_above_item_offset   = -1.0;
   xmb_menu_data->xmb_active_item_factor  = 2.75;
   xmb_menu_data->xmb_under_item_offset   = 4.0;
   xmb_menu_data->xmb_alpha               = 1.0f;
   xmb_menu_data->xmb_depth               = 0;
   xmb_menu_data->xmb_bg                  = 0;

   xmb_init_core_info(menu);

   strlcpy(xmb_menu_data->icon_dir, "96", sizeof(xmb_menu_data->icon_dir));

   return menu;
}

static void xmb_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);

   if (xmb_menu_data)
      free(xmb_menu_data);

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
   int k;
   char bgpath[PATH_MAX];
   char mediapath[PATH_MAX], themepath[PATH_MAX], iconpath[PATH_MAX];
   menu_handle_t *menu = (menu_handle_t*)data;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   xmb_handle_t *xmb = (xmb_handle_t*)xmb_menu_data;
    
   (void)gl;

   driver.gfx_use_rgba = true;

   if (!menu || !xmb)
      return;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   if (path_file_exists(bgpath))
      xmb->xmb_bg = xmb_png_texture_load(bgpath);

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   fill_pathname_join(xmb->textures[RMB_TEXTURE_BG].path, iconpath,
         "bg.png", sizeof(xmb->textures[RMB_TEXTURE_BG].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(xmb->textures[RMB_TEXTURE_SETTINGS].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(xmb->textures[RMB_TEXTURE_SETTING].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(xmb->textures[RMB_TEXTURE_SUBSETTING].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(xmb->textures[RMB_TEXTURE_ARROW].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(xmb->textures[RMB_TEXTURE_RUN].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(xmb->textures[RMB_TEXTURE_RESUME].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(xmb->textures[RMB_TEXTURE_SAVESTATE].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(xmb->textures[RMB_TEXTURE_LOADSTATE].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(xmb->textures[RMB_TEXTURE_SCREENSHOT].path));
   fill_pathname_join(xmb->textures[RMB_TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(xmb->textures[RMB_TEXTURE_RELOAD].path));

   for (k = 0; k < RMB_TEXTURE_LAST; k++)
      xmb->textures[k].id = xmb_png_texture_load(xmb->textures[k].path);
}

static void xmb_navigation_clear(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_decrement(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_increment(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_set(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_set_last(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_descend_alphabet(void *data, size_t *unused)
{
   (void)data;
   (void)unused;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_ascend_alphabet(void *data, size_t *unused)
{
   (void)data;
   (void)unused;

   xmb_selection_pointer_changed();
}

static void xmb_list_insert(void *data,
      const char *path, const char *unused, size_t list_size)
{
   (void)data;
   (void)path;
   (void)unused;
   (void)list_size;
}

static void xmb_list_delete(void *data, size_t list_size)
{
   (void)data;
   (void)list_size;
}

static void xmb_list_clear(void *data)
{
   (void)data;
}

static void xmb_list_set_selection(void *data)
{
   (void)data;
}

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_get_message,
   NULL,
   xmb_frame,
   xmb_init,
   NULL,
   xmb_free,
   xmb_context_reset,
   NULL,
   xmb_populate_entries,
   NULL,
   NULL,
   xmb_navigation_clear,
   xmb_navigation_decrement,
   xmb_navigation_increment,
   xmb_navigation_set,
   xmb_navigation_set_last,
   xmb_navigation_descend_alphabet,
   xmb_navigation_ascend_alphabet,
   xmb_list_insert,
   xmb_list_delete,
   xmb_list_clear,
   xmb_list_set_selection,
   xmb_init_core_info,
   &menu_ctx_backend_common,
   "xmb",
};
