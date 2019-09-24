/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <compat/posix_string.h>
#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_input.h"

#include "../widgets/menu_osk.h"

#include "../../core_info.h"
#include "../../core.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#include "../../file_path_special.h"

#include "../../dynamic.h"

/* This struct holds the y position and the line height for each menu entry */
typedef struct
{
   bool switch_is_on;
   bool do_draw_text;
   bool texture_switch_set;
   bool texture_switch2_set;
   unsigned texture_switch_index;
   unsigned texture_switch2_index;
   float line_height;
   float y;
} materialui_node_t;

/* Textures used for the tabs and the switches */
enum
{
   MUI_TEXTURE_POINTER = 0,
   MUI_TEXTURE_BACK,
   MUI_TEXTURE_SWITCH_ON,
   MUI_TEXTURE_SWITCH_OFF,
   MUI_TEXTURE_TAB_MAIN,
   MUI_TEXTURE_TAB_PLAYLISTS,
   MUI_TEXTURE_TAB_SETTINGS,
   MUI_TEXTURE_KEY,
   MUI_TEXTURE_KEY_HOVER,
   MUI_TEXTURE_FOLDER,
   MUI_TEXTURE_PARENT_DIRECTORY,
   MUI_TEXTURE_IMAGE,
   MUI_TEXTURE_ARCHIVE,
   MUI_TEXTURE_VIDEO,
   MUI_TEXTURE_MUSIC,
   MUI_TEXTURE_QUIT,
   MUI_TEXTURE_HELP,
   MUI_TEXTURE_UPDATE,
   MUI_TEXTURE_HISTORY,
   MUI_TEXTURE_INFO,
   MUI_TEXTURE_ADD,
   MUI_TEXTURE_SETTINGS,
   MUI_TEXTURE_FILE,
   MUI_TEXTURE_PLAYLIST,
   MUI_TEXTURE_UPDATER,
   MUI_TEXTURE_QUICKMENU,
   MUI_TEXTURE_NETPLAY,
   MUI_TEXTURE_CORES,
   MUI_TEXTURE_SHADERS,
   MUI_TEXTURE_CONTROLS,
   MUI_TEXTURE_CLOSE,
   MUI_TEXTURE_CORE_OPTIONS,
   MUI_TEXTURE_CORE_CHEAT_OPTIONS,
   MUI_TEXTURE_RESUME,
   MUI_TEXTURE_RESTART,
   MUI_TEXTURE_ADD_TO_FAVORITES,
   MUI_TEXTURE_RUN,
   MUI_TEXTURE_RENAME,
   MUI_TEXTURE_DATABASE,
   MUI_TEXTURE_ADD_TO_MIXER,
   MUI_TEXTURE_SCAN,
   MUI_TEXTURE_REMOVE,
   MUI_TEXTURE_START_CORE,
   MUI_TEXTURE_LOAD_STATE,
   MUI_TEXTURE_SAVE_STATE,
   MUI_TEXTURE_UNDO_LOAD_STATE,
   MUI_TEXTURE_UNDO_SAVE_STATE,
   MUI_TEXTURE_STATE_SLOT,
   MUI_TEXTURE_TAKE_SCREENSHOT,
   MUI_TEXTURE_CONFIGURATIONS,
   MUI_TEXTURE_LOAD_CONTENT,
   MUI_TEXTURE_DISK,
   MUI_TEXTURE_EJECT,
   MUI_TEXTURE_CHECKMARK,
   MUI_TEXTURE_LAST
};

/* The menu has 3 tabs */
enum
{
   MUI_SYSTEM_TAB_MAIN = 0,
   MUI_SYSTEM_TAB_PLAYLISTS,
   MUI_SYSTEM_TAB_SETTINGS
};

#define MUI_SYSTEM_TAB_END MUI_SYSTEM_TAB_SETTINGS

typedef struct materialui_handle
{
   bool need_compute;
   bool need_scroll;
   bool mouse_show;

   int cursor_size;

   unsigned tabs_height;
   unsigned line_height;
   unsigned shadow_height;
   unsigned scrollbar_width;
   unsigned icon_size;
   unsigned margin;
   unsigned glyph_width;
   unsigned glyph_width2;
   unsigned categories_active_idx;
   unsigned categories_active_idx_old;

   size_t categories_selection_ptr;
   size_t categories_selection_ptr_old;

   /* Y position of the vertical scroll */
   float scroll_y;
   float content_height;
   float textures_arrow_alpha;
   float categories_x_pos;

   char *box_message;

   char menu_title[255];

   struct
   {
      menu_texture_item bg;
      menu_texture_item list[MUI_TEXTURE_LAST];
   } textures;

   /* One font for the menu entries, one font for the labels */
   font_data_t *font;
   font_data_t *font2;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;

} materialui_handle_t;

static const char *materialui_texture_path(unsigned id)
{
   switch (id)
   {
      case MUI_TEXTURE_POINTER:
         return "pointer.png";
      case MUI_TEXTURE_BACK:
         return "back.png";
      case MUI_TEXTURE_SWITCH_ON:
         return "on.png";
      case MUI_TEXTURE_SWITCH_OFF:
         return "off.png";
      case MUI_TEXTURE_TAB_MAIN:
         return "main_tab_passive.png";
      case MUI_TEXTURE_TAB_PLAYLISTS:
         return "playlists_tab_passive.png";
      case MUI_TEXTURE_TAB_SETTINGS:
         return "settings_tab_passive.png";
      case MUI_TEXTURE_KEY:
         return "key.png";
      case MUI_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case MUI_TEXTURE_FOLDER:
         return "folder.png";
      case MUI_TEXTURE_PARENT_DIRECTORY:
         return "parent_directory.png";
      case MUI_TEXTURE_IMAGE:
         return "image.png";
      case MUI_TEXTURE_VIDEO:
         return "video.png";
      case MUI_TEXTURE_MUSIC:
         return "music.png";
      case MUI_TEXTURE_ARCHIVE:
         return "archive.png";
      case MUI_TEXTURE_QUIT:
         return "quit.png";
      case MUI_TEXTURE_HELP:
         return "help.png";
      case MUI_TEXTURE_NETPLAY:
         return "netplay.png";
      case MUI_TEXTURE_CORES:
         return "cores.png";
      case MUI_TEXTURE_CONTROLS:
         return "controls.png";
      case MUI_TEXTURE_RESUME:
         return "resume.png";
      case MUI_TEXTURE_RESTART:
         return "restart.png";
      case MUI_TEXTURE_CLOSE:
         return "close.png";
      case MUI_TEXTURE_CORE_OPTIONS:
         return "core_options.png";
      case MUI_TEXTURE_CORE_CHEAT_OPTIONS:
         return "core_cheat_options.png";
      case MUI_TEXTURE_SHADERS:
         return "shaders.png";
      case MUI_TEXTURE_ADD_TO_FAVORITES:
         return "add_to_favorites.png";
      case MUI_TEXTURE_RUN:
         return "run.png";
      case MUI_TEXTURE_RENAME:
         return "rename.png";
      case MUI_TEXTURE_DATABASE:
         return "database.png";
      case MUI_TEXTURE_ADD_TO_MIXER:
         return "add_to_mixer.png";
      case MUI_TEXTURE_SCAN:
         return "scan.png";
      case MUI_TEXTURE_REMOVE:
         return "remove.png";
      case MUI_TEXTURE_START_CORE:
         return "start_core.png";
      case MUI_TEXTURE_LOAD_STATE:
         return "load_state.png";
      case MUI_TEXTURE_SAVE_STATE:
         return "save_state.png";
      case MUI_TEXTURE_DISK:
         return "disk.png";
      case MUI_TEXTURE_EJECT:
         return "eject.png";
      case MUI_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case MUI_TEXTURE_UNDO_LOAD_STATE:
         return "undo_load_state.png";
      case MUI_TEXTURE_UNDO_SAVE_STATE:
         return "undo_save_state.png";
      case MUI_TEXTURE_STATE_SLOT:
         return "state_slot.png";
      case MUI_TEXTURE_TAKE_SCREENSHOT:
         return "take_screenshot.png";
      case MUI_TEXTURE_CONFIGURATIONS:
         return "configurations.png";
      case MUI_TEXTURE_LOAD_CONTENT:
         return "load_content.png";
      case MUI_TEXTURE_UPDATER:
         return "update.png";
      case MUI_TEXTURE_QUICKMENU:
         return "quickmenu.png";
      case MUI_TEXTURE_HISTORY:
         return "history.png";
      case MUI_TEXTURE_INFO:
         return "information.png";
      case MUI_TEXTURE_ADD:
         return "add.png";
      case MUI_TEXTURE_SETTINGS:
         return "settings.png";
      case MUI_TEXTURE_FILE:
         return "file.png";
      case MUI_TEXTURE_PLAYLIST:
         return "playlist.png";
   }

   return NULL;
}

