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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <compat/posix_string.h>
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

#ifndef HAVE_DYNAMIC
#include "../../frontend/frontend_driver.h"
#endif

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_navigation.h"
#include "../menu_display.h"
#include "../menu_event.h"

#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_osk.h"

#include "../../core_info.h"
#include "../../core.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#include "../../file_path_special.h"

typedef struct
{
   float line_height;
   float y;
} mui_node_t;

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
   MUI_TEXTURE_LAST
};

enum
{
   MUI_SYSTEM_TAB_MAIN = 0,
   MUI_SYSTEM_TAB_PLAYLISTS,
   MUI_SYSTEM_TAB_SETTINGS
};

#define MUI_SYSTEM_TAB_END MUI_SYSTEM_TAB_SETTINGS

typedef struct mui_handle
{
   unsigned tabs_height;
   unsigned line_height;
   unsigned shadow_height;
   unsigned scrollbar_width;
   unsigned icon_size;
   unsigned margin;
   unsigned glyph_width;
   unsigned glyph_width2;
   char box_message[1024];
   bool mouse_show;

   struct
   {
      int size;
   } cursor;

   struct
   {
      struct
      {
         float alpha;
      } arrow;

      menu_texture_item bg;
      menu_texture_item list[MUI_TEXTURE_LAST];
   } textures;

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

   font_data_t *font;
   font_data_t *font2;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;
   float scroll_y;
} mui_handle_t;

static void hex32_to_rgba_normalized(uint32_t hex, float* rgba, float alpha)
{
   rgba[0] = rgba[4] = rgba[8]  = rgba[12] = ((hex >> 16) & 0xFF) * (1.0f / 255.0f); /* r */
   rgba[1] = rgba[5] = rgba[9]  = rgba[13] = ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f); /* g */
   rgba[2] = rgba[6] = rgba[10] = rgba[14] = ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f); /* b */
   rgba[3] = rgba[7] = rgba[11] = rgba[15] = alpha;
}

static const char *mui_texture_path(unsigned id)
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
   }

   return NULL;
}

static void mui_context_reset_textures(mui_handle_t *mui)
{
   unsigned i;
   char iconpath[PATH_MAX_LENGTH];

   iconpath[0] = '\0';

   fill_pathname_application_special(iconpath, sizeof(iconpath),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS);

   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      menu_display_reset_textures_list(mui_texture_path(i), iconpath, &mui->textures.list[i]);
}

static void mui_draw_icon(
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

   menu_display_blend_begin();

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = height - y - icon_size;
   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw);
   menu_display_blend_end();
}

