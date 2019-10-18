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

/* Defines the 'device independent pixel' base
 * unit reference size for all UI elements.
 * 212 px corresponds to the the baseline standard
 * 22 inch, 96 DPI display */
#define MUI_DIP_BASE_UNIT_SIZE 212.0f

/* ==============================
 * Colour Themes START
 * ============================== */

/* Theme colours */
typedef struct
{
   /* Text (& small inline icon) colours */
   uint32_t on_sys_bar;
   uint32_t on_header;
   uint32_t list_text;
   uint32_t list_text_highlighted;
   uint32_t list_hint_text;
   uint32_t list_hint_text_highlighted;
   /* Background colours */
   uint32_t sys_bar_background;
   uint32_t title_bar_background;
   uint32_t list_background;
   uint32_t list_highlighted_background;
   uint32_t nav_bar_background;
   uint32_t surface_background;
   /* List icon colours */
   uint32_t list_icon;
   uint32_t list_switch_on;
   uint32_t list_switch_on_background;
   uint32_t list_switch_off;
   uint32_t list_switch_off_background;
   /* Navigation bar icon colours */
   uint32_t nav_bar_icon_active;
   uint32_t nav_bar_icon_passive;
   uint32_t nav_bar_icon_disabled;
   /* Misc. colours */
   uint32_t shadow;
   uint32_t scrollbar;
   uint32_t divider;
   uint32_t screen_fade;
   float shadow_opacity;
   float screen_fade_opacity;
} materialui_theme_t;

static const materialui_theme_t materialui_theme_blue = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0x0069c0, /* sys_bar_background */
   0x2196f3, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xc1d5e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0x0069c0, /* list_icon */
   0x2196f3, /* list_switch_on */
   0x6ec6ff, /* list_switch_on_background */
   0x808e95, /* list_switch_off */
   0xbabdbe, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x0069c0, /* nav_bar_icon_active */
   0x9ea7aa, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x0069c0, /* scrollbar */
   0x9ea7aa, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_blue_grey = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0x34515e, /* sys_bar_background */
   0x607d8b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xe0e0e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0x34515e, /* list_icon */
   0x607d8b, /* list_switch_on */
   0x8eacbb, /* list_switch_on_background */
   0xbcbcbc, /* list_switch_off */
   0xc7c7c7, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x34515e, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x34515e, /* scrollbar */
   0xc2c2c2, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_dark_blue = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x34515e, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   /* List icon colours */
   0x90caf9, /* list_icon */
   0x64b5f6, /* list_switch_on */
   0x5d99c6, /* list_switch_on_background */
   0x4b636e, /* list_switch_off */
   0x607d8b, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x90caf9, /* nav_bar_icon_active */
   0x8eacbb, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x90caf9, /* scrollbar */
   0x607d8b, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_green = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0x087f23, /* sys_bar_background */
   0x4caf50, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xdcedc8, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0x087f23, /* list_icon */
   0x4caf50, /* list_switch_on */
   0x80e27e, /* list_switch_on_background */
   0xaabb97, /* list_switch_off */
   0xbec5b7, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x087f23, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x087f23, /* scrollbar */
   0xaabb97, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_red = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0xba000d, /* sys_bar_background */
   0xf44336, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xf8bbd0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0xba000d, /* list_icon */
   0xf44336, /* list_switch_on */
   0xff7961, /* list_switch_on_background */
   0xbf5f82, /* list_switch_off */
   0xc48b9f, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xba000d, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0xba000d, /* scrollbar */
   0xbf5f82, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_yellow = {
   /* Text (& small inline icon) colours */
   0x212121, /* on_sys_bar */
   0x000000, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0xc8b900, /* sys_bar_background */
   0xffeb3b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xffecb3, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0xc6a700, /* list_icon */
   0xffeb3b, /* list_switch_on */
   0xccc5af, /* list_switch_on_background */
   0xcaae53, /* list_switch_off */
   0xccc5af, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xc6a700, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xFFFFFF, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0xc6a700, /* scrollbar */
   0xcbba83, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_nvidia_shield = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x255d00, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   /* List icon colours */
   0x7ab547, /* list_icon */
   0x85bb5c, /* list_switch_on */
   0x498515, /* list_switch_on_background */
   0x33691e, /* list_switch_off */
   0x003d00, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x7ab547, /* nav_bar_icon_active */
   0x558b2f, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x7ab547, /* scrollbar */
   0x498515, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_materialui = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   /* Background colours */
   0x3700B3, /* sys_bar_background */
   0x6200ee, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xF2E7FE, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   /* List icon colours */
   0x3700B3, /* list_icon */
   0x03DAC6, /* list_switch_on */
   0x018786, /* list_switch_on_background */
   0x9e47ff, /* list_switch_off */
   0x0400ba, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x018786, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0x018786, /* scrollbar */
   0x018786, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_materialui_dark = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x51455E, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   /* List icon colours */
   0xbb86fc, /* list_icon */
   0x03DAC5, /* list_switch_on */
   0x00a895, /* list_switch_on_background */
   0xbb86fc, /* list_switch_off */
   0x8858c8, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x03DAC6, /* nav_bar_icon_active */
   0x00a895, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0xC89EFC, /* scrollbar */
   0x03DAC6, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_ozone_dark = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x373737, /* title_bar_background */
   0x2D2D2D, /* list_background */
   0x268C75, /* list_highlighted_background */
   0x373737, /* nav_bar_background */
   0x333333, /* surface_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x00FFC5, /* list_switch_on */
   0x00D8AE, /* list_switch_on_background */
   0x9F9FA1, /* list_switch_off */
   0x7D7D7D, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x00FFC5, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x242424, /* nav_bar_icon_disabled */
   /* Misc. colours */
   0x000000, /* shadow */
   0xFFFFFF, /* scrollbar */
   0xFFFFFF, /* divider */
   0x000000, /* screen_fade */
   0.3f,     /* shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

typedef struct
{
   /* Text */
   uint32_t sys_bar_text;
   uint32_t header_text;
   uint32_t list_text;
   uint32_t list_text_highlighted;
   uint32_t list_hint_text;
   uint32_t list_hint_text_highlighted;
   /* Background colours */
   float sys_bar_background[16];
   float title_bar_background[16];
   float list_background[16];
   float list_highlighted_background[16];
   float nav_bar_background[16];
   float surface_background[16];
   /* System bar + header icon colours */
   float sys_bar_icon[16];
   float header_icon[16];
   /* List icon colours */
   float list_icon[16];
   float list_switch_on[16];
   float list_switch_on_background[16];
   float list_switch_off[16];
   float list_switch_off_background[16];
   /* Navigation bar icon colours */
   float nav_bar_icon_active[16];
   float nav_bar_icon_passive[16];
   float nav_bar_icon_disabled[16];
   /* Misc. colours */
   float shadow[16];
   float scrollbar[16];
   float divider[16];
   float screen_fade[16];
} materialui_colors_t;

/* ==============================
 * Colour Themes END
 * ============================== */

/* This struct holds the y position and the line height for each menu entry */
typedef struct
{
   bool has_icon;
   unsigned icon_texture_index;
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
   MUI_TEXTURE_SWITCH_BG,
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
   MUI_TEXTURE_SEARCH,
   MUI_TEXTURE_BATTERY_CRITICAL,
   MUI_TEXTURE_BATTERY_20,
   MUI_TEXTURE_BATTERY_30,
   MUI_TEXTURE_BATTERY_50,
   MUI_TEXTURE_BATTERY_60,
   MUI_TEXTURE_BATTERY_80,
   MUI_TEXTURE_BATTERY_90,
   MUI_TEXTURE_BATTERY_100,
   MUI_TEXTURE_BATTERY_CHARGING,
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

/* Defines all possible entry value types
 * > Note: These are not necessarily 'values',
 *   but they correspond to the object drawn in
 *   the 'value' location when rendering
 *   menu lists */
enum materialui_entry_value_type
{
   MUI_ENTRY_VALUE_NONE = 0,
   MUI_ENTRY_VALUE_TEXT,
   MUI_ENTRY_VALUE_SWITCH_ON,
   MUI_ENTRY_VALUE_SWITCH_OFF,
   MUI_ENTRY_VALUE_CHECKMARK
};

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block;
   int font_height;
   unsigned glyph_width;
} materialui_font_data_t;

#define MUI_BATTERY_PERCENT_MAX_LENGTH 12
#define MUI_TIMEDATE_MAX_LENGTH        255

/* This structure is used to cache system bar
 * string data (+ metadata) to improve rendering
 * performance */
typedef struct
{
   char battery_percent_str[MUI_BATTERY_PERCENT_MAX_LENGTH];
   char timedate_str[MUI_TIMEDATE_MAX_LENGTH];
   int battery_percent_width;
   int timedate_width;
} materialui_sys_bar_cache_t;