static void materialui_context_reset_textures(materialui_handle_t *mui)
{
   unsigned i;
   char *iconpath = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

   iconpath[0]    = '\0';

   fill_pathname_application_special(iconpath,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS);

   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      menu_display_reset_textures_list(materialui_texture_path(i), iconpath, &mui->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
   free(iconpath);
}

static void materialui_draw_icon(
      video_frame_info_t *video_info,
      unsigned icon_size,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   menu_display_blend_begin(video_info);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = height - y - icon_size;
   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

/* Draw a single tab */
static void materialui_draw_tab(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned i,
      unsigned width, unsigned height,
      float *tab_color,
      float *active_tab_color)
{
   unsigned tab_icon = 0;

   switch (i)
   {
      case MUI_SYSTEM_TAB_MAIN:
         tab_icon = MUI_TEXTURE_TAB_MAIN;
         if (i == mui->categories_selection_ptr)
            tab_color = active_tab_color;
         break;
      case MUI_SYSTEM_TAB_PLAYLISTS:
         tab_icon = MUI_TEXTURE_TAB_PLAYLISTS;
         if (i == mui->categories_selection_ptr)
            tab_color = active_tab_color;
         break;
      case MUI_SYSTEM_TAB_SETTINGS:
         tab_icon = MUI_TEXTURE_TAB_SETTINGS;
         if (i == mui->categories_selection_ptr)
            tab_color = active_tab_color;
         break;
   }

   materialui_draw_icon(video_info,
         mui->icon_size,
         mui->textures.list[tab_icon],
         width / (MUI_SYSTEM_TAB_END+1) * (i+0.5) - mui->icon_size/2,
         height - mui->tabs_height,
         width,
         height,
         0,
         1,
         &tab_color[0]);
}

/* Draw the tabs background */
static void materialui_draw_tab_begin(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      float *tabs_bg_color, float *tabs_separator_color)
{
   float scale_factor = menu_display_get_dpi(video_info->width,
         video_info->height);

   mui->tabs_height   = scale_factor / 3;

   /* tabs background */
   menu_display_draw_quad(
         video_info,
         0, height - mui->tabs_height, width,
         mui->tabs_height,
         width, height,
         tabs_bg_color);

   /* tabs separator */
   menu_display_draw_quad(
         video_info,
         0, height - mui->tabs_height, width,
         1,
         width, height,
         tabs_separator_color);
}

/* Draw the active tab */
static void materialui_draw_tab_end(materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      unsigned header_height,
      float *active_tab_marker_color)
{
   /* active tab marker */
   unsigned tab_width = width / (MUI_SYSTEM_TAB_END+1);

   menu_display_draw_quad(
         video_info,
         (int)(mui->categories_selection_ptr * tab_width),
         height - (header_height/16),
         tab_width,
         header_height/16,
         width, height,
         &active_tab_marker_color[0]);
}

/* Draw the scrollbar */
static void materialui_draw_scrollbar(materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height, float *coord_color)
{
   unsigned header_height = menu_display_get_header_height();
   float total_height     = height - header_height - mui->tabs_height;
   float scrollbar_margin = mui->scrollbar_width;
   float scrollbar_height = total_height / (mui->content_height / total_height);
   float y                = total_height * mui->scroll_y / mui->content_height;

   /* apply a margin on the top and bottom of the scrollbar for aestetic */
   scrollbar_height      -= scrollbar_margin * 2;
   y                     += scrollbar_margin;

   if (mui->content_height < total_height)
      return;

   /* if the scrollbar is extremely short, display it as a square */
   if (scrollbar_height <= mui->scrollbar_width)
      scrollbar_height = mui->scrollbar_width;

   menu_display_draw_quad(
         video_info,
         width - mui->scrollbar_width - scrollbar_margin,
         header_height + y,
         mui->scrollbar_width,
         scrollbar_height,
         width, height,
         coord_color);
}

static void materialui_get_message(void *data, const char *message)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui || !message || !*message)
      return;

   if (!string_is_empty(mui->box_message))
      free(mui->box_message);
   mui->box_message = strdup(message);
}

/* Draw the modal */
static void materialui_render_messagebox(materialui_handle_t *mui,
      video_frame_info_t *video_info,
      const char *message, float *body_bg_color, uint32_t font_color)
{
   unsigned i, y_position;
   int x, y, line_height, longest = 0, longest_width = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = NULL;

   if (!mui || !mui->font)
      goto end;

   list                     = (struct string_list*)
      string_split(message, "\n");

   if (!list || list->elems == 0)
      goto end;

   line_height = mui->font->size * 1.2;

   y_position = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position = height / 4;

   x = width  / 2;
   y = (int)(y_position - (list->size-1) * line_height / 2);

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int len         = (int)utf8len(msg);
      if (len > longest)
      {
         longest = len;
         longest_width = font_driver_get_message_width(
               mui->font, msg, (unsigned)strlen(msg), 1);
      }
   }

   if (body_bg_color)
   {
      menu_display_set_alpha(body_bg_color, 1.0);

      menu_display_draw_quad(
            video_info,
            x - longest_width / 2.0 -  mui->margin * 2.0,
            y - line_height   / 2.0 -  mui->margin * 2.0,
            longest_width +            mui->margin * 4.0,
            line_height * list->size + mui->margin * 4.0,
            width,
            height,
            &body_bg_color[0]);
   }

   /* print each line */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         menu_display_draw_text(
               mui->font, msg,
               x - longest_width/2.0,
               y + i * line_height + mui->font->size / 3,
               width, height, font_color, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

   }

   if (menu_input_dialog_get_display_kb())
      menu_display_draw_keyboard(
            mui->textures.list[MUI_TEXTURE_KEY_HOVER],
            mui->font,
            video_info,
            menu_event_get_osk_grid(), menu_event_get_osk_ptr(),
            0xffffffff);

end:
   if (list)
      string_list_free(list);
}

/* Used for the sublabels */
static unsigned materialui_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

/* Compute the line height for each menu entry. */
static void materialui_compute_entries_box(materialui_handle_t* mui, int width,
      int height)
{
   unsigned i;
   size_t usable_width       = width - (mui->margin * 2);
   file_list_t *list         = menu_entries_get_selection_buf_ptr(0);
   float sum                 = 0;
   size_t entries_end        = menu_entries_get_size();
   float scale_factor        = menu_display_get_dpi(width, height);

   for (i = 0; i < entries_end; i++)
   {
      menu_entry_t entry;
      char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
      const char *sublabel_str  = NULL;
      unsigned lines            = 0;
      materialui_node_t *node   = (materialui_node_t*)
         file_list_get_userdata_at_offset(list, i);

      wrapped_sublabel_str[0] = '\0';

      menu_entry_init(&entry);
      entry.path_enabled       = false;
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      menu_entry_get(&entry, 0, i, NULL, true);

      menu_entry_get_sublabel(&entry, &sublabel_str);

      if (!string_is_empty(sublabel_str))
      {
         int icon_margin = 0;

         if (node->texture_switch2_set)
            if (mui->textures.list[node->texture_switch2_index])
               icon_margin = mui->icon_size;

         word_wrap(wrapped_sublabel_str, sublabel_str,
               (int)((usable_width - icon_margin) / mui->glyph_width2),
               false, 0);
         lines = materialui_count_lines(wrapped_sublabel_str);
      }

      node->line_height  = (scale_factor / 3) + (lines * mui->font->size);
      node->y            = sum;
      sum               += node->line_height;
   }

   mui->content_height = sum;
}

