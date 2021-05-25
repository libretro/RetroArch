/*  RetroArch - A frontend for libretro.
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

#ifndef _OZONE_H
#define _OZONE_H

typedef struct ozone_handle ozone_handle_t;

#include "ozone_theme.h"
#include "ozone_sidebar.h"

#include <retro_miscellaneous.h>
#include <retro_inline.h>

#include "../../gfx/gfx_animation.h"
#include "../../gfx/gfx_display.h"
#include "../../gfx/gfx_thumbnail_path.h"
#include "../../gfx/gfx_thumbnail.h"
#include "../../menu_screensaver.h"

#include "../../configuration.h"

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

/* > 'SIDEBAR_WIDTH' must be kept in sync with
 *   menu driver metrics */
#define SIDEBAR_WIDTH               408
#define SIDEBAR_X_PADDING           40
#define SIDEBAR_Y_PADDING           20
#define SIDEBAR_ENTRY_HEIGHT        50
#define SIDEBAR_ENTRY_Y_PADDING     10
#define SIDEBAR_ENTRY_ICON_SIZE     46
#define SIDEBAR_ENTRY_ICON_PADDING  15
#define SIDEBAR_GRADIENT_HEIGHT     28

#define FULLSCREEN_THUMBNAIL_PADDING 48

#define CURSOR_SIZE 64
/* Cursor becomes active when it moves more
 * than CURSOR_ACTIVE_DELTA pixels (adjusted
 * by current scale factor) */
#define CURSOR_ACTIVE_DELTA 3

#define INTERVAL_OSK_CURSOR            (0.5f * 1000000)

#if defined(__APPLE__)
/* UTF-8 support is currently broken on Apple devices... */
#define OZONE_TICKER_SPACER "   |   "
#else
/* <EM SPACE><BULLET><EM SPACE>
 * UCN equivalent: "\u2003\u2022\u2003" */
#define OZONE_TICKER_SPACER "\xE2\x80\x83\xE2\x80\xA2\xE2\x80\x83"
#endif

enum ozone_onscreen_entry_position_type
{
   OZONE_ONSCREEN_ENTRY_FIRST = 0,
   OZONE_ONSCREEN_ENTRY_LAST,
   OZONE_ONSCREEN_ENTRY_CENTRE
};

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block; /* ptr alignment */
   int glyph_width;
   int wideglyph_width;
   int line_height;
   int line_ascender;
   int line_centre_offset;
} ozone_font_data_t;

/* Container for a footer text label */
typedef struct
{
   const char *str;
   int width;
} ozone_footer_label_t;

struct ozone_handle
{
   menu_input_pointer_t pointer; /* retro_time_t alignment */

   ozone_theme_t *theme;
   gfx_thumbnail_path_data_t *thumbnail_path_data;
   char *pending_message;
   file_list_t selection_buf_old;                  /* ptr alignment */
   file_list_t horizontal_list; /* console tabs */ /* ptr alignment */
   menu_screensaver_t *screensaver;

   struct
   {
      ozone_font_data_t footer;
      ozone_font_data_t title;
      ozone_font_data_t time;
      ozone_font_data_t entries_label;
      ozone_font_data_t entries_sublabel;
      ozone_font_data_t sidebar;
   } fonts;

   void (*word_wrap)(char *dst, size_t dst_size, const char *src,
      int line_width, int wideglyph_width, unsigned max_lines);

   struct
   {
      ozone_footer_label_t ok;
      ozone_footer_label_t back;
      ozone_footer_label_t search;
      ozone_footer_label_t fullscreen_thumbs;
      ozone_footer_label_t metadata_toggle;
   } footer_labels;

