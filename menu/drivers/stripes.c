/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018 - Alfredo Monclús
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
#include <streams/file_stream.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_entries.h"
#include "../menu_input.h"

#include "../../core_info.h"
#include "../../core.h"

#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_osk.h"
#include "../widgets/menu_filebrowser.h"

#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../playlist.h"
#include "../../retroarch.h"

#include "../../tasks/tasks_internal.h"

#include "../../cheevos/badges.h"
#include "../../content.h"

#define STRIPES_RIBBON_ROWS 64
#define STRIPES_RIBBON_COLS 64
#define STRIPES_RIBBON_VERTICES 2*STRIPES_RIBBON_COLS*STRIPES_RIBBON_ROWS-2*STRIPES_RIBBON_COLS

#ifndef STRIPES_DELAY
#define STRIPES_DELAY 166
#endif

#define BATTERY_LEVEL_CHECK_INTERVAL (30 * 1000000)

#if 0
#define STRIPES_DEBUG
#endif

/* NOTE: If you change this you HAVE to update
 * stripes_alloc_node() and stripes_copy_node() */
typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   float width;
   uintptr_t icon;
   uintptr_t content_icon;
   char *fullpath;
} stripes_node_t;

enum
{
   STRIPES_TEXTURE_MAIN_MENU = 0,
   STRIPES_TEXTURE_SETTINGS,
   STRIPES_TEXTURE_HISTORY,
   STRIPES_TEXTURE_FAVORITES,
   STRIPES_TEXTURE_MUSICS,
#ifdef HAVE_FFMPEG
   STRIPES_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   STRIPES_TEXTURE_NETPLAY,
   STRIPES_TEXTURE_ROOM,
/* stub these out until we have the icons
   STRIPES_TEXTURE_ROOM_LAN,
   STRIPES_TEXTURE_ROOM_MITM,*/
#endif
#ifdef HAVE_IMAGEVIEWER
   STRIPES_TEXTURE_IMAGES,
#endif
   STRIPES_TEXTURE_SETTING,
   STRIPES_TEXTURE_SUBSETTING,
   STRIPES_TEXTURE_ARROW,
   STRIPES_TEXTURE_RUN,
   STRIPES_TEXTURE_CLOSE,
   STRIPES_TEXTURE_RESUME,
   STRIPES_TEXTURE_SAVESTATE,
   STRIPES_TEXTURE_LOADSTATE,
   STRIPES_TEXTURE_UNDO,
   STRIPES_TEXTURE_CORE_INFO,
   STRIPES_TEXTURE_WIFI,
   STRIPES_TEXTURE_CORE_OPTIONS,
   STRIPES_TEXTURE_INPUT_REMAPPING_OPTIONS,
   STRIPES_TEXTURE_CHEAT_OPTIONS,
   STRIPES_TEXTURE_DISK_OPTIONS,
   STRIPES_TEXTURE_SHADER_OPTIONS,
   STRIPES_TEXTURE_ACHIEVEMENT_LIST,
   STRIPES_TEXTURE_SCREENSHOT,
   STRIPES_TEXTURE_RELOAD,
   STRIPES_TEXTURE_RENAME,
   STRIPES_TEXTURE_FILE,
   STRIPES_TEXTURE_FOLDER,
   STRIPES_TEXTURE_ZIP,
   STRIPES_TEXTURE_FAVORITE,
   STRIPES_TEXTURE_ADD_FAVORITE,
   STRIPES_TEXTURE_MUSIC,
   STRIPES_TEXTURE_IMAGE,
   STRIPES_TEXTURE_MOVIE,
   STRIPES_TEXTURE_CORE,
   STRIPES_TEXTURE_RDB,
   STRIPES_TEXTURE_CURSOR,
   STRIPES_TEXTURE_SWITCH_ON,
   STRIPES_TEXTURE_SWITCH_OFF,
   STRIPES_TEXTURE_CLOCK,
   STRIPES_TEXTURE_BATTERY_FULL,
   STRIPES_TEXTURE_BATTERY_CHARGING,
   STRIPES_TEXTURE_POINTER,
   STRIPES_TEXTURE_ADD,
   STRIPES_TEXTURE_KEY,
   STRIPES_TEXTURE_KEY_HOVER,
   STRIPES_TEXTURE_DIALOG_SLICE,
   STRIPES_TEXTURE_LAST
};

enum
{
   STRIPES_SYSTEM_TAB_MAIN = 0,
   STRIPES_SYSTEM_TAB_SETTINGS,
   STRIPES_SYSTEM_TAB_HISTORY,
   STRIPES_SYSTEM_TAB_FAVORITES,
   STRIPES_SYSTEM_TAB_MUSIC,
#ifdef HAVE_FFMPEG
   STRIPES_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   STRIPES_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   STRIPES_SYSTEM_TAB_NETPLAY,
#endif
   STRIPES_SYSTEM_TAB_ADD,

   /* End of this enum - use the last one to determine num of possible tabs */
   STRIPES_SYSTEM_TAB_MAX_LENGTH
};

typedef struct stripes_handle
{
   bool mouse_show;

   uint8_t system_tab_end;
   uint8_t tabs[STRIPES_SYSTEM_TAB_MAX_LENGTH];

   int depth;
   int old_depth;
   int icon_size;
   int cursor_size;

   size_t categories_selection_ptr;
   size_t categories_selection_ptr_old;
   size_t selection_ptr_old;

   unsigned categories_active_idx;
   unsigned categories_active_idx_old;
   uintptr_t thumbnail;
   uintptr_t left_thumbnail;
   uintptr_t savestate_thumbnail;

   float x;
   float alpha;
   float thumbnail_width;
   float thumbnail_height;
   float left_thumbnail_width;
   float left_thumbnail_height;
   float savestate_thumbnail_width;
   float savestate_thumbnail_height;
   float above_subitem_offset;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;
   float shadow_offset;
   float font_size;
   float font2_size;

   float margins_screen_left;
   float margins_screen_top;
   float margins_setting_left;
   float margins_title_left;
   float margins_title_top;
   float margins_title_bottom;
   float margins_label_left;
   float margins_label_top;
   float icon_spacing_horizontal;
   float icon_spacing_vertical;
   float items_active_alpha;
   float items_active_zoom;
   float items_passive_alpha;
   float items_passive_zoom;
   float margins_dialog;
   float margins_slice;
   float textures_arrow_alpha;
   float categories_x_pos;
   float categories_angle;
   float categories_active_y;
   float categories_before_y;
   float categories_after_y;
   float categories_active_x;
   float categories_before_x;
   float categories_after_x;
   float categories_passive_alpha;
   float categories_passive_zoom;
   float categories_passive_width;
   float categories_active_zoom;
   float categories_active_alpha;
   float categories_active_width;

   char title_name[255];
   char *box_message;
   char *thumbnail_system;
   char *thumbnail_content;
   char *savestate_thumbnail_file_path;
   char *thumbnail_file_path;
   char *left_thumbnail_file_path;
   char *bg_file_path;

   file_list_t *selection_buf_old;
   file_list_t *horizontal_list;

   struct
   {
      menu_texture_item bg;
      menu_texture_item list[STRIPES_TEXTURE_LAST];
   } textures;

   stripes_node_t main_menu_node;
#ifdef HAVE_IMAGEVIEWER
   stripes_node_t images_tab_node;
#endif
   stripes_node_t music_tab_node;
#ifdef HAVE_FFMPEG
   stripes_node_t video_tab_node;
#endif
   stripes_node_t settings_tab_node;
   stripes_node_t history_tab_node;
   stripes_node_t favorites_tab_node;
   stripes_node_t add_tab_node;
   stripes_node_t netplay_tab_node;

   font_data_t *font;
   font_data_t *font2;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;
} stripes_handle_t;

float stripes_scale_mod[8] = {
   1, 1, 1, 1, 1, 1, 1, 1
};

static float stripes_coord_shadow[] = {
   0, 0, 0,
   0, 0, 0,
   0, 0, 0,
   0, 0, 0,
   0, 0, 0, 0
};

static float stripes_coord_black[] = {
   0, 0, 0,
   0, 0, 0,
   0, 0, 0,
   0, 0, 0,
   0, 0, 0, 0
};

static float stripes_coord_white[] = {
   1, 1, 1,
   1, 1, 1,
   1, 1, 1,
   1, 1, 1,
   1, 1, 1, 1
};

static float stripes_item_color[] = {
   1, 1, 1,
   1, 1, 1,
   1, 1, 1,
   1, 1, 1,
   1, 1, 1, 1
};

static float HueToRGB(float v1, float v2, float vH)
{
   if (vH < 0)
      vH += 1;

   if (vH > 1)
      vH -= 1;

   if ((6 * vH) < 1)
      return (v1 + (v2 - v1) * 6 * vH);

   if ((2 * vH) < 1)
      return v2;

   if ((3 * vH) < 2)
      return (v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6);

   return v1;
}

static void HSLToRGB(float H, float S, float L, float *rgb) {
   if (S == 0)
      rgb[0] = rgb[1] = rgb[2] = L;
   else
   {
      float v1, v2;

      v2 = (L < 0.5) ? (L * (1 + S)) : ((L + S) - (L * S));
      v1 = 2 * L - v2;

      rgb[0] = HueToRGB(v1, v2, H + (1.0f / 3));
      rgb[1] = HueToRGB(v1, v2, H);
      rgb[2] = HueToRGB(v1, v2, H - (1.0f / 3));
   }
}

static void stripes_calculate_visible_range(const stripes_handle_t *stripes,
      unsigned height, size_t list_size, unsigned current,
      unsigned *first, unsigned *last);

const char* stripes_theme_ident(void)
{
   settings_t *settings = config_get_ptr();
   switch (settings->uints.menu_xmb_theme)
   {
      case XMB_ICON_THEME_FLATUI:
         return "flatui";
      case XMB_ICON_THEME_RETROACTIVE:
         return "retroactive";
      case XMB_ICON_THEME_RETROSYSTEM:
         return "retrosystem";
      case XMB_ICON_THEME_PIXEL:
         return "pixel";
      case XMB_ICON_THEME_NEOACTIVE:
         return "neoactive";
      case XMB_ICON_THEME_SYSTEMATIC:
         return "systematic";
      case XMB_ICON_THEME_DOTART:
         return "dot-art";
      case XMB_ICON_THEME_CUSTOM:
         return "custom";
      case XMB_ICON_THEME_MONOCHROME_INVERTED:
         return "monochrome-inverted";
      case XMB_ICON_THEME_MONOCHROME:
      default:
         break;
   }

   return "monochrome";
}

/* NOTE: This exists because calloc()ing stripes_node_t is expensive
 * when you can have big lists like MAME and fba playlists */
static stripes_node_t *stripes_alloc_node(void)
{
   stripes_node_t *node = (stripes_node_t*)malloc(sizeof(*node));

   node->alpha = node->label_alpha  = 0;
   node->zoom  = node->x = node->y  = 0;
   node->icon  = node->content_icon = 0;
   node->fullpath = NULL;

   return node;
}

static void stripes_free_node(stripes_node_t *node)
{
    if (!node)
        return;

    if (node->fullpath)
        free(node->fullpath);

    node->fullpath = NULL;

    free(node);
}

/**
 * @brief frees all stripes_node_t in a file_list_t
 *
 * file_list_t asumes userdata holds a simple structure and
 * free()'s it. Can't change this at the time because other
 * code depends on this behavior.
 *
 * @param list
 * @param actiondata whether to free actiondata too
 */
