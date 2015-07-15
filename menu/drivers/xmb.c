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
#include <string/stdstring.h>
#include <string/string_list.h>

#include "../menu.h"
#include "../menu_driver.h"
#include "../menu_entry.h"
#include "../menu_animation.h"
#include "../menu_display.h"
#include "../menu_hash.h"
#include "../menu_video.h"

#include "../menu_cbs.h"

#include "../../file_ext.h"
#include "../../gfx/video_texture.h"

#include "../../runloop_data.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 10
#endif

typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   GRuint icon;
   GRuint content_icon;
} xmb_node_t;

enum
{
   XMB_TEXTURE_SETTINGS = 0,
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
   XMB_TEXTURE_LAST
};

struct xmb_texture_item
{
   GRuint id;
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
   GRuint boxart;
   float boxart_size;

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

   char title_name[PATH_MAX_LENGTH];

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
      float item;
      float subitem;
   } above_offset;

   struct
   {
      float item;
   } under_offset;

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
         float zoom;
         float alpha;
         unsigned idx;
         unsigned idx_old;
      } active;

      struct
      {
         float zoom;
         float alpha;
      } passive;

      float x_pos;
      size_t selection_ptr_old;
      size_t selection_ptr;
   } categories;

   struct
   {
      struct
      {
         float zoom;
         float alpha;
         float factor;
      } active;

      struct
      {
         float zoom;
         float alpha;
      } passive;
   } item;

   xmb_node_t settings_node;
   bool prevent_populate;

   gfx_font_raster_block_t raster_block;
} xmb_handle_t;

static const GRfloat rmb_vertex[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

static const GRfloat rmb_tex_coord[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0,
};

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu);
static void xmb_init_horizontal_list(menu_handle_t *menu, xmb_handle_t *xmb);
static void xmb_context_reset_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu, const char *themepath);

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
   menu_handle_t *menu     = (menu_handle_t*)data;
   menu_entries_t *entries = menu    ? &menu->entries : NULL;
   menu_list_t *menu_list  = entries ? entries->menu_list : NULL;
   xmb_handle_t *xmb       = menu    ? (xmb_handle_t*)menu->userdata : NULL;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         if (menu_list)
            list_size  = menu_list_get_stack_size(menu_list);
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         break;
   }

   return list_size;
}

static void *xmb_list_get_entry(void *data, menu_list_type_t type, unsigned i)
{
   size_t list_size       = 0;
   menu_handle_t *menu    = (menu_handle_t*)data;
   xmb_handle_t *xmb      = menu ? (xmb_handle_t*)menu->userdata : NULL;
   menu_list_t *menu_list = menu_list_get_ptr();
   void *ptr              = NULL;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         if (menu_list)
            list_size  = menu_list_get_stack_size(menu_list);
         if (i < list_size)
            ptr = (void*)&menu_list->menu_stack->list[i];
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         if (i < list_size)
            ptr = (void*)&xmb->horizontal_list->list[i];
         break;
   }

   return ptr;
}

static float xmb_item_y(xmb_handle_t *xmb, int i, size_t current)
{
   float iy = xmb->icon.spacing.vertical;

   if (i < (int)current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + xmb->above_offset.subitem);
      else
         iy *= (i - (int)current + xmb->above_offset.item);
   else
      iy    *= (i - (int)current + xmb->under_offset.item);

   if (i == (int)current)
      iy = xmb->icon.spacing.vertical * xmb->item.active.factor;

   return iy;
}

static void xmb_draw_icon_begin(gl_t *gl)
{
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
}

static void xmb_draw_icon_end(void)
{
   glDisable(GL_BLEND);
}

