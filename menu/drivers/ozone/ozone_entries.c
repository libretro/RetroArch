/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
 *  Copyright (C) 2019      - Patrick Scheurenbrand
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

#include "ozone.h"
#include "ozone_texture.h"
#include "ozone_display.h"

#include <string/stdstring.h>
#include <encodings/utf.h>

#include "../../menu_driver.h"
#include "../../menu_animation.h"

#include "../../../configuration.h"

static int ozone_get_entries_padding(ozone_handle_t* ozone, bool old_list)
{
   if (ozone->depth == 1)
      if (old_list)
         return ozone->dimensions.entry_padding_horizontal_full;
      else
         return ozone->dimensions.entry_padding_horizontal_half;
   else if (ozone->depth == 2)
      if (old_list && ozone->fade_direction == false) /* false = left to right */
         return ozone->dimensions.entry_padding_horizontal_half;
      else
         return ozone->dimensions.entry_padding_horizontal_full;
   else
      return ozone->dimensions.entry_padding_horizontal_full;
}

static void ozone_draw_entry_value(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      char *value,
      unsigned x, unsigned y,
      uint32_t alpha_uint32,
      menu_entry_t *entry)
{
   bool switch_is_on = true;
   bool do_draw_text = false;

   if (!entry->checked && string_is_empty(value))
      return;

   /* check icon */
   if (entry->checked)
   {
      menu_display_blend_begin(video_info);
      ozone_draw_icon(video_info, 30, 30, ozone->theme->textures[OZONE_THEME_TEXTURE_CHECK], x - 20, y - 22, video_info->width, video_info->height, 0, 1, ozone->theme_dynamic.entries_checkmark);
      menu_display_blend_end(video_info);
      return;
   }

   /* text value */
   if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      switch_is_on = false;
      do_draw_text = false;
   }
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      switch_is_on = true;
      do_draw_text = false;
   }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (
               string_is_equal(entry->value, "...")     ||
               string_is_equal(entry->value, "(PRESET)")  ||
               string_is_equal(entry->value, "(SHADER)")  ||
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
            return;
         }
         else
            do_draw_text = true;
      }
      else
         do_draw_text = true;
   }

   if (do_draw_text)
   {
      ozone_draw_text(video_info, ozone, value, x, y, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.entries_label, COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32), false);
   }
   else
   {
      ozone_draw_text(video_info, ozone, (switch_is_on ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON) : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)),
               x, y, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.entries_label,
               COLOR_TEXT_ALPHA((switch_is_on ? ozone->theme->text_selected_rgba : ozone->theme->text_sublabel_rgba), alpha_uint32), false);
   }
}

/* Compute new scroll position
 * If the center of the currently selected entry is not in the middle
 * And if we can scroll so that it's in the middle
 * Then scroll
 */
void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;
   menu_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   unsigned video_info_height;

   video_driver_get_size(NULL, &video_info_height);

   current_selection_middle_onscreen    =
      ozone->dimensions.header_height +
      ozone->dimensions.entry_padding_vertical +
      ozone->animations.scroll_y +
      node->position_y +
      node->height / 2;

   bottom_boundary                      = video_info_height - ozone->dimensions.header_height - 1 - ozone->dimensions.footer_height;
   entries_middle                       = video_info_height/2;

   new_scroll = ozone->animations.scroll_y - (current_selection_middle_onscreen - entries_middle);

   if (new_scroll + ozone->entries_height < bottom_boundary)
      new_scroll = bottom_boundary - ozone->entries_height - ozone->dimensions.entry_padding_vertical * 2;

   if (new_scroll > 0)
      new_scroll = 0;

   if (allow_animation)
   {
      /* Cursor animation */
      ozone->animations.cursor_alpha = 0.0f;

      entry.cb             = NULL;
      entry.duration       = ANIMATION_CURSOR_DURATION;
      entry.easing_enum    = EASING_OUT_QUAD;
      entry.subject        = &ozone->animations.cursor_alpha;
      entry.tag            = tag;
      entry.target_value   = 1.0f;
      entry.userdata       = NULL;

      menu_animation_push(&entry);

      /* Scroll animation */
      entry.cb             = NULL;
      entry.duration       = ANIMATION_CURSOR_DURATION;
      entry.easing_enum    = EASING_OUT_QUAD;
      entry.subject        = &ozone->animations.scroll_y;
      entry.tag            = tag;
      entry.target_value   = new_scroll;
      entry.userdata       = NULL;

      menu_animation_push(&entry);
   }
   else
   {
      ozone->selection_old = ozone->selection;
      ozone->animations.scroll_y = new_scroll;
   }
}

