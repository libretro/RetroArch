/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018-2020 - natinusala
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
#include "ozone_theme.h"
#include "ozone_display.h"
#include "ozone_sidebar.h"

#include <string/stdstring.h>
#include <file/file_path.h>
#include <formats/image.h>

#include "../../../gfx/gfx_animation.h"

#include "../../../configuration.h"

static const enum msg_hash_enums ozone_system_tabs_value[OZONE_SYSTEM_TAB_LAST] = {
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
#endif
#ifdef HAVE_LIBRETRODB
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB
#else
   MENU_ENUM_LABEL_VALUE_ADD_TAB
#endif
};

static const enum menu_settings_type ozone_system_tabs_type[OZONE_SYSTEM_TAB_LAST] = {
   MENU_SETTINGS,
   MENU_SETTINGS_TAB,
   MENU_HISTORY_TAB,
   MENU_FAVORITES_TAB,
   MENU_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_NETPLAY_TAB,
#endif
#ifdef HAVE_LIBRETRODB
   MENU_ADD_TAB,
   MENU_EXPLORE_TAB
#else
   MENU_ADD_TAB
#endif
};

static const enum msg_hash_enums ozone_system_tabs_idx[OZONE_SYSTEM_TAB_LAST] = {
   MENU_ENUM_LABEL_MAIN_MENU,
   MENU_ENUM_LABEL_SETTINGS_TAB,
   MENU_ENUM_LABEL_HISTORY_TAB,
   MENU_ENUM_LABEL_FAVORITES_TAB,
   MENU_ENUM_LABEL_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_ENUM_LABEL_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_ENUM_LABEL_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_ENUM_LABEL_NETPLAY_TAB,
#endif
#ifdef HAVE_LIBRETRODB
   MENU_ENUM_LABEL_ADD_TAB,
   MENU_ENUM_LABEL_EXPLORE_TAB
#else
   MENU_ENUM_LABEL_ADD_TAB
#endif
};

static const unsigned ozone_system_tabs_icons[OZONE_SYSTEM_TAB_LAST] = {
   OZONE_TAB_TEXTURE_MAIN_MENU,
   OZONE_TAB_TEXTURE_SETTINGS,
   OZONE_TAB_TEXTURE_HISTORY,
   OZONE_TAB_TEXTURE_FAVORITES,
   OZONE_TAB_TEXTURE_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_TAB_TEXTURE_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_TAB_TEXTURE_IMAGE,
#endif
#ifdef HAVE_NETWORKING
   OZONE_TAB_TEXTURE_NETWORK,
#endif
   OZONE_TAB_TEXTURE_SCAN_CONTENT
};

static void ozone_sidebar_collapse_end(void *userdata)
{
   ozone_handle_t *ozone    = (ozone_handle_t*)userdata;

   ozone->sidebar_collapsed = true;
}

static float ozone_sidebar_get_scroll_y(
      ozone_handle_t *ozone, unsigned video_height)
{
   float scroll_y                          =
      ozone->animations.scroll_y_sidebar;
   float selected_position_y               =
      ozone_get_selected_sidebar_y_position(ozone);
   float current_selection_middle_onscreen =
        ozone->dimensions.header_height
      + ozone->dimensions.spacer_1px
      + ozone->animations.scroll_y_sidebar
      + selected_position_y
      + ozone->dimensions.sidebar_entry_height / 2.0f;
   float bottom_boundary                   =
      (float)video_height
      - (ozone->dimensions.header_height + ozone->dimensions.spacer_1px)
      - ozone->dimensions.footer_height;
   float entries_middle                    = (float)video_height / 2.0f;
   float entries_height                    = ozone_get_sidebar_height(ozone);

   if (current_selection_middle_onscreen != entries_middle)
      scroll_y = ozone->animations.scroll_y_sidebar - (current_selection_middle_onscreen - entries_middle);

   if (scroll_y + entries_height < bottom_boundary)
      scroll_y = bottom_boundary - entries_height - ozone->dimensions.sidebar_padding_vertical;

   if (scroll_y > 0.0f)
      return 0.0f;
   return scroll_y;
}

