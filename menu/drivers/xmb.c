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

#include "../menu.h"
#include "../menu_entry.h"
#include "../menu_animation.h"
#include "../menu_display.h"

#include <file/file_path.h>
#include "../../gfx/video_thread_wrapper.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_texture.h"
#include <compat/posix_string.h>
#include <string/stdstring.h>

#include "../../runloop_data.h"

#include "shared.h"

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
   GLuint icon;
   GLuint content_icon;
} xmb_node_t;

enum
{
   XMB_TEXTURE_SETTINGS = 0,
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_CORE_INFO,
   XMB_TEXTURE_CORE_OPTIONS,
   XMB_TEXTURE_INPUT_REMAPPING_OPTIONS,
   XMB_TEXTURE_CHEAT_OPTIONS,
   XMB_TEXTURE_DISK_OPTIONS,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
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
   GLuint id;
   char path[PATH_MAX_LENGTH];
};

typedef struct xmb_handle
{
   file_list_t *menu_stack_old;
   file_list_t *selection_buf_old;
   size_t selection_ptr_old;
   int depth;
   int old_depth;
   char box_message[PATH_MAX_LENGTH];
   float x;
   float alpha;

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

   gl_font_raster_block_t raster_block;
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
      GLuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gl_coords coords;
   math_matrix_4x4 mymat, mrot, mscal;
   global_t *global = global_get_ptr();

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   if (
         x < -xmb->icon.size/2 || 
         x > global->video_data.width ||
         y < xmb->icon.size/2 ||
         y > global->video_data.height + xmb->icon.size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   glViewport(x, global->video_data.height - y, xmb->icon.size, xmb->icon.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   matrix_4x4_rotate_z(&mrot, rotation);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   menu_gl_draw_frame(gl->shader, &coords, &mymat, false, texture);
}

static void xmb_draw_icon_predone(gl_t *gl, xmb_handle_t *xmb,
      math_matrix_4x4 *mymat,
      GLuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gl_coords coords;
   global_t *global = global_get_ptr();

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   if (
         x < -xmb->icon.size/2 || 
         x > global->video_data.width ||
         y < xmb->icon.size/2 ||
         y > global->video_data.height + xmb->icon.size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, global->video_data.height - y, xmb->icon.size, xmb->icon.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   menu_gl_draw_frame(gl->shader, &coords, mymat, false, texture);
}

static void xmb_draw_text(menu_handle_t *menu,
      xmb_handle_t *xmb,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align)
{
   uint8_t a8                =   0;
   struct font_params params = {0};
   global_t *global          = global_get_ptr();

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8 = 255 * alpha;

   if (a8 == 0)
      return;

   if (x < -xmb->icon.size || x > global->video_data.width + xmb->icon.size
         || y < -xmb->icon.size || y > global->video_data.height + xmb->icon.size)
      return;

   params.x           = x        / global->video_data.width;
   params.y           = 1.0f - y / global->video_data.height;

   params.scale       = scale_factor;
   params.color       = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;
   params.text_align  = text_align;

   video_driver_set_osd_msg(str, &params, menu->font.buf);
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
   struct string_list *list = NULL;
   gl_t *gl                 = NULL;
   xmb_handle_t *xmb        = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   global_t *global         = global_get_ptr();

   if (!menu)
      return;

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

   x = global->video_data.width  / 2 - strlen(list->elems[0].data) * menu->font.size / 4;
   y = global->video_data.height / 2 - list->size * menu->font.size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(menu, xmb, msg, x,
               y + i * menu->font.size, 1, 1, TEXT_ALIGN_LEFT);
   }

end:
   string_list_free(list);
}

static void xmb_selection_pointer_changed(void)
{
   unsigned i, current, end;
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   current = menu->navigation.selection_ptr;
   end     = menu_list_get_size(menu->menu_list);

   for (i = 0; i < end; i++)
   {
      float iy;
      float ia = xmb->item.passive.alpha;
      float iz = xmb->item.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            menu->menu_list->selection_buf, i);

      if (!node)
         continue;

      iy = xmb_item_y(xmb, i, current);

      if (i == current)
      {
         ia = xmb->item.active.alpha;
         iz = xmb->item.active.zoom;
      }

      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->label_alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, iz, &node->zoom,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, iy, &node->y,     EASING_IN_OUT_QUAD, NULL);
   }
}

