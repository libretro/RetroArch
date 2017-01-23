/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include <lists/string_list.h>
#include <gfx/math/matrix_4x4.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef HAVE_DYNAMIC
#include "../../frontend/frontend_driver.h"
#endif

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_display.h"
#include "../menu_navigation.h"

#include "../widgets/menu_entry.h"
#include "../widgets/menu_list.h"
#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_osk.h"

#include "../menu_event.h"

#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../playlist.h"
#include "../../runloop.h"

#include "../../tasks/tasks_internal.h"

#define XMB_RIBBON_ROWS 64
#define XMB_RIBBON_COLS 64
#define XMB_RIBBON_VERTICES 2*XMB_RIBBON_COLS*XMB_RIBBON_ROWS-2*XMB_RIBBON_COLS

#ifndef XMB_DELAY
#define XMB_DELAY 10
#endif

#define BATTERY_LEVEL_CHECK_INTERVAL (30 * 1000000)

#if 0
#define XMB_DEBUG
#endif

typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   uintptr_t icon;
   uintptr_t content_icon;
   char fullpath[4096];
} xmb_node_t;

enum
{
   XMB_TEXTURE_MAIN_MENU = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_HISTORY,
#ifdef HAVE_FFMPEG
   XMB_TEXTURE_MUSICS,
   XMB_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   XMB_TEXTURE_NETPLAY,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_TEXTURE_IMAGES,
#endif
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_CLOSE,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_UNDO,
   XMB_TEXTURE_CORE_INFO,
   XMB_TEXTURE_WIFI,
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
   XMB_TEXTURE_BATTERY_FULL,
   XMB_TEXTURE_BATTERY_CHARGING,
   XMB_TEXTURE_POINTER,
   XMB_TEXTURE_ADD,
   XMB_TEXTURE_KEY,
   XMB_TEXTURE_KEY_HOVER,
   XMB_TEXTURE_LAST
};

enum
{
   XMB_SYSTEM_TAB_MAIN = 0,
   XMB_SYSTEM_TAB_SETTINGS,
   XMB_SYSTEM_TAB_HISTORY,
#ifdef HAVE_FFMPEG
   XMB_SYSTEM_TAB_MUSIC,
   XMB_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_SYSTEM_TAB_IMAGES,
#endif
   XMB_SYSTEM_TAB_ADD,
#ifdef HAVE_NETWORKING
   XMB_SYSTEM_TAB_NETPLAY
#endif
};

#ifdef HAVE_NETWORKING
#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_NETPLAY
#elif defined(HAVE_LIBRETRODB)
#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_ADD
#elif defined(HAVE_IMAGEVIEWER)
#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_IMAGES
#elif defined(HAVE_FFMPEG)
#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_MUSIC
#else
#define XMB_SYSTEM_TAB_END XMB_SYSTEM_TAB_HISTORY
#endif


typedef struct xmb_handle
{
   file_list_t *menu_stack_old;
   file_list_t *selection_buf_old;
   file_list_t *horizontal_list;
   size_t selection_ptr_old;
   int depth;
   int old_depth;
   char box_message[1024];
   float x;
   float alpha;
   uintptr_t thumbnail;
   uintptr_t savestate_thumbnail;
   float thumbnail_width;
   float thumbnail_height;
   float thumbnail_orig_width;
   float thumbnail_orig_height;
   float savestate_thumbnail_width;
   float savestate_thumbnail_height;
   char background_file_path[PATH_MAX_LENGTH];
   char thumbnail_file_path[PATH_MAX_LENGTH];
   char savestate_thumbnail_file_path[PATH_MAX_LENGTH];

   bool mouse_show;

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

   float above_subitem_offset;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;

   float shadow_offset;

   char title_name[255];

   struct
   {
      struct
      {
         float alpha;
      } arrow;

      menu_texture_item bg;
      menu_texture_item list[XMB_TEXTURE_LAST];
   } textures;

   struct
   {
      struct
      {
         float horizontal;
         float vertical;
      } spacing;

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
         float alpha;
         float zoom;
      } active;

      struct
      {
         float alpha;
         float zoom;
      } passive;

      float x_pos;
      size_t selection_ptr_old;
      size_t selection_ptr;
   } categories;

   struct
   {
      struct
      {
         float alpha;
         float zoom;
      } active;

      struct
      {
         float alpha;
         float zoom;
      } passive;
   } items;

   xmb_node_t main_menu_node;
#ifdef HAVE_IMAGEVIEWER
   xmb_node_t images_tab_node;
#endif
#ifdef HAVE_FFMPEG
   xmb_node_t video_tab_node;
   xmb_node_t music_tab_node;
#endif
   xmb_node_t settings_tab_node;
   xmb_node_t history_tab_node;
   xmb_node_t add_tab_node;
   xmb_node_t netplay_tab_node;

   font_data_t *font;
   font_data_t *font2;
   float font_size;
   float font2_size;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;

   unsigned tabs[8];
   unsigned system_tab_end;
} xmb_handle_t;

float gradient_dark_purple[16] = {
    20/255.0,  13/255.0,  20/255.0, 1.0,
    20/255.0,  13/255.0,  20/255.0, 1.0,
    92/255.0,  44/255.0,  92/255.0, 1.0,
   148/255.0,  90/255.0, 148/255.0, 1.0,
};

float gradient_midnight_blue[16] = {
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
};

float gradient_golden[16] = {
   174/255.0, 123/255.0,  44/255.0, 1.0,
   205/255.0, 174/255.0,  84/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
};

float gradient_legacy_red[16] = {
   171/255.0,  70/255.0,  59/255.0, 1.0,
   171/255.0,  70/255.0,  59/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
};

float gradient_electric_blue[16] = {
     1/255.0,   2/255.0,  67/255.0, 1.0,
     1/255.0,  73/255.0, 183/255.0, 1.0,
     1/255.0,  93/255.0, 194/255.0, 1.0,
     3/255.0, 162/255.0, 254/255.0, 1.0,
};

float gradient_apple_green[16] = {
   102/255.0, 134/255.0,  58/255.0, 1.0,
   122/255.0, 131/255.0,  52/255.0, 1.0,
    82/255.0, 101/255.0,  35/255.0, 1.0,
    63/255.0,  95/255.0,  30/255.0, 1.0,
};

float gradient_undersea[16] = {
    23/255.0,  18/255.0,  41/255.0, 1.0,
    30/255.0,  72/255.0, 114/255.0, 1.0,
    52/255.0,  88/255.0, 110/255.0, 1.0,
    69/255.0, 125/255.0, 140/255.0, 1.0,

};

float gradient_volcanic_red[16] = {
   1.0, 0.0, 0.1, 1.00,
   1.0, 0.1, 0.0, 1.00,
   0.1, 0.0, 0.1, 1.00,
   0.1, 0.0, 0.1, 1.00,
};

float gradient_dark[16] = {
   0.1, 0.1, 0.1, 1.00,
   0.1, 0.1, 0.1, 1.00,
   0.0, 0.0, 0.0, 1.00,
   0.0, 0.0, 0.0, 1.00,
};

const char* xmb_theme_ident(void)
{
   settings_t *settings = config_get_ptr();
   switch (settings->menu.xmb.theme)
   {
      case XMB_ICON_THEME_FLATUI:
         return "flatui";
      case XMB_ICON_THEME_RETROACTIVE:
         return "retroactive";
      case XMB_ICON_THEME_PIXEL:
         return "pixel";
      case XMB_ICON_THEME_NEOACTIVE:
         return "neoactive";
      case XMB_ICON_THEME_SYSTEMATIC:
         return "systematic";
      case XMB_ICON_THEME_CUSTOM:
         return "custom";
      case XMB_ICON_THEME_MONOCHROME:
      default:
         break;
   }

   return "monochrome";
}

static const char *xmb_thumbnails_ident(void)
{
   settings_t *settings = config_get_ptr();

   switch (settings->menu.thumbnails)
   {
      case 1:
         return "Named_Snaps";
      case 2:
         return "Named_Titles";
      case 3:
         return "Named_Boxarts";
      case 0:
      default:
         break;
   }

   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
}

static float *xmb_gradient_ident(video_frame_info_t *video_info)
{
   switch (video_info->xmb_color_theme)
   {
      case XMB_THEME_DARK_PURPLE:
         return &gradient_dark_purple[0];
      case XMB_THEME_MIDNIGHT_BLUE:
         return &gradient_midnight_blue[0];
      case XMB_THEME_GOLDEN:
         return &gradient_golden[0];
      case XMB_THEME_ELECTRIC_BLUE:
         return &gradient_electric_blue[0];
      case XMB_THEME_APPLE_GREEN:
         return &gradient_apple_green[0];
      case XMB_THEME_UNDERSEA:
         return &gradient_undersea[0];
      case XMB_THEME_VOLCANIC_RED:
         return &gradient_volcanic_red[0];
      case XMB_THEME_DARK:
         return &gradient_dark[0];
      case XMB_THEME_LEGACY_RED:
      default:
         break;
   }

   return &gradient_legacy_red[0];
}

static size_t xmb_list_get_selection(void *data)
{
   xmb_handle_t *xmb      = (xmb_handle_t*)data;

   if (!xmb)
      return 0;

   return xmb->categories.selection_ptr;
}

static size_t xmb_list_get_size(void *data, enum menu_list_type type)
{
   xmb_handle_t *xmb       = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            return file_list_get_size(xmb->horizontal_list);
         break;
      case MENU_LIST_TABS:
         return xmb->system_tab_end;
   }

   return 0;
}

