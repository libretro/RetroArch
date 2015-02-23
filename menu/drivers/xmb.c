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
#include "../menu_animation.h"
#include "../menu_texture.h"

#include <file/file_path.h>
#include "../../gfx/gl_common.h"
#include "../../gfx/video_thread_wrapper.h"
#include <compat/posix_string.h>

#include "shared.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 0.02
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
      struct
      {
         float zoom;
         float alpha;
         int idx;
         int idx_old;
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

   struct
   {
      void *buf;
      int size;
   } font;

   xmb_node_t settings_node;
   bool prevent_populate;
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

   if (i < current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + xmb->above_offset.subitem);
      else
         iy *= (i - (int)current + xmb->above_offset.item);
   else
      iy    *= (i - (int)current + xmb->under_offset.item);

   if (i == current)
      iy = xmb->icon.spacing.vertical * xmb->item.active.factor;

   return iy;
}

static int xmb_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;
   
   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu->menu_list->selection_buf,
            menu->navigation.selection_ptr);

   menu_list_get_last_stack(menu->menu_list, NULL, &label, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);
   
   return -1;
}

static char *xmb_str_replace (const char *string,
      const char *substr, const char *replacement)
{
   char *tok, *newstr, *oldstr, *head;

   /* if either substr or replacement is NULL, 
    * duplicate string a let caller handle it. */
   if (!substr || !replacement)
      return strdup(string);

   newstr = strdup(string);
   head   = newstr;

   while ( (tok = strstr ( head, substr )))
   {
      oldstr = newstr;
      newstr = (char*)malloc(
            strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);

      if (!newstr)
      {
         /* Failed to allocate memory,
          * free old string and return NULL. */
         free (oldstr);
         return NULL;
      }

      memcpy(newstr, oldstr, tok - oldstr );
      memcpy(newstr + (tok - oldstr), replacement, strlen ( replacement ) );
      memcpy(newstr + (tok - oldstr) + strlen( replacement ), tok +
            strlen ( substr ), strlen ( oldstr ) -
            strlen ( substr ) - ( tok - oldstr ) );
      memset(newstr + strlen ( oldstr ) - strlen ( substr ) +
            strlen ( replacement ) , 0, 1 );

      /* Move back head right after the last replacement. */
      head = newstr + (tok - oldstr) + strlen( replacement );
      free (oldstr);
   }

   return newstr;
}

static void xmb_draw_icon(gl_t *gl, xmb_handle_t *xmb,
      GLuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gl_coords coords;
   math_matrix_4x4 mymat, mrot, mscal;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   if (
         x < -xmb->icon.size/2 || 
         x > gl->win_width ||
         y < xmb->icon.size/2 ||
         y > gl->win_height + xmb->icon.size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, xmb->icon.size, xmb->icon.size);

   coords.vertices      = 4;
   coords.vertex        = rmb_vertex;
   coords.tex_coord     = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color         = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   matrix_4x4_rotate_z(&mrot, rotation);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
}