/* Compute the scroll value depending on the highlighted entry */
static float materialui_get_scroll(materialui_handle_t *mui)
{
   unsigned i, width, height = 0;
   float half, sum = 0;
   size_t selection   = menu_navigation_get_selection();
   file_list_t *list  = menu_entries_get_selection_buf_ptr(0);

   if (!mui)
      return 0;

   /* Whenever we perform a 'manual' scroll, scroll
    * acceleration must be reset */
   menu_input_set_pointer_y_accel(0.0f);

   video_driver_get_size(&width, &height);

   half = height / 2;

   for (i = 0; i < selection; i++)
   {
      materialui_node_t *node   = (materialui_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (node)
         sum += node->line_height;
   }

   if (sum < half)
      return 0;

   return sum - half;
}

/* Called on each frame. We use this callback to implement the touch scroll
   with acceleration */
static void materialui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   int bottom, header_height;
   menu_input_pointer_t pointer;
   size_t        i             = 0;
   materialui_handle_t *mui    = (materialui_handle_t*)data;
   file_list_t        *list    = menu_entries_get_selection_buf_ptr(0);

   if (!mui)
      return;

   /* Here's a nasty issue:
    * After calling populate_entries(), we need to call
    * materialui_get_scroll() so the last selected item
    * is correctly displayed on screen.
    * But we can't do this until materialui_compute_entries_box()
    * has been called, so we should delegate it until mui->need_compute
    * is acted upon.
    * *But* we can't do this in the same frame that mui->need_compute
    * is acted upon, because of the order in which materialui_frame()
    * and materialui_render() are called. Since mui->tabs_height is
    * set by materialui_frame(), the first time materialui_render() is
    * called after populate_entries() it has the wrong mui->tabs_height
    * value...
    * We therefore have to delegate the scroll until the frame after
    * mui->need_compute is handled... */
   if (mui->need_scroll)
   {
      mui->scroll_y    = materialui_get_scroll(mui);
      mui->need_scroll = false;
   }

   if (mui->need_compute)
   {
      if (mui->font)
         materialui_compute_entries_box(mui, width, height);
      mui->need_compute = false;
      mui->need_scroll  = true;
   }

   menu_display_set_width(width);
   menu_display_set_height(height);
   header_height = menu_display_get_header_height();

   menu_input_get_pointer_state(&pointer);

   if (pointer.type != MENU_POINTER_DISABLED)
   {
      size_t ii;
      int16_t pointer_y   = pointer.y;
      size_t entries_end  = menu_entries_get_size();

      for (ii = 0; ii < entries_end; ii++)
      {
         materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, ii);

         if ((pointer_y > (-mui->scroll_y + header_height + node->y)) &&
             (pointer_y < (-mui->scroll_y + header_height + node->y + node->line_height)))
         {
            menu_input_set_pointer_selection(ii);
            break;
         }
      }

      mui->scroll_y -= pointer.y_accel;
   }

   if (mui->scroll_y < 0)
      mui->scroll_y = 0;

   bottom = mui->content_height - height + header_height + mui->tabs_height;
   if (mui->scroll_y > bottom)
      mui->scroll_y = bottom;

   if (mui->content_height
         < height - header_height - mui->tabs_height)
      mui->scroll_y = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
}

/* Display an entry value on the right of the screen. */
static void materialui_render_label_value(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      materialui_node_t *node,
      int i, int y, unsigned width, unsigned height,
      uint64_t index, uint32_t color, bool selected, const char *label,
      const char *value, float *label_color,
      uint32_t sublabel_color)
{
   menu_entry_t entry;
   menu_animation_ctx_ticker_t ticker;
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   char label_str[255];
   char value_str[255];
   char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
   unsigned ticker_label_x_offset  = 0;
   unsigned ticker_value_x_offset  = 0;
   unsigned ticker_str_width       = 0;
   int value_x_offset              = 0;
   unsigned entry_type             = 0;
   const char *sublabel_str        = NULL;
   bool switch_is_on               = true;
   int value_len                   = (int)utf8len(value);
   int ticker_limit                = 0;
   uintptr_t texture_switch        = 0;
   uintptr_t texture_switch2       = 0;
   bool do_draw_text               = false;
   size_t usable_width             = width - (mui->margin * 2);
   int icon_margin                 = 0;
   enum msg_file_type hash_type    = msg_hash_to_file_type(msg_hash_calculate(value));
   float scale_factor              = menu_display_get_dpi(video_info->width,
         video_info->height);
   settings_t *settings            = config_get_ptr();
   bool use_smooth_ticker          = settings->bools.menu_ticker_smooth;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = menu_animation_get_ticker_pixel_idx();
      ticker_smooth.font          = mui->font;
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker_smooth.spacer        = NULL;
      ticker_smooth.dst_str_width = &ticker_str_width;
   }
   else
   {
      ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker.spacer    = NULL;
   }

   label_str[0] = value_str[0] = wrapped_sublabel_str[0] = '\0';

   menu_entry_init(&entry);
   entry.path_enabled       = false;
   entry.label_enabled      = false;
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   menu_entry_get(&entry, 0, i, NULL, true);
   entry_type = menu_entry_get_type_new(&entry);

   if (value_len * mui->glyph_width > usable_width / 2)
      value_len    = (int)((usable_width/2) / mui->glyph_width);

   ticker_limit    = (int)((usable_width / mui->glyph_width) - (value_len + 3));

   if (use_smooth_ticker)
   {
      /* Label */
      ticker_smooth.selected    = selected;
      ticker_smooth.field_width = ticker_limit * mui->glyph_width;
      ticker_smooth.src_str     = label;
      ticker_smooth.dst_str     = label_str;
      ticker_smooth.dst_str_len = sizeof(label_str);
      ticker_smooth.x_offset    = &ticker_label_x_offset;

      menu_animation_ticker_smooth(&ticker_smooth);

      /* Value */
      ticker_smooth.field_width = (value_len + 1) * mui->glyph_width;
      ticker_smooth.src_str     = value;
      ticker_smooth.dst_str     = value_str;
      ticker_smooth.dst_str_len = sizeof(value_str);
      ticker_smooth.x_offset    = &ticker_value_x_offset;

      /* Value text is right aligned, so have to offset x
       * by the 'padding' width at the end of the ticker string... */
      if (menu_animation_ticker_smooth(&ticker_smooth))
         value_x_offset = (ticker_value_x_offset + ticker_str_width) - ticker_smooth.field_width;
   }
   else
   {
      ticker.s        = label_str;
      ticker.len      = ticker_limit;
      ticker.idx      = index;
      ticker.str      = label;
      ticker.selected = selected;

      menu_animation_ticker(&ticker);

      ticker.s        = value_str;
      ticker.len      = value_len;
      ticker.str      = value;

      menu_animation_ticker(&ticker);
   }

   /* set switch_is_on */
   /* set texture_switch */
   if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      if (mui->textures.list[MUI_TEXTURE_SWITCH_OFF])
      {
         switch_is_on = false;
         texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_OFF];
      }
      else
         do_draw_text = true;
   }
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      if (mui->textures.list[MUI_TEXTURE_SWITCH_ON])
      {
         switch_is_on = true;
         texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_ON];
      }
      else
         do_draw_text = true;
   }
   else if
      (
       (entry.checked) && 
       ((entry_type >= MENU_SETTING_DROPDOWN_ITEM) && (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL))
      )
      {
         texture_switch = mui->textures.list[MUI_TEXTURE_CHECKMARK];
         node->texture_switch2_set   = false;
      }
   /* set do_draw_text */
   else
   {
      switch (hash_type)
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
         default:
            do_draw_text = true;
            break;
      }
   }

   /* set texture_switch2 */
   if (node->texture_switch2_set)
      texture_switch2 = mui->textures.list[node->texture_switch2_index];
   else
   {
      switch (hash_type)
      {
         case FILE_TYPE_COMPRESSED:
            texture_switch2 = mui->textures.list[MUI_TEXTURE_ARCHIVE];
            break;
         case FILE_TYPE_IMAGE:
            texture_switch2 = mui->textures.list[MUI_TEXTURE_IMAGE];
            break;
         default:
            break;
      }
   }

   menu_entry_get_sublabel(&entry, &sublabel_str);

   if (texture_switch2)
      icon_margin      = mui->icon_size;

   /* Sublabel */
   if (!string_is_empty(sublabel_str) && mui->font)
   {
      word_wrap(wrapped_sublabel_str, sublabel_str,
            (int)((usable_width - icon_margin) / mui->glyph_width2),
            false, 0);

      menu_display_draw_text(mui->font2, wrapped_sublabel_str,
            mui->margin + icon_margin,
            y + (scale_factor / 4) + mui->font->size,
            width, height, sublabel_color, TEXT_ALIGN_LEFT,
            1.0f, false, 0, false);
   }

   menu_display_draw_text(mui->font, label_str,
         ticker_label_x_offset + mui->margin + icon_margin,
         y + (scale_factor / 5),
         width, height, color, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

   if (do_draw_text)
      menu_display_draw_text(mui->font, value_str,
            value_x_offset + width - mui->margin,
            y + (scale_factor / 5),
            width, height, color, TEXT_ALIGN_RIGHT, 1.0f, false, 0, false);

   if (texture_switch2)
      materialui_draw_icon(video_info,
            mui->icon_size,
            (uintptr_t)texture_switch2,
            0,
            y + (scale_factor / 6) - mui->icon_size/2,
            width,
            height,
            0,
            1,
            &label_color[0]
            );

   if (texture_switch)
   {
      /* This will be used instead of label_color if
       * texture_switch is 'off' icon */
      float pure_white[16]=  {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };

      materialui_draw_icon(video_info,
            mui->icon_size,
            (uintptr_t)texture_switch,
            width - mui->margin    - mui->icon_size,
            y + (scale_factor / 6) - mui->icon_size/2,
            width,
            height,
            0,
            1,
            switch_is_on ? &label_color[0] :  &pure_white[0]
            );
   }
}

