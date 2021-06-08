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
#include "../../../gfx/gfx_animation.h"

#include "../../../configuration.h"

static int ozone_get_entries_padding(ozone_handle_t* ozone, bool old_list)
{
   if (ozone->depth == 1)
   {
      if (!old_list)
         return ozone->dimensions.entry_padding_horizontal_half;
   }
   else if (ozone->depth == 2)
   {
      if (old_list && !ozone->fade_direction) /* false = left to right */
         return ozone->dimensions.entry_padding_horizontal_half;
   }
   return ozone->dimensions.entry_padding_horizontal_full;
}

static void ozone_draw_entry_value(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      char *value,
      unsigned x, unsigned y,
      uint32_t alpha_uint32,
      menu_entry_t *entry)
{
   bool switch_is_on                 = true;
   bool do_draw_text                 = false;
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* check icon */
   if (entry->checked)
   {
      float *col = ozone->theme_dynamic.entries_checkmark;
      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      ozone_draw_icon(
            p_disp,
            userdata,
            video_width,
            video_height,
            30 * scale_factor,
            30 * scale_factor,
            ozone->theme->textures[OZONE_THEME_TEXTURE_CHECK],
            x - 20 * scale_factor,
            y - 22 * scale_factor,
            video_width,
            video_height,
            0,
            1,
            col);
      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
      return;
   }
   else if (string_is_empty(value))
      return;
      

   /* text value */
   if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
      switch_is_on = false;
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))) { }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (string_is_equal(entry->value, "..."))
            return;
         if (string_starts_with_size(entry->value, "(", STRLEN_CONST("(")) &&
               string_ends_with  (entry->value, ")")
            )
         {
            if (
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
               return;
         }
      }

      do_draw_text = true;
   }

   if (do_draw_text)
   {
      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            value,
            x,
            y,
            video_width,
            video_height,
            COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32),
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);
   }
   else
      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            (switch_is_on ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON) : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)),
            x,
            y,
            video_width,
            video_height,
            COLOR_TEXT_ALPHA((switch_is_on ? ozone->theme->text_selected_rgba : ozone->theme->text_sublabel_rgba), alpha_uint32),
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);
}

static void ozone_thumbnail_bar_hide_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->show_thumbnail_bar = false;
}

static void ozone_draw_no_thumbnail_available(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned x_position,
      unsigned sidebar_width,
      unsigned y_offset)
{
   unsigned icon                     = OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
   unsigned icon_size                = (unsigned)((float)
         ozone->dimensions.sidebar_entry_icon_size * 1.5f);
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float                        *col = ozone->theme->entries_icon;

   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      if (dispctx->draw)
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               ozone->icons_textures[icon],
               x_position + sidebar_width/2 - icon_size/2,
               video_height/2 - icon_size/2 - y_offset,
               video_width,
               video_height,
               0, 1, col);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   gfx_display_draw_text(
         ozone->fonts.footer.font,
         msg_hash_to_str(MSG_NO_THUMBNAIL_AVAILABLE),
         x_position + sidebar_width   / 2,
           video_height / 2 + icon_size / 2 
         + ozone->fonts.footer.line_ascender - y_offset,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_CENTER,
         1.0f,
         false,
         1.0f,
         true);
}

static void ozone_content_metadata_line(
      unsigned video_width,
      unsigned video_height,
      ozone_handle_t *ozone,
      unsigned *y,
      unsigned column_x,
      const char *text,
      uint32_t color,
      unsigned lines_count)
{
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         text,
         column_x,
         *y + ozone->fonts.footer.line_ascender,
         video_width,
         video_height,
         color,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         true);

   if (lines_count > 0)
      *y += (unsigned)(ozone->fonts.footer.line_height * (lines_count - 1)) + (unsigned)((float)ozone->fonts.footer.line_height * 1.5f);
}


/* Compute new scroll position
 * If the center of the currently selected entry is not in the middle
 * And if we can scroll so that it's in the middle
 * Then scroll
 */