static void xmb_draw_icon(gl_t *gl, xmb_handle_t *xmb,
      GRuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gfx_coords coords;
   unsigned width, height;
   GRfloat color[16];
   math_matrix_4x4 mymat, mrot, mscal;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   video_driver_get_size(&width, &height);

   if (
         x < -xmb->icon.size/2 || 
         x > width ||
         y < xmb->icon.size/2 ||
         y > height + xmb->icon.size)
      return;

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = alpha;

   matrix_4x4_rotate_z(&mrot, rotation);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   glViewport(x, height - y, xmb->icon.size, xmb->icon.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   menu_video_draw_frame(gl->shader, &coords, &mymat, false, texture);
}

static void xmb_draw_icon_predone(gl_t *gl, xmb_handle_t *xmb,
      math_matrix_4x4 *mymat,
      GRuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gfx_coords coords;
   unsigned width, height;
   GRfloat color[16];

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   video_driver_get_size(&width, &height);

   if (
         x < -xmb->icon.size/2 || 
         x > width ||
         y < xmb->icon.size/2 ||
         y > height + xmb->icon.size)
      return;

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = alpha;

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, height - y, xmb->icon.size, xmb->icon.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   menu_video_draw_frame(gl->shader, &coords, mymat, false, texture);
}

static void xmb_draw_boxart(gl_t *gl, xmb_handle_t *xmb)
{
   struct gfx_coords coords;
   unsigned width, height;
   float x, y;
   math_matrix_4x4 mymat, mrot, mscal;
   GRfloat color[16];

   video_driver_get_size(&width, &height);

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = xmb->alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = xmb->alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = xmb->alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = xmb->alpha;

   y = xmb->margins.screen.top + xmb->icon.size + xmb->boxart_size;

   x = xmb->margins.screen.left + xmb->icon.spacing.horizontal +
      xmb->icon.spacing.horizontal*4 - xmb->icon.size / 4;

   matrix_4x4_rotate_z(&mrot, 0);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, 1, 1, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   glViewport(x, height - y, xmb->boxart_size, xmb->boxart_size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   menu_video_draw_frame(gl->shader, &coords, &mymat, false, xmb->boxart);
}

static void xmb_draw_text(menu_handle_t *menu,
      xmb_handle_t *xmb,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align)
{
   unsigned width, height;
   uint8_t a8                =   0;
   struct font_params params = {0};

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8 = 255 * alpha;

   if (a8 == 0)
      return;

   video_driver_get_size(&width, &height);

   if (x < -xmb->icon.size || x > width + xmb->icon.size
         || y < -xmb->icon.size || y > height + xmb->icon.size)
      return;

   params.x           = x        / width;
   params.y           = 1.0f - y / height;

   params.scale       = scale_factor;
   params.color       = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;
   params.text_align  = text_align;

   video_driver_set_osd_msg(str, &params, menu->display.font.buf);
}

static void xmb_render_messagebox_internal(const char *message)
{
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_frame_messagebox(const char *message)
{
   int x, y;
   unsigned i;
   unsigned width, height;
   struct string_list *list = NULL;
   gl_t *gl                 = NULL;
   xmb_handle_t *xmb        = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();

   if (!menu)
      return;

   video_driver_get_size(&width, &height);

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   x = width  / 2 - strlen(list->elems[0].data) * menu->display.font.size / 4;
   y = height / 2 - list->size * menu->display.font.size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(menu,
               xmb,
               msg,
               x,
               y + i * menu->display.font.size,
               1,
               1,
               TEXT_ALIGN_LEFT);
   }

end:
   string_list_free(list);
}

static void xmb_update_boxart(xmb_handle_t *xmb, unsigned i)
{
   menu_entry_t entry;
   char path[PATH_MAX_LENGTH] = {0};
   settings_t *settings   = config_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   menu_entry_get(&entry, i, menu_list->selection_buf, true);

   fill_pathname_join(path, settings->boxarts_directory, entry.path, sizeof(path));
   strlcat(path, ".png", sizeof(path));

   if (path_file_exists(path))
      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, path,
            "cb_menu_boxart", 0, 1, true);
   else if (xmb->depth == 1)
      xmb->boxart = 0;
}

static void xmb_selection_pointer_changed(bool allow_animations)
{
   unsigned i, current, end, tag, height, skip;
   int threshold = 0;
   xmb_handle_t    *xmb   = NULL;
   menu_handle_t    *menu = menu_driver_get_ptr();
   menu_display_t   *disp = menu_display_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   current   = nav->selection_ptr;
   end       = menu_entries_get_end();
   tag       = (uintptr_t)menu_list;
   threshold = xmb->icon.size*10;

   video_driver_get_size(NULL, &height);

   menu_animation_kill_by_tag(disp->animation, tag);
   menu_entries_set_start(0);
   skip = 0;

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia = xmb->item.passive.alpha;
      float iz = xmb->item.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)menu_list_get_userdata_at_offset(
            menu_list->selection_buf, i);

      if (!node)
         continue;

      iy      = xmb_item_y(xmb, i, current);
      real_iy = iy + xmb->margins.screen.top;

      if (i == current)
      {
         ia = xmb->item.active.alpha;
         iz = xmb->item.active.zoom;

         if (settings->menu.boxart_enable)
            xmb_update_boxart(xmb, i);
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
         menu_animation_push(disp->animation,
               XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, ia, &node->label_alpha, EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, iz, &node->zoom,  EASING_IN_OUT_QUAD, tag, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, iy, &node->y,     EASING_IN_OUT_QUAD, tag, NULL);
      }
   }

   menu_entries_set_start(skip);
}