static void stripes_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = file_list_get_size(list);

   for (i = 0; i < size; ++i)
   {
      stripes_free_node((stripes_node_t*)file_list_get_userdata_at_offset(list, i));

      /* file_list_set_userdata() doesn't accept NULL */
      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

static stripes_node_t *stripes_copy_node(const stripes_node_t *old_node)
{
   stripes_node_t *new_node = (stripes_node_t*)malloc(sizeof(*new_node));

   *new_node            = *old_node;
   new_node->fullpath   = old_node->fullpath ? strdup(old_node->fullpath) : NULL;

   return new_node;
}

static const char *stripes_thumbnails_ident(char pos)
{
   char folder          = 0;
   settings_t *settings = config_get_ptr();

   if (pos == 'R')
      folder = settings->uints.menu_thumbnails;
   if (pos == 'L')
      folder = settings->uints.menu_left_thumbnails;

   switch (folder)
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

static size_t stripes_list_get_selection(void *data)
{
   stripes_handle_t *stripes      = (stripes_handle_t*)data;

   if (!stripes)
      return 0;

   return stripes->categories_selection_ptr;
}

static size_t stripes_list_get_size(void *data, enum menu_list_type type)
{
   stripes_handle_t *stripes       = (stripes_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         if (stripes && stripes->horizontal_list)
            return file_list_get_size(stripes->horizontal_list);
         break;
      case MENU_LIST_TABS:
         return stripes->system_tab_end;
   }

   return 0;
}

static void *stripes_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   stripes_handle_t *stripes       = (stripes_handle_t*)data;

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
         if (stripes && stripes->horizontal_list)
            list_size = file_list_get_size(stripes->horizontal_list);
         if (i < list_size)
            return (void*)&stripes->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static INLINE float stripes_item_y(const stripes_handle_t *stripes, int i, size_t current)
{
   float iy = stripes->icon_spacing_vertical;

   if (i < (int)current)
      if (stripes->depth > 1)
         iy *= (i - (int)current + stripes->above_subitem_offset);
      else
         iy *= (i - (int)current + stripes->above_item_offset);
   else
      iy    *= (i - (int)current + stripes->under_item_offset);

   if (i == (int)current)
      iy = stripes->icon_spacing_vertical * stripes->active_item_factor;

   return iy;
}

static void stripes_draw_icon(
      video_frame_info_t *video_info,
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
   draw.rotation        = rotation;
   draw.scale_factor    = scale_factor;
#if defined(VITA) || defined(WIIU)
   draw.width          *= scale_factor;
   draw.height         *= scale_factor;
#endif
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   if (video_info->xmb_shadows_enable)
   {
      menu_display_set_alpha(stripes_coord_shadow, color[3] * 0.35f);

      coords.color      = stripes_coord_shadow;
      draw.x            = x + shadow_offset;
      draw.y            = height - y - shadow_offset;

#if defined(VITA) || defined(WIIU)
      if (scale_factor < 1)
      {
         draw.x         = draw.x + (icon_size-draw.width)/2;
         draw.y         = draw.y + (icon_size-draw.width)/2;
      }
#endif
      menu_display_draw(&draw, video_info);
   }

   coords.color         = (const float*)color;
   draw.x               = x;
   draw.y               = height - y;

#if defined(VITA) || defined(WIIU)
   if (scale_factor < 1)
   {
      draw.x            = draw.x + (icon_size-draw.width)/2;
      draw.y            = draw.y + (icon_size-draw.width)/2;
   }
#endif
   menu_display_draw(&draw, video_info);
}

static void stripes_draw_text(
      video_frame_info_t *video_info,
      stripes_handle_t *stripes,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font)
{
   uint32_t color;
   uint8_t a8;
   settings_t *settings;

   if (alpha > stripes->alpha)
      alpha = stripes->alpha;

   a8       = 255 * alpha;

   /* Avoid drawing 100% transparent text */
   if (a8 == 0)
      return;

   settings = config_get_ptr();
   color = FONT_COLOR_RGBA(
         settings->uints.menu_font_color_red,
         settings->uints.menu_font_color_green,
         settings->uints.menu_font_color_blue, a8);

   menu_display_draw_text(font, str, x, y,
         width, height, color, text_align, scale_factor,
         video_info->xmb_shadows_enable,
         stripes->shadow_offset, false);
}

static void stripes_messagebox(void *data, const char *message)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;

   if (!stripes || string_is_empty(message))
      return;

   stripes->box_message = strdup(message);
}

static void stripes_render_keyboard(
      stripes_handle_t *stripes,
      video_frame_info_t *video_info,
      char **grid, unsigned id)
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

   menu_display_draw_quad(
         video_info,
         0, height/2.0, width, height/2.0,
         width, height,
         &dark[0]);

   ptr_width  = width / 11;
   ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y = (i / 11) * height / 10.0;

      if (i == id)
      {
         uintptr_t texture = stripes->textures.list[STRIPES_TEXTURE_KEY_HOVER];

         menu_display_blend_begin(video_info);

         menu_display_draw_texture(
               video_info,
               width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width,
               height/2.0 + ptr_height*1.5 + line_y,
               ptr_width, ptr_height,
               width, height,
               &white[0],
               texture);

         menu_display_blend_end(video_info);
      }

      menu_display_draw_text(stripes->font, grid[i],
            width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width + ptr_width/2.0,
            height/2.0 + ptr_height + line_y + stripes->font->size / 3,
            width, height, 0xffffffff, TEXT_ALIGN_CENTER, 1.0f,
            false, 0, false);
   }
}

/* Returns the OSK key at a given position */
static int stripes_osk_ptr_at_pos(void *data, int x, int y, unsigned width, unsigned height)
{
   unsigned i;
   int ptr_width, ptr_height;
   stripes_handle_t *stripes = (stripes_handle_t*)data;

   if (!stripes)
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

static void stripes_render_messagebox_internal(
      video_frame_info_t *video_info,
      stripes_handle_t *stripes, const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   float line_height        = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = !string_is_empty(message)
      ? string_split(message, "\n") : NULL;

   if (!list || !stripes || !stripes->font)
   {
      if (list)
         string_list_free(list);
      return;
   }

   if (list->elems == 0)
      goto end;

   line_height      = stripes->font->size * 1.2;

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list->size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg  = list->elems[i].data;
      int len          = (int)utf8len(msg);

      if (len > longest)
      {
         longest       = len;
         longest_width = font_driver_get_message_width(
               stripes->font, msg, strlen(msg), 1);
      }
   }

   menu_display_blend_begin(video_info);

   menu_display_draw_texture_slice(
         video_info,
         x - longest_width/2 - stripes->margins_dialog,
         y + stripes->margins_slice - stripes->margins_dialog,
         256, 256,
         longest_width + stripes->margins_dialog * 2,
         line_height * list->size + stripes->margins_dialog * 2,
         width, height,
         NULL,
         stripes->margins_slice, 1.0,
         stripes->textures.list[STRIPES_TEXTURE_DIALOG_SLICE]);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         menu_display_draw_text(stripes->font, msg,
               x - longest_width/2.0,
               y + (i+0.75) * line_height,
               width, height, 0x444444ff, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
   }

   if (menu_input_dialog_get_display_kb())
      stripes_render_keyboard(stripes,
            video_info,
            menu_event_get_osk_grid(),
            menu_event_get_osk_ptr());

end:
   string_list_free(list);
}

static void stripes_update_thumbnail_path(void *data, unsigned i, char pos)
{
   menu_entry_t entry;
   unsigned entry_type            = 0;
   char new_path[PATH_MAX_LENGTH] = {0};
   settings_t     *settings       = config_get_ptr();
   stripes_handle_t     *stripes          = (stripes_handle_t*)data;
   playlist_t     *playlist       = NULL;
   const char    *dir_thumbnails  = settings->paths.directory_thumbnails;

   if (!stripes || string_is_empty(dir_thumbnails))
      goto end;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, true);

   entry_type = menu_entry_get_type_new(&entry);

   if (entry_type == FILE_TYPE_IMAGEVIEWER || entry_type == FILE_TYPE_IMAGE)
   {
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(selection_buf, i);

      if (!string_is_empty(node->fullpath) &&
         (pos == 'R' || (pos == 'L' && string_is_equal(stripes_thumbnails_ident('R'),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))))
      {
         if (!string_is_empty(entry.path))
            fill_pathname_join(
                  new_path,
                  node->fullpath,
                  entry.path,
                  sizeof(new_path));

         goto end;
      }
   }
   else if (filebrowser_get_type() != FILEBROWSER_NONE)
   {
      video_driver_texture_unload(&stripes->thumbnail);
      goto end;
   }

   playlist = playlist_get_cached();

   if (playlist)
   {
      const struct playlist_entry *entry  = NULL;
      playlist_get_index(playlist, i, &entry);

      if (string_is_equal(entry->core_name, "imageviewer"))
      {
         if (pos == 'R' || (pos == 'L' && string_is_equal(stripes_thumbnails_ident('R'),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
         {
            if (!string_is_empty(entry->label))
               strlcpy(new_path, entry->label,
                     sizeof(new_path));
            goto end;
         }
         else
         {
            video_driver_texture_unload(&stripes->left_thumbnail);
            goto end;
         }
      }
   }

   /* Append thumbnail system directory */
   if (!string_is_empty(stripes->thumbnail_system))
      fill_pathname_join(
            new_path,
            dir_thumbnails,
            stripes->thumbnail_system,
            sizeof(new_path));

   if (!string_is_empty(new_path))
   {
      char            *tmp_new2      = (char*)
         malloc(PATH_MAX_LENGTH * sizeof(char));

      tmp_new2[0]                    = '\0';

      /* Append Named_Snaps/Named_Boxarts/Named_Titles */
      if (pos ==  'R')
         fill_pathname_join(tmp_new2, new_path,
               stripes_thumbnails_ident('R'), PATH_MAX_LENGTH * sizeof(char));
      if (pos ==  'L')
         fill_pathname_join(tmp_new2, new_path,
               stripes_thumbnails_ident('L'), PATH_MAX_LENGTH * sizeof(char));

      strlcpy(new_path, tmp_new2,
            PATH_MAX_LENGTH * sizeof(char));
      free(tmp_new2);
   }

   /* Scrub characters that are not cross-platform and/or violate the
    * No-Intro filename standard:
    * http://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).zip
    * Replace these characters in the entry name with underscores.
    */
   if (!string_is_empty(stripes->thumbnail_content))
   {
      char *scrub_char_pointer       = NULL;
      char            *tmp_new       = (char*)
         malloc(PATH_MAX_LENGTH * sizeof(char));
      char            *tmp           = strdup(stripes->thumbnail_content);

      tmp_new[0]                     = '\0';

      while((scrub_char_pointer = strpbrk(tmp, "&*/:`\"<>?\\|")))
         *scrub_char_pointer = '_';

      /* Look for thumbnail file with this scrubbed filename */

      fill_pathname_join(tmp_new,
            new_path,
            tmp, PATH_MAX_LENGTH * sizeof(char));

      if (!string_is_empty(tmp_new))
         strlcpy(new_path,
               tmp_new, sizeof(new_path));

      free(tmp_new);
      free(tmp);
   }

   /* Append png extension */
   if (!string_is_empty(new_path))
      strlcat(new_path, ".png", sizeof(new_path));

end:
   if (stripes && !string_is_empty(new_path))
   {
      if (pos == 'R')
         stripes->thumbnail_file_path = strdup(new_path);
      if (pos == 'L')
         stripes->left_thumbnail_file_path = strdup(new_path);
   }
}

static void stripes_update_savestate_thumbnail_path(void *data, unsigned i)
{
   menu_entry_t entry;
   settings_t     *settings = config_get_ptr();
   stripes_handle_t     *stripes    = (stripes_handle_t*)data;

   if (!stripes)
      return;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, true);

   if (!string_is_empty(stripes->savestate_thumbnail_file_path))
      free(stripes->savestate_thumbnail_file_path);
   stripes->savestate_thumbnail_file_path = NULL;

   if (!string_is_empty(entry.label))
   {
      if (     (settings->bools.savestate_thumbnail_enable)
            && ((string_is_equal(entry.label, "state_slot"))
               || (string_is_equal(entry.label, "loadstate"))
               || (string_is_equal(entry.label, "savestate"))))
      {
         size_t path_size         = 8024 * sizeof(char);
         char             *path   = (char*)malloc(8204 * sizeof(char));
         global_t         *global = global_get_ptr();

         path[0] = '\0';

         if (global)
         {
            int state_slot = settings->ints.state_slot;

            if (state_slot > 0)
               snprintf(path, path_size, "%s%d",
                     global->name.savestate, state_slot);
            else if (state_slot < 0)
               fill_pathname_join_delim(path,
                     global->name.savestate, "auto", '.', path_size);
            else
               strlcpy(path, global->name.savestate, path_size);
         }

         strlcat(path, ".png", path_size);

         if (path_is_valid(path))
         {
            if (!string_is_empty(stripes->savestate_thumbnail_file_path))
               free(stripes->savestate_thumbnail_file_path);
            stripes->savestate_thumbnail_file_path = strdup(path);
         }

         free(path);
      }
   }
}

static void stripes_update_thumbnail_image(void *data)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   bool supports_rgba        = video_driver_supports_rgba();
   if (!stripes)
      return;

   if (!(string_is_empty(stripes->thumbnail_file_path)))
      {
         if (path_is_valid(stripes->thumbnail_file_path))
            task_push_image_load(stripes->thumbnail_file_path,
                  supports_rgba, 0,
                  menu_display_handle_thumbnail_upload, NULL);
         else
            video_driver_texture_unload(&stripes->thumbnail);

         free(stripes->thumbnail_file_path);
         stripes->thumbnail_file_path = NULL;
      }

   if (!(string_is_empty(stripes->left_thumbnail_file_path)))
      {
         if (path_is_valid(stripes->left_thumbnail_file_path))
            task_push_image_load(stripes->left_thumbnail_file_path,
                  supports_rgba, 0,
                  menu_display_handle_left_thumbnail_upload, NULL);
         else
            video_driver_texture_unload(&stripes->left_thumbnail);

         free(stripes->left_thumbnail_file_path);
         stripes->left_thumbnail_file_path = NULL;
      }
}

static void stripes_refresh_thumbnail_image(void *data, unsigned i)
{
   stripes_update_thumbnail_image(data);
}

static void stripes_set_thumbnail_system(void *data, char*s, size_t len)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   if (!stripes)
      return;

   if (!string_is_empty(stripes->thumbnail_system))
      free(stripes->thumbnail_system);
   stripes->thumbnail_system = strdup(s);
}

