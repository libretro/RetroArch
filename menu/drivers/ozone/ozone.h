/*  RetroArch - A frontend for libretro.
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

#ifndef _OZONE_H
#define _OZONE_H

typedef struct ozone_handle ozone_handle_t;

#include "ozone_theme.h"
#include "ozone_sidebar.h"

#include <retro_miscellaneous.h>

#include "../../menu_thumbnail_path.h"
#include "../../menu_driver.h"

#include "../../../retroarch.h"

#define ANIMATION_PUSH_ENTRY_DURATION  166
#define ANIMATION_CURSOR_DURATION      133
#define ANIMATION_CURSOR_PULSE         500

#define FONT_SIZE_FOOTER            18
#define FONT_SIZE_TITLE             36
#define FONT_SIZE_TIME              22
#define FONT_SIZE_ENTRIES_LABEL     24
#define FONT_SIZE_ENTRIES_SUBLABEL  18
#define FONT_SIZE_SIDEBAR           24

#define HEADER_HEIGHT 87
#define FOOTER_HEIGHT 78

#define ENTRY_PADDING_HORIZONTAL_HALF  60
#define ENTRY_PADDING_HORIZONTAL_FULL  150
#define ENTRY_PADDING_VERTICAL         20
#define ENTRY_HEIGHT                   50
#define ENTRY_SPACING                  8
#define ENTRY_ICON_SIZE                46
#define ENTRY_ICON_PADDING             15

#define SIDEBAR_WIDTH               408
#define SIDEBAR_X_PADDING           40
#define SIDEBAR_Y_PADDING           20
#define SIDEBAR_ENTRY_HEIGHT        50
#define SIDEBAR_ENTRY_Y_PADDING     10
#define SIDEBAR_ENTRY_ICON_SIZE     46
#define SIDEBAR_ENTRY_ICON_PADDING  15

#define CURSOR_SIZE 64

#define INTERVAL_OSK_CURSOR            (0.5f * 1000000)

#if defined(__APPLE__)
/* UTF-8 support is currently broken on Apple devices... */
#define OZONE_TICKER_SPACER "   |   "
#else
/* <EM SPACE><BULLET><EM SPACE>
 * UCN equivalent: "\u2003\u2022\u2003" */
#define OZONE_TICKER_SPACER "\xE2\x80\x83\xE2\x80\xA2\xE2\x80\x83"
#endif

struct ozone_handle
{
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

      float sidebar_text_alpha;
      float thumbnail_bar_position;
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
   unsigned footer_font_glyph_width;
   unsigned sidebar_font_glyph_width;

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

   struct {
      int header_height;
      int footer_height;

      int entry_padding_horizontal_half;
      int entry_padding_horizontal_full;
      int entry_padding_vertical;
      int entry_height;
      int entry_spacing;
      int entry_icon_size;
      int entry_icon_padding;

      int sidebar_width_normal;
      int sidebar_width_collapsed;

      float sidebar_width; /* animated field */
      int sidebar_padding_horizontal;
      int sidebar_padding_vertical;
      int sidebar_entry_padding_vertical;
      int sidebar_entry_height;
      int sidebar_entry_icon_size;
      int sidebar_entry_icon_padding;

      int cursor_size;

      int thumbnail_bar_width;

      float thumbnail_width; /* set at layout time */
      float thumbnail_height; /* set later to thumbnail_width * image aspect ratio */
      float left_thumbnail_width; /* set at layout time */
      float left_thumbnail_height; /* set later to left_thumbnail_width * image aspect ratio */
   } dimensions;

   bool show_cursor;
   bool cursor_mode;

   int16_t cursor_x_old;
   int16_t cursor_y_old;

   bool sidebar_collapsed;

   /* Thumbnails data */
   bool show_thumbnail_bar;

   uintptr_t thumbnail;
   uintptr_t left_thumbnail;

   menu_thumbnail_path_data_t *thumbnail_path_data;

   char selection_core_name[255];
   char selection_playtime[255];
   char selection_lastplayed[255];
   unsigned selection_core_name_lines;
   unsigned selection_lastplayed_lines;
   bool selection_core_is_viewer;

   bool is_db_manager_list;
   bool first_frame;
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
   char *fullpath;

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

ozone_node_t *ozone_alloc_node(void);

size_t ozone_list_get_size(void *data, enum menu_list_type type);

void ozone_free_list_nodes(file_list_t *list, bool actiondata);

bool ozone_is_playlist(ozone_handle_t *ozone, bool depth);

void ozone_compute_entries_position(ozone_handle_t *ozone);

void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node);

void ozone_sidebar_update_collapse(ozone_handle_t *ozone, bool allow_animation);

void ozone_entries_update_thumbnail_bar(ozone_handle_t *ozone, bool is_playlist, bool allow_animation);

void ozone_draw_thumbnail_bar(ozone_handle_t *ozone, video_frame_info_t *video_info);

unsigned ozone_count_lines(const char *str);

void ozone_update_content_metadata(ozone_handle_t *ozone);

#endif