static void xmb_list_open_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height = 0;
   int threshold = xmb->icon.size * 10;
   size_t           end = 0;
   menu_display_t *disp = menu_display_get_ptr();

   if (!disp)
      return;

   end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)menu_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = xmb->item.active.alpha;
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
         menu_animation_push(disp->animation,
               XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, 0, &node->label_alpha, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, xmb->icon.size * dir * -2, &node->x,
               EASING_IN_OUT_QUAD, -1, NULL);
      }
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   int threshold = xmb->icon.size * 10;
   menu_display_t *disp = menu_display_get_ptr();
   size_t           end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)
         menu_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (dir == 1 || (dir == -1 && i != current))
         node->alpha = 0;

      if (dir == 1 || dir == -1)
         node->label_alpha = 0;

      node->x = xmb->icon.size * dir * 2;
      node->y = xmb_item_y(xmb, i, current);
      node->zoom = xmb->categories.passive.zoom;

      real_y = node->y + xmb->margins.screen.top;

      if (i == current)
         node->zoom = xmb->categories.active.zoom;

      ia    = xmb->item.passive.alpha;
      if (i == current)
         ia = xmb->item.active.alpha;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x = 0;
      }
      else
      {
         menu_animation_push(disp->animation,
               XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
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

   node->alpha = xmb->categories.passive.alpha;
   node->zoom  = xmb->categories.passive.zoom;

   if ((i + 1) == xmb->categories.active.idx)
   {
      node->alpha = xmb->categories.active.alpha;
      node->zoom  = xmb->categories.active.zoom;
   }

   file_list_free_actiondata(xmb->horizontal_list, i);
   file_list_set_actiondata(xmb->horizontal_list, i, node);

   return node;
}

static xmb_node_t* xmb_get_userdata_from_horizontal_list(
      xmb_handle_t *xmb, unsigned i)
{
   return (xmb_node_t*)menu_list_get_actiondata_at_offset(xmb->horizontal_list, i);
}

static void xmb_push_animations(xmb_node_t *node, float ia, float ix)
{
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp)
      return;

   menu_animation_push(disp->animation,
         XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, -1, NULL);
   menu_animation_push(disp->animation,
         XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, -1, NULL);
   menu_animation_push(disp->animation,
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
         menu_list_get_userdata_at_offset(list, i);
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

      if (path_file_exists(path))
         rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, path,
               "cb_menu_wallpaper", 0, 1, true);
   }

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         menu_list_get_userdata_at_offset(list, i);
      float ia         = 0.5;

      if (!node)
         continue;

      node->x           = xmb->icon.spacing.horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = xmb->item.active.alpha;

      xmb_push_animations(node, ia, 0);
   }
}

static void xmb_set_title(xmb_handle_t *xmb)
{
   if (xmb->categories.selection_ptr == 0)
      menu_entries_get_title(xmb->title_name, sizeof(xmb->title_name));
   else
   {
      const char *path = NULL;
      file_list_get_at_offset(
            xmb->horizontal_list,
            xmb->categories.selection_ptr - 1,
            &path, NULL, NULL, NULL);

      if (!path)
         return;

      strlcpy(xmb->title_name, path, sizeof(xmb->title_name));

      path_remove_extension(xmb->title_name);
   }
}

static void xmb_list_switch_horizontal_list(xmb_handle_t *xmb, menu_handle_t *menu)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   for (j = 0; j <= list_size; j++)
   {
      float ia                    = xmb->categories.passive.alpha;
      float iz                    = xmb->categories.passive.zoom;
      xmb_node_t *node            = &xmb->settings_node;

      if (j > 0)
         node = xmb_get_userdata_from_horizontal_list(xmb, j - 1);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
      {
         ia = xmb->categories.active.alpha;
         iz = xmb->categories.active.zoom;
      }

      menu_animation_push(menu->display.animation,
            XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
      menu_animation_push(menu->display.animation,
            XMB_DELAY, iz, &node->zoom, EASING_IN_OUT_QUAD, -1, NULL);
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   int dir = -1;
   menu_handle_t    *menu = menu_driver_get_ptr();
   menu_display_t   *disp = menu_display_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   settings_t *settings   = config_get_ptr();

   if (!menu)
      return;

   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb->categories.active.idx += dir;

   xmb_list_switch_horizontal_list(xmb, menu);

   menu_animation_push(disp->animation, XMB_DELAY,
         xmb->icon.spacing.horizontal * -(float)xmb->categories.selection_ptr,
         &xmb->categories.x_pos, EASING_IN_OUT_QUAD, -1, NULL);

   dir = -1;
   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_switch_new(xmb, menu_list->selection_buf,
         dir, nav->selection_ptr);
   xmb->categories.active.idx_old = xmb->categories.selection_ptr;

   if (settings->menu.boxart_enable)
      xmb_update_boxart(xmb, 0);
}

static void xmb_list_open_horizontal_list(xmb_handle_t *xmb, menu_handle_t *menu)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   for (j = 0; j <= list_size; j++)
   {
      float ia          = 0;
      xmb_node_t *node  = &xmb->settings_node;

      if (j > 0)
         node = xmb_get_userdata_from_horizontal_list(xmb, j - 1);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
         ia = xmb->categories.active.alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories.passive.alpha;

      menu_animation_push(menu->display.animation, XMB_DELAY, ia,
            &node->alpha, EASING_IN_OUT_QUAD, -1, NULL);
   }
}