void ozone_draw_sidebar(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity
      )
{
   size_t y;
   int entry_width;
   char console_title[255];
   unsigned i, sidebar_height;
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const
      ticker_spacer                  = OZONE_TICKER_SPACER;
   unsigned ticker_x_offset          = 0;
   settings_t *settings              = config_get_ptr();
   uint32_t text_alpha               = ozone->animations.sidebar_text_alpha 
      * 255.0f;
   bool use_smooth_ticker            = settings->bools.menu_ticker_smooth;
   float scale_factor                = ozone->last_scale_factor;
   enum gfx_animation_ticker_type
      menu_ticker_type               = (enum gfx_animation_ticker_type)
      settings->uints.menu_ticker_type;
   unsigned selection_y              = 0;
   unsigned selection_old_y          = 0;
   unsigned horizontal_list_size     = 0;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = p_anim->ticker_pixel_idx;
      ticker_smooth.font          = ozone->fonts.sidebar.font;
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = menu_ticker_type;
      ticker_smooth.spacer        = ticker_spacer;
      ticker_smooth.x_offset      = &ticker_x_offset;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx                  = p_anim->ticker_idx;
      ticker.type_enum            = menu_ticker_type;
      ticker.spacer               = ticker_spacer;
   }

   horizontal_list_size           = (unsigned)ozone->horizontal_list.size;

   gfx_display_scissor_begin(userdata,
         video_width, video_height,
         0,
         ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
         (unsigned) ozone->dimensions_sidebar_width,
         video_height - ozone->dimensions.header_height - ozone->dimensions.footer_height - ozone->dimensions.spacer_1px);

   /* Background */
   sidebar_height = video_height - ozone->dimensions.header_height - ozone->dimensions.sidebar_gradient_height * 2 - ozone->dimensions.footer_height;

   if (!libretro_running || (menu_framebuffer_opacity >= 1.0f))
   {
      gfx_display_draw_quad(
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            (unsigned)ozone->dimensions_sidebar_width,
            ozone->dimensions.sidebar_gradient_height,
            video_width,
            video_height,
            ozone->theme->sidebar_top_gradient);
      gfx_display_draw_quad(
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_gradient_height,
            (unsigned)ozone->dimensions_sidebar_width,
            sidebar_height,
            video_width,
            video_height,
            ozone->theme->sidebar_background);
      gfx_display_draw_quad(
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset,
              video_height 
            - ozone->dimensions.footer_height 
            - ozone->dimensions.sidebar_gradient_height 
            - ozone->dimensions.spacer_1px,
            (unsigned)ozone->dimensions_sidebar_width,
              ozone->dimensions.sidebar_gradient_height 
            + ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme->sidebar_bottom_gradient);
   }

   /* Tabs */
   /* y offset computation */
   y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_padding_vertical;
   for (i = 0; i < ozone->system_tab_end + horizontal_list_size + 1; i++)
   {
      if (i == ozone->categories_selection_ptr)
      {
         selection_y = (unsigned)y;
         if (ozone->categories_selection_ptr > ozone->system_tab_end)
            selection_y += ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px;
      }

      if (i == ozone->categories_active_idx_old)
      {
         selection_old_y = (unsigned)y;
         if (ozone->categories_active_idx_old > ozone->system_tab_end)
            selection_old_y += ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px;
      }

      y += ozone->dimensions.sidebar_entry_height + ozone->dimensions.sidebar_entry_padding_vertical;
   }

   entry_width = (unsigned) ozone->dimensions_sidebar_width - ozone->dimensions.sidebar_padding_horizontal * 2;

   /* Cursor */
   if (ozone->cursor_in_sidebar)
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal + ozone->dimensions.spacer_3px,
            entry_width - ozone->dimensions.spacer_5px,
            ozone->dimensions.sidebar_entry_height + ozone->dimensions.spacer_1px,
            selection_y + ozone->animations.scroll_y_sidebar,
            ozone->animations.cursor_alpha);

   if (ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal + ozone->dimensions.spacer_3px,
            entry_width - ozone->dimensions.spacer_5px,
            ozone->dimensions.sidebar_entry_height + ozone->dimensions.spacer_1px,
            selection_old_y + ozone->animations.scroll_y_sidebar,
            1-ozone->animations.cursor_alpha);

   /* Menu tabs */
   y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_padding_vertical;
   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   for (i = 0; i < (unsigned)(ozone->system_tab_end+1); i++)
   {
      enum msg_hash_enums value_idx;
      const char *title    = NULL;
      bool     selected    = (ozone->categories_selection_ptr == i);
      unsigned     icon    = ozone_system_tabs_icons[ozone->tabs[i]];

      uint32_t text_color  = COLOR_TEXT_ALPHA((selected ? ozone->theme->text_selected_rgba : ozone->theme->text_rgba), text_alpha);
      float *col           = (selected ? ozone->theme->text_selected : ozone->theme->entries_icon);

      if (!col)
         col               = ozone->pure_white;

      /* Icon */
      ozone_draw_icon(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->dimensions.sidebar_entry_icon_size,
            ozone->dimensions.sidebar_entry_icon_size,
            ozone->tab_textures[icon],
            ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal + ozone->dimensions.sidebar_entry_icon_padding,
            y + ozone->dimensions.sidebar_entry_height / 2 - ozone->dimensions.sidebar_entry_icon_size / 2 + ozone->animations.scroll_y_sidebar,
            video_width,
            video_height,
            0,
            1,
            col);

      value_idx = ozone_system_tabs_value[ozone->tabs[i]];
      title     = msg_hash_to_str(value_idx);

      /* Text */
      if (!ozone->sidebar_collapsed)
         gfx_display_draw_text(
               ozone->fonts.sidebar.font,
               title,
               ozone->sidebar_offset 
               + ozone->dimensions.sidebar_padding_horizontal 
               + ozone->dimensions.sidebar_entry_icon_padding * 2 
               + ozone->dimensions.sidebar_entry_icon_size,
               y + ozone->dimensions.sidebar_entry_height / 2.0f 
               + ozone->fonts.sidebar.line_centre_offset 
               + ozone->animations.scroll_y_sidebar,
               video_width,
               video_height,
               text_color,
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               true);

      y += ozone->dimensions.sidebar_entry_height + ozone->dimensions.sidebar_entry_padding_vertical;
   }

   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);

   /* Console tabs */
   if (horizontal_list_size > 0)
   {
      gfx_display_draw_quad(
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal,
            y + ozone->animations.scroll_y_sidebar,
            entry_width,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme->entries_border);

      y += ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px;

      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);

      for (i = 0; i < horizontal_list_size; i++)
      {
         bool selected = (ozone->categories_selection_ptr == ozone->system_tab_end + 1 + i);

         uint32_t text_color  = COLOR_TEXT_ALPHA((selected ? ozone->theme->text_selected_rgba : ozone->theme->text_rgba), text_alpha);

         ozone_node_t *node = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;
         float *col         = (selected ? ozone->theme->text_selected : ozone->theme->entries_icon);

         if (!node)
            goto console_iterate;

         if (!col)
            col             = ozone->pure_white;

         /* Icon */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               ozone->dimensions.sidebar_entry_icon_size,
               ozone->dimensions.sidebar_entry_icon_size,
               node->icon,
               ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal + ozone->dimensions.sidebar_entry_icon_padding,
               y + ozone->dimensions.sidebar_entry_height / 2 - ozone->dimensions.sidebar_entry_icon_size / 2 + ozone->animations.scroll_y_sidebar,
               video_width,
               video_height,
               0,
               1,
               col);

         /* Text */
         if (ozone->sidebar_collapsed)
            goto console_iterate;

         if (use_smooth_ticker)
         {
            ticker_smooth.selected    = selected;
            /* TODO/FIXME - undefined behavior reported by ASAN -
             *-12.549 is outside the range of representable values
             of type 'unsigned int'
             * */
            ticker_smooth.field_width = (entry_width - ozone->dimensions.sidebar_entry_icon_size - 40 * scale_factor);
            ticker_smooth.src_str     = node->console_name;
            ticker_smooth.dst_str     = console_title;
            ticker_smooth.dst_str_len = sizeof(console_title);

            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.len      = (entry_width - ozone->dimensions.sidebar_entry_icon_size - 40 * scale_factor) / ozone->fonts.sidebar.glyph_width;
            ticker.s        = console_title;
            ticker.selected = selected;
            ticker.str      = node->console_name;

            gfx_animation_ticker(&ticker);
         }

         gfx_display_draw_text(
               ozone->fonts.sidebar.font,
               console_title,
               ticker_x_offset + ozone->sidebar_offset 
               + ozone->dimensions.sidebar_padding_horizontal 
               + ozone->dimensions.sidebar_entry_icon_padding * 2 
               + ozone->dimensions.sidebar_entry_icon_size,
               y + ozone->dimensions.sidebar_entry_height / 2 
               + ozone->fonts.sidebar.line_centre_offset 
               + ozone->animations.scroll_y_sidebar,
               video_width,
               video_height,
               text_color,
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               true);