static void xmb_draw_text(gl_t *gl, xmb_handle_t *xmb, const char *str, float x,
      float y, float scale_factor, float alpha, bool align_right)
{
   uint8_t a8 = 0;
   struct font_params params = {0};

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8 = 255 * alpha;

   if (a8 == 0)
      return;

   if (x < -xmb->icon.size || x > gl->win_width + xmb->icon.size
         || y < -xmb->icon.size || y > gl->win_height + xmb->icon.size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   params.x           = x / gl->win_width;
   params.y           = 1.0f - y / gl->win_height;

   params.scale       = scale_factor;
   params.color       = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;
   params.align_right = align_right;

   if (driver.video_data && driver.video_poke
       && driver.video_poke->set_osd_msg)
       driver.video_poke->set_osd_msg(driver.video_data,
                                      str, &params, xmb->font.buf);
}

static void xmb_render_background(gl_t *gl, xmb_handle_t *xmb,
      bool force_transparency)
{
   struct gl_coords coords;
   float alpha              = 0.75f;
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

   glViewport(0, 0, gl->win_width, gl->win_height);

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;

   if ((g_settings.menu.pause_libretro
      || !g_extern.main_is_init || g_extern.libretro_dummy)
      && !force_transparency
      && xmb->textures.bg.id)
   {
      coords.color = color;
      glBindTexture(GL_TEXTURE_2D, xmb->textures.bg.id);
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

static void xmb_get_message(const char *message)
{
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;
   
   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_messagebox(const char *message)
{
   int x, y;
   unsigned i;
   struct string_list *list = NULL;
   gl_t *gl = NULL;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);

   if (!gl)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   x = gl->win_width / 2 - strlen(list->elems[0].data) * xmb->font.size / 4;
   y = gl->win_height / 2 - list->size * xmb->font.size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(gl, xmb, msg, x, y + i * xmb->font.size, 1, 1, 0);
   }

end:
   string_list_free(list);
}

static void xmb_selection_pointer_changed(void)
{
   int i;
   unsigned current, end;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

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

      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->label_alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, iz, &node->zoom,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, iy, &node->y,     EASING_IN_OUT_QUAD, NULL);
   }
}

static void xmb_list_open_old(xmb_handle_t *xmb, file_list_t *list, int dir, size_t current)
{
   int i;
   menu_handle_t *menu = menu_driver_resolve();

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

      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, 0, &node->label_alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, xmb->icon.size * dir * -2, &node->x, EASING_IN_OUT_QUAD, NULL);
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb, file_list_t *list, int dir, size_t current)
{
   int i;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

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

      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, 0, &node->x, EASING_IN_OUT_QUAD, NULL);
   }

   xmb->old_depth = xmb->depth;
}

static xmb_node_t* xmb_get_userdata_from_core(xmb_handle_t *xmb, int i)
{
   core_info_t *info           = NULL;
   xmb_node_t *node            = NULL;
   core_info_list_t *info_list = (core_info_list_t*)g_extern.core_info;

   if (!info_list)
      return NULL;

   if (!info_list->count)
      return NULL;

   info = (core_info_t*)&info_list->list[i];

   if (!info)
      return NULL;

   node = (xmb_node_t*)info->userdata;

   if (node)
      return node;

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

static void xmb_push_animations(xmb_node_t *node, float ia, float ix)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return;

   menu_animation_push(menu->animation, XMB_DELAY, ia, &node->alpha,  EASING_IN_OUT_QUAD, NULL);
   menu_animation_push(menu->animation, XMB_DELAY, ia, &node->label_alpha,  EASING_IN_OUT_QUAD, NULL);
   menu_animation_push(menu->animation, XMB_DELAY, ix, &node->x, EASING_IN_OUT_QUAD, NULL);
}