void ozone_compute_entries_position(ozone_handle_t *ozone)
{
   /* Compute entries height and adjust scrolling if needed */
   unsigned video_info_height;
   unsigned video_info_width;
   unsigned lines;
   size_t i, entries_end;

   file_list_t *selection_buf = NULL;
   int entry_padding          = ozone_get_entries_padding(ozone, false);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end   = menu_entries_get_size();
   selection_buf = menu_entries_get_selection_buf_ptr(0);

   video_driver_get_size(&video_info_width, &video_info_height);

   ozone->entries_height = 0;

   for (i = 0; i < entries_end; i++)
   {
      /* Entry */
      menu_entry_t entry;
      ozone_node_t *node     = NULL;

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      /* Empty playlist detection:
         only one item which icon is
         OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO */
      if (ozone->is_playlist && entries_end == 1)
      {
         menu_texture_item tex = ozone_entries_icon_get_texture(ozone, entry.enum_idx, entry.type, false);
         ozone->empty_playlist = tex == ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      }
      else
      {
         ozone->empty_playlist = false;
      }

      /* Cache node */
      node = (ozone_node_t*)file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      node->height = ozone->dimensions.entry_height + (entry.sublabel ? ozone->dimensions.entry_spacing + 40 : 0);
      node->wrap   = false;

      if (entry.sublabel)
      {
         char *sublabel_str = menu_entry_get_sublabel(&entry);

         int sublabel_max_width = video_info_width -
            entry_padding * 2 - ozone->dimensions.entry_icon_padding * 2;

         if (ozone->depth == 1)
            sublabel_max_width -= (unsigned) ozone->dimensions.sidebar_width;

         if (ozone->show_thumbnail_bar)
            sublabel_max_width -= ozone->dimensions.thumbnail_bar_width;

         word_wrap(sublabel_str, sublabel_str, sublabel_max_width / ozone->sublabel_font_glyph_width, false, 0);

         lines = ozone_count_lines(sublabel_str);

         if (lines > 1)
         {
            node->height += lines * 15;
            node->wrap = true;
         }

         free(sublabel_str);
      }

      node->position_y = ozone->entries_height;

      ozone->entries_height += node->height;

      menu_entry_free(&entry);
   }

   /* Update scrolling */
   ozone->selection = menu_navigation_get_selection();
   ozone_update_scroll(ozone, false, (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, ozone->selection));
}

static void ozone_thumbnail_bar_hide_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->show_thumbnail_bar = false;
}

void ozone_entries_update_thumbnail_bar(ozone_handle_t *ozone, bool is_playlist, bool allow_animation)
{
   struct menu_animation_ctx_entry entry;
   menu_animation_ctx_tag tag = (uintptr_t) &ozone->show_thumbnail_bar;

   entry.duration    = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag         = tag;
   entry.subject     = &ozone->animations.thumbnail_bar_position;

   menu_animation_kill_by_tag(&tag);

   /* Show it */
   if (is_playlist && !ozone->cursor_in_sidebar && !ozone->show_thumbnail_bar && ozone->depth == 1)
   {
      if (allow_animation)
      {
         ozone->show_thumbnail_bar = true;

         entry.cb             = NULL;
         entry.userdata       = NULL;
         entry.target_value   = ozone->dimensions.thumbnail_bar_width;

         menu_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = ozone->dimensions.thumbnail_bar_width;
         ozone->show_thumbnail_bar = true;
      }
   }
   /* Hide it */
   else
   {
      if (allow_animation)
      {
         entry.cb             = ozone_thumbnail_bar_hide_end;
         entry.userdata       = ozone;
         entry.target_value   = 0.0f;

         menu_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = 0.0f;
         ozone_thumbnail_bar_hide_end(ozone);
      }
   }
}