static void *xmb_list_get_entry(void *data, enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   xmb_handle_t *xmb       = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         if (i < list_size)
            return (void*)&xmb->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static INLINE float xmb_item_y(xmb_handle_t *xmb, int i, size_t current)
{
   float iy = xmb->icon.spacing.vertical;

   if (i < (int)current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + xmb->above_subitem_offset);
      else
         iy *= (i - (int)current + xmb->above_item_offset);
   else
      iy    *= (i - (int)current + xmb->under_item_offset);

   if (i == (int)current)
      iy = xmb->icon.spacing.vertical * xmb->active_item_factor;

   return iy;
}

static void xmb_draw_icon(
      menu_display_frame_info_t menu_disp_info,
      int icon_size,
      math_matrix_4x4 *mymat,
      uintptr_t texture,
      float x,
      float y,
      unsigned width,
      unsigned height,
      float alpha,
      float rotation,
      float scale_factor,
      float *color,
      float shadow_offset)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   float shadow[16];
   unsigned i;

   if (
         (x < (-icon_size / 2.0f)) ||
         (x > width)               ||
         (y < (icon_size  / 2.0f)) ||
         (y > height + icon_size)
      )
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;

   draw.width           = icon_size;
   draw.height          = icon_size;
#if defined(VITA)
   draw.width          *= scale_factor;
   draw.height         *= scale_factor;
#endif
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   if (menu_disp_info.shadows_enable)
   {
      for (i = 0; i < 16; i++)
         shadow[i]      = 0;

      menu_display_set_alpha(shadow, color[3] * 0.35f);

      coords.color      = shadow;
      draw.x            = x + shadow_offset;
      draw.y            = height - y - shadow_offset;

#if defined(VITA)
      if(scale_factor < 1)
      {
         draw.x         = draw.x + (icon_size-draw.width)/2;
         draw.y         = draw.y + (icon_size-draw.width)/2;
      }
#endif
      menu_display_draw(&draw);
   }

   coords.color         = (const float*)color;
   draw.x               = x;
   draw.y               = height - y;

#if defined(VITA)
   if(scale_factor < 1)
   {
      draw.x            = draw.x + (icon_size-draw.width)/2;
      draw.y            = draw.y + (icon_size-draw.width)/2;
   }
#endif
   menu_display_draw(&draw);
}

static void xmb_draw_thumbnail(
      menu_display_frame_info_t menu_disp_info,
      xmb_handle_t *xmb, float *color,
      unsigned width, unsigned height, float w, float h, uintptr_t texture)
{
   unsigned i;
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   float shadow[16];
   float y                  = 
      xmb->margins.screen.top + xmb->icon.size + h;
   float x                  = 
      xmb->margins.screen.left + xmb->icon.spacing.horizontal +
      xmb->icon.spacing.horizontal*4 - xmb->icon.size / 4;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw);

   coords.vertices          = 4;
   coords.vertex            = NULL;
   coords.tex_coord         = NULL;
   coords.lut_tex_coord     = NULL;

   draw.width               = w;
   draw.height              = h;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.texture             = texture;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;

   if (menu_disp_info.shadows_enable)
   {
      for (i = 0; i < 16; i++)
         shadow[i]      = 0;

      menu_display_set_alpha(shadow, color[3] * 0.35f);

      coords.color          = shadow;
      draw.x                = x + xmb->shadow_offset;
      draw.y                = height - y - xmb->shadow_offset;

      menu_display_draw(&draw);
   }

   coords.color             = (const float*)color;
   draw.x                   = x;
   draw.y                   = height - y;

   menu_display_set_alpha((float*)coords.color, 1.0f);

   menu_display_draw(&draw);
}

static void xmb_draw_text(
      menu_display_frame_info_t menu_disp_info,
      xmb_handle_t *xmb,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font)
{
   uint32_t color;
   uint8_t a8;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8       = 255 * alpha;

   /* Avoid drawing 100% transparent text */
   if (a8 == 0)
      return;

   color = FONT_COLOR_RGBA(255, 255, 255, a8);

   menu_display_draw_text(font, str, x, y,
         width, height, color, text_align, scale_factor,
         menu_disp_info.shadows_enable,
         xmb->shadow_offset);
}

static void xmb_messagebox(void *data, const char *message)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb || string_is_empty(message))
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_keyboard(xmb_handle_t *xmb,
      video_frame_info_t *video_info,
      const char *grid[], unsigned id)
{
   unsigned i;
   int ptr_width, ptr_height;
   unsigned width    = video_info->width;
   unsigned height   = video_info->height;
   float dark[16]    =  {
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
   };

   float white[16]=  {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };

   menu_display_draw_quad(0, height/2.0, width, height/2.0,
         width, height,
         &dark[0]);

   ptr_width  = width / 11;
   ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y        = (i / 11) * height / 10.0;
      uintptr_t texture = xmb->textures.list[XMB_TEXTURE_KEY];

      if (i == id)
         texture = xmb->textures.list[XMB_TEXTURE_KEY_HOVER];

      menu_display_blend_begin();

      menu_display_draw_texture(
            width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width,
            height/2.0 + ptr_height*1.5 + line_y,
            ptr_width, ptr_height,
            width, height,
            &white[0],
            texture);

      menu_display_blend_end();

      menu_display_draw_text(xmb->font, grid[i],
            width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width + ptr_width/2.0,
            height/2.0 + ptr_height + line_y + xmb->font->size / 3,
            width, height, 0xffffffff, TEXT_ALIGN_CENTER, 1.0f,
            false, 0);
   }
}

/* Returns the OSK key at a given position */
static int xmb_osk_ptr_at_pos(void *data, int x, int y, unsigned width, unsigned height)
{
   unsigned i;
   int ptr_width, ptr_height;
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb)
      return -1;

   ptr_width  = width  / 11;
   ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y    = (i / 11)*height/10.0;
      int ptr_x     = width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width;
      int ptr_y     = height/2.0 + ptr_height*1.5 + line_y - ptr_height;

      if (x > ptr_x && x < ptr_x + ptr_width
       && y > ptr_y && y < ptr_y + ptr_height)
         return i;
   }

   return -1;
}

static void xmb_render_messagebox_internal(
      menu_display_frame_info_t menu_disp_info,
      video_frame_info_t *video_info,
      xmb_handle_t *xmb, const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   y_position = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position = height / 4;

   x = width  / 2;
   y = y_position - (list->size-1) * xmb->font->size / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int len = utf8len(msg);
      if (len > longest)
      {
         longest = len;
         longest_width = font_driver_get_message_width(xmb->font, msg, len, 1);
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(
               menu_disp_info,
               xmb, msg,
               x - longest_width/2.0,
               y + i * xmb->font->size,
               1,
               1,
               TEXT_ALIGN_LEFT,
               width,
               height,
               xmb->font);
   }

   if (menu_input_dialog_get_display_kb())
      xmb_render_keyboard(xmb,
            video_info,
            menu_event_get_osk_grid(),
            menu_event_get_osk_ptr());

end:
   string_list_free(list);
}

static void xmb_update_thumbnail_path(void *data, unsigned i)
{
   menu_entry_t entry;
   char tmp_new[PATH_MAX_LENGTH];
   char             *tmp    = NULL;
   char *scrub_char_pointer = NULL;
   settings_t     *settings = config_get_ptr();
   xmb_handle_t     *xmb    = (xmb_handle_t*)data;
   playlist_t     *playlist = NULL;
   const char    *core_name = NULL;

   if (!xmb)
      return;

   entry.path[0]       = '\0';
   entry.label[0]      = '\0';
   entry.sublabel[0]   = '\0';
   entry.value[0]      = '\0';
   entry.rich_label[0] = '\0';
   entry.enum_idx      = MSG_UNKNOWN;
   entry.entry_idx     = 0;
   entry.idx           = 0;
   entry.type          = 0;
   entry.spacing       = 0;

   menu_entry_get(&entry, 0, i, NULL, true);

   if (entry.type == FILE_TYPE_IMAGEVIEWER || entry.type == FILE_TYPE_IMAGE)
   {
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(selection_buf, i);

      if (node)
      {
         fill_pathname_join(
               xmb->thumbnail_file_path,
               node->fullpath,
               entry.path,
               sizeof(xmb->thumbnail_file_path));

         return;
      }
   }
   else if (xmb_list_get_selection(xmb) <= XMB_SYSTEM_TAB_SETTINGS)
   {
      xmb->thumbnail_file_path[0] = '\0';
      xmb->thumbnail = 0;
      return;
   }

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   if (playlist)
   {
      playlist_get_index(playlist, i,
            NULL, NULL, NULL, &core_name, NULL, NULL);

      if (core_name && string_is_equal(core_name, "imageviewer"))
      {
         strlcpy(xmb->thumbnail_file_path, entry.label,
               sizeof(xmb->thumbnail_file_path));
         return;
      }
   }

   fill_pathname_join(
         xmb->thumbnail_file_path,
         settings->directory.thumbnails,
         xmb->title_name,
         sizeof(xmb->thumbnail_file_path));

   fill_pathname_join(xmb->thumbnail_file_path, xmb->thumbnail_file_path,
         xmb_thumbnails_ident(), sizeof(xmb->thumbnail_file_path));

   /* Scrub characters that are not cross-platform and/or violate the 
    * No-Intro filename standard:
    * http://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).zip
    * Replace these characters in the entry name with underscores.
    */
   tmp = strdup(entry.path);

   while((scrub_char_pointer = strpbrk(tmp, "&*/:`<>?\\|")))
      *scrub_char_pointer = '_';

   /* Look for thumbnail file with this scrubbed filename */
   tmp_new[0] = '\0';

   fill_pathname_join(tmp_new,
         xmb->thumbnail_file_path,
         tmp, sizeof(tmp_new));
   strlcpy(xmb->thumbnail_file_path,
         tmp_new, sizeof(xmb->thumbnail_file_path));
   free(tmp);

   strlcat(xmb->thumbnail_file_path,
         file_path_str(FILE_PATH_PNG_EXTENSION),
         sizeof(xmb->thumbnail_file_path));
}

static void xmb_update_savestate_thumbnail_path(void *data, unsigned i)
{
   menu_entry_t entry;
   settings_t     *settings = config_get_ptr();
   global_t         *global = global_get_ptr();
   xmb_handle_t     *xmb    = (xmb_handle_t*)data;
   playlist_t     *playlist = NULL;

   if (!xmb)
      return;

   entry.path[0]       = '\0';
   entry.label[0]      = '\0';
   entry.sublabel[0]   = '\0';
   entry.value[0]      = '\0';
   entry.rich_label[0] = '\0';
   entry.enum_idx      = MSG_UNKNOWN;
   entry.entry_idx     = 0;
   entry.idx           = 0;
   entry.type          = 0;
   entry.spacing       = 0;

   menu_entry_get(&entry, 0, i, NULL, true);

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   xmb->savestate_thumbnail_file_path[0] = '\0';

   if (     (settings->savestate_thumbnail_enable)
         && (string_is_equal(entry.label, "state_slot")
         || (string_is_equal(entry.label, "loadstate"))
         || (string_is_equal(entry.label, "savestate"))))
   {
      char path[PATH_MAX_LENGTH];

      path[0] = '\0';

      if (settings->state_slot > 0)
         snprintf(path, sizeof(path), "%s%d",
               global->name.savestate, settings->state_slot);
      else if (settings->state_slot < 0)
         fill_pathname_join_delim(path,
               global->name.savestate, "auto", '.', sizeof(path));
      else
         strlcpy(path, global->name.savestate, sizeof(path));

      strlcat(path, file_path_str(FILE_PATH_PNG_EXTENSION), sizeof(path));

      if (path_file_exists(path))
      {
         strlcpy(xmb->savestate_thumbnail_file_path, path,
               sizeof(xmb->savestate_thumbnail_file_path));
      }
   }
}