static void xmb_list_open_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      float ia = 0;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = xmb->item.active.alpha;
      if (dir == -1)
         ia = 0;

      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, 0, &node->label_alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, xmb->icon.size * dir * -2, &node->x,
            EASING_IN_OUT_QUAD, NULL);
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (dir == 1 || (dir == -1 && i != current))
         node->alpha = 0;

      if (dir == 1 || dir == -1)
         node->label_alpha = 0;

      node->x = xmb->icon.size * dir * 2;
      node->y = xmb_item_y(xmb, i, current);

      if (i == current)
         node->zoom = 1;
   }
   for (i = 0; i < file_list_get_size(list); i++)
   {
      float ia;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);
       
      if (!node)
          continue;

      ia    = xmb->item.passive.alpha;
      if (i == current)
         ia = xmb->item.active.alpha;

      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, 0, &node->x, EASING_IN_OUT_QUAD, NULL);
   }

   xmb->old_depth = xmb->depth;
}

static xmb_node_t *xmb_node_allocate_userdata(xmb_handle_t *xmb,
      core_info_t *info, unsigned i)
{
   xmb_node_t *node            = NULL;
   global_t *global            = global_get_ptr();
   core_info_list_t *info_list = (core_info_list_t*)global->core_info;

   if (!info_list)
      return NULL;
   if (!info_list->count)
      return NULL;
   rarch_assert(i >= 0);
   rarch_assert(i <= info_list->count);
   if (i >= info_list->count)
      return NULL;

   info = (core_info_t*)&info_list->list[i];

   if (!info)
      return NULL;
   
   info->userdata = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!info->userdata)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node = (xmb_node_t*)info->userdata;

   if (!node)
      return NULL;

   node->alpha = xmb->categories.passive.alpha;
   node->zoom  = xmb->categories.passive.zoom;

   if ((i + 1) == xmb->categories.active.idx)
   {
      node->alpha = xmb->categories.active.alpha;
      node->zoom  = xmb->categories.active.zoom;
   }

   return node;
}

static xmb_node_t* xmb_get_userdata_from_core(xmb_handle_t *xmb,
      core_info_t *info, unsigned i)
{
   global_t *global            = global_get_ptr();
   core_info_list_t *info_list = (core_info_list_t*)global->core_info;

   if (!info_list)
      return NULL;
   if (!info_list->count)
      return NULL;
   rarch_assert(i >= 0);
   rarch_assert(i <= info_list->count);
   if (i >= info_list->count)
      return NULL;

   info = (core_info_t*)&info_list->list[i];

   if (!info)
      return NULL;


   return (xmb_node_t*)info->userdata;
}

static void xmb_push_animations(xmb_node_t *node, float ia, float ix)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu_animation_push(menu->animation,
         XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, NULL);
   menu_animation_push(menu->animation,
         XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, NULL);
   menu_animation_push(menu->animation,
         XMB_DELAY, ix, &node->x, EASING_IN_OUT_QUAD, NULL);
}