void ozone_draw_entries(ozone_handle_t *ozone, video_frame_info_t *video_info,
   unsigned selection, unsigned selection_old,
   file_list_t *selection_buf, float alpha, float scroll_y,
   bool is_playlist)
{
   uint32_t alpha_uint32;
   size_t i, y, entries_end;
   float sidebar_offset, bottom_boundary, invert, alpha_anim;
   unsigned video_info_height, video_info_width, entry_width, button_height;
   settings_t *settings = config_get_ptr();

   bool old_list           = selection_buf == ozone->selection_buf_old;
   int x_offset            = 0;
   size_t selection_y      = 0; /* 0 means no selection (we assume that no entry has y = 0) */
   size_t old_selection_y  = 0;
   int entry_padding       = ozone_get_entries_padding(ozone, old_list);

   int16_t cursor_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t cursor_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   if (settings->bools.menu_mouse_enable && !ozone->cursor_mode && (cursor_x != ozone->cursor_x_old || cursor_y != ozone->cursor_y_old))
      ozone->cursor_mode = true;
   else if (!settings->bools.menu_mouse_enable)
      ozone->cursor_mode = false; /* we need to disable it on the fly */

   ozone->cursor_x_old = cursor_x;
   ozone->cursor_y_old = cursor_y;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end    = file_list_get_size(selection_buf);
   y              = ozone->dimensions.header_height + 1 + ozone->dimensions.entry_padding_vertical;
   sidebar_offset = ozone->sidebar_offset;
   entry_width    = video_info->width - (unsigned) ozone->dimensions.sidebar_width - ozone->sidebar_offset - entry_padding * 2 - ozone->animations.thumbnail_bar_position;
   button_height  = ozone->dimensions.entry_height; /* height of the button (entry minus sublabel) */

   video_driver_get_size(&video_info_width, &video_info_height);

   bottom_boundary = video_info_height - ozone->dimensions.header_height - ozone->dimensions.footer_height;
   invert          = (ozone->fade_direction) ? -1 : 1;
   alpha_anim      = old_list ? alpha : 1.0f - alpha;

   if (old_list)
      alpha = 1.0f - alpha;

   if (alpha != 1.0f)
   {
      if (old_list)
         x_offset += invert * -(alpha_anim * 120); /* left */
      else
         x_offset += invert * (alpha_anim * 120);  /* right */
   }

   x_offset     += (int) sidebar_offset;
   alpha_uint32  = (uint32_t)(alpha*255.0f);

   /* Borders layer */
   for (i = 0; i < entries_end; i++)
   {
      bool entry_selected     = selection == i;
      bool entry_old_selected = selection_old == i;

      int border_start_x, border_start_y;

      ozone_node_t *node      = NULL;

      if (entry_selected && selection_y == 0)
         selection_y = y;

      if (entry_old_selected && old_selection_y == 0)
         old_selection_y = y;

      node                    = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node || ozone->empty_playlist)
         goto border_iterate;

      if (y + scroll_y + node->height + 20 < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
         goto border_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto border_iterate;

      border_start_x = (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding;
      border_start_y = y + scroll_y;

      menu_display_set_alpha(ozone->theme_dynamic.entries_border, alpha);
      menu_display_set_alpha(ozone->theme_dynamic.entries_checkmark, alpha);

      /* Borders */
      menu_display_draw_quad(video_info, border_start_x,
         border_start_y, entry_width, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);
      menu_display_draw_quad(video_info, border_start_x,
         border_start_y + button_height, entry_width, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);

      /* Cursor */
      if (!old_list && ozone->cursor_mode)
      {
         if (  cursor_x >= border_start_x && cursor_x <= border_start_x + entry_width &&
               cursor_y >= border_start_y && cursor_y <= border_start_y + button_height)
         {
            selection_y = y;
            menu_navigation_set_selection(i);
            menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &i);
         }
      }

border_iterate:
      if (node)
         y += node->height;
   }

   /* Cursor(s) layer - current */
   if (!ozone->cursor_in_sidebar)
      ozone_draw_cursor(ozone, video_info, (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding + 3,
         entry_width - 5, button_height + 2, selection_y + scroll_y + 1, ozone->animations.cursor_alpha * alpha);

   /* Old*/
   if (!ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(ozone, video_info, (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding + 3,
         entry_width - 5, button_height + 2, old_selection_y + scroll_y + 1, (1-ozone->animations.cursor_alpha) * alpha);

   /* Icons + text */
   y = ozone->dimensions.header_height + 1 + ozone->dimensions.entry_padding_vertical;

   if (old_list)
      y += ozone->old_list_offset_y;

   for (i = 0; i < entries_end; i++)
   {
      menu_texture_item tex;
      menu_entry_t entry;
      menu_animation_ctx_ticker_t ticker;
      static const char* const ticker_spacer = OZONE_TICKER_SPACER;
      char entry_value[255];
      char rich_label[255];
      char entry_value_ticker[255];
      char *sublabel_str;
      ozone_node_t *node      = NULL;
      char *entry_rich_label  = NULL;
      bool entry_selected     = false;
      int text_offset         = -ozone->dimensions.entry_icon_padding - ozone->dimensions.entry_icon_size;
      float *icon_color       = NULL;

      /* Initial ticker configuration */
      ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker.spacer = ticker_spacer;

      entry_value[0]         = '\0';
      entry_selected         = selection == i;
      node                   = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, selection_buf, true);
      menu_entry_get_value(&entry, entry_value, sizeof(entry_value));

      if (!node)
         continue;

      if (y + scroll_y + node->height + 20 < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
         goto icons_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto icons_iterate;

      /* Prepare text */
      entry_rich_label = menu_entry_get_rich_label(&entry);

      ticker.idx      = menu_animation_get_ticker_idx();
      ticker.s        = rich_label;
      ticker.str      = entry_rich_label;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len      = (entry_width - entry_padding) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);

      if (ozone->empty_playlist)
      {
         unsigned text_width = font_driver_get_message_width(ozone->fonts.entries_label, rich_label, (unsigned)strlen(rich_label), 1);
         x_offset = (video_info_width - (unsigned) ozone->dimensions.sidebar_width - entry_padding * 2) / 2 - text_width / 2 - 60;
         y = video_info_height / 2 - 60;
      }

      sublabel_str = menu_entry_get_sublabel(&entry);

      if (node->wrap && sublabel_str)
      {
            int sublabel_max_width = video_info_width -
               entry_padding * 2 - ozone->dimensions.entry_icon_padding * 2;

            if (ozone->show_thumbnail_bar)
               sublabel_max_width -= ozone->dimensions.thumbnail_bar_width;

            if (ozone->depth == 1)
               sublabel_max_width -= (unsigned) ozone->dimensions.sidebar_width;

            word_wrap(sublabel_str, sublabel_str, sublabel_max_width / ozone->sublabel_font_glyph_width, false, 0);
      }

      /* Icon */
      tex = ozone_entries_icon_get_texture(ozone, entry.enum_idx, entry.type, entry_selected);
      if (tex != ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING])
      {
         uintptr_t texture = tex;

         /* Console specific icons */
         if (entry.type == FILE_TYPE_RPL_ENTRY && ozone->horizontal_list && ozone->categories_selection_ptr > ozone->system_tab_end)
         {
            ozone_node_t *sidebar_node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

            if (!sidebar_node || !sidebar_node->content_icon)
               texture = tex;
            else
               texture = sidebar_node->content_icon;
         }

         /* Cheevos badges should not be recolored */
         if (!(
            (entry.type >= MENU_SETTINGS_CHEEVOS_START) &&
            (entry.type < MENU_SETTINGS_NETPLAY_ROOMS_START)
         ))
         {
            icon_color = ozone->theme_dynamic.entries_icon;
         }
         else
         {
            icon_color = ozone_pure_white;
         }

         menu_display_set_alpha(icon_color, alpha);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, ozone->dimensions.entry_icon_size, ozone->dimensions.entry_icon_size, texture,
            (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding + ozone->dimensions.entry_icon_padding,
            y + scroll_y + ozone->dimensions.entry_height / 2 - ozone->dimensions.entry_icon_size / 2, video_info->width, video_info->height, 0, 1, icon_color);
         menu_display_blend_end(video_info);

         if (icon_color == ozone_pure_white)
            menu_display_set_alpha(icon_color, 1.0f);

         text_offset = 0;
      }

      /* Draw text */
      ozone_draw_text(video_info, ozone, rich_label, text_offset + (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding + ozone->dimensions.entry_icon_size + ozone->dimensions.entry_icon_padding * 2,
         y + ozone->dimensions.entry_height / 2 + FONT_SIZE_ENTRIES_LABEL * 3/8 + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_label, COLOR_TEXT_ALPHA(ozone->theme->text_rgba, alpha_uint32), false);
      if (sublabel_str)
         ozone_draw_text(video_info, ozone, sublabel_str, (unsigned) ozone->dimensions.sidebar_width + x_offset + entry_padding + ozone->dimensions.entry_icon_padding,
            y + ozone->dimensions.entry_height + 1 + 5 + FONT_SIZE_ENTRIES_SUBLABEL + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_sublabel, COLOR_TEXT_ALPHA(ozone->theme->text_sublabel_rgba, alpha_uint32), false);

      /* Value */
      ticker.idx      = menu_animation_get_ticker_idx();
      ticker.s        = entry_value_ticker;
      ticker.str      = entry_value;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len      = (entry_width - ozone->dimensions.entry_icon_size - ozone->dimensions.entry_icon_padding * 2 -
         ((int)utf8len(entry_rich_label) * ozone->entry_font_glyph_width)) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);

      ozone_draw_entry_value(ozone, video_info, entry_value_ticker, (unsigned) ozone->dimensions.sidebar_width + entry_padding + x_offset + entry_width - ozone->dimensions.entry_icon_padding,
         y + ozone->dimensions.entry_height / 2 + FONT_SIZE_ENTRIES_LABEL * 3/8 + scroll_y, alpha_uint32, &entry);

      free(entry_rich_label);

      if (sublabel_str)
         free(sublabel_str);

icons_iterate:
      y += node->height;
      menu_entry_free(&entry);
   }

   /* Text layer */
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_label, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_sublabel, video_info);
}