static void xmb_update_thumbnail_image(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   if (path_file_exists(xmb->thumbnail_file_path))
      task_push_image_load(xmb->thumbnail_file_path,
            menu_display_handle_thumbnail_upload, NULL);
   else if (xmb->depth == 1)
      xmb->thumbnail = 0;
}

static void xmb_update_savestate_thumbnail_image(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   if (path_file_exists(xmb->savestate_thumbnail_file_path))
      task_push_image_load(xmb->savestate_thumbnail_file_path,
            menu_display_handle_savestate_thumbnail_upload, NULL);
   else
      xmb->savestate_thumbnail = 0;
}

static void xmb_selection_pointer_changed(
      xmb_handle_t *xmb, bool allow_animations)
{
   unsigned i, end, height;
   menu_animation_ctx_tag_t tag;
   size_t selection, num      = 0;
   int threshold              = 0;
   menu_list_t     *menu_list = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   menu_entries_ctl(MENU_ENTRIES_CTL_LIST_GET, &menu_list);

   if (!xmb)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   end       = menu_entries_get_end();
   threshold = xmb->icon.size*10;

   video_driver_get_size(NULL, &height);

   tag.id    = (uintptr_t)menu_list;

   menu_animation_ctl(MENU_ANIMATION_CTL_KILL_BY_TAG, &tag);
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &num);

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia         = xmb->items.passive.alpha;
      float iz         = xmb->items.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      iy      = xmb_item_y(xmb, i, selection);
      real_iy = iy + xmb->margins.screen.top;

      if (i == selection)
      {
         unsigned depth = xmb_list_get_size(xmb, MENU_LIST_PLAIN);

         ia             = xmb->items.active.alpha;
         iz             = xmb->items.active.zoom;

         if (!string_is_equal(xmb_thumbnails_ident(),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))
               && (depth == 1 || xmb_list_get_selection(xmb) <= XMB_SYSTEM_TAB_SETTINGS))
         {
            xmb_update_thumbnail_path(xmb, i);
            xmb_update_thumbnail_image(xmb);
         }
         xmb_update_savestate_thumbnail_path(xmb, i);
         xmb_update_savestate_thumbnail_image(xmb);
      }

      if (     (!allow_animations)
            || (real_iy < -threshold
            || real_iy > height+threshold))
      {
         node->alpha = node->label_alpha = ia;
         node->y = iy;
         node->zoom = iz;
      }
      else
      {
         menu_animation_ctx_entry_t entry;

         entry.duration     = XMB_DELAY;
         entry.target_value = ia;
         entry.subject      = &node->alpha;
         entry.easing_enum  = EASING_OUT_QUAD;
         entry.tag          = tag.id;
         entry.cb           = NULL;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.subject      = &node->label_alpha;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = iz;
         entry.subject      = &node->zoom;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = iy;
         entry.subject      = &node->y;

         if (entry.subject)
            menu_animation_push(&entry);
      }
   }
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
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = xmb->items.active.alpha;
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
         menu_animation_ctx_entry_t entry;

         entry.duration     = XMB_DELAY;
         entry.target_value = ia;
         entry.subject      = &node->alpha;
         entry.easing_enum  = EASING_OUT_QUAD;
         entry.tag          = -1;
         entry.cb           = NULL;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = 0;
         entry.subject      = &node->label_alpha;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = xmb->icon.size * dir * -2;
         entry.subject      = &node->x;

         if (entry.subject)
            menu_animation_push(&entry);
      }
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   size_t skip          = 0;
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
      node->zoom = xmb->categories.passive.zoom;

      real_y = node->y + xmb->margins.screen.top;

      if (i == current)
         node->zoom = xmb->categories.active.zoom;

      ia    = xmb->items.passive.alpha;
      if (i == current)
         ia = xmb->items.active.alpha;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x = 0;
      }
      else
      {
         menu_animation_ctx_entry_t entry;

         entry.duration     = XMB_DELAY;
         entry.target_value = ia;
         entry.subject      = &node->alpha;
         entry.easing_enum  = EASING_OUT_QUAD;
         entry.tag          = -1;
         entry.cb           = NULL;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.subject      = &node->label_alpha;

         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = 0;
         entry.subject      = &node->x;

         if (entry.subject)
            menu_animation_push(&entry);
      }
   }

   xmb->old_depth = xmb->depth;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &skip);

   if (xmb_list_get_selection(xmb) <= XMB_SYSTEM_TAB_SETTINGS)
      xmb_update_thumbnail_path(xmb, 0);
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

   if ((i + xmb->system_tab_end) == xmb->categories.active.idx)
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
   return (xmb_node_t*)
      menu_entries_get_actiondata_at_offset(xmb->horizontal_list, i);
}

static void xmb_push_animations(xmb_node_t *node, float ia, float ix)
{
   menu_animation_ctx_entry_t entry;

   entry.duration     = XMB_DELAY;
   entry.target_value = ia;
   entry.subject      = &node->alpha;
   entry.easing_enum  = EASING_OUT_QUAD;
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);

   entry.subject      = &node->label_alpha;

   if (entry.subject)
      menu_animation_push(&entry);

   entry.target_value = ix;
   entry.subject      = &node->x;

   if (entry.subject)
      menu_animation_push(&entry);
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
      char path[PATH_MAX_LENGTH];
      char *tmp = string_replace_substring(xmb->title_name, "/", " ");

      path[0] = '\0';

      if (tmp)
      {
         fill_pathname_join_noext(
               path,
               settings->directory.dynamic_wallpapers,
               tmp,
               sizeof(path));
         free(tmp);
      }

      strlcat(path,
            file_path_str(FILE_PATH_PNG_EXTENSION),
            sizeof(path));

      if (!path_file_exists(path))
         fill_pathname_application_special(path, sizeof(path),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

       if(!string_is_equal(path, xmb->background_file_path))
       {
           if(path_file_exists(path))
           {
              task_push_image_load(path,
                  menu_display_handle_wallpaper_upload, NULL);
              strlcpy(xmb->background_file_path,
                    path, sizeof(xmb->background_file_path));
           }
       }
   }

   end = file_list_get_size(list);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);
      float ia         = xmb->items.passive.alpha;

      if (!node)
         continue;

      node->x           = xmb->icon.spacing.horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = xmb->items.active.alpha;

      xmb_push_animations(node, ia, 0);
   }
}

static void xmb_set_title(xmb_handle_t *xmb)
{
   if (xmb->categories.selection_ptr <= xmb->system_tab_end)
   {
      menu_entries_get_title(xmb->title_name, sizeof(xmb->title_name));
   }
   else
   {
      const char *path = NULL;
      menu_entries_get_at_offset(
            xmb->horizontal_list,
            xmb->categories.selection_ptr - (xmb->system_tab_end + 1),
            &path, NULL, NULL, NULL, NULL);

      if (!path)
         return;

      fill_pathname_base_noext(xmb->title_name, path, sizeof(xmb->title_name));
   }
}

static unsigned xmb_get_system_tab(xmb_handle_t *xmb, unsigned i)
{
   if (i <= xmb->system_tab_end)
   {
      return xmb->tabs[i];
   }
   return UINT_MAX;
}

static xmb_node_t* xmb_get_node(xmb_handle_t *xmb, unsigned i)
{
   switch (xmb_get_system_tab(xmb, i))
   {
      case XMB_SYSTEM_TAB_SETTINGS:
         return &xmb->settings_tab_node;
#ifdef HAVE_IMAGEVIEWER
      case XMB_SYSTEM_TAB_IMAGES:
         return &xmb->images_tab_node;
#endif
#ifdef HAVE_FFMPEG
      case XMB_SYSTEM_TAB_MUSIC:
         return &xmb->music_tab_node;
      case XMB_SYSTEM_TAB_VIDEO:
         return &xmb->video_tab_node;
#endif
      case XMB_SYSTEM_TAB_HISTORY:
         return &xmb->history_tab_node;
      case XMB_SYSTEM_TAB_ADD:
         return &xmb->add_tab_node;
#ifdef HAVE_NETWORKING
      case XMB_SYSTEM_TAB_NETPLAY:
         return &xmb->netplay_tab_node;
#endif
      default:
         if (i > xmb->system_tab_end)
            return xmb_get_userdata_from_horizontal_list(
                  xmb, i - (xmb->system_tab_end + 1));
   }

   return &xmb->main_menu_node;
}

static void xmb_list_switch_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t entry;
      float ia                    = xmb->categories.passive.alpha;
      float iz                    = xmb->categories.passive.zoom;
      xmb_node_t *node            = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
      {
         ia = xmb->categories.active.alpha;
         iz = xmb->categories.active.zoom;
      }

      entry.duration     = XMB_DELAY;
      entry.target_value = ia;
      entry.subject      = &node->alpha;
      entry.easing_enum  = EASING_OUT_QUAD;
      entry.tag          = -1;
      entry.cb           = NULL;

      if (entry.subject)
         menu_animation_push(&entry);

      entry.target_value = iz;
      entry.subject      = &node->zoom;

      if (entry.subject)
         menu_animation_push(&entry);
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   menu_animation_ctx_entry_t entry;
   size_t selection;
   int dir = -1;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb->categories.active.idx += dir;

   xmb_list_switch_horizontal_list(xmb);

   entry.duration     = XMB_DELAY;
   entry.target_value = xmb->icon.spacing.horizontal * -(float)xmb->categories.selection_ptr;
   entry.subject      = &xmb->categories.x_pos;
   entry.easing_enum  = EASING_OUT_QUAD;
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);

   dir = -1;
   if (xmb->categories.selection_ptr > xmb->categories.selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_switch_new(xmb, selection_buf, dir, selection);
   xmb->categories.active.idx_old = xmb->categories.selection_ptr;

   if (!string_is_equal(xmb_thumbnails_ident(),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
   {
      xmb_update_thumbnail_path(xmb, 0);
      xmb_update_thumbnail_image(xmb);
   }
}

static void xmb_list_open_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t entry;
      float ia          = 0;
      xmb_node_t *node  = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories.active.idx)
         ia = xmb->categories.active.alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories.passive.alpha;

      entry.duration     = XMB_DELAY;
      entry.target_value = ia;
      entry.subject      = &node->alpha;
      entry.easing_enum  = EASING_OUT_QUAD;
      entry.tag          = -1;
      entry.cb           = NULL;

      if (entry.subject)
         menu_animation_push(&entry);
   }
}

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      xmb_node_t *node = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         continue;

      file_list_get_at_offset(xmb->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path || !strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      video_driver_texture_unload(&node->icon);
      video_driver_texture_unload(&node->content_icon);
   }
}

