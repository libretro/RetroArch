/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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
               COLOR_TEXT_ALPHA(switch_is_on ? ozone->theme->text_selected_rgba : ozone->theme->text_sublabel_rgba, alpha_uint32), false);
   }
}

void ozone_draw_entries(ozone_handle_t *ozone, video_frame_info_t *video_info,
   unsigned selection, unsigned selection_old,
   file_list_t *selection_buf, float alpha, float scroll_y,
   bool is_playlist)
{
   bool old_list;
   uint32_t alpha_uint32;
   size_t i, y, entries_end;
   float sidebar_offset, bottom_boundary, invert, alpha_anim;
   unsigned video_info_height, video_info_width, entry_width, button_height;
   int x_offset            = 22;
   size_t selection_y      = 0;
   size_t old_selection_y  = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end    = file_list_get_size(selection_buf);
   old_list       = selection_buf == ozone->selection_buf_old;
   y              = ENTRIES_START_Y;
   sidebar_offset = ozone->sidebar_offset / 2.0f;
   entry_width    = video_info->width - 548;
   button_height  = 52; /* height of the button (entry minus sublabel) */

   video_driver_get_size(&video_info_width, &video_info_height);

   bottom_boundary = video_info_height - 87 - 78;
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
      ozone_node_t *node      = NULL;
      if (entry_selected)
         selection_y = y;
      
      if (entry_old_selected)
         old_selection_y = y;

      node                    = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node || ozone->empty_playlist)
         goto border_iterate;

      if (y + scroll_y + node->height + 20 < ENTRIES_START_Y)
         goto border_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto border_iterate;

      ozone_color_alpha(ozone->theme_dynamic.entries_border, alpha);
      ozone_color_alpha(ozone->theme_dynamic.entries_checkmark, alpha);

      /* Borders */
      menu_display_draw_quad(video_info, x_offset + 456-3, y - 3 + scroll_y, entry_width + 10 - 3 -1, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);
      menu_display_draw_quad(video_info, x_offset + 456-3, y - 3 + button_height + scroll_y, entry_width + 10 - 3-1, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);

border_iterate:
      y += node->height;
   }

   /* Cursor(s) layer - current */
   if (!ozone->cursor_in_sidebar)
      ozone_draw_cursor(ozone, video_info, x_offset + 456, entry_width, button_height, selection_y + scroll_y, ozone->animations.cursor_alpha * alpha);

   /* Old*/
   if (!ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(ozone, video_info, x_offset + 456, entry_width, button_height, old_selection_y + scroll_y, (1-ozone->animations.cursor_alpha) * alpha);

   /* Icons + text */
   y = ENTRIES_START_Y;

   if (old_list)
      y += ozone->old_list_offset_y;

   for (i = 0; i < entries_end; i++)
   {
      menu_texture_item tex;
      menu_entry_t entry;
      menu_animation_ctx_ticker_t ticker;
      char entry_value[255];
      char rich_label[255];
      char entry_value_ticker[255];
      char *sublabel_str;
      ozone_node_t *node      = NULL;
      char *entry_rich_label  = NULL;
      bool entry_selected     = false;
      int text_offset         = -40;
      float *icon_color       = NULL;

      entry_value[0]         = '\0';
      entry_selected         = selection == i;
      node                   = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, selection_buf, true);
      menu_entry_get_value(&entry, entry_value, sizeof(entry_value));

      if (!node)
         continue;

      if (y + scroll_y + node->height + 20 < ENTRIES_START_Y)
         goto icons_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto icons_iterate;

      /* Prepare text */
      entry_rich_label = menu_entry_get_rich_label(&entry);

      ticker.idx = ozone->frame_count / 20;
      ticker.s = rich_label;
      ticker.str = entry_rich_label;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len = (entry_width - 60 - text_offset) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);

      if (ozone->empty_playlist)
      {
         unsigned text_width = font_driver_get_message_width(ozone->fonts.entries_label, rich_label, (unsigned)strlen(rich_label), 1);
         x_offset = (video_info_width - 408 - 162)/2 - text_width/2;
         y = video_info_height/2 - 60;
      }

      sublabel_str = menu_entry_get_sublabel(&entry);

      if (node->wrap && sublabel_str)
         word_wrap(sublabel_str, sublabel_str, (video_info->width - 548) / ozone->sublabel_font_glyph_width, false);

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

         ozone_color_alpha(icon_color, alpha);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, 46, 46, texture, x_offset + 451+5+10, y + scroll_y, video_info->width, video_info->height, 0, 1, icon_color);
         menu_display_blend_end(video_info);

         if (icon_color == ozone_pure_white)
            ozone_color_alpha(icon_color, 1.0f);

         text_offset = 0;
      }

      /* Draw text */
      ozone_draw_text(video_info, ozone, rich_label, text_offset + x_offset + 521, y + FONT_SIZE_ENTRIES_LABEL + 8 - 1 + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_label, COLOR_TEXT_ALPHA(ozone->theme->text_rgba, alpha_uint32), false);
      if (sublabel_str)
         ozone_draw_text(video_info, ozone, sublabel_str, x_offset + 470, y + FONT_SIZE_ENTRIES_SUBLABEL + 80 - 20 - 3 + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_sublabel, COLOR_TEXT_ALPHA(ozone->theme->text_sublabel_rgba, alpha_uint32), false);

      /* Value */
      ticker.idx = ozone->frame_count / 20;
      ticker.s = entry_value_ticker;
      ticker.str = entry_value;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len = (entry_width - 60 - ((int)utf8len(entry_rich_label) * ozone->entry_font_glyph_width)) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);
      ozone_draw_entry_value(ozone, video_info, entry_value_ticker, x_offset + 426 + entry_width, y + FONT_SIZE_ENTRIES_LABEL + 8 - 1 + scroll_y,alpha_uint32, &entry);
      
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