console_iterate:
         y += ozone->dimensions.sidebar_entry_height + ozone->dimensions.sidebar_entry_padding_vertical;
      }

      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   ozone_font_flush(video_width, video_height, &ozone->fonts.sidebar);

   if (dispctx && dispctx->scissor_end)
      dispctx->scissor_end(userdata,
            video_width, video_height);
}

void ozone_go_to_sidebar(ozone_handle_t *ozone, uintptr_t tag)
{
   struct gfx_animation_ctx_entry entry;

   ozone->selection_old           = ozone->selection;
   ozone->cursor_in_sidebar_old   = ozone->cursor_in_sidebar;
   ozone->cursor_in_sidebar       = true;

   /* Cursor animation */
   ozone->animations.cursor_alpha = 0.0f;

   entry.cb                       = NULL;
   entry.duration                 = ANIMATION_CURSOR_DURATION;
   entry.easing_enum              = EASING_OUT_QUAD;
   entry.subject                  = &ozone->animations.cursor_alpha;
   entry.tag                      = tag;
   entry.target_value             = 1.0f;
   entry.userdata                 = NULL;

   gfx_animation_push(&entry);

   ozone_sidebar_update_collapse(ozone, true);
}

void ozone_leave_sidebar(ozone_handle_t *ozone, uintptr_t tag)
{
   struct gfx_animation_ctx_entry entry;

   if (ozone->empty_playlist)
      return;

   ozone_update_content_metadata(ozone);

   ozone->categories_active_idx_old = ozone->categories_selection_ptr;
   ozone->cursor_in_sidebar_old     = ozone->cursor_in_sidebar;
   ozone->cursor_in_sidebar         = false;

   /* Cursor animation */
   ozone->animations.cursor_alpha   = 0.0f;

   entry.cb             = NULL;
   entry.duration       = ANIMATION_CURSOR_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &ozone->animations.cursor_alpha;
   entry.tag            = tag;
   entry.target_value   = 1.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);

   ozone_sidebar_update_collapse(ozone, true);
}