void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node)
{
   unsigned video_info_height;
   gfx_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   uintptr_t tag              = (uintptr_t) selection_buf;

   video_driver_get_size(NULL, &video_info_height);

   current_selection_middle_onscreen    =
      ozone->dimensions.header_height +
      ozone->dimensions.entry_padding_vertical +
      ozone->animations.scroll_y +
      node->position_y +
      node->height / 2;

   bottom_boundary                      = video_info_height - ozone->dimensions.header_height - ozone->dimensions.spacer_1px - ozone->dimensions.footer_height;
   entries_middle                       = video_info_height/2;

   new_scroll = ozone->animations.scroll_y - (current_selection_middle_onscreen - entries_middle);

   if (new_scroll + ozone->entries_height < bottom_boundary)
      new_scroll = bottom_boundary - ozone->entries_height - ozone->dimensions.entry_padding_vertical * 2;

   if (new_scroll > 0)
      new_scroll = 0;

   /* Kill any existing scroll animation */
   gfx_animation_kill_by_tag(&tag);

   /* ozone->animations.scroll_y will be modified
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

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

      gfx_animation_push(&entry);

      /* Scroll animation */
      entry.cb             = NULL;
      entry.duration       = ANIMATION_CURSOR_DURATION;
      entry.easing_enum    = EASING_OUT_QUAD;
      entry.subject        = &ozone->animations.scroll_y;
      entry.tag            = tag;
      entry.target_value   = new_scroll;
      entry.userdata       = NULL;

      gfx_animation_push(&entry);
   }
   else
   {
      ozone->selection_old = ozone->selection;
      ozone->animations.scroll_y = new_scroll;
   }
}

void ozone_compute_entries_position(
      ozone_handle_t *ozone,
      settings_t *settings,
      size_t entries_end)
{
   /* Compute entries height and adjust scrolling if needed */
   unsigned video_info_height;
   unsigned video_info_width;
   size_t i;
   file_list_t *selection_buf    = NULL;
   int entry_padding             = ozone_get_entries_padding(ozone, false);
   float scale_factor            = ozone->last_scale_factor;
   bool menu_show_sublabels      = settings->bools.menu_show_sublabels;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   selection_buf                 = menu_entries_get_selection_buf_ptr(0);

   video_driver_get_size(&video_info_width, &video_info_height);

   ozone->entries_height = 0;

   for (i = 0; i < entries_end; i++)
   {
      /* Entry */
      menu_entry_t entry;
      ozone_node_t *node       = NULL;

      MENU_ENTRY_INIT(entry);
      entry.path_enabled       = false;
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      /* Empty playlist detection:
         only one item which icon is
         OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO */
      if (ozone->is_playlist && entries_end == 1)
      {
         uintptr_t         tex = ozone_entries_icon_get_texture(ozone, entry.enum_idx, entry.type, false);
         ozone->empty_playlist = tex == ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      }
      else
         ozone->empty_playlist = false;

      /* Cache node */
      node                     = (ozone_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      node->height             = ozone->dimensions.entry_height;
      node->wrap               = false;
      node->sublabel_lines     = 0;

      if (menu_show_sublabels)
      {
         if (!string_is_empty(entry.sublabel))
         {
            int sublabel_max_width;
            char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
            wrapped_sublabel_str[0] = '\0';

            node->height += ozone->dimensions.entry_spacing + 40 * scale_factor;

            sublabel_max_width = video_info_width -
               entry_padding * 2 - ozone->dimensions.entry_icon_padding * 2;

            if (ozone->depth == 1)
            {
               sublabel_max_width -= (unsigned) ozone->dimensions_sidebar_width;

               if (ozone->show_thumbnail_bar)
                  sublabel_max_width -= ozone->dimensions.thumbnail_bar_width;
            }

            (ozone->word_wrap)(wrapped_sublabel_str, sizeof(wrapped_sublabel_str), entry.sublabel,
                  sublabel_max_width / 
                  ozone->fonts.entries_sublabel.glyph_width,
                  ozone->fonts.entries_sublabel.wideglyph_width, 0);

            node->sublabel_lines = ozone_count_lines(wrapped_sublabel_str);

            if (node->sublabel_lines > 1)
            {
               node->height += (node->sublabel_lines - 1) * ozone->fonts.entries_sublabel.line_height;
               node->wrap = true;
            }
         }
      }

      node->position_y = ozone->entries_height;

      ozone->entries_height += node->height;
   }

   /* Update scrolling */
   ozone->selection = menu_navigation_get_selection();
   ozone_update_scroll(ozone, false, (ozone_node_t*)selection_buf->list[ozone->selection].userdata);
}

void ozone_entries_update_thumbnail_bar(ozone_handle_t *ozone, bool is_playlist, bool allow_animation)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t tag     = (uintptr_t)&ozone->show_thumbnail_bar;

   entry.duration    = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag         = tag;
   entry.subject     = &ozone->animations.thumbnail_bar_position;

   gfx_animation_kill_by_tag(&tag);

   /* Show it */
   if (is_playlist && !ozone->cursor_in_sidebar && !ozone->show_thumbnail_bar && ozone->depth == 1)
   {
      if (allow_animation)
      {
         ozone->show_thumbnail_bar = true;

         entry.cb             = NULL;
         entry.userdata       = NULL;
         entry.target_value   = ozone->dimensions.thumbnail_bar_width;

         gfx_animation_push(&entry);
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

         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = 0.0f;
         ozone_thumbnail_bar_hide_end(ozone);
      }
   }
}

