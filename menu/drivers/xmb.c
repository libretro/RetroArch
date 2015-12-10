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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <file/file_path.h>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <string/string_list.h>
#include <gfx/math/matrix_4x4.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_entry.h"
#include "../menu_animation.h"
#include "../menu_display.h"
#include "../menu_hash.h"
#include "../menu_display.h"
#include "../menu_navigation.h"

#include "../menu_cbs.h"

#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../file_ext.h"

#include "../../tasks/tasks.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 10
#endif

#define XMB_ABOVE_OFFSET_SUBITEM     1.5
#define XMB_ABOVE_OFFSET_ITEM       -1.0
#define XMB_ITEM_ACTIVE_FACTOR       3.0
#define XMB_UNDER_OFFSET_ITEM        5.0

#define XMB_CATEGORIES_ACTIVE_ZOOM   1.0
#define XMB_CATEGORIES_PASSIVE_ZOOM  0.5
#define XMB_ITEM_ACTIVE_ZOOM         1.0
#define XMB_ITEM_PASSIVE_ZOOM        0.5

#define XMB_CATEGORIES_ACTIVE_ALPHA  1.0
#define XMB_CATEGORIES_PASSIVE_ALPHA 0.5
#define XMB_ITEM_ACTIVE_ALPHA        1.0
#define XMB_ITEM_PASSIVE_ALPHA       0.5

typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   uintptr_t icon;
   uintptr_t content_icon;
} xmb_node_t;

enum
{
   XMB_TEXTURE_MAIN_MENU = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_HISTORY,
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_CLOSE,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_CORE_INFO,
   XMB_TEXTURE_CORE_OPTIONS,
   XMB_TEXTURE_INPUT_REMAPPING_OPTIONS,
   XMB_TEXTURE_CHEAT_OPTIONS,
   XMB_TEXTURE_DISK_OPTIONS,
   XMB_TEXTURE_SHADER_OPTIONS,
   XMB_TEXTURE_ACHIEVEMENT_LIST,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
   XMB_TEXTURE_MUSIC,
   XMB_TEXTURE_IMAGE,
   XMB_TEXTURE_MOVIE,
   XMB_TEXTURE_CORE,
   XMB_TEXTURE_RDB,
   XMB_TEXTURE_CURSOR,
   XMB_TEXTURE_SWITCH_ON,
   XMB_TEXTURE_SWITCH_OFF,
   XMB_TEXTURE_CLOCK,
   XMB_TEXTURE_POINTER,
   XMB_TEXTURE_ADD,
   XMB_TEXTURE_LAST
};

enum
{
   XMB_SYSTEM_TAB_MAIN = 0,
   XMB_SYSTEM_TAB_SETTINGS,
   XMB_SYSTEM_TAB_HISTORY,
   XMB_SYSTEM_TAB_ADD
};

#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_ADD

struct xmb_texture_item
{
   uintptr_t id;
};

typedef struct xmb_handle
{
   file_list_t *menu_stack_old;
   file_list_t *selection_buf_old;
   file_list_t *horizontal_list;
   size_t selection_ptr_old;
   int depth;
   int old_depth;
   char box_message[PATH_MAX_LENGTH];
   float x;
   float alpha;
   uintptr_t boxart;
   float boxart_width;
   float boxart_height;
   char background_file_path[PATH_MAX_LENGTH];
   char boxart_file_path[PATH_MAX_LENGTH];

   struct
   {
      struct
      {
         float left;
         float top;

      } screen;

      struct
      {
         float left;
      } setting;

      struct
      {
         float left;
         float top;
         float bottom;
      } title;

      struct
      {
         float left;
         float top;
      } label;
   } margins;

   char title_name[256];

   struct 
   {
      struct
      {
         float alpha;
      } arrow;

      struct xmb_texture_item bg;
      struct xmb_texture_item list[XMB_TEXTURE_LAST];
   } textures;

   struct
   {
      struct
      {
         float horizontal;
         float vertical;
      } spacing;

      char dir[4];
      int size;
   } icon;

   struct
   {
      int size;
   } cursor;

   struct
   {
      struct
      {
         unsigned idx;
         unsigned idx_old;
      } active;

      float x_pos;
      size_t selection_ptr_old;
      size_t selection_ptr;
   } categories;

   xmb_node_t main_menu_node;
   xmb_node_t settings_tab_node;
   xmb_node_t history_tab_node;
   xmb_node_t add_tab_node;

   gfx_font_raster_block_t raster_block;
} xmb_handle_t;

static void xmb_fill_default_background_path(xmb_handle_t *xmb, char *path, size_t size)
{
    char mediapath[PATH_MAX_LENGTH] = {0};
    char themepath[PATH_MAX_LENGTH] = {0};
    char iconpath[PATH_MAX_LENGTH]  = {0};
    settings_t *settings = config_get_ptr();
    
    strlcpy(xmb->icon.dir, "png", sizeof(xmb->icon.dir));
    
    fill_pathname_join(mediapath, settings->assets_directory,
                       "xmb", sizeof(mediapath));
    fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
    fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
    fill_pathname_slash(iconpath, sizeof(iconpath));
    
    fill_pathname_join(path, iconpath, "bg.png", size);
    
    if (*settings->menu.wallpaper)
        strlcpy(path, settings->menu.wallpaper, size);
}

static size_t xmb_list_get_selection(void *data)
{
   menu_handle_t *menu    = (menu_handle_t*)data;
   xmb_handle_t *xmb      = menu ? (xmb_handle_t*)menu->userdata : NULL;

   if (!xmb)
      return 0;

   return xmb->categories.selection_ptr;
}

static size_t xmb_list_get_size(void *data, menu_list_type_t type)
{
   size_t list_size        = 0;
   xmb_handle_t *xmb       = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         list_size  = menu_entries_get_stack_size(0);
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         break;
      case MENU_LIST_TABS:
         list_size = XMB_SYSTEM_TAB_END;
   }

   return list_size;
}

static void *xmb_list_get_entry(void *data, menu_list_type_t type, unsigned i)
{
   void *ptr               = NULL;
   size_t list_size        = 0;
   menu_handle_t *menu     = (menu_handle_t*)data;
   xmb_handle_t *xmb       = menu ? (xmb_handle_t*)menu->userdata : NULL;
   file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);

   switch (type)
   {
      case MENU_LIST_PLAIN:
         list_size  = menu_entries_get_stack_size(0);
         if (i < list_size)
            ptr = (void*)&menu_stack->list[i];
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         if (i < list_size)
            ptr = (void*)&xmb->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return ptr;
}

static float xmb_item_y(xmb_handle_t *xmb, int i, size_t current)
{
   float iy = xmb->icon.spacing.vertical;

   if (i < (int)current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + XMB_ABOVE_OFFSET_SUBITEM);
      else
         iy *= (i - (int)current + XMB_ABOVE_OFFSET_ITEM);
   else
      iy    *= (i - (int)current + XMB_UNDER_OFFSET_ITEM);

   if (i == (int)current)
      iy = xmb->icon.spacing.vertical * XMB_ITEM_ACTIVE_FACTOR;

   return iy;
}

static void xmb_draw_icon(xmb_handle_t *xmb,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   struct gfx_coords coords;
   math_matrix_4x4 mymat;

   if (
         x < -xmb->icon.size/2 || 
         x > width ||
         y < xmb->icon.size/2 ||
         y > height + xmb->icon.size)
      return;

   menu_display_matrix_4x4_rotate_z(&mymat, rotation, scale_factor, scale_factor, 1, true);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_draw(
         x,
         height - y,
         xmb->icon.size,
         xmb->icon.size,
         &coords, &mymat, texture,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);
}

static void xmb_draw_icon_predone(xmb_handle_t *xmb,
      math_matrix_4x4 *mymat,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float alpha, float rotation, float scale_factor,
      float *color)
{
   struct gfx_coords coords;

   if (
         x < -xmb->icon.size/2 || 
         x > width ||
         y < xmb->icon.size/2 ||
         y > height + xmb->icon.size)
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   menu_display_draw(
         x,
         height - y,
         xmb->icon.size,
         xmb->icon.size,
         &coords, mymat, texture,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);
}