static void xmb_list_switch_old(xmb_handle_t *xmb, file_list_t *list, int dir, size_t current)
{
   int i;
   size_t end = 0;
   menu_handle_t *menu = menu_driver_resolve();

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

static void xmb_list_switch_new(xmb_handle_t *xmb, file_list_t *list, int dir, size_t current)
{
   int i;
   size_t end = 0;
   menu_handle_t *menu = menu_driver_resolve();

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
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   if (menu->categories.selection_ptr == 0)
   {
      const char *dir   = NULL;
      const char *label = NULL;
      unsigned menu_type = 0;

      menu_list_get_last_stack(menu->menu_list, &dir, &label, &menu_type);
      get_title(label, dir, menu_type, xmb->title_name, sizeof(xmb->title_name));
   }
   else
   {
      core_info_t *info = NULL;
      core_info_list_t *info_list = (core_info_list_t*)g_extern.core_info;

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
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   if (menu->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb->categories.active.idx += dir;

   for (j = 0; j < menu->categories.size; j++)
   {
      float ia         = xmb->categories.passive.alpha;
      float iz         = xmb->categories.passive.zoom;
      xmb_node_t *node = j ? xmb_get_userdata_from_core(xmb, j - 1) : &xmb->settings_node;

      if (!node)
         continue;
      
      if (j == xmb->categories.active.idx)
      {
         ia = xmb->categories.active.alpha;
         iz = xmb->categories.active.zoom;
      }

      menu_animation_push(menu->animation, XMB_DELAY, ia, &node->alpha, EASING_IN_OUT_QUAD, NULL);
      menu_animation_push(menu->animation, XMB_DELAY, iz, &node->zoom, EASING_IN_OUT_QUAD, NULL);
   }

   menu_animation_push(menu->animation, XMB_DELAY,
         xmb->icon.spacing.horizontal * -(float)menu->categories.selection_ptr,
         &xmb->categories.x_pos, EASING_IN_OUT_QUAD, NULL);

   dir = -1;
   if (menu->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old, dir, xmb->selection_ptr_old);
   xmb_list_switch_new(xmb, menu->menu_list->selection_buf, dir, menu->navigation.selection_ptr);
   xmb->categories.active.idx_old = menu->categories.selection_ptr;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   unsigned j;
   int dir = 0;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   xmb->depth = file_list_get_size(menu->menu_list->menu_stack);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;

   for (j = 0; j < menu->categories.size; j++)
   {
      float ia = 0;
      xmb_node_t *node = j ? xmb_get_userdata_from_core(xmb, j - 1) : &xmb->settings_node;

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
         ia = xmb->categories.active.alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories.passive.alpha;

      menu_animation_push(menu->animation, XMB_DELAY, ia,
            &node->alpha, EASING_IN_OUT_QUAD, NULL);
   }

   xmb_list_open_old(xmb, xmb->selection_buf_old, dir, xmb->selection_ptr_old);
   xmb_list_open_new(xmb, menu->menu_list->selection_buf, dir, menu->navigation.selection_ptr);

   switch (xmb->depth)
   {
      case 1:
         menu_animation_push(menu->animation, XMB_DELAY, xmb->icon.size * -(xmb->depth*2-2),
               &xmb->x, EASING_IN_OUT_QUAD, NULL);
         menu_animation_push(menu->animation, XMB_DELAY, 0, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, NULL);
         break;
      case 2:
         menu_animation_push(menu->animation, XMB_DELAY,
               xmb->icon.size * -(xmb->depth*2-2), &xmb->x, EASING_IN_OUT_QUAD, NULL);
         menu_animation_push(menu->animation, XMB_DELAY, 1, &xmb->textures.arrow.alpha,
               EASING_IN_OUT_QUAD, NULL);
         break;
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb   = NULL;
   menu_handle_t *menu = menu_driver_resolve();

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

static void xmb_draw_items(xmb_handle_t *xmb, gl_t *gl,
      file_list_t *list, file_list_t *stack,
      size_t current, size_t cat_selection_ptr)
{
   unsigned i;
   const char *label     = NULL;
   xmb_node_t *core_node = NULL;
   size_t end            = 0;

   if (!list || !list->size)
      return;

   file_list_get_last(stack, NULL, &label, NULL);

   if (xmb->categories.active.idx)
      core_node = xmb_get_userdata_from_core(xmb, cat_selection_ptr - 1);

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      float icon_x, icon_y;
      char type_str[PATH_MAX_LENGTH], path_buf[PATH_MAX_LENGTH];
      char name[PATH_MAX_LENGTH], value[PATH_MAX_LENGTH];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      menu_file_list_cbs_t *cbs = NULL;
      GLuint icon = 0;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;
      
      icon_x = node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal - xmb->icon.size / 2.0;
      icon_y = xmb->margins.screen.top + node->y + xmb->icon.size / 2.0;

      if (
            icon_x < -xmb->icon.size / 2 || 
            icon_x > gl->win_width ||
            icon_y < xmb->icon.size / 2 ||
            icon_y > gl->win_height + xmb->icon.size)
         continue;

      menu_list_get_at_offset(list, i, &path, &entry_label, &type);

      cbs = (menu_file_list_cbs_t*)menu_list_get_actiondata_at_offset(list, i);

      if (cbs && cbs->action_get_representation)
         cbs->action_get_representation(list,
               &w, type, i, label,
               type_str, sizeof(type_str), 
               entry_label, path,
               path_buf, sizeof(path_buf));

      if (type == MENU_FILE_CONTENTLIST_ENTRY)
         strlcpy(path_buf, path_basename(path_buf), sizeof(path_buf));

      switch(type)
      {
         case MENU_FILE_DIRECTORY:
            icon = xmb->textures.list[XMB_TEXTURE_FOLDER].id;
            break;
         case MENU_FILE_PLAIN:
            icon = xmb->textures.list[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_PLAYLIST_ENTRY:
            icon = xmb->textures.list[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_CONTENTLIST_ENTRY:
            icon = xmb->textures.list[XMB_TEXTURE_FILE].id;
            if (core_node)
               icon = core_node->content_icon;
            break;
         case MENU_FILE_CARCHIVE:
            icon = xmb->textures.list[XMB_TEXTURE_ZIP].id;
            break;
         case MENU_FILE_CORE:
            icon = xmb->textures.list[XMB_TEXTURE_CORE].id;
            break;
         case MENU_FILE_RDB:
            icon = xmb->textures.list[XMB_TEXTURE_RDB].id;
            break;
         case MENU_FILE_CURSOR:
            icon = xmb->textures.list[XMB_TEXTURE_CURSOR].id;
            break;
         case MENU_SETTING_ACTION_RUN:
            icon = xmb->textures.list[XMB_TEXTURE_RUN].id;
            break;
         case MENU_SETTING_ACTION_SAVESTATE:
            icon = xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
            break;
         case MENU_SETTING_ACTION_LOADSTATE:
            icon = xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
            break;
         case MENU_SETTING_ACTION_CORE_INFORMATION:
            icon = xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].id;
            break;
         case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].id;
            break;
         case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].id;
            break;
         case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
            icon = xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].id;
            break;
         case MENU_SETTING_ACTION_SCREENSHOT:
            icon = xmb->textures.list[XMB_TEXTURE_SCREENSHOT].id;
            break;
         case MENU_SETTING_ACTION_RESET:
            icon = xmb->textures.list[XMB_TEXTURE_RELOAD].id;
            break;
         case MENU_SETTING_ACTION:
            icon = xmb->textures.list[XMB_TEXTURE_SETTING].id;
            if (xmb->depth == 3)
               icon = xmb->textures.list[XMB_TEXTURE_SUBSETTING].id;
            break;
         case MENU_SETTING_GROUP:
            icon = xmb->textures.list[XMB_TEXTURE_SETTING].id;
            break;
         default:
            icon = xmb->textures.list[XMB_TEXTURE_SUBSETTING].id;
            break;
      }

      if (!strcmp(entry_label, "core_options"))
         icon = xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS].id;
      else if (!strcmp(entry_label, "core_information"))
         icon = xmb->textures.list[XMB_TEXTURE_CORE_INFO].id;
      else if (!strcmp(entry_label, "core_input_remapping_options"))
         icon = xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS].id;
      else if (!strcmp(entry_label, "core_cheat_options"))
         icon = xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS].id;
      else if (!strcmp(entry_label, "core_disk_options"))
         icon = xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS].id;
      else if (!strcmp(entry_label, "savestate"))
         icon = xmb->textures.list[XMB_TEXTURE_SAVESTATE].id;
      else if (!strcmp(entry_label, "loadstate"))
         icon = xmb->textures.list[XMB_TEXTURE_LOADSTATE].id;
      else if (!strcmp(entry_label, "take_screenshot"))
         icon = xmb->textures.list[XMB_TEXTURE_SCREENSHOT].id;
      else if (!strcmp(entry_label, "restart_content"))
         icon = xmb->textures.list[XMB_TEXTURE_RELOAD].id;
      else if (!strcmp(entry_label, "resume_content"))
         icon = xmb->textures.list[XMB_TEXTURE_RESUME].id;

      xmb_draw_icon(gl, xmb, icon, icon_x, icon_y, node->alpha, 0, node->zoom);

      menu_animation_ticker_line(name, 35, g_extern.frame_count / 20, path_buf,
         (i == current));

      xmb_draw_text(gl, xmb, name,
            node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal + xmb->margins.label.left, 
            xmb->margins.screen.top + node->y + xmb->margins.label.top, 
            1, node->label_alpha, 0);

      menu_animation_ticker_line(value, 35, g_extern.frame_count / 20, type_str,
            (i == current));

      if((     strcmp(type_str, "...")
            && strcmp(type_str, "(CORE)")
            && strcmp(type_str, "(RDB)")
            && strcmp(type_str, "(CURSOR)")
            && strcmp(type_str, "(FILE)")
            && strcmp(type_str, "(DIR)")
            && strcmp(type_str, "(COMP)")
            && strcmp(type_str, "ON")
            && strcmp(type_str, "OFF"))
            || ((!strcmp(type_str, "ON")
            && !xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
            || (!strcmp(type_str, "OFF")
            && !xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)))
         xmb_draw_text(gl, xmb, value,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal + 
               xmb->margins.label.left + xmb->margins.setting.left, 
               xmb->margins.screen.top + node->y + xmb->margins.label.top, 
               1, 
               node->label_alpha,
               0);

      if (!strcmp(type_str, "ON") && xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id)
         xmb_draw_icon(gl, xmb, xmb->textures.list[XMB_TEXTURE_SWITCH_ON].id,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal
               + xmb->icon.size / 2.0 + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->icon.size / 2.0,
               node->alpha,
               0,
               1);

      if (!strcmp(type_str, "OFF") && xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id)
         xmb_draw_icon(gl, xmb, xmb->textures.list[XMB_TEXTURE_SWITCH_OFF].id,
               node->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal
               + xmb->icon.size / 2.0 + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->icon.size / 2.0,
               node->alpha,
               0,
               1);
   }
}