static void xmb_init_horizontal_list(xmb_handle_t *xmb)
{
   menu_displaylist_info_t info;
   settings_t *settings         = config_get_ptr();

   info.need_sort               = false;
   info.need_refresh            = false;
   info.need_entries_refresh    = false;
   info.need_push               = false;
   info.push_builtin_cores      = false;
   info.download_core           = false;
   info.need_clear              = false;
   info.need_navigation_clear   = false;
   info.list                    = xmb->horizontal_list;
   info.menu_list               = NULL;
   strlcpy(info.path, settings->directory.playlist, sizeof(info.path));
   info.path_b[0]               = '\0';
   info.path_c[0]               = '\0';
   strlcpy(info.label,
         msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
         sizeof(info.label));
   info.label_hash              = 0;
   strlcpy(info.exts,
         file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT), sizeof(info.exts));
   info.type                    = 0;
   info.type_default            = FILE_TYPE_PLAIN;
   info.directory_ptr           = 0;
   info.flags                   = 0;
   info.enum_idx                = MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST;
   info.setting                 = NULL;

   if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
   {
      size_t i;
      for (i=0; i < xmb->horizontal_list->size; i++)
         xmb_node_allocate_userdata(xmb, i);
      menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
   }
}

static void xmb_toggle_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

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

static void xmb_context_reset_horizontal_list(
      xmb_handle_t *xmb)
{
   unsigned i;
   int depth; /* keep this integer */
   char themepath[PATH_MAX_LENGTH];
   size_t list_size                = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   themepath[0] = '\0';

   fill_pathname_application_special(themepath, sizeof(themepath),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);

   xmb->categories.x_pos = xmb->icon.spacing.horizontal *
      -(float)xmb->categories.selection_ptr;

   depth = (xmb->depth > 1) ? 2 : 1;
   xmb->x = xmb->icon.size * -(depth*2-2);

   for (i = 0; i < list_size; i++)
   {
      char iconpath[PATH_MAX_LENGTH];
      char sysname[PATH_MAX_LENGTH];
      char texturepath[PATH_MAX_LENGTH];
      char content_texturepath[PATH_MAX_LENGTH];
      struct texture_image ti;
      const char *path                          = NULL;
      xmb_node_t *node                          =
         xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
      {
         node = xmb_node_allocate_userdata(xmb, i);
         if (!node)
            continue;
      }

      iconpath[0] = sysname[0] = texturepath[0] =
         content_texturepath[0] = '\0';

      file_list_get_at_offset(xmb->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path)
         continue;

      if (!strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      fill_pathname_base_noext(sysname, path, sizeof(sysname));

      fill_pathname_application_special(iconpath, sizeof(iconpath),
            APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

      fill_pathname_join_concat(texturepath, iconpath, sysname,
            file_path_str(FILE_PATH_PNG_EXTENSION),
            sizeof(texturepath));

      ti.width         = 0;
      ti.height        = 0;
      ti.pixels        = NULL;
      ti.supports_rgba = video_driver_supports_rgba();

      if (image_texture_load(&ti, texturepath))
      {
         if(ti.pixels)
         {
            video_driver_texture_unload(&node->icon);
            video_driver_texture_load(&ti,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &node->icon);
         }

         image_texture_free(&ti);
      }

      strlcat(iconpath, sysname, sizeof(iconpath));
      fill_pathname_join_delim(content_texturepath, iconpath,
            file_path_str(FILE_PATH_CONTENT_BASENAME), '-',
            sizeof(content_texturepath));

      if (image_texture_load(&ti, content_texturepath))
      {
         if(ti.pixels)
         {
            video_driver_texture_unload(&node->content_icon);
            video_driver_texture_load(&ti,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &node->content_icon);
         }

         image_texture_free(&ti);
      }
   }

   xmb_toggle_horizontal_list(xmb);
}

static void xmb_refresh_horizontal_list(xmb_handle_t *xmb)
{
   xmb_context_destroy_horizontal_list(xmb);
   if (xmb->horizontal_list)
      file_list_free(xmb->horizontal_list);
   xmb->horizontal_list = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   xmb->horizontal_list         = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (xmb->horizontal_list)
      xmb_init_horizontal_list(xmb);

   xmb_context_reset_horizontal_list(xmb);
}

static int xmb_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   xmb_handle_t *xmb        = (xmb_handle_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!xmb)
            return -1;
         xmb->mouse_show = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!xmb)
            return -1;
         xmb->mouse_show = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!xmb)
            return -1;

         xmb_refresh_horizontal_list(xmb);
         break;
      default:
         return -1;
   }

   return 0;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   menu_animation_ctx_entry_t entry;

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


   entry.duration     = XMB_DELAY;
   entry.target_value = xmb->icon.size * -(xmb->depth*2-2);
   entry.subject      = &xmb->x;
   entry.easing_enum  = EASING_OUT_QUAD;
   entry.tag          = -1;
   entry.cb           = NULL;


   switch (xmb->depth)
   {
      case 1:
         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = 0;
         entry.subject      = &xmb->textures.arrow.alpha;

         if (entry.subject)
            menu_animation_push(&entry);
         break;
      case 2:
         if (entry.subject)
            menu_animation_push(&entry);

         entry.target_value = 1;
         entry.subject      = &xmb->textures.arrow.alpha;

         if (entry.subject)
            menu_animation_push(&entry);
         break;
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb)
      return;

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      xmb_selection_pointer_changed(xmb, false);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      if (!string_is_equal(xmb_thumbnails_ident(),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
         xmb_update_thumbnail_image(xmb);
      xmb_update_savestate_thumbnail_image(xmb);
      return;
   }

   xmb_set_title(xmb);

   if (xmb->categories.selection_ptr != xmb->categories.active.idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);
}

static uintptr_t xmb_icon_get_id(xmb_handle_t *xmb,
      xmb_node_t *core_node, xmb_node_t *node,
      enum msg_hash_enums enum_idx, unsigned type, bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_SAVE_STATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return xmb->textures.list[XMB_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      default:
         break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case FILE_TYPE_RPL_ENTRY:
         if (core_node)
            return core_node->content_icon;

#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_FFMPEG)
         switch (xmb_get_system_tab(xmb, xmb->categories.selection_ptr))
         {
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               return xmb->textures.list[XMB_TEXTURE_IMAGE];
#endif
#ifdef HAVE_FFMPEG
            case XMB_SYSTEM_TAB_MUSIC:
               return xmb->textures.list[XMB_TEXTURE_MUSIC];
            case XMB_SYSTEM_TAB_VIDEO:
               return xmb->textures.list[XMB_TEXTURE_MOVIE];
#endif
            default:
               break;
         }
#endif
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case FILE_TYPE_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case FILE_TYPE_RDB:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return xmb->textures.list[XMB_TEXTURE_CURSOR];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_SETTING_ACTION_CLOSE:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case FILE_TYPE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_RESET:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION:
         if (xmb->depth == 3)
            return xmb->textures.list[XMB_TEXTURE_SUBSETTING];
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_SETTING_GROUP:
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_INFO_MESSAGE:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_WIFI:
         return xmb->textures.list[XMB_TEXTURE_WIFI];
   }

   return xmb->textures.list[XMB_TEXTURE_SUBSETTING];
}