typedef struct materialui_handle
{
   bool need_compute;
   bool mouse_show;
   bool is_playlist;
   bool is_file_list;
   bool is_dropdown_list;

   unsigned last_width;
   unsigned last_height;
   float last_scale_factor;
   float dip_base_unit_size;

   int cursor_size;

   unsigned sys_bar_height;
   unsigned title_bar_height;
   unsigned tabs_height;
   unsigned shadow_height;
   unsigned scrollbar_width;
   unsigned divider_width;
   unsigned icon_size;
   unsigned sys_bar_icon_size;
   unsigned margin;
   unsigned sys_bar_margin;
   unsigned categories_active_idx;
   unsigned categories_active_idx_old;

   size_t categories_selection_ptr;
   size_t categories_selection_ptr_old;

   size_t first_onscreen_entry;
   size_t last_onscreen_entry;

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

   /* Font data */
   struct
   {
      materialui_font_data_t title;
      materialui_font_data_t list;
      materialui_font_data_t hint;
   } font_data;

   /* Pointer info */
   menu_input_pointer_t pointer;
   int16_t pointer_start_x;
   int16_t pointer_start_y;
   float pointer_start_scroll_y;

   /* Colour theme parameters */
   enum materialui_color_theme color_theme;
   materialui_colors_t colors;

   /* Cached system bar data */
   materialui_sys_bar_cache_t sys_bar_cache;

   /* Use common tickers for all text
    * > Simplifies configuration and
    *   improves performance */
   bool use_smooth_ticker;
   menu_animation_ctx_ticker_t ticker;
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   unsigned ticker_x_offset;
   unsigned ticker_str_width;

   /* Touch feedback animation parameters */
   bool touch_feedback_cache_selection;
   unsigned touch_feedback_selection;
   float touch_feedback_alpha;

} materialui_handle_t;

static const materialui_theme_t *materialui_get_theme(enum materialui_color_theme color_theme)
{
   switch (color_theme)
   {
      case MATERIALUI_THEME_BLUE:
         return &materialui_theme_blue;
      case MATERIALUI_THEME_BLUE_GREY:
         return &materialui_theme_blue_grey;
      case MATERIALUI_THEME_DARK_BLUE:
         return &materialui_theme_dark_blue;
      case MATERIALUI_THEME_GREEN:
         return &materialui_theme_green;
      case MATERIALUI_THEME_RED:
         return &materialui_theme_red;
      case MATERIALUI_THEME_YELLOW:
         return &materialui_theme_yellow;
      case MATERIALUI_THEME_NVIDIA_SHIELD:
         return &materialui_theme_nvidia_shield;
      case MATERIALUI_THEME_MATERIALUI:
         return &materialui_theme_materialui;
      case MATERIALUI_THEME_MATERIALUI_DARK:
         return &materialui_theme_materialui_dark;
      case MATERIALUI_THEME_OZONE_DARK:
         return &materialui_theme_ozone_dark;
      default:
         break;
   }

   return &materialui_theme_blue;
}

static void materialui_prepare_colors(
      materialui_handle_t *mui, enum materialui_color_theme color_theme)
{
   const materialui_theme_t *current_theme = materialui_get_theme(color_theme);

   /* Parse theme colours */

   /* > Text (& small inline icon) colours */
   mui->colors.sys_bar_text               = (current_theme->on_sys_bar                 << 8) | 0xFF;
   mui->colors.header_text                = (current_theme->on_header                  << 8) | 0xFF;
   mui->colors.list_text                  = (current_theme->list_text                  << 8) | 0xFF;
   mui->colors.list_text_highlighted      = (current_theme->list_text_highlighted      << 8) | 0xFF;
   mui->colors.list_hint_text             = (current_theme->list_hint_text             << 8) | 0xFF;
   mui->colors.list_hint_text_highlighted = (current_theme->list_hint_text_highlighted << 8) | 0xFF;

   /* > Background colours */
   hex32_to_rgba_normalized(
            current_theme->sys_bar_background,
            mui->colors.sys_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->title_bar_background,
            mui->colors.title_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_background,
            mui->colors.list_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_highlighted_background,
            mui->colors.list_highlighted_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_background,
            mui->colors.nav_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->surface_background,
            mui->colors.surface_background, 1.0f);

   /* > System bar + header icon colours */
   hex32_to_rgba_normalized(
            current_theme->on_sys_bar,
            mui->colors.sys_bar_icon, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->on_header,
            mui->colors.header_icon, 1.0f);

   /* > List icon colours */
   hex32_to_rgba_normalized(
            current_theme->list_icon,
            mui->colors.list_icon, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_on,
            mui->colors.list_switch_on, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_on_background,
            mui->colors.list_switch_on_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_off,
            mui->colors.list_switch_off, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_off_background,
            mui->colors.list_switch_off_background, 1.0f);

   /* > Navigation bar icon colours */
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_active,
            mui->colors.nav_bar_icon_active, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_passive,
            mui->colors.nav_bar_icon_passive, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_disabled,
            mui->colors.nav_bar_icon_disabled, 1.0f);

   /* > Misc. colours */
   hex32_to_rgba_normalized(
            current_theme->shadow,
            mui->colors.shadow, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->scrollbar,
            mui->colors.scrollbar, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->divider,
            mui->colors.divider, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->screen_fade,
            mui->colors.screen_fade, current_theme->screen_fade_opacity);

   /* Shadow colour requires special handling
    * (since it is a gradient) */
   mui->colors.shadow[11] = current_theme->shadow_opacity;
   mui->colors.shadow[15] = current_theme->shadow_opacity;
}