static void xmb_list_switch_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i;
   size_t end          = 0;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);
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
   size_t end          = 0;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);
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
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (menu->categories.selection_ptr == 0)
   {
      const char *dir   = NULL;
      const char *label = NULL;
      unsigned menu_type = 0;

      menu_list_get_last_stack(menu->menu_list, &dir, &label, &menu_type);
      get_title(label, dir, menu_type,
            xmb->title_name, sizeof(xmb->title_name));
   }
   else
   {
      core_info_t *info           = NULL;
      global_t *global            = global_get_ptr();
      core_info_list_t *info_list = (core_info_list_t*)global->core_info;

      if (!info_list)
         return;

      info = (core_info_t*)&info_list->list[menu->categories.selection_ptr - 1];

      if (info)
         strlcpy(xmb->title_name, info->display_name, sizeof(xmb->title_name));
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   unsigned j;
   int dir = -1;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (menu->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb->categories.active.idx += dir;

   for (j = 0; j < menu->categories.size; j++)
   {
      core_info_t *info           = NULL;
      float ia                    = xmb->categories.passive.alpha;
      float iz                    = xmb->categories.passive.zoom;
      xmb_node_t *node            = &xmb->settings_node;
      
      if (j > 0)
         node = xmb_get_userdata_from_core(xmb, info, j - 1);

      if (!node)
         continue;
      
      if (j == xmb->categories.active.idx)
      {
         ia = xmb->categories.active.alpha;
         iz = xmb->categories.active.zoom;
      }

      menu_animation_push(menu->animation,
            XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation,
            XMB_DELAY, iz, &node->zoom, EASING_IN_OUT_QUAD, NULL);
   }

   menu_animation_push(menu->animation, XMB_DELAY,
         xmb->icon.spacing.horizontal * -(float)menu->categories.selection_ptr,
         &xmb->categories.x_pos, EASING_IN_OUT_QUAD, NULL);

   dir = -1;
   if (menu->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_switch_new(xmb, menu->menu_list->selection_buf,
         dir, menu->navigation.selection_ptr);
   xmb->categories.active.idx_old = menu->categories.selection_ptr;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   unsigned j;
   int dir             = 0;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb->depth = file_list_get_size(menu->menu_list->menu_stack);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;

   for (j = 0; j < menu->categories.size; j++)
   {
      core_info_t *info = NULL;
      float ia          = 0;
      xmb_node_t *node  = &xmb->settings_node;
      
      if (j > 0)
         node = xmb_get_userdata_from_core(xmb, info, j - 1);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
         ia = xmb->categories.active.alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories.passive.alpha;

      menu_animation_push(menu->animation, XMB_DELAY, ia,
            &node->alpha, EASING_IN_OUT_QUAD, NULL);
   }

   xmb_list_open_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_open_new(xmb, menu->menu_list->selection_buf,
         dir, menu->navigation.selection_ptr);

   switch (xmb->depth)
   {
      case 1:
         menu_animation_push(menu->animation,
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, NULL);
         menu_animation_push(menu->animation,
               XMB_DELAY, 0, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, NULL);
         break;
      case 2:
         menu_animation_push(menu->animation,
               XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, NULL);
         menu_animation_push(menu->animation,
               XMB_DELAY, 1, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, NULL);
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
      xmb->prevent_populate = false;
      return;
   }

   xmb_set_title(xmb);

   if (menu->categories.selection_ptr != xmb->categories.active.idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);
}

static GLuint xmb_icon_get_type(xmb_handle_t *xmb,
      xmb_node_t *core_node, unsigned type)
{
   switch(type)
   {
      case MENU_FILE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER].id;
      case MENU_FILE_PLAIN:
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_PLAYLIST_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_CONTENTLIST_ENTRY:
         if (core_node)
            return core_node->content_icon;
         return xmb->textures.list[XMB_TEXTURE_FILE].id;
      case MENU_FILE_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_ZIP].id;
      case MENU_FILE_CORE:
         return xmb->textures.list[XMB_TEXTURE_CORE].id;
      case MENU_FILE_RDB:
         return xmb->textures.list[XMB_TEXTURE_RDB].id;
      case MENU_FILE_CURSOR:
         return xmb->textures.list[XMB_TEXTURE_CURSOR].id;
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN].id;
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
   unsigned i;
   math_matrix_4x4 mymat, mrot, mscal;
   core_info_t *info     = NULL;
   const char *label     = NULL;
   xmb_node_t *core_node = NULL;
   size_t end            = 0;
   global_t *global      = global_get_ptr();
   uint64_t frame_count  = video_driver_get_frame_count();

   if (!list || !list->size)
      return;

   file_list_get_last(stack, NULL, &label, NULL);

   if (cat_selection_ptr)
      core_node = xmb_get_userdata_from_core(xmb, info, cat_selection_ptr - 1);

   end = file_list_get_size(list);

   matrix_4x4_rotate_z(&mrot, 0 /* rotation */);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, 1 /* scale_factor */, 1 /* scale_factor */, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   for (i = 0; i < end; i++)
   {
      menu_entry_t entry;
      float icon_x, icon_y;
      char name[PATH_MAX_LENGTH], value[PATH_MAX_LENGTH];
      GLuint texture_switch = 0;
      GLuint         icon   = 0;
      xmb_node_t *   node   = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);
      menu_handle_t *menu   = menu_driver_get_ptr();

      if (!node)
         continue;
      
      icon_x = node->x + xmb->margins.screen.left + 
         xmb->icon.spacing.horizontal - xmb->icon.size / 2.0;
      icon_y = xmb->margins.screen.top + node->y + xmb->icon.size / 2.0;

      if (
            icon_x < -xmb->icon.size / 2 || 
            icon_x > global->video_data.width ||
            icon_y < xmb->icon.size / 2 ||
            icon_y > global->video_data.height + xmb->icon.size)
         continue;

      menu_entry_get(&entry, i, list, true);

      if (entry.type == MENU_FILE_CONTENTLIST_ENTRY)
         strlcpy(entry.path, path_basename(entry.path), sizeof(entry.path));

      icon = xmb_icon_get_type(xmb, core_node, entry.type);

      if (!strcmp(entry.label, "core_options"))
         icon = xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].id;
      else if (!strcmp(entry.label, "core_information"))
         icon = xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
      else if (!strcmp(entry.label, "core_input_remapping_options"))
         icon = xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].id;
      else if (!strcmp(entry.label, "core_cheat_options"))
         icon = xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].id;
      else if (!strcmp(entry.label, "core_disk_options"))
         icon = xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].id;
      else if (!strcmp(entry.label, "savestate"))
         icon = xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
      else if (!strcmp(entry.label, "loadstate"))
         icon = xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
      else if (!strcmp(entry.label, "take_screenshot"))
         icon = xmb->textures.list[XMB_TEXTURE_SCREENSHOT].id;
      else if (!strcmp(entry.label, "restart_content"))
         icon = xmb->textures.list[XMB_TEXTURE_RELOAD].id;
      else if (!strcmp(entry.label, "resume_content"))
         icon = xmb->textures.list[XMB_TEXTURE_RESUME].id;


      menu_animation_ticker_line(name, 35,
            frame_count / 20, entry.path,
            (i == current));

      xmb_draw_text(menu, xmb, name,
            node->x + xmb->margins.screen.left + 
            xmb->icon.spacing.horizontal + xmb->margins.label.left, 
            xmb->margins.screen.top + node->y + xmb->margins.label.top, 
            1, node->label_alpha, TEXT_ALIGN_LEFT);

      menu_animation_ticker_line(value, 35,
            frame_count / 20, entry.value,
            (i == current));

      if((     strcmp(entry.value, "...")
            && strcmp(entry.value, "(CORE)")
            && strcmp(entry.value, "(RDB)")
            && strcmp(entry.value, "(CURSOR)")
            && strcmp(entry.value, "(FILE)")
            && strcmp(entry.value, "(DIR)")
            && strcmp(entry.value, "(COMP)")
            && strcmp(entry.value, "ON")
            && strcmp(entry.value, "OFF"))
            || ((!strcmp(entry.value, "ON")
            && !xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
            || (!strcmp(entry.value, "OFF")
            && !xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)))
         xmb_draw_text(menu, xmb, value,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal + 
               xmb->margins.label.left + xmb->margins.setting.left, 
               xmb->margins.screen.top + node->y + xmb->margins.label.top, 
               1, 
               node->label_alpha,
               TEXT_ALIGN_LEFT);


      xmb_draw_icon_begin(gl);

      xmb_draw_icon(gl, xmb, icon, icon_x, icon_y, node->alpha, 0, node->zoom);

      if (!strcmp(entry.value, "ON") && xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id;

      if (!strcmp(entry.value, "OFF") && xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id;

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
   struct gl_coords coords;
   math_matrix_4x4 mymat, mrot;
   global_t *global = global_get_ptr();

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
   };

   glViewport(x, global->video_data.height - y, xmb->cursor.size, xmb->cursor.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;

   matrix_4x4_rotate_z(&mrot, 0);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   xmb_draw_icon_begin(gl);

   menu_gl_draw_frame(gl->shader, &coords, &mymat, true, xmb->textures.list[XMB_TEXTURE_POINTER].id);
}

static void xmb_render(void)
{
   unsigned i, current, end;
   settings_t *settings = config_get_ptr();


   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   xmb_handle_t *xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   menu_animation_update(menu->animation, menu->dt / IDEAL_DT);

   current = menu->navigation.selection_ptr;
   end     = menu_list_get_size(menu->menu_list);

   if (settings->menu.pointer.enable)
   {
      for (i = 0; i < end; i++)
      {
         float item_y = xmb->margins.screen.top + xmb_item_y(xmb, i, current);

         if (menu->pointer.y > item_y 
               && menu->pointer.y < item_y + xmb->icon.size)
            menu->pointer.ptr = i;
      }
   }

   if (settings->menu.mouse.enable)
   {
      for (i = 0; i < end; i++)
      {
         float item_y = xmb->margins.screen.top + xmb_item_y(xmb, i, current);

         if (menu->mouse.y > item_y && menu->mouse.y < item_y + xmb->icon.size)
            menu->mouse.ptr = i;
      }
   }

   menu->animation_is_active = false;
   menu->label.is_updated    = false;
}

static void xmb_frame(void)
{
   math_matrix_4x4 mymat, mrot, mscal;
   unsigned i, depth;
   char msg[PATH_MAX_LENGTH];
   char title_msg[PATH_MAX_LENGTH], timedate[PATH_MAX_LENGTH];
   bool render_background = false;
   xmb_handle_t *xmb = NULL;
   gl_t *gl = NULL;
   const struct font_renderer *font_driver = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (!menu)
      return;
    
   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   menu_display_font_bind_block(menu, font_driver, &xmb->raster_block);

   xmb->raster_block.carr.coords.vertices = 0;

   gl_menu_frame_background(menu, settings, gl, xmb->textures.bg.id, xmb->alpha, 0.75f, false);

   xmb_draw_text(menu, xmb,
         xmb->title_name, xmb->margins.title.left,
         xmb->margins.title.top, 1, 1, TEXT_ALIGN_LEFT);

   if (settings->menu.timedate_enable)
   {
      disp_timedate_set_label(timedate, sizeof(timedate), 0);

      xmb_draw_text(menu, xmb, timedate,
            global->video_data.width - xmb->margins.title.left - xmb->icon.size / 4, 
            xmb->margins.title.top, 1, 1, TEXT_ALIGN_RIGHT);
   }

   if (settings->menu.core_enable)
   {
      get_core_title(title_msg, sizeof(title_msg));
      xmb_draw_text(menu, xmb, title_msg, xmb->margins.title.left, 
            global->video_data.height - xmb->margins.title.bottom, 1, 1, TEXT_ALIGN_LEFT);
   }

   depth = file_list_get_size(menu->menu_list->menu_stack);

   xmb_draw_items(xmb, gl,
         xmb->selection_buf_old,
         xmb->menu_stack_old,
         xmb->selection_ptr_old,
         depth > 1 ? menu->categories.selection_ptr :
                     xmb->categories.selection_ptr_old);

   xmb_draw_items(xmb, gl,
         menu->menu_list->selection_buf,
         menu->menu_list->menu_stack,
         menu->navigation.selection_ptr,
         menu->categories.selection_ptr);

   matrix_4x4_rotate_z(&mrot, 0 /* rotation */);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, 1 /* scale_factor */, 1 /* scale_factor */, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   xmb_draw_icon_begin(gl);

   if (settings->menu.timedate_enable)
      xmb_draw_icon_predone(gl, xmb, &mymat, xmb->textures.list[XMB_TEXTURE_CLOCK].id,
            global->video_data.width - xmb->icon.size, xmb->icon.size, 1, 0, 1);

   xmb_draw_icon_predone(gl, xmb, &mymat, xmb->textures.list[XMB_TEXTURE_ARROW].id,
         xmb->x + xmb->margins.screen.left + 
         xmb->icon.spacing.horizontal - xmb->icon.size / 2.0 + xmb->icon.size,
         xmb->margins.screen.top + 
         xmb->icon.size / 2.0 + xmb->icon.spacing.vertical 
         * xmb->item.active.factor,
         xmb->textures.arrow.alpha, 0, 1);

   for (i = 0; i < menu->categories.size; i++)
   {
      core_info_t *info           = NULL;
      xmb_node_t *node = &xmb->settings_node;
      
      if (i > 0)
         node = xmb_get_userdata_from_core(xmb, info, i - 1);

      if (node)
      {
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

   menu_display_font_flush_block(menu, font_driver);

   if (menu->keyboard.display)
   {
      const char *str = *menu->keyboard.buffer;

      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s",
            menu->keyboard.label, str);
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
      gl_menu_frame_background(menu, settings, gl, xmb->textures.bg.id, xmb->alpha, 0.75f, true);
      xmb_frame_messagebox(msg);
   }

   if (settings->menu.mouse.enable)
      xmb_draw_cursor(gl, xmb, menu->mouse.x, menu->mouse.y);

   menu_display_unset_viewport(menu);
}

static void *xmb_init(void)
{
   menu_handle_t *menu                = NULL;
   xmb_handle_t *xmb                  = NULL;
   const video_driver_t *video_driver = NULL;
   float scale_factor                 = 1;
   global_t *global                   = global_get_ptr();
   gl_t *gl                           = (gl_t*)
      video_driver_get_ptr(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize XMB menu driver: GL video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

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

   menu->frame_buf.width  = global->video_data.width;
   menu->frame_buf.height = global->video_data.height;

   if (global->video_data.width >= 3840)
      scale_factor              = 2.0;
   else if (global->video_data.width >= 2560)
      scale_factor              = 1.5;
   else if (global->video_data.width >= 1920)
      scale_factor              = 1.0;
   else if (global->video_data.width >= 1280)
      scale_factor              = 0.75;
   else if (global->video_data.width >=  640)
      scale_factor              = 0.5;
   else if (global->video_data.width >=  320)
      scale_factor              = 0.25;

   strlcpy(xmb->icon.dir, "256", sizeof(xmb->icon.dir));

   xmb->icon.size               = 128.0 * scale_factor;
   xmb->cursor.size             = 48.0;
   menu->font.size              = 32.0 * scale_factor;
   xmb->icon.spacing.horizontal = 200.0 * scale_factor;
   xmb->icon.spacing.vertical   = 64.0 * scale_factor;
   xmb->margins.screen.left     = 336.0 * scale_factor;
   xmb->margins.screen.top      = (256+32) * scale_factor;
   xmb->margins.title.left      = 60 * scale_factor;
   xmb->margins.title.top       = 60 * scale_factor + menu->font.size/3;
   xmb->margins.title.bottom    = 60 * scale_factor - menu->font.size/3;
   xmb->margins.label.left      = 85.0 * scale_factor;
   xmb->margins.label.top       = menu->font.size/3.0;
   xmb->margins.setting.left    = 600.0 * scale_factor;

   menu->categories.size      = 1;
   menu->header_height = xmb->icon.size;

   if (global->core_info)
      menu->categories.size   = global->core_info->count + 1;

   return menu;

error:
   if (menu)
      free(menu);
   if (xmb && xmb->menu_stack_old)
      free(xmb->menu_stack_old);
   if (xmb && xmb->selection_buf_old)
      free(xmb->selection_buf_old);
   return NULL;
}

static void xmb_free(void *data)
{
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;
   driver_t *driver    = driver_get_ptr();
   const struct font_renderer *font_driver = 
      (const struct font_renderer*)driver->font_osd_driver;

   if (menu && menu->userdata)
   {
      xmb = (xmb_handle_t*)menu->userdata;

      if (!xmb)
         return;

      free(xmb->menu_stack_old);
      free(xmb->selection_buf_old);

      gl_coord_array_free(&xmb->raster_block.carr);

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

static bool xmb_load_wallpaper(void *data)
{
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return false;
   
   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return false;
   if (!data)
      return false;

   xmb_context_bg_destroy(xmb);

   xmb->textures.bg.id   = video_texture_load(data,
         TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);


   return true;
}

static void xmb_context_reset(void)
{
   unsigned i, k;
   char bgpath[PATH_MAX_LENGTH];
   char mediapath[PATH_MAX_LENGTH], themepath[PATH_MAX_LENGTH],
        iconpath[PATH_MAX_LENGTH],  fontpath[PATH_MAX_LENGTH],
        core_id[PATH_MAX_LENGTH],   texturepath[PATH_MAX_LENGTH],
        content_texturepath[PATH_MAX_LENGTH];

   struct texture_image ti     = {0};
   core_info_list_t* info_list = NULL;
   gl_t *gl                    = NULL;
   xmb_handle_t *xmb           = NULL;
   xmb_node_t *node            = NULL;
   menu_handle_t *menu         = menu_driver_get_ptr();
   settings_t *settings        = config_get_ptr();
   global_t   *global          = global_get_ptr();

   if (!menu)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);
   if (!gl)
      return;

   xmb = (xmb_handle_t*)menu->userdata;
   if (!xmb)
      return;

   fill_pathname_join(bgpath, settings->assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   fill_pathname_join(mediapath, settings->assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   fill_pathname_join(fontpath, themepath, "font.ttf", sizeof(fontpath));

   if (!menu_display_init_main_font(menu, fontpath, menu->font.size))
      RARCH_WARN("Failed to load font.");

   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(xmb->textures.list[XMB_TEXTURE_SETTINGS].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(xmb->textures.list[XMB_TEXTURE_SETTING].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(xmb->textures.list[XMB_TEXTURE_SUBSETTING].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(xmb->textures.list[XMB_TEXTURE_ARROW].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(xmb->textures.list[XMB_TEXTURE_RUN].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(xmb->textures.list[XMB_TEXTURE_RESUME].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(xmb->textures.list[XMB_TEXTURE_SAVESTATE].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(xmb->textures.list[XMB_TEXTURE_LOADSTATE].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CORE_INFO].path, iconpath,
         "core-infos.png", sizeof(xmb->textures.list[XMB_TEXTURE_CORE_INFO].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].path, iconpath,
         "core-options.png", sizeof(xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].path, iconpath,
         "core-input-remapping-options.png", sizeof(xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].path, iconpath,
         "core-cheat-options.png", sizeof(xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].path, iconpath,
         "core-disk-options.png", sizeof(xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(xmb->textures.list[XMB_TEXTURE_SCREENSHOT].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(xmb->textures.list[XMB_TEXTURE_RELOAD].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_FILE].path, iconpath,
         "file.png", sizeof(xmb->textures.list[XMB_TEXTURE_FILE].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_FOLDER].path, iconpath,
         "folder.png", sizeof(xmb->textures.list[XMB_TEXTURE_FOLDER].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_ZIP].path, iconpath,
         "zip.png", sizeof(xmb->textures.list[XMB_TEXTURE_ZIP].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CORE].path, iconpath,
         "core.png", sizeof(xmb->textures.list[XMB_TEXTURE_CORE].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_RDB].path, iconpath,
         "database.png", sizeof(xmb->textures.list[XMB_TEXTURE_RDB].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CURSOR].path, iconpath,
         "cursor.png", sizeof(xmb->textures.list[XMB_TEXTURE_CURSOR].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SWITCH_ON].path, iconpath,
         "on.png", sizeof(xmb->textures.list[XMB_TEXTURE_SWITCH_ON].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].path, iconpath,
         "off.png", sizeof(xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_CLOCK].path, iconpath,
         "clock.png", sizeof(xmb->textures.list[XMB_TEXTURE_CLOCK].path));
   fill_pathname_join(xmb->textures.list[XMB_TEXTURE_POINTER].path, iconpath,
         "pointer.png", sizeof(xmb->textures.list[XMB_TEXTURE_POINTER].path));

   for (k = 0; k < XMB_TEXTURE_LAST; k++)
   {
      const char *path = xmb->textures.list[k].path;
      if (! path_file_exists(path))
         continue;

      texture_image_load(&ti, path);

      xmb->textures.list[k].id   = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }

   {
      char path[PATH_MAX_LENGTH];

      fill_pathname_join(path, iconpath,
            "bg.png", sizeof(path));

      if (*settings->menu.wallpaper)
         strlcpy(path, settings->menu.wallpaper,
               sizeof(path));

      if ( path_file_exists(path))
         rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, path, "cb_menu_wallpaper", 0, 1, true);
   }

   xmb->settings_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS].id;
   xmb->settings_node.alpha = xmb->categories.active.alpha;
   xmb->settings_node.zoom  = xmb->categories.active.zoom;

   info_list = (core_info_list_t*)global->core_info;

   if (!info_list)
      return;

   for (i = 1; i < menu->categories.size; i++)
   {
      core_info_t *info           = NULL;
      struct texture_image ti = {0};
      node = xmb_get_userdata_from_core(xmb, info, i - 1);

      if (!node)
      {
         node = xmb_node_allocate_userdata(xmb, info, i - 1);
         if (!node)
            continue;
      }

      fill_pathname_join(mediapath, settings->assets_directory,
            "lakka", sizeof(mediapath));
      fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
      fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      info = (core_info_t*)&info_list->list[i-1];

      if (!info)
         continue;

      if (info->systemname)
      {
         char *tmp = string_replace_substring(info->systemname, "/", " ");

         if (tmp)
         {
            strlcpy(core_id, tmp, sizeof(core_id));
            free(tmp);
         }
      }
      else
         strlcpy(core_id, "default", sizeof(core_id));

      strlcpy(texturepath, iconpath, sizeof(texturepath));
      strlcat(texturepath, core_id, sizeof(texturepath));
      strlcat(texturepath, ".png", sizeof(texturepath));

      strlcpy(content_texturepath, iconpath, sizeof(content_texturepath));
      strlcat(content_texturepath, core_id, sizeof(content_texturepath));
      strlcat(content_texturepath, "-content.png", sizeof(content_texturepath));

      node->alpha        = 0;
      node->zoom         = xmb->categories.passive.zoom;

      texture_image_load(&ti, texturepath);

      node->icon         = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);

      texture_image_load(&ti, content_texturepath);

      node->content_icon = video_texture_load(&ti,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);

      if (i == xmb->categories.active.idx)
      {
         node->alpha = xmb->categories.active.alpha;
         node->zoom  = xmb->categories.active.zoom;
      }
      else if (xmb->depth <= 1)
         node->alpha = xmb->categories.passive.alpha;
   }
}

static void xmb_navigation_clear(bool pending_push)
{
   if (!pending_push)
      xmb_selection_pointer_changed();
}

static void xmb_navigation_decrement(void)
{
   xmb_selection_pointer_changed();
}

static void xmb_navigation_increment(void)
{
   xmb_selection_pointer_changed();
}

static void xmb_navigation_set(bool scroll)
{
   xmb_selection_pointer_changed();
}

static void xmb_navigation_set_last(void)
{
   xmb_selection_pointer_changed();
}

static void xmb_navigation_descend_alphabet(size_t *unused)
{
   xmb_selection_pointer_changed();
}

static void xmb_navigation_ascend_alphabet(size_t *unused)
{
   xmb_selection_pointer_changed();
}

static void xmb_list_insert(file_list_t *list,
      const char *path, const char *unused, size_t list_size)
{
   int current = 0, i = list_size;
   xmb_node_t *node = NULL;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;
   
   xmb = (xmb_handle_t*)menu->userdata;

   if (!list || !xmb)
      return;

   list->list[i].userdata = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!list->list[i].userdata)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   node = (xmb_node_t*)list->list[i].userdata;

   if (!node)
      return;

   current           = menu->navigation.selection_ptr;

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
}

static void xmb_list_delete(file_list_t *list,
      size_t idx, size_t list_size)
{
   if (!list)
      return;

   if (list->list[idx].userdata)
      free(list->list[idx].userdata);
   list->list[idx].userdata = NULL;
}

static void xmb_list_cache(bool horizontal, unsigned action)
{
   size_t stack_size;
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   file_list_copy(menu->menu_list->selection_buf, xmb->selection_buf_old);
   file_list_copy(menu->menu_list->menu_stack, xmb->menu_stack_old);
   xmb->selection_ptr_old = menu->navigation.selection_ptr;

   if(!horizontal)
      return;

   xmb->categories.selection_ptr_old = menu->categories.selection_ptr;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         menu->categories.selection_ptr--;
         break;
      default:
         menu->categories.selection_ptr++;
         break;
   }

   stack_size = menu->menu_list->menu_stack->size;

   if (menu->menu_list->menu_stack->list[stack_size - 1].label)
	   free(menu->menu_list->menu_stack->list[stack_size - 1].label);
   menu->menu_list->menu_stack->list[stack_size - 1].label = NULL;

   if (menu->categories.selection_ptr == 0)
   {
	   menu->menu_list->menu_stack->list[stack_size - 1].label = 
         strdup("Main Menu");
	   menu->menu_list->menu_stack->list[stack_size - 1].type = 
      MENU_SETTINGS;
   }
   else
   {
	   menu->menu_list->menu_stack->list[stack_size - 1].label = 
         strdup("Horizontal Menu");
	   menu->menu_list->menu_stack->list[stack_size - 1].type = 
      MENU_SETTING_HORIZONTAL_MENU;
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

   for (i = 1; i < menu->categories.size; i++)
   {
      core_info_t *info           = NULL;
      xmb_node_t *node = xmb_get_userdata_from_core(xmb, info, i - 1);

      if (!node)
         continue;

      glDeleteTextures(1, &node->icon);
      glDeleteTextures(1, &node->content_icon);
   }

   menu_display_free_main_font(menu);
}

static void xmb_toggle(bool menu_on)
{
   unsigned i;
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   xmb->depth = file_list_get_size(menu->menu_list->menu_stack);

   if (!menu_on)
   {
      xmb->alpha = 0;
      return;
   }

   menu_animation_push(menu->animation, XMB_DELAY, 1.0f,
         &xmb->alpha, EASING_IN_OUT_QUAD, NULL);

   xmb->prevent_populate = !menu->need_refresh;

   for (i = 0; i < menu->categories.size; i++)
   {
      core_info_t *info           = NULL;
      xmb_node_t *node = &xmb->settings_node;
      
      if (i > 0)
         node = xmb_get_userdata_from_core(xmb, info, i - 1);

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
   xmb_navigation_decrement,
   xmb_navigation_increment,
   xmb_navigation_set,
   xmb_navigation_set_last,
   xmb_navigation_descend_alphabet,
   xmb_navigation_ascend_alphabet,
   xmb_list_insert,
   xmb_list_delete,
   NULL,
   xmb_list_cache,
   NULL,
   xmb_load_wallpaper,
   "xmb",
};