void ozone_draw_entries(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned selection,
      unsigned selection_old,
      file_list_t *selection_buf,
      float alpha,
      float scroll_y,
      bool is_playlist)
{
   uint32_t alpha_uint32;
   size_t i;
   float bottom_boundary;
   unsigned video_info_height, video_info_width;
   bool menu_show_sublabels          = settings->bools.menu_show_sublabels;
   bool use_smooth_ticker            = settings->bools.menu_ticker_smooth;
   enum gfx_animation_ticker_type 
      menu_ticker_type               = (enum gfx_animation_ticker_type)
      settings->uints.menu_ticker_type;
   bool old_list                     = selection_buf == &ozone->selection_buf_old;
   int x_offset                      = 0;
   size_t selection_y                = 0; /* 0 means no selection (we assume that no entry has y = 0) */
   size_t old_selection_y            = 0;
   int entry_padding                 = ozone_get_entries_padding(ozone, old_list);
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   size_t entries_end                = selection_buf ? selection_buf->size : 0;
   size_t y                          = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.entry_padding_vertical;
   float sidebar_offset              = ozone->sidebar_offset;
   unsigned entry_width              = video_width - (unsigned) ozone->dimensions_sidebar_width - ozone->sidebar_offset - entry_padding * 2 - ozone->animations.thumbnail_bar_position;
   unsigned button_height            = ozone->dimensions.entry_height; /* height of the button (entry minus sublabel) */
   float invert                      = (ozone->fade_direction) ? -1 : 1;
   float alpha_anim                  = old_list ? alpha : 1.0f - alpha;

   video_driver_get_size(&video_info_width, &video_info_height);

   bottom_boundary                   = video_info_height - ozone->dimensions.header_height - ozone->dimensions.footer_height;

   if (old_list)
   {
      alpha = 1.0f - alpha;
      if (alpha != 1.0f)
         x_offset += invert * -(alpha_anim * 120 * scale_factor); /* left */
   }
   else
   {
      if (alpha != 1.0f)
         x_offset += invert *  (alpha_anim * 120 * scale_factor);  /* right */
   }

   x_offset       += (int)sidebar_offset;
   alpha_uint32    = (uint32_t)(alpha * 255.0f);

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

      node                    = (ozone_node_t*)selection_buf->list[i].userdata;

      if (!node || ozone->empty_playlist)
         goto border_iterate;

      if (y + scroll_y + node->height + 20 * scale_factor < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
         goto border_iterate;
      else if (y + scroll_y - node->height - 20 * scale_factor > bottom_boundary)
         goto border_iterate;

      border_start_x = (unsigned) ozone->dimensions_sidebar_width 
         + x_offset + entry_padding;
      border_start_y = y + scroll_y;

      gfx_display_set_alpha(ozone->theme_dynamic.entries_border, alpha);
      gfx_display_set_alpha(ozone->theme_dynamic.entries_checkmark, alpha);

      /* Borders */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            border_start_x,
            border_start_y,
            entry_width,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme_dynamic.entries_border);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            border_start_x,
            border_start_y + button_height,
            entry_width,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme_dynamic.entries_border);