static void materialui_render_menu_list(
      video_frame_info_t *video_info,
      materialui_handle_t *mui,
      unsigned width, unsigned height,
      uint32_t font_normal_color,
      uint32_t font_hover_color,
      float *menu_list_color,
      uint32_t sublabel_color)
{
   size_t i;
   float sum                               = 0;
   size_t entries_end                      = 0;
   file_list_t *list                       = NULL;
   unsigned header_height                  =
      menu_display_get_header_height();

   mui->raster_block.carr.coords.vertices  = 0;
   mui->raster_block2.carr.coords.vertices = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   list                                    =
      menu_entries_get_selection_buf_ptr(0);

   entries_end = menu_entries_get_size();

   for (i = 0; i < entries_end; i++)
   {
      menu_entry_t entry;
      const char *entry_value    = NULL;
      const char *rich_label     = NULL;
      bool entry_selected        = false;
      materialui_node_t *node    = (materialui_node_t*)
         file_list_get_userdata_at_offset(list, i);
      size_t selection           = menu_navigation_get_selection();
      int               y        = header_height - mui->scroll_y + sum;

      sum += node->line_height;

      if (y + (int)node->line_height < 0)
         continue;

      if (y > (int)height)
         break;

      menu_entry_init(&entry);
      entry.path_enabled     = false;
      entry.label_enabled    = false;
      entry.sublabel_enabled = false;
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);
      menu_entry_get_value(&entry, &entry_value);
      menu_entry_get_rich_label(&entry, &rich_label);
      entry_selected = selection == i;

      /* Render label, value, and associated icons */

      materialui_render_label_value(
            mui,
            video_info,
            node,
            (int)i,
            y,
            width,
            height,
            menu_animation_get_ticker_idx(),
            font_hover_color,
            entry_selected,
            rich_label,
            entry_value,
            menu_list_color,
            sublabel_color
            );
   }
}

static size_t materialui_list_get_size(void *data, enum menu_list_type type)
{
   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_TABS:
         return MUI_SYSTEM_TAB_END;
      default:
         break;
   }

   return 0;
}

static int materialui_get_core_title(char *s, size_t len)
{
   settings_t *settings              = config_get_ptr();
   struct retro_system_info *system  = runloop_get_libretro_system_info();
   const char *core_name             = system ? system->library_name : NULL;
   const char *core_version          = system ? system->library_version : NULL;

   if (!settings->bools.menu_core_enable)
      return -1;

   if (string_is_empty(core_name))
      core_name    = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
   if (!core_version)
      core_version = "";

   snprintf(s, len, "%s %s", core_name, core_version);

   return 0;
}

static void materialui_draw_bg(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   bool add_opacity       = false;
   float opacity_override = video_info->menu_wallpaper_opacity;

   menu_display_blend_begin(video_info);

   draw->x               = 0;
   draw->y               = 0;
   draw->pipeline.id     = 0;
   draw->pipeline.active = false;

   if (video_info->libretro_running)
   {
      add_opacity      = true;
      opacity_override = video_info->menu_framebuffer_opacity;
   }

   menu_display_draw_bg(draw, video_info, add_opacity,
         opacity_override);
   menu_display_draw(draw, video_info);
   menu_display_blend_end(video_info);
}

/* Main function of the menu driver. Takes care of drawing the header, the tabs,
   and the menu list */