unsigned ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone)
{
   return ozone->categories_selection_ptr * ozone->dimensions.sidebar_entry_height +
         (ozone->categories_selection_ptr - 1) * ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.sidebar_padding_vertical +
         (ozone->categories_selection_ptr > ozone->system_tab_end ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px : 0);
}

unsigned ozone_get_sidebar_height(ozone_handle_t *ozone)
{
   int entries = (int)(ozone->system_tab_end + 1 + (ozone->horizontal_list.size ));
   return entries * ozone->dimensions.sidebar_entry_height + (entries - 1) * ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.sidebar_padding_vertical +
         (ozone->horizontal_list.size > 0 ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px : 0);
}

void ozone_sidebar_update_collapse(ozone_handle_t *ozone, bool allow_animation)
{
   /* Collapse sidebar if needed */
   struct gfx_animation_ctx_entry entry;
   settings_t *settings      = config_get_ptr();
   bool is_playlist          = ozone_is_playlist(ozone, false);
   uintptr_t tag             = (uintptr_t)&ozone->sidebar_collapsed;
   bool collapse_sidebar     = settings->bools.ozone_collapse_sidebar;

   entry.easing_enum    = EASING_OUT_QUAD;
   entry.tag            = tag;
   entry.userdata       = ozone;
   entry.duration       = ANIMATION_CURSOR_DURATION;

   gfx_animation_kill_by_tag(&tag);

   /* Collapse it */
   if (collapse_sidebar || (is_playlist && !ozone->cursor_in_sidebar))
   {
      if (allow_animation)
      {
         entry.cb = ozone_sidebar_collapse_end;

         /* Text alpha */
         entry.subject        = &ozone->animations.sidebar_text_alpha;
         entry.target_value   = 0.0f;

         gfx_animation_push(&entry);

         /* Collapse */
         entry.subject        = &ozone->dimensions_sidebar_width;
         entry.target_value   = ozone->dimensions.sidebar_width_collapsed;

         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.sidebar_text_alpha = 0.0f;
         ozone->dimensions_sidebar_width      = 
            ozone->dimensions.sidebar_width_collapsed;
         ozone->sidebar_collapsed             = true;
      }
   }
   /* Show it */
   else if (ozone->cursor_in_sidebar || (!is_playlist && !collapse_sidebar))
   {
      if (allow_animation)
      {
         ozone->sidebar_collapsed = false;

         entry.cb = NULL;

         /* Text alpha */
         entry.subject        = &ozone->animations.sidebar_text_alpha;
         entry.target_value   = 1.0f;

         gfx_animation_push(&entry);

         /* Collapse */
         entry.subject        = &ozone->dimensions_sidebar_width;
         entry.target_value   = ozone->dimensions.sidebar_width_normal;

         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.sidebar_text_alpha   = 1.0f;
         ozone->dimensions_sidebar_width        = ozone->dimensions.sidebar_width_normal;
         ozone->sidebar_collapsed               = false;
      }
   }

   ozone_entries_update_thumbnail_bar(ozone, is_playlist, allow_animation);
}