static const char *materialui_texture_path(unsigned id)
{
   switch (id)
   {
      case MUI_TEXTURE_POINTER:
         return "pointer.png";
      case MUI_TEXTURE_BACK:
         return "back.png";
      case MUI_TEXTURE_SWITCH_ON:
         return "switch_on.png";
      case MUI_TEXTURE_SWITCH_OFF:
         return "switch_off.png";
      case MUI_TEXTURE_SWITCH_BG:
         return "switch_bg.png";
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
      case MUI_TEXTURE_SEARCH:
         return "search.png";
      case MUI_TEXTURE_BATTERY_CRITICAL:
         return "battery_critical.png";
      case MUI_TEXTURE_BATTERY_20:
         return "battery_20.png";
      case MUI_TEXTURE_BATTERY_30:
         return "battery_30.png";
      case MUI_TEXTURE_BATTERY_50:
         return "battery_50.png";
      case MUI_TEXTURE_BATTERY_60:
         return "battery_60.png";
      case MUI_TEXTURE_BATTERY_80:
         return "battery_80.png";
      case MUI_TEXTURE_BATTERY_90:
         return "battery_90.png";
      case MUI_TEXTURE_BATTERY_100:
         return "battery_100.png";
      case MUI_TEXTURE_BATTERY_CHARGING:
         return "battery_charging.png";
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
      unsigned width, unsigned height)
{
   unsigned tab_icon = 0;
   bool is_active    = false;

   switch (i)
   {
      case MUI_SYSTEM_TAB_MAIN:
         tab_icon = MUI_TEXTURE_TAB_MAIN;
         if (i == mui->categories_selection_ptr)
            is_active = true;
         break;
      case MUI_SYSTEM_TAB_PLAYLISTS:
         tab_icon = MUI_TEXTURE_TAB_PLAYLISTS;
         if (i == mui->categories_selection_ptr)
            is_active = true;
         break;
      case MUI_SYSTEM_TAB_SETTINGS:
         tab_icon = MUI_TEXTURE_TAB_SETTINGS;
         if (i == mui->categories_selection_ptr)
            is_active = true;
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
         is_active ? mui->colors.nav_bar_icon_active : mui->colors.nav_bar_icon_passive);
}

/* Draw the tabs background */
static void materialui_draw_tab_begin(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   /* tabs background */
   menu_display_draw_quad(
         video_info,
         0, height - mui->tabs_height, width,
         mui->tabs_height,
         width, height,
         mui->colors.nav_bar_background);

   /* tabs divider */
   menu_display_draw_quad(
         video_info,
         0, height - mui->tabs_height, width,
         mui->divider_width,
         width, height,
         mui->colors.divider);
}

/* Draw the active tab */
static void materialui_draw_tab_end(materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   /* active tab marker */
   unsigned tab_width = width / (MUI_SYSTEM_TAB_END+1);

   menu_display_draw_quad(
         video_info,
         (int)(mui->categories_selection_ptr * tab_width),
         height - (mui->tabs_height / 16),
         tab_width,
         mui->tabs_height / 16,
         width, height,
         mui->colors.nav_bar_icon_active);
}

/* Draw the scrollbar */
static void materialui_draw_scrollbar(materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height)
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
         mui->colors.scrollbar);
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
      video_frame_info_t *video_info, const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = NULL;

   if (!mui || !mui->font_data.list.font)
      goto end;

   list                     = (struct string_list*)
      string_split(message, "\n");

   if (!list || list->elems == 0)
      goto end;

   y_position = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position = height / 4;

   x = width  / 2;
   y = (int)(y_position - (list->size-1) * mui->font_data.list.font_height / 2);

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int len         = (int)utf8len(msg);
      if (len > longest)
      {
         longest = len;
         longest_width = font_driver_get_message_width(
               mui->font_data.list.font, msg, (unsigned)strlen(msg), 1);
      }
   }

   menu_display_draw_quad(
         video_info,
         x - longest_width / 2.0 - mui->margin * 2.0,
         y -  mui->font_data.list.font_height / 2.0 -  mui->margin * 2.0,
         longest_width + mui->margin * 4.0,
         mui->font_data.list.font_height * list->size + mui->margin * 4.0,
         width,
         height,
         mui->colors.surface_background);

   /* print each line */
   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         menu_display_draw_text(
               mui->font_data.list.font, msg,
               x - longest_width/2.0,
               y + i *  mui->font_data.list.font_height + mui->font_data.list.font_height / 3,
               width, height, mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
   }

   if (menu_input_dialog_get_display_kb())
      menu_display_draw_keyboard(
            mui->textures.list[MUI_TEXTURE_KEY_HOVER],
            mui->font_data.list.font,
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

   for (i = 0; i < entries_end; i++)
   {
      menu_entry_t entry;
      char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
      const char *sublabel_str  = NULL;
      unsigned sublabel_lines   = 0;
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

         if (node->has_icon)
            if (mui->textures.list[node->icon_texture_index])
               icon_margin = mui->icon_size;

         word_wrap(wrapped_sublabel_str, sublabel_str,
               (int)((usable_width - icon_margin) / mui->font_data.hint.glyph_width),
               false, 0);
         sublabel_lines = materialui_count_lines(wrapped_sublabel_str);
      }

      node->line_height =
            (mui->dip_base_unit_size / 5) +
             mui->font_data.list.font_height +
            (sublabel_lines * mui->font_data.hint.font_height);
      node->y           = sum;
      sum              += node->line_height;
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

static void materialui_context_reset_internal(
      materialui_handle_t *mui, bool is_threaded);

/* Called on each frame. We use this callback to:
 * - Determine current scroll postion
 * - Determine index of first/last onscreen entries
 * - Handle dynamic pointer input */
static void materialui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   int header_height        = menu_display_get_header_height();
   size_t entries_end       = menu_entries_get_size();
   file_list_t *list        = menu_entries_get_selection_buf_ptr(0);
   bool first_entry_found   = false;
   size_t i;
   int bottom;
   float scale_factor;

   if (!mui || !list)
      return;

   /* Check whether menu scale factor has changed */
   scale_factor = menu_display_get_dpi_scale(width, height);
   if (scale_factor != mui->last_scale_factor)
   {
      mui->dip_base_unit_size = scale_factor * MUI_DIP_BASE_UNIT_SIZE;
      mui->last_scale_factor  = scale_factor;
      materialui_context_reset_internal(mui, video_driver_is_threaded());
   }

   /* Check whether screen dimensions have changed
    * (this can happen without changing the scaling factor,
    * and it affects list spacing) */
   if ((width != mui->last_width) || (height != mui->last_height))
   {
      mui->need_compute = true;
      mui->last_width   = width;
      mui->last_height  = height;
   }

   if (mui->need_compute)
   {
      if (mui->font_data.list.font && mui->font_data.hint.font)
         materialui_compute_entries_box(mui, width, height);

      /* After calling populate_entries(), we need to call
       * materialui_get_scroll() so the last selected item
       * is correctly displayed on screen.
       * But we can't do this until materialui_compute_entries_box()
       * has been called, so we delay it until here, when
       * mui->need_compute is acted upon. */
      mui->scroll_y    = materialui_get_scroll(mui);

      mui->need_compute = false;
   }

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   menu_display_set_width(width);
   menu_display_set_height(height);

   /* Read pointer state */
   menu_input_get_pointer_state(&mui->pointer);

   /* Need to adjust/range-check scroll position first,
    * otherwise cannot determine correct entry index for
    * MENU_ENTRIES_CTL_SET_START */
   if (mui->pointer.type != MENU_POINTER_DISABLED)
      mui->scroll_y -= mui->pointer.y_accel;

   if (mui->scroll_y < 0.0f)
      mui->scroll_y = 0.0f;

   bottom = mui->content_height - height + header_height + mui->tabs_height;
   if (mui->scroll_y > (float)bottom)
      mui->scroll_y = (float)bottom;

   if (mui->content_height < (height - header_height - mui->tabs_height))
      mui->scroll_y = 0.0f;

   /* Loop over all entries */
   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = (entries_end > 0) ? entries_end - 1 : 0;

   for (i = 0; i < entries_end; i++)
   {
      materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, i);
      int entry_y;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry y postion */
      entry_y = header_height - mui->scroll_y + node->y;

      /* Check whether this is the first onscreen entry */
      if (!first_entry_found)
      {
         if ((entry_y + (int)node->line_height) > header_height)
         {
            mui->first_onscreen_entry = i;
            first_entry_found = true;
         }
      }

      /* Track pointer input, if required */
      if (first_entry_found && (mui->pointer.type != MENU_POINTER_DISABLED))
      {
         int16_t pointer_y = mui->pointer.y;

         if ((pointer_y > entry_y) &&
             (pointer_y < (entry_y + node->line_height)))
         {
            menu_input_set_pointer_selection(i);

            /* The first time this runs following a pointer
             * down event, we have to cache the current
             * pointer selection value in order to correctly
             * handle touch feedback animations */
            if (mui->touch_feedback_cache_selection)
            {
               mui->touch_feedback_selection       = i;
               mui->touch_feedback_cache_selection = false;
            }

            /* If pointer is pressed, stationary, and has been pressed
             * for at least MENU_INPUT_PRESS_TIME_SHORT ms, select current
             * entry */
            if (mui->pointer.pressed &&
                !mui->pointer.dragged &&
                (mui->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT))
               menu_navigation_set_selection(i);
         }
      }

      /* Check whether this is the last onscreen entry */
      if (entry_y > ((int)height - (int)mui->tabs_height))
      {
         /* Current entry is off screen - get index
          * of previous entry */
         if (i > 0)
            mui->last_onscreen_entry = i - 1;
         break;
      }
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &mui->first_onscreen_entry);
}

enum materialui_entry_value_type materialui_get_entry_value_type(
      materialui_handle_t *mui,
      const char *entry_value, bool entry_checked,
      unsigned entry_type, enum msg_file_type entry_file_type)
{
   enum materialui_entry_value_type value_type = MUI_ENTRY_VALUE_NONE;

   /* Check entry value string */
   if (!string_is_empty(entry_value))
   {
      /* Toggle switch off */
      if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
          string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      {
         if (mui->textures.list[MUI_TEXTURE_SWITCH_OFF])
            value_type = MUI_ENTRY_VALUE_SWITCH_OFF;
         else
            value_type = MUI_ENTRY_VALUE_TEXT;
      }
      /* Toggle switch on */
      else if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
               string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))
      {
         if (mui->textures.list[MUI_TEXTURE_SWITCH_ON])
            value_type = MUI_ENTRY_VALUE_SWITCH_ON;
         else
            value_type = MUI_ENTRY_VALUE_TEXT;
      }
      /* Normal value text */
      else
      {
         switch (entry_file_type)
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
               value_type = MUI_ENTRY_VALUE_TEXT;
               break;
         }
      }
   }

   /* Check whether this is the currently selected item
    * of a drop down list */
   if (entry_checked &&
       ((entry_type >= MENU_SETTING_DROPDOWN_ITEM) &&
        (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL)))
      value_type = MUI_ENTRY_VALUE_CHECKMARK;

   return value_type;
}

static void materialui_render_switch_icon(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      float y,
      unsigned width, unsigned height,
      bool on)
{
   unsigned switch_texture_index = on ?
         MUI_TEXTURE_SWITCH_ON : MUI_TEXTURE_SWITCH_OFF;
   float *bg_color               = on ?
         mui->colors.list_switch_on_background : mui->colors.list_switch_off_background;
   float *switch_color           = on ?
         mui->colors.list_switch_on : mui->colors.list_switch_off;

   /* Draw background */
   if (mui->textures.list[MUI_TEXTURE_SWITCH_BG])
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SWITCH_BG],
            width - mui->margin - mui->icon_size,
            y,
            width,
            height,
            0,
            1,
            bg_color);

   /* Draw switch */
   if (mui->textures.list[switch_texture_index])
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[switch_texture_index],
            width - mui->margin - mui->icon_size,
            y,
            width,
            height,
            0,
            1,
            switch_color);
}