static void materialui_frame(void *data, video_frame_info_t *video_info)
{
   /* This controls the main background color */
   menu_display_ctx_clearcolor_t clearcolor;

   menu_animation_ctx_ticker_t ticker;
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   unsigned ticker_x_offset = 0;
   menu_display_ctx_draw_t draw;
   char msg[255];
   char menu_title[640];
   char title_buf[640];
   char title_msg[255];

   float black_bg[16]   = {
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
   };

   float white_bg[16] = {
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
   };

   float white_transp_bg[16] = {
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
   };

   float grey_bg[16] = {
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
   };

   /* TODO/FIXME  convert this over to new hex format */
   float greyish_blue[16] = {
      0.22, 0.28, 0.31, 1.00,
      0.22, 0.28, 0.31, 1.00,
      0.22, 0.28, 0.31, 1.00,
      0.22, 0.28, 0.31, 1.00,
   };
   float almost_black[16] = {
      0.13, 0.13, 0.13, 0.90,
      0.13, 0.13, 0.13, 0.90,
      0.13, 0.13, 0.13, 0.90,
      0.13, 0.13, 0.13, 0.90,
   };

   /* https://material.google.com/style/color.html#color-color-palette */
   /* Hex values converted to RGB normalized decimals, alpha set to 1 */
   float blue_500[16]              = {0};
   float blue_50[16]               = {0};
   float green_500[16]             = {0};
   float green_50[16]              = {0};
   float red_500[16]               = {0};
   float red_50[16]                = {0};
   float yellow_500[16]            = {0};
   float blue_grey_500[16]         = {0};
   float blue_grey_50[16]          = {0};
   float yellow_200[16]            = {0};
   float color_nv_header[16]       = {0};
   float color_nv_body[16]         = {0};
   float color_nv_accent[16]       = {0};
   float footer_bg_color_real[16]  = {0};
   float header_bg_color_real[16]  = {0};

   /* Default is blue theme */
   float *header_bg_color          = NULL;
   float *highlighted_entry_color  = NULL;
   float *footer_bg_color          = NULL;
   float *body_bg_color            = NULL;
   float *active_tab_marker_color  = NULL;
   float *passive_tab_icon_color   = grey_bg;

   file_list_t *list               = NULL;
   materialui_node_t *node         = NULL;

   unsigned width                  = video_info->width;
   unsigned height                 = video_info->height;
   unsigned i                      = 0;
   unsigned header_height          = 0;
   uint32_t sublabel_color         = 0x888888ff;
   uint32_t font_normal_color      = 0;
   uint32_t font_hover_color       = 0;
   uint32_t font_header_color      = 0;

   uint32_t black_opaque_54        = 0x0000008a;
   uint32_t black_opaque_87        = 0x000000de;
   uint32_t white_opaque_70        = 0xffffffb3;

   size_t usable_width             = 0;
   size_t selection                = 0;
   size_t title_margin             = 0;

   bool background_rendered        = false;
   bool libretro_running           = video_info->libretro_running;

   settings_t *settings            = config_get_ptr();
   materialui_handle_t *mui        = (materialui_handle_t*)data;
   bool use_smooth_ticker          = settings->bools.menu_ticker_smooth;

   if (!mui)
      return;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = menu_animation_get_ticker_pixel_idx();
      ticker_smooth.font          = mui->font;
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker_smooth.spacer        = NULL;
      ticker_smooth.x_offset      = &ticker_x_offset;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx       = menu_animation_get_ticker_idx();
      ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker.spacer    = NULL;
   }

   msg[0] = menu_title[0] = title_buf[0] = title_msg[0] = '\0';

   switch (video_info->materialui_color_theme)
   {
      case MATERIALUI_THEME_BLUE:
         hex32_to_rgba_normalized(0x2196F3, blue_500,       1.00);
         hex32_to_rgba_normalized(0x2196F3, header_bg_color_real, 1.00);
         hex32_to_rgba_normalized(0xE3F2FD, blue_50,        0.90);
         hex32_to_rgba_normalized(0xFFFFFF, footer_bg_color_real, 1.00);

         header_bg_color         = header_bg_color_real;
         highlighted_entry_color = blue_50;
         footer_bg_color         = footer_bg_color_real;
         body_bg_color           = white_transp_bg;
         active_tab_marker_color = blue_500;

         font_normal_color       = black_opaque_54;
         font_hover_color        = black_opaque_87;
         font_header_color       = 0xffffffff;

         clearcolor.r            = 1.0f;
         clearcolor.g            = 1.0f;
         clearcolor.b            = 1.0f;
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_BLUE_GREY:
         hex32_to_rgba_normalized(0x607D8B, blue_grey_500,  1.00);
         hex32_to_rgba_normalized(0x607D8B, header_bg_color_real,  1.00);
         hex32_to_rgba_normalized(0xCFD8DC, blue_grey_50,   0.90);
         hex32_to_rgba_normalized(0xFFFFFF, footer_bg_color_real, 1.00);

         header_bg_color         = header_bg_color_real;
         body_bg_color           = white_transp_bg;
         highlighted_entry_color = blue_grey_50;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = blue_grey_500;

         font_normal_color       = black_opaque_54;
         font_hover_color        = black_opaque_87;
         font_header_color       = 0xffffffff;

         clearcolor.r            = 1.0f;
         clearcolor.g            = 1.0f;
         clearcolor.b            = 1.0f;
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_GREEN:
         hex32_to_rgba_normalized(0x4CAF50, green_500,      1.00);
         hex32_to_rgba_normalized(0x4CAF50, header_bg_color_real,      1.00);
         hex32_to_rgba_normalized(0xC8E6C9, green_50,       0.90);
         hex32_to_rgba_normalized(0xFFFFFF, footer_bg_color_real, 1.00);

         header_bg_color         = header_bg_color_real;
         body_bg_color           = white_transp_bg;
         highlighted_entry_color = green_50;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = green_500;

         font_normal_color       = black_opaque_54;
         font_hover_color        = black_opaque_87;
         font_header_color       = 0xffffffff;

         clearcolor.r            = 1.0f;
         clearcolor.g            = 1.0f;
         clearcolor.b            = 1.0f;
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_RED:
         hex32_to_rgba_normalized(0xF44336, red_500,        1.00);
         hex32_to_rgba_normalized(0xF44336, header_bg_color_real,        1.00);
         hex32_to_rgba_normalized(0xFFEBEE, red_50,         0.90);
         hex32_to_rgba_normalized(0xFFFFFF, footer_bg_color_real, 1.00);

         header_bg_color         = header_bg_color_real;
         body_bg_color           = white_transp_bg;
         highlighted_entry_color = red_50;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = red_500;

         font_normal_color       = black_opaque_54;
         font_hover_color        = black_opaque_87;
         font_header_color       = 0xffffffff;

         clearcolor.r            = 1.0f;
         clearcolor.g            = 1.0f;
         clearcolor.b            = 1.0f;
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_YELLOW:
         hex32_to_rgba_normalized(0xFFEB3B, yellow_500,     1.00);
         hex32_to_rgba_normalized(0xFFEB3B, header_bg_color_real,     1.00);
         hex32_to_rgba_normalized(0xFFF9C4, yellow_200,     0.90);
         hex32_to_rgba_normalized(0xFFFFFF, footer_bg_color_real, 1.00);

         header_bg_color         = header_bg_color_real;
         body_bg_color           = white_transp_bg;
         highlighted_entry_color = yellow_200;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = yellow_500;

         font_normal_color       = black_opaque_54;
         font_hover_color        = black_opaque_87;
         font_header_color       = black_opaque_54;

         clearcolor.r            = 1.0f;
         clearcolor.g            = 1.0f;
         clearcolor.b            = 1.0f;
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_DARK_BLUE:
         hex32_to_rgba_normalized(0x212121, footer_bg_color_real, 1.00);
         memcpy(header_bg_color_real, greyish_blue, sizeof(header_bg_color_real));
         header_bg_color         = header_bg_color_real;
         body_bg_color           = almost_black;
         highlighted_entry_color = grey_bg;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = greyish_blue;

         font_normal_color       = white_opaque_70;
         font_hover_color        = 0xffffffff;
         font_header_color       = 0xffffffff;

         clearcolor.r            = body_bg_color[0];
         clearcolor.g            = body_bg_color[1];
         clearcolor.b            = body_bg_color[2];
         clearcolor.a            = 0.75f;
         break;
      case MATERIALUI_THEME_NVIDIA_SHIELD:
         hex32_to_rgba_normalized(0x282F37, color_nv_header,1.00);
         hex32_to_rgba_normalized(0x282F37, header_bg_color_real,1.00);
         hex32_to_rgba_normalized(0x202427, color_nv_body,  0.90);
         hex32_to_rgba_normalized(0x77B900, color_nv_accent,0.90);
         hex32_to_rgba_normalized(0x202427, footer_bg_color_real,  1.00);

         sublabel_color          = 0xffffffff;
         header_bg_color         = header_bg_color_real;
         body_bg_color           = color_nv_body;
         highlighted_entry_color = color_nv_accent;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = white_bg;

         font_normal_color       = 0xbbc0c4ff;
         font_hover_color        = 0xffffffff;
         font_header_color       = 0xffffffff;

         clearcolor.r            = color_nv_body[0];
         clearcolor.g            = color_nv_body[1];
         clearcolor.b            = color_nv_body[2];
         clearcolor.a            = 0.75f;
         break;
   }

   menu_display_set_alpha(header_bg_color_real, video_info->menu_header_opacity);
   menu_display_set_alpha(footer_bg_color_real, video_info->menu_footer_opacity);

   menu_display_set_viewport(video_info->width, video_info->height);
   header_height = menu_display_get_header_height();

   if (libretro_running)
   {
      draw.x                  = 0;
      draw.y                  = 0;
      draw.width              = width;
      draw.height             = height;
      draw.coords             = NULL;
      draw.matrix_data        = NULL;
      draw.texture            = menu_display_white_texture;
      draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
      draw.color              = body_bg_color ? &body_bg_color[0] : NULL;
      draw.vertex             = NULL;
      draw.tex_coord          = NULL;
      draw.vertex_count       = 4;

      draw.pipeline.id        = 0;
      draw.pipeline.active    = false;
      draw.pipeline.backend_data = NULL;

      materialui_draw_bg(&draw, video_info);
   }
   else
   {
      menu_display_clear_color(&clearcolor, video_info);

      if (mui->textures.bg)
      {
         background_rendered     = true;

         menu_display_set_alpha(white_transp_bg, 0.30);

         draw.x                  = 0;
         draw.y                  = 0;
         draw.width              = width;
         draw.height             = height;
         draw.coords             = NULL;
         draw.matrix_data        = NULL;
         draw.texture            = mui->textures.bg;
         draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
         draw.color              = &white_transp_bg[0];
         draw.vertex             = NULL;
         draw.tex_coord          = NULL;
         draw.vertex_count       = 4;

         draw.pipeline.id        = 0;
         draw.pipeline.active    = false;
         draw.pipeline.backend_data = NULL;

         if (draw.texture)
            draw.color           = &white_bg[0];

         materialui_draw_bg(&draw, video_info);

         /* Restore opacity of transposed white background */
         menu_display_set_alpha(white_transp_bg, 0.90);
      }
   }

   selection = menu_navigation_get_selection();

   if (background_rendered || libretro_running)
      menu_display_set_alpha(blue_50, 0.75);
   else
      menu_display_set_alpha(blue_50, 1.0);

   /* highlighted entry */
   list             = menu_entries_get_selection_buf_ptr(0);
   node             = (materialui_node_t*)file_list_get_userdata_at_offset(
         list, selection);

   if (node)
      menu_display_draw_quad(
            video_info,
            0,
            header_height - mui->scroll_y + node->y,
            width,
            node->line_height,
            width,
            height,
            highlighted_entry_color ? &highlighted_entry_color[0] : NULL
            );

   font_driver_bind_block(mui->font, &mui->raster_block);
   font_driver_bind_block(mui->font2, &mui->raster_block2);

   if (menu_display_get_update_pending())
      materialui_render_menu_list(
            video_info,
            mui,
            width,
            height,
            font_normal_color,
            font_hover_color,
            active_tab_marker_color ? &active_tab_marker_color[0] : NULL,
            sublabel_color
            );

   font_driver_flush(video_info->width, video_info->height, mui->font,
         video_info);
   font_driver_bind_block(mui->font, NULL);

   font_driver_flush(video_info->width,
         video_info->height,
         mui->font2,
         video_info);
   font_driver_bind_block(mui->font2, NULL);

   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);

   /* header */
   menu_display_draw_quad(
         video_info,
         0,
         0,
         width,
         header_height,
         width,
         height,
         header_bg_color ? &header_bg_color[0] : NULL);

   mui->tabs_height = 0;

   /* display tabs if depth equal one, if not hide them */
   if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
   {
      materialui_draw_tab_begin(mui,
            video_info,
            width, height,
            footer_bg_color ? &footer_bg_color[0] : NULL,
            &grey_bg[0]);

      for (i = 0; i <= MUI_SYSTEM_TAB_END; i++)
         materialui_draw_tab(mui, video_info,
               i, width, height,
               &passive_tab_icon_color[0],
               active_tab_marker_color ? &active_tab_marker_color[0] : NULL
               );

      materialui_draw_tab_end(mui,
            video_info,
            width, height, header_height,
            active_tab_marker_color ? &active_tab_marker_color[0] : NULL
            );
   }

   {
      float shadow_bg[16]=  {
         0.00, 0.00, 0.00, 0.00,
         0.00, 0.00, 0.00, 0.00,
         0.00, 0.00, 0.00, 0.20,
         0.00, 0.00, 0.00, 0.20,
      };

      menu_display_draw_quad(
            video_info,
            0,
            header_height,
            width,
            mui->shadow_height,
            width,
            height,
            &shadow_bg[0]);
   }

   title_margin = mui->margin;

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      float pure_white[16] = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };

      title_margin = mui->icon_size;
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_BACK],
            0,
            0,
            width,
            height,
            0,
            1,
            &pure_white[0]
            );
   }

   /* Title */
   usable_width = width - (mui->margin * 2) - title_margin;

   strlcpy(menu_title, mui->menu_title, sizeof(menu_title));

   if (materialui_get_core_title(title_msg, sizeof(title_msg)) == 0)
   {
      strlcat(menu_title, " (", sizeof(menu_title));
      strlcat(menu_title, title_msg, sizeof(menu_title));
      strlcat(menu_title, ")", sizeof(menu_title));
   }

   if (use_smooth_ticker)
   {
      ticker_smooth.selected    = true;
      ticker_smooth.field_width = (unsigned)usable_width;
      ticker_smooth.src_str     = menu_title;
      ticker_smooth.dst_str     = title_buf;
      ticker_smooth.dst_str_len = sizeof(title_buf);

      menu_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = title_buf;
      ticker.len      = (unsigned)(usable_width / mui->glyph_width);
      ticker.str      = menu_title;
      ticker.selected = true;

      menu_animation_ticker(&ticker);
   }

   if (mui->font)
      menu_display_draw_text(mui->font, title_buf,
            ticker_x_offset + title_margin,
            header_height / 2 + mui->font->size / 3,
            width, height, font_header_color, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

   materialui_draw_scrollbar(mui, video_info, width, height, &grey_bg[0]);

   if (menu_input_dialog_get_display_kb())
   {
      const char *str          = menu_input_dialog_get_buffer();
      const char *label        = menu_input_dialog_get_label_buffer();

      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, &black_bg[0]);
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);

      materialui_render_messagebox(mui, video_info,
            msg, &body_bg_color[0], font_hover_color);
   }

   if (!string_is_empty(mui->box_message))
   {
      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, &black_bg[0]);

      materialui_render_messagebox(mui, video_info,
            mui->box_message, &body_bg_color[0], font_hover_color);

      free(mui->box_message);
      mui->box_message    = NULL;
   }

   if (mui->mouse_show)
   {
      menu_input_pointer_t pointer;
      menu_input_get_pointer_state(&pointer);

      menu_display_draw_cursor(
            video_info,
            &white_bg[0],
            mui->cursor_size,
            mui->textures.list[MUI_TEXTURE_POINTER],
            pointer.x,
            pointer.y,
            width,
            height);
   }

   menu_display_restore_clear_color();
   menu_display_unset_viewport(video_info->width, video_info->height);
}