static void stripes_get_thumbnail_system(void *data, char*s, size_t len)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   if (!stripes)
      return;

   if (!string_is_empty(stripes->thumbnail_system))
      strlcpy(s, stripes->thumbnail_system, len);
}

static void stripes_reset_thumbnail_content(void *data)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   if (!stripes)
      return;
   if (!string_is_empty(stripes->thumbnail_content))
      free(stripes->thumbnail_content);
   stripes->thumbnail_content = NULL;
}

static void stripes_set_thumbnail_content(void *data, char *s, size_t len)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   if (!stripes)
      return;
   if (!string_is_empty(stripes->thumbnail_content))
      free(stripes->thumbnail_content);
   stripes->thumbnail_content = strdup(s);
}

static void stripes_update_savestate_thumbnail_image(void *data)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;
   if (!stripes)
      return;

   if (path_is_valid(stripes->savestate_thumbnail_file_path))
      task_push_image_load(stripes->savestate_thumbnail_file_path,
            video_driver_supports_rgba(), 0,
            menu_display_handle_savestate_thumbnail_upload, NULL);
   else
      video_driver_texture_unload(&stripes->savestate_thumbnail);
}

static unsigned stripes_get_system_tab(stripes_handle_t *stripes, unsigned i)
{
   if (i <= stripes->system_tab_end)
   {
      return stripes->tabs[i];
   }
   return UINT_MAX;
}

static void stripes_selection_pointer_changed(
      stripes_handle_t *stripes, bool allow_animations)
{
   unsigned i, end, height;
   menu_animation_ctx_tag tag;
   menu_entry_t entry;
   size_t num                 = 0;
   int threshold              = 0;
   menu_list_t     *menu_list = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   const char *thumb_ident    = stripes_thumbnails_ident('R');
   const char *lft_thumb_ident= stripes_thumbnails_ident('L');

   menu_entries_ctl(MENU_ENTRIES_CTL_LIST_GET, &menu_list);
   menu_entry_init(&entry);

   if (!stripes)
      return;

   menu_entry_get(&entry, 0, selection, NULL, true);

   end       = (unsigned)menu_entries_get_size();
   threshold = stripes->icon_size * 10;

   video_driver_get_size(NULL, &height);

   tag       = (uintptr_t)selection_buf;

   menu_animation_kill_by_tag(&tag);
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &num);

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia         = stripes->items_passive_alpha;
      float iz         = stripes->items_passive_zoom;
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      iy      = stripes_item_y(stripes, i, selection);
      real_iy = iy + stripes->margins_screen_top;

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
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = STRIPES_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = tag;
         anim_entry.cb           = NULL;

         menu_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = iz;
         anim_entry.subject      = &node->zoom;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = iy;
         anim_entry.subject      = &node->y;

         menu_animation_push(&anim_entry);
      }
   }
}

static void stripes_list_open_old(stripes_handle_t *stripes,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height = 0;
   int        threshold = stripes->icon_size * 10;
   size_t           end = 0;

   end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float real_y;
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = stripes->items_active_alpha;
      if (dir == -1)
         ia = 0;

      real_y = node->y + stripes->margins_screen_top;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = ia;
         node->label_alpha = 0;
         node->x = stripes->icon_size * dir * -2;
      }
      else
      {
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = STRIPES_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = stripes->icon_size * dir * -2;
         anim_entry.subject      = &node->x;

         menu_animation_push(&anim_entry);
      }
   }
}

static void stripes_list_open_new(stripes_handle_t *stripes,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   unsigned stripes_system_tab = 0;
   size_t skip             = 0;
   int        threshold    = stripes->icon_size * 10;
   size_t           end    = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia;
      float real_y;
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (dir == 1 || (dir == -1 && i != current))
         node->alpha = 0;

      if (dir == 1 || dir == -1)
         node->label_alpha = 0;

      node->x = stripes->icon_size * dir * 2;
      node->y = stripes_item_y(stripes, i, current);
      node->zoom = stripes->categories_passive_zoom;

      real_y = node->y + stripes->margins_screen_top;

      if (i == current)
         node->zoom = stripes->categories_active_zoom;

      ia    = stripes->items_passive_alpha;
      if (i == current)
         ia = stripes->items_active_alpha;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x = 0;
      }
      else
      {
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = STRIPES_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         menu_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->x;

         menu_animation_push(&anim_entry);
      }
   }

   stripes->old_depth = stripes->depth;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &skip);

   stripes_system_tab = stripes_get_system_tab(stripes,
         (unsigned)stripes->categories_selection_ptr);

   if (stripes_system_tab <= STRIPES_SYSTEM_TAB_SETTINGS)
   {
      if (stripes->depth < 4)
         stripes_reset_thumbnail_content(stripes);
      stripes_update_thumbnail_path(stripes, 0, 'R');
      stripes_update_thumbnail_image(stripes);
      stripes_update_thumbnail_path(stripes, 0, 'L');
      stripes_update_thumbnail_image(stripes);
   }
}

static stripes_node_t *stripes_node_allocate_userdata(
      stripes_handle_t *stripes, unsigned i)
{
   stripes_node_t *tmp  = NULL;
   stripes_node_t *node = stripes_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node->alpha = stripes->categories_passive_alpha;
   node->zoom  = stripes->categories_passive_zoom;

   if ((i + stripes->system_tab_end) == stripes->categories_active_idx)
   {
      node->alpha = stripes->categories_active_alpha;
      node->zoom  = stripes->categories_active_zoom;
   }

   tmp = (stripes_node_t*)file_list_get_userdata_at_offset(
         stripes->horizontal_list, i);
   stripes_free_node(tmp);

   file_list_set_userdata(stripes->horizontal_list, i, node);

   return node;
}

static stripes_node_t* stripes_get_userdata_from_horizontal_list(
      stripes_handle_t *stripes, unsigned i)
{
   return (stripes_node_t*)
      file_list_get_userdata_at_offset(stripes->horizontal_list, i);
}

static void stripes_push_animations(stripes_node_t *node,
      uintptr_t tag, float ia, float ix)
{
   menu_animation_ctx_entry_t anim_entry;

   anim_entry.duration     = STRIPES_DELAY;
   anim_entry.target_value = ia;
   anim_entry.subject      = &node->alpha;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   anim_entry.tag          = tag;
   anim_entry.cb           = NULL;

   menu_animation_push(&anim_entry);

   anim_entry.subject      = &node->label_alpha;

   menu_animation_push(&anim_entry);

   anim_entry.target_value = ix;
   anim_entry.subject      = &node->x;

   menu_animation_push(&anim_entry);
}

static void stripes_list_switch_old(stripes_handle_t *stripes,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, first, last, height;
   size_t end = file_list_get_size(list);
   float ix   = -stripes->icon_spacing_horizontal * dir;
   float ia   = 0;

   first = 0;
   last  = end > 0 ? end - 1 : 0;

   video_driver_get_size(NULL, &height);
   stripes_calculate_visible_range(stripes, height, end,
         current, &first, &last);

   for (i = 0; i < end; i++)
   {
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i >= first && i <= last)
         stripes_push_animations(node, (uintptr_t)list, ia, ix);
      else
      {
         node->alpha = node->label_alpha = ia;
         node->x     = ix;
      }
   }
}

static void stripes_list_switch_new(stripes_handle_t *stripes,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, first, last, height;
   size_t end           = 0;
   settings_t *settings = config_get_ptr();

   if (settings->bools.menu_dynamic_wallpaper_enable)
   {
      size_t path_size = PATH_MAX_LENGTH * sizeof(char);
      char       *path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      char       *tmp  = string_replace_substring(stripes->title_name, "/", " ");

      path[0]          = '\0';

      if (tmp)
      {
         fill_pathname_join_noext(
               path,
               settings->paths.directory_dynamic_wallpapers,
               tmp,
               path_size);
         free(tmp);
      }

      strlcat(path, ".png", path_size);

      if (!path_is_valid(path))
         fill_pathname_application_special(path, path_size,
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

       if (!string_is_equal(path, stripes->bg_file_path))
       {
           if (path_is_valid(path))
           {
              task_push_image_load(path,
                    video_driver_supports_rgba(), 0,
                  menu_display_handle_wallpaper_upload, NULL);
              if (!string_is_empty(stripes->bg_file_path))
                 free(stripes->bg_file_path);
              stripes->bg_file_path = strdup(path);
           }
       }

       free(path);
   }

   end = file_list_get_size(list);

   first = 0;
   last  = end > 0 ? end - 1 : 0;

   video_driver_get_size(NULL, &height);
   stripes_calculate_visible_range(stripes, height, end, current, &first, &last);

   for (i = 0; i < end; i++)
   {
      stripes_node_t *node = (stripes_node_t*)
         file_list_get_userdata_at_offset(list, i);
      float ia         = stripes->items_passive_alpha;

      if (!node)
         continue;

      node->x           = stripes->icon_spacing_horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = stripes->items_active_alpha;

      if (i >= first && i <= last)
         stripes_push_animations(node, (uintptr_t)list, ia, 0);
      else
      {
         node->x     = 0;
         node->alpha = node->label_alpha = ia;
      }
   }
}

static void stripes_set_title(stripes_handle_t *stripes)
{
   if (stripes->categories_selection_ptr <= stripes->system_tab_end)
   {
      menu_entries_get_title(stripes->title_name, sizeof(stripes->title_name));
   }
   else
   {
      const char *path = NULL;
      menu_entries_get_at_offset(
            stripes->horizontal_list,
            stripes->categories_selection_ptr - (stripes->system_tab_end + 1),
            &path, NULL, NULL, NULL, NULL);

      if (!path)
         return;

      fill_pathname_base_noext(
            stripes->title_name, path, sizeof(stripes->title_name));
   }
}

static stripes_node_t* stripes_get_node(stripes_handle_t *stripes, unsigned i)
{
   switch (stripes_get_system_tab(stripes, i))
   {
      case STRIPES_SYSTEM_TAB_SETTINGS:
         return &stripes->settings_tab_node;
#ifdef HAVE_IMAGEVIEWER
      case STRIPES_SYSTEM_TAB_IMAGES:
         return &stripes->images_tab_node;
#endif
      case STRIPES_SYSTEM_TAB_MUSIC:
         return &stripes->music_tab_node;
#ifdef HAVE_FFMPEG
      case STRIPES_SYSTEM_TAB_VIDEO:
         return &stripes->video_tab_node;
#endif
      case STRIPES_SYSTEM_TAB_HISTORY:
         return &stripes->history_tab_node;
      case STRIPES_SYSTEM_TAB_FAVORITES:
         return &stripes->favorites_tab_node;
#ifdef HAVE_NETWORKING
      case STRIPES_SYSTEM_TAB_NETPLAY:
         return &stripes->netplay_tab_node;
#endif
      case STRIPES_SYSTEM_TAB_ADD:
         return &stripes->add_tab_node;
      default:
         if (i > stripes->system_tab_end)
            return stripes_get_userdata_from_horizontal_list(
                  stripes, i - (stripes->system_tab_end + 1));
   }

   return &stripes->main_menu_node;
}

static void stripes_list_switch_horizontal_list(stripes_handle_t *stripes)
{
   unsigned j;
   size_t list_size = stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t entry;
      float ia                    = stripes->categories_passive_alpha;
      float iz                    = stripes->categories_passive_zoom;
      float iw                    = stripes->categories_passive_width;
      float ix                    = stripes->categories_before_x;
      float iy                    = stripes->categories_before_y;
      stripes_node_t *node            = stripes_get_node(stripes, j);

      if (!node)
         continue;

      if (j == stripes->categories_active_idx)
      {
         ia = stripes->categories_active_alpha;
         iz = stripes->categories_active_zoom;
         iw = stripes->categories_active_width;
         ix = stripes->categories_active_x;
         iy = stripes->categories_active_y;
      }
      else if (j < stripes->categories_active_idx)
      {
         ix = stripes->categories_before_x;
         iy = stripes->categories_before_y;
      }
      else if (j > stripes->categories_active_idx)
      {
         ix = stripes->categories_after_x;
         iy = stripes->categories_after_y;
      }

      entry.duration     = STRIPES_DELAY;
      entry.target_value = ia;
      entry.subject      = &node->alpha;
      entry.easing_enum  = EASING_OUT_QUAD;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      entry.tag          = -1;
      entry.cb           = NULL;

      menu_animation_push(&entry);

      entry.target_value = iz;
      entry.subject      = &node->zoom;

      menu_animation_push(&entry);

      entry.target_value = iy;
      entry.subject      = &node->y;

      menu_animation_push(&entry);

      entry.target_value = ix;
      entry.subject      = &node->x;

      menu_animation_push(&entry);

      entry.target_value = iw;
      entry.subject      = &node->width;

      menu_animation_push(&entry);
   }
}