static void xmb_refresh_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu)
{
   char mediapath[PATH_MAX_LENGTH] = {0};
   char themepath[PATH_MAX_LENGTH] = {0};

   settings_t *settings = config_get_ptr();

   fill_pathname_join(mediapath, settings->assets_directory, "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));

   xmb_context_destroy_horizontal_list(xmb, menu);
   if (xmb->horizontal_list)
      free(xmb->horizontal_list);
   xmb->horizontal_list = NULL;

   xmb_init_horizontal_list(menu, xmb);
   xmb_context_reset_horizontal_list(xmb, menu, themepath);
}

static int xmb_environ(menu_environ_cb_t type, void *data)
{
   switch (type)
   {
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         {
            menu_handle_t    *menu   = menu_driver_get_ptr();
            xmb_handle_t *xmb        = menu ? 
               (xmb_handle_t*)menu->userdata : NULL;

            if (!xmb || !menu)
               return -1;

            xmb_refresh_horizontal_list(xmb, menu);
         }
         break;
      default:
         return -1;
   }

   return 0;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   int                dir = 0;
   menu_handle_t    *menu = menu_driver_get_ptr();
   menu_display_t   *disp = menu_display_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!menu)
      return;

   xmb->depth = xmb_list_get_size(menu, MENU_LIST_PLAIN);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;

   xmb_list_open_horizontal_list(xmb, menu);

   xmb_list_open_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_open_new(xmb, menu_list->selection_buf,
         dir, nav->selection_ptr);

   switch (xmb->depth)
   {
      case 1:
         menu_animation_push(disp->animation,
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, 0, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, -1, NULL);
         break;
      case 2:
         menu_animation_push(disp->animation,
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, -1, NULL);
         menu_animation_push(disp->animation,
               XMB_DELAY, 1, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, -1, NULL);
         break;
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   if (xmb->prevent_populate)
   {
      xmb_selection_pointer_changed(false);
      xmb->prevent_populate = false;
      return;
   }

   xmb_set_title(xmb);

   if (xmb->categories.selection_ptr != xmb->categories.active.idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);
}

static GRuint xmb_icon_get_id(xmb_handle_t *xmb,
      xmb_node_t *core_node, xmb_node_t *node, unsigned type, bool active)
{
   switch(type)
   {
      case MENU_FILE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER].id;
      case MENU_FILE_PLAIN:
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_PLAYLIST_ENTRY:
      case MENU_FILE_RDB_ENTRY:
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
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN].id;
      case MENU_SETTING_ACTION_CLOSE:
         return xmb->textures.list[XMB_TEXTURE_CLOSE].id;
      case MENU_SETTING_ACTION_SAVESTATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
      case MENU_SETTING_ACTION_LOADSTATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
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
   }

   return xmb->textures.list[XMB_TEXTURE_SUBSETTING].id;
}

static void xmb_draw_items(xmb_handle_t *xmb, gl_t *gl,
      file_list_t *list, file_list_t *stack,
      size_t current, size_t cat_selection_ptr)
{
   unsigned i, width, height, ticker_limit;
   math_matrix_4x4 mymat, mrot, mscal;
   const char *label           = NULL;
   xmb_node_t *core_node       = NULL;
   size_t end                  = 0;
   uint64_t frame_count        = video_driver_get_frame_count();
   menu_handle_t *menu         = menu_driver_get_ptr();
   settings_t   *settings      = config_get_ptr();

   if (!list || !list->size || !menu)
      return;

   video_driver_get_size(&width, &height);

   menu_list_get_last(stack, NULL, &label, NULL, NULL);

   if (cat_selection_ptr)
      core_node = xmb_get_userdata_from_horizontal_list(xmb, cat_selection_ptr - 1);

   end = file_list_get_size(list);

   matrix_4x4_rotate_z(&mrot, 0 /* rotation */);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, 1 /* scale_factor */, 1 /* scale_factor */, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   i = menu_entries_get_start();

   if (list == xmb->selection_buf_old)
      i = 0;

   for (; i < end; i++)
   {
      const float half_size = xmb->icon.size / 2.0f;
      char name[PATH_MAX_LENGTH];
      char value[PATH_MAX_LENGTH];
      menu_entry_t entry;
      float icon_x, icon_y;

      GRuint texture_switch       = 0;
      GRuint         icon         = 0;
      xmb_node_t *   node         = (xmb_node_t*)menu_list_get_userdata_at_offset(list, i);
      uint32_t hash_label         = 0;
      uint32_t hash_value         = 0;
      bool do_draw_text           = false;

      *name = *value = 0;
      *entry.path = *entry.label = *entry.value = 0;
      entry.idx = entry.spacing = entry.type = 0;

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

      menu_entry_get(&entry, i, list, true);

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
         case MENU_LABEL_CORE_INFORMATION:
            icon = xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
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

      ticker_limit = 35;
      if (entry.value[0] == '\0')
      {
         if (settings->menu.boxart_enable && xmb->boxart)
            ticker_limit = 40;
         else
            ticker_limit = 70;
      }

      menu_animation_ticker_str(name, ticker_limit,
            frame_count / 20, entry.path,
            (i == current));

      xmb_draw_text(menu, xmb, name,
            node->x + xmb->margins.screen.left + 
            xmb->icon.spacing.horizontal + xmb->margins.label.left, 
            xmb->margins.screen.top + node->y + xmb->margins.label.top, 
            1, node->label_alpha, TEXT_ALIGN_LEFT);

      menu_animation_ticker_str(value, 35,
            frame_count / 20, entry.value,
            (i == current));


      if (do_draw_text)
         xmb_draw_text(menu, xmb, value,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal + 
               xmb->margins.label.left + xmb->margins.setting.left, 
               xmb->margins.screen.top + node->y + xmb->margins.label.top, 
               1, 
               node->label_alpha,
               TEXT_ALIGN_LEFT);

      xmb_draw_icon_begin(gl);

      xmb_draw_icon(gl, xmb, icon, icon_x, icon_y, node->alpha, 0, node->zoom);


      if (texture_switch != 0)
         xmb_draw_icon_predone(gl, xmb, &mymat,
               texture_switch,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal
               + xmb->icon.size / 2.0 + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->icon.size / 2.0,
               node->alpha,
               0,
               1);

      xmb_draw_icon_end();
   }
}