static void xmb_draw_boxart(xmb_handle_t *xmb, float *color, unsigned width, unsigned height)
{
   struct gfx_coords coords;
   math_matrix_4x4 mymat;
   float y = xmb->margins.screen.top + xmb->icon.size + xmb->boxart_height;
   float x = xmb->margins.screen.left + xmb->icon.spacing.horizontal +
      xmb->icon.spacing.horizontal*4 - xmb->icon.size / 4;

   menu_display_matrix_4x4_rotate_z(&mymat, 0, 1, 1, 1, true);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_draw(
         x,
         height - y,
         xmb->boxart_width,
         xmb->boxart_height,
         &coords, &mymat, xmb->boxart,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);
}

static void xmb_draw_text(xmb_handle_t *xmb,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align,
      unsigned width, unsigned height)
{
   struct font_params params;
   uint8_t a8                =   0;
   void *disp_buf            = NULL;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8 = 255 * alpha;

   if (a8 == 0)
      return;

   if (x < -xmb->icon.size || x > width + xmb->icon.size
         || y < -xmb->icon.size || y > height + xmb->icon.size)
      return;

   params.x           = x        / width;
   params.y           = 1.0f - y / height;
   params.scale       = scale_factor;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;
   params.text_align  = text_align;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &disp_buf);

   video_driver_set_osd_msg(str, &params, disp_buf);
}

static void xmb_messagebox(void *data, const char *message)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_messagebox_internal(xmb_handle_t *xmb, const char *message)
{
   int x, y, font_size;
   unsigned i;
   unsigned width, height;
   struct string_list *list = NULL;

   if (!xmb)
      return;

   video_driver_get_size(&width, &height);

   list = string_split(message, "\n");
   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   x = width  / 2 - strlen(list->elems[0].data) * font_size / 4;
   y = height / 2 - list->size * font_size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(
               xmb, msg,
               x,
               y + i * font_size,
               1,
               1,
               TEXT_ALIGN_LEFT,
               width,
               height);
   }

end:
   string_list_free(list);
}

static void xmb_update_boxart_path(xmb_handle_t *xmb, unsigned i)
{
   menu_entry_t entry;
   settings_t *settings       = config_get_ptr();

   menu_entry_get(&entry, 0, i, NULL, true);

   fill_pathname_join(xmb->boxart_file_path, settings->boxarts_directory,
         xmb->title_name, sizeof(xmb->boxart_file_path));
   fill_pathname_join(xmb->boxart_file_path, xmb->boxart_file_path,
         "Named_Snaps", sizeof(xmb->boxart_file_path));
   fill_pathname_join(xmb->boxart_file_path, xmb->boxart_file_path,
         entry.path, sizeof(xmb->boxart_file_path));

   strlcat(xmb->boxart_file_path, ".png", sizeof(xmb->boxart_file_path));
}

static void xmb_update_boxart_image(xmb_handle_t *xmb)
{
   if (path_file_exists(xmb->boxart_file_path))
      rarch_task_push_image_load(xmb->boxart_file_path, "cb_menu_boxart",
            menu_display_handle_boxart_upload, NULL);
   else if (xmb->depth == 1)
      xmb->boxart = 0;
}

static void xmb_selection_pointer_changed(xmb_handle_t *xmb, bool allow_animations)
{
   size_t selection;
   unsigned i, end, tag, height, skip, depth;
   int threshold = 0;
   menu_list_t     *menu_list = menu_list_get_ptr();
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   settings_t       *settings = config_get_ptr();

   if (!xmb)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   end       = menu_entries_get_end();
   tag       = (uintptr_t)menu_list;
   threshold = xmb->icon.size*10;

   video_driver_get_size(NULL, &height);

   menu_animation_kill_by_tag(tag);
   menu_entries_set_start(0);
   skip = 0;

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia = XMB_ITEM_PASSIVE_ALPHA;
      float iz = XMB_ITEM_PASSIVE_ZOOM;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      iy      = xmb_item_y(xmb, i, selection);
      real_iy = iy + xmb->margins.screen.top;

      if (i == selection)
      {
         ia = XMB_ITEM_ACTIVE_ALPHA;
         iz = XMB_ITEM_ACTIVE_ZOOM;

         depth = xmb_list_get_size(xmb, MENU_LIST_PLAIN);
         if (settings->menu.boxart_enable && depth == 1)
         {
            xmb_update_boxart_path(xmb, i);
            xmb_update_boxart_image(xmb);
         }
      }

      if (real_iy < -threshold)
         skip++;

      if (!allow_animations || (real_iy < -threshold || real_iy > height+threshold))
      {
         node->alpha = node->label_alpha = ia;
         node->y = iy;
         node->zoom = iz;
      }
      else
      {
         menu_animation_push(XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(XMB_DELAY, ia, &node->label_alpha, EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(XMB_DELAY, iz, &node->zoom,  EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(XMB_DELAY, iy, &node->y,     EASING_IN_OUT_QUAD, tag, NULL);
      }
   }

   menu_entries_set_start(skip);
}

static void xmb_list_open_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height = 0;
   int        threshold = xmb->icon.size * 10;
   size_t           end = 0;

   end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = XMB_ITEM_ACTIVE_ALPHA;
      if (dir == -1)
         ia = 0;

      real_y = node->y + xmb->margins.screen.top;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = ia;
         node->label_alpha = 0;
         node->x = xmb->icon.size * dir * -2;
      }
      else
      {
         menu_animation_push(
               XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, 0, &node->label_alpha, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, xmb->icon.size * dir * -2, &node->x,
               EASING_IN_OUT_QUAD, -1, NULL);
      }
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   int        threshold = xmb->icon.size * 10;
   size_t           end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (dir == 1 || (dir == -1 && i != current))
         node->alpha = 0;

      if (dir == 1 || dir == -1)
         node->label_alpha = 0;

      node->x = xmb->icon.size * dir * 2;
      node->y = xmb_item_y(xmb, i, current);
      node->zoom = XMB_CATEGORIES_PASSIVE_ZOOM;

      real_y = node->y + xmb->margins.screen.top;

      if (i == current)
         node->zoom = XMB_CATEGORIES_ACTIVE_ZOOM;

      ia    = XMB_ITEM_PASSIVE_ALPHA;
      if (i == current)
         ia = XMB_ITEM_ACTIVE_ALPHA;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x = 0;
      }
      else
      {
         menu_animation_push(
               XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, 0, &node->x, EASING_IN_OUT_QUAD, -1, NULL);
      }
   }

   xmb->old_depth = xmb->depth;
   menu_entries_set_start(0);
}

static xmb_node_t *xmb_node_allocate_userdata(xmb_handle_t *xmb, unsigned i)
{
   xmb_node_t *node = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node->alpha = XMB_CATEGORIES_PASSIVE_ALPHA;
   node->zoom  = XMB_CATEGORIES_PASSIVE_ZOOM;

   if ((i + XMB_SYSTEM_TAB_END) == xmb->categories.active.idx)
   {
      node->alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
      node->zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;
   }

   file_list_free_actiondata(xmb->horizontal_list, i);
   file_list_set_actiondata(xmb->horizontal_list, i, node);

   return node;
}

static xmb_node_t* xmb_get_userdata_from_horizontal_list(
      xmb_handle_t *xmb, unsigned i)
{
   return (xmb_node_t*)menu_entries_get_actiondata_at_offset(xmb->horizontal_list, i);
}

static void xmb_push_animations(xmb_node_t *node, float ia, float ix)
{
   menu_animation_push(
         XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, -1, NULL);
   menu_animation_push(
         XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, -1, NULL);
   menu_animation_push(
         XMB_DELAY, ix, &node->x, EASING_IN_OUT_QUAD, -1, NULL);
}

static void xmb_list_switch_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i;
   size_t end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);
      float ia         = 0;

      if (!node)
         continue;

      xmb_push_animations(node, ia, -xmb->icon.spacing.horizontal * dir);
   }
}