static void mui_draw_tab(mui_handle_t *mui,
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
         if (i == mui->categories.selection_ptr)
            tab_color = active_tab_color;
         break;
      case MUI_SYSTEM_TAB_PLAYLISTS:
         tab_icon = MUI_TEXTURE_TAB_PLAYLISTS;
         if (i == mui->categories.selection_ptr)
            tab_color = active_tab_color;
         break;
      case MUI_SYSTEM_TAB_SETTINGS:
         tab_icon = MUI_TEXTURE_TAB_SETTINGS;
         if (i == mui->categories.selection_ptr)
            tab_color = active_tab_color;
         break;
   }

   mui_draw_icon(
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

static void mui_render_keyboard(mui_handle_t *mui,
      video_frame_info_t *video_info,
      const char *grid[], unsigned id)
{
   int ptr_width, ptr_height;
   unsigned i;
   unsigned width    = video_info->width;
   unsigned height   = video_info->height;
   float dark[16]    =  {
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
   };

   float white[16]   =  {
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
      int line_y        = (i / 11)*height/10.0;
      uintptr_t texture = mui->textures.list[MUI_TEXTURE_KEY];

      if (i == id)
         texture = mui->textures.list[MUI_TEXTURE_KEY_HOVER];

      menu_display_blend_begin();

      menu_display_draw_texture(
            width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width,
            height/2.0 + ptr_height*1.5 + line_y,
            ptr_width, ptr_height,
            width, height,
            &white[0],
            texture);

      menu_display_draw_text(mui->font, grid[i],
            width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width + ptr_width/2.0,
            height/2.0 + ptr_height + line_y + mui->font->size / 3,
            width, height, 0xffffffff, TEXT_ALIGN_CENTER, 1.0f,
            false, 0);
   }
}

/* Returns the OSK key at a given position */
static int mui_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height)
{
   unsigned i;
   int ptr_width, ptr_height;
   mui_handle_t *mui = (mui_handle_t*)data;

   if (!mui)
      return -1;

   ptr_width  = width / 11;
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

static void mui_draw_tab_begin(mui_handle_t *mui,
      unsigned width, unsigned height,
      float *tabs_bg_color, float *tabs_separator_color)
{
   float scale_factor = menu_display_get_dpi();

   mui->tabs_height   = scale_factor / 3;

   /* tabs background */
   menu_display_draw_quad(0, height - mui->tabs_height, width,
         mui->tabs_height,
         width, height,
         tabs_bg_color);

   /* tabs separator */
   menu_display_draw_quad(0, height - mui->tabs_height, width,
         1,
         width, height,
         tabs_separator_color);
}

static void mui_draw_tab_end(mui_handle_t *mui,
      unsigned width, unsigned height,
      unsigned header_height,
      float *active_tab_marker_color)
{
   /* active tab marker */
   unsigned tab_width = width / (MUI_SYSTEM_TAB_END+1);

   menu_display_draw_quad(mui->categories.selection_ptr * tab_width,
         height - (header_height/16),
         tab_width,
         header_height/16,
         width, height,
         &active_tab_marker_color[0]);
}

static float mui_content_height(void)
{
   unsigned i;
   file_list_t *list = menu_entries_get_selection_buf_ptr(0);
   float sum         = 0;

   for (i = 0; i < menu_entries_get_end(); i++)
   {
      mui_node_t *node  = (mui_node_t*)
         menu_entries_get_userdata_at_offset(list, i);
      sum              += node->line_height;
   }
   return sum;
}

static void mui_draw_scrollbar(mui_handle_t *mui,
      unsigned width, unsigned height, float *coord_color)
{
   unsigned header_height = menu_display_get_header_height();
   float content_height   = mui_content_height();
   float total_height     = height - header_height - mui->tabs_height;
   float scrollbar_margin = mui->scrollbar_width;
   float scrollbar_height = total_height / (content_height / total_height);
   float y                = total_height * mui->scroll_y / content_height;

   /* apply a margin on the top and bottom of the scrollbar for aestetic */
   scrollbar_height      -= scrollbar_margin * 2;
   y                     += scrollbar_margin;

   if (content_height < total_height)
      return;

   /* if the scrollbar is extremely short, display it as a square */
   if (scrollbar_height <= mui->scrollbar_width)
      scrollbar_height = mui->scrollbar_width;

   menu_display_draw_quad(
         width - mui->scrollbar_width - scrollbar_margin,
         header_height + y,
         mui->scrollbar_width,
         scrollbar_height,
         width, height,
         coord_color);
}

static void mui_get_message(void *data, const char *message)
{
   mui_handle_t *mui   = (mui_handle_t*)data;

   if (!mui || !message || !*message)
      return;

   strlcpy(mui->box_message, message, sizeof(mui->box_message));
}

static void mui_render_messagebox(mui_handle_t *mui,
      video_frame_info_t *video_info,
      const char *message, float *body_bg_color, uint32_t font_color)
{
   unsigned i, y_position;
   int x, y, line_height, longest = 0, longest_width = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = (struct string_list*)
      string_split(message, "\n");

   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   line_height = mui->font->size * 1.2;

   y_position = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position = height / 4;

   x = width  / 2;
   y = y_position - (list->size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int len = utf8len(msg);
      if (len > longest)
      {
         longest = len;
         longest_width = font_driver_get_message_width(mui->font, msg, len, 1);
      }
   }

   menu_display_set_alpha(body_bg_color, 1.0);

   menu_display_draw_quad(         x - longest_width/2.0 - mui->margin*2.0,
         y - line_height/2.0 - mui->margin*2.0,
         longest_width + mui->margin*4.0,
         line_height * list->size + mui->margin*4.0,
         width,
         height,
         &body_bg_color[0]);

   /* print each line */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         menu_display_draw_text(mui->font, msg,
               x - longest_width/2.0,
               y + i * line_height + mui->font->size / 3,
               width, height, font_color, TEXT_ALIGN_LEFT, 1.0f, false, 0);

   }

   if (menu_input_dialog_get_display_kb())
      mui_render_keyboard(mui,
            video_info,
            menu_event_get_osk_grid(), menu_event_get_osk_ptr());

end:
   string_list_free(list);
}