static void xmb_draw_items(
      video_frame_info_t *video_info,
      menu_display_frame_info_t menu_disp_info,
      xmb_handle_t *xmb,
      file_list_t *list, file_list_t *stack,
      size_t current, size_t cat_selection_ptr, float *color,
      unsigned width, unsigned height)
{
   size_t i;
   math_matrix_4x4 mymat;
   menu_display_ctx_rotate_draw_t rotate_draw;
   xmb_node_t *core_node       = NULL;
   size_t end                  = 0;
   uint64_t frame_count        = video_info->frame_count;

   if (!list || !list->size)
      return;

   if (cat_selection_ptr > xmb->system_tab_end)
      core_node = xmb_get_userdata_from_horizontal_list(
            xmb, cat_selection_ptr - (xmb->system_tab_end + 1));

   end = file_list_get_size(list);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (list == xmb->selection_buf_old)
      i = 0;

   menu_display_blend_begin();

   for (; i < end; i++)
   {
      float icon_x, icon_y, label_offset;
      menu_animation_ctx_ticker_t ticker;
      char ticker_str[PATH_MAX_LENGTH];
      char name[255];
      char value[255];
      char entry_value[255];
      char entry_sublabel[255];
      menu_entry_t entry;
      const float half_size             = xmb->icon.size / 2.0f;
      uintptr_t texture_switch          = 0;
      xmb_node_t *   node               = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);
      bool do_draw_text                 = false;
      unsigned ticker_limit             = 35;

      if (!node)
         continue;

      entry.path[0]       = '\0';
      entry.label[0]      = '\0';
      entry.sublabel[0]   = '\0';
      entry.value[0]      = '\0';
      entry.rich_label[0] = '\0';
      entry.enum_idx      = MSG_UNKNOWN;
      entry.entry_idx     = 0;
      entry.idx           = 0;
      entry.type          = 0;
      entry.spacing       = 0;

      ticker_str[0] = name[0] = value[0] = entry_sublabel[0] = entry_value[0] = '\0';

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


      if (entry.type == FILE_TYPE_CONTENTLIST_ENTRY)
         fill_short_pathname_representation(entry.path, entry.path,
               sizeof(entry.path));

      menu_entry_get_value(i, list, entry_value, sizeof(entry_value));

      if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
      {
         if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF])
            texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF];
         else
            do_draw_text = true;
      }
      else if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
            (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
      {
         if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON])
            texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON];
         else
            do_draw_text = true;
      }
      else
      {
         enum msg_file_type type = msg_hash_to_file_type(msg_hash_calculate(entry_value));

         switch (type)
         {
            case FILE_TYPE_IN_CARCHIVE:
            case FILE_TYPE_COMPRESSED:
            case FILE_TYPE_MORE:
            case FILE_TYPE_CORE:
            case FILE_TYPE_DIRECT_LOAD:
            case FILE_TYPE_RDB:
            case FILE_TYPE_CURSOR:
            case FILE_TYPE_PLAIN:
            case FILE_TYPE_DIRECTORY:
            case FILE_TYPE_MUSIC:
            case FILE_TYPE_IMAGE:
            case FILE_TYPE_MOVIE:
               break;
            case FILE_TYPE_BOOL_ON:
               if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON])
                  texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON];
               else
                  do_draw_text = true;
               break;
            case FILE_TYPE_BOOL_OFF:
               if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF])
                  texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF];
               else
                  do_draw_text = true;
               break;
            default:
               do_draw_text = true;
               break;
         }
      }

      if (string_is_empty(entry_value))
      {
         if (xmb->savestate_thumbnail ||
               (!string_is_equal
                (
                 xmb_thumbnails_ident(),
                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)) 
                && xmb->thumbnail)
            )
            ticker_limit = 40;
         else
            ticker_limit = 70;
      }

      menu_entry_get_rich_label(i, ticker_str, sizeof(ticker_str));

      ticker.s        = name;
      ticker.len      = ticker_limit;
      ticker.idx      = frame_count / 20;
      ticker.str      = ticker_str;
      ticker.selected = (i == current);

      menu_animation_ticker(&ticker);

      label_offset = xmb->margins.label.top;
      if (i == current && width > 320 && height > 240
         && menu_entry_get_sublabel(i, entry_sublabel, sizeof(entry_sublabel)))
      {
         label_offset = - xmb->margins.label.top;

         word_wrap(entry_sublabel, entry_sublabel, 50);

         xmb_draw_text(menu_disp_info, xmb, entry_sublabel,
               node->x + xmb->margins.screen.left +
               xmb->icon.spacing.horizontal + xmb->margins.label.left,
               xmb->margins.screen.top + node->y + xmb->margins.label.top*3.5,
               1, node->label_alpha, TEXT_ALIGN_LEFT,
               width, height, xmb->font2);
      }

      xmb_draw_text(menu_disp_info, xmb, name,
            node->x + xmb->margins.screen.left +
            xmb->icon.spacing.horizontal + xmb->margins.label.left,
            xmb->margins.screen.top + node->y + label_offset,
            1, node->label_alpha, TEXT_ALIGN_LEFT,
            width, height, xmb->font);

      ticker.s        = value;
      ticker.len      = 35;
      ticker.idx      = frame_count / 20;
      ticker.str      = entry_value;
      ticker.selected = (i == current);

      menu_animation_ticker(&ticker);

      if (do_draw_text)
         xmb_draw_text(menu_disp_info, xmb, value,
               node->x +
               + xmb->margins.screen.left
               + xmb->icon.spacing.horizontal
               + xmb->margins.label.left
               + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->margins.label.top,
               1,
               node->label_alpha,
               TEXT_ALIGN_LEFT,
               width, height, xmb->font);


      menu_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

      if (color[3] != 0)
      {
         math_matrix_4x4 mymat;
         menu_display_ctx_rotate_draw_t rotate_draw;
         uintptr_t texture        = xmb_icon_get_id(xmb, core_node, node,
                                    entry.enum_idx, entry.type, (i == current));
         float x                  = icon_x;
         float y                  = icon_y;
         float rotation           = 0;
         float scale_factor       = node->zoom;

         rotate_draw.matrix       = &mymat;
         rotate_draw.rotation     = rotation;
         rotate_draw.scale_x      = scale_factor;
         rotate_draw.scale_y      = scale_factor;
         rotate_draw.scale_z      = 1;
         rotate_draw.scale_enable = true;

         menu_display_rotate_z(&rotate_draw);

         xmb_draw_icon(
               menu_disp_info,
               xmb->icon.size,
               &mymat,
               texture,
               x,
               y,
               width,
               height,
               1.0,
               rotation,
               scale_factor,
               &color[0],
               xmb->shadow_offset);
      }

      menu_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

      if (texture_switch != 0 && color[3] != 0)
         xmb_draw_icon(
               menu_disp_info,
               xmb->icon.size,
               &mymat,
               texture_switch,
               node->x + xmb->margins.screen.left
               + xmb->icon.spacing.horizontal
               + xmb->icon.size / 2.0 + xmb->margins.setting.left,
               xmb->margins.screen.top + node->y + xmb->icon.size / 2.0,
               width, height,
               node->alpha,
               0,
               1,
               &color[0],
               xmb->shadow_offset);
   }

   menu_display_blend_end();
}

static void xmb_render(void *data)
{
   size_t i;
   float delta_time;
   menu_animation_ctx_delta_t delta;
   settings_t   *settings   = config_get_ptr();
   xmb_handle_t *xmb        = (xmb_handle_t*)data;
   unsigned      end        = menu_entries_get_size();

   if (!xmb)
      return;

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_get_ideal_delta_time(&delta))
      menu_animation_update(delta.ideal);

   if (settings->menu.pointer.enable || settings->menu.mouse.enable)
   {
      size_t selection;

      if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
         return;

      for (i = 0; i < end; i++)
      {
         float item_y1     = xmb->margins.screen.top
            + xmb_item_y(xmb, i, selection);
         float item_y2     = item_y1 + xmb->icon.size;
         int16_t pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
         int16_t mouse_y   = menu_input_mouse_state(MENU_MOUSE_Y_AXIS)
            + (xmb->cursor.size/2);

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

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);
}

static bool xmb_shader_pipeline_active(video_frame_info_t *video_info)
{
   if (!string_is_equal(menu_driver_ident(), "xmb"))
      return false;
   if (video_info->menu_shader_pipeline == XMB_SHADER_PIPELINE_WALLPAPER)
      return false;
   return true;
}

static void xmb_draw_bg(
      xmb_handle_t *xmb,
      video_frame_info_t *video_info,
      unsigned width,
      unsigned height,
      float alpha,
      uintptr_t texture_id,
      float *coord_black,
      float *coord_white)
{
   menu_display_ctx_draw_t draw;

#if 0
   RARCH_LOG("DRAW BG %d %d \n",width,height);
#endif

   bool running              = video_info->libretro_running;

   draw.x                    = 0;
   draw.y                    = 0;
   draw.texture              = texture_id;
   draw.width                = width;
   draw.height               = height;
   draw.color                = &coord_black[0];
   draw.vertex               = NULL;
   draw.tex_coord            = NULL;
   draw.vertex_count         = 4;
   draw.prim_type            = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id          = 0;
   draw.pipeline.active      = xmb_shader_pipeline_active(video_info);

   menu_display_blend_begin();
   menu_display_set_viewport(video_info->width, video_info->height);

#ifdef HAVE_SHADERPIPELINE
   if (video_info->menu_shader_pipeline > XMB_SHADER_PIPELINE_WALLPAPER
         &&
         (video_info->xmb_color_theme != XMB_THEME_WALLPAPER))
   {
      draw.color = xmb_gradient_ident(video_info);

      if (running)
         menu_display_set_alpha(draw.color, coord_black[3]);
      else
         menu_display_set_alpha(draw.color, coord_white[3]);

      menu_display_draw_gradient(&draw, video_info);

      draw.pipeline.id = VIDEO_SHADER_MENU_2;

      switch (video_info->menu_shader_pipeline)
      {
         case XMB_SHADER_PIPELINE_RIBBON:
            draw.pipeline.id  = VIDEO_SHADER_MENU;
            break;
         case XMB_SHADER_PIPELINE_SIMPLE_SNOW:
            draw.pipeline.id  = VIDEO_SHADER_MENU_3;
            break;
         case XMB_SHADER_PIPELINE_SNOW:
            draw.pipeline.id  = VIDEO_SHADER_MENU_4;
            break;
         case XMB_SHADER_PIPELINE_BOKEH:
            draw.pipeline.id  = VIDEO_SHADER_MENU_5;
            break;
         default:
            break;
      }

      menu_display_draw_pipeline(&draw);
   }
   else
#endif
   {
      uintptr_t texture           = draw.texture;

      if (video_info->xmb_color_theme != XMB_THEME_WALLPAPER)
         draw.color = xmb_gradient_ident(video_info);

      if (running)
         menu_display_set_alpha(draw.color, coord_black[3]);
      else
         menu_display_set_alpha(draw.color, coord_white[3]);

      if (video_info->xmb_color_theme != XMB_THEME_WALLPAPER)
         menu_display_draw_gradient(&draw, video_info);

      {
         bool add_opacity = false;
         draw.texture = texture;
         menu_display_set_alpha(draw.color, coord_white[3]);

         if (!running && draw.texture)
            draw.color = &coord_white[0];

         if (video_info->xmb_color_theme == XMB_THEME_WALLPAPER)
            add_opacity = true;

         menu_display_draw_bg(&draw, video_info, add_opacity);
      }
   }

   menu_display_draw(&draw);
   menu_display_blend_end();
}

static void xmb_draw_dark_layer(
      xmb_handle_t *xmb,
      unsigned width,
      unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   float black[16] = {
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
   };

   menu_display_set_alpha(black, MIN(xmb->alpha, 0.75));

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = &black[0];

   draw.x           = 0;
   draw.y           = 0;
   draw.width       = width;
   draw.height      = height;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = menu_display_white_texture;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id = 0;

   menu_display_blend_begin();
   menu_display_draw(&draw);
   menu_display_blend_end();
}