void ozone_sidebar_goto(ozone_handle_t *ozone, unsigned new_selection)
{
   unsigned video_info_height;
   struct gfx_animation_ctx_entry entry;
   uintptr_t tag = (uintptr_t)ozone;

   video_driver_get_size(NULL, &video_info_height);

   if (ozone->categories_selection_ptr != new_selection)
   {
      ozone->categories_active_idx_old = ozone->categories_selection_ptr;
      ozone->categories_selection_ptr = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      gfx_animation_kill_by_tag(&tag);
   }

   /* ozone->animations.scroll_y_sidebar will be modified
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

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
   entry.cb           = NULL;
   entry.duration     = ANIMATION_CURSOR_DURATION;
   entry.easing_enum  = EASING_OUT_QUAD;
   entry.subject      = &ozone->animations.scroll_y_sidebar;
   entry.tag          = tag;
   entry.target_value = ozone_sidebar_get_scroll_y(ozone, video_info_height);
   entry.userdata     = NULL;

   gfx_animation_push(&entry);

   if (new_selection > ozone->system_tab_end)
      ozone_change_tab(ozone, MENU_ENUM_LABEL_HORIZONTAL_MENU, MENU_SETTING_HORIZONTAL_MENU);
   else
      ozone_change_tab(ozone, ozone_system_tabs_idx[ozone->tabs[new_selection]], ozone_system_tabs_type[ozone->tabs[new_selection]]);
}

void ozone_refresh_sidebars(ozone_handle_t *ozone, unsigned video_height)
{
   settings_t *settings                 = config_get_ptr();
   uintptr_t collapsed_tag              = (uintptr_t)&ozone->sidebar_collapsed;
   uintptr_t offset_tag                 = (uintptr_t)&ozone->sidebar_offset;
   uintptr_t thumbnail_tag              = (uintptr_t)&ozone->show_thumbnail_bar;
   uintptr_t scroll_tag                 = (uintptr_t)ozone;
   bool is_playlist                     = ozone_is_playlist(ozone, false);
   bool collapse_sidebar                = settings->bools.ozone_collapse_sidebar;

   /* Kill any existing animations */
   gfx_animation_kill_by_tag(&collapsed_tag);
   gfx_animation_kill_by_tag(&offset_tag);
   gfx_animation_kill_by_tag(&thumbnail_tag);
   if (ozone->depth == 1)
      gfx_animation_kill_by_tag(&scroll_tag);

   /* Set sidebar width */
   if (collapse_sidebar || (is_playlist && !ozone->cursor_in_sidebar))
   {
      ozone->animations.sidebar_text_alpha = 0.0f;
      ozone->dimensions_sidebar_width      = ozone->dimensions.sidebar_width_collapsed;
      ozone->sidebar_collapsed             = true;
   }
   else if (ozone->cursor_in_sidebar || (!is_playlist && !collapse_sidebar))
   {
      ozone->animations.sidebar_text_alpha = 1.0f;
      ozone->dimensions_sidebar_width      = ozone->dimensions.sidebar_width_normal;
      ozone->sidebar_collapsed             = false;
   }

   /* Set sidebar offset */
   if (ozone->depth == 1)
   {
      ozone->sidebar_offset = 0.0f;
      ozone->draw_sidebar   = true;
   }
   else if (ozone->depth > 1)
   {
      ozone->sidebar_offset = -ozone->dimensions_sidebar_width;
      ozone->draw_sidebar   = false;
   }

   /* Set thumbnail bar position */
   if (is_playlist && !ozone->cursor_in_sidebar && ozone->depth == 1)
   {
      ozone->animations.thumbnail_bar_position = ozone->dimensions.thumbnail_bar_width;
      ozone->show_thumbnail_bar                = true;
   }
   else
   {
      ozone->animations.thumbnail_bar_position = 0.0f;
      ozone->show_thumbnail_bar                = false;
   }

   /* If sidebar is on-screen, update scroll position */
   if (ozone->depth == 1)
   {
      ozone->animations.cursor_alpha     = 1.0f;
      ozone->animations.scroll_y_sidebar = ozone_sidebar_get_scroll_y(ozone, video_height);
   }
}