static void xmb_frame(void)
{
   int i, depth;
   char title_msg[PATH_MAX_LENGTH], timedate[PATH_MAX_LENGTH];
   const char *core_name = NULL;
   const char *core_version = NULL;
   xmb_handle_t *xmb = NULL;
   gl_t *gl = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);

   if (!gl)
      return;

   menu_animation_update(menu->animation, 0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   xmb_render_background(gl, xmb, false);

   xmb_draw_text(gl, xmb,
         xmb->title_name, xmb->margins.title.left, xmb->margins.title.top, 1, 1, 0);

   if (g_settings.menu.timedate_enable)
   {
      disp_timedate_set_label(timedate, sizeof(timedate), 0);

      xmb_draw_text(gl, xmb,
            timedate, gl->win_width - xmb->margins.title.left - xmb->icon.size / 4, 
            xmb->margins.title.top, 1, 1, 1);

      xmb_draw_icon(gl, xmb, xmb->textures.list[XMB_TEXTURE_CLOCK].id,
            gl->win_width - xmb->icon.size, xmb->icon.size, 1, 0, 1);
   }

   if (g_settings.menu.core_enable)
   {
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
      xmb_draw_text(gl, xmb, title_msg, xmb->margins.title.left, 
            gl->win_height - xmb->margins.title.bottom, 1, 1, 0);
   }

   xmb_draw_icon(gl, xmb, xmb->textures.list[XMB_TEXTURE_ARROW].id,
         xmb->x + xmb->margins.screen.left + xmb->icon.spacing.horizontal - xmb->icon.size / 2.0 + xmb->icon.size,
         xmb->margins.screen.top + xmb->icon.size / 2.0 + xmb->icon.spacing.vertical * xmb->item.active.factor,
         xmb->textures.arrow.alpha, 0, 1);

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

   for (i = 0; i < menu->categories.size; i++)
   {
      xmb_node_t *node = i ? xmb_get_userdata_from_core(xmb, i - 1) : &xmb->settings_node;

      if (node)
         xmb_draw_icon(gl, xmb, node->icon, 
               xmb->x + xmb->categories.x_pos + xmb->margins.screen.left + xmb->icon.spacing.horizontal * (i + 1) - xmb->icon.size / 2.0,
               xmb->margins.screen.top + xmb->icon.size / 2.0, 
               node->alpha, 
               0, 
               node->zoom);
   }

#ifdef GEKKO
   const char *message_queue;

   if (menu->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      menu->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   xmb_render_messagebox(message_queue);
#endif

   if (menu->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char *str = *menu->keyboard.buffer;

      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s",
            menu->keyboard.label, str);
      xmb_render_background(gl, xmb, true);
      xmb_render_messagebox(msg);
   }

   if (xmb->box_message[0] != '\0')
   {
      xmb_render_background(gl, xmb, true);
      xmb_render_messagebox(xmb->box_message);
      xmb->box_message[0] = '\0';
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void *xmb_init(void)
{
   menu_handle_t *menu = NULL;
   xmb_handle_t *xmb = NULL;
   const video_driver_t *video_driver = NULL;
   float scale_factor = 1;
   gl_t *gl = (gl_t*)video_driver_resolve(&video_driver);

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

   if (gl->win_width >= 3840)
      scale_factor              = 2.0;
   else if (gl->win_width >= 2560)
      scale_factor              = 1.5;
   else if (gl->win_width >= 1920)
      scale_factor              = 1.0;
   else if (gl->win_width >= 1280)
      scale_factor              = 0.75;
   else if (gl->win_width >=  640)
      scale_factor              = 0.5;
   else if (gl->win_width >=  320)
      scale_factor              = 0.25;

   strlcpy(xmb->icon.dir, "256", sizeof(xmb->icon.dir));

   xmb->icon.size               = 128.0 * scale_factor;
   xmb->font.size               = 32.0 * scale_factor;
   xmb->icon.spacing.horizontal = 200.0 * scale_factor;
   xmb->icon.spacing.vertical   = 64.0 * scale_factor;
   xmb->margins.screen.left     = 336.0 * scale_factor;
   xmb->margins.screen.top      = (256+32) * scale_factor;
   xmb->margins.title.left      = 60 * scale_factor;
   xmb->margins.title.top       = 60 * scale_factor + xmb->font.size/3;
   xmb->margins.title.bottom    = 60 * scale_factor - xmb->font.size/3;
   xmb->margins.label.left      = 85.0 * scale_factor;
   xmb->margins.label.top       = xmb->font.size/3.0;
   xmb->margins.setting.left    = 600.0 * scale_factor;

   menu->categories.size      = 1;

   if (g_extern.core_info)
      menu->categories.size   = g_extern.core_info->count + 1;

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
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu && menu->userdata)
      free(menu->userdata);
}

static bool xmb_font_init_first(const gl_font_renderer_t **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float xmb_font_size)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;

      if (!thr)
         return false;

      thr->cmd_data.font_init.method = gl_font_init_first;
      thr->cmd_data.font_init.font_driver = font_driver;
      thr->cmd_data.font_init.font_handle = font_handle;
      thr->cmd_data.font_init.video_data = video_data;
      thr->cmd_data.font_init.font_path = font_path;
      thr->cmd_data.font_init.font_size = xmb_font_size;
      thr->send_cmd_func(thr, CMD_FONT_INIT);
      thr->wait_reply_func(thr, CMD_FONT_INIT);

      return thr->cmd_data.font_init.return_value;
   }

   return gl_font_init_first(font_driver, font_handle, video_data,
         font_path, xmb_font_size);
}