/* Draws specified menu entry */
static void materialui_render_menu_entry(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      unsigned width, unsigned height)
{
   const char *entry_value                           = NULL;
   const char *entry_label                           = NULL;
   const char *entry_sublabel                        = NULL;
   unsigned entry_type                               = 0;
   enum materialui_entry_value_type entry_value_type = MUI_ENTRY_VALUE_NONE;
   size_t entry_value_width                          = 0;
   enum msg_file_type entry_file_type                = FILE_TYPE_NONE;
   int entry_y                                       = header_height - mui->scroll_y + node->y;
   int entry_margin                                  = mui->margin;
   int usable_width                                  = width - (mui->margin * 2);
   int label_y                                       = 0;
   int value_icon_y                                  = 0;
   uintptr_t icon_texture                            = 0;

   /* Initial ticker configuration
    * > Note: ticker is only used for labels/values,
    *   not sublabel text */
   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font     = mui->font_data.list.font;
      mui->ticker_smooth.selected = entry_selected;
   }
   else
      mui->ticker.selected = entry_selected;

   /* Read entry parameters */
   menu_entry_get_rich_label(entry, &entry_label);
   menu_entry_get_value(entry, &entry_value);
   menu_entry_get_sublabel(entry, &entry_sublabel);
   entry_type = menu_entry_get_type_new(entry);

   entry_file_type = msg_hash_to_file_type(msg_hash_calculate(entry_value));
   entry_value_type = materialui_get_entry_value_type(
      mui, entry_value, entry->checked, entry_type, entry_file_type);

   /* Draw entry icon
    * > Has to be done first, since it affects the left
    *   hand margin size for label + sublabel text */
   if (node->has_icon)
   {
      if (entry->checked &&
          ((entry_type >= MENU_SETTING_DROPDOWN_ITEM) &&
           (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL)))
         node->has_icon = false;
      else
         icon_texture = mui->textures.list[node->icon_texture_index];
   }
   else
   {
      switch (entry_file_type)
      {
         case FILE_TYPE_COMPRESSED:
            icon_texture = mui->textures.list[MUI_TEXTURE_ARCHIVE];
            break;
         case FILE_TYPE_IMAGE:
            icon_texture = mui->textures.list[MUI_TEXTURE_IMAGE];
            break;
         default:
            break;
      }
   }

   if (icon_texture)
   {
      materialui_draw_icon(video_info,
            mui->icon_size,
            (uintptr_t)icon_texture,
            0,
            entry_y + (node->line_height / 2.0f) - (mui->icon_size / 2.0f),
            width,
            height,
            0,
            1,
            mui->colors.list_icon);

      entry_margin += mui->icon_size;
      usable_width -= mui->icon_size;
   }

   /* Draw entry sublabel
    * > Must be done before label + value, since it
    *   affects y offset positions */
   if (!string_is_empty(entry_sublabel))
   {
      char wrapped_sublabel[MENU_SUBLABEL_MAX_LENGTH];

      wrapped_sublabel[0] = '\0';

      /* Wrap sublabel string */
      word_wrap(wrapped_sublabel, entry_sublabel,
            (int)(usable_width / mui->font_data.hint.glyph_width),
            true, 0);

      /* Draw sublabel string */
      menu_display_draw_text(mui->font_data.hint.font, wrapped_sublabel,
            entry_margin,
            entry_y + (mui->dip_base_unit_size / 5) + mui->font_data.list.font_height,
            width, height,
            (entry_selected || touch_feedback_active) ?
                  mui->colors.list_hint_text_highlighted : mui->colors.list_hint_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

      /* If we have a sublabel, entry label y position has a
       * fixed vertical offset */
      label_y      = entry_y + (mui->dip_base_unit_size / 5.0f);
      value_icon_y = entry_y + (mui->dip_base_unit_size / 6.0f) - (mui->icon_size / 2.0f);
   }
   else
   {
      /* If we don't have a sublabel, entry label is drawn
       * at the vertical centre of the current node
       * Note: Text is drawn relative to the baseline,
       * so we can't do this accurately - but as a general
       * rule of thumb, the descender of a font is at least
       * 20% of it's height - so we just add (font_height / 5) */
      label_y      = entry_y + (node->line_height / 2.0f) + (mui->font_data.list.font_height / 5.0f);
      value_icon_y = entry_y + (node->line_height / 2.0f) - (mui->icon_size / 2.0f);
   }

   /* Draw entry value */
   switch (entry_value_type)
   {
      case MUI_ENTRY_VALUE_TEXT:
         {
            int value_x_offset         = 0;
            size_t entry_value_len     = utf8len(entry_value);
            size_t entry_value_len_max =
                  (size_t)(((usable_width / 2) - mui->margin) / mui->font_data.list.glyph_width);
            char value_buf[255];

            value_buf[0] = '\0';

            /* Limit length of value string */
            entry_value_len = (entry_value_len > entry_value_len_max) ?
                  entry_value_len_max : entry_value_len;

            /* Get effective width of value string
             * > Approximate value - too expensive to use
             *   font_driver_get_message_width() here... */
            entry_value_width = (entry_value_len + 1) * mui->font_data.list.glyph_width;

            /* Apply ticker */
            if (mui->use_smooth_ticker)
            {
               mui->ticker_smooth.field_width = entry_value_width;
               mui->ticker_smooth.src_str     = entry_value;
               mui->ticker_smooth.dst_str     = value_buf;
               mui->ticker_smooth.dst_str_len = sizeof(value_buf);

               /* Value text is right aligned, so have to offset x
                * by the 'padding' width at the end of the ticker string... */
               if (menu_animation_ticker_smooth(&mui->ticker_smooth))
                  value_x_offset = (mui->ticker_x_offset + mui->ticker_str_width) - entry_value_width;
            }
            else
            {
               mui->ticker.s        = value_buf;
               mui->ticker.len      = entry_value_len;
               mui->ticker.str      = entry_value;

               menu_animation_ticker(&mui->ticker);
            }

            /* Draw value string */
            menu_display_draw_text(mui->font_data.list.font, value_buf,
                  value_x_offset + width - mui->margin,
                  label_y,
                  width, height,
                  (entry_selected || touch_feedback_active) ?
                        mui->colors.list_text_highlighted : mui->colors.list_text,
                  TEXT_ALIGN_RIGHT, 1.0f, false, 0, false);
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_ON:
         {
            materialui_render_switch_icon(
                  mui, video_info, value_icon_y, width, height, true);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_OFF:
         {
            materialui_render_switch_icon(
                  mui, video_info, value_icon_y, width, height, false);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_CHECKMARK:
         {
            /* Draw checkmark */
            if (mui->textures.list[MUI_TEXTURE_CHECKMARK])
               materialui_draw_icon(video_info,
                     mui->icon_size,
                     mui->textures.list[MUI_TEXTURE_CHECKMARK],
                     width - mui->margin - mui->icon_size,
                     value_icon_y,
                     width,
                     height,
                     0,
                     1,
                     mui->colors.list_switch_on);

            entry_value_width = mui->icon_size;
         }
         break;
      default:
         entry_value_width = 0;
         break;
   }

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      int label_width = usable_width;
      char label_buf[255];

      label_buf[0] = '\0';

      /* Get maximum width of label string
       * > If a value is present, need additional padding
       *   between label and value */
      label_width = (entry_value_width > 0) ?
            label_width - (entry_value_width + mui->margin) : label_width;

      if (label_width > 0)
      {
         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            /* Label */
            mui->ticker_smooth.field_width = (unsigned)label_width;
            mui->ticker_smooth.src_str     = entry_label;
            mui->ticker_smooth.dst_str     = label_buf;
            mui->ticker_smooth.dst_str_len = sizeof(label_buf);

            menu_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            /* Label */
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(label_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            menu_animation_ticker(&mui->ticker);
         }

         /* Draw label string */
         menu_display_draw_text(mui->font_data.list.font, label_buf,
               mui->ticker_x_offset + entry_margin,
               label_y,
               width, height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
      }
   }
}

/* Draws current menu list */
static void materialui_render_menu_list(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   size_t i;
   size_t first_entry;
   size_t last_entry;
   file_list_t *list           = NULL;
   size_t entries_end          = menu_entries_get_size();
   unsigned header_height      = menu_display_get_header_height();
   size_t selection            = menu_navigation_get_selection();
   bool touch_feedback_enabled =
         (mui->touch_feedback_alpha >= 0.5f) &&
         (mui->touch_feedback_selection == menu_input_get_pointer_selection());

   list = menu_entries_get_selection_buf_ptr(0);
   if (!list)
      return;

   /* Unnecessary sanity check... */
   first_entry = (mui->first_onscreen_entry < entries_end) ? mui->first_onscreen_entry : entries_end;
   last_entry  = (mui->last_onscreen_entry  < entries_end) ? mui->last_onscreen_entry  : entries_end;

   for (i = first_entry; i <= last_entry; i++)
   {
      bool entry_selected        = (selection == i);
      bool touch_feedback_active = touch_feedback_enabled && (mui->touch_feedback_selection == i);
      materialui_node_t *node    = (materialui_node_t*)file_list_get_userdata_at_offset(list, i);
      menu_entry_t entry;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry */
      menu_entry_init(&entry);
      entry.path_enabled = false;
      menu_entry_get(&entry, 0, i, NULL, true);

      /* Render label, value, and associated icons */
      materialui_render_menu_entry(
            mui,
            video_info,
            node,
            &entry,
            entry_selected,
            touch_feedback_active,
            header_height,
            width,
            height);
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

static void materialui_render_background(materialui_handle_t *mui, video_frame_info_t *video_info)
{
   menu_display_ctx_draw_t draw;
   bool add_opacity       = false;
   float opacity_override = 1.0f;
   float color_white[16]  = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };

   /* Configure draw object */
   draw.x                     = 0;
   draw.y                     = 0;
   draw.width                 = video_info->width;
   draw.height                = video_info->height;
   draw.coords                = NULL;
   draw.matrix_data           = NULL;
   draw.prim_type             = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.vertex                = NULL;
   draw.tex_coord             = NULL;
   draw.vertex_count          = 4;
   draw.pipeline.id           = 0;
   draw.pipeline.active       = false;
   draw.pipeline.backend_data = NULL;

   if (mui->textures.bg && !video_info->libretro_running)
   {
      draw.texture = mui->textures.bg;
      draw.color   = color_white;

      /* We are showing a wallpaper - set opacity
       * override to menu_wallpaper_opacity */
      add_opacity      = true;
      opacity_override = video_info->menu_wallpaper_opacity;
   }
   else
   {
      draw.texture = menu_display_white_texture;
      draw.color   = mui->colors.list_background;

      /* We are not showing a wallpaper - if content
       * is running, set opacity override to
       * menu_framebuffer_opacity */
      if (video_info->libretro_running)
      {
         add_opacity      = true;
         opacity_override = video_info->menu_framebuffer_opacity;
      }
   }

   /* If content is not running, have to call menu_display_clear_color()
    * (have no idea if or why this is necessary, but it was
    * included in the original code...) */
   if (!video_info->libretro_running)
   {
      menu_display_ctx_clearcolor_t clearcolor;

      clearcolor.r = mui->colors.list_background[0];
      clearcolor.g = mui->colors.list_background[1];
      clearcolor.b = mui->colors.list_background[2];
      clearcolor.a = mui->colors.list_background[3];

      menu_display_clear_color(&clearcolor, video_info);
   }

   /* Draw background */
   menu_display_blend_begin(video_info);
   menu_display_draw_bg(&draw, video_info, add_opacity, opacity_override);
   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void materialui_render_entry_touch_feedback(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height, unsigned header_height,
      size_t current_selection)
{
   /* Check whether pointer is currently
    * held and stationary */
   bool pointer_active = (mui->pointer.pressed && !mui->pointer.dragged);

   /* If pointer is held and stationary, need to check
    * that current pointer selection is valid
    * i.e. item touched at pointer down event may
    * have changed due to scroll acceleration */
   if (pointer_active)
      pointer_active = (mui->touch_feedback_selection == menu_input_get_pointer_selection());

   /* Touch feedback highlight fades in when pointer
    * is held stationary on a menu entry */
   if (pointer_active)
   {
      /* If pointer is held on currently selected item,
       * background highlight is already drawn
       * > Feedback animation is over, so reset
       *   alpha value and draw nothing */
      if (mui->touch_feedback_selection == current_selection)
      {
         mui->touch_feedback_alpha = 0.0f;
         return;
      }

      /* Update highlight opacity */
      mui->touch_feedback_alpha = (float)mui->pointer.press_duration / (float)MENU_INPUT_PRESS_TIME_SHORT;
      mui->touch_feedback_alpha = (mui->touch_feedback_alpha > 1.0f) ? 1.0f : mui->touch_feedback_alpha;
   }
   /* If pointer has moved, or has been released, any
    * unfinished feedback hightlight animation must
    * fade out */
   else if (mui->touch_feedback_alpha > 0.0f)
   {
      mui->touch_feedback_alpha -= (menu_animation_get_delta_time() * 1000.0f) / (float)MENU_INPUT_PRESS_TIME_SHORT;
      mui->touch_feedback_alpha = (mui->touch_feedback_alpha < 0.0f) ? 0.0f : mui->touch_feedback_alpha;
   }

   /* If alpha value is greater than zero, draw
    * touch feedback highlight */
   if (mui->touch_feedback_alpha > 0.0f)
   {
      /* Sanity check */
      if ((mui->touch_feedback_selection >= mui->first_onscreen_entry) &&
          (mui->touch_feedback_selection <= mui->last_onscreen_entry))
      {
         file_list_t *list       = NULL;
         materialui_node_t *node = NULL;
         float higlight_color[16];

         list = menu_entries_get_selection_buf_ptr(0);
         node = (materialui_node_t*)file_list_get_userdata_at_offset(
               list, mui->touch_feedback_selection);

         /* Sanity check */
         if (!node)
            return;

         /* Set highlight colour */
         memcpy(higlight_color, mui->colors.list_highlighted_background, sizeof(higlight_color));
         menu_display_set_alpha(higlight_color, mui->touch_feedback_alpha);

         /* Draw highlight */
         menu_display_draw_quad(
               video_info,
               0,
               header_height - mui->scroll_y + node->y,
               width,
               node->line_height,
               width,
               height,
               higlight_color);
      }
   }
}

static void materialui_render_header(materialui_handle_t *mui, video_frame_info_t *video_info)
{
   settings_t *settings          = config_get_ptr();
   unsigned width                = video_info->width;
   unsigned height               = video_info->height;
   size_t menu_title_margin      = 0;
   int usable_sys_bar_width      = (int)width;
   int usable_title_bar_width    = (int)width;
   size_t sys_bar_battery_width  = 0;
   size_t sys_bar_clock_width    = 0;
   int sys_bar_text_y            = (mui->sys_bar_height / 2.0f) + (mui->font_data.hint.font_height / 4.0f);
   char menu_title_buf[255];

   menu_title_buf[0]  = '\0';

   if (!settings)
      return;

   /* Draw background quads
    * > Title bar is underneath system bar
    * > Shadow is underneath title bar */

   /* > Shadow */
   menu_display_draw_quad(
         video_info,
         0,
         mui->sys_bar_height + mui->title_bar_height,
         width,
         mui->shadow_height,
         width,
         height,
         mui->colors.shadow);

   /* > Title bar background */
   menu_display_draw_quad(
         video_info,
         0,
         0,
         width,
         mui->sys_bar_height + mui->title_bar_height,
         width,
         height,
         mui->colors.title_bar_background);

   /* > System bar background */
   menu_display_draw_quad(
         video_info,
         0,
         0,
         width,
         mui->sys_bar_height,
         width,
         height,
         mui->colors.sys_bar_background);

   /* System bar items */

   /* > Draw battery indicator (if required) */
   if (settings->bools.menu_battery_level_enable)
   {
      menu_display_ctx_powerstate_t powerstate;
      char percent_str[MUI_BATTERY_PERCENT_MAX_LENGTH];

      percent_str[0] = '\0';

      powerstate.s   = percent_str;
      powerstate.len = sizeof(percent_str);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         /* Need to determine pixel width of percent string
          * > This is somewhat expensive, so utilise a cache
          *   and only update when the string actually changes */
         if (!string_is_equal(percent_str, mui->sys_bar_cache.battery_percent_str))
         {
            /* Cache new string */
            strlcpy(mui->sys_bar_cache.battery_percent_str, percent_str,
                  MUI_BATTERY_PERCENT_MAX_LENGTH * sizeof(char));

            /* Cache width */
            mui->sys_bar_cache.battery_percent_width = font_driver_get_message_width(
                  mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  strlen(mui->sys_bar_cache.battery_percent_str),
                  1.0f);
         }

         if (mui->sys_bar_cache.battery_percent_width > 0)
         {
            /* Set critical by default, to ensure texture_battery
             * is always valid */
            uintptr_t texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_CRITICAL];

            /* Draw battery icon */
            if (powerstate.charging)
               texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_CHARGING];
            else
            {
               if (powerstate.percent >= 100)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_100];
               else if (powerstate.percent >= 90)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_90];
               else if (powerstate.percent >= 80)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_80];
               else if (powerstate.percent >= 60)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_60];
               else if (powerstate.percent >= 50)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_50];
               else if (powerstate.percent >= 30)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_30];
               else if (powerstate.percent >= 20)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_20];
            }

            materialui_draw_icon(video_info,
                  mui->sys_bar_icon_size,
                  (uintptr_t)texture_battery,
                  width - (mui->sys_bar_cache.battery_percent_width +
                        mui->sys_bar_margin + mui->sys_bar_icon_size),
                  0,
                  width,
                  height,
                  0,
                  1,
                  mui->colors.sys_bar_icon);

            /* Draw percent text */
            menu_display_draw_text(mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  width - (mui->sys_bar_cache.battery_percent_width + mui->sys_bar_margin),
                  sys_bar_text_y,
                  width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

            sys_bar_battery_width = mui->sys_bar_cache.battery_percent_width +
                  mui->sys_bar_margin + mui->sys_bar_icon_size;
            usable_sys_bar_width -= sys_bar_battery_width;
         }
      }
   }

   /* > Draw clock (if required) */
   if (settings->bools.menu_timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate_str[MUI_TIMEDATE_MAX_LENGTH];

      timedate_str[0] = '\0';

      datetime.s         = timedate_str;
      datetime.len       = sizeof(timedate_str);
      datetime.time_mode = settings->uints.menu_timedate_style;

      menu_display_timedate(&datetime);

      /* Need to determine pixel width of time string
       * > This is somewhat expensive, so utilise a cache
       *   and only update when the string actually changes */
      if (!string_is_equal(timedate_str, mui->sys_bar_cache.timedate_str))
      {
         /* Cache new string */
         strlcpy(mui->sys_bar_cache.timedate_str, timedate_str,
               MUI_TIMEDATE_MAX_LENGTH * sizeof(char));

         /* Cache width */
         mui->sys_bar_cache.timedate_width = font_driver_get_message_width(
               mui->font_data.hint.font,
               mui->sys_bar_cache.timedate_str,
               utf8len(mui->sys_bar_cache.timedate_str),
               1.0f);
      }

      /* Draw time string */
      if (mui->sys_bar_cache.timedate_width > 0)
      {
         sys_bar_clock_width = mui->sys_bar_cache.timedate_width;

         /* If there is no battery indicator, must add padding */
         if (sys_bar_battery_width == 0)
            sys_bar_clock_width += mui->sys_bar_margin;

         menu_display_draw_text(mui->font_data.hint.font,
               mui->sys_bar_cache.timedate_str,
               width - (sys_bar_clock_width + sys_bar_battery_width),
               sys_bar_text_y,
               width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

         usable_sys_bar_width -= sys_bar_clock_width;
      }
   }

   usable_sys_bar_width -= (2 * mui->sys_bar_margin);
   usable_sys_bar_width = (usable_sys_bar_width > 0) ? usable_sys_bar_width : 0;

   /* > Draw core name, if required */
   if (settings->bools.menu_core_enable)
   {
      char core_title[255];
      char core_title_buf[255];

      core_title[0]     = '\0';
      core_title_buf[0] = '\0';

      menu_entries_get_core_title(core_title, sizeof(core_title));

      if (mui->use_smooth_ticker)
      {
         mui->ticker_smooth.font        = mui->font_data.hint.font;
         mui->ticker_smooth.selected    = true;
         mui->ticker_smooth.field_width = (unsigned)usable_sys_bar_width;
         mui->ticker_smooth.src_str     = core_title;
         mui->ticker_smooth.dst_str     = core_title_buf;
         mui->ticker_smooth.dst_str_len = sizeof(core_title_buf);

         menu_animation_ticker_smooth(&mui->ticker_smooth);
      }
      else
      {
         mui->ticker.s        = core_title_buf;
         mui->ticker.len      = (unsigned)(usable_sys_bar_width / mui->font_data.hint.glyph_width);
         mui->ticker.str      = core_title;
         mui->ticker.selected = true;

         menu_animation_ticker(&mui->ticker);
      }

      menu_display_draw_text(mui->font_data.hint.font, core_title_buf,
            mui->ticker_x_offset + mui->sys_bar_margin,
            sys_bar_text_y,
            width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
   }

   /* Title bar items */

   /* > Draw 'back' icon, if required */
   menu_title_margin = mui->margin;

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      menu_title_margin = mui->icon_size;

      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_BACK],
            0,
            mui->sys_bar_height,
            width,
            height,
            0,
            1,
            mui->colors.header_icon);
   }

   usable_title_bar_width -= menu_title_margin;

   /* > Draw 'search' icon, if required */
   if (mui->is_playlist || mui->is_file_list)
   {
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SEARCH],
            width - mui->icon_size,
            mui->sys_bar_height,
            width,
            height,
            0,
            1,
            mui->colors.header_icon);

      usable_title_bar_width -= mui->icon_size;
   }
   else
      usable_title_bar_width -= mui->margin;

   usable_title_bar_width = (usable_title_bar_width > 0) ? usable_title_bar_width : 0;

   /* > Draw title string */
   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font        = mui->font_data.title.font;
      mui->ticker_smooth.selected    = true;
      mui->ticker_smooth.field_width = (unsigned)usable_title_bar_width;
      mui->ticker_smooth.src_str     = mui->menu_title;
      mui->ticker_smooth.dst_str     = menu_title_buf;
      mui->ticker_smooth.dst_str_len = sizeof(menu_title_buf);

      menu_animation_ticker_smooth(&mui->ticker_smooth);
   }
   else
   {
      mui->ticker.s        = menu_title_buf;
      mui->ticker.len      = (unsigned)(usable_title_bar_width / mui->font_data.title.glyph_width);
      mui->ticker.str      = mui->menu_title;
      mui->ticker.selected = true;

      menu_animation_ticker(&mui->ticker);
   }

   menu_display_draw_text(mui->font_data.title.font, menu_title_buf,
         mui->ticker_x_offset + menu_title_margin,
         mui->sys_bar_height + (mui->title_bar_height / 2.0f) + (mui->font_data.title.font_height / 4.0f),
         width, height, mui->colors.header_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
}