/* Compute the positions of the widgets */
static void materialui_layout(materialui_handle_t *mui, bool video_is_threaded)
{
   float scale_factor;
   int new_font_size, new_font_size2;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   /* Mobiles platforms may have very small display metrics
    * coupled to a high resolution, so we should be DPI aware
    * to ensure the entries hitboxes are big enough.
    *
    * On desktops, we just care about readability, with every widget
    * size proportional to the display width. */
   scale_factor         = menu_display_get_dpi(width, height);

   new_header_height    = scale_factor / 3;
   new_font_size        = scale_factor / 9;
   new_font_size2       = scale_factor / 12;

   mui->shadow_height   = scale_factor / 36;
   mui->scrollbar_width = scale_factor / 36;
   mui->tabs_height     = scale_factor / 3;
   mui->line_height     = scale_factor / 3;
   mui->margin          = scale_factor / 9;
   mui->icon_size       = scale_factor / 3;

   /* we assume the average glyph aspect ratio is close to 3:4 */
   mui->glyph_width     = new_font_size  * 3/4;
   mui->glyph_width2    = new_font_size2 * 3/4;

   menu_display_set_header_height(new_header_height);

   mui->font            = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         new_font_size,
         video_is_threaded);

   mui->font2           = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         new_font_size2,
         video_is_threaded);

   if (mui->font) /* calculate a more realistic ticker_limit */
   {
      unsigned m_width =
         font_driver_get_message_width(mui->font, "a", 1, 1);

      if (m_width)
         mui->glyph_width = m_width;
   }

   if (mui->font2) /* calculate a more realistic ticker_limit */
   {
      unsigned m_width2 =
         font_driver_get_message_width(mui->font2, "t", 1, 1);

      if (m_width2)
         mui->glyph_width2 = m_width2;
   }
}

static void *materialui_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   float scale_factor         = 0.0f;
   materialui_handle_t   *mui = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   video_driver_get_size(&width, &height);
   scale_factor = menu_display_get_dpi(width, height);

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   mui = (materialui_handle_t*)calloc(1, sizeof(materialui_handle_t));

   if (!mui)
      goto error;

   *userdata         = mui;
   mui->cursor_size  = scale_factor / 3;
   mui->need_compute = false;
   mui->need_scroll  = false;

   mui->menu_title[0] = '\0';

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void materialui_free(void *data)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return;

   video_coord_array_free(&mui->raster_block.carr);
   video_coord_array_free(&mui->raster_block2.carr);

   font_driver_bind_block(NULL, NULL);
}

static void materialui_context_bg_destroy(materialui_handle_t *mui)
{
   if (!mui)
      return;

   video_driver_texture_unload(&mui->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
}

static void materialui_context_destroy(void *data)
{
   unsigned i;
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return;

   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      video_driver_texture_unload(&mui->textures.list[i]);

   menu_display_font_free(mui->font);
   menu_display_font_free(mui->font2);

   materialui_context_bg_destroy(mui);
}

/* Upload textures to the gpu */
static bool materialui_load_image(void *userdata, void *data, enum menu_image_type type)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         materialui_context_bg_destroy(mui);
         video_driver_texture_unload(&mui->textures.bg);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR, &mui->textures.bg);
         menu_display_allocate_white_texture();
         break;
      case MENU_IMAGE_THUMBNAIL:
      case MENU_IMAGE_LEFT_THUMBNAIL:
      case MENU_IMAGE_SAVESTATE_THUMBNAIL:
         break;
   }

   return true;
}

/* The navigation pointer has been updated (for example by pressing up or down
   on the keyboard). We use this function to animate the scroll. */