static bool xmb_load_wallpaper(const char *path)
{
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return false;
   
   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return false;
   if (!path)
      return false;

   if (xmb->textures.bg.id)
      glDeleteTextures(1, &xmb->textures.bg.id);

   strlcpy(xmb->textures.bg.path, path, sizeof(xmb->textures.bg.path));

   xmb->textures.bg.id   = menu_texture_load(xmb->textures.bg.path,
         TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

   return true;
}

static void xmb_context_reset(void)
{
   int i, k;
   char bgpath[PATH_MAX_LENGTH];
   char mediapath[PATH_MAX_LENGTH], themepath[PATH_MAX_LENGTH], iconpath[PATH_MAX_LENGTH],
         fontpath[PATH_MAX_LENGTH], core_id[PATH_MAX_LENGTH], texturepath[PATH_MAX_LENGTH],
         content_texturepath[PATH_MAX_LENGTH];

   core_info_t* info = NULL;
   core_info_list_t* info_list = NULL;
   gl_t *gl = NULL;
   xmb_handle_t *xmb = NULL;
   xmb_node_t *node = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);
   if (!gl)
      return;

   xmb = (xmb_handle_t*)menu->userdata;
   if (!xmb)
      return;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "lakka", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   fill_pathname_join(fontpath, themepath, "font.ttf", sizeof(fontpath));

   xmb_font_init_first(&gl->font_driver, &xmb->font.buf, gl, fontpath, xmb->font.size);

   if (*g_settings.menu.wallpaper)
      strlcpy(xmb->textures.bg.path, g_settings.menu.wallpaper,
            sizeof(xmb->textures.bg.path));
   else
      fill_pathname_join(xmb->textures.bg.path, iconpath,
            "bg.png", sizeof(xmb->textures.bg.path));
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

   for (k = 0; k < XMB_TEXTURE_LAST; k++)
      xmb->textures.list[k].id   = menu_texture_load(xmb->textures.list[k].path,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);

   xmb_load_wallpaper(xmb->textures.bg.path);

   xmb->settings_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS].id;
   xmb->settings_node.alpha = xmb->categories.active.alpha;
   xmb->settings_node.zoom  = xmb->categories.active.zoom;

   info_list = (core_info_list_t*)g_extern.core_info;

   if (!info_list)
      return;

   for (i = 1; i < menu->categories.size; i++)
   {
      node = xmb_get_userdata_from_core(xmb, i - 1);

      fill_pathname_join(mediapath, g_settings.assets_directory,
            "lakka", sizeof(mediapath));
      fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
      fill_pathname_join(iconpath, themepath, xmb->icon.dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      info = (core_info_t*)&info_list->list[i-1];

      if (!info)
         continue;

      if (info->systemname)
      {
         char *tmp = xmb_str_replace(info->systemname, "/", " ");
         strlcpy(core_id, tmp, sizeof(core_id));
         free(tmp);
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
      node->icon         = menu_texture_load(texturepath,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);
      node->content_icon = menu_texture_load(content_texturepath,
            TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);


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
   menu_handle_t *menu = menu_driver_resolve();

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
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

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

   strlcpy(menu->menu_list->menu_stack->list[stack_size-1].label,
         "Main Menu", PATH_MAX_LENGTH);
   menu->menu_list->menu_stack->list[stack_size-1].type = 
      MENU_SETTINGS;

   if (menu->categories.selection_ptr == 0)
      return;

   strlcpy(menu->menu_list->menu_stack->list[stack_size-1].label,
         "Horizontal Menu", PATH_MAX_LENGTH);
   menu->menu_list->menu_stack->list[stack_size-1].type = 
      MENU_SETTING_HORIZONTAL_MENU;
}

static void xmb_context_destroy(void)
{
   unsigned i;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      glDeleteTextures(1, &xmb->textures.list[i].id);

   for (i = 1; i < menu->categories.size; i++)
   {
      xmb_node_t *node = xmb_get_userdata_from_core(xmb, i - 1);

      if (!node)
         continue;

      glDeleteTextures(1, &node->icon);
      glDeleteTextures(1, &node->content_icon);
   }
}

static void xmb_toggle(bool menu_on)
{
   int i;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = menu_driver_resolve();

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
      xmb_node_t *node = i ? xmb_get_userdata_from_core(xmb, i - 1) : &xmb->settings_node;

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
   xmb_get_message,
   NULL,
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
   xmb_entry_iterate,
   xmb_load_wallpaper,
   "xmb",
};