/* Main function of the menu driver
 * Draws all menu elements */
static void materialui_frame(void *data, video_frame_info_t *video_info)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   settings_t *settings     = config_get_ptr();
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   unsigned header_height   = menu_display_get_header_height();
   size_t selection         = menu_navigation_get_selection();

   if (!mui || !settings)
      return;

   menu_display_set_viewport(width, height);

   /* It seems that we have to call this every frame
    * (don't know why...) */
   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);

   /* Clear text */
   font_driver_bind_block(mui->font_data.title.font, &mui->font_data.title.raster_block);
   font_driver_bind_block(mui->font_data.list.font,  &mui->font_data.list.raster_block);
   font_driver_bind_block(mui->font_data.hint.font,  &mui->font_data.hint.raster_block);

   mui->font_data.title.raster_block.carr.coords.vertices = 0;
   mui->font_data.list.raster_block.carr.coords.vertices  = 0;
   mui->font_data.hint.raster_block.carr.coords.vertices  = 0;

   /* Update theme colours, if required */
   if (mui->color_theme != video_info->materialui_color_theme)
   {
      materialui_prepare_colors(mui, (enum materialui_color_theme)video_info->materialui_color_theme);
      mui->color_theme = (enum materialui_color_theme)video_info->materialui_color_theme;
   }

   /* Update line ticker(s) */
   mui->use_smooth_ticker = settings->bools.menu_ticker_smooth;

   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.idx       = menu_animation_get_ticker_pixel_idx();
      mui->ticker_smooth.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   }
   else
   {
      mui->ticker.idx       = menu_animation_get_ticker_idx();
      mui->ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   }

   /* Draw background */
   materialui_render_background(mui, video_info);

   /* Draw 'highlighted entry' selection box */
   if ((selection >= mui->first_onscreen_entry) &&
       (selection <= mui->last_onscreen_entry))
   {
      file_list_t *list       = NULL;
      materialui_node_t *node = NULL;

      list = menu_entries_get_selection_buf_ptr(0);
      node = (materialui_node_t*)file_list_get_userdata_at_offset(list, selection);

      if (node)
         menu_display_draw_quad(
               video_info,
               0,
               header_height - mui->scroll_y + node->y,
               width,
               node->line_height,
               width,
               height,
               mui->colors.list_highlighted_background);
   }

   /* Draw 'short press' touch feedback highlight */
   materialui_render_entry_touch_feedback(
         mui, video_info, width, height, header_height, selection);

   /* Draw menu list */
   if (menu_display_get_update_pending())
      materialui_render_menu_list(mui, video_info, width, height);

   materialui_draw_scrollbar(mui, video_info, width, height);

   /* Flush first layer of text
    * > Menu list only uses list and hint fonts */
   font_driver_flush(width, height, mui->font_data.list.font, video_info);
   font_driver_flush(width, height, mui->font_data.hint.font, video_info);

   mui->font_data.list.raster_block.carr.coords.vertices = 0;
   mui->font_data.hint.raster_block.carr.coords.vertices = 0;

   /* Draw title + system bar */
   materialui_render_header(mui, video_info);

   /* Draw navigation bar */
   if (mui->tabs_height > 0)
   {
      unsigned i = 0;

      materialui_draw_tab_begin(mui, video_info, width, height);

      for (i = 0; i <= MUI_SYSTEM_TAB_END; i++)
         materialui_draw_tab(mui, video_info, i, width, height);

      materialui_draw_tab_end(mui, video_info, width, height);
   }

   /* Flush second layer of text
    * > Title + system bar only use title and hint fonts */
   font_driver_flush(width, height, mui->font_data.title.font, video_info);
   font_driver_flush(width, height, mui->font_data.hint.font,  video_info);

   font_driver_bind_block(mui->font_data.title.font, NULL);
   font_driver_bind_block(mui->font_data.list.font,  NULL);
   font_driver_bind_block(mui->font_data.hint.font,  NULL);

   /* Handle onscreen keyboard */
   if (menu_input_dialog_get_display_kb())
   {
      char msg[255];
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      msg[0] = '\0';

      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, mui->colors.screen_fade);
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);

      materialui_render_messagebox(mui, video_info, msg);
   }

   /* Draw message box */
   if (!string_is_empty(mui->box_message))
   {
      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, mui->colors.screen_fade);

      materialui_render_messagebox(mui, video_info, mui->box_message);

      free(mui->box_message);
      mui->box_message = NULL;
   }

   /* Draw mouse cursor */
   if (mui->mouse_show && (mui->pointer.type != MENU_POINTER_DISABLED))
   {
      float color_white[16]  = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f
      };

      menu_display_draw_cursor(
            video_info,
            color_white,
            mui->cursor_size,
            mui->textures.list[MUI_TEXTURE_POINTER],
            mui->pointer.x,
            mui->pointer.y,
            width,
            height);
   }

   menu_display_restore_clear_color();
   menu_display_unset_viewport(width, height);
}