static unsigned mui_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static void mui_compute_entries_box(mui_handle_t* mui, int width)
{
   size_t usable_width = width - (mui->margin * 2);
   file_list_t *list   = menu_entries_get_selection_buf_ptr(0);
   float sum           = 0;
   unsigned i          = 0;

   for (; i < menu_entries_get_end(); i++)
   {
      char sublabel_str[255];
      float scale_factor;
      unsigned lines   = 0;
      mui_node_t *node = (mui_node_t*)
            menu_entries_get_userdata_at_offset(list, i);

      sublabel_str[0]  = '\0';

      if (menu_entry_get_sublabel(i, sublabel_str, sizeof(sublabel_str)))
      {
         word_wrap(sublabel_str, sublabel_str, (int)(usable_width / mui->glyph_width2));
         lines = mui_count_lines(sublabel_str);
      }

      scale_factor       = menu_display_get_dpi();
      node->line_height  = (scale_factor / 3) + (lines * mui->font->size);
      node->y            = sum;
      sum               += node->line_height;
   }
}

static void mui_render(void *data)
{
   menu_animation_ctx_delta_t delta;
   float delta_time;
   unsigned bottom, width, height, header_height;
   size_t i             = 0;
   mui_handle_t *mui    = (mui_handle_t*)data;
   settings_t *settings = config_get_ptr();
   file_list_t *list    = menu_entries_get_selection_buf_ptr(0);

   if (!mui)
      return;

   video_driver_get_size(&width, &height);

   mui_compute_entries_box(mui, width);

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_get_ideal_delta_time(&delta))
      menu_animation_update(delta.ideal);

   menu_display_set_width(width);
   menu_display_set_height(height);
   header_height = menu_display_get_header_height();

   if (settings->menu.pointer.enable)
   {
      int16_t        pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
      float    old_accel_val   = 0.0f;
      float new_accel_val      = 0.0f;

      size_t ii = 0;
      for (ii = 0; ii < menu_entries_get_size(); ii++)
      {
         mui_node_t *node = (mui_node_t*)
               menu_entries_get_userdata_at_offset(list, ii);

         if (pointer_y > (-mui->scroll_y + header_height + node->y)
          && pointer_y < (-mui->scroll_y + header_height + node->y + node->line_height)
         )
         menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &ii);
      }

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_READ, &old_accel_val);

      mui->scroll_y            -= old_accel_val / 60.0;

      new_accel_val = old_accel_val * 0.96;

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_WRITE, &new_accel_val);
   }

   if (settings->menu.mouse.enable)
   {
      int16_t mouse_y          = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      size_t ii = 0;
      for (ii = 0; ii < menu_entries_get_size(); ii++)
      {
         mui_node_t *node = (mui_node_t*)
               menu_entries_get_userdata_at_offset(list, ii);

         if (mouse_y > (-mui->scroll_y + header_height + node->y)
          && mouse_y < (-mui->scroll_y + header_height + node->y + node->line_height)
         )
         menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &ii);
      }
   }

   if (mui->scroll_y < 0)
      mui->scroll_y = 0;

   bottom = mui_content_height() - height + header_height + mui->tabs_height;
   if (mui->scroll_y > bottom)
      mui->scroll_y = bottom;

   if (mui_content_height()
         < height - header_height - mui->tabs_height)
      mui->scroll_y = 0;

   /*if (menu_entries_get_end() < height / mui->line_height) { }
   else
      i = mui->scroll_y / mui->line_height;*/

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
}