border_iterate:
      if (node)
         y += node->height;
   }

   /* Cursor(s) layer - current */
   if (!ozone->cursor_in_sidebar)
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            (unsigned) ozone->dimensions_sidebar_width 
            + x_offset + entry_padding + ozone->dimensions.spacer_3px,
            entry_width - ozone->dimensions.spacer_5px,
            button_height + ozone->dimensions.spacer_1px,
            selection_y + scroll_y,
            ozone->animations.cursor_alpha * alpha);

   /* Old*/
   if (!ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            (unsigned)ozone->dimensions_sidebar_width 
            + x_offset + entry_padding + ozone->dimensions.spacer_3px,
            /* TODO/FIXME - undefined behavior reported by ASAN -
             *-35.2358 is outside the range of representable values
             of type 'unsigned int'
             * */
            entry_width - ozone->dimensions.spacer_5px,
            button_height + ozone->dimensions.spacer_1px,
            old_selection_y + scroll_y,
            (1-ozone->animations.cursor_alpha) * alpha);

   /* Icons + text */
   y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.entry_padding_vertical;

   if (old_list)
      y += ozone->old_list_offset_y;

   for (i = 0; i < entries_end; i++)
   {
      char rich_label[255];
      char entry_value_ticker[255];
      char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
      uintptr_t tex;
      menu_entry_t entry;
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      unsigned ticker_x_offset     = 0;
      unsigned ticker_str_width    = 0;
      int value_x_offset           = 0;
      static const char* const 
         ticker_spacer             = OZONE_TICKER_SPACER;
      const char *sublabel_str     = NULL;
      ozone_node_t *node           = NULL;
      const char *entry_rich_label = NULL;
      const char *entry_value      = NULL;
      bool entry_selected          = false;
      int text_offset              = -ozone->dimensions.entry_icon_padding - ozone->dimensions.entry_icon_size;
      float *icon_color            = NULL;

      /* Initial ticker configuration */
      if (use_smooth_ticker)
      {
         ticker_smooth.idx           = p_anim->ticker_pixel_idx;
         ticker_smooth.font          = ozone->fonts.entries_label.font;
         ticker_smooth.font_scale    = 1.0f;
         ticker_smooth.type_enum     = menu_ticker_type;
         ticker_smooth.spacer        = ticker_spacer;
         ticker_smooth.x_offset      = &ticker_x_offset;
         ticker_smooth.dst_str_width = &ticker_str_width;
      }
      else
      {
         ticker.idx                  = p_anim->ticker_idx;
         ticker.type_enum            = menu_ticker_type;
         ticker.spacer               = ticker_spacer;
      }

      node                           = (ozone_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      if (y + scroll_y + node->height + 20 * scale_factor 
            < ozone->dimensions.header_height 
            + ozone->dimensions.entry_padding_vertical)
      {
         y += node->height;
         continue;
      }
      else if (y + scroll_y - node->height - 20 * scale_factor 
            > bottom_boundary)
      {
         y += node->height;
         continue;
      }

      entry_selected                 = selection == i;

      MENU_ENTRY_INIT(entry);
      entry.path_enabled             = false;
      entry.label_enabled            = false;
      menu_entry_get(&entry, 0, (unsigned)i, selection_buf, true);

      if (entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
         entry_value         = entry.password_value;
      else
         entry_value         = entry.value;

      /* Prepare text */
      if (!string_is_empty(entry.rich_label))
         entry_rich_label  = entry.rich_label;
      else
         entry_rich_label  = entry.path;

      if (use_smooth_ticker)
      {
         ticker_smooth.selected    = entry_selected && !ozone->cursor_in_sidebar;
         ticker_smooth.field_width = entry_width - entry_padding - (10 * scale_factor) - ozone->dimensions.entry_icon_padding;
         ticker_smooth.src_str     = entry_rich_label;
         ticker_smooth.dst_str     = rich_label;
         ticker_smooth.dst_str_len = sizeof(rich_label);

         gfx_animation_ticker_smooth(&ticker_smooth);
      }
      else
      {
         ticker.s        = rich_label;
         ticker.str      = entry_rich_label;
         ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
         ticker.len      = (entry_width - entry_padding - (10 * scale_factor) - ozone->dimensions.entry_icon_padding) / ozone->fonts.entries_label.glyph_width;

         gfx_animation_ticker(&ticker);
      }

      if (ozone->empty_playlist)
      {
         /* Note: This entry can never be selected, so ticker_x_offset
          * is irrelevant here (i.e. this text will never scroll) */
         unsigned text_width = font_driver_get_message_width(ozone->fonts.entries_label.font, rich_label, (unsigned)strlen(rich_label), 1);
         x_offset = (video_info_width - (unsigned)
               ozone->dimensions_sidebar_width - entry_padding * 2) 
            / 2 - text_width / 2 - 60 * scale_factor;
         y = video_info_height / 2 - 60 * scale_factor;
      }

      sublabel_str = entry.sublabel;

      if (menu_show_sublabels)
      {
         if (node->wrap && !string_is_empty(sublabel_str))
         {
            int sublabel_max_width = video_info_width -
               entry_padding * 2 - ozone->dimensions.entry_icon_padding * 2;

            if (ozone->depth == 1)
            {
               sublabel_max_width -= (unsigned)
                  ozone->dimensions_sidebar_width;

               if (ozone->show_thumbnail_bar)
                  sublabel_max_width -= ozone->dimensions.thumbnail_bar_width;
            }

            wrapped_sublabel_str[0] = '\0';
            (ozone->word_wrap)(wrapped_sublabel_str, sizeof(wrapped_sublabel_str),
                  sublabel_str, sublabel_max_width / ozone->fonts.entries_sublabel.glyph_width,
                  ozone->fonts.entries_sublabel.wideglyph_width, 0);
            sublabel_str = wrapped_sublabel_str;
         }
      }

      /* Icon */
      tex = ozone_entries_icon_get_texture(ozone, entry.enum_idx, entry.type, entry_selected);
      if (tex != ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING])
      {
         uintptr_t texture = tex;

         /* Console specific icons */
         if (     entry.type == FILE_TYPE_RPL_ENTRY 
               && ozone->categories_selection_ptr > ozone->system_tab_end)
         {
            ozone_node_t *sidebar_node = (ozone_node_t*) file_list_get_userdata_at_offset(&ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

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
            icon_color = ozone->theme_dynamic.entries_icon;
         else
            icon_color = ozone->pure_white;

         gfx_display_set_alpha(icon_color, alpha);

         if (dispctx)
         {
            if (dispctx->blend_begin)
               dispctx->blend_begin(userdata);
            if (dispctx->draw)
               ozone_draw_icon(
                     p_disp,
                     userdata,
                     video_width,
                     video_height,
                     ozone->dimensions.entry_icon_size,
                     ozone->dimensions.entry_icon_size,
                     texture,
                     (unsigned)ozone->dimensions_sidebar_width 
                     + x_offset + entry_padding 
                     + ozone->dimensions.entry_icon_padding,
                     y + scroll_y + ozone->dimensions.entry_height 
                     / 2 - ozone->dimensions.entry_icon_size / 2,
                     video_width,
                     video_height,
                     0,
                     1,
                     icon_color);
            if (dispctx->blend_end)
               dispctx->blend_end(userdata);
         }

         if (icon_color == ozone->pure_white)
            gfx_display_set_alpha(icon_color, 1.0f);

         text_offset = 0;
      }

      /* Draw text */
      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            rich_label,
            ticker_x_offset + text_offset + (unsigned)
            ozone->dimensions_sidebar_width + x_offset        + 
            entry_padding + ozone->dimensions.entry_icon_size + 
            ozone->dimensions.entry_icon_padding * 2,
            y + ozone->dimensions.entry_height / 2.0f         + 
            ozone->fonts.entries_label.line_centre_offset     + 
            scroll_y,
            video_width,
            video_height,
            COLOR_TEXT_ALPHA(ozone->theme->text_rgba, alpha_uint32),
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

      if (menu_show_sublabels)
      {
         if (!string_is_empty(sublabel_str))
            gfx_display_draw_text(
                  ozone->fonts.entries_sublabel.font,
                  sublabel_str,
                  (unsigned) ozone->dimensions_sidebar_width + 
                  x_offset + entry_padding                   + 
                  ozone->dimensions.entry_icon_padding,
                  y + ozone->dimensions.entry_height - ozone->dimensions.spacer_1px + (node->height - ozone->dimensions.entry_height - (node->sublabel_lines * ozone->fonts.entries_sublabel.line_height))/2.0f + ozone->fonts.entries_sublabel.line_ascender + scroll_y,
                  video_width,
                  video_height,
                  COLOR_TEXT_ALPHA(ozone->theme->text_sublabel_rgba,alpha_uint32),
                  TEXT_ALIGN_LEFT,
                  1.0f,
                  false,
                  1.0f,
                  false);
      }

      /* Value */
      if (use_smooth_ticker)
      {
         ticker_smooth.selected    = entry_selected && !ozone->cursor_in_sidebar;
         ticker_smooth.field_width = (entry_width - ozone->dimensions.entry_icon_size - ozone->dimensions.entry_icon_padding * 2 -
               ((unsigned)utf8len(entry_rich_label) * ozone->fonts.entries_label.glyph_width));
         ticker_smooth.src_str     = entry_value;
         ticker_smooth.dst_str     = entry_value_ticker;
         ticker_smooth.dst_str_len = sizeof(entry_value_ticker);

         /* Value text is right aligned, so have to offset x
          * by the 'padding' width at the end of the ticker string... */
         if (gfx_animation_ticker_smooth(&ticker_smooth))
            value_x_offset = (ticker_x_offset + ticker_str_width) - ticker_smooth.field_width;
      }
      else
      {
         ticker.s        = entry_value_ticker;
         ticker.str      = entry_value;
         ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
         ticker.len      = (entry_width - ozone->dimensions.entry_icon_size - ozone->dimensions.entry_icon_padding * 2 -
               ((unsigned)utf8len(entry_rich_label) * ozone->fonts.entries_label.glyph_width)) / ozone->fonts.entries_label.glyph_width;

         gfx_animation_ticker(&ticker);
      }

      ozone_draw_entry_value(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            entry_value_ticker,
            value_x_offset + (unsigned) ozone->dimensions_sidebar_width 
            + entry_padding + x_offset 
            + entry_width - ozone->dimensions.entry_icon_padding,
            y + ozone->dimensions.entry_height / 2 + ozone->fonts.entries_label.line_centre_offset + scroll_y,
            alpha_uint32,
            &entry);

      y += node->height;
   }

   /* Text layer */
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);

   if (menu_show_sublabels)
      ozone_font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
}

void ozone_draw_thumbnail_bar(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity)
{
   enum gfx_thumbnail_alignment right_thumbnail_alignment;
   enum gfx_thumbnail_alignment left_thumbnail_alignment;
   unsigned sidebar_width            = ozone->dimensions.thumbnail_bar_width;
   unsigned thumbnail_width          = sidebar_width - (ozone->dimensions.sidebar_entry_icon_padding * 2);
   int right_thumbnail_y_position    = 0;
   int left_thumbnail_y_position     = 0;
   int bottom_row_y_position         = 0;
   bool show_right_thumbnail         = false;
   bool show_left_thumbnail          = false;
   unsigned sidebar_height           = video_height - ozone->dimensions.header_height - ozone->dimensions.sidebar_gradient_height * 2 - ozone->dimensions.footer_height;
   unsigned x_position               = video_width - (unsigned) ozone->animations.thumbnail_bar_position;
   int thumbnail_x_position          = x_position + ozone->dimensions.sidebar_entry_icon_padding;
   unsigned thumbnail_height         = (video_height - ozone->dimensions.header_height - ozone->dimensions.spacer_2px - ozone->dimensions.footer_height - (ozone->dimensions.sidebar_entry_icon_padding * 3)) / 2;
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* Background */
   if (!libretro_running || (menu_framebuffer_opacity >= 1.0f))
   {
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            (unsigned)ozone->animations.thumbnail_bar_position,
            ozone->dimensions.sidebar_gradient_height,
            video_width,
            video_height,
            ozone->theme->sidebar_top_gradient);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_gradient_height,
            (unsigned)ozone->animations.thumbnail_bar_position,
            sidebar_height,
            video_width,
            video_height,
            ozone->theme->sidebar_background);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            video_height - ozone->dimensions.footer_height - ozone->dimensions.sidebar_gradient_height - ozone->dimensions.spacer_1px,
            (unsigned) ozone->animations.thumbnail_bar_position,
            ozone->dimensions.sidebar_gradient_height + ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme->sidebar_bottom_gradient);
   }

   /* Thumbnails */
   show_right_thumbnail =
         (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING) &&
         gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT);
   show_left_thumbnail  =
         (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING) &&
         gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT) &&
         !ozone->selection_core_is_viewer;

   /* If this entry is associated with the image viewer
    * and no right thumbnail is available, show a centred
    * message and return immediately */
   if (ozone->selection_core_is_viewer && !show_right_thumbnail)
   {
      ozone_draw_no_thumbnail_available(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position, sidebar_width, 0);
      return;
   }

   /* Top row
    * > Displays one item, with the following order
    *   of preference:
    *   1) Right thumbnail, if available
    *   2) Left thumbnail, if available
    *   3) 'No thumbnail available' message */

   /* > If this entry is associated with the image viewer
    *   core, there can be only one thumbnail and no
    *   content metadata -> centre image vertically */
   if (ozone->selection_core_is_viewer)
   {
      right_thumbnail_y_position =
            ozone->dimensions.header_height +
            ((thumbnail_height / 2) +
            (int)(1.5f * (float)ozone->dimensions.sidebar_entry_icon_padding));

      right_thumbnail_alignment = GFX_THUMBNAIL_ALIGN_CENTRE;
   }
   else
   {
      right_thumbnail_y_position =
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
            ozone->dimensions.sidebar_entry_icon_padding;

      right_thumbnail_alignment = GFX_THUMBNAIL_ALIGN_BOTTOM;
   }

   /* > If we have a right thumbnail, show it */
   if (show_right_thumbnail)
      gfx_thumbnail_draw(
            userdata,
            video_width,
            video_height,
            &ozone->thumbnails.right,
            (float)thumbnail_x_position,
            (float)right_thumbnail_y_position,
            thumbnail_width,
            thumbnail_height,
            right_thumbnail_alignment,
            1.0f, 1.0f, NULL);
   /* > If we have neither a right thumbnail nor
    *   a left thumbnail to show in its place,
    *   display 'no thumbnail available' message */
   else if (!show_left_thumbnail)
      ozone_draw_no_thumbnail_available(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            sidebar_width,
            thumbnail_height / 2);

   /* Bottom row
    * > Displays one item, with the following order
    *   of preference:
    *   1) Left thumbnail, if available
    *      *and*
    *      right thumbnail has been placed in the top row
    *      *and*
    *      content metadata override is not enabled
    *   2) Content metadata */

   /* > Get baseline 'start' position of bottom row */
   bottom_row_y_position = ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
         thumbnail_height +
         (ozone->dimensions.sidebar_entry_icon_padding * 2);

   /* > If we have a left thumbnail, show it */
   if (show_left_thumbnail)
   {
      float left_thumbnail_alpha;

      /* Normally a right thumbnail will be shown
       * in the top row - if so, left thumbnail
       * goes at the bottom */
      if (show_right_thumbnail)
      {
         left_thumbnail_y_position = bottom_row_y_position;
         left_thumbnail_alignment  = GFX_THUMBNAIL_ALIGN_TOP;
         /* In this case, thumbnail opacity is dependent
          * upon the content metadata override
          * > i.e. Need to handle fade in/out animations
          *   and set opacity to zero when override
          *   is fully active */
         left_thumbnail_alpha      = ozone->animations.left_thumbnail_alpha;
      }
      /* If right thumbnail is missing, shift left
       * thumbnail up to the top row */
      else
      {
         left_thumbnail_y_position = right_thumbnail_y_position;
         left_thumbnail_alignment  = right_thumbnail_alignment;
         /* In this case, there is no dependence on content
          * metadata - thumbnail is always shown at full
          * opacity */
         left_thumbnail_alpha      = 1.0f;
      }

      /* Note: This is a NOOP when alpha is zero
       * (i.e. no performance impact when content
       * metadata override is fully active) */
      gfx_thumbnail_draw(
            userdata,
            video_width,
            video_height,
            &ozone->thumbnails.left,
            (float)thumbnail_x_position,
            (float)left_thumbnail_y_position,
            thumbnail_width,
            thumbnail_height,
            left_thumbnail_alignment,
            left_thumbnail_alpha,
            1.0f, NULL);
   }

   /* > Display content metadata in the bottom
    *   row if:
    *   - This is *not* image viewer content
    *     *and*
    *   - There is no left thumbnail
    *     *or*
    *     left thumbnail has been shifted to
    *     the top row
    *     *or*
    *     content metadata override is enabled
    *     (i.e. fade in, fade out, or fully
    *     active) */
   if (!ozone->selection_core_is_viewer &&
       (!show_left_thumbnail || !show_right_thumbnail ||
        (ozone->animations.left_thumbnail_alpha < 1.0f)))
   {
      char ticker_buf[255];
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      static const char* const ticker_spacer = OZONE_TICKER_SPACER;
      unsigned ticker_x_offset               = 0;
      bool scroll_content_metadata           = settings->bools.ozone_scroll_content_metadata;
      bool use_smooth_ticker                 = settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type 
         menu_ticker_type                    = (enum gfx_animation_ticker_type)
               settings->uints.menu_ticker_type;
      bool show_entry_idx                    = settings->bools.playlist_show_entry_idx;
      unsigned y                             = (unsigned)bottom_row_y_position;
      unsigned separator_padding             = ozone->dimensions.sidebar_entry_icon_padding*2;
      unsigned column_x                      = x_position + separator_padding;
      bool metadata_override_enabled         = show_left_thumbnail &&
                                               show_right_thumbnail &&
                                               (ozone->animations.left_thumbnail_alpha < 1.0f);
      float metadata_alpha                   = metadata_override_enabled ?
            (1.0f - ozone->animations.left_thumbnail_alpha) : 1.0f;
      uint32_t text_color                    = COLOR_TEXT_ALPHA(
            ozone->theme->text_rgba, (uint32_t)(metadata_alpha * 255.0f));

      if (scroll_content_metadata)
      {
         /* Initial ticker configuration */
         if (use_smooth_ticker)
         {
            ticker_smooth.idx                = p_anim->ticker_pixel_idx;
            ticker_smooth.font_scale         = 1.0f;
            ticker_smooth.type_enum          = menu_ticker_type;
            ticker_smooth.spacer             = ticker_spacer;
            ticker_smooth.x_offset           = &ticker_x_offset;
            ticker_smooth.dst_str_width      = NULL;

            ticker_smooth.font               = ozone->fonts.footer.font;
            ticker_smooth.selected           = true;
            ticker_smooth.field_width        = sidebar_width - (separator_padding * 2);
            ticker_smooth.dst_str            = ticker_buf;
            ticker_smooth.dst_str_len        = sizeof(ticker_buf);
         }
         else
         {
            ticker.idx                       = p_anim->ticker_idx;
            ticker.type_enum                 = menu_ticker_type;
            ticker.spacer                    = ticker_spacer;

            ticker.selected                  = true;
            ticker.len                       = (sidebar_width - (separator_padding * 2)) / ozone->fonts.footer.glyph_width;
            ticker.s                         = ticker_buf;
         }
      }

      /* Content metadata */

      /* Separator */
      gfx_display_set_alpha(ozone->theme_dynamic.entries_border, metadata_alpha);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position + separator_padding,
            y,
            sidebar_width - separator_padding*2,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme_dynamic.entries_border);

      y += 18 * scale_factor;

      if (scroll_content_metadata)
      {
         /* Entry enumeration */
         if (show_entry_idx)
         {
            ticker_buf[0] = '\0';

            if (use_smooth_ticker)
            {
               ticker_smooth.src_str = ozone->selection_entry_enumeration;
               gfx_animation_ticker_smooth(&ticker_smooth);
            }
            else
            {
               ticker.str = ozone->selection_entry_enumeration;
               gfx_animation_ticker(&ticker);
            }

            ozone_content_metadata_line(
                  video_width,
                  video_height,
                  ozone,
                  &y,
                  ticker_x_offset + column_x,
                  ticker_buf,
                  text_color,
                  1);
         }

         /* Core association */
         ticker_buf[0] = '\0';

         if (use_smooth_ticker)
         {
            ticker_smooth.src_str = ozone->selection_core_name;
            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.str = ozone->selection_core_name;
            gfx_animation_ticker(&ticker);
         }

         ozone_content_metadata_line(
               video_width, 
               video_height,
               ozone,
               &y,
               ticker_x_offset + column_x,
               ticker_buf,
               text_color,
               1);

         /* Playtime
          * Note: It is essentially impossible for this string
          * to exceed the width of the sidebar, but since we
          * are ticker-texting everything else, we include this
          * by default */
         ticker_buf[0] = '\0';

         if (use_smooth_ticker)
         {
            ticker_smooth.src_str = ozone->selection_playtime;
            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.str = ozone->selection_playtime;
            gfx_animation_ticker(&ticker);
         }

         ozone_content_metadata_line(
               video_width,
               video_height,
               ozone,
               &y,
               ticker_x_offset + column_x,
               ticker_buf,
               text_color,
               1);

         /* Last played */
         ticker_buf[0] = '\0';

         if (use_smooth_ticker)
         {
            ticker_smooth.src_str = ozone->selection_lastplayed;
            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.str = ozone->selection_lastplayed;
            gfx_animation_ticker(&ticker);
         }

         ozone_content_metadata_line(
               video_width,
               video_height,
               ozone,
               &y,
               ticker_x_offset + column_x,
               ticker_buf,
               text_color,
               1);
      }
      else
      {
         /* Entry enumeration */
         if (show_entry_idx)
            ozone_content_metadata_line(
                  video_width,
                  video_height,
                  ozone,
                  &y,
                  column_x,
                  ozone->selection_entry_enumeration,
                  text_color,
                  1);

         /* Core association */
         ozone_content_metadata_line(
               video_width,
               video_height,
               ozone,
               &y,
               column_x,
               ozone->selection_core_name,
               text_color,
               ozone->selection_core_name_lines);

         /* Playtime */
         ozone_content_metadata_line(
               video_width,
               video_height,
               ozone,
               &y,
               column_x,
               ozone->selection_playtime,
               text_color,
               1);

         /* Last played */
         ozone_content_metadata_line(
               video_width,
               video_height,
               ozone,
               &y,
               column_x,
               ozone->selection_lastplayed,
               text_color,
               ozone->selection_lastplayed_lines);
      }

      /* If metadata override is active, display an
       * icon to notify that a left thumbnail image
       * is available */
      if (metadata_override_enabled)
      {
         float         *col = ozone->theme_dynamic.entries_icon;
         /* Icon should be small and unobtrusive
          * > Make it 80% of the normal entry icon size */
         unsigned icon_size = (unsigned)((float)ozone->dimensions.sidebar_entry_icon_size * 0.8f);

         /* > Set its opacity to a maximum of 80% */
         gfx_display_set_alpha(ozone->theme_dynamic.entries_icon, metadata_alpha * 0.8f);

         if (dispctx)
         {
            /* Draw icon in the bottom right corner of
             * the thumbnail bar */
            if (dispctx->blend_begin)
               dispctx->blend_begin(userdata);
            if (dispctx->draw)
               ozone_draw_icon(
                     p_disp,
                     userdata,
                     video_width,
                     video_height,
                     icon_size,
                     icon_size,
                     ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE],
                     x_position + sidebar_width - separator_padding - icon_size,
                     video_height - ozone->dimensions.footer_height - ozone->dimensions.sidebar_entry_icon_padding - icon_size,
                     video_width,
                     video_height,
                     0, 1, col);
            if (dispctx->blend_end)
               dispctx->blend_end(userdata);
         }
      }
   }
}