static void ozone_draw_no_thumbnail_available(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      unsigned x_position,
      unsigned sidebar_width,
      unsigned y_offset)
{
   unsigned icon        = OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
   unsigned icon_size   = (unsigned)((float)ozone->dimensions.sidebar_entry_icon_size * 1.5f);
   unsigned text_height = font_driver_get_line_height(ozone->fonts.footer, 1.0f);

   menu_display_blend_begin(video_info);
   ozone_draw_icon(video_info,
      icon_size,
      icon_size,
      ozone->icons_textures[icon],
      x_position + sidebar_width/2 - icon_size/2,
      video_info->height / 2 - icon_size/2 - y_offset,
      video_info->width,
      video_info->height,
      0, 1, ozone->theme->entries_icon);
   menu_display_blend_end(video_info);

   ozone_draw_text(video_info,
      ozone,
      msg_hash_to_str(MSG_NO_THUMBNAIL_AVAILABLE),
      x_position + sidebar_width/2,
      video_info->height / 2 - icon_size/2 + text_height * 3 + FONT_SIZE_FOOTER - y_offset,
      TEXT_ALIGN_CENTER,
      video_info->width, video_info->height,
      ozone->fonts.footer,
      ozone->theme->text_rgba,
      true
   );
}

static void ozone_content_metadata_line(video_frame_info_t *video_info, ozone_handle_t *ozone,
   unsigned *y, unsigned column_x,
   const char *text, unsigned lines_count)
{
   ozone_draw_text(video_info, ozone,
      text,
      column_x,
      *y + FONT_SIZE_FOOTER,
      TEXT_ALIGN_LEFT,
      video_info->width, video_info->height,
      ozone->fonts.footer,
      ozone->theme->text_rgba,
      true
   );

   *y += (font_driver_get_line_height(ozone->fonts.footer, 1) * 1.5) * lines_count;
}