static void xmb_draw_cursor(gl_t *gl, xmb_handle_t *xmb, float x, float y)
{
   unsigned width, height;
   struct gfx_coords coords;
   math_matrix_4x4 mymat, mrot;
   GRfloat color[16];

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = xmb->alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = xmb->alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = xmb->alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = xmb->alpha;

   video_driver_get_size(&width, &height);

   matrix_4x4_rotate_z(&mrot, 0);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   glViewport(x, height - y, xmb->cursor.size, xmb->cursor.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   xmb_draw_icon_begin(gl);

   menu_video_draw_frame(gl->shader, &coords, &mymat, true, xmb->textures.list[XMB_TEXTURE_POINTER].id);
}

static void xmb_render(void)
{
   unsigned i, current, end, height = 0;
   xmb_handle_t      *xmb   = NULL;
   settings_t   *settings   = config_get_ptr();
   menu_handle_t    *menu   = menu_driver_get_ptr();
   menu_display_t   *disp   = menu_display_get_ptr();
   menu_animation_t *anim   = menu_animation_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   menu_animation_update(disp->animation, disp->animation->delta_time / IDEAL_DT);

   video_driver_get_size(NULL, &height);

   current = nav->selection_ptr;
   end     = menu_list_get_size(menu_list);

   if (settings->menu.pointer.enable || settings->menu.mouse.enable)
   {
      for (i = 0; i < end; i++)
      {
         float item_y1 = xmb->margins.screen.top + xmb_item_y(xmb, i, current);
         float item_y2 = item_y1 + xmb->icon.size;

         if (settings->menu.pointer.enable)
         {
            if (menu_input->pointer.y > item_y1 && menu_input->pointer.y < item_y2)
               menu_input->pointer.ptr = i;
         }

         if (settings->menu.mouse.enable)
         {
            if (menu_input->mouse.y > item_y1 && menu_input->mouse.y < item_y2)
               menu_input->mouse.ptr = i;
         }
      }
   }

   if (menu_entries_get_start() >= end)
      menu_entries_set_start(0);

   anim->is_active = false;
   anim->label.is_updated    = false;
}

static void xmb_frame_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu, gl_t *gl)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = &xmb->settings_node;

      if (i > 0)
         node = xmb_get_userdata_from_horizontal_list(xmb, i - 1);

      if (!node)
         continue;

      xmb_draw_icon_begin(gl);

      xmb_draw_icon(gl, xmb, node->icon, 
            xmb->x + xmb->categories.x_pos + 
            xmb->margins.screen.left + 
            xmb->icon.spacing.horizontal * (i + 1) - xmb->icon.size / 2.0,
            xmb->margins.screen.top + xmb->icon.size / 2.0, 
            node->alpha, 
            0, 
            node->zoom);

      xmb_draw_icon_end();
   }
}

static void xmb_frame(void)
{
   math_matrix_4x4 mymat, mrot, mscal;
   unsigned depth;
   unsigned width, height;
   char msg[PATH_MAX_LENGTH];
   char title_msg[PATH_MAX_LENGTH];
   char timedate[PATH_MAX_LENGTH];
   bool render_background                  = false;
   xmb_handle_t *xmb                       = NULL;
   gl_t *gl                                = NULL;
   const struct font_renderer *font_driver = NULL;
   menu_handle_t   *menu                   = menu_driver_get_ptr();
   menu_input_t *menu_input                = menu_input_get_ptr();
   menu_navigation_t *nav                  = menu_navigation_get_ptr();
   menu_list_t *menu_list                  = menu_list_get_ptr();
   settings_t   *settings                  = config_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   msg[0]       = '\0';
   title_msg[0] = '\0';
   timedate[0]  = '\0';

   video_driver_get_size(&width, &height);

   menu_display_font_bind_block(menu, font_driver, &xmb->raster_block);

   xmb->raster_block.carr.coords.vertices = 0;

   menu_video_frame_background(menu, settings,
         gl, xmb->textures.bg.id, xmb->alpha, 0.75f, false);

   xmb_draw_text(menu, xmb,
         xmb->title_name, xmb->margins.title.left,
         xmb->margins.title.top, 1, 1, TEXT_ALIGN_LEFT);

   if (settings->menu.timedate_enable)
   {
      menu_display_timedate(timedate, sizeof(timedate), 0);

      xmb_draw_text(menu, xmb, timedate,
            width - xmb->margins.title.left - xmb->icon.size / 4, 
            xmb->margins.title.top, 1, 1, TEXT_ALIGN_RIGHT);
   }

   if (settings->menu.core_enable)
   {
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      xmb_draw_text(menu, xmb, title_msg, xmb->margins.title.left, 
            height - xmb->margins.title.bottom, 1, 1, TEXT_ALIGN_LEFT);
   }

   depth = xmb_list_get_size(menu, MENU_LIST_PLAIN);

   xmb_draw_items(xmb, gl,
         xmb->selection_buf_old,
         xmb->menu_stack_old,
         xmb->selection_ptr_old,
         depth > 1 ? xmb->categories.selection_ptr :
         xmb->categories.selection_ptr_old);

   xmb_draw_items(xmb, gl,
         menu_list->selection_buf,
         menu_list->menu_stack,
         nav->selection_ptr,
         xmb->categories.selection_ptr);

   matrix_4x4_rotate_z(&mrot, 0 /* rotation */);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, 1 /* scale_factor */, 1 /* scale_factor */, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   xmb_draw_icon_begin(gl);

   if (settings->menu.boxart_enable && xmb->boxart)
      xmb_draw_boxart(gl, xmb);

   if (settings->menu.timedate_enable)
      xmb_draw_icon_predone(gl, xmb, &mymat, xmb->textures.list[XMB_TEXTURE_CLOCK].id,
            width - xmb->icon.size, xmb->icon.size, 1, 0, 1);

   xmb_draw_icon_predone(gl, xmb, &mymat, xmb->textures.list[XMB_TEXTURE_ARROW].id,
         xmb->x + xmb->margins.screen.left + 
         xmb->icon.spacing.horizontal - xmb->icon.size / 2.0 + xmb->icon.size,
         xmb->margins.screen.top + 
         xmb->icon.size / 2.0 + xmb->icon.spacing.vertical 
         * xmb->item.active.factor,
         xmb->textures.arrow.alpha, 0, 1);

   xmb_frame_horizontal_list(xmb, menu, gl);

   menu_display_font_flush_block(menu, font_driver);

   if (menu_input->keyboard.display)
   {
      const char *str = *menu_input->keyboard.buffer;

      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s",
            menu_input->keyboard.label, str);
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
      menu_video_frame_background(menu, settings, gl,
            xmb->textures.bg.id, xmb->alpha, 0.75f, true);
      xmb_frame_messagebox(msg);
   }

   if (settings->menu.mouse.enable)
      xmb_draw_cursor(gl, xmb, menu_input->mouse.x, menu_input->mouse.y);

   menu_display_unset_viewport();
}