static void xmb_frame(void *data, video_frame_info_t *video_info)
{
   size_t selection;
   size_t percent_width = 0;
   math_matrix_4x4 mymat;
   unsigned i;
   float item_color[16], coord_black[16], coord_white[16];
   menu_display_ctx_rotate_draw_t rotate_draw;
   char msg[1024];
   char title_msg[255];
   char title_truncated[255];
   menu_display_frame_info_t menu_disp_info;
   unsigned width                          = video_info->width;
   unsigned height                         = video_info->height;
   bool render_background                  = false;
   file_list_t *selection_buf              = NULL;
   file_list_t *menu_stack                 = NULL;
   xmb_handle_t *xmb                       = (xmb_handle_t*)data;

   if (!xmb)
      return;

   menu_disp_info.shadows_enable           = video_info->xmb_shadows_enable;

   msg[0]             = '\0';
   title_msg[0]       = '\0';
   title_truncated[0] = '\0';

   menu_display_font_bind_block(xmb->font, &xmb->raster_block);
   menu_display_font_bind_block(xmb->font2, &xmb->raster_block2);

   xmb->raster_block.carr.coords.vertices = 0;
   xmb->raster_block2.carr.coords.vertices = 0;

   for (i = 0; i < 16; i++)
   {
      coord_black[i]  = 0;
      coord_white[i] = 1.0f;
      item_color[i]   = 1.0f;
   }

   menu_display_set_alpha(coord_black, MIN(
         (float)video_info->xmb_alpha_factor/100, xmb->alpha));
   menu_display_set_alpha(coord_white, xmb->alpha);

   xmb_draw_bg(
         xmb,
         video_info,
         width,
         height,
         xmb->alpha,
         xmb->textures.bg,
         coord_black,
         coord_white);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   strlcpy(title_truncated, xmb->title_name, sizeof(title_truncated));
   if (selection > 1)
      title_truncated[25] = '\0';

   /* Title text */
   xmb_draw_text(menu_disp_info, xmb,
         title_truncated, xmb->margins.title.left,
         xmb->margins.title.top, 1, 1, TEXT_ALIGN_LEFT,
         width, height, xmb->font);

   if (menu_entries_get_core_title(title_msg, sizeof(title_msg)) == 0)
      xmb_draw_text(menu_disp_info, xmb, title_msg, xmb->margins.title.left,
            height - xmb->margins.title.bottom, 1, 1, TEXT_ALIGN_LEFT,
            width, height, xmb->font);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw);
   menu_display_blend_begin();

   /* Clock image */
   menu_display_set_alpha(coord_white, MIN(xmb->alpha, 1.00f));

   if (video_info->battery_level_enable)
   {
      char msg[12];
      static retro_time_t last_time  = 0;
      bool charging                  = false;
      retro_time_t current_time      = cpu_features_get_time_usec();
      int percent                    = 0;
      enum frontend_powerstate state = get_last_powerstate(&percent);

      if (state == FRONTEND_POWERSTATE_CHARGING)
         charging = true;

      if (current_time - last_time >= BATTERY_LEVEL_CHECK_INTERVAL)
      {
         last_time = current_time;
         task_push_get_powerstate();
      }

      *msg = '\0';

      if (percent > 0)
      {
         size_t x_pos      = xmb->icon.size / 6;
         size_t x_pos_icon = xmb->margins.title.left;

         if (coord_white[3] != 0)
            xmb_draw_icon(
                  menu_disp_info,
                  xmb->icon.size,
                  &mymat,
                  xmb->textures.list[charging 
                  ? XMB_TEXTURE_BATTERY_CHARGING : XMB_TEXTURE_BATTERY_FULL],
                  width - (xmb->icon.size / 2) - x_pos_icon,
                  xmb->icon.size,
                  width,
                  height,
                  1,
                  0,
                  1,
                  &coord_white[0],
                  xmb->shadow_offset);

         snprintf(msg, sizeof(msg), "%d%%", percent);

         percent_width = font_driver_get_message_width(xmb->font, msg, utf8len(msg), 1);

         xmb_draw_text(menu_disp_info, xmb, msg,
               width - xmb->margins.title.left - x_pos,
               xmb->margins.title.top, 1, 1, TEXT_ALIGN_RIGHT,
               width, height, xmb->font);
      }
   }

   if (video_info->timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];
      int x_pos = 0;

      if (coord_white[3] != 0)
      {
         int x_pos = 0;

         if (percent_width)
            x_pos = percent_width + (xmb->icon.size / 2.5);

         xmb_draw_icon(
               menu_disp_info,
               xmb->icon.size,
               &mymat,
               xmb->textures.list[XMB_TEXTURE_CLOCK],
               width - xmb->icon.size - x_pos,
               xmb->icon.size,width,
               height,
               1,
               0,
               1,
               &coord_white[0],
               xmb->shadow_offset);
      }

      timedate[0]        = '\0';

      datetime.s         = timedate;
      datetime.len       = sizeof(timedate);
      datetime.time_mode = 4;

      menu_display_timedate(&datetime);

      if (percent_width)
         x_pos = percent_width + (xmb->icon.size / 2.5);

      xmb_draw_text(menu_disp_info, xmb, timedate,
            width - xmb->margins.title.left - xmb->icon.size / 4 - x_pos,
            xmb->margins.title.top, 1, 1, TEXT_ALIGN_RIGHT,
            width, height, xmb->font);
   }

   /* Arrow image */
   menu_display_set_alpha(coord_white, MIN(xmb->textures.arrow.alpha, xmb->alpha));

   if (coord_white[3] != 0)
      xmb_draw_icon(
            menu_disp_info,
            xmb->icon.size,
            &mymat,
            xmb->textures.list[XMB_TEXTURE_ARROW],
            xmb->x + xmb->margins.screen.left +
            xmb->icon.spacing.horizontal - xmb->icon.size / 2.0 + xmb->icon.size,
            xmb->margins.screen.top +
            xmb->icon.size / 2.0 + xmb->icon.spacing.vertical
            * xmb->active_item_factor,
            width,
            height,
            xmb->textures.arrow.alpha,
            0,
            1,
            &coord_white[0],
            xmb->shadow_offset);

   menu_display_blend_begin();

   /* Horizontal tab icons */
   for (i = 0; i <= xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

      if (!node)
         continue;

      menu_display_set_alpha(item_color, MIN(node->alpha, xmb->alpha));

      if (item_color[3] != 0)
      {
         menu_display_ctx_rotate_draw_t rotate_draw;
         math_matrix_4x4 mymat;
         uintptr_t texture        = node->icon;
         float x                  = xmb->x + xmb->categories.x_pos +
                                    xmb->margins.screen.left +
                                    xmb->icon.spacing.horizontal * (i + 1) - xmb->icon.size / 2.0;
         float y                  = xmb->margins.screen.top + xmb->icon.size / 2.0;
         float rotation           = 0;
         float scale_factor       = node->zoom;

         rotate_draw.matrix       = &mymat;
         rotate_draw.rotation     = rotation;
         rotate_draw.scale_x      = scale_factor;
         rotate_draw.scale_y      = scale_factor;
         rotate_draw.scale_z      = 1;
         rotate_draw.scale_enable = true;

         menu_display_rotate_z(&rotate_draw);

         xmb_draw_icon(
               menu_disp_info,
               xmb->icon.size,
               &mymat,
               texture,
               x,
               y,
               width,
               height,
               1.0,
               rotation,
               scale_factor,
               &item_color[0],
               xmb->shadow_offset);
      }
   }

   menu_display_blend_end();

   /* Vertical icons */
   xmb_draw_items(
         video_info,
         menu_disp_info,
         xmb,
         xmb->selection_buf_old,
         xmb->menu_stack_old,
         xmb->selection_ptr_old,
         (xmb_list_get_size(xmb, MENU_LIST_PLAIN) > 1)
         ? xmb->categories.selection_ptr : xmb->categories.selection_ptr_old,
         &item_color[0],
         width,
         height);

   selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_stack    = menu_entries_get_menu_stack_ptr(0);

   xmb_draw_items(
         video_info,
         menu_disp_info,
         xmb,
         selection_buf,
         menu_stack,
         selection,
         xmb->categories.selection_ptr,
         &item_color[0],
         width,
         height);

   menu_display_font_flush_block(video_info->width, video_info->height, xmb->font);
   menu_display_font_flush_block(video_info->width, video_info->height, xmb->font2);

   if (xmb->savestate_thumbnail)
      xmb_draw_thumbnail(menu_disp_info,
            xmb, &coord_white[0], width, height,
            xmb->savestate_thumbnail_width, xmb->savestate_thumbnail_height,
            xmb->savestate_thumbnail);
   else if (xmb->thumbnail
      && !string_is_equal(xmb_thumbnails_ident(),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
   {
#ifdef XMB_DEBUG
      RARCH_LOG("[XMB thumbnail] width: %.2f, height: %.2f\n", xmb->thumbnail_width, xmb->thumbnail_height);
      RARCH_LOG("[XMB thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

      xmb_draw_thumbnail(menu_disp_info,
            xmb, &coord_white[0], width, height,
            xmb->thumbnail_width, xmb->thumbnail_height,
            xmb->thumbnail);
   }

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      render_background = true;
   }

   if (!string_is_empty(xmb->box_message))
   {
      strlcpy(msg, xmb->box_message,
            sizeof(msg));
      xmb->box_message[0] = '\0';
      render_background = true;
   }

   if (render_background)
   {
      xmb_draw_dark_layer(xmb, width, height);

      xmb_render_messagebox_internal(menu_disp_info, video_info, xmb, msg);
   }

   /* Cursor image */
   if (xmb->mouse_show)
   {
      menu_display_set_alpha(coord_white, MIN(xmb->alpha, 1.00f));
      menu_display_draw_cursor(
            &coord_white[0],
            xmb->cursor.size,
            xmb->textures.list[XMB_TEXTURE_POINTER],
            menu_input_mouse_state(MENU_MOUSE_X_AXIS),
            menu_input_mouse_state(MENU_MOUSE_Y_AXIS),
            width,
            height);
   }

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void xmb_layout_ps3(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();

   float scale_factor            = 
      (settings->menu.xmb.scale_factor * width) / (1920.0 * 100);

   xmb->above_subitem_offset     =   1.5;
   xmb->above_item_offset        =  -1.0;
   xmb->active_item_factor       =   3.0;
   xmb->under_item_offset        =   5.0;

   xmb->categories.active.zoom   = 1.0;
   xmb->categories.passive.zoom  = 0.5;
   xmb->items.active.zoom        = 1.0;
   xmb->items.passive.zoom       = 0.5;

   xmb->categories.active.alpha  = 1.0;
   xmb->categories.passive.alpha = 0.85;
   xmb->items.active.alpha       = 1.0;
   xmb->items.passive.alpha      = 0.85;

   xmb->shadow_offset            = 2.0;

   new_font_size                 = 32.0  * scale_factor;
   xmb->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;


   xmb->thumbnail_width          = 460.0 * scale_factor;
   xmb->savestate_thumbnail_width= 460.0 * scale_factor;
   xmb->cursor.size              = 64.0;

   xmb->icon.spacing.horizontal  = 200.0 * scale_factor;
   xmb->icon.spacing.vertical    = 64.0 * scale_factor;

   xmb->margins.screen.top       = (256+32) * scale_factor;
   xmb->margins.screen.left      = 336.0 * scale_factor;

   xmb->margins.title.left       = 60 * scale_factor;
   xmb->margins.title.top        = 60 * scale_factor + new_font_size / 3;
   xmb->margins.title.bottom     = 60 * scale_factor - new_font_size / 3;

   xmb->margins.label.left       = 85.0 * scale_factor;
   xmb->margins.label.top        = new_font_size / 3.0;

   xmb->margins.setting.left     = 600.0 * scale_factor;

   xmb->icon.size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;

#ifdef XMB_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  xmb->margins.screen.left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  xmb->margins.screen.top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  xmb->margins.title.left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  xmb->margins.title.top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  xmb->margins.title.bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  xmb->margins.label.left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  xmb->margins.label.top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  xmb->margins.setting.left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  xmb->icon.spacing.horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  xmb->icon.spacing.vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  xmb->icon.size);
#endif

   menu_display_set_header_height(new_header_height);
}

static void xmb_layout_psp(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();
   float scale_factor            = 
      ((settings->menu.xmb.scale_factor * width) / (1920.0 * 100)) * 1.5;

#ifdef _3DS
   scale_factor = settings->menu.xmb.scale_factor / 400.0;
#endif

   xmb->above_subitem_offset     =  1.5;
   xmb->above_item_offset        = -1.0;
   xmb->active_item_factor       =  2.0;
   xmb->under_item_offset        =  3.0;

   xmb->categories.active.zoom   = 1.0;
   xmb->categories.passive.zoom  = 1.0;
   xmb->items.active.zoom        = 1.0;
   xmb->items.passive.zoom       = 1.0;

   xmb->categories.active.alpha  = 1.0;
   xmb->categories.passive.alpha = 0.85;
   xmb->items.active.alpha       = 1.0;
   xmb->items.passive.alpha      = 0.85;

   xmb->shadow_offset            = 1.0;

   new_font_size                 = 32.0  * scale_factor;
   xmb->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;
   xmb->margins.screen.top       = (256+32) * scale_factor;

   xmb->thumbnail_width          = 460.0 * scale_factor;
   xmb->savestate_thumbnail_width= 460.0 * scale_factor;
   xmb->cursor.size              = 64.0;

   xmb->icon.spacing.horizontal  = 250.0 * scale_factor;
   xmb->icon.spacing.vertical    = 108.0 * scale_factor;

   xmb->margins.screen.left      = 136.0 * scale_factor;
   xmb->margins.title.left       = 60 * scale_factor;
   xmb->margins.title.top        = 60 * scale_factor + new_font_size / 3;
   xmb->margins.title.bottom     = 60 * scale_factor - new_font_size / 3;
   xmb->margins.label.left       = 85.0 * scale_factor;
   xmb->margins.label.top        = new_font_size / 3.0;
   xmb->margins.setting.left     = 600.0 * scale_factor;
   xmb->icon.size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;

#ifdef XMB_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  xmb->margins.screen.left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  xmb->margins.screen.top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  xmb->margins.title.left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  xmb->margins.title.top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  xmb->margins.title.bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  xmb->margins.label.left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  xmb->margins.label.top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  xmb->margins.setting.left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  xmb->icon.spacing.horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  xmb->icon.spacing.vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  xmb->icon.size);
#endif

   menu_display_set_header_height(new_header_height);
}

static void xmb_layout(xmb_handle_t *xmb)
{
   size_t selection;
   unsigned width, height, i, current, end;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   video_driver_get_size(&width, &height);

   /* Mimic the layout of the PSP instead of the PS3 on tiny screens */
   if (width > 320 && height > 240)
      xmb_layout_ps3(xmb, width);
   else
      xmb_layout_psp(xmb, width);

   current = selection;
   end     = menu_entries_get_end();

   for (i = 0; i < end; i++)
   {
      float ia = xmb->items.passive.alpha;
      float iz = xmb->items.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(
            selection_buf, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia = xmb->items.active.alpha;
         iz = xmb->items.active.alpha;
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
      float iz = xmb->items.passive.zoom;
      xmb_node_t *node = (xmb_node_t*)menu_entries_get_userdata_at_offset(
            xmb->selection_buf_old, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia = xmb->items.active.alpha;
         iz = xmb->items.active.alpha;
      }

      node->alpha       = ia;
      node->label_alpha = 0;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
      node->x           = xmb->icon.size * 1 * -2;
   }
}

static void xmb_ribbon_set_vertex(float *ribbon_verts,
      unsigned idx, unsigned row, unsigned col)
{
   ribbon_verts[idx++] = ((float)col) / (XMB_RIBBON_COLS-1) * 2.0f - 1.0f;
   ribbon_verts[idx++] = ((float)row) / (XMB_RIBBON_ROWS-1) * 2.0f - 1.0f;
}

static void xmb_init_ribbon(xmb_handle_t * xmb)
{
   video_coords_t coords;
   unsigned vertices_total;
   unsigned r, c, col;
   unsigned i                = 0;
   float *ribbon_verts       = NULL;
   float *dummy              = NULL;
   video_coord_array_t *ca   = menu_display_get_coords_array();

   vertices_total = XMB_RIBBON_VERTICES;

   dummy          = (float*)calloc(4 * vertices_total, sizeof(float));
   ribbon_verts   = (float*)calloc(2 * vertices_total, sizeof(float));


   /* Set up vertices */
   for (r = 0; r < XMB_RIBBON_ROWS - 1; r++)
   {
      for (c = 0; c < XMB_RIBBON_COLS; c++)
      {
         col = r % 2 ? XMB_RIBBON_COLS - c - 1 : c;
         xmb_ribbon_set_vertex(ribbon_verts, i,     r,     col);
         xmb_ribbon_set_vertex(ribbon_verts, i + 2, r + 1, col);
         i  += 4;
      }
   }

   coords.color         = dummy;
   coords.vertex        = ribbon_verts;
   coords.tex_coord     = dummy;
   coords.lut_tex_coord = dummy;
   coords.vertices      = vertices_total;

   video_coord_array_append(ca, &coords, coords.vertices);

   free(dummy);
   free(ribbon_verts);
}



static void *xmb_init(void **userdata)
{
   unsigned width, height;
   xmb_handle_t *xmb          = NULL;
   settings_t *settings       = config_get_ptr();
   menu_handle_t *menu        = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   if (!menu_display_init_first_driver())
      goto error;

   video_driver_get_size(&width, &height);

   xmb = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!xmb)
      goto error;

   *userdata = xmb;

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

   xmb->system_tab_end                = 0;
   xmb->tabs[xmb->system_tab_end]     = XMB_SYSTEM_TAB_MAIN;
   if (settings->menu.xmb.show_settings)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_SETTINGS;
   if (settings->menu.xmb.show_history)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->menu.xmb.show_images)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_IMAGES;
#endif
#ifdef HAVE_FFMPEG
   if (settings->menu.xmb.show_music)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_MUSIC;
   if (settings->menu.xmb.show_video)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_LIBRETRODB
	if (settings->menu.xmb.show_add)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_ADD;
#endif
#ifdef HAVE_NETWORKING
   xmb->tabs[++xmb->system_tab_end]    = XMB_SYSTEM_TAB_NETPLAY;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   menu_display_set_width(width);
   menu_display_set_height(height);

   menu_display_allocate_white_texture();

   xmb->horizontal_list         = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (xmb->horizontal_list)
      xmb_init_horizontal_list(xmb);

   xmb_init_ribbon(xmb);

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
         file_list_free(xmb->horizontal_list);
      xmb->horizontal_list = NULL;
   }
   return NULL;
}