static void xmb_list_switch_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i;
   size_t end           = 0;
   settings_t *settings = config_get_ptr();

   if (settings->menu.dynamic_wallpaper_enable)
   {
      char path[PATH_MAX_LENGTH] = {0};
      char *tmp = string_replace_substring(xmb->title_name, "/", " ");

      if (tmp)
      {
         fill_pathname_join(path, settings->dynamic_wallpapers_directory, tmp, sizeof(path));
         path_remove_extension(path);
         free(tmp);
      }

      strlcat(path, ".png", sizeof(path));
     
      if (!path_file_exists(path))
          xmb_fill_default_background_path(xmb, path, sizeof(path));
       
       if(strcmp(path, xmb->background_file_path) != 0)
       {
           if(path_file_exists(path))
           {
              rarch_task_push_image_load(path, "cb_menu_wallpaper",
                  menu_display_handle_wallpaper_upload, NULL);
              strlcpy(xmb->background_file_path, path, sizeof(xmb->background_file_path));
           }
       }
   }

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);
      float ia         = 0.5;

      if (!node)
         continue;

      node->x           = xmb->icon.spacing.horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = XMB_ITEM_ACTIVE_ALPHA;

      xmb_push_animations(node, ia, 0);
   }
}

static void xmb_set_title(xmb_handle_t *xmb)
{
   if (xmb->categories.selection_ptr <= XMB_SYSTEM_TAB_END)
   {
      menu_entries_get_title(xmb->title_name, sizeof(xmb->title_name));
   }
   else
   {
      const char *path = NULL;
      menu_entries_get_at_offset(
            xmb->horizontal_list,
            xmb->categories.selection_ptr - (XMB_SYSTEM_TAB_END + 1),
            &path, NULL, NULL, NULL, NULL);

      if (!path)
         return;

      strlcpy(xmb->title_name, path, sizeof(xmb->title_name));

      path_remove_extension(xmb->title_name);
   }
}

static xmb_node_t* xmb_get_node(xmb_handle_t *xmb, unsigned i)
{
   xmb_node_t *node = NULL;
   
   switch (i)
   {
      case XMB_SYSTEM_TAB_SETTINGS:
         node = &xmb->settings_tab_node;
         break;
      case XMB_SYSTEM_TAB_HISTORY:
         node = &xmb->history_tab_node;
         break;
      case XMB_SYSTEM_TAB_ADD:
         node = &xmb->add_tab_node;
         break;
      default:
         node = &xmb->main_menu_node;
         if (i > XMB_SYSTEM_TAB_END)
            node = xmb_get_userdata_from_horizontal_list(xmb, i - (XMB_SYSTEM_TAB_END + 1));
         break;
   }

   return node;
}

static void xmb_list_switch_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL) + XMB_SYSTEM_TAB_END;

   for (j = 0; j <= list_size; j++)
   {
      float ia                    = XMB_CATEGORIES_PASSIVE_ALPHA;
      float iz                    = XMB_CATEGORIES_PASSIVE_ZOOM;
      xmb_node_t *node            = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
      {
         ia = XMB_CATEGORIES_ACTIVE_ALPHA;
         iz = XMB_CATEGORIES_ACTIVE_ZOOM;
      }

      menu_animation_push(
            XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
      menu_animation_push(
            XMB_DELAY, iz, &node->zoom, EASING_IN_OUT_QUAD, -1, NULL);
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   size_t selection;
   int dir = -1;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   settings_t     *settings   = config_get_ptr();

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb->categories.active.idx += dir;

   xmb_list_switch_horizontal_list(xmb);

   menu_animation_push(XMB_DELAY,
         xmb->icon.spacing.horizontal * -(float)xmb->categories.selection_ptr,
         &xmb->categories.x_pos, EASING_IN_OUT_QUAD, -1, NULL);

   dir = -1;
   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_switch_new(xmb, selection_buf, dir, selection);
   xmb->categories.active.idx_old = xmb->categories.selection_ptr;

   if (settings->menu.boxart_enable)
   {
      xmb_update_boxart_path(xmb, 0);
      xmb_update_boxart_image(xmb);
   }
}

static void xmb_list_open_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL) + XMB_SYSTEM_TAB_END;

   for (j = 0; j <= list_size; j++)
   {
      float ia          = 0;
      xmb_node_t *node  = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
         ia = XMB_CATEGORIES_ACTIVE_ALPHA;
      else if (xmb->depth <= 1)
         ia = XMB_CATEGORIES_PASSIVE_ALPHA;

      menu_animation_push(XMB_DELAY, ia,
            &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
   }
}

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      xmb_node_t *node = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         continue;

      menu_display_texture_unload((uintptr_t*)&node->icon);
      menu_display_texture_unload((uintptr_t*)&node->content_icon);
   }
}

static void xmb_init_horizontal_list(xmb_handle_t *xmb)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();

   xmb->horizontal_list     = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!xmb->horizontal_list)
      return;

   info.list         = xmb->horizontal_list;
   info.menu_list    = NULL;
   info.type         = 0;
   info.type_default = MENU_FILE_PLAIN;
   info.flags        = SL_FLAG_ALLOW_EMPTY_LIST;
   strlcpy(info.label,
         menu_hash_to_str(MENU_LABEL_CONTENT_COLLECTION_LIST), sizeof(info.label));
   strlcpy(info.path, settings->playlist_directory, sizeof(info.path));
   strlcpy(info.exts, "lpl", sizeof(info.exts));

   if (menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL) == 0)
      menu_displaylist_push_list_process(&info);
}

static void xmb_toggle_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL) + XMB_SYSTEM_TAB_END;

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

      if (!node)
         continue;

      node->alpha = 0;
      node->zoom  = XMB_CATEGORIES_PASSIVE_ZOOM;

      if (i == xmb->categories.active.idx)
      {
         node->alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
         node->zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;
      }
      else if (xmb->depth <= 1)
         node->alpha = XMB_CATEGORIES_PASSIVE_ALPHA;
   }
}