   struct
   {
      gfx_thumbnail_t right;  /* uintptr_t alignment */
      gfx_thumbnail_t left;   /* uintptr_t alignment */
   } thumbnails;
   uintptr_t textures[OZONE_THEME_TEXTURE_LAST];
   uintptr_t icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LAST];
   uintptr_t tab_textures[OZONE_TAB_TEXTURE_LAST];

   size_t categories_selection_ptr; /* active tab id  */
   size_t categories_active_idx_old;

   size_t selection; /* currently selected entry */
   size_t selection_old; /* previously selected entry (for fancy animation) */
   size_t selection_old_list;
   size_t fullscreen_thumbnail_selection;
   size_t num_search_terms_old;
   size_t pointer_categories_selection;
   size_t first_onscreen_entry;
   size_t last_onscreen_entry;
   size_t first_onscreen_category;
   size_t last_onscreen_category;

   int depth;

   struct
   {
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

      int sidebar_padding_horizontal;
      int sidebar_padding_vertical;
      int sidebar_entry_padding_vertical;
      int sidebar_entry_height;
      int sidebar_entry_icon_size;
      int sidebar_entry_icon_padding;
      int sidebar_gradient_height;

      int cursor_size;

      int thumbnail_bar_width;
      int fullscreen_thumbnail_padding;

      int spacer_1px;
      int spacer_2px;
      int spacer_3px;
      int spacer_5px;
   } dimensions;

   unsigned footer_labels_language;
   unsigned last_width;
   unsigned last_height;
   unsigned entries_height;
   unsigned theme_dynamic_cursor_state; /* 0 -> 1 -> 0 -> 1 [...] */
   unsigned selection_core_name_lines;
   unsigned selection_lastplayed_lines;
   unsigned old_list_offset_y;

   float dimensions_sidebar_width; /* animated field */
   float sidebar_offset;
   float last_scale_factor;
   float pure_white[16];

   struct
   {
      float cursor_alpha;
      float scroll_y;
      float scroll_y_sidebar;

      float list_alpha;

      float messagebox_alpha;

      float sidebar_text_alpha;
      float thumbnail_bar_position;

      float fullscreen_thumbnail_alpha;
      float left_thumbnail_alpha;
   } animations;

   struct
   {
      float selection_border[16];
      float selection[16];
      float entries_border[16];
      float entries_icon[16];
      float entries_checkmark[16];
      float cursor_alpha[16];

      float cursor_border[16];
      float message_background[16];
   } theme_dynamic;

   float scroll_old;

   int16_t pointer_active_delta;
   int16_t cursor_x_old;
   int16_t cursor_y_old;

   uint8_t system_tab_end;
   uint8_t tabs[OZONE_SYSTEM_TAB_LAST];

   char title[PATH_MAX_LENGTH];

   char assets_path[PATH_MAX_LENGTH];
   char png_path[PATH_MAX_LENGTH];
   char icons_path[PATH_MAX_LENGTH];
   char tab_path[PATH_MAX_LENGTH];
   char fullscreen_thumbnail_label[255];

   char selection_core_name[255];
   char selection_playtime[255];
   char selection_lastplayed[255];
   char selection_entry_enumeration[255];

   bool cursor_in_sidebar;
   bool cursor_in_sidebar_old;

   bool fade_direction; /* false = left to right, true = right to left */

   bool draw_sidebar;
   bool empty_playlist;

   bool osk_cursor; /* true = display it, false = don't */
   bool messagebox_state;
   bool messagebox_state_old;
   bool should_draw_messagebox;

   bool need_compute;
   bool draw_old_list;
   bool has_all_assets;

   bool is_playlist;
   bool is_playlist_old;

   bool pointer_in_sidebar;
   bool last_pointer_in_sidebar;
   bool show_cursor;
   bool show_screensaver;
   bool cursor_mode;
   bool sidebar_collapsed;
   bool show_thumbnail_bar;
   bool fullscreen_thumbnails_available;
   bool show_fullscreen_thumbnails;
   bool selection_core_is_viewer;

   bool force_metadata_display;

   bool is_db_manager_list;
   bool is_file_list;
   bool is_quick_menu;
   bool first_frame;

   struct
   {
      retro_time_t start_time;
      float amplitude;
      enum menu_action direction;
      bool wiggling;
   } cursor_wiggle_state;
};

/* If you change this struct, also
   change ozone_alloc_node and
   ozone_copy_node */
typedef struct ozone_node
{
   char *fullpath;            /* Entry fullpath */
   char *console_name;        /* Console tab name */
   uintptr_t icon;            /* Console tab icon */
   uintptr_t content_icon;    /* console content icon */
   unsigned height;           /* Entry height */
   unsigned position_y;       /* Entry position Y */
   unsigned sublabel_lines;   /* Entry sublabel lines */
   bool wrap;                 /* Wrap entry? */
} ozone_node_t;

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
      bool is_playlist);

void ozone_draw_sidebar(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity
      );

void ozone_change_tab(ozone_handle_t *ozone,
      enum msg_hash_enums tab,
      enum menu_settings_type type);

void ozone_sidebar_goto(ozone_handle_t *ozone, unsigned new_selection);

unsigned ozone_get_sidebar_height(ozone_handle_t *ozone);

unsigned ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone);

void ozone_leave_sidebar(ozone_handle_t *ozone,
      settings_t *settings,
      uintptr_t tag);

void ozone_go_to_sidebar(ozone_handle_t *ozone,
      settings_t *settings,
      uintptr_t tag);

void ozone_refresh_horizontal_list(ozone_handle_t *ozone,
      settings_t *settings);

void ozone_init_horizontal_list(ozone_handle_t *ozone,
      settings_t *settings);

void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone);

void ozone_context_reset_horizontal_list(ozone_handle_t *ozone);

ozone_node_t *ozone_alloc_node(void);

size_t ozone_list_get_size(void *data, enum menu_list_type type);

void ozone_free_list_nodes(file_list_t *list, bool actiondata);

bool ozone_is_playlist(ozone_handle_t *ozone, bool depth);

void ozone_compute_entries_position(
      ozone_handle_t *ozone,
      settings_t *settings,
      size_t entries_end);

void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node);

void ozone_sidebar_update_collapse(
      ozone_handle_t *ozone,
      settings_t *settings,
      bool allow_animation);

void ozone_refresh_sidebars(
      ozone_handle_t *ozone,
      settings_t *settings,
      unsigned video_height);

void ozone_entries_update_thumbnail_bar(ozone_handle_t *ozone, bool is_playlist, bool allow_animation);

void ozone_draw_thumbnail_bar(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity);

void ozone_hide_fullscreen_thumbnails(ozone_handle_t *ozone, bool animate);
void ozone_show_fullscreen_thumbnails(ozone_handle_t *ozone);

static INLINE unsigned ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

void ozone_update_content_metadata(ozone_handle_t *ozone);

void ozone_font_flush(
      unsigned video_width, unsigned video_height,
      ozone_font_data_t *font_data);

void ozone_toggle_metadata_override(ozone_handle_t *ozone);

#define OZONE_WIGGLE_DURATION 15

/**
 * Starts the cursor wiggle animation in the given direction
 * Use ozone_get_cursor_wiggle_offset to read the animation
 * once it has started
 */
void ozone_start_cursor_wiggle(ozone_handle_t* ozone, enum menu_action direction);

/**
 * Changes x and y to the current offset of the cursor wiggle animation
 */
void ozone_apply_cursor_wiggle_offset(ozone_handle_t* ozone, int* x, size_t* y);

void ozone_list_cache(void *data,
      enum menu_list_type type, unsigned action);

#endif