void ozone_change_tab(ozone_handle_t *ozone,
      enum msg_hash_enums tab,
      enum menu_settings_type type)
{
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t stack_size          = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   menu_stack->list[stack_size - 1].label =
      strdup(msg_hash_to_str(tab));
   menu_stack->list[stack_size - 1].type =
      type;

   ozone_list_cache(ozone, MENU_LIST_HORIZONTAL,
         MENU_ACTION_LEFT);

   menu_driver_deferred_push_content_list(selection_buf);
}

void ozone_init_horizontal_list(ozone_handle_t *ozone)
{
   menu_displaylist_info_t info;
   size_t list_size;
   size_t i;
   settings_t *settings              = config_get_ptr();
   const char *dir_playlist          = settings->paths.directory_playlist;
   bool menu_content_show_playlists  = settings->bools.menu_content_show_playlists;
   bool ozone_truncate_playlist_name = settings->bools.ozone_truncate_playlist_name;
   bool ozone_sort_after_truncate    = settings->bools.ozone_sort_after_truncate_playlist_name;

   menu_displaylist_info_init(&info);

   info.list                    = &ozone->horizontal_list;
   info.path                    = strdup(dir_playlist);
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_PLAYLISTS_TAB;

   if (menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
         menu_displaylist_process(&info);
   }

   menu_displaylist_info_free(&info);

   /* Loop through list and set console names */
   list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      char playlist_file_noext[255];
      char *console_name        = NULL;
      const char *playlist_file = ozone->horizontal_list.list[i].path;

      playlist_file_noext[0] = '\0';

      if (!playlist_file)
      {
         file_list_set_alt_at_offset(&ozone->horizontal_list, i, NULL);
         continue;
      }

      /* Remove extension */
      fill_pathname_base_noext(playlist_file_noext,
            playlist_file, sizeof(playlist_file_noext));

      console_name = playlist_file_noext;

      /* Truncate playlist names, if required
       * > Format: "Vendor - Console"
           Remove everything before the hyphen
           and the subsequent space */
      if (ozone_truncate_playlist_name)
      {
         bool hyphen_found = false;

         for (;;)
         {
            /* Check for "- " */
            if (*console_name == '\0')
               break;
            else if (*console_name == '-' && *(console_name + 1) == ' ')
            {
               hyphen_found = true;
               break;
            }

            console_name++;
         }

         if (hyphen_found)
            console_name += 2;
         else
            console_name = playlist_file_noext;
      }

      /* Assign console name to list */
      file_list_set_alt_at_offset(&ozone->horizontal_list, i, console_name);
   }

   /* If playlist names were truncated and option is
    * enabled, re-sort list by console name */
   if (ozone_truncate_playlist_name &&
       ozone_sort_after_truncate &&
       (list_size > 0))
      file_list_sort_on_alt(&ozone->horizontal_list);
}

void ozone_refresh_horizontal_list(ozone_handle_t *ozone)
{
   ozone_context_destroy_horizontal_list(ozone);
   ozone_free_list_nodes(&ozone->horizontal_list, false);
   file_list_deinitialize(&ozone->horizontal_list);

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   file_list_initialize(&ozone->horizontal_list);
   ozone_init_horizontal_list(ozone);

   ozone_context_reset_horizontal_list(ozone);
}