static void xmb_context_reset_horizontal_list(xmb_handle_t *xmb, const char *themepath)
{
   unsigned i;
   int depth; /* keep this integer */
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   xmb->categories.x_pos = xmb->icon.spacing.horizontal *
      -(float)xmb->categories.selection_ptr;

   depth = (xmb->depth > 1) ? 2 : 1;
   xmb->x = xmb->icon.size * -(depth*2-2);

   for (i = 0; i < list_size; i++)
   {
      char iconpath[PATH_MAX_LENGTH]            = {0};
      char sysname[PATH_MAX_LENGTH]             = {0};
      char texturepath[PATH_MAX_LENGTH]         = {0};
      char content_texturepath[PATH_MAX_LENGTH] = {0};
      struct texture_image ti                   = {0};
      const char *path                          = NULL;
      xmb_node_t *node                          = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
      {
         node = xmb_node_allocate_userdata(xmb, i);
         if (!node)
            continue;
      }

      file_list_get_at_offset(xmb->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path)
         continue;

      strlcpy(sysname, path, sizeof(sysname));
      path_remove_extension(sysname);

      fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      fill_pathname_join(texturepath, iconpath, sysname, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      fill_pathname_join(content_texturepath, iconpath, sysname, sizeof(content_texturepath));
      strlcat(content_texturepath, "-content.png", sizeof(content_texturepath));

      texture_image_load(&ti, texturepath);

      node->icon         = menu_display_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
      texture_image_load(&ti, content_texturepath);

      node->content_icon = menu_display_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }

   xmb_toggle_horizontal_list(xmb);
}

static void xmb_refresh_horizontal_list(xmb_handle_t *xmb)
{
   char mediapath[PATH_MAX_LENGTH] = {0};
   char themepath[PATH_MAX_LENGTH] = {0};

   settings_t *settings = config_get_ptr();

   fill_pathname_join(mediapath, settings->assets_directory, "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));

   xmb_context_destroy_horizontal_list(xmb);
   if (xmb->horizontal_list)
      free(xmb->horizontal_list);
   xmb->horizontal_list = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   xmb_init_horizontal_list(xmb);
   xmb_context_reset_horizontal_list(xmb, themepath);
}

static int xmb_environ(menu_environ_cb_t type, void *data, void *userdata)
{
   switch (type)
   {
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         {
            xmb_handle_t *xmb        = (xmb_handle_t*)userdata;

            if (!xmb)
               return -1;

            xmb_refresh_horizontal_list(xmb);
         }
         break;
      default:
         return -1;
   }

   return 0;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   size_t selection;
   int                    dir = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   xmb->depth = xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;

   xmb_list_open_horizontal_list(xmb);

   xmb_list_open_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_open_new(xmb, selection_buf,
         dir, selection);

   switch (xmb->depth)
   {
      case 1:
         menu_animation_push(
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, 0, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, -1, NULL);
         break;
      case 2:
         menu_animation_push(
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(
               XMB_DELAY, 1, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, -1, NULL);
         break;
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   settings_t *settings = config_get_ptr();
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb)
      return;

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      xmb_selection_pointer_changed(xmb, false);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      if (settings->menu.boxart_enable)
         xmb_update_boxart_image(xmb);
      return;
   }

   xmb_set_title(xmb);

   if (xmb->categories.selection_ptr != xmb->categories.active.idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);
}

static uintptr_t xmb_icon_get_id(xmb_handle_t *xmb,
      xmb_node_t *core_node, xmb_node_t *node, unsigned type, bool active)
{
   switch(type)
   {
      case MENU_FILE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER].id;
      case MENU_FILE_PLAIN:
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_RPL_ENTRY:
         if (core_node)
            return core_node->content_icon;
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_ZIP].id;
      case MENU_FILE_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC].id;
      case MENU_FILE_IMAGEVIEWER:
         return xmb->textures.list[XMB_TEXTURE_IMAGE].id;
      case MENU_FILE_MOVIE:
         return xmb->textures.list[XMB_TEXTURE_MOVIE].id;
      case MENU_FILE_CORE:
         return xmb->textures.list[XMB_TEXTURE_CORE].id;
      case MENU_FILE_RDB:
         return xmb->textures.list[XMB_TEXTURE_RDB].id;
      case MENU_FILE_CURSOR:
         return xmb->textures.list[XMB_TEXTURE_CURSOR].id;
      case MENU_FILE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN].id;
      case MENU_SETTING_ACTION_CLOSE:
         return xmb->textures.list[XMB_TEXTURE_CLOSE].id;
      case MENU_SETTING_ACTION_SAVESTATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
      case MENU_SETTING_ACTION_LOADSTATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
      case MENU_FILE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].id;
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].id;
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].id;
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].id;
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS].id;
      case MENU_SETTING_ACTION_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT].id;
      case MENU_SETTING_ACTION_RESET:
         return xmb->textures.list[XMB_TEXTURE_RELOAD].id;
      case MENU_SETTING_ACTION:
         if (xmb->depth == 3)
            return xmb->textures.list[XMB_TEXTURE_SUBSETTING].id;
         return xmb->textures.list[XMB_TEXTURE_SETTING].id;
      case MENU_SETTING_GROUP:
         return xmb->textures.list[XMB_TEXTURE_SETTING].id;
      case MENU_INFO_MESSAGE:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
   }

   return xmb->textures.list[XMB_TEXTURE_SUBSETTING].id;
}

static void xmb_draw_items(xmb_handle_t *xmb,
      file_list_t *list, file_list_t *stack,
      size_t current, size_t cat_selection_ptr, float *color,
      unsigned width, unsigned height)
{
   uint64_t *frame_count;
   unsigned i, ticker_limit;
   math_matrix_4x4 mymat;
   xmb_node_t *core_node       = NULL;
   size_t end                  = 0;
   settings_t   *settings      = config_get_ptr();

   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   if (!list || !list->size)
      return;

   if (cat_selection_ptr > XMB_SYSTEM_TAB_END)
      core_node = xmb_get_userdata_from_horizontal_list(xmb, cat_selection_ptr - (XMB_SYSTEM_TAB_END + 1));

   end = file_list_get_size(list);

   menu_display_matrix_4x4_rotate_z(&mymat, 0, 1, 1, 1, true);

   i = menu_entries_get_start();

   if (list == xmb->selection_buf_old)
      i = 0;

   for (; i < end; i++)
   {
      menu_entry_t entry;
      char name[PATH_MAX_LENGTH];
      char value[PATH_MAX_LENGTH];
      float icon_x, icon_y;

      const float half_size       = xmb->icon.size / 2.0f;
      uintptr_t texture_switch       = 0;
      uintptr_t         icon         = 0;
      xmb_node_t *   node         = (xmb_node_t*)menu_entries_get_userdata_at_offset(list, i);
      uint32_t hash_label         = 0;
      uint32_t hash_value         = 0;
      bool do_draw_text           = false;

      if (!node)
         continue;

      icon_y = xmb->margins.screen.top + node->y + half_size;

      if (icon_y < half_size)
         continue;

      if (icon_y > height + xmb->icon.size)
         break;

      icon_x = node->x + xmb->margins.screen.left +
         xmb->icon.spacing.horizontal - half_size;

      if (icon_x < -half_size || icon_x > width)
         continue;

      menu_entry_get(&entry, 0, i, list, true);

      hash_label = menu_hash_calculate(entry.label);
      hash_value = menu_hash_calculate(entry.value);

      if (entry.type == MENU_FILE_CONTENTLIST_ENTRY)
         fill_short_pathname_representation(entry.path, entry.path,
               sizeof(entry.path));

      icon = xmb_icon_get_id(xmb, core_node, node, entry.type, (i == current));

      switch (hash_label)
      {
         case MENU_LABEL_CORE_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].id;
            break;
         case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].id;
            break;
         case MENU_LABEL_CORE_CHEAT_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].id;
            break;
         case MENU_LABEL_DISK_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].id;
            break;
         case MENU_LABEL_SHADER_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS].id;
            break;
         case MENU_LABEL_ACHIEVEMENT_LIST:
            icon = xmb->textures.list[XMB_TEXTURE_ACHIEVEMENT_LIST].id;
            break;
         case MENU_LABEL_SAVESTATE:
            icon = xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
            break;
         case MENU_LABEL_LOADSTATE:
            icon = xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
            break;
         case MENU_LABEL_TAKE_SCREENSHOT:
            icon = xmb->textures.list[XMB_TEXTURE_SCREENSHOT].id;
            break;
         case MENU_LABEL_RESTART_CONTENT:
            icon = xmb->textures.list[XMB_TEXTURE_RELOAD].id;
            break;
         case MENU_LABEL_RESUME_CONTENT:
            icon = xmb->textures.list[XMB_TEXTURE_RESUME].id;
            break;
      }

      if (!strcmp(entry.value, "disabled") ||
            !strcmp(entry.value, "off"))
      {
         if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)
            texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id;
         else
            do_draw_text = true;
      }
      else if (!strcmp(entry.value, "enabled") ||
            !strcmp(entry.value, "on"))
      {
         if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
            texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id;
         else
            do_draw_text = true;
      }
      else
      {
         switch (hash_value)
         {
            case MENU_VALUE_COMP:
               break;
            case MENU_VALUE_MORE:
               break;
            case MENU_VALUE_CORE:
               break;
            case MENU_VALUE_RDB:
               break;
            case MENU_VALUE_CURSOR:
               break;
            case MENU_VALUE_FILE:
               break;
            case MENU_VALUE_DIR:
               break;
            case MENU_VALUE_MUSIC:
               break;
            case MENU_VALUE_IMAGE:
               break;
            case MENU_VALUE_MOVIE:
               break;
            case MENU_VALUE_ON:
               if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
                  texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id;
               else
                  do_draw_text = true;
               break;
            case MENU_VALUE_OFF:
               if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)
                  texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id;
               else
                  do_draw_text = true;
               break;
            default:
               do_draw_text = true;
               break;
         }
      }

      ticker_limit = 35;
      if (entry.value[0] == '\0')
      {
         if (settings->menu.boxart_enable && xmb->boxart)
            ticker_limit = 40;
         else
            ticker_limit = 70;
      }

      menu_animation_ticker_str(name, ticker_limit,
            *frame_count / 20, entry.path,
            (i == current));

      xmb_draw_text(xmb, name,
            node->x + xmb->margins.screen.left + 
            xmb->icon.spacing.horizontal + xmb->margins.label.left, 
            xmb->margins.screen.top + node->y + xmb->margins.label.top, 
            1, node->label_alpha, TEXT_ALIGN_LEFT,
            width, height);

      menu_animation_ticker_str(value, 35,
            *frame_count / 20, entry.value,
            (i == current));


      if (do_draw_text)
         xmb_draw_text(xmb, value,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal + 
               xmb->margins.label.left + xmb->margins.setting.left, 
               xmb->margins.screen.top + node->y + xmb->margins.label.top, 
               1, 
               node->label_alpha,
               TEXT_ALIGN_LEFT,
               width, height);

      menu_display_blend_begin();

      /* set alpha components of color */
      color[3] = color[7] = color[11] = color[15] = (node->alpha > xmb->alpha) ? xmb->alpha : node->alpha;

      if (color[3] != 0)
         xmb_draw_icon(xmb, icon, icon_x, icon_y, width, height, 
               0, node->zoom, &color[0]);

      /* set alpha components of color */
      color[3]  = color[7]  = color[11]  = color[15]  = (node->alpha > xmb->alpha) ? xmb->alpha : node->alpha;

      if (texture_switch != 0 && color[3] != 0)
         xmb_draw_icon_predone(xmb, &mymat,
               texture_switch,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal
               + xmb->icon.size / 2.0 + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->icon.size / 2.0,
               width, height,
               node->alpha,
               0,
               1, &color[0]);

      menu_display_blend_end();
   }
}

