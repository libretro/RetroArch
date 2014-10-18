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
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../menu_list.h"
#include "../menu_common.h"
#include "../menu_driver.h"
#include "menu_display.h"
#include "../../../general.h"
#include "../../../gfx/gl_common.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../compat/posix_string.h"

#include "shared.h"
#include "../menu_animation.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 0.02
#endif

typedef struct
{
   float alpha;
   float zoom;
   float y;
} xmb_node_t;

enum
{
   XMB_TEXTURE_BG = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
   XMB_TEXTURE_LAST
};

struct xmb_texture_item
{
   GLuint id;
   char path[PATH_MAX];
};

typedef struct xmb_handle
{
   int depth;
   int old_depth;
   char icon_dir[4];
   char box_message[PATH_MAX];
   char title[PATH_MAX];
   struct xmb_texture_item textures[XMB_TEXTURE_LAST];
   int icon_size;
   float x;
   float alpha;
   float hspacing;
   float vspacing;
   float font_size;
   float margin_left;
   float margin_top;
   float title_margin_left;
   float title_margin_top;
   float label_margin_left;
   float label_margin_top;
   float setting_margin_left;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;
   float above_subitem_offset;
   float c_active_zoom;
   float c_active_alpha;
   float i_active_zoom;
   float i_active_alpha;
   float c_passive_zoom;
   float c_passive_alpha;
   float i_passive_zoom;
   float i_passive_alpha;
} xmb_handle_t;

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
      float alpha, float rotation, float scale_factor)
{
   struct gl_coords coords;
   math_matrix mymat, mrot, mscal;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->icon_size || x > gl->win_width + xmb->icon_size
         || y < -xmb->icon_size || y > gl->win_height + xmb->icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, xmb->icon_size, xmb->icon_size);

   coords.vertices = 4;
   coords.vertex = rmb_vertex;
   coords.tex_coord = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
}

static void xmb_draw_text(const char *str, float x,
      float y, float scale_factor, float alpha)
{
   uint8_t a8 = 0;
   struct font_params params = {0};
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;
   a8 = 255 * alpha;
   if (a8 == 0)
      return;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->icon_size || x > gl->win_width + xmb->icon_size
         || y < -xmb->icon_size || y > gl->win_height + xmb->icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

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

static void xmb_render_background(bool force_transparency)
{
   float alpha = 0.75f;
   gl_t *gl = NULL;
   xmb_handle_t *xmb = NULL;

   if (!driver.menu)
      return;

   xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
   };

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

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
      && xmb->textures[XMB_TEXTURE_BG].id)
   {
      coords.color = color;
      glBindTexture(GL_TEXTURE_2D, xmb->textures[XMB_TEXTURE_BG].id);
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

static void xmb_get_message(const char *message)
{
   size_t i;
   (void)i;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_messagebox(const char *message)
{
   unsigned i;
   gl_t *gl = (gl_t*)driver_video_resolve(NULL);
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!gl || !xmb)
      return;

   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   int x = gl->win_width / 2 - strlen(list->elems[0].data) * g_settings.video.font_size / 4;
   int y = gl->win_height / 2 - list->size * g_settings.video.font_size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(msg, x, y + i * g_settings.video.font_size, 1, 1);
   }

   string_list_free(list);
}

static void xmb_selection_pointer_changed(void)
{
   int i, current, end;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   current = driver.menu->selection_ptr;
   end = menu_list_get_size(driver.menu->menu_list);

   for (i = 0; i < end; i++)
   {
      float iy;
      float ia = xmb->i_passive_alpha;
      float iz = xmb->i_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            driver.menu->menu_list->selection_buf, i);

      if (!node)
         continue;

      iy = (i < current) ? xmb->vspacing *
         (i - current + xmb->above_item_offset) :
         xmb->vspacing * (i - current + xmb->under_item_offset);

      if (i == current)
      {
         ia = xmb->i_active_alpha;
         iz = xmb->i_active_zoom;
         iy = xmb->vspacing * xmb->active_item_factor;
      }

      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, iz, &node->zoom,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, iy, &node->y,     &inOutQuad, NULL);
   }
}