static void xmb_init_horizontal_list(menu_handle_t *menu, xmb_handle_t *xmb)
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

   menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL);
}

static void xmb_font(menu_handle_t *menu)
{
   settings_t *settings = config_get_ptr();

   char mediapath[PATH_MAX_LENGTH] = {0};
   char themepath[PATH_MAX_LENGTH] = {0};
   char fontpath[PATH_MAX_LENGTH]  = {0};

   fill_pathname_join(mediapath, settings->assets_directory, "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(fontpath, themepath, "font.ttf", sizeof(fontpath));

   if (!menu_display_init_main_font(menu, fontpath, menu->display.font.size))
      RARCH_WARN("Failed to load font.");
}

static void xmb_layout(menu_handle_t *menu, xmb_handle_t *xmb)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   unsigned width, height, i, current, end;
   float scale_factor;

   video_driver_get_size(&width, &height);

   scale_factor = width / 1920.0;

   xmb->boxart_size             = 460.0 * scale_factor;
   xmb->cursor.size             = 48.0;
   menu->display.font.size      = 32.0 * scale_factor;
   xmb->icon.spacing.horizontal = 200.0 * scale_factor;
   xmb->icon.spacing.vertical   = 64.0 * scale_factor;
   xmb->margins.screen.left     = 336.0 * scale_factor;
   xmb->margins.screen.top      = (256+32) * scale_factor;
   xmb->margins.title.left      = 60 * scale_factor;
   xmb->margins.title.top       = 60 * scale_factor + menu->display.font.size/3;
   xmb->margins.title.bottom    = 60 * scale_factor - menu->display.font.size/3;
   xmb->margins.label.left      = 85.0 * scale_factor;
   xmb->margins.label.top       = menu->display.font.size / 3.0;
   xmb->margins.setting.left    = 600.0 * scale_factor;
   menu->display.header_height  = 128.0 * scale_factor;

   if (width >= 3840)
      scale_factor              = 2.0;
   else if (width >= 2560)
      scale_factor              = 1.5;
   else if (width >= 1920)
      scale_factor              = 1.0;
   else if (width >= 1440)
      scale_factor              = 0.75;
   else if (width >=  960)
      scale_factor              = 0.5;
   else if (width >=  640)
      scale_factor              = 0.375;
   else if (width >=  480)
      scale_factor              = 0.25;
   else if (width >=  320)
      scale_factor              = 0.1875;
   else if (width >=  240)
      scale_factor              = 0.125;

   xmb->icon.size               = 128.0 * scale_factor;

   current = nav->selection_ptr;
   end     = menu_entries_get_end();

   for (i = 0; i < end; i++)
   {
      float ia = xmb->item.passive.alpha;
      float iz = xmb->item.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)menu_list_get_userdata_at_offset(
            menu_list->selection_buf, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia = xmb->item.active.alpha;
         iz = xmb->item.active.zoom;
      }

      node->alpha       = ia;
      node->label_alpha = ia;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
   }
}