static void mui_render_label_value(mui_handle_t *mui, mui_node_t *node,
      int i, int y, unsigned width, unsigned height,
      uint64_t index, uint32_t color, bool selected, const char *label,
      const char *value, float *label_color)
{
   /* This will be used instead of label_color if texture_switch is 'off' icon */
   float pure_white[16]=  {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };

   menu_animation_ctx_ticker_t ticker;
   char label_str[255];
   char sublabel_str[255];
   char value_str[255];
   uint32_t sublabel_color;
   bool switch_is_on               = true;
   int value_len                   = utf8len(value);
   int ticker_limit                = 0;
   uintptr_t texture_switch        = 0;
   bool do_draw_text               = false;
   size_t usable_width             = width - (mui->margin * 2);
   label_str[0] = value_str[0] = sublabel_str[0] = '\0';

#ifdef VITA
   sublabel_color = 0xff888888;
#else
   sublabel_color = 0x888888ff;
#endif

   if (value_len * mui->glyph_width > usable_width / 2)
      value_len = (usable_width/2) / mui->glyph_width;

   ticker_limit = (usable_width / mui->glyph_width) - (value_len + 2);

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

   if (menu_entry_get_sublabel(i, sublabel_str, sizeof(sublabel_str)))
   {
      word_wrap(sublabel_str, sublabel_str, (int)(usable_width / mui->glyph_width2));

      menu_display_draw_text(mui->font2, sublabel_str,
            mui->margin,
            y + (menu_display_get_dpi() / 4) + mui->font->size,
            width, height, sublabel_color, TEXT_ALIGN_LEFT, 1.0f, false, 0);
   }

   menu_display_draw_text(mui->font, label_str,
         mui->margin,
         y + (menu_display_get_dpi() / 5),
         width, height, color, TEXT_ALIGN_LEFT, 1.0f, false, 0);

   if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      if (mui->textures.list[MUI_TEXTURE_SWITCH_OFF])
      {
         texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_OFF];
         switch_is_on = false;
      }
      else
         do_draw_text = true;
   }
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
            (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      if (mui->textures.list[MUI_TEXTURE_SWITCH_ON])
      {
         texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_ON];
         switch_is_on = true;
      }
      else
         do_draw_text = true;
   }
   else
   {
      enum msg_file_type type = msg_hash_to_file_type(msg_hash_calculate(value));

      switch (type)
      {
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
            if (mui->textures.list[MUI_TEXTURE_SWITCH_ON])
            {
               texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_ON];
               switch_is_on = true;
            }
            else
               do_draw_text = true;
            break;
         case FILE_TYPE_BOOL_OFF:
            if (mui->textures.list[MUI_TEXTURE_SWITCH_OFF])
            {
               texture_switch = mui->textures.list[MUI_TEXTURE_SWITCH_OFF];
               switch_is_on = false;
            }
            else
               do_draw_text = true;
            break;
         default:
            do_draw_text = true;
            break;
      }
   }

   if (do_draw_text)
      menu_display_draw_text(mui->font, value_str,
            width - mui->margin,
            y + (menu_display_get_dpi() / 5),
            width, height, color, TEXT_ALIGN_RIGHT, 1.0f, false, 0);

   if (texture_switch)
      mui_draw_icon(
            mui->icon_size,
            texture_switch,
            width - mui->margin - mui->icon_size,
            y + (menu_display_get_dpi() / 6) - mui->icon_size/2,
            width,
            height,
            0,
            1,
            switch_is_on ? &label_color[0] :  &pure_white[0]
      );
}

static void mui_render_menu_list(
      video_frame_info_t *video_info,
      mui_handle_t *mui,
      unsigned width, unsigned height,
      uint32_t font_normal_color,
      uint32_t font_hover_color,
      float *menu_list_color)
{
   float sum               = 0;
   size_t i                = 0;
   file_list_t *list       = NULL;
   uint64_t frame_count    = video_info->frame_count;
   unsigned header_height  = menu_display_get_header_height();

   mui->raster_block.carr.coords.vertices = 0;
   mui->raster_block2.carr.coords.vertices = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   list = menu_entries_get_selection_buf_ptr(0);

   for (; i < menu_entries_get_end(); i++)
   {
      int y;
      size_t selection;
      char rich_label[255];
      char entry_value[255];
      bool entry_selected = false;
      mui_node_t *node    = (mui_node_t*)
            menu_entries_get_userdata_at_offset(list, i);

      rich_label[0] = entry_value[0] = '\0';

      if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
         continue;

      y = header_height - mui->scroll_y + sum;

      /*if ((y - (int)node->line_height) > (int)height
            || ((y + (int)node->line_height) < 0))
         continue;*/

      menu_entry_get_value(i, NULL, entry_value, sizeof(entry_value));
      menu_entry_get_rich_label(i, rich_label, sizeof(rich_label));

      entry_selected = selection == i;

      mui_render_label_value(
         mui,
         node,
         i,
         y,
         width,
         height,
         frame_count / 20,
         font_hover_color,
         entry_selected,
         rich_label,
         entry_value,
         menu_list_color
      );

      sum += node->line_height;
   }
}


static size_t mui_list_get_size(void *data, enum menu_list_type type)
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

static int mui_get_core_title(char *s, size_t len)
{
   struct retro_system_info    *system = NULL;
   rarch_system_info_t      *info = NULL;
   settings_t *settings           = config_get_ptr();
   const char *core_name          = NULL;
   const char *core_version       = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
         &system);

   core_name    = system->library_name;
   core_version = system->library_version;

   if (!settings->menu.core_enable)
      return -1;

   if (runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info))
   {
      if (info)
      {
         if (string_is_empty(core_name))
            core_name = info->info.library_name;
         if (!core_version)
            core_version = info->info.library_version;
      }
   }

   if (string_is_empty(core_name))
      core_name    = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
   if (!core_version)
      core_version = "";

   snprintf(s, len, "%s %s", core_name, core_version);

   return 0;
}