static void xmb_populate_entries(void *data, const char *path,
      const char *label, unsigned j)
{
   int i, current, end;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   xmb->depth = menu_list_get_stack_size(driver.menu->menu_list);

   if (xmb->depth > xmb->old_depth)
   {
      add_tween(XMB_DELAY, xmb->x-20, &xmb->x, &inOutQuad, NULL);
   }
   else if (xmb->depth < xmb->old_depth)
   {
      add_tween(XMB_DELAY, xmb->x+20, &xmb->x, &inOutQuad, NULL);
   }


   current = driver.menu->selection_ptr;
   end = menu_list_get_size(driver.menu->menu_list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            driver.menu->menu_list->selection_buf, i);

      if (!node)
         continue;

      node->alpha = xmb->i_passive_alpha;
      node->zoom = xmb->i_passive_zoom;
      node->y = (i < current) ? xmb->vspacing *
         (i - current + xmb->above_item_offset) :
         xmb->vspacing * (i - current + xmb->under_item_offset);

      if (i == current)
      {
         node->alpha = xmb->i_active_alpha;
         node->zoom = xmb->i_active_zoom;
         node->y = xmb->vspacing * xmb->active_item_factor;
      }
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_frame(void)
{
   int i, current;
   char title_msg[64];
   size_t end;
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   gl_t *gl = (gl_t*)driver_video_resolve(NULL);

   if (!xmb || !gl)
      return;

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   xmb_render_background(false);

   menu_list_get_last_stack(driver.menu->menu_list, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, xmb->title, sizeof(xmb->title));

   xmb_draw_text(
         xmb->title, xmb->title_margin_left, xmb->title_margin_top, 1, 1);

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
   xmb_draw_text(title_msg, xmb->title_margin_left, 
         gl->win_height - xmb->title_margin_top/2, 1, 1);

   end = menu_list_get_size(driver.menu->menu_list);
   current = driver.menu->selection_ptr;

   for (i = 0; i < end; i++)
   {
      char val_buf[PATH_MAX], path_buf[PATH_MAX];
      char name[256], value[256];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      xmb_node_t *node = NULL;

      menu_list_get_at_offset(driver.menu->menu_list->selection_buf, i, &path,
            &entry_label, &type);
      node = (xmb_node_t*)file_list_get_userdata_at_offset(
            driver.menu->menu_list->selection_buf, i);
      
      disp_set_label(&w, type, i, label,
            val_buf, sizeof(val_buf),
            entry_label, path,
            path_buf, sizeof(path_buf));

      GLuint icon = 0;
      switch(type)
      {
         case MENU_FILE_DIRECTORY:
            icon = xmb->textures[XMB_TEXTURE_FOLDER].id;
            break;
         case MENU_FILE_PLAIN:
            icon = xmb->textures[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_CARCHIVE:
            icon = xmb->textures[XMB_TEXTURE_ZIP].id;
            break;
         default:
            icon = xmb->textures[XMB_TEXTURE_SETTING].id;
            break;
      }

      xmb_draw_icon(icon,
            xmb->x + xmb->margin_left + xmb->hspacing - xmb->icon_size/2.0, 
            xmb->margin_top + node->y + xmb->icon_size/2.0, 
            node->alpha, 
            0, 
            node->zoom);

      menu_ticker_line(name, 35, g_extern.frame_count / 20, path_buf,
            (i == current));

      xmb_draw_text(name,
            xmb->x + xmb->margin_left + xmb->hspacing + xmb->label_margin_left, 
            xmb->margin_top + node->y + xmb->label_margin_top, 
            1, 
            node->alpha);

      menu_ticker_line(value, 35, g_extern.frame_count / 20, val_buf,
            (i == current));

      xmb_draw_text(value,
            xmb->x + xmb->margin_left + xmb->hspacing + 
            xmb->label_margin_left + xmb->setting_margin_left, 
            xmb->margin_top + node->y + xmb->label_margin_top, 
            1, 
            node->alpha);
   }

   xmb_draw_icon(xmb->textures[XMB_TEXTURE_SETTINGS].id, 
         xmb->x + xmb->margin_left + xmb->hspacing - xmb->icon_size / 2.0,
         xmb->margin_top + xmb->icon_size / 2.0, 
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
      xmb_render_background(true);
      xmb_render_messagebox(msg);
   }

   if (xmb->box_message[0] != '\0')
   {
      xmb_render_background(true);
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
   xmb_handle_t *xmb = NULL;
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

   menu->userdata = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!menu->userdata)
   {
      free(menu);
      return NULL;
   }

   xmb = (xmb_handle_t*)menu->userdata;

   xmb->x               = 0;
   xmb->alpha           = 1.0f;
   xmb->depth           = 1;
   xmb->old_depth       = 1;

   xmb->c_active_zoom   = 1.0;
   xmb->c_passive_zoom  = 0.5;
   xmb->i_active_zoom   = 1.0;
   xmb->i_passive_zoom  = 0.5;

   xmb->c_active_alpha  = 1.0;
   xmb->c_passive_alpha = 0.5;
   xmb->i_active_alpha  = 1.0;
   xmb->i_passive_alpha = 0.5;

   xmb->above_subitem_offset = 1.5;
   xmb->above_item_offset    = -1.0;
   xmb->active_item_factor   = 2.75;
   xmb->under_item_offset    = 4.0;

   float scale_factor;

   if (gl->win_width >= 3840)
   {
      scale_factor = 2.0;
      strlcpy(xmb->icon_dir, "256", sizeof(xmb->icon_dir));
   }
   else if (gl->win_width >= 2560)
   {
      scale_factor = 1.5;
      strlcpy(xmb->icon_dir, "192", sizeof(xmb->icon_dir));
   }
   else if (gl->win_width >= 1920)
   {
      scale_factor = 1.0;
      strlcpy(xmb->icon_dir, "128", sizeof(xmb->icon_dir));
   }
   else if (gl->win_width <= 640)
   {
      scale_factor = 0.5;
      strlcpy(xmb->icon_dir, "64", sizeof(xmb->icon_dir));
   }
   else
   {
      scale_factor = 3.0f/4.0f;
      strlcpy(xmb->icon_dir, "96", sizeof(xmb->icon_dir));
   }

   xmb->icon_size = 128.0 * scale_factor;
   xmb->hspacing = 200.0 * scale_factor;
   xmb->vspacing = 64.0 * scale_factor;
   xmb->margin_left = 336.0 * scale_factor;
   xmb->margin_top = 256 * scale_factor;
   xmb->title_margin_left = 15.0 * scale_factor;
   xmb->title_margin_top = 20.0 * scale_factor + g_settings.video.font_size/3.0;
   xmb->label_margin_left = 85.0 * scale_factor;
   xmb->label_margin_top = g_settings.video.font_size/3.0;
   xmb->setting_margin_left = 600.0 * scale_factor;

   xmb_init_core_info(menu);

   return menu;
}

static void xmb_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);

   if (menu->userdata)
      free(menu->userdata);

   g_extern.core_info = NULL;
}