static void xmb_draw_cursor(xmb_handle_t *xmb,
      float *color,
      float x, float y, unsigned width, unsigned height)
{
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_blend_begin();

   menu_display_draw(
         x - (xmb->cursor.size/2),
         height - y - (xmb->cursor.size/2),
         xmb->cursor.size,
         xmb->cursor.size,
         &coords, NULL, xmb->textures.list[XMB_TEXTURE_POINTER].id,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   menu_display_blend_end();
}

static void xmb_render(void *data)
{
   float delta_time, dt;
   size_t selection;
   unsigned i, end, height  = 0;
   settings_t   *settings   = config_get_ptr();
   xmb_handle_t *xmb        = (xmb_handle_t*)data;

   if (!xmb)
      return;

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);
   dt = delta_time / IDEAL_DT;
   menu_animation_ctl(MENU_ANIMATION_CTL_UPDATE, &dt);

   video_driver_get_size(NULL, &height);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   end     = menu_entries_get_size();

   if (settings->menu.pointer.enable || settings->menu.mouse.enable)
   {
      for (i = 0; i < end; i++)
      {
         float item_y1     = xmb->margins.screen.top + xmb_item_y(xmb, i, selection);
         float item_y2     = item_y1 + xmb->icon.size;
         int16_t pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
         int16_t mouse_y   = menu_input_mouse_state(MENU_MOUSE_Y_AXIS) + (xmb->cursor.size/2);

         if (settings->menu.pointer.enable)
         {
            if (pointer_y > item_y1 && pointer_y < item_y2)
               menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &i);
         }

         if (settings->menu.mouse.enable)
         {
            if (mouse_y > item_y1 && mouse_y < item_y2)
               menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &i);
         }
      }
   }

   if (menu_entries_get_start() >= end)
      menu_entries_set_start(0);

   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);
}

static void xmb_frame_horizontal_list(xmb_handle_t *xmb,
      unsigned width, unsigned height,
      float *color)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL) + XMB_SYSTEM_TAB_END;

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

      if (!node)
         continue;

      menu_display_blend_begin();

      /* set alpha components of color */
      color[3] = color[7] = color[11] = color[15] = (node->alpha > xmb->alpha) ? xmb->alpha : node->alpha;

      if (color[3] != 0)
         xmb_draw_icon(xmb, node->icon, 
               xmb->x + xmb->categories.x_pos + 
               xmb->margins.screen.left + 
               xmb->icon.spacing.horizontal * (i + 1) - xmb->icon.size / 2.0,
               xmb->margins.screen.top + xmb->icon.size / 2.0, 
               width, height,
               0, 
               node->zoom,
               &color[0]);

      menu_display_blend_end();
   }
}

static void xmb_frame(void *data)
{
   size_t selection;
   math_matrix_4x4 mymat;
   unsigned depth, i, width, height;
   char msg[256];
   char title_msg[256];
   char timedate[256];
   float item_color[16];
   float coord_color[16];
   float coord_color2[16];
   bool display_kb;
   bool render_background                  = false;
   xmb_handle_t *xmb                       = (xmb_handle_t*)data;
   settings_t   *settings                  = config_get_ptr();
   file_list_t *selection_buf              = menu_entries_get_selection_buf_ptr(0);
   file_list_t *menu_stack                 = menu_entries_get_menu_stack_ptr(0);

   if (!xmb)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   msg[0]       = '\0';
   title_msg[0] = '\0';
   timedate[0]  = '\0';

   video_driver_get_size(&width, &height);

   menu_display_font_bind_block(&xmb->raster_block);

   xmb->raster_block.carr.coords.vertices = 0;

   for (i = 0; i < 16; i++)
   {
      coord_color[i]  = 0;
      coord_color2[i] = 1.0f;
      item_color[i]   = 1.0f;
   }

   /* set alpha components of colors */
   coord_color[3]  = coord_color[7]  = coord_color[11]  = coord_color[15]  = (0.75f > xmb->alpha) ? xmb->alpha : 0.75f;
   coord_color2[3] = coord_color2[7] = coord_color2[11] = coord_color2[15] = xmb->alpha;

   menu_display_draw_bg(
         width, height, xmb->textures.bg.id, xmb->alpha, false, &coord_color[0],
         &coord_color2[0], NULL, NULL, 4,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   xmb_draw_text(xmb,
         xmb->title_name, xmb->margins.title.left,
         xmb->margins.title.top, 1, 1, TEXT_ALIGN_LEFT,
         width, height);

   if (settings->menu.timedate_enable)
   {
      menu_display_timedate(timedate, sizeof(timedate), 4);

      xmb_draw_text(xmb, timedate,
            width - xmb->margins.title.left - xmb->icon.size / 4, 
            xmb->margins.title.top, 1, 1, TEXT_ALIGN_RIGHT,
            width, height);
   }

   if (menu_entries_get_core_title(title_msg, sizeof(title_msg)) == 0)
      xmb_draw_text(xmb, title_msg, xmb->margins.title.left, 
            height - xmb->margins.title.bottom, 1, 1, TEXT_ALIGN_LEFT,
            width, height);

   depth = xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   xmb_draw_items(xmb,
         xmb->selection_buf_old,
         xmb->menu_stack_old,
         xmb->selection_ptr_old,
         depth > 1 ? xmb->categories.selection_ptr :
         xmb->categories.selection_ptr_old,
         &item_color[0], width, height);

   xmb_draw_items(xmb,
         selection_buf,
         menu_stack,
         selection,
         xmb->categories.selection_ptr,
         &item_color[0], width, height);

   menu_display_matrix_4x4_rotate_z(&mymat, 0, 1, 1, 1, true);

   menu_display_blend_begin();

   if (settings->menu.boxart_enable && xmb->boxart)
      xmb_draw_boxart(xmb, &coord_color2[0], width, height);

   /* set alpha components of colors */
   coord_color2[3]  = coord_color2[7]  = coord_color2[11]  = coord_color2[15]  = (1.00f > xmb->alpha) ? xmb->alpha : 1.00f;

   if (settings->menu.timedate_enable && coord_color2[3] != 0)
      xmb_draw_icon_predone(xmb, &mymat, xmb->textures.list[XMB_TEXTURE_CLOCK].id,
            width - xmb->icon.size, xmb->icon.size, width, height, 1, 0, 1, &coord_color2[0]);

   /* set alpha components of colors */
   coord_color2[3]  = coord_color2[7]  = coord_color2[11]  = coord_color2[15]  = (xmb->textures.arrow.alpha > xmb->alpha) 
      ? xmb->alpha : xmb->textures.arrow.alpha;

   if (coord_color2[3] != 0)
      xmb_draw_icon_predone(
            xmb,
            &mymat,
            xmb->textures.list[XMB_TEXTURE_ARROW].id,
            xmb->x + xmb->margins.screen.left + 
            xmb->icon.spacing.horizontal - xmb->icon.size / 2.0 + xmb->icon.size,
            xmb->margins.screen.top + 
            xmb->icon.size / 2.0 + xmb->icon.spacing.vertical 
            * XMB_ITEM_ACTIVE_FACTOR,
            width,
            height,
            xmb->textures.arrow.alpha,
            0,
            1, &coord_color2[0]);

   xmb_frame_horizontal_list(xmb, width, height, &item_color[0]);

   menu_display_font_flush_block();

   menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_DISPLAY, &display_kb);

   if (display_kb)
   {
      const char *str = NULL, *label = NULL;
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_BUFF_PTR, &str);
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL,    &label);

      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      render_background = true;
   }

   if (xmb->box_message[0] != '\0')
   {
      strlcpy(msg, xmb->box_message,
            sizeof(msg));
      xmb->box_message[0] = '\0';
      render_background = true;
   }

   if (render_background)
   {
      menu_display_draw_bg(
            width, height,
            xmb->textures.bg.id, xmb->alpha, true,
            &coord_color[0], &coord_color2[0],
            NULL, NULL, 4,
            MENU_DISPLAY_PRIM_TRIANGLESTRIP);
      xmb_render_messagebox_internal(xmb, msg);
   }

   /* set alpha components of colors */
   coord_color2[3]  = coord_color2[7]  = coord_color2[11]  = coord_color2[15]  = (1.00f > xmb->alpha) ? xmb->alpha : 1.00f;

   if (settings->menu.mouse.enable && (settings->video.fullscreen || !video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL)))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      xmb_draw_cursor(xmb, &coord_color2[0], mouse_x, mouse_y, width, height);
   }

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}