static void mui_draw_bg(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   menu_display_blend_begin();

   draw->x               = 0;
   draw->y               = 0;
   draw->pipeline.id     = 0;
   draw->pipeline.active = false;

   menu_display_draw_bg(draw, video_info, false);
   menu_display_draw(draw);
   menu_display_blend_end();
}

static void mui_frame(void *data, video_frame_info_t *video_info)
{
   float black_bg[16] = {
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
   };
   float pure_white[16]=  {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
      0.98, 0.98, 0.98, 1.00,
   };
   float white_transp_bg[16]=  {
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
   };
   float grey_bg[16]=  {
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
      0.78, 0.78, 0.78, 0.90,
   };
   float shadow_bg[16]=  {
      0.00, 0.00, 0.00, 0.00,
      0.00, 0.00, 0.00, 0.00,
      0.00, 0.00, 0.00, 0.2,
      0.00, 0.00, 0.00, 0.2,
   };
   /* TODO/FIXME - convert this over to new hex format */
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


   /* This controls the main background color */
   menu_display_ctx_clearcolor_t clearcolor;
   menu_animation_ctx_ticker_t ticker;
   menu_display_ctx_draw_t draw;
   char msg[255];
   char title[255];
   char title_buf[255];
   char title_msg[255];

#ifdef VITA
   uint32_t black_opaque_54        = 0x8a000000;
   uint32_t black_opaque_87        = 0xde000000;
   uint32_t white_opaque_70        = 0xb3ffffff;
#else
   uint32_t black_opaque_54        = 0x0000008a;
   uint32_t black_opaque_87        = 0x000000de;
   uint32_t white_opaque_70        = 0xffffffb3;
#endif
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
   file_list_t *list               = NULL;
   mui_node_t *node                = NULL;
   unsigned width                  = video_info->width;
   unsigned height                 = video_info->height;
   unsigned ticker_limit           = 0;
   unsigned i                      = 0;
   unsigned header_height          = 0;
   size_t selection                = 0;
   size_t title_margin             = 0;
   mui_handle_t *mui               = (mui_handle_t*)data;
   uint64_t frame_count            = video_info->frame_count;
   bool background_rendered        = false;
   bool libretro_running           = video_info->libretro_running;

   /* Default is blue theme */
   float *header_bg_color          = NULL;
   float *highlighted_entry_color  = NULL;
   float *footer_bg_color          = NULL;
   float *body_bg_color            = NULL;
   float *active_tab_marker_color  = NULL;
   float *passive_tab_icon_color   = grey_bg;

   uint32_t font_normal_color      = 0;
   uint32_t font_hover_color       = 0;
   uint32_t font_header_color      = 0;

   if (!mui)
      return;

   msg[0] = title[0] = title_buf[0] = title_msg[0] = '\0';

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

         header_bg_color         = header_bg_color_real;
         body_bg_color           = color_nv_body;
         highlighted_entry_color = color_nv_accent;
         footer_bg_color         = footer_bg_color_real;
         active_tab_marker_color = color_nv_accent;

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
      memset(&draw, 0, sizeof(menu_display_ctx_draw_t));

      draw.width              = width;
      draw.height             = height;
      draw.texture            = menu_display_white_texture;
      draw.color              = &body_bg_color[0];
      draw.vertex             = NULL;
      draw.tex_coord          = NULL;
      draw.vertex_count       = 4;
      draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

      mui_draw_bg(&draw, video_info);
   }
   else
   {
      menu_display_clear_color(&clearcolor);

      if (mui->textures.bg)
      {
         background_rendered = true;

         menu_display_set_alpha(white_transp_bg, 0.30);

         memset(&draw, 0, sizeof(menu_display_ctx_draw_t));

         draw.width              = width;
         draw.height             = height;
         draw.texture            = mui->textures.bg;
         draw.color              = &white_transp_bg[0];
         draw.vertex             = NULL;
         draw.tex_coord          = NULL;
         draw.vertex_count       = 4;
         draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

         if (draw.texture)
            draw.color           = &white_bg[0];

         mui_draw_bg(&draw, video_info);

         /* Restore opacity of transposed white background */
         menu_display_set_alpha(white_transp_bg, 0.90);
      }
   }

   menu_entries_get_title(title, sizeof(title));

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (background_rendered || libretro_running)
      menu_display_set_alpha(blue_50, 0.75);
   else
      menu_display_set_alpha(blue_50, 1.0);

   /* highlighted entry */
   list             = menu_entries_get_selection_buf_ptr(0);
   node             = (mui_node_t*)menu_entries_get_userdata_at_offset(
         list, selection);

   menu_display_draw_quad(
      0,
      header_height - mui->scroll_y + node->y,
      width,
      node->line_height,
      width,
      height,
      &highlighted_entry_color[0]
   );

   menu_display_font_bind_block(mui->font, &mui->raster_block);
   menu_display_font_bind_block(mui->font2, &mui->raster_block2);

   if (menu_display_get_update_pending())
      mui_render_menu_list(
            video_info,
            mui,
            width,
            height,
            font_normal_color,
            font_hover_color,
            &active_tab_marker_color[0]
            );

   menu_display_font_flush_block(video_info->width, video_info->height, mui->font);
   menu_display_font_flush_block(video_info->width, video_info->height, mui->font2);
   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);

   /* header */
   menu_display_draw_quad(
      0,
      0,
      width,
      header_height,
      width,
      height,
      &header_bg_color[0]);

   mui->tabs_height = 0;

   /* display tabs if depth equal one, if not hide them */
   if (mui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
   {
      mui_draw_tab_begin(mui, width, height, &footer_bg_color[0], &grey_bg[0]);

      for (i = 0; i <= MUI_SYSTEM_TAB_END; i++)
         mui_draw_tab(mui, i, width, height, &passive_tab_icon_color[0], &active_tab_marker_color[0]);

      mui_draw_tab_end(mui, width, height, header_height, &active_tab_marker_color[0]);
   }

   menu_display_draw_quad(
      0,
      header_height,
      width,
      mui->shadow_height,
      width,
      height,
      &shadow_bg[0]);

   title_margin = mui->margin;

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      title_margin = mui->icon_size;
      mui_draw_icon(
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

   ticker_limit = (width - mui->margin*2) / mui->glyph_width;

   ticker.s        = title_buf;
   ticker.len      = ticker_limit;
   ticker.idx      = frame_count / 100;
   ticker.str      = title;
   ticker.selected = true;

   menu_animation_ticker(&ticker);

   /* Title */
   if (mui_get_core_title(title_msg, sizeof(title_msg)) == 0)
   {
      int ticker_limit, value_len;
      char title_buf_msg_tmp[255];
      char title_buf_msg[255];
      size_t         usable_width = width - (mui->margin * 2);

      title_buf_msg_tmp[0] = title_buf_msg[0] = '\0';

      snprintf(title_buf_msg, sizeof(title_buf), "%s (%s)",
            title_buf, title_msg);
      value_len = utf8len(title_buf);
      ticker_limit = (usable_width / mui->glyph_width) - (value_len + 2);

      ticker.s        = title_buf_msg_tmp;
      ticker.len      = ticker_limit;
      ticker.idx      = frame_count / 20;
      ticker.str      = title_buf_msg;
      ticker.selected = true;

      menu_animation_ticker(&ticker);

      strlcpy(title_buf, title_buf_msg_tmp, sizeof(title_buf));
   }

   menu_display_draw_text(mui->font, title_buf,
         title_margin,
         header_height / 2 + mui->font->size / 3,
         width, height, font_header_color, TEXT_ALIGN_LEFT, 1.0f, false, 0);

   mui_draw_scrollbar(mui, width, height, &grey_bg[0]);

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      menu_display_draw_quad(0, 0, width, height, width, height, &black_bg[0]);
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      mui_render_messagebox(mui, video_info, msg, &body_bg_color[0], font_hover_color);
   }

   if (!string_is_empty(mui->box_message))
   {
      menu_display_draw_quad(0, 0, width, height, width, height, &black_bg[0]);
      mui_render_messagebox(mui, video_info, mui->box_message, &body_bg_color[0], font_hover_color);
      mui->box_message[0] = '\0';
   }

   if (mui->mouse_show)
      menu_display_draw_cursor(
            &white_bg[0],
            mui->cursor.size,
            mui->textures.list[MUI_TEXTURE_POINTER],
            menu_input_mouse_state(MENU_MOUSE_X_AXIS),
            menu_input_mouse_state(MENU_MOUSE_Y_AXIS),
            width,
            height);

   menu_display_restore_clear_color();
   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void mui_layout(mui_handle_t *mui)
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
   scale_factor = menu_display_get_dpi();

   new_header_height    = scale_factor / 3;
   new_font_size        = scale_factor / 9;
   new_font_size2       = scale_factor / 12;

   mui->shadow_height   = scale_factor / 36;
   mui->scrollbar_width = scale_factor / 36;
   mui->tabs_height     = scale_factor / 3;
   mui->line_height     = scale_factor / 3;
   mui->margin          = scale_factor / 9;
   mui->icon_size       = scale_factor / 3;

   menu_display_set_header_height(new_header_height);

   /* we assume the average glyph aspect ratio is close to 3:4 */
   mui->glyph_width = new_font_size * 3/4;
   mui->glyph_width2 = new_font_size2 * 3/4;

   mui->font = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         new_font_size);

   mui->font2 = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         new_font_size2);

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