void ozone_draw_thumbnail_bar(ozone_handle_t *ozone, video_frame_info_t *video_info)
{
   settings_t *settings    = config_get_ptr();
   unsigned sidebar_height = video_info->height - ozone->dimensions.header_height - 55 - ozone->dimensions.footer_height;
   unsigned sidebar_width  = ozone->dimensions.thumbnail_bar_width;
   unsigned x_position     = video_info->width - (unsigned) ozone->animations.thumbnail_bar_position;

   bool thumbnail;
   bool left_thumbnail;

   /* Background */
   if (!video_info->libretro_running)
   {
      menu_display_draw_quad(video_info, x_position, ozone->dimensions.header_height + 1, (unsigned) ozone->animations.thumbnail_bar_position, 55/2, video_info->width, video_info->height, ozone->theme->sidebar_top_gradient);
      menu_display_draw_quad(video_info, x_position, ozone->dimensions.header_height + 1 + 55/2, (unsigned) ozone->animations.thumbnail_bar_position, sidebar_height, video_info->width, video_info->height, ozone->theme->sidebar_background);
      menu_display_draw_quad(video_info, x_position, video_info->height - ozone->dimensions.footer_height - 55/2 - 1, (unsigned) ozone->animations.thumbnail_bar_position, 55/2 + 1, video_info->width, video_info->height, ozone->theme->sidebar_bottom_gradient);
   }

   /* Thumbnails */
   thumbnail = ozone->thumbnail &&
     menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT);
   left_thumbnail = ozone->left_thumbnail &&
      menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT);

   /* If user requested "left" thumbnail instead of content metadata
    * and no thumbnails are available, show a centered message and
    * return immediately */
   if (!thumbnail && !left_thumbnail && settings->uints.menu_left_thumbnails != 0)
   {
      ozone_draw_no_thumbnail_available(ozone, video_info, x_position, sidebar_width, 0);
      return;
   }

   /* Top row : thumbnail or no thumbnail available message */
   if (thumbnail)
   {
      unsigned thumb_x_position = x_position + sidebar_width/2 - ozone->dimensions.thumbnail_width / 2;
      unsigned thumb_y_position = video_info->height / 2 - ozone->dimensions.thumbnail_height / 2;

      if (!string_is_equal(ozone->selection_core_name, "imageviewer"))
         thumb_y_position -= ozone->dimensions.thumbnail_height / 2 + ozone->dimensions.sidebar_entry_icon_padding/2;

      ozone_draw_icon(video_info,
         ozone->dimensions.thumbnail_width,
         ozone->dimensions.thumbnail_height,
         ozone->thumbnail,
         thumb_x_position,
         thumb_y_position,
         video_info->width, video_info->height,
         0, 1,
         ozone_pure_white
      );
   }
   else
   {
      /* If thumbnails are disabled, we don't know the thumbnail
       * height but we still need to move it to leave room for the
       * content metadata panel */
      unsigned height = video_info->height / 4;

      ozone_draw_no_thumbnail_available(ozone, video_info, x_position, sidebar_width,
         height / 2 + ozone->dimensions.sidebar_entry_icon_padding/2);
   }

   /* Bottom row : "left" thumbnail or content metadata */
   if (thumbnail && left_thumbnail)
   {
      unsigned thumb_x_position = x_position + sidebar_width/2 - ozone->dimensions.left_thumbnail_width / 2;
      unsigned thumb_y_position = video_info->height / 2 + ozone->dimensions.sidebar_entry_icon_padding / 2;

      ozone_draw_icon(video_info,
         ozone->dimensions.left_thumbnail_width,
         ozone->dimensions.left_thumbnail_height,
         ozone->left_thumbnail,
         thumb_x_position,
         thumb_y_position,
         video_info->width, video_info->height,
         0, 1,
         ozone_pure_white
      );
   }
   else if (!ozone->selection_core_is_viewer)
   {
      unsigned y                          = video_info->height / 2 + ozone->dimensions.sidebar_entry_icon_padding / 2;
      unsigned content_metadata_padding   = ozone->dimensions.sidebar_entry_icon_padding*3;
      unsigned separator_padding          = ozone->dimensions.sidebar_entry_icon_padding*2;
      unsigned column_x                   = x_position + content_metadata_padding;

      /* Content metadata */
      y += 10;

      /* Separator */
      menu_display_draw_quad(video_info,
         x_position + separator_padding, y,
         sidebar_width - separator_padding*2, 1,
         video_info->width, video_info->height,
         ozone->theme_dynamic.entries_border);

      y += 18;

      /* Core association */
      ozone_content_metadata_line(video_info, ozone,
         &y, column_x,
         ozone->selection_core_name,
         ozone->selection_core_name_lines
      );

      /* Playtime */
      ozone_content_metadata_line(video_info, ozone,
         &y, column_x,
         ozone->selection_playtime,
         1
      );

      /* Last played */
      ozone_content_metadata_line(video_info, ozone,
         &y, column_x,
         ozone->selection_lastplayed,
         2
      );
   }
}