/* Compute the positions of the widgets */
static void materialui_layout(materialui_handle_t *mui, bool video_is_threaded)
{
   int title_font_size;
   int list_font_size;
   int hint_font_size;
   unsigned new_header_height;

   mui->cursor_size       = mui->dip_base_unit_size / 3;

   mui->sys_bar_height    = mui->dip_base_unit_size / 7;
   mui->title_bar_height  = mui->dip_base_unit_size / 3;
   new_header_height      = mui->sys_bar_height + mui->title_bar_height;

   title_font_size        = mui->dip_base_unit_size / 7;
   list_font_size         = mui->dip_base_unit_size / 9;
   hint_font_size         = mui->dip_base_unit_size / 11;

   mui->shadow_height     = mui->dip_base_unit_size / 36;
   mui->scrollbar_width   = mui->dip_base_unit_size / 36;

   mui->divider_width     = (mui->last_scale_factor > 1.0f) ?
         (unsigned)(mui->last_scale_factor + 0.5f) : 1;

   mui->tabs_height       = 0;
   if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
      mui->tabs_height    = mui->dip_base_unit_size / 3;

   mui->margin            = mui->dip_base_unit_size / 9;
   mui->icon_size         = mui->dip_base_unit_size / 3;

   mui->sys_bar_margin    = mui->dip_base_unit_size / 12;
   mui->sys_bar_icon_size = mui->dip_base_unit_size / 7;

   /* We assume the average glyph aspect ratio is close to 3:4 */
   mui->font_data.title.glyph_width = title_font_size * 3/4;
   mui->font_data.list.glyph_width  = list_font_size  * 3/4;
   mui->font_data.hint.glyph_width  = hint_font_size  * 3/4;

   menu_display_set_header_height(new_header_height);

   if (mui->font_data.title.font)
   {
      menu_display_font_free(mui->font_data.title.font);
      mui->font_data.title.font = NULL;
   }
   if (mui->font_data.list.font)
   {
      menu_display_font_free(mui->font_data.list.font);
      mui->font_data.list.font = NULL;
   }
   if (mui->font_data.hint.font)
   {
      menu_display_font_free(mui->font_data.hint.font);
      mui->font_data.hint.font = NULL;
   }

   mui->font_data.title.font = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         title_font_size, video_is_threaded);

   mui->font_data.list.font  = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         list_font_size, video_is_threaded);

   mui->font_data.hint.font  = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         hint_font_size, video_is_threaded);

   if (mui->font_data.title.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned title_char_width =
         font_driver_get_message_width(mui->font_data.title.font, "a", 1, 1);

      if (title_char_width)
         mui->font_data.title.glyph_width = title_char_width;

      /* Get font height */
      mui->font_data.title.font_height = font_driver_get_line_height(mui->font_data.title.font, 1.0f);
   }

   if (mui->font_data.list.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned list_char_width =
         font_driver_get_message_width(mui->font_data.list.font, "a", 1, 1);

      if (list_char_width)
         mui->font_data.list.glyph_width = list_char_width;

      /* Get font height */
      mui->font_data.list.font_height = font_driver_get_line_height(mui->font_data.list.font, 1.0f);
   }

   if (mui->font_data.hint.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned hint_char_width =
         font_driver_get_message_width(mui->font_data.hint.font, "t", 1, 1);

      if (hint_char_width)
         mui->font_data.hint.glyph_width = hint_char_width;

      /* Get font height */
      mui->font_data.hint.font_height = font_driver_get_line_height(mui->font_data.hint.font, 1.0f);
   }

   /* When updating the layout, the system bar
    * cache must be invalidated (since all text
    * size will change) */
   mui->sys_bar_cache.battery_percent_str[0] = '\0';
   mui->sys_bar_cache.battery_percent_width  = 0;
   mui->sys_bar_cache.timedate_str[0]        = '\0';
   mui->sys_bar_cache.timedate_width         = 0;

   mui->need_compute = true;
}