static void *mui_init(void **userdata)
{
   mui_handle_t   *mui = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   if (!menu_display_init_first_driver())
      goto error;

   mui = (mui_handle_t*)calloc(1, sizeof(mui_handle_t));

   if (!mui)
      goto error;

   *userdata = mui;

   mui->cursor.size  = 64.0;

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void mui_free(void *data)
{
   mui_handle_t *mui   = (mui_handle_t*)data;

   if (!mui)
      return;

   video_coord_array_free(&mui->raster_block.carr);
   video_coord_array_free(&mui->raster_block2.carr);

   font_driver_bind_block(NULL, NULL);
}

static void mui_context_bg_destroy(mui_handle_t *mui)
{
   if (!mui)
      return;

   video_driver_texture_unload(&mui->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
}

static void mui_context_destroy(void *data)
{
   unsigned i;
   mui_handle_t *mui   = (mui_handle_t*)data;

   if (!mui)
      return;

   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      video_driver_texture_unload(&mui->textures.list[i]);

   menu_display_font_free(mui->font);

   mui_context_bg_destroy(mui);
}

static bool mui_load_image(void *userdata, void *data, enum menu_image_type type)
{
   mui_handle_t *mui = (mui_handle_t*)userdata;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         mui_context_bg_destroy(mui);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR, &mui->textures.bg);
         menu_display_allocate_white_texture();
         break;
      case MENU_IMAGE_THUMBNAIL:
      case MENU_IMAGE_SAVESTATE_THUMBNAIL:
         break;
   }

   return true;
}