void ozone_context_reset_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   size_t list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path         = NULL;
      const char *console_name = NULL;
      ozone_node_t *node       = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;

      if (!node)
      {
         node = ozone_alloc_node();
         if (!node)
            continue;
      }

      if (!(path = ozone->horizontal_list.list[i].path))
         continue;
      if (string_ends_with_size(path, ".lpl",
               strlen(path), STRLEN_CONST(".lpl")))
      {
         struct texture_image ti;
         char sysname[PATH_MAX_LENGTH];
         char texturepath[PATH_MAX_LENGTH];
         char content_texturepath[PATH_MAX_LENGTH];
         char icons_path[PATH_MAX_LENGTH];

         strlcpy(icons_path, ozone->icons_path, sizeof(icons_path));

         sysname[0] = texturepath[0] = content_texturepath[0] = '\0';

         fill_pathname_base_noext(sysname, path, sizeof(sysname));

         fill_pathname_join_concat(texturepath, icons_path, sysname,
               ".png", sizeof(texturepath));

         /* If the playlist icon doesn't exist return default */

         if (!path_is_valid(texturepath))
            fill_pathname_join_concat(texturepath, icons_path, "default",
                  ".png", sizeof(texturepath));

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

         fill_pathname_join_delim(sysname, sysname,
               "content.png", '-', sizeof(sysname));
         strlcat(content_texturepath, icons_path, sizeof(content_texturepath));
         strlcat(content_texturepath, PATH_DEFAULT_SLASH(), sizeof(content_texturepath));
         strlcat(content_texturepath, sysname, sizeof(content_texturepath));

         /* If the content icon doesn't exist return default-content */
         if (!path_is_valid(content_texturepath))
         {
            strlcat(icons_path,
                  PATH_DEFAULT_SLASH() "default", sizeof(icons_path));
            fill_pathname_join_delim(content_texturepath, icons_path,
                  "content.png", '-', sizeof(content_texturepath));
         }

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

         /* Console name */
         console_name = ozone->horizontal_list.list[i].alt
            ? ozone->horizontal_list.list[i].alt
            : ozone->horizontal_list.list[i].path;

         if (node->console_name)
            free(node->console_name);

         /* Note: console_name will *always* be valid here,
          * but provide a fallback to prevent NULL pointer
          * dereferencing in case of unknown errors... */
         if (console_name)
            node->console_name = strdup(console_name);
         else
            node->console_name = strdup(path);
      }
   }
}

void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   size_t list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      ozone_node_t *node = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;

      if (!node)
         continue;

      if (!(path = ozone->horizontal_list.list[i].path))
         continue;
      if (string_ends_with_size(path, ".lpl",
               strlen(path), STRLEN_CONST(".lpl")))
      {
         video_driver_texture_unload(&node->icon);
         video_driver_texture_unload(&node->content_icon);
      }
   }
}

bool ozone_is_playlist(ozone_handle_t *ozone, bool depth)
{
   bool is_playlist;

   if (ozone->categories_selection_ptr > ozone->system_tab_end)
      is_playlist = true;
   else
   {
      switch (ozone->tabs[ozone->categories_selection_ptr])
      {
         case OZONE_SYSTEM_TAB_MAIN:
         case OZONE_SYSTEM_TAB_SETTINGS:
         case OZONE_SYSTEM_TAB_ADD:
#ifdef HAVE_NETWORKING
         case OZONE_SYSTEM_TAB_NETPLAY:
#endif
#ifdef HAVE_LIBRETRODB
         case OZONE_SYSTEM_TAB_EXPLORE:
#endif
            is_playlist = false;
            break;
         case OZONE_SYSTEM_TAB_HISTORY:
         case OZONE_SYSTEM_TAB_FAVORITES:
         case OZONE_SYSTEM_TAB_MUSIC:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         case OZONE_SYSTEM_TAB_VIDEO:
#endif
#ifdef HAVE_IMAGEVIEWER
         case OZONE_SYSTEM_TAB_IMAGES:
#endif
         default:
            is_playlist = true;
            break;
      }
   }

   if (depth)
      return is_playlist && ozone->depth == 1;

   return is_playlist;
}