static void *materialui_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   settings_t *settings     = config_get_ptr();
   materialui_handle_t *mui = NULL;
   menu_handle_t *menu      = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu || !settings)
      return NULL;

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   mui = (materialui_handle_t*)calloc(1, sizeof(materialui_handle_t));

   if (!mui)
      goto error;

   *userdata = mui;

   /* Get DPI/screen-size-aware base unit size for
    * UI elements */
   video_driver_get_size(&width, &height);

   mui->last_width           = width;
   mui->last_height          = height;
   mui->last_scale_factor    = menu_display_get_dpi_scale(width, height);
   mui->dip_base_unit_size   = mui->last_scale_factor * MUI_DIP_BASE_UNIT_SIZE;

   mui->need_compute         = false;
   mui->is_playlist          = false;
   mui->is_file_list         = false;
   mui->is_dropdown_list     = false;

   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = 0;

   mui->menu_title[0]        = '\0';

   /* Set initial theme colours */
   mui->color_theme = (enum materialui_color_theme)settings->uints.menu_materialui_color_theme;
   materialui_prepare_colors(mui, (enum materialui_color_theme)mui->color_theme);

   /* Initial ticker configuration */
   mui->use_smooth_ticker           = settings->bools.menu_ticker_smooth;
   mui->ticker_smooth.font_scale    = 1.0f;
   mui->ticker_smooth.spacer        = NULL;
   mui->ticker_smooth.x_offset      = &mui->ticker_x_offset;
   mui->ticker_smooth.dst_str_width = &mui->ticker_str_width;
   mui->ticker.spacer               = NULL;

   /* Ensure touch feedback parameters are zeroed out */
   mui->touch_feedback_cache_selection = false;
   mui->touch_feedback_selection       = 0;
   mui->touch_feedback_alpha           = 0.0f;

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

   video_coord_array_free(&mui->font_data.title.raster_block.carr);
   video_coord_array_free(&mui->font_data.list.raster_block.carr);
   video_coord_array_free(&mui->font_data.hint.raster_block.carr);

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

   if (mui->font_data.title.font)
      menu_display_font_free(mui->font_data.title.font);
   mui->font_data.title.font = NULL;

   if (mui->font_data.list.font)
      menu_display_font_free(mui->font_data.list.font);
   mui->font_data.list.font = NULL;

   if (mui->font_data.hint.font)
      menu_display_font_free(mui->font_data.hint.font);
   mui->font_data.hint.font = NULL;

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

static void materialui_animate_scroll(
      materialui_handle_t *mui, float scroll_pos, float duration)
{
   menu_animation_ctx_entry_t entry;

   /* mui->scroll_y will be modified by the animation
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

   entry.duration     = duration;
   entry.target_value = scroll_pos;
   entry.subject      = &mui->scroll_y;
   entry.easing_enum  = EASING_IN_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   if (entry.subject)
      menu_animation_push(&entry);
}

/* The navigation pointer has been updated (for example by pressing up or down
   on the keyboard) */
static void materialui_navigation_set(void *data, bool scroll)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
    
   if (!mui || !scroll)
      return;

   materialui_animate_scroll(
         mui,
         materialui_get_scroll(mui),
         166.0f);
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
   mui->scroll_y = 0.0f;
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

/* A new list had been pushed */
static void materialui_populate_entries(
      void *data, const char *path,
      const char *label, unsigned i)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;

   if (!mui)
      return;

   /* Set menu title */
   menu_entries_get_title(mui->menu_title, sizeof(mui->menu_title));

   /* Check whether we are currently viewing a playlist,
    * file-browser-type list or dropdown list
    * (each of these is regarded as a 'plain' list,
    * and potentially a 'long' list, with special
    * gesture-based navigation shortcuts)  */
   mui->is_playlist      = false;
   mui->is_file_list     = false;
   mui->is_dropdown_list = false;

   mui->is_playlist = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST));

   if (!mui->is_playlist)
   {
      /* > All of the following count as a 'file list'
       *   Note: MENU_ENUM_LABEL_FAVORITES is always set
       *   as the 'label' when navigating directories after
       *   selecting load content */
      mui->is_file_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES));

      if (!mui->is_file_list)
         mui->is_dropdown_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE));
   }

   /* Set tab bar height
    * (Tab bar is displayed if menu depth equals one,
    * otherwise it is hidden) */
   mui->tabs_height = 0;
   if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
      mui->tabs_height = mui->dip_base_unit_size / 3;

   mui->need_compute = true;

   /* Note: mui->scroll_y position needs to be set here,
    * but we can't do this until materialui_compute_entries_box()
    * has been called. We therefore delegate it until mui->need_compute
    * is acted upon */

   /* Reset touch feedback parameters
    * (i.e. there should be no leftover highlight
    * animations when switching to a new list) */
   mui->touch_feedback_cache_selection = false;
   mui->touch_feedback_selection       = 0;
   mui->touch_feedback_alpha           = 0.0f;
}

static void materialui_context_reset_internal(
      materialui_handle_t *mui, bool is_threaded)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   materialui_layout(mui, is_threaded);
   materialui_context_bg_destroy(mui);
   menu_display_allocate_white_texture();
   materialui_context_reset_textures(mui);

   if (path_is_valid(settings->paths.path_menu_wallpaper))
      task_push_image_load(settings->paths.path_menu_wallpaper,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);
}

/* Context reset is called on launch or when a core is launched */
static void materialui_context_reset(void *data, bool is_threaded)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;

   if (!mui)
      return;

   materialui_context_reset_internal(mui, is_threaded);
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

/* Pointer down event
 * Used cache the initial pointer x/y position,
 * and initialise touch feedback animations */
static int materialui_pointer_down(void *userdata,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return -1;

   mui->pointer_start_x             = x;
   mui->pointer_start_y             = y;
   mui->pointer_start_scroll_y      = mui->scroll_y;

   /* Note: ptr argument is useless here, since it
    * has no meaning when handling touch screen input... */
   mui->touch_feedback_cache_selection = true;
   mui->touch_feedback_selection       = 0;
   mui->touch_feedback_alpha           = 0.0f;

   return 0;
}