static void xmb_font(void)
{
   int font_size;
   char mediapath[PATH_MAX_LENGTH], themepath[PATH_MAX_LENGTH], fontpath[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   fill_pathname_join(mediapath, settings->assets_directory, "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(fontpath, themepath, "font.ttf", sizeof(fontpath));

   if (!menu_display_init_main_font(fontpath, font_size))
      RARCH_WARN("Failed to load font.");
}

static void xmb_layout(xmb_handle_t *xmb)
{
   int new_font_size;
   size_t selection;
   float scale_factor;
   unsigned width, height, i, current, end, new_header_height;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   video_driver_get_size(&width, &height);

   scale_factor                 = width / 1920.0;
   new_font_size                = 32.0  * scale_factor;
   new_header_height            = 128.0 * scale_factor;

   xmb->boxart_width            = 460.0 * scale_factor;
   xmb->cursor.size             = 64.0;

   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE,     &new_font_size);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT, &new_header_height);

   xmb->icon.spacing.horizontal = 200.0 * scale_factor;
   xmb->icon.spacing.vertical   = 64.0 * scale_factor;
   xmb->margins.screen.left     = 336.0 * scale_factor;
   xmb->margins.screen.top      = (256+32) * scale_factor;
   xmb->margins.title.left      = 60 * scale_factor;
   xmb->margins.title.top       = 60 * scale_factor + new_font_size / 3;
   xmb->margins.title.bottom    = 60 * scale_factor - new_font_size / 3;
   xmb->margins.label.left      = 85.0 * scale_factor;
   xmb->margins.label.top       = new_font_size / 3.0;
   xmb->margins.setting.left    = 600.0 * scale_factor;
   xmb->icon.size               = 128.0 * scale_factor;

   current = selection;
   end     = menu_entries_get_end();

   for (i = 0; i < end; i++)
   {
      float ia = XMB_ITEM_PASSIVE_ALPHA;
      float iz = XMB_ITEM_PASSIVE_ZOOM;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(
            selection_buf, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia = XMB_ITEM_ACTIVE_ALPHA;
         iz = XMB_ITEM_ACTIVE_ZOOM;
      }

      node->alpha       = ia;
      node->label_alpha = ia;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
   }

   if (xmb->depth <= 1)
      return;

   current = xmb->selection_ptr_old;
   end = file_list_get_size(xmb->selection_buf_old);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float iz = XMB_ITEM_PASSIVE_ZOOM;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(
            xmb->selection_buf_old, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia = XMB_ITEM_ACTIVE_ALPHA;
         iz = XMB_ITEM_ACTIVE_ZOOM;
      }

      node->alpha       = ia;
      node->label_alpha = 0;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
      node->x           = xmb->icon.size * 1 * -2;
   }
}

static void *xmb_init(void)
{
   unsigned width, height;
   xmb_handle_t *xmb          = NULL;
   menu_handle_t *menu        = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   if (!menu_display_driver_init_first())
      goto error;

   video_driver_get_size(&width, &height);

   menu->userdata             = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!menu->userdata)
      goto error;

   xmb = (xmb_handle_t*)menu->userdata;

   xmb->menu_stack_old        = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!xmb->menu_stack_old)
      goto error;

   xmb->selection_buf_old     = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!xmb->selection_buf_old)
      goto error;

   xmb->categories.active.idx   = 0;
   xmb->categories.active.idx_old   = 0;
   xmb->x                       = 0;
   xmb->categories.x_pos        = 0;
   xmb->textures.arrow.alpha    = 0;
   xmb->depth                   = 1;
   xmb->old_depth               = 1;
   xmb->alpha                   = 0;
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   menu_display_ctl(MENU_DISPLAY_CTL_SET_WIDTH,  &width);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEIGHT, &height);

   xmb_init_horizontal_list(xmb);
   xmb_font();

   return menu;

error:
   if (menu)
      free(menu);

   if (xmb)
   {
      if (xmb->menu_stack_old)
         free(xmb->menu_stack_old);
      xmb->menu_stack_old = NULL;
      if (xmb->selection_buf_old)
         free(xmb->selection_buf_old);
      xmb->selection_buf_old = NULL;
      if (xmb->horizontal_list)
         free(xmb->horizontal_list);
      xmb->horizontal_list = NULL;
   }
   return NULL;
}

static void xmb_free(void *data)
{
   xmb_handle_t *xmb                       = NULL;
   menu_handle_t *menu                     = (menu_handle_t*)data;

   if (menu && menu->userdata)
   {
      xmb = (xmb_handle_t*)menu->userdata;

      if (!xmb)
         return;

      if (xmb->menu_stack_old)
         file_list_free(xmb->menu_stack_old);
      xmb->menu_stack_old = NULL;

      if (xmb->selection_buf_old)
         file_list_free(xmb->selection_buf_old);
      xmb->selection_buf_old = NULL;
      if (xmb->horizontal_list)
         file_list_free(xmb->horizontal_list);
      xmb->horizontal_list = NULL;

      gfx_coord_array_free(&xmb->raster_block.carr);

      if (menu->userdata)
         free(menu->userdata);
      menu->userdata = NULL;
   }

   font_driver_bind_block(NULL, NULL);
}

static void xmb_context_bg_destroy(xmb_handle_t *xmb)
{
   if (!xmb)
      return;

   menu_display_texture_unload((uintptr_t*)&xmb->textures.bg.id);
}

static bool xmb_load_image(void *userdata, void *data, menu_image_type_t type)
{
   xmb_handle_t *xmb = (xmb_handle_t*)userdata;

   if (!xmb || !data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         xmb_context_bg_destroy(xmb);
         xmb->textures.bg.id   = menu_display_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR);
         break;
      case MENU_IMAGE_BOXART:
         {
            struct texture_image *img = (struct texture_image*)data;
            xmb->boxart_height = xmb->boxart_width * (float)img->height / (float)img->width;
            xmb->boxart = menu_display_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR);
         }
         break;
   }

   return true;
}

