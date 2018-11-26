/*  RetroArch - A frontend for libretro.
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

#ifndef _OZONE_H
#define _OZONE_H

typedef struct ozone_handle ozone_handle_t;

#include "ozone_theme.h"
#include "ozone_sidebar.h"

#include <retro_miscellaneous.h>

#include "../../menu_driver.h"
#include "../../../retroarch.h"

#define FONT_SIZE_FOOTER 18
#define FONT_SIZE_TITLE 36
#define FONT_SIZE_TIME 22
#define FONT_SIZE_ENTRIES_LABEL 24
#define FONT_SIZE_ENTRIES_SUBLABEL 18
#define FONT_SIZE_SIDEBAR 24

#define ANIMATION_PUSH_ENTRY_DURATION 10
#define ANIMATION_CURSOR_DURATION 8
#define ANIMATION_CURSOR_PULSE 30

#define ENTRIES_START_Y 127

#define INTERVAL_BATTERY_LEVEL_CHECK (30 * 1000000)
#define INTERVAL_OSK_CURSOR (0.5f * 1000000)

struct ozone_handle
{
   uint64_t frame_count;

   struct 
   {
      font_data_t *footer;
      font_data_t *title;
      font_data_t *time;
      font_data_t *entries_label;
      font_data_t *entries_sublabel;
      font_data_t *sidebar;
   } fonts;

   struct
   {
      video_font_raster_block_t footer;
      video_font_raster_block_t title;
      video_font_raster_block_t time;
      video_font_raster_block_t entries_label;
      video_font_raster_block_t entries_sublabel;
      video_font_raster_block_t sidebar;
   } raster_blocks;

   menu_texture_item textures[OZONE_THEME_TEXTURE_LAST];
   menu_texture_item icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LAST];
   menu_texture_item tab_textures[OZONE_TAB_TEXTURE_LAST];

   char title[PATH_MAX_LENGTH];

   char assets_path[PATH_MAX_LENGTH];
   char png_path[PATH_MAX_LENGTH];
   char icons_path[PATH_MAX_LENGTH];
   char tab_path[PATH_MAX_LENGTH];

   uint8_t system_tab_end;
   uint8_t tabs[OZONE_SYSTEM_TAB_LAST];

   size_t categories_selection_ptr; /* active tab id  */
   size_t categories_active_idx_old;

   bool cursor_in_sidebar;
   bool cursor_in_sidebar_old;

   struct
   {
      float cursor_alpha;
      float scroll_y;
      float scroll_y_sidebar;

      float list_alpha;

      float messagebox_alpha;
   } animations;

   bool fade_direction; /* false = left to right, true = right to left */

   size_t selection; /* currently selected entry */
   size_t selection_old; /* previously selected entry (for fancy animation) */
   size_t selection_old_list;

   unsigned entries_height;

   int depth;

   bool draw_sidebar;
   float sidebar_offset;

   unsigned title_font_glyph_width;
   unsigned entry_font_glyph_width;
   unsigned sublabel_font_glyph_width;

   ozone_theme_t *theme;

   struct {
      float selection_border[16];
      float selection[16];
      float entries_border[16];
      float entries_icon[16];
      float entries_checkmark[16];
      float cursor_alpha[16];

      unsigned cursor_state; /* 0 -> 1 -> 0 -> 1 [...] */
      float cursor_border[16];
      float message_background[16];
   } theme_dynamic;

   bool need_compute;

   file_list_t *selection_buf_old;

   bool draw_old_list;
   float scroll_old;

   char *pending_message;
   bool has_all_assets;

   bool is_playlist;
   bool is_playlist_old;

   bool empty_playlist;

   bool osk_cursor; /* true = display it, false = don't */
   bool messagebox_state;
   bool messagebox_state_old;
   bool should_draw_messagebox;

   unsigned old_list_offset_y;

   file_list_t *horizontal_list; /* console tabs */
};

/* If you change this struct, also
   change ozone_alloc_node and
   ozone_copy_node */
typedef struct ozone_node
{
   /* Entries */
   unsigned height;
   unsigned position_y;
   bool wrap;

   /* Console tabs */
   char *console_name;
   uintptr_t icon;
   uintptr_t content_icon;
} ozone_node_t;

void ozone_draw_entries(ozone_handle_t *ozone, video_frame_info_t *video_info,
   unsigned selection, unsigned selection_old,
   file_list_t *selection_buf, float alpha, float scroll_y,
   bool is_playlist);

void ozone_draw_sidebar(ozone_handle_t *ozone, video_frame_info_t *video_info);

void ozone_change_tab(ozone_handle_t *ozone, 
      enum msg_hash_enums tab, 
      enum menu_settings_type type);

void ozone_sidebar_goto(ozone_handle_t *ozone, unsigned new_selection);

unsigned ozone_get_sidebar_height(ozone_handle_t *ozone);

unsigned ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone);

void ozone_leave_sidebar(ozone_handle_t *ozone, uintptr_t tag);

void ozone_go_to_sidebar(ozone_handle_t *ozone, uintptr_t tag);

void ozone_refresh_horizontal_list(ozone_handle_t *ozone);

void ozone_init_horizontal_list(ozone_handle_t *ozone);

void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone);

void ozone_context_reset_horizontal_list(ozone_handle_t *ozone);

ozone_node_t *ozone_alloc_node();

size_t ozone_list_get_size(void *data, enum menu_list_type type);

void ozone_free_list_nodes(file_list_t *list, bool actiondata);

bool ozone_is_playlist(ozone_handle_t *ozone);

#endif