static int materialui_pointer_up_swipe_horz_plain_list(
      materialui_handle_t *mui, menu_entry_t *entry,
      unsigned height, unsigned header_height, unsigned y,
      size_t selection, size_t entries_end,
      bool scroll_up)
{
   /* A swipe in the top half of the screen ascends/
    * decends the alphabet */
   if (y < (height >> 1))
   {
      menu_entry_t *entry_ptr = entry;
      size_t new_selection    = selection;
      menu_entry_t new_entry;

      /* Check if currently active item is off screen */
      if ((selection < mui->first_onscreen_entry) ||
          (selection > mui->last_onscreen_entry))
      {
         /* ...if it is, must immediately select entry
          * at 'edge' of screen in opposite direction to
          * scroll action */
         new_selection = scroll_up ?
               mui->last_onscreen_entry : mui->first_onscreen_entry;

         if (new_selection < entries_end)
         {
            menu_navigation_set_selection(new_selection);

            /* Update entry pointer */
            menu_entry_init(&new_entry);
            new_entry.path_enabled       = false;
            new_entry.label_enabled      = false;
            new_entry.rich_label_enabled = false;
            new_entry.value_enabled      = false;
            new_entry.sublabel_enabled   = false;
            menu_entry_get(&new_entry, 0, new_selection, NULL, true);
            entry_ptr                    = &new_entry;
         }
         else
            new_selection = selection; /* Should never happen... */
      }

      return menu_entry_action(
            entry_ptr, (unsigned)new_selection,
            scroll_up ? MENU_ACTION_SCROLL_UP : MENU_ACTION_SCROLL_DOWN);
   }
   /* A swipe in the bottom half of the screen scrolls
    * by 10% of the list size or one 'page', whichever
    * is largest */
   else
   {
      float content_height_fraction = mui->content_height * 0.1f;
      float display_height          = (int)height - (int)header_height - mui->tabs_height;
      float scroll_offset           = (display_height > content_height_fraction) ?
            display_height : content_height_fraction;

      materialui_animate_scroll(
            mui,
            mui->scroll_y + (scroll_up ? (scroll_offset * -1.0f) : scroll_offset),
            166.0f);
   }

   return 0;
}

static int materialui_pointer_up_swipe_horz_default(
      materialui_handle_t *mui, menu_entry_t *entry,
      unsigned ptr, size_t selection, size_t entries_end, enum menu_action action)
{
   int ret = 0;

   if ((ptr < entries_end) && (ptr == selection))
   {
      size_t new_selection = menu_navigation_get_selection();
      ret                  = menu_entry_action(entry, (unsigned)selection, action);

      /* If we are changing a settings value, want to scroll
       * back to the 'pointer down' position. In all other cases
       * we do not. An entry is of the 'settings' type if:
       * - Selection pointer remains the same after MENU_ACTION event
       * - Entry has a value
       * Note: cannot use input (argument) entry, since this
       * will always have a blank value component */
      if (selection == new_selection)
      {
         menu_entry_t last_entry;

         menu_entry_init(&last_entry);
         last_entry.path_enabled       = false;
         last_entry.label_enabled      = false;
         last_entry.rich_label_enabled = false;
         last_entry.sublabel_enabled   = false;

         menu_entry_get(&last_entry, 0, selection, NULL, true);

         if (!string_is_empty(last_entry.value))
            materialui_animate_scroll(
                  mui,
                  mui->pointer_start_scroll_y,
                  83.0f);
      }
   }

   return ret;
}

/* Pointer up event */
static int materialui_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width, height;
   unsigned header_height, i;
   size_t selection         = menu_navigation_get_selection();
   size_t entries_end       = menu_entries_get_size();
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return 0;

   header_height = menu_display_get_header_height();
   video_driver_get_size(&width, &height);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         {
            /* Tap/press header: Menu back/cancel, or search */
            if (y < header_height)
            {
               /* If this is a playlist or file list, enable
                * search functionality */
               if (mui->is_playlist || mui->is_file_list)
               {
                  /* Check if user has touched search icon */
                  if (x > (width - mui->icon_size))
                     return menu_input_dialog_start_search() ? 0 : -1;
                  /* If not, add a little extra padding to minimise
                   * the risk of accidentally triggering a cancel */
                  else if (x <= (width - (2 * mui->icon_size)))
                     return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
               }
               /* If this is not a playlist or file list, a tap/press
                * anywhere on the header triggers a MENU_ACTION_CANCEL
                * action */
               else
                  return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
            }
            /* Tap/press tab bar: Switch to corresponding top-level
             * menu screen */
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
            /* Tap/press menu item: Activate and/or select item */
            else if (ptr < entries_end)
            {
               if (gesture == MENU_INPUT_GESTURE_TAP)
               {
                  /* A 'tap' always produces a menu action */

                  /* If current 'pointer' item is not active,
                   * activate it immediately */
                  if (ptr != selection)
                     menu_navigation_set_selection(ptr);

                  /* Perform a MENU_ACTION_SELECT on currently
                   * active item */
                  return menu_entry_action(entry, ptr, MENU_ACTION_SELECT);
               }
               else
               {
                  /* A 'short' press is used only to activate (highlight)
                   * an item - it does not invoke a MENU_ACTION_SELECT
                   * action (this is intended for use in activating a
                   * settings-type entry, prior to swiping)
                   * Note: If everything is working correctly, the
                   * ptr item should already by selected at this stage
                   * - but menu_navigation_set_selection() just sets a
                   * variable, so there's no real point in performing
                   * a (selection != ptr) check here */
                  menu_navigation_set_selection(ptr);
                  menu_input_set_pointer_y_accel(0.0f);
               }
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if ((ptr < entries_end) && (ptr == selection))
            return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         {
            /* If we are at the top level, a swipe should
             * just switch between the three main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *left* swipe should
             * trigger a *right* navigation event */
            if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
               return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_RIGHT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, entries_end, true);
            /* In all other cases, just perform a normal 'left'
             * navigation event */
            else
               return materialui_pointer_up_swipe_horz_default(
                     mui, entry, ptr, selection, entries_end, MENU_ACTION_LEFT);
         }
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         {
            /* If we are at the top level, a swipe should
             * just switch between the three main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *right* swipe should
             * trigger a *left* navigation event */
            if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
               menu_entry_action(entry, (unsigned)selection, MENU_ACTION_LEFT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, entries_end, false);
            /* In all other cases, just perform a normal 'right'
             * navigation event */
            else
               return materialui_pointer_up_swipe_horz_default(
                     mui, entry, ptr, selection, entries_end, MENU_ACTION_RIGHT);
         }
         break;
      default:
         /* Ignore input */
         break;
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

   node->line_height        = mui->dip_base_unit_size / 3;
   node->y                  = 0;
   node->has_icon           = false;
   node->icon_texture_index = 0;

   if (settings->bools.menu_materialui_icons_enable)
   {
      switch (type)
      {
         case MENU_SET_CDROM_INFO:
         case MENU_SET_CDROM_LIST:
         case MENU_SET_LOAD_CDROM_LIST:
            node->icon_texture_index = MUI_TEXTURE_DISK;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CORE:
            node->icon_texture_index = MUI_TEXTURE_CORES;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT:
            node->icon_texture_index = MUI_TEXTURE_IMAGE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            node->icon_texture_index = MUI_TEXTURE_PARENT_DIRECTORY;
            node->has_icon           = true;
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            node->icon_texture_index = MUI_TEXTURE_PLAYLIST;
            node->has_icon           = true;
            break;
         case FILE_TYPE_RDB:
            node->icon_texture_index = MUI_TEXTURE_DATABASE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_RDB_ENTRY:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->has_icon           = true;
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
            node->icon_texture_index = MUI_TEXTURE_FILE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_MUSIC:
            node->icon_texture_index = MUI_TEXTURE_MUSIC;
            node->has_icon           = true;
            break;
         case FILE_TYPE_MOVIE:
            node->icon_texture_index = MUI_TEXTURE_VIDEO;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_DOWNLOAD_URL:
            node->icon_texture_index = MUI_TEXTURE_FOLDER;
            node->has_icon           = true;
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
               node->icon_texture_index = MUI_TEXTURE_INFO;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DATABASE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES)))
            {
               node->icon_texture_index = MUI_TEXTURE_IMAGE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC)))
            {
               node->icon_texture_index = MUI_TEXTURE_MUSIC;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO)))
            {
               node->icon_texture_index = MUI_TEXTURE_VIDEO;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_SCAN;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_HISTORY;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_HELP;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESTART;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESUME;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_CLOSE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORE_OPTIONS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORE_CHEAT_OPTIONS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CONTROLS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_SHADERS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORES;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN)))
            {
               node->icon_texture_index = MUI_TEXTURE_RUN;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_FAVORITES;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RENAME_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES)))
            {
               node->icon_texture_index = MUI_TEXTURE_RENAME;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_MIXER;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_START_CORE))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_START_CORE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_STATE;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_EJECT;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DUMP_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISC_INFORMATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DISK;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME)))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_SAVE_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_LOAD_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_SAVE_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_STATE_SLOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_STATE_SLOT;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_TAKE_SCREENSHOT;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_CONFIGURATIONS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_CONTENT;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY)))
            {
               node->icon_texture_index = MUI_TEXTURE_REMOVE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_NETPLAY;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS)))
            {
               node->icon_texture_index = MUI_TEXTURE_QUICKMENU;
               node->has_icon           = true;
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
                     node->icon_texture_index = MUI_TEXTURE_UPDATER;
                     node->has_icon           = true;
                  }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUIT_RETROARCH)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_RETROARCH))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_QUIT;
               node->has_icon           = true;
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
                     node->icon_texture_index = MUI_TEXTURE_SETTINGS;
                     node->has_icon           = true;
                  }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_FOLDER;
               node->has_icon           = true;
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
                     node->icon_texture_index = MUI_TEXTURE_SETTINGS;
                     node->has_icon           = true;
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