static void xmb_context_reset_textures(xmb_handle_t *xmb, const char *iconpath)
{
   unsigned i;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case XMB_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
            fill_pathname_join(path, iconpath, "lakka.png", sizeof(path));
#else
            fill_pathname_join(path, iconpath, "retroarch.png", sizeof(path));
#endif
            break;
         case XMB_TEXTURE_SETTINGS:
            fill_pathname_join(path, iconpath, "settings.png",   sizeof(path));
            break;
         case XMB_TEXTURE_HISTORY:
            fill_pathname_join(path, iconpath, "history.png",   sizeof(path));
            break;
         case XMB_TEXTURE_SETTING:
            fill_pathname_join(path, iconpath, "setting.png", sizeof(path));
            break;
         case XMB_TEXTURE_SUBSETTING:
            fill_pathname_join(path, iconpath, "subsetting.png", sizeof(path));
            break;
         case XMB_TEXTURE_ARROW:
            fill_pathname_join(path, iconpath, "arrow.png", sizeof(path));
            break;
         case XMB_TEXTURE_RUN:
            fill_pathname_join(path, iconpath, "run.png", sizeof(path));
            break;
         case XMB_TEXTURE_CLOSE:
            fill_pathname_join(path, iconpath, "close.png", sizeof(path));
            break;
         case XMB_TEXTURE_RESUME:
            fill_pathname_join(path, iconpath, "resume.png", sizeof(path));
            break;
         case XMB_TEXTURE_CLOCK:
            fill_pathname_join(path, iconpath, "clock.png",   sizeof(path));
            break;
         case XMB_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath, "pointer.png", sizeof(path));
            break;
         case XMB_TEXTURE_SAVESTATE:
            fill_pathname_join(path, iconpath, "savestate.png", sizeof(path));
            break;
         case XMB_TEXTURE_LOADSTATE:
            fill_pathname_join(path, iconpath, "loadstate.png", sizeof(path));
            break;
         case XMB_TEXTURE_CORE_INFO:
            fill_pathname_join(path, iconpath, "core-infos.png", sizeof(path));
            break;
         case XMB_TEXTURE_CORE_OPTIONS:
            fill_pathname_join(path, iconpath, "core-options.png", sizeof(path));
            break;
         case XMB_TEXTURE_INPUT_REMAPPING_OPTIONS:
            fill_pathname_join(path, iconpath, "core-input-remapping-options.png", sizeof(path));
            break;
         case XMB_TEXTURE_CHEAT_OPTIONS:
            fill_pathname_join(path, iconpath, "core-cheat-options.png", sizeof(path));
            break;
         case XMB_TEXTURE_DISK_OPTIONS:
            fill_pathname_join(path, iconpath, "core-disk-options.png", sizeof(path));
            break;
         case XMB_TEXTURE_SHADER_OPTIONS:
            fill_pathname_join(path, iconpath, "core-shader-options.png", sizeof(path));
            break;
         case XMB_TEXTURE_ACHIEVEMENT_LIST:
            fill_pathname_join(path, iconpath, "achievement-list.png", sizeof(path));
            break;
         case XMB_TEXTURE_SCREENSHOT:
            fill_pathname_join(path, iconpath, "screenshot.png", sizeof(path));
            break;
         case XMB_TEXTURE_RELOAD:
            fill_pathname_join(path, iconpath, "reload.png", sizeof(path));
            break;
         case XMB_TEXTURE_FILE:
            fill_pathname_join(path, iconpath, "file.png", sizeof(path));
            break;
         case XMB_TEXTURE_FOLDER:
            fill_pathname_join(path, iconpath, "folder.png", sizeof(path));
            break;
         case XMB_TEXTURE_ZIP:
            fill_pathname_join(path, iconpath, "zip.png", sizeof(path));
            break;
         case XMB_TEXTURE_MUSIC:
            fill_pathname_join(path, iconpath, "music.png", sizeof(path));
            break;
         case XMB_TEXTURE_IMAGE:
            fill_pathname_join(path, iconpath, "image.png", sizeof(path));
            break;
         case XMB_TEXTURE_MOVIE:
            fill_pathname_join(path, iconpath, "movie.png", sizeof(path));
            break;
         case XMB_TEXTURE_CORE:
            fill_pathname_join(path, iconpath, "core.png", sizeof(path));
            break;
         case XMB_TEXTURE_RDB:
            fill_pathname_join(path, iconpath, "database.png", sizeof(path));
            break;
         case XMB_TEXTURE_CURSOR:
            fill_pathname_join(path, iconpath, "cursor.png", sizeof(path));
            break;
         case XMB_TEXTURE_SWITCH_ON:
            fill_pathname_join(path, iconpath, "on.png", sizeof(path));
            break;
         case XMB_TEXTURE_SWITCH_OFF:
            fill_pathname_join(path, iconpath, "off.png", sizeof(path));
            break;
         case XMB_TEXTURE_ADD:
            fill_pathname_join(path, iconpath, "add.png", sizeof(path));
            break;
      }

      if (path[0] == '\0' || !path_file_exists(path))
         continue;

      texture_image_load(&ti, path);

      xmb->textures.list[i].id   = menu_display_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }

   xmb->main_menu_node.icon  = xmb->textures.list[XMB_TEXTURE_MAIN_MENU].id;
   xmb->main_menu_node.alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
   xmb->main_menu_node.zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;

   xmb->settings_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS].id;
   xmb->settings_tab_node.alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
   xmb->settings_tab_node.zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;

   xmb->history_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_HISTORY].id;
   xmb->history_tab_node.alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
   xmb->history_tab_node.zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;

   xmb->add_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_ADD].id;
   xmb->add_tab_node.alpha = XMB_CATEGORIES_ACTIVE_ALPHA;
   xmb->add_tab_node.zoom  = XMB_CATEGORIES_ACTIVE_ZOOM;
}

static void xmb_context_reset_background(const char *iconpath)
{
   char path[PATH_MAX_LENGTH]  = {0};
   settings_t *settings        = config_get_ptr();

   fill_pathname_join(path, iconpath, "bg.png", sizeof(path));

   if (*settings->menu.wallpaper)
      strlcpy(path, settings->menu.wallpaper, sizeof(path));

   if (path_file_exists(path))
      rarch_task_push_image_load(path, "cb_menu_wallpaper",
            menu_display_handle_wallpaper_upload, NULL);
}

static void xmb_context_reset(void)
{
   char mediapath[PATH_MAX_LENGTH] = {0};
   char themepath[PATH_MAX_LENGTH] = {0};
   char iconpath[PATH_MAX_LENGTH]  = {0};
   xmb_handle_t *xmb               = NULL;
   menu_handle_t *menu             = menu_driver_get_ptr();
   settings_t *settings            = config_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;
   if (!xmb)
      return;

   strlcpy(xmb->icon.dir, "png", sizeof(xmb->icon.dir));
   xmb_fill_default_background_path(xmb, xmb->background_file_path, sizeof(xmb->background_file_path));

   fill_pathname_join(mediapath, settings->assets_directory,
         "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   xmb_layout(xmb);
   xmb_font();
   xmb_context_reset_textures(xmb, iconpath);
   xmb_context_reset_background(iconpath);
   xmb_context_reset_horizontal_list(xmb, themepath);

   if (settings->menu.boxart_enable)
      xmb_update_boxart_image(xmb);
}

static void xmb_navigation_clear(bool pending_push)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   xmb_handle_t  *xmb  = menu ? (xmb_handle_t*)menu->userdata : NULL;
   if (!pending_push)
      xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_pointer_changed(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   xmb_handle_t  *xmb  = menu ? (xmb_handle_t*)menu->userdata : NULL;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_set(bool scroll)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   xmb_handle_t  *xmb  = menu ? (xmb_handle_t*)menu->userdata : NULL;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_alphabet(size_t *unused)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   xmb_handle_t  *xmb  = menu ? (xmb_handle_t*)menu->userdata : NULL;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_list_insert(file_list_t *list,
      const char *path, const char *unused, size_t list_size)
{
   size_t selection;
   int current            = 0;
   int i                  = list_size;
   xmb_node_t *node       = NULL;
   xmb_handle_t *xmb      = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();

   if (!menu)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!list || !xmb)
      return;

   node = (xmb_node_t*)menu_entries_get_userdata_at_offset(list, i);

   if (!node)
      node = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   current           = selection;
   node->alpha       = XMB_ITEM_PASSIVE_ALPHA;
   node->zoom        = XMB_ITEM_PASSIVE_ZOOM;
   node->label_alpha = node->alpha;
   node->y           = xmb_item_y(xmb, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = XMB_ITEM_ACTIVE_ALPHA;
      node->label_alpha = XMB_ITEM_ACTIVE_ALPHA;
      node->zoom        = XMB_ITEM_ACTIVE_ZOOM;
   }

   file_list_set_userdata(list, i, node);
}

static void xmb_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list->size;

   for (i = 0; i < size; ++i)
   {
      float *subjects[5];
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      subjects[0] = &node->alpha;
      subjects[1] = &node->label_alpha;
      subjects[2] = &node->zoom;
      subjects[3] = &node->x;
      subjects[4] = &node->y;

      menu_animation_kill_by_subject(5, subjects);

      file_list_free_userdata(list, i);
   }
}