static void stripes_list_switch(stripes_handle_t *stripes)
{
   menu_animation_ctx_entry_t anim_entry;
   int dir                    = -1;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   settings_t *settings = config_get_ptr();

   if (stripes->categories_selection_ptr > stripes->categories_selection_ptr_old)
      dir = 1;

   stripes->categories_active_idx += dir;

   stripes_list_switch_horizontal_list(stripes);

   anim_entry.duration     = STRIPES_DELAY;
   anim_entry.target_value = stripes->categories_passive_width
      * -(float)stripes->categories_selection_ptr;
   anim_entry.subject      = &stripes->categories_x_pos;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   anim_entry.tag          = -1;
   anim_entry.cb           = NULL;

   if (anim_entry.subject)
      menu_animation_push(&anim_entry);

   dir = -1;
   if (stripes->categories_selection_ptr > stripes->categories_selection_ptr_old)
      dir = 1;

   stripes_list_switch_old(stripes, stripes->selection_buf_old,
         dir, stripes->selection_ptr_old);

   /* Check if we are to have horizontal animations. */
   if (settings->bools.menu_horizontal_animation)
      stripes_list_switch_new(stripes, selection_buf, dir, selection);
   stripes->categories_active_idx_old = (unsigned)stripes->categories_selection_ptr;

   if (!string_is_equal(stripes_thumbnails_ident('R'),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
   {
      menu_entry_t entry;

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, selection, NULL, true);

      if (!string_is_empty(entry.path))
         stripes_set_thumbnail_content(stripes, entry.path, 0 /* will be ignored */);

      stripes_update_thumbnail_path(stripes, 0, 'R');
      stripes_update_thumbnail_image(stripes);
   }
   if (!string_is_equal(stripes_thumbnails_ident('L'),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
   {
      menu_entry_t entry;

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, selection, NULL, true);

      if (!string_is_empty(entry.path))
         stripes_set_thumbnail_content(stripes, entry.path, 0 /* will be ignored */);

      stripes_update_thumbnail_path(stripes, 0, 'L');
      stripes_update_thumbnail_image(stripes);
   }
}

static void stripes_list_open_horizontal_list(stripes_handle_t *stripes)
{
   unsigned j;
   size_t list_size = stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t anim_entry;
      float ia          = 0;
      stripes_node_t *node  = stripes_get_node(stripes, j);

      if (!node)
         continue;

      if (j == stripes->categories_active_idx)
         ia = stripes->categories_active_alpha;
      else if (stripes->depth <= 1)
         ia = stripes->categories_passive_alpha;

      anim_entry.duration     = STRIPES_DELAY;
      anim_entry.target_value = ia;
      anim_entry.subject      = &node->alpha;
      anim_entry.easing_enum  = EASING_OUT_QUAD;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      anim_entry.tag          = -1;
      anim_entry.cb           = NULL;

      if (anim_entry.subject)
         menu_animation_push(&anim_entry);
   }
}

static void stripes_context_destroy_horizontal_list(stripes_handle_t *stripes)
{
   unsigned i;
   size_t list_size = stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      stripes_node_t *node = stripes_get_userdata_from_horizontal_list(stripes, i);

      if (!node)
         continue;

      file_list_get_at_offset(stripes->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path || !strstr(path, ".lpl"))
         continue;

      video_driver_texture_unload(&node->icon);
      video_driver_texture_unload(&node->content_icon);
   }
}

static void stripes_init_horizontal_list(stripes_handle_t *stripes)
{
   menu_displaylist_info_t info;
   settings_t *settings         = config_get_ptr();

   menu_displaylist_info_init(&info);

   info.list                    = stripes->horizontal_list;
   info.path                    = strdup(
         settings->paths.directory_playlist);
#if 0
   /* TODO/FIXME - will need to look what to do here */
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));
   info.enum_idx                = MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST;
#endif
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;

   if (!string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
      {
         size_t i;
         for (i = 0; i < stripes->horizontal_list->size; i++)
            stripes_node_allocate_userdata(stripes, (unsigned)i);
         menu_displaylist_process(&info);
      }
   }

   menu_displaylist_info_free(&info);
}

static void stripes_toggle_horizontal_list(stripes_handle_t *stripes)
{
   unsigned i;
   size_t list_size = stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end;

   for (i = 0; i <= list_size; i++)
   {
      stripes_node_t *node = stripes_get_node(stripes, i);

      if (!node)
         continue;

      node->alpha = 0;
      node->zoom  = stripes->categories_passive_zoom;

      if (i == stripes->categories_active_idx)
      {
         node->alpha = stripes->categories_active_alpha;
         node->zoom  = stripes->categories_active_zoom;
      }
      else if (stripes->depth <= 1)
         node->alpha = stripes->categories_passive_alpha;
   }
}

static void stripes_context_reset_horizontal_list(
      stripes_handle_t *stripes)
{
   unsigned i;
   int depth; /* keep this integer */
   size_t list_size                =
      stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL);

   stripes->categories_x_pos           =
      stripes->categories_passive_width *
      -(float)stripes->categories_selection_ptr;

   depth                           = (stripes->depth > 1) ? 2 : 1;
   stripes->x                          = stripes->icon_size * -(depth*2-2);

   for (i = 0; i < list_size; i++)
   {
      const char *path                          = NULL;
      stripes_node_t *node                          =
         stripes_get_userdata_from_horizontal_list(stripes, i);

      if (!node)
      {
         node = stripes_node_allocate_userdata(stripes, i);
         if (!node)
            continue;
      }

      file_list_get_at_offset(stripes->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path)
         continue;

      if (!strstr(path, ".lpl"))
         continue;

      {
         struct texture_image ti;
         char sysname[256];
         char *iconpath            = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *texturepath         = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *content_texturepath = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));

         iconpath[0]    = sysname[0] =
         texturepath[0] = content_texturepath[0] = '\0';

         fill_pathname_base_noext(sysname, path, sizeof(sysname));

         fill_pathname_application_special(iconpath,
               PATH_MAX_LENGTH * sizeof(char),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

         fill_pathname_join_concat(texturepath, iconpath, sysname,
               ".png",
               PATH_MAX_LENGTH * sizeof(char));

         ti.width         = 0;
         ti.height        = 0;
         ti.pixels        = NULL;
         ti.supports_rgba = video_driver_supports_rgba();

         if (image_texture_load(&ti, texturepath))
         {
            if (ti.pixels)
            {
               video_driver_texture_unload(&node->icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->icon);
            }

            image_texture_free(&ti);
         }

         strlcat(iconpath, sysname, PATH_MAX_LENGTH * sizeof(char));
         fill_pathname_join_delim(content_texturepath, iconpath,
               "content.png", '-',
               PATH_MAX_LENGTH * sizeof(char));

         if (image_texture_load(&ti, content_texturepath))
         {
            if (ti.pixels)
            {
               video_driver_texture_unload(&node->content_icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->content_icon);
            }

            image_texture_free(&ti);
         }

         free(iconpath);
         free(texturepath);
         free(content_texturepath);
      }
   }

   stripes_toggle_horizontal_list(stripes);
}

static void stripes_refresh_horizontal_list(stripes_handle_t *stripes)
{
   stripes_context_destroy_horizontal_list(stripes);
   if (stripes->horizontal_list)
   {
      stripes_free_list_nodes(stripes->horizontal_list, false);
      file_list_free(stripes->horizontal_list);
   }
   stripes->horizontal_list = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   stripes->horizontal_list         = (file_list_t*)
      calloc(1, sizeof(file_list_t));

   if (stripes->horizontal_list)
      stripes_init_horizontal_list(stripes);

   stripes_context_reset_horizontal_list(stripes);
}

static int stripes_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   stripes_handle_t *stripes        = (stripes_handle_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!stripes)
            return -1;
         stripes->mouse_show = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!stripes)
            return -1;
         stripes->mouse_show = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!stripes)
            return -1;

         stripes_refresh_horizontal_list(stripes);
         break;
      default:
         return -1;
   }

   return 0;
}

static void stripes_list_open(stripes_handle_t *stripes)
{
   menu_animation_ctx_entry_t entry;

   int                    dir = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   stripes->depth = (int)stripes_list_get_size(stripes, MENU_LIST_PLAIN);

   if (stripes->depth > stripes->old_depth)
      dir = 1;
   else if (stripes->depth < stripes->old_depth)
      dir = -1;

   stripes_list_open_horizontal_list(stripes);

   stripes_list_open_old(stripes, stripes->selection_buf_old,
         dir, stripes->selection_ptr_old);
   stripes_list_open_new(stripes, selection_buf,
         dir, selection);

   entry.duration     = STRIPES_DELAY;
   entry.target_value = stripes->icon_size * -(stripes->depth*2-2);
   entry.subject      = &stripes->x;
   entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   switch (stripes->depth)
   {
      case 1:
         menu_animation_push(&entry);

         entry.target_value = 0;
         entry.subject      = &stripes->textures_arrow_alpha;

         menu_animation_push(&entry);
         break;
      case 2:
         menu_animation_push(&entry);

         entry.target_value = 1;
         entry.subject      = &stripes->textures_arrow_alpha;

         menu_animation_push(&entry);
         break;
   }

   stripes->old_depth = stripes->depth;
}