static float mui_get_scroll(mui_handle_t *mui)
{
   size_t selection;
   unsigned width, height, half = 0;

   if (!mui)
      return 0;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   video_driver_get_size(&width, &height);

   if (mui->line_height)
      half = (height / mui->line_height) / 3;

   if (selection < half)
      return 0;

   return ((selection + 2 - half) * mui->line_height);
}

static void mui_navigation_set(void *data, bool scroll)
{
   menu_animation_ctx_entry_t entry;
   mui_handle_t *mui    = (mui_handle_t*)data;
   float     scroll_pos = mui ? mui_get_scroll(mui) : 0.0f;

   if (!mui || !scroll)
      return;

   entry.duration     = 10;
   entry.target_value = scroll_pos;
   entry.subject      = &mui->scroll_y;
   entry.easing_enum  = EASING_IN_OUT_QUAD;
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);
}

static void  mui_list_set_selection(void *data, file_list_t *list)
{
   mui_navigation_set(data, true);
}

static void mui_navigation_clear(void *data, bool pending_push)
{
   size_t i             = 0;
   mui_handle_t *mui    = (mui_handle_t*)data;
   if (!mui)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   mui->scroll_y = 0;
}

static void mui_navigation_set_last(void *data)
{
   mui_navigation_set(data, true);
}

static void mui_navigation_alphabet(void *data, size_t *unused)
{
   mui_navigation_set(data, true);
}

static void mui_populate_entries(
      void *data, const char *path,
      const char *label, unsigned i)
{
   mui_handle_t *mui    = (mui_handle_t*)data;
   if (!mui)
      return;

   mui->scroll_y = mui_get_scroll(mui);
}

static void mui_context_reset(void *data)
{
   mui_handle_t *mui              = (mui_handle_t*)data;
   settings_t *settings           = config_get_ptr();

   if (!mui || !settings)
      return;

   mui_layout(mui);
   mui_context_bg_destroy(mui);
   menu_display_allocate_white_texture();
   mui_context_reset_textures(mui);

   if (path_file_exists(settings->path.menu_wallpaper))
      task_push_image_load(settings->path.menu_wallpaper, 
            menu_display_handle_wallpaper_upload, NULL);
}

static int mui_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   mui_handle_t *mui              = (mui_handle_t*)userdata;

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

static void mui_preswitch_tabs(mui_handle_t *mui, unsigned action)
{
   size_t idx              = 0;
   size_t stack_size       = 0;
   file_list_t *menu_stack = NULL;

   if (!mui)
      return;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);

   menu_stack = menu_entries_get_menu_stack_ptr(0);
   stack_size = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   switch (mui->categories.selection_ptr)
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