static void xmb_free(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (xmb)
   {
      if (xmb->menu_stack_old)
         file_list_free(xmb->menu_stack_old);
      xmb->menu_stack_old = NULL;

      if (xmb->selection_buf_old)
         file_list_free(xmb->selection_buf_old);
      xmb->selection_buf_old = NULL;
      if (xmb->horizontal_list)
         file_list_free(xmb->horizontal_list);
      xmb->horizontal_list = NULL;

      video_coord_array_free(&xmb->raster_block.carr);
      video_coord_array_free(&xmb->raster_block2.carr);
   }

   font_driver_bind_block(NULL, NULL);

}

static void xmb_context_bg_destroy(xmb_handle_t *xmb)
{
   if (!xmb)
      return;
   video_driver_texture_unload(&xmb->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
}

static bool xmb_load_image(void *userdata, void *data, enum menu_image_type type)
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
         video_driver_texture_unload(&xmb->textures.bg);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR,
               &xmb->textures.bg);
         menu_display_allocate_white_texture();
         break;
      case MENU_IMAGE_THUMBNAIL:
         {
            struct texture_image *img  = (struct texture_image*)data;
            xmb->thumbnail_orig_width  = (float)img->width;
            xmb->thumbnail_orig_height = (float)img->height;
            xmb->thumbnail_height      = xmb->thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&xmb->thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &xmb->thumbnail);
         }
         break;
      case MENU_IMAGE_SAVESTATE_THUMBNAIL:
         {
            struct texture_image *img       = (struct texture_image*)data;
            xmb->thumbnail_orig_width       = (float)img->width;
            xmb->thumbnail_orig_height      = (float)img->height;
            xmb->savestate_thumbnail_height = xmb->savestate_thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&xmb->savestate_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &xmb->savestate_thumbnail);
         }
         break;
   }

   return true;
}

static const char *xmb_texture_path(unsigned id)
{
   switch (id)
   {
      case XMB_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case XMB_TEXTURE_SETTINGS:
         return "settings.png";
      case XMB_TEXTURE_HISTORY:
         return "history.png";
#ifdef HAVE_FFMPEG
      case XMB_TEXTURE_MUSICS:
         return "musics.png";
      case XMB_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case XMB_TEXTURE_IMAGES:
         return "images.png";
#endif
      case XMB_TEXTURE_SETTING:
         return "setting.png";
      case XMB_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case XMB_TEXTURE_ARROW:
         return "arrow.png";
      case XMB_TEXTURE_RUN:
         return "run.png";
      case XMB_TEXTURE_CLOSE:
         return "close.png";
      case XMB_TEXTURE_RESUME:
         return "resume.png";
      case XMB_TEXTURE_CLOCK:
         return "clock.png";
      case XMB_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case XMB_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case XMB_TEXTURE_POINTER:
         return "pointer.png";
      case XMB_TEXTURE_SAVESTATE:
         return "savestate.png";
      case XMB_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case XMB_TEXTURE_UNDO:
         return "undo.png";
      case XMB_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case XMB_TEXTURE_WIFI:
         return "wifi.png";
      case XMB_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case XMB_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case XMB_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case XMB_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case XMB_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case XMB_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case XMB_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case XMB_TEXTURE_RELOAD:
         return "reload.png";
      case XMB_TEXTURE_FILE:
         return "file.png";
      case XMB_TEXTURE_FOLDER:
         return "folder.png";
      case XMB_TEXTURE_ZIP:
         return "zip.png";
      case XMB_TEXTURE_MUSIC:
         return "music.png";
      case XMB_TEXTURE_IMAGE:
         return "image.png";
      case XMB_TEXTURE_MOVIE:
         return "movie.png";
      case XMB_TEXTURE_CORE:
         return "core.png";
      case XMB_TEXTURE_RDB:
         return "database.png";
      case XMB_TEXTURE_CURSOR:
         return "cursor.png";
      case XMB_TEXTURE_SWITCH_ON:
         return "on.png";
      case XMB_TEXTURE_SWITCH_OFF:
         return "off.png";
      case XMB_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case XMB_TEXTURE_NETPLAY:
         return "wifi.png";
#endif
      case XMB_TEXTURE_KEY:
         return "key.png";
      case XMB_TEXTURE_KEY_HOVER:
         return "key-hover.png";
   }

   return NULL;
}