static void *xmb_init(void)
{
   unsigned width, height;
   menu_handle_t *menu                = NULL;
   xmb_handle_t *xmb                  = NULL;
   const video_driver_t *video_driver = NULL;
   menu_framebuf_t *frame_buf         = NULL;
   float scale_factor                 = 1;
   gl_t *gl                           = (gl_t*)
      video_driver_get_ptr(&video_driver);

   (void)scale_factor;

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize XMB menu driver: GL video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   frame_buf = &menu->display.frame_buf;

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
   xmb->prevent_populate        = false;

   xmb->categories.active.zoom  = 1.0;
   xmb->categories.passive.zoom = 0.5;
   xmb->item.active.zoom        = 1.0;
   xmb->item.passive.zoom       = 0.5;

   xmb->categories.active.alpha = 1.0;
   xmb->categories.passive.alpha= 0.5;
   xmb->item.active.alpha       = 1.0;
   xmb->item.passive.alpha      = 0.5;

   xmb->above_offset.subitem    = 1.5;
   xmb->above_offset.item       = -1.0;
   xmb->item.active.factor      = 3.0;
   xmb->under_offset.item       = 5.0;

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   frame_buf->width  = width;
   frame_buf->height = height;

   xmb_init_horizontal_list(menu, xmb);
   xmb_font(menu);

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
   driver_t *driver                        = driver_get_ptr();
   const struct font_renderer *font_driver = 
      (const struct font_renderer*)driver->font_osd_driver;

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

   if (font_driver->bind_block)
      font_driver->bind_block(driver->font_osd_data, NULL);
}

static void xmb_context_bg_destroy(xmb_handle_t *xmb)
{
   if (xmb->textures.bg.id)
      glDeleteTextures(1, &xmb->textures.bg.id);
}

static bool xmb_load_image(void *data, menu_image_type_t type)
{
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return false;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb || !data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         xmb_context_bg_destroy(xmb);
         xmb->textures.bg.id   = video_texture_load(data,
               TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);
         break;
      case MENU_IMAGE_BOXART:
         xmb->boxart = video_texture_load(data,
               TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);
         break;
   }

   return true;
}

static void xmb_toggle_horizontal_list(xmb_handle_t *xmb, menu_handle_t *menu)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = &xmb->settings_node;

      if (i > 0)
         node = xmb_get_userdata_from_horizontal_list(xmb, i - 1);

      if (!node)
         continue;

      node->alpha = 0;
      node->zoom  = xmb->categories.passive.zoom;

      if (i == xmb->categories.active.idx)
      {
         node->alpha = xmb->categories.active.alpha;
         node->zoom  = xmb->categories.active.zoom;
      }
      else if (xmb->depth <= 1)
         node->alpha = xmb->categories.passive.alpha;
   }
}

static void xmb_context_reset_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu, const char *themepath)
{
   unsigned i;
   size_t list_size            = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   xmb->categories.x_pos = xmb->icon.spacing.horizontal *
      -(float)xmb->categories.selection_ptr;

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

      node->icon         = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
      texture_image_load(&ti, content_texturepath);

      node->content_icon = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }

   xmb_toggle_horizontal_list(xmb, menu);
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
         case XMB_TEXTURE_SETTINGS:
            fill_pathname_join(path, iconpath, "settings.png",   sizeof(path));
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
      }

      if (path[0] == '\0' || !path_file_exists(path))
         continue;

      texture_image_load(&ti, path);

      xmb->textures.list[i].id   = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }

   xmb->settings_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS].id;
   xmb->settings_node.alpha = xmb->categories.active.alpha;
   xmb->settings_node.zoom  = xmb->categories.active.zoom;
}

static void xmb_context_reset_background(const char *iconpath)
{
   char path[PATH_MAX_LENGTH]  = {0};
   settings_t *settings        = config_get_ptr();

   fill_pathname_join(path, iconpath, "bg.png", sizeof(path));

   if (*settings->menu.wallpaper)
      strlcpy(path, settings->menu.wallpaper, sizeof(path));

   if (path_file_exists(path))
      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, path,
            "cb_menu_wallpaper", 0, 1, true);
}

static void xmb_context_reset(void)
{
   char mediapath[PATH_MAX_LENGTH] = {0};
   char themepath[PATH_MAX_LENGTH] = {0};
   char iconpath[PATH_MAX_LENGTH]  = {0};
   gl_t *gl                        = NULL;
   xmb_handle_t *xmb               = NULL;
   menu_handle_t *menu             = menu_driver_get_ptr();
   settings_t *settings            = config_get_ptr();

   if (!menu)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);
   if (!gl)
      return;

   xmb = (xmb_handle_t*)menu->userdata;
   if (!xmb)
      return;

   strlcpy(xmb->icon.dir, "png", sizeof(xmb->icon.dir));

   fill_pathname_join(mediapath, settings->assets_directory,
         "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   xmb_layout(menu, xmb);
   xmb_font(menu);
   xmb_context_reset_textures(xmb, iconpath);
   xmb_context_reset_background(iconpath);
   xmb_context_reset_horizontal_list(xmb, menu, themepath);
}

static void xmb_navigation_clear(bool pending_push)
{
   if (!pending_push)
      xmb_selection_pointer_changed(true);
}

static void xmb_navigation_pointer_changed(void)
{
   xmb_selection_pointer_changed(true);
}

static void xmb_navigation_set(bool scroll)
{
   xmb_selection_pointer_changed(true);
}

static void xmb_navigation_alphabet(size_t *unused)
{
   xmb_selection_pointer_changed(true);
}

static void xmb_list_insert(file_list_t *list,
      const char *path, const char *unused, size_t list_size)
{
   int current            = 0;
   int i                  = list_size;
   xmb_node_t *node       = NULL;
   xmb_handle_t *xmb      = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!list || !xmb)
      return;

   node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   current           = nav->selection_ptr;

   node->alpha       = xmb->item.passive.alpha;
   node->zoom        = xmb->item.passive.zoom;
   node->label_alpha = node->alpha;
   node->y           = xmb_item_y(xmb, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = xmb->item.active.alpha;
      node->label_alpha = xmb->item.active.alpha;
      node->zoom        = xmb->item.active.zoom;
   }

   file_list_set_userdata(list, i, node);
}