static void mui_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t list_size;
   mui_handle_t *mui   = (mui_handle_t*)data;

   if (!mui)
      return;

   list_size = MUI_SYSTEM_TAB_END;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         mui->categories.selection_ptr_old = mui->categories.selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (mui->categories.selection_ptr == 0)
               {
                  mui->categories.selection_ptr = list_size;
                  mui->categories.active.idx = list_size - 1;
               }
               else
                  mui->categories.selection_ptr--;
               break;
            default:
               if (mui->categories.selection_ptr == list_size)
               {
                  mui->categories.selection_ptr = 0;
                  mui->categories.active.idx = 1;
               }
               else
                  mui->categories.selection_ptr++;
               break;
         }

         mui_preswitch_tabs(mui, action);
         break;
      default:
         break;
   }
}

static int mui_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;

   (void)userdata;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (frontend_driver_parse_drive_list(info->list) != 0)
            menu_entries_append_enum(info->list, "/",
                  msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                  MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                  MENU_SETTING_ACTION, 0, 0);

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

         info->need_push    = true;
         info->need_refresh = true;
         ret = 0;
         break;
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
            entry.enum_idx      = MENU_ENUM_LABEL_CORE_LIST;
            menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
         }

         entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);

         entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY;
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
#if defined(HAVE_LAKKA)
         entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
         menu_displaylist_ctl(DISPLAYLIST_SETTING_ENUM, &entry);
#endif
         info->need_push    = true;
         ret = 0;
         break;
   }
   return ret;
}

static size_t mui_list_get_selection(void *data)
{
   mui_handle_t *mui   = (mui_handle_t*)data;

   if (!mui)
      return 0;

   return mui->categories.selection_ptr;
}

static int mui_pointer_tap(void *userdata,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection;
   unsigned width, height;
   unsigned header_height, i;
   mui_handle_t *mui          = (mui_handle_t*)userdata;

   if (!mui)
      return 0;

   header_height = menu_display_get_header_height();
   video_driver_get_size(&width, &height);

   if (y < header_height)
   {
      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
      return menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
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
            mui->categories.selection_ptr = i;

            mui_preswitch_tabs(mui, action);

            if (cbs && cbs->action_content_list_switch)
               return cbs->action_content_list_switch(selection_buf, menu_stack,
                     "", "", 0);
         }
      }
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      size_t idx;
      bool scroll                = false;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, selection, MENU_ACTION_SELECT);

      idx  = ptr;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }

   return 0;
}

static void mui_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
      size_t list_size)
{
   size_t selection;
   float scale_factor;
   int i                  = list_size;
   mui_node_t *node       = NULL;
   mui_handle_t *mui      = (mui_handle_t*)userdata;

   if (!mui || !list)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   node = (mui_node_t*)menu_entries_get_userdata_at_offset(list, i);

   if (!node)
      node = (mui_node_t*)calloc(1, sizeof(mui_node_t));

   if (!node)
   {
      RARCH_ERR("GLUI node could not be allocated.\n");
      return;
   }

   scale_factor = menu_display_get_dpi();

   node->line_height = scale_factor / 3;
   node->y = 0;

   file_list_set_userdata(list, i, node);
}

static void mui_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list->size;

   for (i = 0; i < size; ++i)
   {
      menu_animation_ctx_subject_t subject;
      float *subjects[2];
      mui_node_t *node = (mui_node_t*)
         menu_entries_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      subjects[0] = &node->line_height;
      subjects[1] = &node->y;

      subject.count = 2;
      subject.data  = subjects;

      menu_animation_ctl(MENU_ANIMATION_CTL_KILL_BY_SUBJECT, &subject);

      file_list_free_userdata(list, i);
   }
}

menu_ctx_driver_t menu_ctx_mui = {
   NULL,
   mui_get_message,
   generic_menu_iterate,
   mui_render,
   mui_frame,
   mui_init,
   mui_free,
   mui_context_reset,
   mui_context_destroy,
   mui_populate_entries,
   NULL,
   mui_navigation_clear,
   NULL,
   NULL,
   mui_navigation_set,
   mui_navigation_set_last,
   mui_navigation_alphabet,
   mui_navigation_alphabet,
   generic_menu_init_list,
   mui_list_insert,
   NULL,
   NULL,
   mui_list_clear,
   mui_list_cache,
   mui_list_push,
   mui_list_get_selection,
   mui_list_get_size,
   NULL,
   mui_list_set_selection,
   NULL,
   mui_load_image,
   "glui",
   mui_environ,
   mui_pointer_tap,
   NULL,
   NULL,
   mui_osk_ptr_at_pos,
   NULL,
   NULL
};