static void stripes_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;

   if (!stripes)
      return;

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      stripes_selection_pointer_changed(stripes, false);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      if (!string_is_equal(stripes_thumbnails_ident('R'),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
         stripes_update_thumbnail_image(stripes);
      stripes_update_savestate_thumbnail_image(stripes);
      if (!string_is_equal(stripes_thumbnails_ident('L'),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
         stripes_update_thumbnail_image(stripes);
      return;
   }

   stripes_set_title(stripes);

   if (stripes->categories_selection_ptr != stripes->categories_active_idx_old)
      stripes_list_switch(stripes);
   else
      stripes_list_open(stripes);
}

static uintptr_t stripes_icon_get_id(stripes_handle_t *stripes,
      stripes_node_t *core_node, stripes_node_t *node,
      enum msg_hash_enums enum_idx, unsigned type, bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return stripes->textures.list[STRIPES_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return stripes->textures.list[STRIPES_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
         return stripes->textures.list[STRIPES_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return stripes->textures.list[STRIPES_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return stripes->textures.list[STRIPES_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_SAVE_STATE:
         return stripes->textures.list[STRIPES_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
         return stripes->textures.list[STRIPES_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return stripes->textures.list[STRIPES_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return stripes->textures.list[STRIPES_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return stripes->textures.list[STRIPES_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return stripes->textures.list[STRIPES_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return stripes->textures.list[STRIPES_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return stripes->textures.list[STRIPES_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         return stripes->textures.list[STRIPES_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return stripes->textures.list[STRIPES_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return stripes->textures.list[STRIPES_TEXTURE_RDB];
      default:
         break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return stripes->textures.list[STRIPES_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
         return stripes->textures.list[STRIPES_TEXTURE_FILE];
      case FILE_TYPE_RPL_ENTRY:
         if (core_node)
            return core_node->content_icon;

         switch (stripes_get_system_tab(stripes,
                  (unsigned)stripes->categories_selection_ptr))
         {
            case STRIPES_SYSTEM_TAB_FAVORITES:
               return stripes->textures.list[STRIPES_TEXTURE_FAVORITE];
            case STRIPES_SYSTEM_TAB_MUSIC:
               return stripes->textures.list[STRIPES_TEXTURE_MUSIC];
#ifdef HAVE_IMAGEVIEWER
            case STRIPES_SYSTEM_TAB_IMAGES:
               return stripes->textures.list[STRIPES_TEXTURE_IMAGE];
#endif
#ifdef HAVE_FFMPEG
            case STRIPES_SYSTEM_TAB_VIDEO:
               return stripes->textures.list[STRIPES_TEXTURE_MOVIE];
#endif
            default:
               break;
         }
         return stripes->textures.list[STRIPES_TEXTURE_FILE];
      case FILE_TYPE_CARCHIVE:
         return stripes->textures.list[STRIPES_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return stripes->textures.list[STRIPES_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return stripes->textures.list[STRIPES_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return stripes->textures.list[STRIPES_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return stripes->textures.list[STRIPES_TEXTURE_CORE];
      case FILE_TYPE_RDB:
         return stripes->textures.list[STRIPES_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return stripes->textures.list[STRIPES_TEXTURE_CURSOR];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return stripes->textures.list[STRIPES_TEXTURE_RUN];
      case MENU_SETTING_ACTION_CLOSE:
         return stripes->textures.list[STRIPES_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return stripes->textures.list[STRIPES_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return stripes->textures.list[STRIPES_TEXTURE_LOADSTATE];
      case FILE_TYPE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return stripes->textures.list[STRIPES_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
      case MENU_ENUM_LABEL_SET_CORE_ASSOCIATION:
         return stripes->textures.list[STRIPES_TEXTURE_CORE_OPTIONS];
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return stripes->textures.list[STRIPES_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return stripes->textures.list[STRIPES_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return stripes->textures.list[STRIPES_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_RESET:
         return stripes->textures.list[STRIPES_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION:
         if (stripes->depth == 3)
            return stripes->textures.list[STRIPES_TEXTURE_SUBSETTING];
         return stripes->textures.list[STRIPES_TEXTURE_SETTING];
      case MENU_SETTING_GROUP:
         return stripes->textures.list[STRIPES_TEXTURE_SETTING];
      case MENU_INFO_MESSAGE:
         return stripes->textures.list[STRIPES_TEXTURE_CORE_INFO];
      case MENU_WIFI:
         return stripes->textures.list[STRIPES_TEXTURE_WIFI];
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return stripes->textures.list[STRIPES_TEXTURE_ROOM];
#if 0
      /* stub these out until we have the icons */
      case MENU_ROOM_LAN:
         return stripes->textures.list[STRIPES_TEXTURE_ROOM_LAN];
      case MENU_ROOM_MITM:
         return stripes->textures.list[STRIPES_TEXTURE_ROOM_MITM];
#endif
#endif
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      int new_id = type - MENU_SETTINGS_CHEEVOS_START;
      if (get_badge_texture(new_id) != 0)
         return get_badge_texture(new_id);
      /* Should be replaced with placeholder badge icon. */
      return stripes->textures.list[STRIPES_TEXTURE_SUBSETTING];
   }
#endif

   return stripes->textures.list[STRIPES_TEXTURE_SUBSETTING];
}

static void stripes_calculate_visible_range(const stripes_handle_t *stripes,
      unsigned height, size_t list_size, unsigned current,
      unsigned *first, unsigned *last)
{
   unsigned j;
   float    base_y = stripes->margins_screen_top;

   *first = 0;
   *last  = list_size ? list_size - 1 : 0;

   if (current)
   {
      for (j = current; j-- > 0; )
      {
         float bottom = stripes_item_y(stripes, j, current)
            + base_y + stripes->icon_size;

         if (bottom < 0)
            break;

         *first = j;
      }
   }

   for (j = current+1; j < list_size; j++)
   {
      float top = stripes_item_y(stripes, j, current) + base_y;

      if (top > height)
         break;

      *last = j;
   }
}

static int stripes_draw_item(
      video_frame_info_t *video_info,
      menu_entry_t *entry,
      math_matrix_4x4 *mymat,
      stripes_handle_t *stripes,
      stripes_node_t *core_node,
      file_list_t *list,
      float *color,
      const char *thumb_ident,
      const char *left_thumb_ident,
      size_t i,
      size_t current,
      unsigned width,
      unsigned height
      )
{
   float icon_x, icon_y, label_offset;
   menu_animation_ctx_ticker_t ticker;
   char tmp[255];
   const char *ticker_str            = NULL;
   unsigned entry_type               = 0;
   const float half_size             = stripes->icon_size / 2.0f;
   uintptr_t texture_switch          = 0;
   bool do_draw_text                 = false;
   unsigned ticker_limit             = 35 * stripes_scale_mod[0];
   stripes_node_t *   node               = (stripes_node_t*)
      file_list_get_userdata_at_offset(list, i);
   settings_t *settings              = config_get_ptr();

   /* Initial ticker configuration */
   ticker.type_enum = settings->uints.menu_ticker_type;
   ticker.spacer = NULL;

   if (!node)
      goto iterate;

   tmp[0] = '\0';

   icon_y = stripes->margins_screen_top + node->y + half_size;

   if (icon_y < half_size)
      goto iterate;

   if (icon_y > height + stripes->icon_size)
      goto end;

   icon_x = node->x + stripes->margins_screen_left +
      stripes->icon_spacing_horizontal - half_size;

   if (icon_x < -half_size || icon_x > width)
      goto iterate;

   entry_type = menu_entry_get_type_new(entry);

   if (entry_type == FILE_TYPE_CONTENTLIST_ENTRY)
   {
      char entry_path[PATH_MAX_LENGTH] = {0};
      strlcpy(entry_path, entry->path, sizeof(entry_path));

      fill_short_pathname_representation(entry_path, entry_path,
            sizeof(entry_path));

      if (!string_is_empty(entry_path))
      {
         if (!string_is_empty(entry->path))
            free(entry->path);
         entry->path = strdup(entry_path);
      }
   }

   if (string_is_equal(entry->value,
            msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(entry->value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      if (stripes->textures.list[STRIPES_TEXTURE_SWITCH_OFF])
         texture_switch = stripes->textures.list[STRIPES_TEXTURE_SWITCH_OFF];
      else
         do_draw_text = true;
   }
   else if (string_is_equal(entry->value,
            msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(entry->value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      if (stripes->textures.list[STRIPES_TEXTURE_SWITCH_ON])
         texture_switch = stripes->textures.list[STRIPES_TEXTURE_SWITCH_ON];
      else
         do_draw_text = true;
   }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (
               string_is_equal(entry->value, "...")     ||
               string_is_equal(entry->value, "(COMP)")  ||
               string_is_equal(entry->value, "(CORE)")  ||
               string_is_equal(entry->value, "(MOVIE)") ||
               string_is_equal(entry->value, "(MUSIC)") ||
               string_is_equal(entry->value, "(DIR)")   ||
               string_is_equal(entry->value, "(RDB)")   ||
               string_is_equal(entry->value, "(CURSOR)")||
               string_is_equal(entry->value, "(CFILE)") ||
               string_is_equal(entry->value, "(FILE)")  ||
               string_is_equal(entry->value, "(IMAGE)")
            )
         {
         }
         else
               do_draw_text = true;
      }
      else
         do_draw_text = true;

   }

   if (string_is_empty(entry->value))
   {
      if (stripes->savestate_thumbnail ||
            (!string_is_equal
             (thumb_ident,
              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))
             && stripes->thumbnail) ||
             (!string_is_equal
             (left_thumb_ident,
              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))
             && stripes->left_thumbnail
             && settings->bools.menu_xmb_vertical_thumbnails)
         )
         ticker_limit = 40 * stripes_scale_mod[1];
      else
         ticker_limit = 70 * stripes_scale_mod[2];
   }

   if (!string_is_empty(entry->path))
      menu_entry_get_rich_label(entry, &ticker_str);

   ticker.s        = tmp;
   ticker.len      = ticker_limit;
   ticker.idx      = menu_animation_get_ticker_idx();
   ticker.str      = ticker_str;
   ticker.selected = (i == current);

   if (ticker.str)
      menu_animation_ticker(&ticker);

   label_offset = stripes->margins_label_top;
   if (i == current && width > 320 && height > 240
         && !string_is_empty(entry->sublabel))
   {
      char entry_sublabel[MENU_SUBLABEL_MAX_LENGTH] = {0};

      label_offset      = - stripes->margins_label_top;

      word_wrap(entry_sublabel, entry->sublabel, 50 * stripes_scale_mod[3], true, 0);

      stripes_draw_text(video_info, stripes, entry_sublabel,
            node->x + stripes->margins_screen_left +
            stripes->icon_spacing_horizontal + stripes->margins_label_left,
            stripes->margins_screen_top + node->y + stripes->margins_label_top*3.5,
            1, node->label_alpha, TEXT_ALIGN_LEFT,
            width, height, stripes->font2);
   }

   stripes_draw_text(video_info, stripes, tmp,
         node->x + stripes->margins_screen_left +
         stripes->icon_spacing_horizontal + stripes->margins_label_left,
         stripes->margins_screen_top + node->y + label_offset,
         1, node->label_alpha, TEXT_ALIGN_LEFT,
         width, height, stripes->font);

   tmp[0]          = '\0';

   ticker.s        = tmp;
   ticker.len      = 35 * stripes_scale_mod[7];
   ticker.idx      = menu_animation_get_ticker_idx();
   ticker.selected = (i == current);

   if (!string_is_empty(entry->value))
   {
      ticker.str   = entry->value;
      menu_animation_ticker(&ticker);
   }

   if (do_draw_text)
      stripes_draw_text(video_info, stripes, tmp,
            node->x +
            + stripes->margins_screen_left
            + stripes->icon_spacing_horizontal
            + stripes->margins_label_left
            + stripes->margins_setting_left,
            stripes->margins_screen_top + node->y + stripes->margins_label_top,
            1,
            node->label_alpha,
            TEXT_ALIGN_LEFT,
            width, height, stripes->font);

   menu_display_set_alpha(color, MIN(node->alpha, stripes->alpha));

   if (color[3] != 0)
   {
      math_matrix_4x4 mymat_tmp;
      menu_display_ctx_rotate_draw_t rotate_draw;
      uintptr_t texture        = stripes_icon_get_id(stripes, core_node, node,
            entry->enum_idx, entry_type, (i == current));
      float x                  = icon_x;
      float y                  = icon_y;
      float rotation           = 0;
      float scale_factor       = node->zoom;

      rotate_draw.matrix       = &mymat_tmp;
      rotate_draw.rotation     = rotation;
      rotate_draw.scale_x      = scale_factor;
      rotate_draw.scale_y      = scale_factor;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      menu_display_rotate_z(&rotate_draw, video_info);

      stripes_draw_icon(video_info,
            stripes->icon_size,
            &mymat_tmp,
            texture,
            x,
            y,
            width,
            height,
            1.0,
            rotation,
            scale_factor,
            &color[0],
            stripes->shadow_offset);
   }

   menu_display_set_alpha(color, MIN(node->alpha, stripes->alpha));

   if (texture_switch != 0 && color[3] != 0)
      stripes_draw_icon(video_info,
            stripes->icon_size,
            mymat,
            texture_switch,
            node->x + stripes->margins_screen_left
            + stripes->icon_spacing_horizontal
            + stripes->icon_size / 2.0 + stripes->margins_setting_left,
            stripes->margins_screen_top + node->y + stripes->icon_size / 2.0,
            width, height,
            node->alpha,
            0,
            1,
            &color[0],
            stripes->shadow_offset);

iterate:
   return 0;

end:
   return -1;
}

static void stripes_draw_items(
      video_frame_info_t *video_info,
      stripes_handle_t *stripes,
      file_list_t *list,
      size_t current, size_t cat_selection_ptr, float *color,
      unsigned width, unsigned height)
{
   size_t i;
   unsigned first, last;
   math_matrix_4x4 mymat;
   menu_display_ctx_rotate_draw_t rotate_draw;
   stripes_node_t *core_node       = NULL;
   size_t end                  = 0;
   const char *thumb_ident     = stripes_thumbnails_ident('R');
   const char *left_thumb_ident= stripes_thumbnails_ident('L');

   if (!list || !list->size || !stripes)
      return;

   if (cat_selection_ptr > stripes->system_tab_end)
      core_node = stripes_get_userdata_from_horizontal_list(
            stripes, (unsigned)(cat_selection_ptr - (stripes->system_tab_end + 1)));

   end                      = file_list_get_size(list);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (list == stripes->selection_buf_old)
   {
      stripes_node_t *node = (stripes_node_t*)
            file_list_get_userdata_at_offset(list, current);

      if (node && (uint8_t)(255 * node->alpha) == 0)
         return;

      i = 0;
   }

   first = i;
   last  = end - 1;

   stripes_calculate_visible_range(stripes, height, end, current, &first, &last);

   menu_display_blend_begin(video_info);

   for (i = first; i <= last; i++)
   {
      int ret;
      menu_entry_t entry;
      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, i, list, true);
      ret = stripes_draw_item(video_info,
            &entry,
            &mymat,
            stripes, core_node,
            list, color, thumb_ident, left_thumb_ident,
            i, current,
            width, height);
      if (ret == -1)
         break;
   }

   menu_display_blend_end(video_info);
}

static void stripes_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   size_t i;
   menu_input_pointer_t pointer;
   settings_t   *settings   = config_get_ptr();
   stripes_handle_t *stripes        = (stripes_handle_t*)data;
   unsigned      end        = (unsigned)menu_entries_get_size();

   if (!stripes)
      return;

   menu_input_get_pointer_state(&pointer);

   if (pointer.type != MENU_POINTER_DISABLED)
   {
      size_t selection  = menu_navigation_get_selection();
      int16_t pointer_y = pointer.y;
      unsigned first = 0, last = end;

      pointer_y = (pointer.type == MENU_POINTER_MOUSE) ?
            pointer_y + (stripes->cursor_size/2) : pointer_y;

      if (height)
         stripes_calculate_visible_range(stripes, height,
               end, selection, &first, &last);

      for (i = first; i <= last; i++)
      {
         float item_y1     = stripes->margins_screen_top
            + stripes_item_y(stripes, (int)i, selection);
         float item_y2     = item_y1 + stripes->icon_size;

         if (pointer_y > item_y1 && pointer_y < item_y2)
            menu_input_set_pointer_selection(i);
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

static bool stripes_shader_pipeline_active(video_frame_info_t *video_info)
{
   if (string_is_not_equal(menu_driver_ident(), "stripes"))
      return false;
   if (video_info->menu_shader_pipeline == XMB_SHADER_PIPELINE_WALLPAPER)
      return false;
   return true;
}

static void stripes_draw_bg(
      stripes_handle_t *stripes,
      video_frame_info_t *video_info,
      unsigned width,
      unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   float rgb[3];
   HSLToRGB(0.0,0.5,0.5, &rgb[0]) ;
   float color[16] = {
      rgb[0], rgb[1], rgb[2], 1,
      rgb[0], rgb[1], rgb[2], 1,
      rgb[0], rgb[1], rgb[2], 1,
      rgb[0], rgb[1], rgb[2], 1,
   };

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = &color[0];

   draw.x           = 0;
   draw.y           = 0;
   draw.width       = width;
   draw.height      = height;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = menu_display_white_texture;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id = 0;

   menu_display_blend_begin(video_info);
   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void stripes_draw_dark_layer(
      stripes_handle_t *stripes,
      video_frame_info_t *video_info,
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

   menu_display_set_alpha(black, MIN(stripes->alpha, 0.75));

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

   menu_display_blend_begin(video_info);
   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void stripes_frame(void *data, video_frame_info_t *video_info)
{
   math_matrix_4x4 mymat;
   unsigned i;
   menu_display_ctx_rotate_draw_t rotate_draw;
   char msg[1024];
   char title_msg[255];
   char title_truncated[255];
   size_t selection                        = 0;
   size_t percent_width                    = 0;
   const int min_thumb_size                = 50;
   bool render_background                  = false;
   file_list_t *selection_buf              = NULL;
   unsigned width                          = video_info->width;
   unsigned height                         = video_info->height;
   const float under_thumb_margin          = 0.96;
   float scale_factor                      = 0.0f;
   float pseudo_font_length                = 0.0f;
   float stack_width                       = 285;
   stripes_handle_t *stripes                       = (stripes_handle_t*)data;
   settings_t *settings                    = config_get_ptr();

   if (!stripes)
      return;

   scale_factor                            = (settings->floats.menu_scale_factor * (float)width) / 1920.0f;
   pseudo_font_length                      = stripes->icon_spacing_horizontal * 4 - stripes->icon_size / 4;

   msg[0]             = '\0';
   title_msg[0]       = '\0';
   title_truncated[0] = '\0';

   font_driver_bind_block(stripes->font,  &stripes->raster_block);
   font_driver_bind_block(stripes->font2, &stripes->raster_block2);

   stripes->raster_block.carr.coords.vertices  = 0;
   stripes->raster_block2.carr.coords.vertices = 0;

   menu_display_set_alpha(stripes_coord_black, MIN(
         (float)video_info->xmb_alpha_factor/100, stripes->alpha));
   menu_display_set_alpha(stripes_coord_white, stripes->alpha);

   stripes_draw_bg(
         stripes,
         video_info,
         width,
         height);

   selection = menu_navigation_get_selection();

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);
   menu_display_blend_begin(video_info);

   /* Horizontal stripes */
   for (i = 0; i <= stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end; i++)
   {
      stripes_node_t *node = stripes_get_node(stripes, i);

      if (!node)
         continue;

      float rgb[3];
      HSLToRGB(0.07*(float)i,0.5,0.5, &rgb[0]) ;
      float color[16] = {
         rgb[0], rgb[1], rgb[2], 0.55,
         rgb[0], rgb[1], rgb[2], 0.55,
         rgb[0], rgb[1], rgb[2], 0.55,
         rgb[0], rgb[1], rgb[2], 0.55,
      };

      menu_display_draw_polygon(
            video_info,
            stripes->categories_x_pos + stack_width,
            0,
            stripes->categories_x_pos + stack_width + node->width,
            0,
            stripes->categories_x_pos + stack_width + stripes->categories_angle,
            video_info->height,
            stripes->categories_x_pos + stack_width + stripes->categories_angle + node->width,
            video_info->height,
            video_info->width, video_info->height,
            &color[0]);

      menu_display_blend_begin(video_info);

      stack_width += node->width;
   }

   stack_width = 285;

   /* Horizontal tab icons */
   for (i = 0; i <= stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end; i++)
   {
      stripes_node_t *node = stripes_get_node(stripes, i);

      if (!node)
         continue;

      menu_display_set_alpha(stripes_item_color, MIN(node->alpha, stripes->alpha));

      if (stripes_item_color[3] != 0)
      {
         menu_display_ctx_rotate_draw_t rotate_draw;
         math_matrix_4x4 mymat;
         uintptr_t texture        = node->icon;
         float x                  = stripes->categories_x_pos + stack_width + node->x + node->width / 2.0
                                    - stripes->icon_size / 2.0;
         float y                  = node->y + stripes->icon_size / 2.0;
         float rotation           = 0;
         float scale_factor       = node->zoom;

         rotate_draw.matrix       = &mymat;
         rotate_draw.rotation     = rotation;
         rotate_draw.scale_x      = scale_factor;
         rotate_draw.scale_y      = scale_factor;
         rotate_draw.scale_z      = 1;
         rotate_draw.scale_enable = true;

         menu_display_rotate_z(&rotate_draw, video_info);

         stripes_draw_icon(video_info,
               stripes->icon_size,
               &mymat,
               texture,
               x,
               y,
               width,
               height,
               1.0,
               rotation,
               scale_factor,
               &stripes_item_color[0],
               stripes->shadow_offset);
      }

      stack_width += node->width;
   }

   menu_display_blend_end(video_info);

   /* Vertical icons */
//    if (stripes)
//       stripes_draw_items(
//             video_info,
//             stripes,
//             stripes->selection_buf_old,
//             stripes->selection_ptr_old,
//             (stripes_list_get_size(stripes, MENU_LIST_PLAIN) > 1)
//             ? stripes->categories_selection_ptr :
//             stripes->categories_selection_ptr_old,
//             &stripes_item_color[0],
//             width,
//             height);

//    selection_buf = menu_entries_get_selection_buf_ptr(0);

//    if (stripes)
//       stripes_draw_items(
//             video_info,
//             stripes,
//             selection_buf,
//             selection,
//             stripes->categories_selection_ptr,
//             &stripes_item_color[0],
//             width,
//             height);

   font_driver_flush(video_info->width, video_info->height, stripes->font,
         video_info);
   font_driver_bind_block(stripes->font, NULL);

   font_driver_flush(video_info->width, video_info->height, stripes->font2,
         video_info);
   font_driver_bind_block(stripes->font2, NULL);

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      render_background = true;
   }

   if (!string_is_empty(stripes->box_message))
   {
      strlcpy(msg, stripes->box_message,
            sizeof(msg));
      free(stripes->box_message);
      stripes->box_message  = NULL;
      render_background = true;
   }

   if (render_background)
   {
      stripes_draw_dark_layer(stripes, video_info, width, height);
      stripes_render_messagebox_internal(
            video_info, stripes, msg);
   }

   /* Cursor image */
   if (stripes->mouse_show)
   {
      menu_input_pointer_t pointer;
      menu_input_get_pointer_state(&pointer);

      menu_display_set_alpha(stripes_coord_white, MIN(stripes->alpha, 1.00f));
      menu_display_draw_cursor(
            video_info,
            &stripes_coord_white[0],
            stripes->cursor_size,
            stripes->textures.list[STRIPES_TEXTURE_POINTER],
            pointer.x,
            pointer.y,
            width,
            height);
   }

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void stripes_layout_ps3(stripes_handle_t *stripes, int width, int height)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();

   float scale_factor            =
      (settings->floats.menu_scale_factor * width) / 1920.0f;

   stripes->above_subitem_offset     =   1.5;
   stripes->above_item_offset        =  -1.0;
   stripes->active_item_factor       =   3.0;
   stripes->under_item_offset        =   5.0;

   stripes->categories_active_zoom   = 1.0;
   stripes->categories_passive_zoom  = 0.25;

   stripes->categories_angle         = 400 * scale_factor;

   stripes->categories_active_y      = height / 2;
   stripes->categories_before_y      = 64 * scale_factor;
   stripes->categories_after_y       = height - 64 * scale_factor;

   stripes->categories_active_x      = stripes->categories_angle / 2;
   stripes->categories_before_x      = stripes->categories_angle - 22 * scale_factor;
   stripes->categories_after_x       = 22 * scale_factor;

   stripes->categories_passive_width = 128 * scale_factor;
   stripes->categories_active_width  = 1200 * scale_factor;

   stripes->items_active_zoom        = 1.0;
   stripes->items_passive_zoom       = 0.5;

   stripes->categories_active_alpha  = 1.0;
   stripes->categories_passive_alpha = 1.0;
   stripes->items_active_alpha       = 1.0;
   stripes->items_passive_alpha      = 0.85;

   stripes->shadow_offset            = 2.0;

   new_font_size                 = 32.0  * scale_factor;
   stripes->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;

   stripes->thumbnail_width          = 1024.0 * scale_factor;
   stripes->left_thumbnail_width     = 1024.0 * scale_factor;
   stripes->savestate_thumbnail_width= 460.0 * scale_factor;
   stripes->cursor_size              = 64.0 * scale_factor;

   stripes->icon_spacing_horizontal  = 200.0 * scale_factor;
   stripes->icon_spacing_vertical    = 64.0 * scale_factor;

   stripes->margins_screen_top       = (256+32) * scale_factor;
   stripes->margins_screen_left      = 336.0 * scale_factor;

   stripes->margins_title_left       = 60 * scale_factor;
   stripes->margins_title_top        = 60 * scale_factor + new_font_size / 3;
   stripes->margins_title_bottom     = 60 * scale_factor - new_font_size / 3;

   stripes->margins_label_left       = 85.0 * scale_factor;
   stripes->margins_label_top        = new_font_size / 3.0;

   stripes->margins_setting_left     = 600.0 * scale_factor * stripes_scale_mod[6];
   stripes->margins_dialog           = 48 * scale_factor;

   stripes->margins_slice            = 16;

   stripes->icon_size                = 256.0 * scale_factor;
   stripes->font_size                = new_font_size;

#ifdef STRIPES_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  stripes->margins_screen_left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  stripes->margins_screen_top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  stripes->margins_title_left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  stripes->margins_title_top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  stripes->margins_title_bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  stripes->margins_label_left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  stripes->margins_label_top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  stripes->margins_setting_left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  stripes->icon_spacing_horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  stripes->icon_spacing_vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  stripes->icon_size);
#endif

   menu_display_set_header_height(new_header_height);
}

static void stripes_layout_psp(stripes_handle_t *stripes, int width)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();
   float scale_factor            =
      ((settings->floats.menu_scale_factor * width) / 1920.0) * 1.5;
#ifdef _3DS
   scale_factor                  =
      settings->floats.menu_scale_factor / 4.0;
#endif

   stripes->above_subitem_offset     =  1.5;
   stripes->above_item_offset        = -1.0;
   stripes->active_item_factor       =  2.0;
   stripes->under_item_offset        =  3.0;

   stripes->categories_active_zoom   = 1.0;
   stripes->categories_passive_zoom  = 1.0;
   stripes->items_active_zoom        = 1.0;
   stripes->items_passive_zoom       = 1.0;

   stripes->categories_active_alpha  = 1.0;
   stripes->categories_passive_alpha = 0.85;
   stripes->items_active_alpha       = 1.0;
   stripes->items_passive_alpha      = 0.85;

   stripes->shadow_offset            = 1.0;

   new_font_size                 = 32.0  * scale_factor;
   stripes->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;
   stripes->margins_screen_top       = (256+32) * scale_factor;

   stripes->thumbnail_width          = 460.0 * scale_factor;
   stripes->left_thumbnail_width     = 400.0 * scale_factor;
   stripes->savestate_thumbnail_width= 460.0 * scale_factor;
   stripes->cursor_size              = 64.0;

   stripes->icon_spacing_horizontal  = 250.0 * scale_factor;
   stripes->icon_spacing_vertical    = 108.0 * scale_factor;

   stripes->margins_screen_left      = 136.0 * scale_factor;
   stripes->margins_title_left       = 60 * scale_factor;
   stripes->margins_title_top        = 60 * scale_factor + new_font_size / 3;
   stripes->margins_title_bottom     = 60 * scale_factor - new_font_size / 3;
   stripes->margins_label_left       = 85.0 * scale_factor;
   stripes->margins_label_top        = new_font_size / 3.0;
   stripes->margins_setting_left     = 600.0 * scale_factor;
   stripes->margins_dialog           = 48 * scale_factor;
   stripes->margins_slice            = 16;
   stripes->icon_size                = 128.0 * scale_factor;
   stripes->font_size                = new_font_size;

#ifdef STRIPES_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  stripes->margins_screen_left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  stripes->margins_screen_top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  stripes->margins_title_left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  stripes->margins_title_top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  stripes->margins_title_bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  stripes->margins_label_left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  stripes->margins_label_top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  stripes->margins_setting_left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  stripes->icon_spacing_horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  stripes->icon_spacing_vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  stripes->icon_size);
#endif

   menu_display_set_header_height(new_header_height);
}

static void stripes_layout(stripes_handle_t *stripes)
{
   unsigned width, height, i, current, end;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   video_driver_get_size(&width, &height);

   /* Mimic the layout of the PSP instead of the PS3 on tiny screens */
   if (width > 320 && height > 240)
      stripes_layout_ps3(stripes, width, height);
   else
      stripes_layout_psp(stripes, width);

   current = (unsigned)selection;
   end     = (unsigned)menu_entries_get_size();

   for (i = 0; i < end; i++)
   {
      float ia         = stripes->items_passive_alpha;
      float iz         = stripes->items_passive_zoom;
      stripes_node_t *node = (stripes_node_t*)file_list_get_userdata_at_offset(
            selection_buf, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia             = stripes->items_active_alpha;
         iz             = stripes->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = ia;
      node->zoom        = iz;
      node->y           = stripes_item_y(stripes, i, current);
   }

   if (stripes->depth <= 1)
      return;

   current = (unsigned)stripes->selection_ptr_old;
   end     = (unsigned)file_list_get_size(stripes->selection_buf_old);

   for (i = 0; i < end; i++)
   {
      float         ia = 0;
      float         iz = stripes->items_passive_zoom;
      stripes_node_t *node = (stripes_node_t*)file_list_get_userdata_at_offset(
            stripes->selection_buf_old, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia             = stripes->items_active_alpha;
         iz             = stripes->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = 0;
      node->zoom        = iz;
      node->y           = stripes_item_y(stripes, i, current);
      node->x           = stripes->icon_size * 1 * -2;
   }
}

static void *stripes_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   int i;
   stripes_handle_t *stripes          = NULL;
   settings_t *settings       = config_get_ptr();
   menu_handle_t *menu        = (menu_handle_t*)calloc(1, sizeof(*menu));
   float scale_value          = settings->floats.menu_scale_factor * 100.0f;

   /* scaling multiplier formulas made from these values:     */
   /* stripes_scale 50 = {2.5, 2.5,   2, 1.7, 2.5,   4, 2.4, 2.5} */
   /* stripes_scale 75 = {  2, 1.6, 1.6, 1.4, 1.5, 2.3, 1.9, 1.3} */
   if (scale_value < 100)
   {
   /* text length & word wrap (base 35 apply to file browser, 1st column) */
      stripes_scale_mod[0] = -0.03 * scale_value + 4.083;
   /* playlist text length when thumbnail is ON (small, base 40) */
      stripes_scale_mod[1] = -0.03 * scale_value + 3.95;
   /* playlist text length when thumbnail is OFF (large, base 70) */
      stripes_scale_mod[2] = -0.02 * scale_value + 3.033;
   /* sub-label length & word wrap */
      stripes_scale_mod[3] = -0.014 * scale_value + 2.416;
   /* thumbnail size & vertical margin from top */
      stripes_scale_mod[4] = -0.03 * scale_value + 3.916;
   /* thumbnail horizontal left margin (horizontal positioning) */
      stripes_scale_mod[5] = -0.06 * scale_value + 6.933;
   /* margin before 2nd column start (shaders parameters, cheats...) */
      stripes_scale_mod[6] = -0.028 * scale_value + 3.866;
   /* text length & word wrap (base 35 apply to 2nd column in cheats, shaders, etc) */
      stripes_scale_mod[7] = 134.179 * pow(scale_value, -1.0778);

      for (i = 0; i < 8; i++)
         if (stripes_scale_mod[i] < 1)
            stripes_scale_mod[i] = 1;
   }

   if (!menu)
      goto error;

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   video_driver_get_size(&width, &height);

   stripes = (stripes_handle_t*)calloc(1, sizeof(stripes_handle_t));

   if (!stripes)
      goto error;

   *userdata = stripes;

   stripes->selection_buf_old     = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!stripes->selection_buf_old)
      goto error;

   stripes->categories_active_idx         = 0;
   stripes->categories_active_idx_old     = 0;
   stripes->x                             = 0;
   stripes->categories_x_pos              = 0;
   stripes->textures_arrow_alpha          = 0;
   stripes->depth                         = 1;
   stripes->old_depth                     = 1;
   stripes->alpha                         = 0;

   stripes->system_tab_end                = 0;
   stripes->tabs[stripes->system_tab_end]     = STRIPES_SYSTEM_TAB_MAIN;
   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_MUSIC;
#ifdef HAVE_FFMPEG
   if (settings->bools.menu_content_show_video)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_NETPLAY;
#endif

   if (settings->bools.menu_content_show_add && !settings->bools.kiosk_mode_enable)
      stripes->tabs[++stripes->system_tab_end] = STRIPES_SYSTEM_TAB_ADD;

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   menu_display_set_width(width);
   menu_display_set_height(height);

   menu_display_allocate_white_texture();

   stripes->horizontal_list         = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (stripes->horizontal_list)
      stripes_init_horizontal_list(stripes);

   return menu;

error:
   if (menu)
      free(menu);

   if (stripes)
   {
      if (stripes->selection_buf_old)
         free(stripes->selection_buf_old);
      stripes->selection_buf_old = NULL;
      if (stripes->horizontal_list)
      {
         stripes_free_list_nodes(stripes->horizontal_list, false);
         file_list_free(stripes->horizontal_list);
      }
      stripes->horizontal_list = NULL;
   }
   return NULL;
}

static void stripes_free(void *data)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;

   if (stripes)
   {
      if (stripes->selection_buf_old)
      {
         stripes_free_list_nodes(stripes->selection_buf_old, false);
         file_list_free(stripes->selection_buf_old);
      }

      if (stripes->horizontal_list)
      {
         stripes_free_list_nodes(stripes->horizontal_list, false);
         file_list_free(stripes->horizontal_list);
      }

      stripes->selection_buf_old = NULL;
      stripes->horizontal_list   = NULL;

      video_coord_array_free(&stripes->raster_block.carr);
      video_coord_array_free(&stripes->raster_block2.carr);

      if (!string_is_empty(stripes->box_message))
         free(stripes->box_message);
      if (!string_is_empty(stripes->thumbnail_system))
         free(stripes->thumbnail_system);
      if (!string_is_empty(stripes->thumbnail_content))
         free(stripes->thumbnail_content);
      if (!string_is_empty(stripes->savestate_thumbnail_file_path))
         free(stripes->savestate_thumbnail_file_path);
      if (!string_is_empty(stripes->thumbnail_file_path))
         free(stripes->thumbnail_file_path);
      if (!string_is_empty(stripes->left_thumbnail_file_path))
         free(stripes->left_thumbnail_file_path);
      if (!string_is_empty(stripes->bg_file_path))
         free(stripes->bg_file_path);
   }

   font_driver_bind_block(NULL, NULL);
}

static void stripes_context_bg_destroy(stripes_handle_t *stripes)
{
   if (!stripes)
      return;
   video_driver_texture_unload(&stripes->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
}

static bool stripes_load_image(void *userdata, void *data, enum menu_image_type type)
{
   stripes_handle_t *stripes = (stripes_handle_t*)userdata;

   if (!stripes || !data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         stripes_context_bg_destroy(stripes);
         video_driver_texture_unload(&stripes->textures.bg);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR,
               &stripes->textures.bg);
         menu_display_allocate_white_texture();
         break;
      case MENU_IMAGE_THUMBNAIL:
         {
            struct texture_image *img  = (struct texture_image*)data;
            stripes->thumbnail_height      = stripes->thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&stripes->thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &stripes->thumbnail);
         }
         break;
      case MENU_IMAGE_LEFT_THUMBNAIL:
         {
            struct texture_image *img  = (struct texture_image*)data;
            stripes->left_thumbnail_height      = stripes->left_thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&stripes->left_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &stripes->left_thumbnail);
         }
         break;
      case MENU_IMAGE_SAVESTATE_THUMBNAIL:
         {
            struct texture_image *img       = (struct texture_image*)data;
            stripes->savestate_thumbnail_height = stripes->savestate_thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&stripes->savestate_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &stripes->savestate_thumbnail);
         }
         break;
   }

   return true;
}

static const char *stripes_texture_path(unsigned id)
{
   switch (id)
   {
      case STRIPES_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case STRIPES_TEXTURE_SETTINGS:
         return "settings.png";
      case STRIPES_TEXTURE_HISTORY:
         return "history.png";
      case STRIPES_TEXTURE_FAVORITES:
         return "favorites.png";
      case STRIPES_TEXTURE_ADD_FAVORITE:
         return "add-favorite.png";
      case STRIPES_TEXTURE_MUSICS:
         return "musics.png";
#ifdef HAVE_FFMPEG
      case STRIPES_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case STRIPES_TEXTURE_IMAGES:
         return "images.png";
#endif
      case STRIPES_TEXTURE_SETTING:
         return "setting.png";
      case STRIPES_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case STRIPES_TEXTURE_ARROW:
         return "arrow.png";
      case STRIPES_TEXTURE_RUN:
         return "run.png";
      case STRIPES_TEXTURE_CLOSE:
         return "close.png";
      case STRIPES_TEXTURE_RESUME:
         return "resume.png";
      case STRIPES_TEXTURE_CLOCK:
         return "clock.png";
      case STRIPES_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case STRIPES_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case STRIPES_TEXTURE_POINTER:
         return "pointer.png";
      case STRIPES_TEXTURE_SAVESTATE:
         return "savestate.png";
      case STRIPES_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case STRIPES_TEXTURE_UNDO:
         return "undo.png";
      case STRIPES_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case STRIPES_TEXTURE_WIFI:
         return "wifi.png";
      case STRIPES_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case STRIPES_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case STRIPES_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case STRIPES_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case STRIPES_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case STRIPES_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case STRIPES_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case STRIPES_TEXTURE_RELOAD:
         return "reload.png";
      case STRIPES_TEXTURE_RENAME:
         return "rename.png";
      case STRIPES_TEXTURE_FILE:
         return "file.png";
      case STRIPES_TEXTURE_FOLDER:
         return "folder.png";
      case STRIPES_TEXTURE_ZIP:
         return "zip.png";
      case STRIPES_TEXTURE_MUSIC:
         return "music.png";
      case STRIPES_TEXTURE_FAVORITE:
         return "favorites-content.png";
      case STRIPES_TEXTURE_IMAGE:
         return "image.png";
      case STRIPES_TEXTURE_MOVIE:
         return "movie.png";
      case STRIPES_TEXTURE_CORE:
         return "core.png";
      case STRIPES_TEXTURE_RDB:
         return "database.png";
      case STRIPES_TEXTURE_CURSOR:
         return "cursor.png";
      case STRIPES_TEXTURE_SWITCH_ON:
         return "on.png";
      case STRIPES_TEXTURE_SWITCH_OFF:
         return "off.png";
      case STRIPES_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case STRIPES_TEXTURE_NETPLAY:
         return "netplay.png";
      case STRIPES_TEXTURE_ROOM:
         return "room.png";
      /* stub these out until we have the icons
      case STRIPES_TEXTURE_ROOM_LAN:
         return "room_lan.png";
      case STRIPES_TEXTURE_ROOM_MITM:
         return "room_mitm.png";
      */
#endif
      case STRIPES_TEXTURE_KEY:
         return "key.png";
      case STRIPES_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case STRIPES_TEXTURE_DIALOG_SLICE:
         return "dialog-slice.png";

   }

   return NULL;
}

static void stripes_context_reset_textures(
      stripes_handle_t *stripes, const char *iconpath)
{
   unsigned i;

   for (i = 0; i < STRIPES_TEXTURE_LAST; i++)
      menu_display_reset_textures_list(stripes_texture_path(i), iconpath, &stripes->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);

   menu_display_allocate_white_texture();

   stripes->main_menu_node.icon     = stripes->textures.list[STRIPES_TEXTURE_MAIN_MENU];
   stripes->main_menu_node.alpha    = stripes->categories_active_alpha;
   stripes->main_menu_node.zoom     = stripes->categories_active_zoom;

   stripes->settings_tab_node.icon  = stripes->textures.list[STRIPES_TEXTURE_SETTINGS];
   stripes->settings_tab_node.alpha = stripes->categories_active_alpha;
   stripes->settings_tab_node.zoom  = stripes->categories_active_zoom;

   stripes->history_tab_node.icon   = stripes->textures.list[STRIPES_TEXTURE_HISTORY];
   stripes->history_tab_node.alpha  = stripes->categories_active_alpha;
   stripes->history_tab_node.zoom   = stripes->categories_active_zoom;

   stripes->favorites_tab_node.icon   = stripes->textures.list[STRIPES_TEXTURE_FAVORITES];
   stripes->favorites_tab_node.alpha  = stripes->categories_active_alpha;
   stripes->favorites_tab_node.zoom   = stripes->categories_active_zoom;

   stripes->music_tab_node.icon     = stripes->textures.list[STRIPES_TEXTURE_MUSICS];
   stripes->music_tab_node.alpha    = stripes->categories_active_alpha;
   stripes->music_tab_node.zoom     = stripes->categories_active_zoom;

#ifdef HAVE_FFMPEG
   stripes->video_tab_node.icon     = stripes->textures.list[STRIPES_TEXTURE_MOVIES];
   stripes->video_tab_node.alpha    = stripes->categories_active_alpha;
   stripes->video_tab_node.zoom     = stripes->categories_active_zoom;
#endif

#ifdef HAVE_IMAGEVIEWER
   stripes->images_tab_node.icon    = stripes->textures.list[STRIPES_TEXTURE_IMAGES];
   stripes->images_tab_node.alpha   = stripes->categories_active_alpha;
   stripes->images_tab_node.zoom    = stripes->categories_active_zoom;
#endif

   stripes->add_tab_node.icon       = stripes->textures.list[STRIPES_TEXTURE_ADD];
   stripes->add_tab_node.alpha      = stripes->categories_active_alpha;
   stripes->add_tab_node.zoom       = stripes->categories_active_zoom;

#ifdef HAVE_NETWORKING
   stripes->netplay_tab_node.icon   = stripes->textures.list[STRIPES_TEXTURE_NETPLAY];
   stripes->netplay_tab_node.alpha  = stripes->categories_active_alpha;
   stripes->netplay_tab_node.zoom   = stripes->categories_active_zoom;
#endif
}

static void stripes_context_reset_background(const char *iconpath)
{
   char *path                  = NULL;
   settings_t *settings        = config_get_ptr();
   const char *path_menu_wp    = settings->paths.path_menu_wallpaper;

   if (!string_is_empty(path_menu_wp))
      path = strdup(path_menu_wp);
   else if (!string_is_empty(iconpath))
   {
      path    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      path[0] = '\0';

      fill_pathname_join(path, iconpath, "bg.png",
            PATH_MAX_LENGTH * sizeof(char));
   }

   if (path_is_valid(path))
      task_push_image_load(path,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);

   if (path)
      free(path);
}

static void stripes_context_reset(void *data, bool is_threaded)
{
   stripes_handle_t *stripes = (stripes_handle_t*)data;

   if (stripes)
   {
      char bg_file_path[PATH_MAX_LENGTH] = {0};
      char *iconpath    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      iconpath[0]       = '\0';

      fill_pathname_application_special(bg_file_path,
            sizeof(bg_file_path), APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

      if (!string_is_empty(bg_file_path))
      {
         if (!string_is_empty(stripes->bg_file_path))
            free(stripes->bg_file_path);
         stripes->bg_file_path = strdup(bg_file_path);
      }

      fill_pathname_application_special(iconpath,
            PATH_MAX_LENGTH * sizeof(char),
            APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

      stripes_layout(stripes);
      stripes->font = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
            stripes->font_size,
            is_threaded);
      stripes->font2 = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
            stripes->font2_size,
            is_threaded);
      stripes_context_reset_textures(stripes, iconpath);
      stripes_context_reset_background(iconpath);
      stripes_context_reset_horizontal_list(stripes);

      if (!string_is_equal(stripes_thumbnails_ident('R'),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
         stripes_update_thumbnail_image(stripes);
      if (!string_is_equal(stripes_thumbnails_ident('R'),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
         stripes_update_thumbnail_image(stripes);
      stripes_update_savestate_thumbnail_image(stripes);

      free(iconpath);
   }
   video_driver_monitor_reset();
}

static void stripes_navigation_clear(void *data, bool pending_push)
{
   stripes_handle_t  *stripes  = (stripes_handle_t*)data;
   if (!pending_push)
      stripes_selection_pointer_changed(stripes, true);
}

static void stripes_navigation_pointer_changed(void *data)
{
   stripes_handle_t  *stripes  = (stripes_handle_t*)data;
   stripes_selection_pointer_changed(stripes, true);
}

static void stripes_navigation_set(void *data, bool scroll)
{
   stripes_handle_t  *stripes  = (stripes_handle_t*)data;
   stripes_selection_pointer_changed(stripes, true);
}

static void stripes_navigation_alphabet(void *data, size_t *unused)
{
   stripes_handle_t  *stripes  = (stripes_handle_t*)data;
   stripes_selection_pointer_changed(stripes, true);
}

static void stripes_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
      size_t list_size,
      unsigned entry_type)
{
   int current            = 0;
   int i                  = (int)list_size;
   stripes_node_t *node       = NULL;
   stripes_handle_t *stripes      = (stripes_handle_t*)userdata;
   size_t selection       = menu_navigation_get_selection();

   if (!stripes || !list)
      return;

   node = (stripes_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = stripes_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   current           = (int)selection;

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath = strdup(fullpath);
   }

   node->alpha       = stripes->items_passive_alpha;
   node->zoom        = stripes->items_passive_zoom;
   node->label_alpha = node->alpha;
   node->y           = stripes_item_y(stripes, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = stripes->items_active_alpha;
      node->label_alpha = stripes->items_active_alpha;
      node->zoom        = stripes->items_active_alpha;
   }

   file_list_set_userdata(list, i, node);
}

static void stripes_list_clear(file_list_t *list)
{
   menu_animation_ctx_tag tag = (uintptr_t)list;

   menu_animation_kill_by_tag(&tag);

   stripes_free_list_nodes(list, false);
}

static void stripes_list_free(file_list_t *list, size_t a, size_t b)
{
   stripes_list_clear(list);
}

static void stripes_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j = 0;
   menu_animation_ctx_tag tag = (uintptr_t)dst;

   menu_animation_kill_by_tag(&tag);

   stripes_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];

      void *src_udata = s->userdata;
      void *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         file_list_set_userdata(dst, j, (void*)stripes_copy_node((const stripes_node_t*)src_udata));

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, j, data);
      }

      ++j;
   }

   dst->size = j;
}

static void stripes_list_cache(void *data, enum menu_list_type type, unsigned action)
{
   size_t stack_size, list_size;
   stripes_handle_t      *stripes     = (stripes_handle_t*)data;
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   settings_t *settings       = config_get_ptr();

   if (!stripes)
      return;

   /* Check whether to enable the horizontal animation. */
   if (settings->bools.menu_horizontal_animation)
   {
      unsigned first = 0, last = 0;
      unsigned height = 0;
      video_driver_get_size(NULL, &height);

      /* FIXME: this shouldn't be happening at all */
      if (selection >= selection_buf->size)
         selection = selection_buf->size ? selection_buf->size - 1 : 0;

      stripes->selection_ptr_old = selection;

      stripes_calculate_visible_range(stripes, height, selection_buf->size,
            stripes->selection_ptr_old, &first, &last);

      stripes_list_deep_copy(selection_buf, stripes->selection_buf_old, first, last);

      stripes->selection_ptr_old -= first;
      last                   -= first;
      first                   = 0;
   }
   else
      stripes->selection_ptr_old = 0;

   list_size = stripes_list_get_size(stripes, MENU_LIST_HORIZONTAL)
      + stripes->system_tab_end;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         stripes->categories_selection_ptr_old = stripes->categories_selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (stripes->categories_selection_ptr == 0)
               {
                  stripes->categories_selection_ptr = list_size;
                  stripes->categories_active_idx    = (unsigned)(list_size - 1);
               }
               else
                  stripes->categories_selection_ptr--;
               break;
            default:
               if (stripes->categories_selection_ptr == list_size)
               {
                  stripes->categories_selection_ptr = 0;
                  stripes->categories_active_idx = 1;
               }
               else
                  stripes->categories_selection_ptr++;
               break;
         }

         stack_size = menu_stack->size;

         if (menu_stack->list[stack_size - 1].label)
            free(menu_stack->list[stack_size - 1].label);
         menu_stack->list[stack_size - 1].label = NULL;

         switch (stripes_get_system_tab(stripes, (unsigned)stripes->categories_selection_ptr))
         {
            case STRIPES_SYSTEM_TAB_MAIN:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS;
               break;
            case STRIPES_SYSTEM_TAB_SETTINGS:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS_TAB;
               break;
#ifdef HAVE_IMAGEVIEWER
            case STRIPES_SYSTEM_TAB_IMAGES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_IMAGES_TAB;
               break;
#endif
            case STRIPES_SYSTEM_TAB_MUSIC:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_MUSIC_TAB;
               break;
#ifdef HAVE_FFMPEG
            case STRIPES_SYSTEM_TAB_VIDEO:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_VIDEO_TAB;
               break;
#endif
            case STRIPES_SYSTEM_TAB_HISTORY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_HISTORY_TAB;
               break;
            case STRIPES_SYSTEM_TAB_FAVORITES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_FAVORITES_TAB;
               break;
#ifdef HAVE_NETWORKING
            case STRIPES_SYSTEM_TAB_NETPLAY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_NETPLAY_TAB;
               break;
#endif
            case STRIPES_SYSTEM_TAB_ADD:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_ADD_TAB;
               break;
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

static void stripes_context_destroy(void *data)
{
   unsigned i;
   stripes_handle_t *stripes   = (stripes_handle_t*)data;

   if (!stripes)
      return;

   for (i = 0; i < STRIPES_TEXTURE_LAST; i++)
      video_driver_texture_unload(&stripes->textures.list[i]);

   video_driver_texture_unload(&stripes->thumbnail);
   video_driver_texture_unload(&stripes->left_thumbnail);
   video_driver_texture_unload(&stripes->savestate_thumbnail);

   stripes_context_destroy_horizontal_list(stripes);
   stripes_context_bg_destroy(stripes);

   menu_display_font_free(stripes->font);
   menu_display_font_free(stripes->font2);

   stripes->font = NULL;
   stripes->font2 = NULL;
}

static void stripes_toggle(void *userdata, bool menu_on)
{
   menu_animation_ctx_entry_t entry;
   bool tmp             = false;
   stripes_handle_t *stripes    = (stripes_handle_t*)userdata;

   if (!stripes)
      return;

   stripes->depth         = (int)stripes_list_get_size(stripes, MENU_LIST_PLAIN);

   if (!menu_on)
   {
      stripes->alpha = 0;
      return;
   }

   entry.duration     = STRIPES_DELAY * 2;
   entry.target_value = 1.0f;
   entry.subject      = &stripes->alpha;
   entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   menu_animation_push(&entry);

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   stripes_toggle_horizontal_list(stripes);
}

static int stripes_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   if (!menu_displaylist_ctl(
         DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int stripes_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = stripes_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int stripes_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (stripes_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int stripes_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   int i                  = 0;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t *settings = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

            if (settings->bools.menu_content_show_playlists)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0);

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            entry.data            = menu;
            entry.info            = info;
            entry.parse_type      = PARSE_ACTION;
            entry.add_empty_entry = false;

            if (rarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
                  menu_displaylist_setting(&entry);
               }
            }
            else
            {
               if (system->load_no_content)
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
                  menu_displaylist_setting(&entry);
               }

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     entry.enum_idx   = MENU_ENUM_LABEL_CORE_LIST;
                     menu_displaylist_setting(&entry);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);

               if (menu_displaylist_has_subsystems())
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS;
                  menu_displaylist_setting(&entry);
               }
            }

            entry.enum_idx      = MENU_ENUM_LABEL_ADD_CONTENT_LIST;
            menu_displaylist_setting(&entry);
#if defined(HAVE_NETWORKING)
            {
               settings_t *settings      = config_get_ptr();
               if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
                  menu_displaylist_setting(&entry);
               }
            }
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

#ifndef HAVE_DYNAMIC
            if (settings->bools.menu_show_restart_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
               menu_displaylist_setting(&entry);
            }
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_help)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
               menu_displaylist_setting(&entry);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_quit_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
               menu_displaylist_setting(&entry);
            }
#endif

            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
            menu_displaylist_setting(&entry);
            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static bool stripes_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      goto error;

   info.need_push = true;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static int stripes_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection = menu_navigation_get_selection();

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         {
            /* Normal pointer input */
            unsigned header_height = menu_display_get_header_height();

            if (y < header_height)
               return (unsigned)menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
            else if (ptr <= (menu_entries_get_size() - 1))
            {
               if (ptr == selection && cbs && cbs->action_select)
                  return (unsigned)menu_entry_action(entry, selection, MENU_ACTION_SELECT);

               menu_navigation_set_selection(ptr);
               menu_driver_navigation_set(false);
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if ((ptr <= (menu_entries_get_size() - 1)) &&
             (ptr == selection))
            return menu_entry_action(entry, selection, MENU_ACTION_START);
         break;
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_stripes = {
   NULL,
   stripes_messagebox,
   generic_menu_iterate,
   stripes_render,
   stripes_frame,
   stripes_init,
   stripes_free,
   stripes_context_reset,
   stripes_context_destroy,
   stripes_populate_entries,
   stripes_toggle,
   stripes_navigation_clear,
   stripes_navigation_pointer_changed,
   stripes_navigation_pointer_changed,
   stripes_navigation_set,
   stripes_navigation_pointer_changed,
   stripes_navigation_alphabet,
   stripes_navigation_alphabet,
   stripes_menu_init_list,
   stripes_list_insert,
   NULL,
   stripes_list_free,
   stripes_list_clear,
   stripes_list_cache,
   stripes_list_push,
   stripes_list_get_selection,
   stripes_list_get_size,
   stripes_list_get_entry,
   NULL,
   stripes_list_bind_init,
   stripes_load_image,
   "stripes",
   stripes_environ,
   stripes_update_thumbnail_path,
   stripes_update_thumbnail_image,
   stripes_refresh_thumbnail_image,
   stripes_set_thumbnail_system,
   stripes_get_thumbnail_system,
   stripes_set_thumbnail_content,
   stripes_osk_ptr_at_pos,
   stripes_update_savestate_thumbnail_path,
   stripes_update_savestate_thumbnail_image,
   NULL,                                     /* pointer_down */
   stripes_pointer_up,                       /* pointer_up   */
   NULL,                                     /* get_load_content_animation_data   */
   generic_menu_entry_action
};