static void xmb_context_reset_textures(
      xmb_handle_t *xmb, const char *iconpath)
{
   unsigned i;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      menu_display_reset_textures_list(xmb_texture_path(i), iconpath, &xmb->textures.list[i]);

   menu_display_allocate_white_texture();

   xmb->main_menu_node.icon     = xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
   xmb->main_menu_node.alpha    = xmb->categories.active.alpha;
   xmb->main_menu_node.zoom     = xmb->categories.active.zoom;

   xmb->settings_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS];
   xmb->settings_tab_node.alpha = xmb->categories.active.alpha;
   xmb->settings_tab_node.zoom  = xmb->categories.active.zoom;

   xmb->history_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_HISTORY];
   xmb->history_tab_node.alpha  = xmb->categories.active.alpha;
   xmb->history_tab_node.zoom   = xmb->categories.active.zoom;

#ifdef HAVE_FFMPEG
   xmb->music_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MUSICS];
   xmb->music_tab_node.alpha    = xmb->categories.active.alpha;
   xmb->music_tab_node.zoom     = xmb->categories.active.zoom;

   xmb->video_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MOVIES];
   xmb->video_tab_node.alpha    = xmb->categories.active.alpha;
   xmb->video_tab_node.zoom     = xmb->categories.active.zoom;
#endif

#ifdef HAVE_IMAGEVIEWER
   xmb->images_tab_node.icon    = xmb->textures.list[XMB_TEXTURE_IMAGES];
   xmb->images_tab_node.alpha   = xmb->categories.active.alpha;
   xmb->images_tab_node.zoom    = xmb->categories.active.zoom;
#endif

   xmb->add_tab_node.icon       = xmb->textures.list[XMB_TEXTURE_ADD];
   xmb->add_tab_node.alpha      = xmb->categories.active.alpha;
   xmb->add_tab_node.zoom       = xmb->categories.active.zoom;

#ifdef HAVE_NETWORKING
   xmb->netplay_tab_node.icon       = xmb->textures.list[XMB_TEXTURE_NETPLAY];
   xmb->netplay_tab_node.alpha      = xmb->categories.active.alpha;
   xmb->netplay_tab_node.zoom       = xmb->categories.active.zoom;
#endif
}

static void xmb_context_reset_background(const char *iconpath)
{
   char path[PATH_MAX_LENGTH];
   settings_t *settings        = config_get_ptr();

   path[0] = '\0';

   fill_pathname_join(path, iconpath, "bg.png", sizeof(path));

   if (!string_is_empty(settings->path.menu_wallpaper))
      strlcpy(path, settings->path.menu_wallpaper, sizeof(path));


   if (path_file_exists(path))
      task_push_image_load(path,
            menu_display_handle_wallpaper_upload, NULL);
}

static void xmb_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH];
   xmb_handle_t *xmb               = (xmb_handle_t*)data;
   if (!xmb)
      return;

   iconpath[0] = '\0';

   fill_pathname_application_special(xmb->background_file_path,
         sizeof(xmb->background_file_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

   fill_pathname_application_special(iconpath, sizeof(iconpath),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

   xmb_layout(xmb);
   xmb->font = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT, xmb->font_size);
   xmb->font2 = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT, xmb->font2_size);
   xmb_context_reset_textures(xmb, iconpath);
   xmb_context_reset_background(iconpath);
   xmb_context_reset_horizontal_list(xmb);

   if (!string_is_equal(xmb_thumbnails_ident(),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      xmb_update_thumbnail_image(xmb);
   xmb_update_savestate_thumbnail_image(xmb);
}

static void xmb_navigation_clear(void *data, bool pending_push)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   if (!pending_push)
      xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_pointer_changed(void *data)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_set(void *data, bool scroll)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_alphabet(void *data, size_t *unused)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
      size_t list_size)
{
   size_t selection;
   int current            = 0;
   int i                  = list_size;
   xmb_node_t *node       = NULL;
   xmb_handle_t *xmb      = (xmb_handle_t*)userdata;

   if (!xmb || !list)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
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

   if (!string_is_empty(fullpath))
      strlcpy(node->fullpath, fullpath, sizeof(node->fullpath));

   node->alpha       = xmb->items.passive.alpha;
   node->zoom        = xmb->items.passive.zoom;
   node->label_alpha = node->alpha;
   node->y           = xmb_item_y(xmb, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = xmb->items.active.alpha;
      node->label_alpha = xmb->items.active.alpha;
      node->zoom        = xmb->items.active.alpha;
   }

   file_list_set_userdata(list, i, node);
}

static void xmb_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list->size;

   for (i = 0; i < size; ++i)
   {
      menu_animation_ctx_subject_t subject;
      float *subjects[5];
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      subjects[0] = &node->alpha;
      subjects[1] = &node->label_alpha;
      subjects[2] = &node->zoom;
      subjects[3] = &node->x;
      subjects[4] = &node->y;

      subject.count = 5;
      subject.data  = subjects;

      menu_animation_ctl(MENU_ANIMATION_CTL_KILL_BY_SUBJECT, &subject);

      file_list_free_userdata(list, i);
   }
}

static void xmb_list_deep_copy(const file_list_t *src, file_list_t *dst)
{
   size_t i;
   size_t size = dst->size;

   for (i = 0; i < size; ++i)
   {
      xmb_node_t *node = (xmb_node_t*)
         menu_entries_get_userdata_at_offset(dst, i);

      if (node)
      {
         menu_animation_ctx_subject_t subject;
         float *subjects[5];

         subjects[0]   = &node->alpha;
         subjects[1]   = &node->label_alpha;
         subjects[2]   = &node->zoom;
         subjects[3]   = &node->x;
         subjects[4]   = &node->y;

         subject.count = 5;
         subject.data  = subjects;

         menu_animation_ctl(MENU_ANIMATION_CTL_KILL_BY_SUBJECT, &subject);
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

static void xmb_list_cache(void *data, enum menu_list_type type, unsigned action)
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

   list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

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

         switch (xmb_get_system_tab(xmb, xmb->categories.selection_ptr))
         {
            case XMB_SYSTEM_TAB_MAIN:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS;
               break;
            case XMB_SYSTEM_TAB_SETTINGS:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS_TAB;
               break;
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_IMAGES_TAB;
               break;
#endif
#ifdef HAVE_FFMPEG
            case XMB_SYSTEM_TAB_MUSIC:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_MUSIC_TAB;
               break;
            case XMB_SYSTEM_TAB_VIDEO:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_VIDEO_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_HISTORY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_HISTORY_TAB;
               break;
            case XMB_SYSTEM_TAB_ADD:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_ADD_TAB;
               break;
#ifdef HAVE_NETWORKING
            case XMB_SYSTEM_TAB_NETPLAY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_NETPLAY_TAB;
               break;
#endif
            default:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTING_HORIZONTAL_MENU;
               break;
         }
         break;
      default:
         break;
   }
}


static void xmb_context_destroy(void *data)
{
   unsigned i;
   xmb_handle_t *xmb   = (xmb_handle_t*)data;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&xmb->textures.list[i]);

   xmb_context_destroy_horizontal_list(xmb);
   xmb_context_bg_destroy(xmb);

   menu_display_font_free(xmb->font);
   menu_display_font_free(xmb->font2);

   xmb->font = NULL;
   xmb->font2 = NULL;
}

static void xmb_toggle(void *userdata, bool menu_on)
{
   menu_animation_ctx_entry_t entry;
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

   entry.duration     = XMB_DELAY * 2;
   entry.target_value = 1.0f;
   entry.subject      = &xmb->alpha;
   entry.easing_enum  = EASING_OUT_QUAD;
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   xmb_toggle_horizontal_list(xmb);
}

static int deferred_push_content_actions(menu_displaylist_info_t *info)
{
   if (!menu_displaylist_ctl(
         DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info))
      return -1;
   menu_displaylist_ctl(DISPLAYLIST_PROCESS, info);
   return 0;
}

static int xmb_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int xmb_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (xmb_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int xmb_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret = -1;
   menu_handle_t *menu   = (menu_handle_t*)data;

   switch (type)
   {
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         entry.data            = menu;
         entry.info            = info;
         entry.parse_type      = PARSE_ACTION;
         entry.add_empty_entry = false;

         if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         {
            entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
            menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
         }

         if (menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
         {
            entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
            menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
         }

#ifndef HAVE_DYNAMIC
         if (frontend_driver_has_fork())
#endif
         {
            entry.enum_idx   = MENU_ENUM_LABEL_CORE_LIST;
            menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
         }

         entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);

         entry.enum_idx      = MENU_ENUM_LABEL_ADD_CONTENT_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#if defined(HAVE_NETWORKING)
         entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);

         entry.enum_idx      = MENU_ENUM_LABEL_NETPLAY;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#endif
         entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#ifndef HAVE_DYNAMIC
         entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#endif
         entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);

         entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#if !defined(IOS)
         entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#endif
         entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);

         entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
         info->need_push    = true;
         ret = 0;
         break;
   }
   return ret;
}

static bool xmb_menu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label,
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU), sizeof(info.label));
   info.enum_idx = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      return false;

   info.need_push = true;

   if (!menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info))
      return false;

   return true;
}

static int xmb_pointer_tap(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection;
   unsigned header_height = menu_display_get_header_height();

   if (y < header_height)
   {
      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
      return menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      size_t idx;
      bool scroll              = false;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
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
   xmb_update_thumbnail_path,
   xmb_update_thumbnail_image,
   xmb_osk_ptr_at_pos,
   xmb_update_savestate_thumbnail_path,
   xmb_update_savestate_thumbnail_image
};