static void materialui_navigation_set(void *data, bool scroll)
{
   menu_animation_ctx_entry_t entry;
   materialui_handle_t *mui    = (materialui_handle_t*)data;
   float     scroll_pos = mui ? materialui_get_scroll(mui) : 0.0f;

   if (!mui || !scroll)
      return;

   /* mui->scroll_y will be modified by the animation
    * - Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

   entry.duration     = 166;
   entry.target_value = scroll_pos;
   entry.subject      = &mui->scroll_y;
   entry.easing_enum  = EASING_IN_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);
}

static void materialui_list_set_selection(void *data, file_list_t *list)
{
   /* This is called upon MENU_ACTION_CANCEL
    * Have to set 'scroll' to false, otherwise
    * navigating backwards in the menu is absolutely
    * horrendous... */
   materialui_navigation_set(data, false);
}

/* The navigation pointer is set back to zero */
static void materialui_navigation_clear(void *data, bool pending_push)
{
   size_t i             = 0;
   materialui_handle_t *mui    = (materialui_handle_t*)data;
   if (!mui)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   mui->scroll_y = 0;
   menu_input_set_pointer_y_accel(0.0f);
}

static void materialui_navigation_set_last(void *data)
{
   materialui_navigation_set(data, true);
}

static void materialui_navigation_alphabet(void *data, size_t *unused)
{
   materialui_navigation_set(data, true);
}

/* A new list had been pushed. We update the scroll value */
static void materialui_populate_entries(
      void *data, const char *path,
      const char *label, unsigned i)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;

   if (!mui)
      return;

   menu_entries_get_title(mui->menu_title, sizeof(mui->menu_title));
   mui->need_compute = true;

   /* Note: mui->scroll_y position needs to be set here,
    * but we can't do this until materialui_compute_entries_box()
    * has been called. We therefore delegate it until mui->need_compute
    * is acted upon */
}

/* Context reset is called on launch or when a core is launched */
static void materialui_context_reset(void *data, bool is_threaded)
{
   materialui_handle_t *mui              = (materialui_handle_t*)data;
   settings_t *settings           = config_get_ptr();

   if (!mui || !settings)
      return;

   materialui_layout(mui, is_threaded);
   materialui_context_bg_destroy(mui);
   menu_display_allocate_white_texture();
   materialui_context_reset_textures(mui);

   if (path_is_valid(settings->paths.path_menu_wallpaper))
      task_push_image_load(settings->paths.path_menu_wallpaper,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);
   video_driver_monitor_reset();
}

static int materialui_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   materialui_handle_t *mui              = (materialui_handle_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!mui)
            return -1;
         mui->mouse_show = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!mui)
            return -1;
         mui->mouse_show = false;
         break;
      case 0:
      default:
         break;
   }

   return -1;
}

/* Called before we push the new list after clicking on a tab */
static void materialui_preswitch_tabs(materialui_handle_t *mui, unsigned action)
{
   size_t stack_size       = 0;
   file_list_t *menu_stack = NULL;

   if (!mui)
      return;

   menu_stack = menu_entries_get_menu_stack_ptr(0);
   stack_size = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   switch (mui->categories_selection_ptr)
   {
      case MUI_SYSTEM_TAB_MAIN:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
         menu_stack->list[stack_size - 1].type =
            MENU_SETTINGS;
         break;
      case MUI_SYSTEM_TAB_PLAYLISTS:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
         menu_stack->list[stack_size - 1].type =
            MENU_PLAYLISTS_TAB;
         break;
      case MUI_SYSTEM_TAB_SETTINGS:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
         menu_stack->list[stack_size - 1].type =
            MENU_SETTINGS;
         break;
   }
}

/* This callback is not caching anything. We use it to navigate the tabs
   with the keyboard */
static void materialui_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t list_size;
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return;

   mui->need_compute = true;
   list_size = MUI_SYSTEM_TAB_END;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         mui->categories_selection_ptr_old = mui->categories_selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (mui->categories_selection_ptr == 0)
               {
                  mui->categories_selection_ptr = list_size;
                  mui->categories_active_idx    = (unsigned)(list_size - 1);
               }
               else
                  mui->categories_selection_ptr--;
               break;
            default:
               if (mui->categories_selection_ptr == list_size)
               {
                  mui->categories_selection_ptr = 0;
                  mui->categories_active_idx = 1;
               }
               else
                  mui->categories_selection_ptr++;
               break;
         }

         materialui_preswitch_tabs(mui, action);
         break;
      default:
         break;
   }
}

/* A new list has been pushed. We use this callback to customize a few lists for
   this menu driver */
static int materialui_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;
   const struct retro_subsystem_info* subsystem;

   (void)userdata;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
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

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                  MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                  MENU_SETTING_ACTION, 0, 0);

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
                     entry.enum_idx      = MENU_ENUM_LABEL_CORE_LIST;
                     menu_displaylist_setting(&entry);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);

               /* Core fully loaded, use the subsystem data */
               if (system->subsystem.data)
                     subsystem = system->subsystem.data;
               /* Core not loaded completely, use the data we peeked on load core */
               else
                  subsystem = subsystem_data;

               menu_subsystem_populate(subsystem, info);
            }

            if (settings->bools.menu_content_show_history)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_load_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_DISC;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_DUMP_DISC;
               menu_displaylist_setting(&entry);
            }

#if defined(HAVE_NETWORKING)
#ifdef HAVE_LAKKA
            entry.enum_idx      = MENU_ENUM_LABEL_UPDATE_LAKKA;
            menu_displaylist_setting(&entry);
#else
            {
               if (settings->bools.menu_show_online_updater)
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
                  menu_displaylist_setting(&entry);
               }
            }
#endif

            if (settings->bools.menu_content_show_netplay)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_NETPLAY;
               menu_displaylist_setting(&entry);
            }
#endif
            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_configurations)
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

            if (settings->bools.menu_show_restart_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
               menu_displaylist_setting(&entry);
            }

            entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
            menu_displaylist_setting(&entry);
#endif
#if defined(HAVE_LAKKA)
            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_shutdown)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
               menu_displaylist_setting(&entry);
            }
#endif
            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

/* Returns the active tab id */
static size_t materialui_list_get_selection(void *data)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return 0;

   return mui->categories_selection_ptr;
}

/* The pointer or the mouse is pressed down. We use this callback to
   highlight the entry that has been pressed */
static int materialui_pointer_down(void *userdata,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width, height;
   unsigned header_height;
   size_t entries_end         = menu_entries_get_size();
   materialui_handle_t *mui          = (materialui_handle_t*)userdata;

   if (!mui)
      return 0;

   header_height = menu_display_get_header_height();
   video_driver_get_size(&width, &height);

   if (y < header_height)
   {

   }
   else if (y > height - mui->tabs_height)
   {

   }
   else if (ptr <= (entries_end - 1))
   {
      size_t ii;
      file_list_t *list  = menu_entries_get_selection_buf_ptr(0);

      for (ii = 0; ii < entries_end; ii++)
      {
         materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, ii);

         if (y > (-mui->scroll_y + header_height + node->y)
               && y < (-mui->scroll_y + header_height + node->y + node->line_height)
            )
            menu_navigation_set_selection(ii);
      }

   }

   return 0;
}

/* The pointer or the left mouse button has been released.
   If we clicked on the header, we perform a cancel action.
   If we clicked on the tabs, we switch to a new list.
   If we clicked on a menu entry, we call the entry action callback. */
static int materialui_pointer_up(void *userdata,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width, height;
   unsigned header_height, i;
   size_t entries_end         = menu_entries_get_size();
   materialui_handle_t *mui          = (materialui_handle_t*)userdata;

   if (!mui)
      return 0;

   header_height = menu_display_get_header_height();
   video_driver_get_size(&width, &height);

   if (y < header_height)
   {
      size_t selection = menu_navigation_get_selection();
      return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
   }
   else if (y > height - mui->tabs_height)
   {
      file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

      for (i = 0; i <= MUI_SYSTEM_TAB_END; i++)
      {
         unsigned tab_width = width / (MUI_SYSTEM_TAB_END + 1);
         unsigned start = tab_width * i;

         if ((x >= start) && (x < (start + tab_width)))
         {
            mui->categories_selection_ptr = i;

            materialui_preswitch_tabs(mui, action);

            if (cbs && cbs->action_content_list_switch)
               return cbs->action_content_list_switch(selection_buf, menu_stack,
                     "", "", 0);
         }
      }
   }
   else if (ptr <= (entries_end - 1))
   {
      size_t ii;
      file_list_t *list  = menu_entries_get_selection_buf_ptr(0);

      for (ii = 0; ii < entries_end; ii++)
      {
         materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, ii);

         if (y > (-mui->scroll_y + header_height + node->y)
               && y < (-mui->scroll_y + header_height + node->y + node->line_height)
            )
         {
            if (ptr == ii && cbs && cbs->action_select)
               return menu_entry_action(entry, (unsigned)ii, MENU_ACTION_SELECT);
         }
      }
   }

   return 0;
}