static void xmb_list_free(file_list_t *list,
      size_t idx, size_t list_size)
{
}

static void xmb_list_clear(file_list_t *list)
{
   menu_display_t *disp = menu_display_get_ptr();
   size_t size, i;

   size = list->size;
   for (i = 0; i < size; ++i)
   {
      float *subjects[5];
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      subjects[0] = &node->alpha;
      subjects[1] = &node->label_alpha;
      subjects[2] = &node->zoom;
      subjects[3] = &node->x;
      subjects[4] = &node->y;

      menu_animation_kill_by_subject(disp->animation, 5, subjects);

      file_list_free_userdata(list, i);
   }
}

static void xmb_list_deep_copy(menu_handle_t *menu, const file_list_t *src, file_list_t *dst)
{
   size_t size, i;

   size = dst->size;
   for (i = 0; i < size; ++i)
   {
      float *subjects[5];
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(dst, i);

      if (node)
      {
         subjects[0] = &node->alpha;
         subjects[1] = &node->label_alpha;
         subjects[2] = &node->zoom;
         subjects[3] = &node->x;
         subjects[4] = &node->y;

         menu_animation_kill_by_subject(menu->display.animation, 5, subjects);
      }

      file_list_free_userdata(dst, i);
      file_list_free_actiondata(dst, i); /* this one was allocated by us */
   }

   file_list_copy(src, dst);

   size = dst->size;
   for (i = 0; i < size; ++i)
   {
      void *src_udata = file_list_get_userdata_at_offset(src, i);
      void *src_adata = file_list_get_actiondata_at_offset(src, i);

      if (src_udata)
      {
         void *data = calloc(sizeof(xmb_node_t), 1);
         memcpy(data, src_udata, sizeof(xmb_node_t));
         file_list_set_userdata(dst, i, data);
      }

      if (src_adata)
      {
         void *data = calloc(sizeof(menu_file_list_cbs_t), 1);
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, i, data);
      }
   }
}

static void xmb_list_cache(menu_list_type_t type, unsigned action)
{
   size_t stack_size, list_size;
   xmb_handle_t      *xmb = NULL;
   menu_handle_t    *menu = menu_driver_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   xmb_list_deep_copy(menu, menu_list->selection_buf, xmb->selection_buf_old);
   xmb_list_deep_copy(menu, menu_list->menu_stack, xmb->menu_stack_old);
   xmb->selection_ptr_old = nav->selection_ptr;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         xmb->categories.selection_ptr_old = xmb->categories.selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               xmb->categories.selection_ptr--;
               break;
            default:
               xmb->categories.selection_ptr++;
               break;
         }

         list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);
         if (xmb->categories.selection_ptr > list_size)
         {
            xmb->categories.selection_ptr = list_size;
            return;
         }

         stack_size = menu_list->menu_stack->size;

         if (menu_list->menu_stack->list[stack_size - 1].label)
            free(menu_list->menu_stack->list[stack_size - 1].label);
         menu_list->menu_stack->list[stack_size - 1].label = NULL;

         if (xmb->categories.selection_ptr == 0)
         {
            menu_list->menu_stack->list[stack_size - 1].label = 
               strdup(menu_hash_to_str(MENU_VALUE_MAIN_MENU));
            menu_list->menu_stack->list[stack_size - 1].type = 
               MENU_SETTINGS;
         }
         else
         {
            menu_list->menu_stack->list[stack_size - 1].label = 
               strdup(menu_hash_to_str(MENU_VALUE_HORIZONTAL_MENU));
            menu_list->menu_stack->list[stack_size - 1].type = 
               MENU_SETTING_HORIZONTAL_MENU;
         }
         break;
   }
}

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb,
      menu_handle_t *menu)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(menu, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      xmb_node_t *node = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         continue;

      glDeleteTextures(1, &node->icon);
      glDeleteTextures(1, &node->content_icon);
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
      glDeleteTextures(1, &xmb->textures.list[i].id);

   xmb_context_destroy_horizontal_list(xmb, menu);

   menu_display_free_main_font(menu);
}

static void xmb_toggle(bool menu_on)
{
   xmb_handle_t *xmb    = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();
   menu_display_t *disp = menu_display_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   xmb->depth = xmb_list_get_size(menu, MENU_LIST_PLAIN);

   if (!menu_on)
   {
      xmb->alpha = 0;
      return;
   }

   menu_animation_push(disp->animation, XMB_DELAY, 1.0f,
         &xmb->alpha, EASING_IN_OUT_QUAD, -1, NULL);

   xmb->prevent_populate = !menu_entries_needs_refresh();

   xmb_toggle_horizontal_list(xmb, menu);
}

static int deferred_push_content_actions(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS);
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

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_render_messagebox_internal,
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
   xmb_list_insert,
   xmb_list_free,
   xmb_list_clear,
   xmb_list_cache,
   xmb_list_get_selection,
   xmb_list_get_size,
   xmb_list_get_entry,
   NULL,
   xmb_list_bind_init,
   xmb_load_image,
   "xmb",
   xmb_environ,
   NULL,
};