static void xmb_list_deep_copy(const file_list_t *src, file_list_t *dst)
{
   size_t i;
   size_t size = dst->size;

   for (i = 0; i < size; ++i)
   {
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(dst, i);

      if (node)
      {
         float *subjects[5];

         subjects[0] = &node->alpha;
         subjects[1] = &node->label_alpha;
         subjects[2] = &node->zoom;
         subjects[3] = &node->x;
         subjects[4] = &node->y;

         menu_animation_kill_by_subject(5, subjects);
      }

      file_list_free_userdata(dst, i);
      file_list_free_actiondata(dst, i); /* this one was allocated by us */
   }

   file_list_copy(src, dst);

   size = dst->size;

   for (i = 0; i < size; ++i)
   {
      void *src_udata = menu_entries_get_userdata_at_offset(src, i);
      void *src_adata = (void*)menu_entries_get_actiondata_at_offset(src, i);

      if (src_udata)
      {
         void *data = calloc(1, sizeof(xmb_node_t));
         memcpy(data, src_udata, sizeof(xmb_node_t));
         file_list_set_userdata(dst, i, data);
      }

      if (src_adata)
      {
         void *data = calloc(1, sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, i, data);
      }
   }
}

static void xmb_list_cache(void *data, menu_list_type_t type, unsigned action)
{
   size_t stack_size, list_size, selection;
   xmb_handle_t      *xmb     = (xmb_handle_t*)data;
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!xmb)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   xmb_list_deep_copy(selection_buf, xmb->selection_buf_old);
   xmb_list_deep_copy(menu_stack, xmb->menu_stack_old);
   xmb->selection_ptr_old = selection;

   list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL) + XMB_SYSTEM_TAB_END;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         xmb->categories.selection_ptr_old = xmb->categories.selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (xmb->categories.selection_ptr == 0)
               {
                  xmb->categories.selection_ptr = list_size;
                  xmb->categories.active.idx = list_size - 1;
               }
               else
                  xmb->categories.selection_ptr--;
               break;
            default:
               if (xmb->categories.selection_ptr == list_size)
               {
                  xmb->categories.selection_ptr = 0;
                  xmb->categories.active.idx = 1;
               }
               else
                  xmb->categories.selection_ptr++;
               break;
         }

         stack_size = menu_stack->size;

         if (menu_stack->list[stack_size - 1].label)
            free(menu_stack->list[stack_size - 1].label);
         menu_stack->list[stack_size - 1].label = NULL;

         switch (xmb->categories.selection_ptr)
         {
            case XMB_SYSTEM_TAB_MAIN:
               menu_stack->list[stack_size - 1].label = 
                  strdup(menu_hash_to_str(MENU_VALUE_MAIN_MENU));
               menu_stack->list[stack_size - 1].type = 
                  MENU_SETTINGS;
               break;
            case XMB_SYSTEM_TAB_SETTINGS:
               menu_stack->list[stack_size - 1].label = 
                  strdup(menu_hash_to_str(MENU_VALUE_SETTINGS_TAB));
               menu_stack->list[stack_size - 1].type = 
                  MENU_SETTINGS_TAB;
               break;
            case XMB_SYSTEM_TAB_HISTORY:
               menu_stack->list[stack_size - 1].label = 
                  strdup(menu_hash_to_str(MENU_VALUE_HISTORY_TAB));
               menu_stack->list[stack_size - 1].type = 
                  MENU_HISTORY_TAB;
               break;
            case XMB_SYSTEM_TAB_ADD:
               menu_stack->list[stack_size - 1].label = 
                  strdup(menu_hash_to_str(MENU_VALUE_ADD_TAB));
               menu_stack->list[stack_size - 1].type = 
                  MENU_ADD_TAB;
               break;
            default:
               menu_stack->list[stack_size - 1].label = 
                  strdup(menu_hash_to_str(MENU_VALUE_HORIZONTAL_MENU));
               menu_stack->list[stack_size - 1].type = 
                  MENU_SETTING_HORIZONTAL_MENU;
               break;
         }
         break;
      default:
         break;
   }
}


static void xmb_context_destroy(void)
{
   unsigned i;
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      menu_display_texture_unload((uintptr_t*)&xmb->textures.list[i].id);

   xmb_context_destroy_horizontal_list(xmb);

   menu_display_free_main_font();
}

static void xmb_toggle(void *userdata, bool menu_on)
{
   bool tmp             = false;
   xmb_handle_t *xmb    = (xmb_handle_t*)userdata;

   if (!xmb)
      return;

   xmb->depth = xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (!menu_on)
   {
      xmb->alpha = 0;
      return;
   }

   menu_animation_push(XMB_DELAY, 1.0f,
         &xmb->alpha, EASING_IN_OUT_QUAD, -1, NULL);

   tmp = !menu_entries_needs_refresh();

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   xmb_toggle_horizontal_list(xmb);
}

static int deferred_push_content_actions(menu_displaylist_info_t *info)
{
   int ret = menu_displaylist_push_list(info, DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS);
   if (ret != 0)
      return ret;
   menu_displaylist_push_list_process(info);
   return 0;
}

static int xmb_list_bind_init_compare_label(menu_file_list_cbs_t *cbs,
      uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_CONTENT_ACTIONS:
         cbs->action_deferred_push = deferred_push_content_actions;
         break;
      default:
         return -1;
   }

   return 0;
}

static int xmb_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (xmb_list_bind_init_compare_label(cbs, label_hash) == 0)
      return 0;

   return -1;
}

static int xmb_list_push(menu_displaylist_info_t *info, unsigned type)
{
   int ret = -1;
   menu_handle_t *menu   = menu_driver_get_ptr();
   global_t    *global   = global_get_ptr();

   switch (type)
   {
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_clear(info->list);

         if (global->inited.main && (global->inited.core.type != CORE_TYPE_DUMMY))
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CONTENT_SETTINGS), PARSE_ACTION, false);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CORE_LIST), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_LIST), PARSE_ACTION, false);
#if defined(HAVE_NETWORKING)
#if defined(HAVE_LIBRETRODB)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ADD_CONTENT_LIST), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ONLINE_UPDATER), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INFORMATION_LIST), PARSE_ACTION, false);
#ifndef HAVE_DYNAMIC
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_RESTART_RETROARCH), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CONFIGURATIONS), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_CURRENT_CONFIG), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_NEW_CONFIG), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_HELP_LIST), PARSE_ACTION, false);
#if !defined(IOS)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_QUIT_RETROARCH), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SHUTDOWN), PARSE_ACTION, false);
         info->need_push    = true;
         ret = 0;
         break;
   }
   return ret;
}

static bool xmb_menu_init_list(void *data)
{
   int ret;
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label, menu_hash_to_str(MENU_VALUE_MAIN_MENU), sizeof(info.label));

   menu_entries_push(menu_stack, info.path, info.label, info.type, info.flags, 0);

   info.list  = selection_buf;
   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);

   info.need_push = true;

   (void)ret;

   menu_displaylist_push_list_process(&info);

   return true;
}

static int xmb_pointer_tap(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection, idx;
   unsigned header_height;
   bool scroll              = false;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (y < header_height)
   {
      menu_entries_pop_stack(&selection, 0);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, selection, MENU_ACTION_SELECT);

      idx  = ptr;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_messagebox,
   generic_menu_iterate,
   xmb_render,
   xmb_frame,
   xmb_init,
   xmb_free,
   xmb_context_reset,
   xmb_context_destroy,
   xmb_populate_entries,
   xmb_toggle,
   xmb_navigation_clear,
   xmb_navigation_pointer_changed,
   xmb_navigation_pointer_changed,
   xmb_navigation_set,
   xmb_navigation_pointer_changed,
   xmb_navigation_alphabet,
   xmb_navigation_alphabet,
   xmb_menu_init_list,
   xmb_list_insert,
   NULL,
   xmb_list_clear,
   xmb_list_cache,
   xmb_list_push,
   xmb_list_get_selection,
   xmb_list_get_size,
   xmb_list_get_entry,
   NULL,
   xmb_list_bind_init,
   xmb_load_image,
   "xmb",
   xmb_environ,
   xmb_pointer_tap,
};