static GLuint xmb_png_texture_load_(const char * file_name)
{
   struct texture_image ti = {0};
   GLuint texture = 0;

   if (! path_file_exists(file_name))
      return 0;

   texture_image_load(&ti, file_name);

   /* Generate the OpenGL texture object */
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
   gl_t *gl = NULL;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   gl = (gl_t*)driver_video_resolve(NULL);

   if (!gl)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;
    
   (void)gl;

   driver.gfx_use_rgba = true;

   if (!menu || !xmb)
      return;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   fill_pathname_join(xmb->textures[XMB_TEXTURE_BG].path, iconpath,
         "bg.png", sizeof(xmb->textures[XMB_TEXTURE_BG].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(xmb->textures[XMB_TEXTURE_SETTINGS].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(xmb->textures[XMB_TEXTURE_SETTING].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(xmb->textures[XMB_TEXTURE_SUBSETTING].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(xmb->textures[XMB_TEXTURE_ARROW].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(xmb->textures[XMB_TEXTURE_RUN].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(xmb->textures[XMB_TEXTURE_RESUME].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(xmb->textures[XMB_TEXTURE_SAVESTATE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(xmb->textures[XMB_TEXTURE_LOADSTATE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(xmb->textures[XMB_TEXTURE_SCREENSHOT].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(xmb->textures[XMB_TEXTURE_RELOAD].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_FILE].path, iconpath,
         "file.png", sizeof(xmb->textures[XMB_TEXTURE_RELOAD].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_FOLDER].path, iconpath,
         "folder.png", sizeof(xmb->textures[XMB_TEXTURE_RELOAD].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_ZIP].path, iconpath,
         "zip.png", sizeof(xmb->textures[XMB_TEXTURE_RELOAD].path));

   for (k = 0; k < XMB_TEXTURE_LAST; k++)
      xmb->textures[k].id = xmb_png_texture_load(xmb->textures[k].path);
}

static void xmb_navigation_clear(void *data, bool pending_push)
{
   (void)data;

   if (!pending_push)
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
   int current = 0, i = list_size;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;
   file_list_t *list = (file_list_t*)data;

   if (!list || !xmb)
      return;

   list->list[i].userdata = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!list->list[i].userdata)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   xmb_node_t *node = (xmb_node_t*)list->list[i].userdata;

   if (!node)
      return;

   current = driver.menu->selection_ptr;

   float iy = (i < current) ? xmb->vspacing *
      (i - current + xmb->above_item_offset) :
      xmb->vspacing * (i - current + xmb->under_item_offset);

   if (i == current)
      iy = xmb->vspacing * xmb->active_item_factor;

   node->alpha = (i == current) ? xmb->i_active_alpha : xmb->i_passive_alpha;
   node->zoom  = (i == current) ? xmb->i_active_zoom : xmb->i_passive_zoom;
   node->y = iy;
}

static void xmb_list_delete(void *data, size_t index,
      size_t list_size)
{
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   if (list->list[index].userdata)
      free(list->list[index].userdata);
   list->list[index].userdata = NULL;
}

static void xmb_list_clear(void *data)
{
   (void)data;
}

static void xmb_list_set_selection(void *data)
{
   (void)data;
}

static void xmb_context_destroy(void *data)
{
   int i;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)driver.menu;

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      glDeleteTextures(1, &xmb->textures[i].id);
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
   xmb_context_destroy,
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