/* The menu system can insert menu entries on the fly.
 * It is used in the shaders UI, the wifi UI,
 * the netplay lobby, etc.
 *
 * This function allocates the materialui_node_t
 *for the new entry. */
static void materialui_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   float scale_factor;
   unsigned width, height;
   int i                         = (int)list_size;
   materialui_node_t *node       = NULL;
   settings_t *settings          = config_get_ptr();
   materialui_handle_t *mui      = (materialui_handle_t*)userdata;

   if (!mui || !list)
      return;

   mui->need_compute = true;
   node = (materialui_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = (materialui_node_t*)calloc(1, sizeof(materialui_node_t));

   if (!node)
   {
      RARCH_ERR("GLUI node could not be allocated.\n");
      return;
   }

   video_driver_get_size(&width, &height);
   scale_factor                = menu_display_get_dpi(width, height);

   node->line_height           = scale_factor / 3;
   node->y                     = 0;
   node->texture_switch_set    = false;
   node->texture_switch2_set   = false;
   node->texture_switch_index  = 0;
   node->texture_switch2_index = 0;
   node->switch_is_on          = false;
   node->do_draw_text          = false;

   if (settings->bools.menu_materialui_icons_enable)
   {
      switch (type)
      {
         case MENU_SET_CDROM_INFO:
         case MENU_SET_CDROM_LIST:
         case MENU_SET_LOAD_CDROM_LIST:
            node->texture_switch2_index = MUI_TEXTURE_DISK;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CORE:
            node->texture_switch2_index = MUI_TEXTURE_CORES;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT:
            node->texture_switch2_index = MUI_TEXTURE_IMAGE;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            node->texture_switch2_index = MUI_TEXTURE_PARENT_DIRECTORY;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            node->texture_switch2_index = MUI_TEXTURE_PLAYLIST;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_RDB:
            node->texture_switch2_index = MUI_TEXTURE_DATABASE;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_RDB_ENTRY:
            node->texture_switch2_index = MUI_TEXTURE_SETTINGS;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
            node->texture_switch2_index = MUI_TEXTURE_FILE;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_MUSIC:
            node->texture_switch2_index = MUI_TEXTURE_MUSIC;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_MOVIE:
            node->texture_switch2_index = MUI_TEXTURE_VIDEO;
            node->texture_switch2_set   = true;
            break;
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_DOWNLOAD_URL:
            node->texture_switch2_index = MUI_TEXTURE_FOLDER;
            node->texture_switch2_set = true;
            break;
         default:
            if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION_LIST))              ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_INFORMATION))            ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS))                      ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE))     ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION))                   ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND))             ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_PRESETS_FOUND))
               )
            {
               node->texture_switch2_index = MUI_TEXTURE_INFO;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_DATABASE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES)))
            {
               node->texture_switch2_index = MUI_TEXTURE_IMAGE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC)))
            {
               node->texture_switch2_index = MUI_TEXTURE_MUSIC;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO)))
            {
               node->texture_switch2_index = MUI_TEXTURE_VIDEO;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY)))
            {
               node->texture_switch2_index = MUI_TEXTURE_SCAN;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)))
            {
               node->texture_switch2_index = MUI_TEXTURE_HISTORY;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIST)))
            {
               node->texture_switch2_index = MUI_TEXTURE_HELP;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT)))
            {
               node->texture_switch2_index = MUI_TEXTURE_RESTART;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT)))
            {
               node->texture_switch2_index = MUI_TEXTURE_RESUME;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CLOSE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CORE_OPTIONS;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CORE_CHEAT_OPTIONS;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CONTROLS;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS)))
            {
               node->texture_switch2_index = MUI_TEXTURE_SHADERS;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CORES;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN)))
            {
               node->texture_switch2_index = MUI_TEXTURE_RUN;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_ADD_TO_FAVORITES;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RENAME_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES)))
            {
               node->texture_switch2_index = MUI_TEXTURE_RENAME;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_ADD_TO_MIXER;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_START_CORE))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_START_CORE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_LOAD_STATE;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_EJECT;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DUMP_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISC_INFORMATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_DISK;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME)))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_SAVE_STATE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE)))
            {
               node->texture_switch2_index = MUI_TEXTURE_UNDO_LOAD_STATE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE)))
            {
               node->texture_switch2_index = MUI_TEXTURE_UNDO_SAVE_STATE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_STATE_SLOT)))
            {
               node->texture_switch2_index = MUI_TEXTURE_STATE_SLOT;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT)))
            {
               node->texture_switch2_index = MUI_TEXTURE_TAKE_SCREENSHOT;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS_LIST)))
            {
               node->texture_switch2_index = MUI_TEXTURE_CONFIGURATIONS;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_LOAD_CONTENT;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY)))
            {
               node->texture_switch2_index = MUI_TEXTURE_REMOVE;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_NETPLAY;
               node->texture_switch2_set   = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS)))
            {
               node->texture_switch2_index = MUI_TEXTURE_QUICKMENU;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_ASSETS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CHEATS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_DATABASES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_OVERLAYS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CG_SHADERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS))
                  )
                  {
                     node->texture_switch2_index = MUI_TEXTURE_UPDATER;
                     node->texture_switch2_set = true;
                  }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_ADD;
               node->texture_switch2_set = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUIT_RETROARCH)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_RETROARCH))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_QUIT;
               node->texture_switch2_set   = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DRIVER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_SOUNDS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LATENCY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AI_SERVICE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_TWITCH)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_WIFI_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LAKKA_SERVICES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_USER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MIDI_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS)) ||
#ifdef HAVE_VIDEO_LAYOUT
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS)) ||
#endif
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS))        ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SET_CORE_ASSOCIATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS))
                  )
                  {
                     node->texture_switch2_index = MUI_TEXTURE_SETTINGS;
                     node->texture_switch2_set   = true;
                  }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST))
                  )
            {
               node->texture_switch2_index = MUI_TEXTURE_FOLDER;
               node->texture_switch2_set   = true;
            }
            else if (strcasestr(label, "_input_binds_list"))
            {
               unsigned i;

               for (i = 0; i < MAX_USERS; i++)
               {
                  char val[255];
                  unsigned user_value = i + 1;

                  snprintf(val, sizeof(val), "%d_input_binds_list", user_value);

                  if (string_is_equal(label, val))
                  {
                     node->texture_switch2_index = MUI_TEXTURE_SETTINGS;
                     node->texture_switch2_set   = true;
                  }
               }
            }
            break;
      }
   }

   file_list_set_userdata(list, i, node);
}

/* Clearing the current menu list */
static void materialui_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list ? list->size : 0;

   for (i = 0; i < size; ++i)
   {
      menu_animation_ctx_subject_t subject;
      float *subjects[2];
      materialui_node_t *node = (materialui_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      subjects[0] = &node->line_height;
      subjects[1] = &node->y;

      subject.count = 2;
      subject.data  = subjects;

      menu_animation_kill_by_subject(&subject);

      file_list_free_userdata(list, i);
   }
}

menu_ctx_driver_t menu_ctx_mui = {
   NULL,
   materialui_get_message,
   generic_menu_iterate,
   materialui_render,
   materialui_frame,
   materialui_init,
   materialui_free,
   materialui_context_reset,
   materialui_context_destroy,
   materialui_populate_entries,
   NULL,
   materialui_navigation_clear,
   NULL,
   NULL,
   materialui_navigation_set,
   materialui_navigation_set_last,
   materialui_navigation_alphabet,
   materialui_navigation_alphabet,
   generic_menu_init_list,
   materialui_list_insert,
   NULL,
   NULL,
   materialui_list_clear,
   materialui_list_cache,
   materialui_list_push,
   materialui_list_get_selection,
   materialui_list_get_size,
   NULL,
   materialui_list_set_selection,
   NULL,
   materialui_load_image,
   "glui",
   materialui_environ,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   menu_display_osk_ptr_at_pos,
   NULL, /* update_savestate_thumbnail_path */
   NULL, /* update_savestate_thumbnail_image */
   materialui_pointer_down,
   materialui_pointer_up,
   NULL /* get_load_content_animation_data */
};
