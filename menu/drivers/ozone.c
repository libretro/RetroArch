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

#include <retro_miscellaneous.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <file/file_path.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#include <formats/image.h>
#include <math/float_minmax.h>
#include <array/rhmap.h>

#include "../../config.def.h"
#include "../../file_path_special.h"

#ifdef HAVE_DISCORD_OWN_AVATAR
#include "../../discord/discord.h"
#endif

#include "../menu_cbs.h"
#include "../menu_driver.h"
#include "../menu_screensaver.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos_menu.h"
#endif

#include "../../gfx/gfx_animation.h"
#include "../../gfx/gfx_display.h"
#include "../../gfx/gfx_thumbnail_path.h"
#include "../../gfx/gfx_thumbnail.h"
#include "../../runtime_file.h"

#include "../../input/input_osk.h"

#include "../../configuration.h"
#include "../../audio/audio_driver.h"
#include "../../content.h"
#include "../../core_info.h"

#define ANIMATION_PUSH_ENTRY_DURATION 166.66667f
#define ANIMATION_CURSOR_DURATION     166.66667f
#define ANIMATION_CURSOR_PULSE        166.66667f * 3

#define OZONE_THUMBNAIL_STREAM_DELAY  16.66667f * 5

#define OZONE_EASING_ALPHA            EASING_OUT_CIRC
#define OZONE_EASING_XY               EASING_OUT_QUAD

#define FONT_SIZE_FOOTER              18
#define FONT_SIZE_TITLE               36
#define FONT_SIZE_TIME                22
#define FONT_SIZE_ENTRIES_LABEL       24
#define FONT_SIZE_ENTRIES_SUBLABEL    18
#define FONT_SIZE_SIDEBAR             24

#define HEADER_HEIGHT                 87
#define FOOTER_HEIGHT                 78

#define ENTRY_PADDING_HORIZONTAL_HALF 40
#define ENTRY_PADDING_HORIZONTAL_FULL 140
#define ENTRY_PADDING_VERTICAL        20
#define ENTRY_HEIGHT                  50
#define ENTRY_SPACING                 8
#define ENTRY_ICON_SIZE               46
#define ENTRY_ICON_PADDING            15

/* > 'SIDEBAR_WIDTH' must be kept in sync with
 *   menu driver metrics */
#define SIDEBAR_WIDTH                 408
#define SIDEBAR_X_PADDING             40
#define SIDEBAR_Y_PADDING             20
#define SIDEBAR_ENTRY_HEIGHT          50
#define SIDEBAR_ENTRY_Y_PADDING       10
#define SIDEBAR_ENTRY_ICON_SIZE       46
#define SIDEBAR_ENTRY_ICON_PADDING    15
#define SIDEBAR_GRADIENT_HEIGHT       28

#define FULLSCREEN_THUMBNAIL_PADDING  32

#define CURSOR_SIZE                   64
/* Cursor becomes active when it moves more
 * than CURSOR_ACTIVE_DELTA pixels (adjusted
 * by current scale factor) */
#define CURSOR_ACTIVE_DELTA           3

#define INTERVAL_OSK_CURSOR           (0.5f * 1000000)

#if defined(__APPLE__)
/* UTF-8 support is currently broken on Apple devices... */
#define OZONE_TICKER_SPACER           "   |   "
#else
/* <EM SPACE><BULLET><EM SPACE>
 * UCN equivalent: "\u2003\u2022\u2003" */
#define OZONE_TICKER_SPACER           "\xE2\x80\x83\xE2\x80\xA2\xE2\x80\x83"
#endif

#define OZONE_WIGGLE_DURATION         15
#define OZONE_TAB_MAX_LENGTH          255

/* Returns true if specified entry is currently
 * displayed on screen */
/* Check whether selected item is already on screen */
#define OZONE_ENTRY_ONSCREEN(ozone, idx) (((idx) >= (ozone)->first_onscreen_entry) && ((idx) <= (ozone)->last_onscreen_entry))

enum ozone_onscreen_entry_position_type
{
   OZONE_ONSCREEN_ENTRY_FIRST = 0,
   OZONE_ONSCREEN_ENTRY_LAST,
   OZONE_ONSCREEN_ENTRY_CENTRE
};

enum
{
   OZONE_SYSTEM_TAB_MAIN = 0,
   OZONE_SYSTEM_TAB_SETTINGS,
   OZONE_SYSTEM_TAB_HISTORY,
   OZONE_SYSTEM_TAB_FAVORITES,
   OZONE_SYSTEM_TAB_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   OZONE_SYSTEM_TAB_NETPLAY,
#endif
   OZONE_SYSTEM_TAB_ADD,
   OZONE_SYSTEM_TAB_CONTENTLESS_CORES,
#if defined(HAVE_LIBRETRODB)
   OZONE_SYSTEM_TAB_EXPLORE,
#endif

   /* End of this enum - use the last one to determine num of possible tabs */
   OZONE_SYSTEM_TAB_LAST
};

enum OZONE_TEXTURE
{
   OZONE_TEXTURE_RETROARCH = 0,
   OZONE_TEXTURE_CURSOR_BORDER,
#ifdef HAVE_DISCORD_OWN_AVATAR
   OZONE_TEXTURE_DISCORD_OWN_AVATAR,
#endif
   OZONE_TEXTURE_LAST
};

enum OZONE_THEME_TEXTURES
{
   OZONE_THEME_TEXTURE_SWITCH = 0,
   OZONE_THEME_TEXTURE_CHECK,

   OZONE_THEME_TEXTURE_CURSOR_NO_BORDER,
   OZONE_THEME_TEXTURE_CURSOR_STATIC,

   OZONE_THEME_TEXTURE_LAST
};

enum OZONE_TAB_TEXTURES
{
   OZONE_TAB_TEXTURE_MAIN_MENU = 0,
   OZONE_TAB_TEXTURE_SETTINGS,
   OZONE_TAB_TEXTURE_HISTORY,
   OZONE_TAB_TEXTURE_FAVORITES,
   OZONE_TAB_TEXTURE_MUSIC,
   OZONE_TAB_TEXTURE_VIDEO,
   OZONE_TAB_TEXTURE_IMAGE,
   OZONE_TAB_TEXTURE_NETWORK,
   OZONE_TAB_TEXTURE_SCAN_CONTENT,
   OZONE_TAB_TEXTURE_CONTENTLESS_CORES,
   OZONE_TAB_TEXTURE_EXPLORE,

   OZONE_TAB_TEXTURE_LAST
};

enum
{
   OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU = 0,
   OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS,
   OZONE_ENTRIES_ICONS_TEXTURE_HISTORY,
   OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES,
   OZONE_ENTRIES_ICONS_TEXTURE_MUSICS,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_ENTRIES_ICONS_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_ENTRIES_ICONS_TEXTURE_IMAGES,
#endif
   OZONE_ENTRIES_ICONS_TEXTURE_SETTING,
   OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING,
   OZONE_ENTRIES_ICONS_TEXTURE_ARROW,
   OZONE_ENTRIES_ICONS_TEXTURE_RUN,
   OZONE_ENTRIES_ICONS_TEXTURE_CLOSE,
   OZONE_ENTRIES_ICONS_TEXTURE_RESUME,
   OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE,
   OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE,
   OZONE_ENTRIES_ICONS_TEXTURE_RECORDREPLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_PLAYREPLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_HALTREPLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_UNDO,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO,
   OZONE_ENTRIES_ICONS_TEXTURE_BLUETOOTH,
   OZONE_ENTRIES_ICONS_TEXTURE_WIFI,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST,
   OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT,
   OZONE_ENTRIES_ICONS_TEXTURE_RELOAD,
   OZONE_ENTRIES_ICONS_TEXTURE_RENAME,
   OZONE_ENTRIES_ICONS_TEXTURE_FILE,
   OZONE_ENTRIES_ICONS_TEXTURE_FOLDER,
   OZONE_ENTRIES_ICONS_TEXTURE_ZIP,
   OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE,
   OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE,
   OZONE_ENTRIES_ICONS_TEXTURE_MUSIC,
   OZONE_ENTRIES_ICONS_TEXTURE_IMAGE,
   OZONE_ENTRIES_ICONS_TEXTURE_MOVIE,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE,
   OZONE_ENTRIES_ICONS_TEXTURE_RDB,
   OZONE_ENTRIES_ICONS_TEXTURE_CURSOR,
   OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON,
   OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF,
   OZONE_ENTRIES_ICONS_TEXTURE_CLOCK,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20,
   OZONE_ENTRIES_ICONS_TEXTURE_POINTER,
   OZONE_ENTRIES_ICONS_TEXTURE_ADD,
   OZONE_ENTRIES_ICONS_TEXTURE_DISC,
   OZONE_ENTRIES_ICONS_TEXTURE_KEY,
   OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER,
   OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE,
   OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS,
   OZONE_ENTRIES_ICONS_TEXTURE_AUDIO,
   OZONE_ENTRIES_ICONS_TEXTURE_EXIT,
   OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP,
   OZONE_ENTRIES_ICONS_TEXTURE_INFO,
   OZONE_ENTRIES_ICONS_TEXTURE_HELP,
   OZONE_ENTRIES_ICONS_TEXTURE_NETWORK,
   OZONE_ENTRIES_ICONS_TEXTURE_POWER,
   OZONE_ENTRIES_ICONS_TEXTURE_SAVING,
   OZONE_ENTRIES_ICONS_TEXTURE_UPDATER,
   OZONE_ENTRIES_ICONS_TEXTURE_VIDEO,
   OZONE_ENTRIES_ICONS_TEXTURE_RECORD,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS,
   OZONE_ENTRIES_ICONS_TEXTURE_MIXER,
   OZONE_ENTRIES_ICONS_TEXTURE_LOG,
   OZONE_ENTRIES_ICONS_TEXTURE_OSD,
   OZONE_ENTRIES_ICONS_TEXTURE_UI,
   OZONE_ENTRIES_ICONS_TEXTURE_USER,
   OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY,
   OZONE_ENTRIES_ICONS_TEXTURE_LATENCY,
   OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS,
   OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST,
   OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU,
   OZONE_ENTRIES_ICONS_TEXTURE_REWIND,
   OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE,
   OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_STREAM,
   OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT,
   OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK,
   OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD,
   OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS,
   OZONE_ENTRIES_ICONS_TEXTURE_PAUSE,
   OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_TOGGLE,
   OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BIND_ALL,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_MOUSE,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LGUN,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO,
   OZONE_ENTRIES_ICONS_TEXTURE_LAST
};

enum ozone_pending_thumbnail_type
{
   OZONE_PENDING_THUMBNAIL_NONE = 0,
   OZONE_PENDING_THUMBNAIL_RIGHT,
   OZONE_PENDING_THUMBNAIL_LEFT,
   OZONE_PENDING_THUMBNAIL_BOTH
};

/* Container for a footer text label */
typedef struct
{
   const char *str;
   size_t width;
} ozone_footer_label_t;

typedef struct ozone_theme
{
   /* Background color */
   float background[16];
   float *background_libretro_running;

   /* Float colors for quads and icons */
   float header_footer_separator[16];
   float text[16];
   float selection[16];
   float selection_border[16];
   float entries_border[16];
   float entries_icon[16];
   float text_selected[16];
   float message_background[16];

   /* RGBA colors for text */
   uint32_t text_rgba;
   uint32_t text_sidebar_rgba;
   uint32_t text_selected_rgba;
   uint32_t text_sublabel_rgba;

   /* Screensaver 'tint' (RGB24) */
   uint32_t screensaver_tint;

   /* Sidebar color */
   float *sidebar_background;
   float *sidebar_top_gradient;
   float *sidebar_bottom_gradient;

   /* Fancy cursor colors */
   float *cursor_border_0;
   float *cursor_border_1;

   uintptr_t textures[OZONE_THEME_TEXTURE_LAST];

   const char *name;
} ozone_theme_t;

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
   uint8_t sublabel_lines;    /* Entry sublabel lines */
   bool wrap;                 /* Wrap entry? */
} ozone_node_t;


enum ozone_handle_flags
{
   OZONE_FLAG_IS_DB_MANAGER_LIST              = (1 << 0),
   OZONE_FLAG_IS_EXPLORE_LIST                 = (1 << 1),
   OZONE_FLAG_IS_CONTENTLESS_CORES            = (1 << 2),
   OZONE_FLAG_IS_FILE_LIST                    = (1 << 3),
   OZONE_FLAG_IS_STATE_SLOT                   = (1 << 4),
   OZONE_FLAG_WAS_QUICK_MENU                  = (1 << 5),
   OZONE_FLAG_LIBRETRO_RUNNING                = (1 << 6),
   OZONE_FLAG_FIRST_FRAME                     = (1 << 7),
   OZONE_FLAG_NEED_COMPUTE                    = (1 << 8),
   OZONE_FLAG_DRAW_OLD_LIST                   = (1 << 9 ),
   OZONE_FLAG_HAS_ALL_ASSETS                  = (1 << 10),
   OZONE_FLAG_IS_PLAYLIST                     = (1 << 11),
   OZONE_FLAG_IS_PLAYLIST_OLD                 = (1 << 12),
   OZONE_FLAG_CURSOR_IN_SIDEBAR               = (1 << 13),
   OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD           = (1 << 14),
   /* false = left to right, true = right to left */
   OZONE_FLAG_FADE_DIRECTION                  = (1 << 15),
   OZONE_FLAG_DRAW_SIDEBAR                    = (1 << 16),
   OZONE_FLAG_EMPTY_PLAYLIST                  = (1 << 17),
   /* true = display it, false = don't */
   OZONE_FLAG_OSK_CURSOR                      = (1 << 18),
   OZONE_FLAG_MSGBOX_STATE                    = (1 << 19),
   OZONE_FLAG_MSGBOX_STATE_OLD                = (1 << 20),
   OZONE_FLAG_SHOULD_DRAW_MSGBOX              = (1 << 21),
   OZONE_FLAG_WANT_THUMBNAIL_BAR              = (1 << 22),
   OZONE_FLAG_SKIP_THUMBNAIL_RESET            = (1 << 23),
   OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE = (1 << 24),
   OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR      = (1 << 25),
   OZONE_FLAG_CURSOR_MODE                     = (1 << 26),
   OZONE_FLAG_SHOW_CURSOR                     = (1 << 27),
   OZONE_FLAG_SHOW_SCREENSAVER                = (1 << 28),
   OZONE_FLAG_NO_THUMBNAIL_AVAILABLE          = (1 << 29),
   OZONE_FLAG_FORCE_METADATA_DISPLAY          = (1 << 30)
};

enum ozone_handle_flags2
{
   OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS            = (1 <<  0),
   OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS            = (1 <<  1),
   OZONE_FLAG2_SELECTION_CORE_IS_VIEWER              = (1 <<  2),
   OZONE_FLAG2_SELECTION_CORE_IS_VIEWER_REAL         = (1 <<  3),
   OZONE_FLAG2_POINTER_IN_SIDEBAR                    = (1 <<  4),
   OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR               = (1 <<  5),
   OZONE_FLAG2_CURSOR_WIGGLING                       = (1 <<  6),
   OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME = (1 <<  7)
};

struct ozone_handle
{
   menu_input_pointer_t pointer; /* retro_time_t alignment */

   ozone_theme_t *theme;
   char *pending_message;
   file_list_t selection_buf_old;                  /* ptr alignment */
   file_list_t horizontal_list; /* console tabs */ /* ptr alignment */
   /* Maps console tabs to playlist database names */
   ozone_node_t **playlist_db_node_map;
   menu_screensaver_t *screensaver;

   struct
   {
      font_data_impl_t footer;
      font_data_impl_t title;
      font_data_impl_t time;
      font_data_impl_t entries_label;
      font_data_impl_t entries_sublabel;
      font_data_impl_t sidebar;
   } fonts;

   size_t (*word_wrap)(
         char *dst, size_t dst_size,
         const char *src, size_t src_len,
         int line_width, int wideglyph_width, unsigned max_lines);

   struct
   {
      ozone_footer_label_t ok;
      ozone_footer_label_t back;
      ozone_footer_label_t cycle;
      ozone_footer_label_t search;
      ozone_footer_label_t fullscreen_thumbs;
      ozone_footer_label_t reset_to_default;
      ozone_footer_label_t manage;
      ozone_footer_label_t metadata_toggle;
      ozone_footer_label_t help;
      ozone_footer_label_t clear;
      ozone_footer_label_t scan;
   } footer_labels;

   struct
   {
      gfx_thumbnail_t right;  /* uintptr_t alignment */
      gfx_thumbnail_t left;   /* uintptr_t alignment */
      gfx_thumbnail_t savestate;
      float stream_delay;
      enum ozone_pending_thumbnail_type pending;
   } thumbnails;
   uintptr_t textures[OZONE_THEME_TEXTURE_LAST];
   uintptr_t icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LAST];
   uintptr_t tab_textures[OZONE_TAB_TEXTURE_LAST];

   size_t categories_selection_ptr; /* active tab id  */
   size_t categories_active_idx_old;
   size_t playlist_index;
   size_t tab_selection[OZONE_TAB_MAX_LENGTH];

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
   unsigned old_list_offset_y;

   uint32_t flags;

   float dimensions_sidebar_width; /* animated field */
   float sidebar_offset;
   float last_scale_factor;
   float last_thumbnail_scale_factor;
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

   uint8_t flags2;
   uint8_t selection_lastplayed_lines;
   uint8_t system_tab_end;
   uint8_t tabs[OZONE_SYSTEM_TAB_LAST];
   uint8_t sidebar_index_list[SCROLL_INDEX_SIZE];
   uint8_t sidebar_index_size;

   char title[PATH_MAX_LENGTH];

   char assets_path[PATH_MAX_LENGTH];
   char png_path[PATH_MAX_LENGTH];
   char icons_path[PATH_MAX_LENGTH];
   char icons_path_default[PATH_MAX_LENGTH];
   char tab_path[PATH_MAX_LENGTH];
   char fullscreen_thumbnail_label[255];

   /* These have to be huge, because runloop_st->name.savestate
    * has a hard-coded size of 8192...
    * (the extra space here is required to silence compiler
    * warnings...) */
   char savestate_thumbnail_file_path[8204];
   char prev_savestate_thumbnail_file_path[8204];

   char selection_core_name[255];
   char selection_playtime[255];
   char selection_lastplayed[255];
   char selection_entry_enumeration[255];

   char thumbnails_left_status_prev;
   char thumbnails_right_status_prev;

   bool show_thumbnail_bar;
   bool is_quick_menu;
   bool sidebar_collapsed;
   bool pending_cursor_in_sidebar;

   struct
   {
      retro_time_t start_time;
      float amplitude;
      enum menu_action direction;
   } cursor_wiggle_state;
};

typedef struct ozone_handle ozone_handle_t;

static float ozone_sidebar_gradient_top_light[16]                     = {
   0.94f,  0.94f,  0.94f,  1.00f,
   0.94f,  0.94f,  0.94f,  1.00f,
   0.922f, 0.922f, 0.922f, 1.00f,
   0.922f, 0.922f, 0.922f, 1.00f,
};

static float ozone_sidebar_gradient_bottom_light[16]                  = {
   0.922f, 0.922f, 0.922f, 1.00f,
   0.922f, 0.922f, 0.922f, 1.00f,
   0.94f,  0.94f,  0.94f,  1.00f,
   0.94f,  0.94f,  0.94f,  1.00f,
};

static float ozone_sidebar_gradient_top_dark[16]                      = {
   0.2f,  0.2f,  0.2f,  1.00f,
   0.2f,  0.2f,  0.2f,  1.00f,
   0.18f, 0.18f, 0.18f, 1.00f,
   0.18f, 0.18f, 0.18f, 1.00f,
};

static float ozone_sidebar_gradient_bottom_dark[16]                   = {
   0.18f, 0.18f, 0.18f, 1.00f,
   0.18f, 0.18f, 0.18f, 1.00f,
   0.2f,  0.2f,  0.2f,  1.00f,
   0.2f,  0.2f,  0.2f,  1.00f,
};

static float ozone_sidebar_gradient_top_nord[16]                      = {
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
   0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_nord[16]                   = {
   0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
   0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_gradient_top_gruvbox_dark[16]              = {
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
   0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_gruvbox_dark[16]           = {
   0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
   0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

static float ozone_sidebar_gradient_top_boysenberry[16]               = {
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.19215686274f, 0.0f,           0.04705882352f, 1.00f,
   0.19215686274f, 0.0f,           0.04705882352f, 1.00f,
};

static float ozone_sidebar_gradient_bottom_boysenberry[16]            = {
   0.19215686274f, 0.0f,           0.04705882352f, 1.00f,
   0.19215686274f, 0.0f,           0.04705882352f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
};

static float ozone_sidebar_gradient_top_hacking_the_kernel[16]        = {
   0.0f, 0.13333333f, 0.0f, 1.0f,
   0.0f, 0.13333333f, 0.0f, 1.0f,
   0.0f, 0.13333333f, 0.0f, 1.0f,
   0.0f, 0.13333333f, 0.0f, 1.0f,
};

static float ozone_sidebar_gradient_bottom_hacking_the_kernel[16]     = {
   0.0f, 0.0666666f,  0.0f, 1.0f,
   0.0f, 0.0666666f,  0.0f, 1.0f,
   0.0f, 0.13333333f, 0.0f, 1.0f,
   0.0f, 0.13333333f, 0.0f, 1.0f,
};

static float ozone_sidebar_gradient_top_twilight_zone[16]             = {
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
};

static float ozone_sidebar_gradient_bottom_twilight_zone[16]          = {
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
};

static float ozone_sidebar_gradient_top_dracula[16]                   = {
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
};

static float ozone_sidebar_gradient_bottom_dracula[16]                = {
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
};

static float ozone_sidebar_gradient_top_solarized_dark[16]            = {
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
};

static float ozone_sidebar_gradient_bottom_solarized_dark[16]         = {
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
};

static float ozone_sidebar_gradient_top_solarized_light[16]           = {
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
};

static float ozone_sidebar_gradient_bottom_solarized_light[16]        = {
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
};

static float ozone_sidebar_background_gray_dark[16]                   =
   COLOR_HEX_TO_FLOAT(0x101010, 0.0f);

static float ozone_sidebar_background_gray_light[16]                  =
   COLOR_HEX_TO_FLOAT(0x303030, 0.0f);

static float ozone_sidebar_background_light[16]                       = {
   0.94f, 0.94f, 0.94f, 1.00f,
   0.94f, 0.94f, 0.94f, 1.00f,
   0.94f, 0.94f, 0.94f, 1.00f,
   0.94f, 0.94f, 0.94f, 1.00f,
};

static float ozone_sidebar_background_dark[16]                        = {
   0.2f, 0.2f, 0.2f, 1.00f,
   0.2f, 0.2f, 0.2f, 1.00f,
   0.2f, 0.2f, 0.2f, 1.00f,
   0.2f, 0.2f, 0.2f, 1.00f,
};

static float ozone_sidebar_background_nord[16]                        = {
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
   0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_background_gruvbox_dark[16]                = {
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
   0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

static float ozone_sidebar_background_boysenberry[16]                 = {
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 1.00f,
};

static float ozone_sidebar_background_hacking_the_kernel[16]          = {
   0.0f, 0.1333333f, 0.0f, 1.0f,
   0.0f, 0.1333333f, 0.0f, 1.0f,
   0.0f, 0.1333333f, 0.0f, 1.0f,
   0.0f, 0.1333333f, 0.0f, 1.0f,
};

static float ozone_sidebar_background_twilight_zone[16]               = {
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
};

static float ozone_sidebar_background_dracula[16]                     = {
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
   0.2666666f, 0.2784314f, 0.3529412f, 1.0f,
};

static float ozone_sidebar_background_solarized_dark[16]              = {
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
};

static float ozone_sidebar_background_solarized_light[16]             = {
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
};

static float ozone_sidebar_background_purple_rain[16] = {
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
};

static float ozone_background_libretro_running_gray_dark[16]          =
   COLOR_HEX_TO_FLOAT(0x101010, 1.0f);

static float ozone_background_libretro_running_gray_light[16]         =
   COLOR_HEX_TO_FLOAT(0x303030, 1.0f);

static float ozone_background_libretro_running_light[16]              = {
   0.690f, 0.690f, 0.690f, 0.75f,
   0.690f, 0.690f, 0.690f, 0.75f,
   0.922f, 0.922f, 0.922f, 1.0f,
   0.922f, 0.922f, 0.922f, 1.0f
};

static float ozone_background_libretro_running_dark[16]               = {
   0.176f, 0.176f, 0.176f, 0.75f,
   0.176f, 0.176f, 0.176f, 0.75f,
   0.178f, 0.178f, 0.178f, 1.0f,
   0.178f, 0.178f, 0.178f, 1.0f,
};

static float ozone_background_libretro_running_nord[16]               = {
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
};

static float ozone_background_libretro_running_gruvbox_dark[16]       = {
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
};

static float ozone_background_libretro_running_boysenberry[16]        = {
   0.27058823529f, 0.09803921568f, 0.14117647058f, 0.75f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 0.75f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 0.75f,
   0.27058823529f, 0.09803921568f, 0.14117647058f, 0.75f,
};

static float ozone_background_libretro_running_hacking_the_kernel[16] = {
   0.0f, 0.0666666f, 0.0f, 0.75f,
   0.0f, 0.0666666f, 0.0f, 0.75f,
   0.0f, 0.0666666f, 0.0f, 1.0f,
   0.0f, 0.0666666f, 0.0f, 1.0f,
};

static float ozone_background_libretro_running_twilight_zone[16]      = {
   0.0078431f, 0.0f, 0.0156862f, 0.75f,
   0.0078431f, 0.0f, 0.0156862f, 0.75f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
   0.0078431f, 0.0f, 0.0156862f, 1.0f,
};

static float ozone_background_libretro_running_dracula[16]            = {
   0.1568627f, 0.1647058f, 0.2117647f, 0.75f,
   0.1568627f, 0.1647058f, 0.2117647f, 0.75f,
   0.1568627f, 0.1647058f, 0.2117647f, 1.0f,
   0.1568627f, 0.1647058f, 0.2117647f, 1.0f,
};

static float ozone_background_libretro_running_solarized_dark[16]     = {
   0.0000000f, 0.1294118f, 0.1725490f, .85f,
   0.0000000f, 0.1294118f, 0.1725490f, .85f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
   0.0000000f, 0.1294118f, 0.1725490f, 1.0f,
};

static float ozone_background_libretro_running_solarized_light[16]    = {
   1.0000000f, 1.0000000f, 0.9294118f, 0.85f,
   1.0000000f, 1.0000000f, 0.9294118f, 0.85f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
   1.0000000f, 1.0000000f, 0.9294118f, 1.0f,
};

static float ozone_background_libretro_running_purple_rain[16] = {
   0.0862745f, 0.0f, 0.1294117f, 0.75f,
   0.0862745f, 0.0f, 0.1294117f, 0.75f,
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
   0.0862745f, 0.0f, 0.1294117f, 1.0f,
};

static float ozone_border_gray[16]                 = COLOR_HEX_TO_FLOAT(0x303030, 1.0f);

static float ozone_border_0_light[16]              = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_light[16]              = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_dark[16]               = COLOR_HEX_TO_FLOAT(0x198AC6, 1.00);
static float ozone_border_1_dark[16]               = COLOR_HEX_TO_FLOAT(0x89F1F2, 1.00);

static float ozone_border_0_nord[16]               = COLOR_HEX_TO_FLOAT(0x5E81AC, 1.0f);
static float ozone_border_1_nord[16]               = COLOR_HEX_TO_FLOAT(0x88C0D0, 1.0f);

static float ozone_border_0_gruvbox_dark[16]       = COLOR_HEX_TO_FLOAT(0xAF3A03, 1.0f);
static float ozone_border_1_gruvbox_dark[16]       = COLOR_HEX_TO_FLOAT(0xFE8019, 1.0f);

static float ozone_border_0_boysenberry[16]        = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_boysenberry[16]        = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x008C00, 1.0f);
static float ozone_border_1_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x00E000, 1.0f);

static float ozone_border_0_twilight_zone[16]      = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_twilight_zone[16]      = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);

static float ozone_border_0_dracula[16]            = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_dracula[16]            = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);

static float ozone_border_0_solarized_dark[16]     = COLOR_HEX_TO_FLOAT(0x67ECE2, 1.0f);
static float ozone_border_1_solarized_dark[16]     = COLOR_HEX_TO_FLOAT(0x2AA198, 1.0f);

static float ozone_border_0_solarized_light[16]    = COLOR_HEX_TO_FLOAT(0x8F120F, 1.0f);
static float ozone_border_1_solarized_light[16]    = COLOR_HEX_TO_FLOAT(0xDC322F, 1.0f);

static float ozone_border_0_purple_rain[16]        = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_purple_rain[16]        = COLOR_HEX_TO_FLOAT(0x8C3DCC, 1.0f);

static ozone_theme_t ozone_theme_light = {
   COLOR_HEX_TO_FLOAT(0xEBEBEB, 1.00f),                  /* background */
   ozone_background_libretro_running_light,              /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0x2B2B2B, 1.00f),                  /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00f),                  /* text */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00f),                  /* selection */
   COLOR_HEX_TO_FLOAT(0x10BEC5, 1.00f),                  /* selection_border */
   COLOR_HEX_TO_FLOAT(0xCDCDCD, 1.00f),                  /* entries_border */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00f),                  /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x374CFF, 1.00f),                  /* text_selected */
   COLOR_HEX_TO_FLOAT(0xF0F0F0, 1.00f),                  /* message_background */

   0x333333FF,                                           /* text_rgba */
   0x333333FF,                                           /* text_sidebar_rgba */
   0x374CFFFF,                                           /* text_selected_rgba */
   0x878787FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xEBEBEB,                                             /* screensaver_tint */

   ozone_sidebar_background_light,                       /* sidebar_background */
   ozone_sidebar_gradient_top_light,                     /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_light,                  /* sidebar_bottom_gradient */

   ozone_border_0_light,                                 /* cursor_border_0 */
   ozone_border_1_light,                                 /* cursor_border_1 */

   {0},                                                  /* textures */

   "light"                                               /* name */
};

static ozone_theme_t ozone_theme_dark = {
   COLOR_HEX_TO_FLOAT(0x2D2D2D, 1.00f),                  /* background */
   ozone_background_libretro_running_dark,               /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00f),                  /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00f),                  /* text */
   COLOR_HEX_TO_FLOAT(0x212227, 1.00f),                  /* selection */
   COLOR_HEX_TO_FLOAT(0x2DA3CB, 1.00f),                  /* selection_border */
   COLOR_HEX_TO_FLOAT(0x51514F, 1.00f),                  /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00f),                  /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x00D9AE, 1.00f),                  /* text_selected */
   COLOR_HEX_TO_FLOAT(0x464646, 1.00f),                  /* message_background */

   0xFFFFFFFF,                                           /* text_rgba */
   0xFFFFFFFF,                                           /* text_sidebar_rgba */
   0x00FFC5FF,                                           /* text_selected_rgba */
   0x9F9FA1FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFFFFFF,                                             /* screensaver_tint */

   ozone_sidebar_background_dark,                        /* sidebar_background */
   ozone_sidebar_gradient_top_dark,                      /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_dark,                   /* sidebar_bottom_gradient */

   ozone_border_0_dark,                                  /* cursor_border_0 */
   ozone_border_1_dark,                                  /* cursor_border_1 */

   {0},                                                  /* textures */

   "dark"                                                /* name */
};

static ozone_theme_t ozone_theme_nord = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x2E3440, 1.0f),                   /* background */
   ozone_background_libretro_running_nord,               /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD8DEE9, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xECEFF4, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x232730, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x73A1BE, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x4C566A, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xE5E9F0, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xA9C791, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x434C5E, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xECEFF4FF,                                           /* text_rgba */
   0xECEFF4FF,                                           /* text_sidebar_rgba */
   0xA9C791FF,                                           /* text_selected_rgba */
   0x8FBCBBFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xECEFF4,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_nord,                        /* sidebar_background */
   ozone_sidebar_gradient_top_nord,                      /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_nord,                   /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_nord,                                  /* cursor_border_0 */
   ozone_border_1_nord,                                  /* cursor_border_1 */

   {0},                                                  /* textures */

   "nord"                                                /* name */
};

static ozone_theme_t ozone_theme_gruvbox_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282828, 1.0f),                   /* background */
   ozone_background_libretro_running_gruvbox_dark,       /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD5C4A1, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x1D2021, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xD75D0E, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x665C54, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x8EC07C, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x32302F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xEBDBB2FF,                                           /* text_rgba */
   0xEBDBB2FF,                                           /* text_sidebar_rgba */
   0x8EC07CFF,                                           /* text_selected_rgba */
   0xD79921FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xEBDBB2,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_gruvbox_dark,                /* sidebar_background */
   ozone_sidebar_gradient_top_gruvbox_dark,              /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_gruvbox_dark,           /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_gruvbox_dark,                          /* cursor_border_0 */
   ozone_border_1_gruvbox_dark,                          /* cursor_border_1 */

   {0},                                                  /* textures */

   "gruvbox_dark"                                        /* name */
};

static ozone_theme_t ozone_theme_boysenberry = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x31000C, 1.0f),                   /* background */
   ozone_background_libretro_running_boysenberry,        /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x85535F, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x4E2A35, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xD599FF, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x73434C, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFEBCFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xD599FF, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x32302F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xFEBCFFFF,                                           /* text_rgba */
   0xFEBCFFFF,                                           /* text_sidebar_rgba */
   0xFEBCFFFF,                                           /* text_selected_rgba */
   0xD599FFFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFEBCFF,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_boysenberry,                 /* sidebar_background */
   ozone_sidebar_gradient_top_boysenberry,               /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_boysenberry,            /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_boysenberry,                           /* cursor_border_0 */
   ozone_border_1_boysenberry,                           /* cursor_border_1 */

   {0},                                                  /* textures */

   "boysenberry"                                         /* name */
};

static ozone_theme_t ozone_theme_hacking_the_kernel = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x001100, 1.0f),                   /* background */
   ozone_background_libretro_running_hacking_the_kernel, /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x17C936, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x00FF29, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x003400, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x1BDA3C, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x008C00, 0.1f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0x00FF00, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x8EC07C, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x0D0E0F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0x00E528FF,                                           /* text_rgba */
   0x00E528FF,                                           /* text_sidebar_rgba */
   0x83FF83FF,                                           /* text_selected_rgba */
   0x53E63DFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0x00E528,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_hacking_the_kernel,          /* sidebar_background */
   ozone_sidebar_gradient_top_hacking_the_kernel,        /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_hacking_the_kernel,     /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_hacking_the_kernel,                    /* cursor_border_0 */
   ozone_border_1_hacking_the_kernel,                    /* cursor_border_1 */

   {0},                                                  /* textures */

   "hacking_the_kernel"                                  /* name */
};

static ozone_theme_t ozone_theme_twilight_zone = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x020004, 1.0f),                   /* background */
   ozone_background_libretro_running_twilight_zone,      /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x5B5069, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xF7F0FA, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x232038, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xC27AFF, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xB78CC8, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0xB78CC8, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xFDFCFEFF,                                           /* text_rgba */
   0xFDFCFEFF,                                           /* text_sidebar_rgba */
   0xB78CC8FF,                                           /* text_selected_rgba */
   0x9A6C99FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFDFCFE,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_twilight_zone,               /* sidebar_background */
   ozone_sidebar_gradient_top_twilight_zone,             /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_twilight_zone,          /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_twilight_zone,                         /* cursor_border_0 */
   ozone_border_1_twilight_zone,                         /* cursor_border_1 */

   {0},                                                  /* textures */

   "twilight_zone"                                       /* name */
};

static ozone_theme_t ozone_theme_dracula = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282A36, 1.0f),                   /* background */
   ozone_background_libretro_running_dracula,            /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x282A36, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0xFF79C6, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xFF79C6, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x282A36, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xFF79C6, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x6272A4, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xF8F8F2FF,                                           /* text_rgba */
   0xF8F8F2FF,                                           /* text_sidebar_rgba */
   0xFF79C6FF,                                           /* text_selected_rgba */
   0x6272A4FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xF8F8F2,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_dracula,                     /* sidebar_background */
   ozone_sidebar_gradient_top_dracula,                   /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_dracula,                /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_dracula,                               /* cursor_border_0 */
   ozone_border_1_dracula,                               /* cursor_border_1 */

   {0},                                                  /* textures */

   "dracula"                                             /* name */
};

static ozone_theme_t ozone_theme_solarized_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x002B36, 1.0f),                   /* background */
   ozone_background_libretro_running_solarized_dark,     /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x839496, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x93A1A1, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x073642, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x2AA198, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x073642, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0x268BD2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x93A1A1, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x002B36, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0x93A1A1FF,                                           /* text_rgba */
   0x93A1A1FF,                                           /* text_sidebar_rgba */
   0x2AA198FF,                                           /* text_selected_rgba */
   0x657B83FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0x073642,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_solarized_dark,              /* sidebar_background */
   ozone_sidebar_gradient_top_solarized_dark,            /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_solarized_dark,         /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_solarized_dark,                        /* cursor_border_0 */
   ozone_border_1_solarized_dark,                        /* cursor_border_1 */

   {0},                                                  /* textures */

   "solarized_dark"                                      /* name */
};

static ozone_theme_t ozone_theme_solarized_light = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0xFDF6E3, 1.0f),                   /* background */
   ozone_background_libretro_running_solarized_light,    /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x657B83, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x586E75, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0xEEE8D5, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xDC322F, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xEEE8D5, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xCB4B16, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x586E75, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0xFDF6E3, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0x586E75FF,                                           /* text_rgba */
   0x586E75FF,                                           /* text_sidebar_rgba */
   0xDC322FFF,                                           /* text_selected_rgba */
   0x839496FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xEEE8D5,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_solarized_light,             /* sidebar_background */
   ozone_sidebar_gradient_top_solarized_light,           /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_solarized_light,        /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_solarized_light,                       /* cursor_border_0 */
   ozone_border_1_solarized_light,                       /* cursor_border_1 */

   {0},                                                  /* textures */

   "solarized_light"                                     /* name */
};

static ozone_theme_t ozone_theme_gray_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x101010, 1.0f),                   /* background */
   ozone_background_libretro_running_gray_dark,          /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x303030, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x303030, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x181818, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x202020, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xC0C0C0FF,                                           /* text_rgba */
   0x808080FF,                                           /* text_sidebar_rgba */
   0xFFFFFFFF,                                           /* text_selected_rgba */
   0x707070FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFFFFFF,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_gray_dark,                   /* sidebar_background */
   ozone_sidebar_background_gray_dark,                   /* sidebar_top_gradient */
   ozone_sidebar_background_gray_dark,                   /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_gray,                                    /* cursor_border_0 */
   ozone_border_gray,                                    /* cursor_border_1 */

   {0},                                                  /* textures */

   /* No theme assets */
   NULL,                                                 /* name */
};

static ozone_theme_t ozone_theme_gray_light = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x303030, 1.0f),                   /* background */
   ozone_background_libretro_running_gray_light,         /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x000000, 0.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x101010, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x101010, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x282828, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x202020, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xC0C0C0FF,                                           /* text_rgba */
   0x808080FF,                                           /* text_sidebar_rgba */
   0xFFFFFFFF,                                           /* text_selected_rgba */
   0x707070FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFFFFFF,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_gray_light,                  /* sidebar_background */
   ozone_sidebar_background_gray_light,                  /* sidebar_top_gradient */
   ozone_sidebar_background_gray_light,                  /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_gray,                                    /* cursor_border_0 */
   ozone_border_gray,                                    /* cursor_border_1 */

   {0},                                                  /* textures */

   /* No theme assets */
   NULL,                                                 /* name */
};

static ozone_theme_t ozone_theme_purple_rain = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x160021, 1.0f),                   /* background */
   ozone_background_libretro_running_purple_rain,        /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xAA00CC, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x660099, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x660099, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xAA00CC, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x660099, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xFFFFFFFF,                                           /* text_rgba */
   0xFFFFFFFF,                                           /* text_sidebar_rgba */
   0xFFFFFFFF,                                           /* text_selected_rgba */
   0xFFFFFFFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFFFFFF,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_purple_rain,                 /* sidebar_background */
   ozone_sidebar_background_purple_rain,                 /* sidebar_top_gradient */
   ozone_sidebar_background_purple_rain,                 /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_purple_rain,                           /* cursor_border_0 */
   ozone_border_1_purple_rain,                           /* cursor_border_1 */

   {0},                                                  /* textures */

   /* No theme assets */
   "purple_rain"                                         /* name */
};

static ozone_theme_t *ozone_themes[] = {
   &ozone_theme_light,
   &ozone_theme_dark,
   &ozone_theme_nord,
   &ozone_theme_gruvbox_dark,
   &ozone_theme_boysenberry,
   &ozone_theme_hacking_the_kernel,
   &ozone_theme_twilight_zone,
   &ozone_theme_dracula,
   &ozone_theme_solarized_dark,
   &ozone_theme_solarized_light,
   &ozone_theme_gray_dark,
   &ozone_theme_gray_light,
   &ozone_theme_purple_rain
};

/* TODO/FIXME - global variables referenced outside */
static unsigned ozone_last_color_theme      = 0;
static ozone_theme_t *ozone_default_theme   = &ozone_theme_dark; /* also used as a tag for cursor animation */
/* Enable runtime configuration of framebuffer opacity */
static float ozone_last_framebuffer_opacity = -1.0f;

/* Forward declarations */
static void ozone_cursor_animation_cb(void *userdata);
static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation);
static void ozone_unload_thumbnail_textures(void *data);

static INLINE uint8_t ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   uint8_t lines  = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static void ozone_animate_cursor(ozone_handle_t *ozone,
      float *dst,
      float *target)
{
   int i;
   gfx_animation_ctx_entry_t entry;

   entry.easing_enum = OZONE_EASING_XY;
   entry.tag         = (uintptr_t)&ozone_default_theme;
   entry.duration    = ANIMATION_CURSOR_PULSE;
   entry.userdata    = ozone;

   for (i = 0; i < 16; i++)
   {
      if (i == 3 || i == 7 || i == 11 || i == 15)
         continue;

      if (i == 14)
         entry.cb = ozone_cursor_animation_cb;
      else
         entry.cb = NULL;

      entry.subject        = &dst[i];
      entry.target_value   = target[i];

      gfx_animation_push(&entry);
   }
}

static void ozone_cursor_animation_cb(void *userdata)
{
   float *target         = NULL;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   switch (ozone->theme_dynamic_cursor_state)
   {
      case 0:
         target = ozone->theme->cursor_border_1;
         break;
      case 1:
         target = ozone->theme->cursor_border_0;
         break;
   }

   ozone->theme_dynamic_cursor_state = (ozone->theme_dynamic_cursor_state + 1) % 2;

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, target);
}

static void ozone_restart_cursor_animation(ozone_handle_t *ozone)
{
   uintptr_t tag = (uintptr_t)&ozone_default_theme;

   ozone->theme_dynamic_cursor_state = 1;
   memcpy(ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_0,
         sizeof(ozone->theme_dynamic.cursor_border));
   gfx_animation_kill_by_tag(&tag);

   ozone_animate_cursor(ozone,
         ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_1);
}

static void ozone_set_color_theme(
      ozone_handle_t *ozone,
      unsigned color_theme)
{
   ozone_theme_t *theme = ozone_default_theme;

   switch (color_theme)
   {
      case OZONE_COLOR_THEME_BASIC_WHITE:
         theme = &ozone_theme_light;
         break;
      case OZONE_COLOR_THEME_BASIC_BLACK:
         theme = &ozone_theme_dark;
         break;
      case OZONE_COLOR_THEME_NORD:
         theme = &ozone_theme_nord;
         break;
      case OZONE_COLOR_THEME_GRUVBOX_DARK:
         theme = &ozone_theme_gruvbox_dark;
         break;
      case OZONE_COLOR_THEME_BOYSENBERRY:
         theme = &ozone_theme_boysenberry;
         break;
      case OZONE_COLOR_THEME_HACKING_THE_KERNEL:
         theme = &ozone_theme_hacking_the_kernel;
         break;
      case OZONE_COLOR_THEME_TWILIGHT_ZONE:
         theme = &ozone_theme_twilight_zone;
         break;
      case OZONE_COLOR_THEME_DRACULA:
         theme = &ozone_theme_dracula;
         break;
      case OZONE_COLOR_THEME_SOLARIZED_DARK:
         theme = &ozone_theme_solarized_dark;
         break;
      case OZONE_COLOR_THEME_SOLARIZED_LIGHT:
         theme = &ozone_theme_solarized_light;
         break;
      case OZONE_COLOR_THEME_GRAY_DARK:
         theme = &ozone_theme_gray_dark;
         break;
      case OZONE_COLOR_THEME_GRAY_LIGHT:
         theme = &ozone_theme_gray_light;
         break;
      case OZONE_COLOR_THEME_PURPLE_RAIN:
         theme = &ozone_theme_purple_rain;
         break;
      default:
         break;
   }

   ozone->theme = theme;

   memcpy(ozone->theme_dynamic.selection_border,
         ozone->theme->selection_border,
         sizeof(ozone->theme_dynamic.selection_border));
   memcpy(ozone->theme_dynamic.selection,
         ozone->theme->selection,
         sizeof(ozone->theme_dynamic.selection));
   memcpy(ozone->theme_dynamic.entries_border,
         ozone->theme->entries_border,
         sizeof(ozone->theme_dynamic.entries_border));
   memcpy(ozone->theme_dynamic.entries_icon,
         ozone->theme->entries_icon,
         sizeof(ozone->theme_dynamic.entries_icon));
   memcpy(ozone->theme_dynamic.entries_checkmark,
         ozone->pure_white,
         sizeof(ozone->theme_dynamic.entries_checkmark));
   memcpy(ozone->theme_dynamic.cursor_alpha,
         ozone->pure_white,
         sizeof(ozone->theme_dynamic.cursor_alpha));
   memcpy(ozone->theme_dynamic.message_background,
         ozone->theme->message_background,
         sizeof(ozone->theme_dynamic.message_background));

   if (ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS)
      ozone_restart_cursor_animation(ozone);

   ozone_last_color_theme = color_theme;
}

static unsigned ozone_get_system_theme(void)
{
#ifdef HAVE_LIBNX
   if (R_SUCCEEDED(setsysInitialize()))
   {
      ColorSetId theme;
      unsigned ret = 0;
      setsysGetColorSetId(&theme);
      if (theme == ColorSetId_Dark)
         ret = 1;
      setsysExit();
      return ret;
   }
   return 0;
#else
   return DEFAULT_OZONE_COLOR_THEME;
#endif
}

/* Running background gradient disabled for now due to
 * rather jarring steps with certain themes because they
 * already have background gradient */
static void ozone_set_background_running_opacity(
      ozone_handle_t *ozone,
      float framebuffer_opacity)
{
#if USE_BG_GRADIENT
   static float background_running_alpha_top    = 1.0f;
   static float background_running_alpha_bottom = 0.75f;
   float *background                            =
         ozone->theme->background_libretro_running;

   /* When content is running, background is a
    * gradient that from top to bottom transitions
    * from maximum to minimum opacity
    * > RetroArch default 'framebuffer_opacity'
    *   is 0.900. At this setting:
    *   - Background top has an alpha of 1.0
    *   - Background bottom has an alpha of 0.75 */
   background_running_alpha_top                 = framebuffer_opacity / 0.9f;
   background_running_alpha_top                 = (background_running_alpha_top > 1.0f)
         ?  1.0f
         : (background_running_alpha_top < 0.0f)
               ?  0.0f
               : background_running_alpha_top;

   background_running_alpha_bottom              = (2.5f * framebuffer_opacity) - 1.5f;
   background_running_alpha_bottom              = (background_running_alpha_bottom > 1.0f)
         ?  1.0f
         : (background_running_alpha_bottom < 0.0f)
               ?  0.0f
               : background_running_alpha_bottom;

   background[11]                               = background_running_alpha_top;
   background[15]                               = background_running_alpha_top;
   background[3]                                = background_running_alpha_bottom;
   background[7]                                = background_running_alpha_bottom;
#else
   float *background                            =
         ozone->theme->background_libretro_running;

   background[11]                               = framebuffer_opacity;
   background[15]                               = framebuffer_opacity;
   background[3]                                = framebuffer_opacity;
   background[7]                                = framebuffer_opacity;
#endif

   ozone_last_framebuffer_opacity               = framebuffer_opacity;

   /* Set sidebar background to half opacity if transparent */
   if (ozone->theme->sidebar_background[3] > 0)
   {
      gfx_display_set_alpha(ozone->theme->sidebar_top_gradient, 0.5f);
      gfx_display_set_alpha(ozone->theme->sidebar_background, 0.5f);
      gfx_display_set_alpha(ozone->theme->sidebar_bottom_gradient, 0.5f);
   }
}

static uintptr_t ozone_entries_icon_get_texture(
      ozone_handle_t *ozone,
      enum msg_hash_enums enum_idx,
      const char *enum_path,
      const char *enum_label,
      unsigned type,
      bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_LOAD_DISC:
      case MENU_ENUM_LABEL_DUMP_DISC:
#ifdef HAVE_LAKKA
      case MENU_ENUM_LABEL_EJECT_DISC:
#endif
      case MENU_ENUM_LABEL_DISC_INFORMATION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISC];
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE:
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_TRAY_EJECT:
      case MENU_ENUM_LABEL_DISK_TRAY_INSERT:
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
      case MENU_ENUM_LABEL_DISK_INDEX:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_STATE_SLOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_REPLAY_SLOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_SAVESTATE_LIST:
      case MENU_ENUM_LABEL_SAVE_STATE:
      case MENU_ENUM_LABEL_CORE_CREATE_BACKUP:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
      case MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
      case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];

      /* Menu collection submenus */
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ZIP];
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE];
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MUSIC];
      case MENU_ENUM_LABEL_GOTO_EXPLORE:
         if (!string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];

      /* Menu icons */
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_CORE:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES:
      case MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_CORE_INFORMATION:
            if (!string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION)))
               return 0;
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_FILE:
      case MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UPDATER];
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
      case MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY];
      case MENU_ENUM_LABEL_OSK_OVERLAY_SETTINGS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_INFORMATION:
      case MENU_ENUM_LABEL_INFORMATION_LIST:
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INFO];
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
      case MENU_ENUM_LABEL_RDB_ENTRY_DETAIL:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
      case MENU_ENUM_LABEL_HELP_LIST:
      case MENU_ENUM_LABEL_HELP_CONTROLS:
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_EXIT];

      /* Settings icons */
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_DRIVERS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS];
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_VIDEO:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_VIDEO];
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_AUDIO:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_AUDIO];
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MIXER];
      case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_CONFIGURATION:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_FILE_BROWSER:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_ACCESSIBILITY:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING];
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING];
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_INPUT:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS:
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
      case MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
      case MENU_ENUM_LABEL_START_NET_RETROPAD:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
      case MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO];
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
      case MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_LATENCY:
      case MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE:
      case MENU_ENUM_LABEL_VIDEO_FRAME_REST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_SAVING:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_REMAP_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
      case MENU_ENUM_LABEL_OVERRIDE_FILE_SAVE_AS:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_AS:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_REPLAY:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_LOGGING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE:
      case MENU_ENUM_LABEL_FASTFORWARD_FRAMESKIP:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP];
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RECORD];
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_STREAM];
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
      case MENU_ENUM_LABEL_CHEAT_DELETE:
      case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
      case MENU_ENUM_LABEL_CORE_DELETE:
      case MENU_ENUM_LABEL_DELETE_PLAYLIST:
      case MENU_ENUM_LABEL_CORE_DELETE_BACKUP_LIST:
      case MENU_ENUM_LABEL_VIDEO_FILTER_REMOVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME:
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN_REMOVE:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE:
      case MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
      case MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_CORE_OPTIONS_RESET:
      case MENU_ENUM_LABEL_REMAP_FILE_RESET:
      case MENU_ENUM_LABEL_OVERRIDE_UNLOAD:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH:
      case MENU_ENUM_LABEL_REMAP_FILE_FLUSH:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case MENU_ENUM_LABEL_CORE_LOCK:
      case MENU_ENUM_LABEL_CORE_SET_STANDALONE_EXEMPT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UI];
#if defined(HAVE_LIBNX)
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
      case MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS:
      case MENU_ENUM_LABEL_NETPLAY_LOBBY_FILTERS:
      case MENU_ENUM_LABEL_NETPLAY_KICK:
      case MENU_ENUM_LABEL_NETPLAY_BAN:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NETWORK];
      case MENU_ENUM_LABEL_BLUETOOTH_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_BLUETOOTH];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_USER:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_USER];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY:
      case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY];
      case MENU_ENUM_LABEL_REWIND_SETTINGS:
      case MENU_ENUM_LABEL_CONTENT_SHOW_REWIND:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_REWIND];
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES:
      case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE];
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
      case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS];
#ifdef HAVE_NETWORKING
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM];
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
#ifdef HAVE_NETPLAYDISCOVERY
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
#endif
#endif
      case MENU_ENUM_LABEL_REBOOT:
      case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
      case MENU_ENUM_LABEL_CHEAT_COPY_AFTER:
      case MENU_ENUM_LABEL_CHEAT_COPY_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
      case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST:
      case MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            /* Only show icon in Throttle settings */
            if (ozone->depth < 3)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
            break;
      case MENU_ENUM_LABEL_SHUTDOWN:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN];
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_OVERRIDE_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK];
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_TOGGLE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG];
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
#ifdef HAVE_LIBRETRODB
      case MENU_ENUM_LABEL_EXPLORE_ITEM:
      {
         uintptr_t icon = menu_explore_get_entry_icon(type);
         if (icon)
            return icon;
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
         else if (type != FILE_TYPE_RDB)
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
         break;
      }
#endif
      case MENU_ENUM_LABEL_CONTENTLESS_CORE:
      {
         uintptr_t icon = menu_contentless_cores_get_entry_icon(enum_label);
         if (icon)
            return icon;
         break;
      }
      /* No icon */
      case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
         return 0;
      default:
         break;
   }

   switch (type)
   {
      case MENU_SET_CDROM_INFO:
      case MENU_SET_CDROM_LIST:
#ifdef HAVE_LAKKA
      case MENU_SET_EJECT_DISC:
#endif
      case MENU_SET_LOAD_CDROM_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISC];
      case FILE_TYPE_DIRECTORY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
      case FILE_TYPE_RPL_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case FILE_TYPE_SHADER:
      case FILE_TYPE_SHADER_PRESET:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case FILE_TYPE_CARCHIVE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case FILE_TYPE_PLAYLIST_COLLECTION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case FILE_TYPE_RDB:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case FILE_TYPE_RDB_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_SETTING_ACTION_CLOSE:
      case MENU_SETTING_ACTION_CLOSE_HORIZONTAL:
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_SETTING_ACTION_PLAYREPLAY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PLAYREPLAY];
      case MENU_SETTING_ACTION_RECORDREPLAY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RECORDREPLAY];
      case MENU_SETTING_ACTION_HALTREPLAY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_HALTREPLAY];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         if (string_starts_with(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_VIDEO];
         else if (string_starts_with(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS)) ||
                  string_starts_with(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_AUDIO];
         else if (string_starts_with(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS)) ||
                  string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LATENCY];
         else if (string_starts_with(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS)) ||
                  string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS)) ||
                  string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS];
         else if (strstr(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
         else if (strstr(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE];
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_SETTING_ACTION_CORE_OPTION_OVERRIDE_LIST:
      case MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_RESET:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PAUSE];
      case MENU_SETTING_GROUP:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_SET_SCREEN_BRIGHTNESS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS];
      case MENU_INFO_MESSAGE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      case MENU_BLUETOOTH:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_BLUETOOTH];
      case MENU_WIFI:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_WIFI];
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM];
      case MENU_ROOM_LAN:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN];
      case MENU_ROOM_RELAY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY];
#endif
      case MENU_SETTING_ACTION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_SETTINGS_INPUT_LIBRETRO_DEVICE:
      case MENU_SETTINGS_INPUT_INPUT_REMAP_PORT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_SETTINGS_INPUT_ANALOG_DPAD_MODE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC];
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      char buffer[64];
      int               index = type - MENU_SETTINGS_CHEEVOS_START;
      uintptr_t badge_texture = rcheevos_menu_get_badge_texture(index);
      if (badge_texture)
         return badge_texture;

      /* no state means its a header - show the info icon */
      if (!rcheevos_menu_get_state(index, buffer, sizeof(buffer)))
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INFO];

      /* placeholder badge image was not found, show generic menu icon */
      return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
   }
#endif

   if (
         (type >= MENU_SETTINGS_INPUT_BEGIN) &&
         (type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
      )
      {
         /* This part is only utilized by Input User # Binds */
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if (type == input_id)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC];
#ifdef HAVE_LIBNX
            /* account for the additional split joycon option in Input User # Binds */
            input_id++;
#endif
            if (type == input_id + 1)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
            if (type == input_id + 2)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_MOUSE];
            if (type == input_id + 3)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BIND_ALL];
            if (type == input_id + 4)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
            if (type == input_id + 5)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
            if ((type > (input_id + 29)) && (type < (input_id + 41)))
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LGUN];
            if (type == input_id + 41)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO];
            /* align to use the same code of Quickmenu controls*/
            input_id = input_id + 6;
         }
         else
         {
            /* Quickmenu controls repeats the same icons for all users*/
            if (type < MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)
               input_id = MENU_SETTINGS_INPUT_DESC_BEGIN;
            else
               input_id = MENU_SETTINGS_INPUT_DESC_KBD_BEGIN;
            while (type > (input_id + 23))
               input_id = (input_id + 24);

            /* Human readable bind order */
            if (type < (input_id + RARCH_ANALOG_BIND_LIST_END))
            {
               unsigned index = 0;
               int input_num  = type - input_id;
               for (index = 0; index < ARRAY_SIZE(input_config_bind_order); index++)
               {
                  if ((int)input_config_bind_order[index] == input_num)
                  {
                     type = input_id + index;
                     break;
                  }
               }
            }
         }

         /* This is utilized for both Input Binds and Quickmenu controls*/
         if (type == input_id)
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U];
         if (type == (input_id + 1))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D];
         if (type == (input_id + 2))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L];
         if (type == (input_id + 3))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R];
         if (type == (input_id + 4))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D];
         if (type == (input_id + 5))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R];
         if (type == (input_id + 6))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L];
         if (type == (input_id + 7))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U];
         if (type == (input_id + 8))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT];
         if (type == (input_id + 9))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START];
         if (type == (input_id + 10))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB];
         if (type == (input_id + 11))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB];
         if (type == (input_id + 12))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT];
         if (type == (input_id + 13))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT];
         if (type == (input_id + 14))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P];
         if (type == (input_id + 15))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P];
         if (type == (input_id + 16))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U];
         if (type == (input_id + 17))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D];
         if (type == (input_id + 18))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L];
         if (type == (input_id + 19))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R];
         if (type == (input_id + 20))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U];
         if (type == (input_id + 21))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D];
         if (type == (input_id + 22))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L];
         if (type == (input_id + 23))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R];
      }

   if (
         (type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN) &&
         (type <= MENU_SETTINGS_REMAPPING_PORT_END)
      )
      return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];

   /* No icon by default */
   return 0;
}

static const char *ozone_entries_icon_texture_path(unsigned id)
{
   switch (id)
   {
      case OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS:
         return "settings.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_HISTORY:
         return "history.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES:
         return "favorites.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE:
         return "add-favorite.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSICS:
         return "musics.png";
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGES:
         return "images.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTING:
         return "setting.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ARROW:
         return "arrow.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RUN:
         return "run.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOSE:
         return "close.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RESUME:
         return "resume.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOCK:
         return "clock.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80:
         return "battery-80.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60:
         return "battery-60.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40:
         return "battery-40.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20:
         return "battery-20.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_POINTER:
         return "pointer.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE:
         return "savestate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RECORDREPLAY:
         return "savestate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PLAYREPLAY:
         return "loadstate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_HALTREPLAY:
         return "close.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UNDO:
         return "undo.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BLUETOOTH:
         return "bluetooth.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_WIFI:
         return "wifi.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RELOAD:
         return "reload.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RENAME:
         return "rename.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FILE:
         return "file.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FOLDER:
         return "folder.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ZIP:
         return "zip.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSIC:
         return "music.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE:
         return "favorites-content.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGE:
         return "image.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIE:
         return "movie.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE:
         return "core.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RDB:
         return "database.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CURSOR:
         return "cursor.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON:
         return "on.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF:
         return "off.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DISC:
         return "disc.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY:
         return "netplay.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM:
         return "menu_room.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN:
         return "menu_room_lan.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY:
         return "menu_room_relay.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY:
         return "key.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE:
         return "dialog-slice.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS:
         return "menu_achievements.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_AUDIO:
         return "menu_audio.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS:
         return "menu_drivers.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_EXIT:
         return "menu_exit.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP:
         return "menu_frameskip.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_HELP:
         return "menu_help.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INFO:
         return "menu_info.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS:
         return "Libretro - Pad.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LATENCY:
         return "menu_latency.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_NETWORK:
         return "menu_network.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_POWER:
         return "menu_power.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RECORD:
         return "menu_record.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVING:
         return "menu_saving.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UPDATER:
         return "menu_updater.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_VIDEO:
         return "menu_video.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MIXER:
         return "menu_mixer.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LOG:
         return "menu_log.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OSD:
         return "menu_osd.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UI:
         return "menu_ui.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_USER:
         return "menu_user.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY:
         return "menu_privacy.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST:
         return "menu_playlist.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU:
         return "menu_quickmenu.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_REWIND:
         return "menu_rewind.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY:
         return "menu_overlay.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE:
         return "menu_override.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS:
         return "menu_notifications.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_STREAM:
         return "menu_stream.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN:
         return "menu_shutdown.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U:
         return "input_DPAD-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D:
         return "input_DPAD-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L:
         return "input_DPAD-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R:
         return "input_DPAD-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U:
         return "input_STCK-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D:
         return "input_STCK-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L:
         return "input_STCK-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R:
         return "input_STCK-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P:
         return "input_STCK-P.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U:
         return "input_BTN-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D:
         return "input_BTN-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L:
         return "input_BTN-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R:
         return "input_BTN-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB:
         return "input_LB.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB:
         return "input_RB.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT:
         return "input_LT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT:
         return "input_RT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT:
         return "input_SELECT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START:
         return "input_START.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD:
         return "menu_add.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS:
         return "menu_brightness.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PAUSE:
         return "menu_pause.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_TOGGLE:
         return "menu_apply_toggle.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG:
         return "menu_apply_cog.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC:
         return "input_ADC.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BIND_ALL:
         return "input_BIND_ALL.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_MOUSE:
         return "input_MOUSE.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LGUN:
         return "input_LGUN.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO:
         return "input_TURBO.png";
   }
   return NULL;
}

static void ozone_unload_theme_textures(ozone_handle_t *ozone)
{
   unsigned i, j;

   for (j = 0; j < ARRAY_SIZE(ozone_themes); j++)
   {
      ozone_theme_t *theme = ozone_themes[j];
      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
            video_driver_texture_unload(&theme->textures[i]);
   }
}

static bool ozone_reset_theme_textures(ozone_handle_t *ozone)
{
   static const char *OZONE_THEME_TEXTURES_FILES[OZONE_THEME_TEXTURE_LAST] = {
      "switch.png",
      "check.png",

      "cursor_noborder.png",
      "cursor_static.png"
   };
   unsigned i, j;
   char theme_path[255];
   bool result = true;

   for (j = 0; j < ARRAY_SIZE(ozone_themes); j++)
   {
      ozone_theme_t *theme = ozone_themes[j];

      if (!theme->name)
         continue;

      fill_pathname_join_special(
            theme_path,
            ozone->png_path,
            theme->name,
            sizeof(theme_path)
      );

      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
      {
         if (!gfx_display_reset_textures_list(
               OZONE_THEME_TEXTURES_FILES[i],
               theme_path,
               &theme->textures[i],
               TEXTURE_FILTER_MIPMAP_LINEAR,
               NULL,
               NULL))
            result = false;
      }
   }

   return result;
}

static void ozone_sidebar_collapse_end(void *userdata)
{
   ozone_handle_t *ozone    = (ozone_handle_t*)userdata;

   ozone->sidebar_collapsed = true;
}

static unsigned ozone_get_sidebar_height(ozone_handle_t *ozone)
{
   int entries = (int)(ozone->system_tab_end + 1 + (ozone->horizontal_list.size));
   return entries * ozone->dimensions.sidebar_entry_height
         + (entries - 1) * ozone->dimensions.sidebar_entry_padding_vertical
         + ozone->dimensions.sidebar_padding_vertical
         + (ozone->horizontal_list.size > 0
               ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px
               : 0);
}

static size_t ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone)
{
   return ozone->categories_selection_ptr * ozone->dimensions.sidebar_entry_height
         + (ozone->categories_selection_ptr - 1) * ozone->dimensions.sidebar_entry_padding_vertical
         + ozone->dimensions.sidebar_padding_vertical
         + (ozone->categories_selection_ptr > ozone->system_tab_end
               ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px
               : 0);
}

static float ozone_sidebar_get_scroll_y(
      ozone_handle_t *ozone,
      unsigned video_height)
{
   float scroll_y                          = ozone->animations.scroll_y_sidebar;
   float selected_position_y               = ozone_get_selected_sidebar_y_position(ozone);
   float current_selection_middle_onscreen = ozone->dimensions.header_height
         + ozone->dimensions.spacer_1px
         + ozone->animations.scroll_y_sidebar
         + selected_position_y
         + ozone->dimensions.sidebar_entry_height / 2.0f;
   float bottom_boundary                   = (float)video_height
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

static void ozone_draw_icon(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color,
      math_matrix_4x4 *mymat)
{
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   gfx_display_ctx_driver_t
         *dispctx       = p_disp->dispctx;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;

   if (draw.height > 0 && draw.width > 0)
      dispctx->draw(&draw, userdata, video_width, video_height);
}

static int ozone_wiggle(ozone_handle_t* ozone, float t)
{
   float a = ozone->cursor_wiggle_state.amplitude;
   /* Damped sine wave */
   float w = 0.8f;   /* period */
   float c = 0.35f;  /* damp factor */
   return roundf(a * exp(-(c * t)) * sin(w * t));
}

/* Changes x and y to the current offset of the cursor wiggle animation */
static void ozone_apply_cursor_wiggle_offset(ozone_handle_t *ozone, int *x, size_t *y)
{
   retro_time_t cur_time, t;

   /* Don't do anything if we are not wiggling */
   if (!ozone || (!(ozone->flags2 & OZONE_FLAG2_CURSOR_WIGGLING)))
      return;

   cur_time = menu_driver_get_current_time() / 1000;
   t        = (cur_time - ozone->cursor_wiggle_state.start_time) / 10;

   /* Has the animation ended? */
   if (t >= OZONE_WIGGLE_DURATION)
   {
      ozone->flags2 &= ~OZONE_FLAG2_CURSOR_WIGGLING;
      return;
   }

   /* Change cursor position depending on wiggle direction */
   switch (ozone->cursor_wiggle_state.direction)
   {
      case MENU_ACTION_RIGHT:
         *x += ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_LEFT:
         *x -= ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_DOWN:
         *y += ozone_wiggle(ozone, t);
         break;
      case MENU_ACTION_UP:
         *y -= ozone_wiggle(ozone, t);
         break;
      default:
         break;
   }
}

static void ozone_draw_cursor_slice(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width,
      unsigned height,
      size_t y,
      float alpha,
      math_matrix_4x4 *mymat)
{
   gfx_display_ctx_driver_t
         *dispctx                  = p_disp->dispctx;
   static float last_alpha         = 0.0f;
   float scale_factor              = ozone->last_scale_factor;
   int slice_x                     = x_offset - 12 * scale_factor;
   int slice_y                     = (int)y + 8 * scale_factor;
   unsigned slice_new_w            = width + (24 + 1) * scale_factor;
   unsigned slice_new_h            = height + 20 * scale_factor;
   unsigned slice_w                = 80;
   unsigned slice_h                = 80;
   unsigned offset                 = 20;

   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone->theme_dynamic.cursor_alpha, alpha);
      gfx_display_set_alpha(ozone->theme_dynamic.cursor_border, alpha);
      last_alpha = alpha;
   }

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Cursor without border */
   gfx_display_draw_texture_slice(
         p_disp,
         userdata,
         video_width,
         video_height,
         slice_x,
         slice_y,
         slice_w,
         slice_h,
         slice_new_w,
         slice_new_h,
         video_width,
         video_height,
         ozone->theme_dynamic.cursor_alpha,
         offset,
         scale_factor,
         ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_NO_BORDER],
         mymat
         );

   /* Tainted border */
   gfx_display_draw_texture_slice(
         p_disp,
         userdata,
         video_width,
         video_height,
         slice_x,
         slice_y,
         slice_w,
         slice_h,
         slice_new_w,
         slice_new_h,
         video_width,
         video_height,
         ozone->theme_dynamic.cursor_border,
         offset,
         scale_factor,
         ozone->textures[OZONE_TEXTURE_CURSOR_BORDER],
         mymat
         );

   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);
}

static void ozone_draw_cursor_fallback(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width,
      unsigned height,
      size_t y,
      float alpha)
{
   static float last_alpha         = 0.0f;

   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone->theme_dynamic.selection_border, alpha);
      gfx_display_set_alpha(ozone->theme_dynamic.selection, alpha);
      last_alpha = alpha;
   }

   /* Fill */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_3px,
         (int)y,
         width + ozone->dimensions.spacer_3px * 2,
         height,
         video_width,
         video_height,
         ozone->theme_dynamic.selection,
         NULL);

   /* Borders (can't do one single quad because of alpha) */

   /* Top */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_5px,
         (int)(y - ozone->dimensions.spacer_3px),
         width + 1 + ozone->dimensions.spacer_5px * 2,
         ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border,
         NULL);

   /* Bottom */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_5px,
         (int)(y + height),
         width + 1 + ozone->dimensions.spacer_5px * 2,
         ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border,
         NULL);

   /* Left */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_5px,
         (int)y,
         ozone->dimensions.spacer_3px,
         height,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border,
         NULL);

   /* Right */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset + width + ozone->dimensions.spacer_3px,
         (int)y,
         ozone->dimensions.spacer_3px,
         height,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border,
         NULL);
}

static void ozone_draw_cursor(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width,
      unsigned height,
      size_t y,
      float alpha,
      math_matrix_4x4 *mymat)
{
   int new_x    = x_offset;
   size_t new_y = y;

   /* Apply wiggle animation if needed */
   if (ozone->flags2 & OZONE_FLAG2_CURSOR_WIGGLING)
      ozone_apply_cursor_wiggle_offset(ozone, &new_x, &new_y);

   /* Draw the cursor */
   if (     (ozone->theme->name)
         && (ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS))
      ozone_draw_cursor_slice(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            new_x,
            width,
            height,
            new_y,
            alpha,
            mymat);
   else
      ozone_draw_cursor_fallback(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            new_x,
            width,
            height,
            new_y,
            alpha);
}

static void ozone_draw_sidebar(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity,
      math_matrix_4x4 *mymat)
{
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
      MENU_ENUM_LABEL_VALUE_ADD_TAB,
      MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
#ifdef HAVE_LIBRETRODB
      MENU_ENUM_LABEL_VALUE_EXPLORE_TAB
#endif
   };
   static const unsigned ozone_system_tabs_icons[OZONE_SYSTEM_TAB_LAST]            = {
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
      OZONE_TAB_TEXTURE_SCAN_CONTENT,
      OZONE_TAB_TEXTURE_CONTENTLESS_CORES,
#ifdef HAVE_LIBRETRODB
      OZONE_TAB_TEXTURE_EXPLORE
#endif
   };
   size_t y;
   int entry_width;
   char console_title[255];
   unsigned i, sidebar_height;
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const
         ticker_spacer               = OZONE_TICKER_SPACER;
   unsigned ticker_x_offset          = 0;
   uint32_t text_alpha               = ozone->animations.sidebar_text_alpha * 255.0f;
   bool use_smooth_ticker            = settings->bools.menu_ticker_smooth;
   float scale_factor                = ozone->last_scale_factor;
   enum gfx_animation_ticker_type
         menu_ticker_type            =
         (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
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

   if (p_disp->dispctx && p_disp->dispctx->scissor_begin)
      gfx_display_scissor_begin(
            p_disp,
            userdata,
            video_width, video_height,
            0,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            (unsigned)ozone->dimensions_sidebar_width,
            video_height
                  - ozone->dimensions.header_height
                  - ozone->dimensions.footer_height
                  - ozone->dimensions.spacer_1px);

   /* Background */
   sidebar_height = video_height
         - ozone->dimensions.header_height
         - ozone->dimensions.sidebar_gradient_height * 2
         - ozone->dimensions.footer_height;

   if (sidebar_height)
   {
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            (unsigned)ozone->dimensions_sidebar_width,
            ozone->dimensions.sidebar_gradient_height,
            video_width,
            video_height,
            ozone->theme->sidebar_top_gradient,
            NULL);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px
                  + ozone->dimensions.sidebar_gradient_height,
            (unsigned)ozone->dimensions_sidebar_width,
            sidebar_height,
            video_width,
            video_height,
            ozone->theme->sidebar_background,
            NULL);
      gfx_display_draw_quad(
            p_disp,
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
            ozone->theme->sidebar_bottom_gradient,
            NULL);
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

   entry_width = (unsigned)ozone->dimensions_sidebar_width - ozone->dimensions.sidebar_padding_horizontal * 2;

   /* Cursor */
   if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
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
            ozone->animations.cursor_alpha,
            mymat);

   if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD)
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
            1 - ozone->animations.cursor_alpha,
            mymat);

   /* Menu tabs */
   y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_padding_vertical;
   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   for (i = 0; i < (unsigned)(ozone->system_tab_end + 1); i++)
   {
      float *col                     = NULL;
      bool selected                  = (ozone->categories_selection_ptr == i);
      unsigned icon                  = ozone_system_tabs_icons[ozone->tabs[i]];

      if (!(col = selected ? ozone->theme->text_selected : ozone->theme->entries_icon))
         col = ozone->pure_white;

      /* Icon */
      ozone_draw_icon(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->dimensions.sidebar_entry_icon_size,
            ozone->dimensions.sidebar_entry_icon_size,
            ozone->tab_textures[icon],
            ozone->sidebar_offset
                  + ozone->dimensions.sidebar_padding_horizontal
                  + ozone->dimensions.sidebar_entry_icon_padding,
            y
                  + ozone->dimensions.sidebar_entry_height / 2
                  - ozone->dimensions.sidebar_entry_icon_size / 2
                  + ozone->animations.scroll_y_sidebar,
            video_width,
            video_height,
            0.0f,
            1.0f,
            col,
            mymat);

      /* Text */
      if (!ozone->sidebar_collapsed)
      {
         enum msg_hash_enums value_idx  = ozone_system_tabs_value[ozone->tabs[i]];
         const char *title              = msg_hash_to_str(value_idx);
         uint32_t text_color            = selected
               ? COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, text_alpha)
               : COLOR_TEXT_ALPHA(ozone->theme->text_sidebar_rgba, text_alpha);
         gfx_display_draw_text(
               ozone->fonts.sidebar.font,
               title,
               ozone->sidebar_offset
                     + ozone->dimensions.sidebar_padding_horizontal
                     + ozone->dimensions.sidebar_entry_icon_padding * 2
                     + ozone->dimensions.sidebar_entry_icon_size,
               y
                     + ozone->dimensions.sidebar_entry_height / 2.0f
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
      }

      y += ozone->dimensions.sidebar_entry_height + ozone->dimensions.sidebar_entry_padding_vertical;
   }

   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);

   /* Console tabs */
   if (horizontal_list_size > 0)
   {
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset + ozone->dimensions.sidebar_padding_horizontal,
            y + ozone->animations.scroll_y_sidebar,
            entry_width,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme->entries_border,
            NULL);

      y += ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px;

      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);

      for (i = 0; i < horizontal_list_size; i++)
      {
         ozone_node_t *node   = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;
         float *col           = NULL;
         bool selected        = (ozone->categories_selection_ptr == ozone->system_tab_end + 1 + i);
         uint32_t text_color  = COLOR_TEXT_ALPHA((selected
               ? ozone->theme->text_selected_rgba
               : ozone->theme->text_sidebar_rgba), text_alpha);

         if (!node)
            goto console_iterate;

         if (!(col = (selected ? ozone->theme->text_selected : ozone->theme->entries_icon)))
            col = ozone->pure_white;

         /* Icon */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               ozone->dimensions.sidebar_entry_icon_size,
               ozone->dimensions.sidebar_entry_icon_size,
               node->icon,
               ozone->sidebar_offset
                     + ozone->dimensions.sidebar_padding_horizontal
                     + ozone->dimensions.sidebar_entry_icon_padding,
               y
                     + ozone->dimensions.sidebar_entry_height / 2
                     - ozone->dimensions.sidebar_entry_icon_size / 2
                     + ozone->animations.scroll_y_sidebar,
               video_width,
               video_height,
               0.0f,
               1.0f,
               col,
               mymat);

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
            ticker_smooth.field_width = (entry_width
                  - ozone->dimensions.sidebar_entry_icon_size
                  - 40 * scale_factor);
            ticker_smooth.src_str     = node->console_name;
            ticker_smooth.dst_str     = console_title;
            ticker_smooth.dst_str_len = sizeof(console_title);

            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.len      = (entry_width
                  - ozone->dimensions.sidebar_entry_icon_size
                  - 40 * scale_factor)
                  / ozone->fonts.sidebar.glyph_width;
            ticker.s        = console_title;
            ticker.selected = selected;
            ticker.str      = node->console_name;

            gfx_animation_ticker(&ticker);
         }

         gfx_display_draw_text(
               ozone->fonts.sidebar.font,
               console_title,
               ticker_x_offset
                     + ozone->sidebar_offset
                     + ozone->dimensions.sidebar_padding_horizontal
                     + ozone->dimensions.sidebar_entry_icon_padding * 2
                     + ozone->dimensions.sidebar_entry_icon_size,
               y
                     + ozone->dimensions.sidebar_entry_height / 2
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

   font_flush(video_width, video_height, &ozone->fonts.sidebar);

   if (dispctx && dispctx->scissor_end)
      dispctx->scissor_end(userdata, video_width, video_height);
}

static void ozone_thumbnail_bar_hide_end(void *userdata)
{
   ozone_handle_t *ozone      = (ozone_handle_t*) userdata;
   ozone->show_thumbnail_bar  = false;
   ozone->flags              &= ~(OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR);

   if (!(ozone->is_quick_menu && menu_is_running_quick_menu()))
      ozone->flags           |= OZONE_FLAG_NEED_COMPUTE;
}

static bool ozone_is_load_content_playlist(void *userdata)
{
   ozone_handle_t *ozone      = (ozone_handle_t*) userdata;
   menu_entry_t entry;

   if (     (ozone->depth != 4)
         || (ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST)
         || (ozone->flags & OZONE_FLAG_IS_FILE_LIST))
      return false;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
   menu_entry_get(&entry, 0, 0, NULL, true);

   return entry.type == FILE_TYPE_RPL_ENTRY;
}

static void ozone_update_savestate_thumbnail_path(void *data, unsigned i)
{
   settings_t *settings     = config_get_ptr();
   ozone_handle_t *ozone    = (ozone_handle_t*)data;
   int state_slot           = settings->ints.state_slot;
   bool savestate_thumbnail = settings->bools.savestate_thumbnail_enable;

   if (!ozone)
      return;

   /* Cache previous savestate thumbnail path */
   strlcpy(
         ozone->prev_savestate_thumbnail_file_path,
         ozone->savestate_thumbnail_file_path,
         sizeof(ozone->prev_savestate_thumbnail_file_path));

   if (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER_REAL)
      ozone->flags2 |=  OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;
   else
      ozone->flags2 &= ~OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;

   if (ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET)
      return;

   ozone->savestate_thumbnail_file_path[0] = '\0';

   /* Savestate thumbnails are only relevant
    * when viewing the running quick menu or state slots */
   if (!(   (ozone->is_quick_menu && menu_is_running_quick_menu())
         || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)))
      return;

   if (savestate_thumbnail)
   {
      menu_entry_t entry;

      MENU_ENTRY_INITIALIZE(entry);
      entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED;
      menu_entry_get(&entry, 0, i, NULL, true);

      if (!string_is_empty(entry.label))
      {
         if (string_to_unsigned(entry.label) == MENU_ENUM_LABEL_STATE_SLOT ||
             string_is_equal(entry.label, "state_slot") ||
             string_is_equal(entry.label, "loadstate") ||
             string_is_equal(entry.label, "savestate"))
         {
            size_t _len;
            char path[8204];
            runloop_state_t *runloop_st = runloop_state_get_ptr();

            /* State slot dropdown */
            if (string_to_unsigned(entry.label) == MENU_ENUM_LABEL_STATE_SLOT)
            {
               state_slot    = i - 1;
               ozone->flags |= OZONE_FLAG_IS_STATE_SLOT;
            }

            if (state_slot < 0)
            {
               path[0]       = '\0';
               _len          = fill_pathname_join_delim(path,
                     runloop_st->name.savestate, "auto", '.', sizeof(path));
            }
            else
            {
               _len          = strlcpy(path, runloop_st->name.savestate, sizeof(path));
               if (state_slot > 0)
                  _len      += snprintf(path + _len, sizeof(path) - _len, "%d", state_slot);
            }
            strlcpy(path + _len, FILE_PATH_PNG_EXTENSION, sizeof(path) - _len);

            if (path_is_valid(path))
            {
               strlcpy(
                     ozone->savestate_thumbnail_file_path, path,
                     sizeof(ozone->savestate_thumbnail_file_path));

               ozone->flags |= OZONE_FLAG_WANT_THUMBNAIL_BAR
                             | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
               menu_update_fullscreen_thumbnail_label(
                     ozone->fullscreen_thumbnail_label,
                     sizeof(ozone->fullscreen_thumbnail_label),
                     ozone->is_quick_menu,
                     NULL);
            }
            else if (!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
               ozone->flags &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                               | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);
            else
               ozone->flags &= ~OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
         }
         else if (!(ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET))
            ozone->flags &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                            | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);

         if (     ((ozone->show_thumbnail_bar)
               != ((ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR) > 0)
               && (!(ozone->flags & OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR))))
            ozone->flags |= OZONE_FLAG_NEED_COMPUTE;
      }
   }
}

static void ozone_update_savestate_thumbnail_image(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   settings_t *settings  = config_get_ptr();
   unsigned thumbnail_upscale_threshold
                         = settings->uints.gfx_thumbnail_upscale_threshold;

   if ((!ozone) || (ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET))
      return;

   /* If path is empty, just reset thumbnail */
   if (string_is_empty(ozone->savestate_thumbnail_file_path))
      gfx_thumbnail_reset(&ozone->thumbnails.savestate);
   else
   {
      /* Only request thumbnail if:
       * > Thumbnail has never been loaded *OR*
       * > Thumbnail path has changed */
      if ((ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_UNKNOWN) ||
          !string_is_equal(ozone->savestate_thumbnail_file_path, ozone->prev_savestate_thumbnail_file_path))
      {
         gfx_thumbnail_request_file(
               ozone->savestate_thumbnail_file_path,
               &ozone->thumbnails.savestate,
               thumbnail_upscale_threshold);
      }
   }

   ozone->thumbnails.savestate.flags |= GFX_THUMB_FLAG_CORE_ASPECT;
}

static void ozone_entries_update_thumbnail_bar(
      ozone_handle_t *ozone,
      bool is_playlist,
      bool allow_animation)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t tag            = (uintptr_t)&ozone->show_thumbnail_bar;
   settings_t *settings     = config_get_ptr();
   bool savestate_thumbnail = settings ? settings->bools.savestate_thumbnail_enable : false;

   if (!savestate_thumbnail)
      ozone->flags &= ~OZONE_FLAG_IS_STATE_SLOT;

   entry.duration    = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = OZONE_EASING_XY;
   entry.tag         = tag;
   entry.subject     = &ozone->animations.thumbnail_bar_position;

   gfx_animation_kill_by_tag(&tag);

   /* Show it
    * > We only want to trigger a 'show' animation
    *   if 'show_thumbnail_bar' is currently false.
    *   However: 'show_thumbnail_bar' is only set
    *   to false by the 'ozone_thumbnail_bar_hide_end'
    *   callback. If the above 'gfx_animation_kill_by_tag()'
    *   kills an existing 'hide' animation, then the
    *   callback will not fire - so the sidebar will be
    *   off screen, but a subsequent attempt to show it
    *   here will fail, since 'show_thumbnail_bar' will
    *   be a false positive. We therefore require an
    *   additional OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR parameter
    *   to track mid-animation state changes... */
   if ( (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
         && ( (!(ozone->show_thumbnail_bar))
            ||  (ozone->flags & OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR))
         && (   (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR)
            ||  (ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
      )
   {
      if (allow_animation)
      {
         entry.cb                  = NULL;
         entry.userdata            = NULL;
         entry.target_value        = ozone->dimensions.thumbnail_bar_width;

         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position
                                   = ozone->dimensions.thumbnail_bar_width;
      }

      ozone->show_thumbnail_bar    = true;
      ozone->flags                &= ~OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR;

      /* Want thumbnails to load instantly when thumbnail
       * sidebar first opens */
      ozone->thumbnails.stream_delay = 0.0f;
      gfx_thumbnail_set_stream_delay(ozone->thumbnails.stream_delay);
   }
   /* Hide it */
   else if ((ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         || ((ozone->show_thumbnail_bar)
            && (!(ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR))
            && (!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))))
   {
      if (allow_animation)
      {
         entry.cb                  = ozone_thumbnail_bar_hide_end;
         entry.userdata            = ozone;
         entry.target_value        = 0.0f;

         ozone->flags             |= OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR;
         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = 0.0f;
         ozone_thumbnail_bar_hide_end(ozone);
      }
   }
}

static unsigned ozone_get_horizontal_selection_type(ozone_handle_t *ozone)
{
   if (ozone->categories_selection_ptr > ozone->system_tab_end)
   {
      size_t i = ozone->categories_selection_ptr - ozone->system_tab_end - 1;
      return ozone->horizontal_list.list[i].type;
   }
   return 0;
}

static bool ozone_is_playlist(ozone_handle_t *ozone, bool depth)
{
   bool is_playlist;

   if (ozone->categories_selection_ptr > ozone->system_tab_end)
   {
      unsigned type = ozone_get_horizontal_selection_type(ozone);
      is_playlist   = type == FILE_TYPE_PLAYLIST_COLLECTION;
   }
   else
   {
      switch (ozone->tabs[ozone->categories_selection_ptr])
      {
         case OZONE_SYSTEM_TAB_MAIN:
            if (ozone_is_load_content_playlist(ozone))
               return true;
         case OZONE_SYSTEM_TAB_SETTINGS:
         case OZONE_SYSTEM_TAB_ADD:
#ifdef HAVE_NETWORKING
         case OZONE_SYSTEM_TAB_NETPLAY:
#endif
#ifdef HAVE_LIBRETRODB
         case OZONE_SYSTEM_TAB_EXPLORE:
#endif
         case OZONE_SYSTEM_TAB_CONTENTLESS_CORES:
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

static bool ozone_want_collapse(ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      bool is_playlist)
{
   return (ozone_collapse_sidebar)
         || (is_playlist && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)));
}

static void ozone_sidebar_update_collapse(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      bool allow_animation)
{
   /* Collapse sidebar if needed */
   struct gfx_animation_ctx_entry entry;
   bool is_playlist          = ozone_is_playlist(ozone, false);
   bool collapse             = false;
   uintptr_t tag             = (uintptr_t)&ozone->sidebar_collapsed;

   entry.easing_enum         = OZONE_EASING_XY;
   entry.tag                 = tag;
   entry.userdata            = ozone;
   entry.duration            = ANIMATION_CURSOR_DURATION;

   gfx_animation_kill_by_tag(&tag);

   /* Treat Explore lists as playlists */
   if (ozone_get_horizontal_selection_type(ozone) == MENU_EXPLORE_TAB)
      is_playlist = true;

   /* Playlists under 'Load Content' don't need sidebar animations */
   if (is_playlist && ozone->depth > 3)
      goto end;

   /* To collapse or not to collapse */
   collapse = ozone_want_collapse(ozone, ozone_collapse_sidebar, is_playlist);

   /* Skip if already at wanted state */
   if ( (collapse  && ozone->sidebar_collapsed) ||
        (!collapse && !ozone->sidebar_collapsed))
      goto end;

   /* Collapse it */
   if (collapse)
   {
      if (allow_animation)
      {
         entry.cb             = ozone_sidebar_collapse_end;

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
         ozone->dimensions_sidebar_width      = ozone->dimensions.sidebar_width_collapsed;
         ozone->sidebar_collapsed             = true;
      }
   }
   /* Show it */
   else
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

end:
   ozone_entries_update_thumbnail_bar(ozone, is_playlist, allow_animation);
}

static void ozone_go_to_sidebar(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      uintptr_t tag)
{
   struct gfx_animation_ctx_entry entry;

   ozone->selection_old           = ozone->selection;
   if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
      ozone->flags               |=  OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
   else
      ozone->flags               &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
   ozone->flags                  |=  OZONE_FLAG_CURSOR_IN_SIDEBAR;

   /* Remember last selection per tab */
   ozone->tab_selection[ozone->categories_selection_ptr] = ozone->selection;

   /* Cursor animation */
   ozone->animations.cursor_alpha = 0.0f;

   entry.cb                       = NULL;
   entry.duration                 = ANIMATION_CURSOR_DURATION;
   entry.easing_enum              = OZONE_EASING_ALPHA;
   entry.subject                  = &ozone->animations.cursor_alpha;
   entry.tag                      = tag;
   entry.target_value             = 1.0f;
   entry.userdata                 = NULL;

   gfx_animation_push(&entry);

   ozone_sidebar_update_collapse(ozone, ozone_collapse_sidebar, true);
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_play_scroll_sound(false);
#endif
}

static void linebreak_after_colon(char (*str)[255])
{
   char *delim = (char*)strchr(*str, ':');
   if (delim)
      *++delim = '\n';
}

static void ozone_update_content_metadata(ozone_handle_t *ozone)
{
   const char *core_name             = NULL;
   struct menu_state *menu_st        = menu_state_get_ptr();
   menu_list_t *menu_list            = menu_st->entries.list;
   size_t selection                  = menu_st->selection_ptr;
   playlist_t *playlist              = playlist_get_cached();
   settings_t *settings              = config_get_ptr();
   bool scroll_content_metadata      = settings->bools.ozone_scroll_content_metadata;
   bool show_entry_idx               = settings->bools.playlist_show_entry_idx;
   const char *directory_runtime_log = settings->paths.directory_runtime_log;
   const char *directory_playlist    = settings->paths.directory_playlist;
   unsigned runtime_type             = settings->uints.playlist_sublabel_runtime_type;
   enum playlist_sublabel_last_played_style_type
         runtime_last_played_style   =
               (enum playlist_sublabel_last_played_style_type)
                     settings->uints.playlist_sublabel_last_played_style;
   enum playlist_sublabel_last_played_date_separator_type
         runtime_date_separator      =
               (enum playlist_sublabel_last_played_date_separator_type)
                     settings->uints.menu_timedate_date_separator;

   /* Must check whether core corresponds to 'viewer'
    * content even when not using a playlist, otherwise
    * file browser image updates are mishandled */
   if (gfx_thumbnail_get_core_name(menu_st->thumbnail_path_data, &core_name))
   {
      if (     string_is_equal(core_name, "imageviewer")
            || string_is_equal(core_name, "musicplayer")
            || string_is_equal(core_name, "movieplayer"))
         ozone->flags2 |=  OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;
      else
         ozone->flags2 &= ~OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;
   }
   else
      ozone->flags2 &= ~OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;

   if (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER)
      ozone->flags2 |=  OZONE_FLAG2_SELECTION_CORE_IS_VIEWER_REAL;
   else
      ozone->flags2 &= ~OZONE_FLAG2_SELECTION_CORE_IS_VIEWER_REAL;

   if ( (playlist
         && (   (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            ||  (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
            ||  (ozone->is_quick_menu && !menu_is_running_quick_menu())))
         || (   (ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && ozone->depth == 4))
   {
      size_t _len;
      const char *core_label             = NULL;
      const struct playlist_entry *entry = NULL;
      ssize_t playlist_index             = selection;
      size_t list_size                   = MENU_LIST_GET_SELECTION(menu_list, 0)->size;
      file_list_t *list                  = MENU_LIST_GET_SELECTION(menu_list, 0);
      bool content_runtime_log           = settings->bools.content_runtime_log;
      bool content_runtime_log_aggr      = settings->bools.content_runtime_log_aggregate;

      if (ozone->is_quick_menu)
      {
         playlist_index        = ozone->playlist_index;
         list_size             = playlist_get_size(playlist);

         /* Fill play time if applicable */
         if (content_runtime_log || content_runtime_log_aggr)
            playlist_get_index(playlist, playlist_index, &entry);
      }
#if defined(HAVE_LIBRETRODB)
      else if (list
            && (selection < list_size)
            && (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST))
      {
         playlist_index = menu_explore_get_entry_playlist_index(
               list->list[selection].type, &playlist, &entry, list, &selection, &list_size);

         /* Fill play time if applicable */
         if (content_runtime_log || content_runtime_log_aggr)
            playlist_get_index(playlist, playlist_index, &entry);

         /* Remember playlist index for metadata */
         if (playlist_index >= 0)
            ozone->playlist_index = playlist_index;
      }
#endif
      /* Get playlist index corresponding
       * to the selected entry */
      else if (list
            && (selection < list_size)
            && (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
      {
         playlist_index        = list->list[selection].entry_idx;

         /* Fill play time if applicable */
         if (content_runtime_log || content_runtime_log_aggr)
            playlist_get_index(playlist, playlist_index, &entry);

         /* Remember playlist index for metadata */
         ozone->playlist_index = playlist_index;
      }

      /* Fill entry enumeration */
      if (show_entry_idx)
      {
         unsigned long _entry = (unsigned long)(playlist_index + 1);
         if (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
            _entry            = (unsigned long)(selection + 1);

         snprintf(ozone->selection_entry_enumeration,
               sizeof(ozone->selection_entry_enumeration),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX),
               _entry, (unsigned long)list_size);

         if (!scroll_content_metadata)
            linebreak_after_colon(&ozone->selection_entry_enumeration);
      }
      else
         ozone->selection_entry_enumeration[0] = '\0';

      /* Fill core name */
      if (entry)
      {
         if (!entry->core_name || string_is_equal(entry->core_name, "DETECT"))
            core_label = msg_hash_to_str(MSG_AUTODETECT);
         else
            core_label = entry->core_name;
      }
      else
      {
         if (!core_name || string_is_equal(core_name, "DETECT"))
            core_label = msg_hash_to_str(MSG_AUTODETECT);
         else
            core_label = core_name;
      }

      _len                               = strlcpy(ozone->selection_core_name,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE),
            sizeof(ozone->selection_core_name));
      ozone->selection_core_name[  _len] = ' ';
      ozone->selection_core_name[++_len] = '\0';
      strlcpy(ozone->selection_core_name + _len, core_label, sizeof(ozone->selection_core_name) - _len);

      if (!scroll_content_metadata)
         linebreak_after_colon(&ozone->selection_core_name);

      /* Word wrap core name string, if required */
      if (!scroll_content_metadata)
      {
         char tmpstr[256];
         unsigned metadata_len =
               (ozone->dimensions.thumbnail_bar_width
                     - ((ozone->dimensions.sidebar_entry_icon_padding * 2) * 2))
                     / ozone->fonts.footer.glyph_width;
         size_t _len = strlcpy(tmpstr, ozone->selection_core_name, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_core_name,
               sizeof(ozone->selection_core_name),
               tmpstr, _len,
               metadata_len, ozone->fonts.footer.wideglyph_width, 0);
         ozone->selection_core_name_lines = ozone_count_lines(ozone->selection_core_name);
      }
      else
         ozone->selection_core_name_lines = 1;

      if (entry)
      {
         if (     (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
               || (ozone->is_quick_menu))
            runtime_update_playlist(
                  playlist, playlist_index,
                  directory_runtime_log,
                  directory_playlist,
                  (runtime_type == PLAYLIST_RUNTIME_PER_CORE),
                  runtime_last_played_style,
                  runtime_date_separator);

         if (!string_is_empty(entry->runtime_str))
            strlcpy(ozone->selection_playtime, entry->runtime_str, sizeof(ozone->selection_playtime));
         if (!string_is_empty(entry->last_played_str))
            strlcpy(ozone->selection_lastplayed, entry->last_played_str, sizeof(ozone->selection_lastplayed));
      }
      else
      {
         const char *disabled_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED);
         size_t _len                       =
            strlcpy(ozone->selection_playtime,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
               sizeof(ozone->selection_playtime));
         ozone->selection_playtime[  _len] = ' ';
         ozone->selection_playtime[++_len] = '\0';
         strlcpy(ozone->selection_playtime + _len, disabled_str, sizeof(ozone->selection_playtime) - _len);

         _len                                =
            strlcpy(ozone->selection_lastplayed,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
               sizeof(ozone->selection_lastplayed));
         ozone->selection_lastplayed[  _len] = ' ';
         ozone->selection_lastplayed[++_len] = '\0';
         strlcpy(ozone->selection_lastplayed + _len, disabled_str, sizeof(ozone->selection_lastplayed) - _len);
      }

      if (!scroll_content_metadata)
      {
         linebreak_after_colon(&ozone->selection_playtime);
         linebreak_after_colon(&ozone->selection_lastplayed);
      }

      /* Word wrap last played string, if required */
      if (!scroll_content_metadata)
      {
         /* Note: Have to use a fixed length of '30' here, to
          * avoid awkward wrapping for certain last played time
          * formats. Last played strings are well defined, however
          * (unlike core names), so this should never overflow the
          * side bar */
         char tmpstr[256];
         size_t _len = strlcpy(tmpstr, ozone->selection_lastplayed, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_lastplayed,
               sizeof(ozone->selection_lastplayed), tmpstr, _len, 30, 100, 0);
         ozone->selection_lastplayed_lines = ozone_count_lines(ozone->selection_lastplayed);
      }
      else
         ozone->selection_lastplayed_lines = 1;
   }
}

static void ozone_tab_set_selection(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;

   /* Do not set tab selection when using pointer */
   if (     ozone
         && !(ozone->flags2 & OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR))
   {
      size_t tab_selection = ozone->tab_selection[ozone->categories_selection_ptr];
      if (tab_selection)
      {
         struct menu_state *menu_st = menu_state_get_ptr();
         menu_st->selection_ptr     = tab_selection;
         ozone_selection_changed(ozone, false);
      }
   }
}

static void ozone_leave_sidebar(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      uintptr_t tag)
{
   struct gfx_animation_ctx_entry entry;
   settings_t *settings             = config_get_ptr();
   unsigned remember_selection_type = settings->uints.menu_remember_selection;
   bool ozone_main_tab_selected     = false;

   ozone_update_content_metadata(ozone);

   ozone->categories_active_idx_old = ozone->categories_selection_ptr;

   if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
      ozone->flags               |=  OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
   else
      ozone->flags               &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
   ozone->flags                  &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR;

   if    ((ozone->tabs[ozone->categories_selection_ptr] == OZONE_SYSTEM_TAB_MAIN)
      || (ozone->tabs[ozone->categories_selection_ptr] == OZONE_SYSTEM_TAB_SETTINGS))
      ozone_main_tab_selected = true;

   /* Restore last selection per tab */
   if    ((remember_selection_type == MENU_REMEMBER_SELECTION_ALWAYS)
      || ((remember_selection_type == MENU_REMEMBER_SELECTION_PLAYLISTS) && (ozone->flags & OZONE_FLAG_IS_PLAYLIST))
      || ((remember_selection_type == MENU_REMEMBER_SELECTION_MAIN) && (ozone_main_tab_selected)))
      ozone_tab_set_selection(ozone);

   /* Cursor animation */
   ozone->animations.cursor_alpha   = 0.0f;

   entry.cb                         = NULL;
   entry.duration                   = ANIMATION_CURSOR_DURATION;
   entry.easing_enum                = OZONE_EASING_ALPHA;
   entry.subject                    = &ozone->animations.cursor_alpha;
   entry.tag                        = tag;
   entry.target_value               = 1.0f;
   entry.userdata                   = NULL;

   gfx_animation_push(&entry);

   ozone_sidebar_update_collapse(ozone, ozone_collapse_sidebar, true);

#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_play_scroll_sound(true);
#endif
}

static void ozone_free_node(ozone_node_t *node)
{
   if (!node)
      return;

   if (node->console_name)
      free(node->console_name);

   node->console_name = NULL;

   if (node->fullpath)
      free(node->fullpath);

   node->fullpath = NULL;

   free(node);
}

static void ozone_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = list ? (unsigned)list->size : 0;

   for (i = 0; i < size; ++i)
   {
      ozone_free_node((ozone_node_t*)file_list_get_userdata_at_offset(list, i));

      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

static ozone_node_t *ozone_copy_node(const ozone_node_t *old_node)
{
   ozone_node_t *new_node = (ozone_node_t*)malloc(sizeof(*new_node));

   *new_node              = *old_node;
   new_node->fullpath     = old_node->fullpath
         ? strdup(old_node->fullpath)
         : NULL;

   return new_node;
}

static void ozone_list_deep_copy(
      const file_list_t *src,
      file_list_t *dst,
      size_t first,
      size_t last)
{
   size_t i, j   = 0;
   uintptr_t tag = (uintptr_t)dst;

   gfx_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];
      void     *src_udata = s->userdata;
      void     *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         dst->list[j].userdata = (void*)ozone_copy_node((const ozone_node_t*)src_udata);

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         dst->list[j].actiondata = data;
      }

      ++j;
   }

   dst->size = j;
}

static void ozone_list_cache(
      void *data,
      enum menu_list_type type,
      unsigned action)
{
   size_t y, entries_end;
   unsigned i;
   unsigned video_info_height;
   float bottom_boundary;
   ozone_node_t *first_node;
   float scale_factor;
   unsigned first             = 0;
   unsigned last              = 0;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *selection_buf = NULL;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   scale_factor               = ozone->last_scale_factor;
   ozone->selection_old_list  = ozone->selection;
   ozone->scroll_old          = ozone->animations.scroll_y;
   ozone->flags              |=  OZONE_FLAG_NEED_COMPUTE;
   if (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
      ozone->flags           |=  OZONE_FLAG_IS_PLAYLIST_OLD;
   else
      ozone->flags           &= ~OZONE_FLAG_IS_PLAYLIST_OLD;

   /* Deep copy visible elements */
   video_driver_get_size(NULL, &video_info_height);
   y                          = ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical;
   entries_end                = MENU_LIST_GET_SELECTION(menu_list, 0)->size;
   selection_buf              = MENU_LIST_GET_SELECTION(menu_list, 0);
   bottom_boundary            = video_info_height - ozone->dimensions.header_height - ozone->dimensions.footer_height;

   for (i = 0; i < entries_end; i++)
   {
      ozone_node_t *node = (ozone_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      if (y + ozone->animations.scroll_y + node->height + 20 * scale_factor < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
      {
         first++;
         goto text_iterate;
      }
      else if (y + ozone->animations.scroll_y - node->height - 20 * scale_factor > bottom_boundary)
         goto text_iterate;

      last++;
text_iterate:
      y += node->height;
   }

   last                    -= 1;
   last                    += first;

   first_node               = (ozone_node_t*)selection_buf->list[first].userdata;
   ozone->old_list_offset_y = first_node->position_y;

   ozone_list_deep_copy(selection_buf,
         &ozone->selection_buf_old, first, last);
}

static void ozone_change_tab(
      ozone_handle_t *ozone,
      enum msg_hash_enums tab,
      enum menu_settings_type type)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *menu_stack    = MENU_LIST_GET(menu_list, 0);
   file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_list, 0);
   size_t stack_size          = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   menu_stack->list[stack_size - 1].label = strdup(msg_hash_to_str(tab));
   menu_stack->list[stack_size - 1].type  = type;

   ozone_list_cache(ozone, MENU_LIST_HORIZONTAL, MENU_ACTION_LEFT);

   menu_driver_deferred_push_content_list(selection_buf);
}

static void ozone_sidebar_goto(
      ozone_handle_t *ozone,
      unsigned new_selection)
{
   static const enum msg_hash_enums ozone_system_tabs_idx[OZONE_SYSTEM_TAB_LAST]      = {
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
      MENU_ENUM_LABEL_ADD_TAB,
      MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB,
#ifdef HAVE_LIBRETRODB
      MENU_ENUM_LABEL_EXPLORE_TAB
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
      MENU_ADD_TAB,
      MENU_CONTENTLESS_CORES_TAB,
#ifdef HAVE_LIBRETRODB
      MENU_EXPLORE_TAB
#endif
   };

   unsigned video_info_height;
   struct gfx_animation_ctx_entry entry;
   uintptr_t tag = (uintptr_t)ozone;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_input_t *menu_input   = &menu_st->input_state;

   video_driver_get_size(NULL, &video_info_height);

   if (ozone->categories_selection_ptr != new_selection)
   {
      ozone->categories_active_idx_old = ozone->categories_selection_ptr;
      ozone->categories_selection_ptr  = new_selection;

      if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         ozone->flags                 |=  OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
      else
         ozone->flags                 &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;

      gfx_animation_kill_by_tag(&tag);
   }

   /* ozone->animations.scroll_y_sidebar will be modified
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input->pointer.y_accel    = 0.0f;

   /* Cursor animation */
   ozone->animations.cursor_alpha = 0.0f;

   entry.cb             = NULL;
   entry.duration       = ANIMATION_CURSOR_DURATION;
   entry.easing_enum    = OZONE_EASING_ALPHA;
   entry.subject        = &ozone->animations.cursor_alpha;
   entry.tag            = tag;
   entry.target_value   = 1.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);

   /* Scroll animation */
   entry.cb           = NULL;
   entry.duration     = ANIMATION_CURSOR_DURATION;
   entry.easing_enum  = OZONE_EASING_XY;
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

static void ozone_sidebar_entries_build_scroll_indices(ozone_handle_t *ozone)
{
   size_t i                     = 0;
   const char *path             = ozone->horizontal_list.list[0].alt
                                ? ozone->horizontal_list.list[0].alt
                                : ozone->horizontal_list.list[0].path;
   int ret                      = path ? TOLOWER((int)*path) : 0;
   int current                  = ELEM_GET_FIRST_CHAR(ret);

   ozone->sidebar_index_list[0] = 0;
   ozone->sidebar_index_size    = 1;

   for (i = 1; i < ozone->horizontal_list.size; i++)
   {
      int first;
      path    = ozone->horizontal_list.list[i].alt
              ? ozone->horizontal_list.list[i].alt
              : ozone->horizontal_list.list[i].path;
      ret     = path ? TOLOWER((int)*path) : 0;
      first   = ELEM_GET_FIRST_CHAR(ret);

      if (first != current)
      {
         /* Add scroll index */
         ozone->sidebar_index_list[ozone->sidebar_index_size] = i + ozone->system_tab_end + 1;
         if (!((ozone->sidebar_index_size + 1) >= SCROLL_INDEX_SIZE))
            ozone->sidebar_index_size++;
      }

      current = first;
   }

   /* Add scroll index */
   ozone->sidebar_index_list[ozone->sidebar_index_size] = ozone->horizontal_list.size - 1;
   if (!((ozone->sidebar_index_size + 1) >= SCROLL_INDEX_SIZE))
      ozone->sidebar_index_size++;
}

static void ozone_refresh_sidebars(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      unsigned video_height)
{
   uintptr_t collapsed_tag              = (uintptr_t)&ozone->sidebar_collapsed;
   uintptr_t offset_tag                 = (uintptr_t)&ozone->sidebar_offset;
   uintptr_t thumbnail_tag              = (uintptr_t)&ozone->show_thumbnail_bar;
   uintptr_t scroll_tag                 = (uintptr_t)ozone;
   bool is_playlist                     = ozone_is_playlist(ozone, false);
   bool collapse                        = false;

   /* Kill any existing animations */
   gfx_animation_kill_by_tag(&collapsed_tag);
   gfx_animation_kill_by_tag(&offset_tag);
   gfx_animation_kill_by_tag(&thumbnail_tag);
   if (ozone->depth == 1)
      gfx_animation_kill_by_tag(&scroll_tag);

   /* Treat Explore lists as playlists */
   if (ozone_get_horizontal_selection_type(ozone) == MENU_EXPLORE_TAB)
      is_playlist = true;

   /* To collapse or not to collapse */
   collapse = ozone_want_collapse(ozone, ozone_collapse_sidebar, is_playlist);

   /* Set sidebar width */
   if (collapse)
   {
      ozone->animations.sidebar_text_alpha = 0.0f;
      ozone->dimensions_sidebar_width      = ozone->dimensions.sidebar_width_collapsed;
      ozone->sidebar_collapsed             = true;
   }
   else
   {
      ozone->animations.sidebar_text_alpha = 1.0f;
      ozone->dimensions_sidebar_width      = ozone->dimensions.sidebar_width_normal;
      ozone->sidebar_collapsed             = false;
   }

   /* Set sidebar offset */
   if (ozone->depth == 1)
   {
      ozone->sidebar_offset = 0.0f;
      ozone->flags         |= OZONE_FLAG_DRAW_SIDEBAR;
   }
   else if (ozone->depth > 1)
   {
      ozone->sidebar_offset = -ozone->dimensions_sidebar_width;
      ozone->flags         &= ~OZONE_FLAG_DRAW_SIDEBAR;
   }

   /* Set thumbnail bar position */
   if (     ( !(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
         && ( !(ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR)
            || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
      )
   {
      ozone->animations.thumbnail_bar_position = ozone->dimensions.thumbnail_bar_width;
      ozone->show_thumbnail_bar                = true;

      /* Want thumbnails to load instantly when thumbnail
       * sidebar first opens */
      ozone->thumbnails.stream_delay = 0.0f;
      gfx_thumbnail_set_stream_delay(ozone->thumbnails.stream_delay);
   }
   else
   {
      ozone->animations.thumbnail_bar_position = 0.0f;
      ozone->show_thumbnail_bar                = false;
   }
   ozone->flags &= ~OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR;

   /* If sidebar is on-screen, update scroll position */
   if (ozone->depth == 1)
   {
      ozone->animations.cursor_alpha     = 1.0f;
      ozone->animations.scroll_y_sidebar = ozone_sidebar_get_scroll_y(ozone, video_height);
   }

   if (ozone->horizontal_list.size)
      ozone_sidebar_entries_build_scroll_indices(ozone);
}

static size_t ozone_list_get_size(void *data, enum menu_list_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      switch (type)
      {
         case MENU_LIST_PLAIN:
            {
               struct menu_state   *menu_st   = menu_state_get_ptr();
               menu_list_t *menu_list         = menu_st->entries.list;
               if (menu_list)
                  return MENU_LIST_GET_STACK_SIZE(menu_list, 0);
            }
            break;
         case MENU_LIST_HORIZONTAL:
            return ozone->horizontal_list.size;
         case MENU_LIST_TABS:
            return ozone->system_tab_end;
      }
   }

   return 0;
}

static void ozone_init_horizontal_list(
      ozone_handle_t *ozone,
      settings_t *settings)
{
   menu_displaylist_info_t info;
   size_t list_size;
   size_t i;
   const char *dir_playlist          = settings->paths.directory_playlist;
   bool menu_content_show_playlists  = settings->bools.menu_content_show_playlists;
   bool ozone_truncate_playlist_name = settings->bools.ozone_truncate_playlist_name;
   bool ozone_sort_after_truncate    = settings->bools.ozone_sort_after_truncate_playlist_name;

   menu_displaylist_info_init(&info);

   info.list                    = &ozone->horizontal_list;
   info.path                    = strdup(dir_playlist);
   info.label                   = strdup(msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
   info.exts                    = strldup("lpl", sizeof("lpl"));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_PLAYLISTS_TAB;

   if (menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info, settings))
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

      if (!playlist_file)
      {
         file_list_set_alt_at_offset(&ozone->horizontal_list, i, NULL);
         continue;
      }

      /* Remove extension */
      fill_pathname_base(playlist_file_noext,
            playlist_file, sizeof(playlist_file_noext));
      path_remove_extension(playlist_file_noext);

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
   if (     ozone_truncate_playlist_name
         && ozone_sort_after_truncate
         && (list_size > 0))
      file_list_sort_on_alt(&ozone->horizontal_list);
}

static void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   size_t list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path   = NULL;
      ozone_node_t *node = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;

      if (!node)
         continue;

      if (!(path = ozone->horizontal_list.list[i].path))
         continue;

      if (string_ends_with_size(path, ".lpl", strlen(path), STRLEN_CONST(".lpl")))
      {
         video_driver_texture_unload(&node->icon);
         video_driver_texture_unload(&node->content_icon);
      }
   }
}

static ozone_node_t *ozone_alloc_node(void)
{
   ozone_node_t *node   = (ozone_node_t*)malloc(sizeof(*node));

   node->height         = 0;
   node->position_y     = 0;
   node->console_name   = NULL;
   node->icon           = 0;
   node->content_icon   = 0;
   node->fullpath       = NULL;
   node->sublabel_lines = 0;
   node->wrap           = false;

   return node;
}

static void ozone_context_reset_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   size_t list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   RHMAP_FREE(ozone->playlist_db_node_map);

   for (i = 0; i < list_size; i++)
   {
      const char *path         = NULL;
      const char *console_name = NULL;
      ozone_node_t *node       = (ozone_node_t*)ozone->horizontal_list.list[i].userdata;

      if (!node)
         if (!(node = ozone_alloc_node()))
            continue;

      if (!(path = ozone->horizontal_list.list[i].path))
         continue;

      if (string_ends_with_size(path, ".lpl", strlen(path), STRLEN_CONST(".lpl")))
      {
         size_t len;
         struct texture_image ti;
         char sysname[PATH_MAX_LENGTH];
         char texturepath[PATH_MAX_LENGTH];
         char content_texturepath[PATH_MAX_LENGTH];

         /* Add current node to playlist database name map */
         RHMAP_SET_STR(ozone->playlist_db_node_map, path, node);

         len                = fill_pathname_base(
               sysname, path, sizeof(sysname));
         /* Manually strip the extension (and dot) from sysname */
            sysname[len-4]  =
            sysname[len-3]  =
            sysname[len-2]  =
            sysname[len-1]  = '\0';
         len                = fill_pathname_join_special(texturepath,
               ozone->icons_path, sysname,
               sizeof(texturepath));
         texturepath[  len] = '.';
         texturepath[++len] = 'p';
         texturepath[++len] = 'n';
         texturepath[++len] = 'g';
         texturepath[++len] = '\0';

         /* If the playlist icon doesn't exist, return default */
         if (!path_is_valid(texturepath))
         {
            len                = fill_pathname_join_special(
                  texturepath, ozone->icons_path, "default",
                  sizeof(texturepath));
            texturepath[  len] = '.';
            texturepath[++len] = 'p';
            texturepath[++len] = 'n';
            texturepath[++len] = 'g';
            texturepath[++len] = '\0';
         }

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

         strlcat(sysname, "-content.png", sizeof(sysname));
         /* Assemble new icon path */
         fill_pathname_join_special(
               content_texturepath, ozone->icons_path, sysname,
               sizeof(content_texturepath));

         /* If the content icon doesn't exist, return default-content */
         if (!path_is_valid(content_texturepath))
            fill_pathname_join_delim(content_texturepath, ozone->icons_path_default,
                  "content.png", '-', sizeof(content_texturepath));

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
      else if (string_ends_with_size(ozone->horizontal_list.list[i].label, ".lvw",
            strlen(ozone->horizontal_list.list[i].label), STRLEN_CONST(".lvw")))
      {
         node->console_name = strdup(path + strlen(msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_VIEW)) + 2);
         node->icon = ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
      }
   }
}

static void ozone_refresh_horizontal_list(
      ozone_handle_t *ozone,
      settings_t *settings)
{
   struct menu_state *menu_st = menu_state_get_ptr();

   ozone_context_destroy_horizontal_list(ozone);
   ozone_free_list_nodes(&ozone->horizontal_list, false);
   file_list_deinitialize(&ozone->horizontal_list);
   RHMAP_FREE(ozone->playlist_db_node_map);

   menu_st->flags                 |=  MENU_ST_FLAG_PREVENT_POPULATE;

   ozone->horizontal_list.list     = NULL;
   ozone->horizontal_list.capacity = 0;
   ozone->horizontal_list.size     = 0;
   ozone_init_horizontal_list(ozone, settings);

   ozone_context_reset_horizontal_list(ozone);
}

static int ozone_get_entries_padding_old_list(ozone_handle_t* ozone)
{
   if (ozone->depth == 2) /* false = left to right */
      if (!(ozone->flags & OZONE_FLAG_FADE_DIRECTION))
         return ozone->dimensions.entry_padding_horizontal_half;
   return ozone->dimensions.entry_padding_horizontal_full;
}

static int ozone_get_entries_padding(ozone_handle_t* ozone)
{
   if (ozone->depth == 1)
      return ozone->dimensions.entry_padding_horizontal_half;
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
      menu_entry_t *entry,
      math_matrix_4x4 *mymat)
{
   bool switch_is_on                 = true;
   bool do_draw_text                 = false;
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* Check icon */
   if (entry->flags & MENU_ENTRY_FLAG_CHECKED)
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
            ozone->theme->name
                  ? ozone->theme->textures[OZONE_THEME_TEXTURE_CHECK]
                  : ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK],
            x - 20 * scale_factor,
            y - 22 * scale_factor,
            video_width,
            video_height,
            0.0f,
            1.0f,
            col,
            mymat);
      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
      return;
   }
   else if (string_is_empty(value))
      return;

   /* Text value */
   if (     string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED))
         || string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      switch_is_on = false;
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED))
         || string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))) { }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (string_is_equal(entry->value, "..."))
            return;
         if (string_starts_with_size(entry->value, "(", STRLEN_CONST("("))
               && string_ends_with  (entry->value, ")"))
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
            !string_is_equal(value, "null")
                  ? COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32)
                  : COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32 >> 1),
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);
   }
   else
      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            (switch_is_on
                  ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)),
            x,
            y,
            video_width,
            video_height,
            switch_is_on
                  ? COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32)
                  : COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32 >> 1),
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);
}

static void ozone_draw_no_thumbnail_available(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned x_position,
      unsigned sidebar_width,
      unsigned y_offset,
      math_matrix_4x4 *mymat)
{
   unsigned icon                     = OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
   unsigned icon_size                = (unsigned)((float)
         ozone->dimensions.sidebar_entry_icon_size * 1.5f);
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float                        *col = ozone->theme->entries_icon;

   ozone->flags                     |= OZONE_FLAG_NO_THUMBNAIL_AVAILABLE;

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
               x_position + sidebar_width / 2 - icon_size / 2,
               video_height / 2 - icon_size / 2 - y_offset,
               video_width,
               video_height,
               0.0f,
               1.0f,
               col,
               mymat);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   gfx_display_draw_text(
         ozone->fonts.footer.font,
         msg_hash_to_str(MSG_NO_THUMBNAIL_AVAILABLE),
         x_position + sidebar_width / 2,
         video_height / 2 + icon_size / 2 + ozone->fonts.footer.line_ascender - y_offset,
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
      uint8_t lines_count)
{
   if (*y + (lines_count * (unsigned)ozone->fonts.footer.line_height)
         > video_height - ozone->dimensions.footer_height)
      return;

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
      *y += (unsigned)(ozone->fonts.footer.line_height * (lines_count - 1))
            + (unsigned)((float)ozone->fonts.footer.line_height * 1.5f);
}


/* Compute new scroll position
 * If the center of the currently selected entry is not in the middle
 * And if we can scroll so that it's in the middle
 * Then scroll
 */
static void ozone_update_scroll(
      ozone_handle_t *ozone,
      bool allow_animation,
      ozone_node_t *node)
{
   unsigned video_info_height;
   gfx_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_input_t *menu_input   = &menu_st->input_state;
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_list, 0);
   uintptr_t tag              = (uintptr_t)selection_buf;

   video_driver_get_size(NULL, &video_info_height);

   if (!node)
      return;

   current_selection_middle_onscreen    = ozone->dimensions.header_height
         + ozone->dimensions.entry_padding_vertical
         + ozone->animations.scroll_y
         + node->position_y
         + node->height / 2;

   bottom_boundary                      = video_info_height
         - ozone->dimensions.header_height
         - ozone->dimensions.spacer_1px
         - ozone->dimensions.footer_height;
   entries_middle                       = video_info_height / 2;

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
   menu_input->pointer.y_accel    = 0.0f;

   if (allow_animation)
   {
      /* Cursor animation */
      ozone->animations.cursor_alpha = 0.0f;

      entry.cb             = NULL;
      entry.duration       = ANIMATION_CURSOR_DURATION;
      entry.easing_enum    = OZONE_EASING_ALPHA;
      entry.subject        = &ozone->animations.cursor_alpha;
      entry.tag            = tag;
      entry.target_value   = 1.0f;
      entry.userdata       = NULL;

      gfx_animation_push(&entry);

      /* Scroll animation */
      entry.cb             = NULL;
      entry.duration       = ANIMATION_CURSOR_DURATION;
      entry.easing_enum    = OZONE_EASING_ALPHA;
      entry.subject        = &ozone->animations.scroll_y;
      entry.tag            = tag;
      entry.target_value   = new_scroll;
      entry.userdata       = NULL;

      gfx_animation_push(&entry);
   }
   else
   {
      ozone->selection_old       = ozone->selection;
      ozone->animations.scroll_y = new_scroll;
   }
}

static int ozone_get_sublabel_max_width(ozone_handle_t *ozone,
      unsigned video_info_width,
      unsigned entry_padding)
{
   int sublabel_max_width = video_info_width
         - (entry_padding * 2)
         - (ozone->dimensions.entry_icon_padding * 2);

   if (ozone->depth == 1)
      sublabel_max_width -= (int)ozone->dimensions_sidebar_width;
   if (ozone->show_thumbnail_bar)
   {
      if (ozone->is_quick_menu && menu_is_running_quick_menu())
         sublabel_max_width -= ozone->dimensions.thumbnail_bar_width - entry_padding * 2;
      else
         sublabel_max_width -= ozone->dimensions.thumbnail_bar_width - entry_padding;
   }

   return sublabel_max_width;
}

static void ozone_compute_entries_position(
      ozone_handle_t *ozone,
      bool menu_show_sublabels,
      size_t entries_end)
{
   size_t i;
   /* Compute entries height and adjust scrolling if needed */
   unsigned video_info_height;
   unsigned video_info_width;
   struct menu_state *menu_st    = menu_state_get_ptr();
   menu_list_t *menu_list        = menu_st->entries.list;
   file_list_t *selection_buf    = NULL;
   int entry_padding             = ozone_get_entries_padding(ozone);
   float scale_factor            = ozone->last_scale_factor;
   bool want_thumbnail_bar       = (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR) ? true : false;
   bool show_thumbnail_bar       = ozone->show_thumbnail_bar;

   if (show_thumbnail_bar != want_thumbnail_bar)
   {
      if (!(      (ozone->flags & OZONE_FLAG_PENDING_HIDE_THUMBNAIL_BAR)
               && (ozone->is_quick_menu)))
         ozone_entries_update_thumbnail_bar(ozone, false, true);
   }

   if (ozone->show_thumbnail_bar)
      ozone_update_content_metadata(ozone);

   selection_buf                 = MENU_LIST_GET_SELECTION(menu_list, 0);

   video_driver_get_size(&video_info_width, &video_info_height);

   ozone->entries_height         = 0;

   for (i = 0; i < entries_end; i++)
   {
      /* Entry */
      menu_entry_t entry;
      ozone_node_t *node       = NULL;

      MENU_ENTRY_INITIALIZE(entry);
      entry.flags |= MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      /* Empty playlist detection:
         only one item which icon is
         OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO */
      if (     (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            && (entries_end == 1))
      {
         uintptr_t         tex = ozone_entries_icon_get_texture(ozone,
               entry.enum_idx, entry.path, entry.label, entry.type, false);
         if (tex == ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO])
            ozone->flags      |=  OZONE_FLAG_EMPTY_PLAYLIST;
         else
            ozone->flags      &= ~OZONE_FLAG_EMPTY_PLAYLIST;
      }
      else
         ozone->flags         &= ~OZONE_FLAG_EMPTY_PLAYLIST;

      /* Cache node */
      if (!(node = (ozone_node_t*)selection_buf->list[i].userdata))
         continue;

      node->height             = ozone->dimensions.entry_height;
      node->wrap               = false;
      node->sublabel_lines     = 0;

      if (menu_show_sublabels)
      {
         if (!string_is_empty(entry.sublabel))
         {
            int sublabel_max_width = ozone_get_sublabel_max_width(ozone, video_info_width, entry_padding);
            char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];

            wrapped_sublabel_str[0] = '\0';
            (ozone->word_wrap)(wrapped_sublabel_str,
                  sizeof(wrapped_sublabel_str),
                  entry.sublabel,
                  strlen(entry.sublabel),
                  sublabel_max_width / ozone->fonts.entries_sublabel.glyph_width,
                  ozone->fonts.entries_sublabel.wideglyph_width,
                  0);

            node->sublabel_lines = ozone_count_lines(wrapped_sublabel_str);
            node->height        += ozone->dimensions.entry_spacing + 40 * scale_factor;

            if (node->sublabel_lines > 1)
            {
               node->height += (node->sublabel_lines - 1) * ozone->fonts.entries_sublabel.line_height;
               node->wrap    = true;
            }
         }
      }

      node->position_y       = ozone->entries_height;
      ozone->entries_height += node->height;
   }

   /* Update scrolling */
   ozone->selection          = menu_st->selection_ptr;
   ozone_update_scroll(ozone, false, (ozone_node_t*)selection_buf->list[ozone->selection].userdata);
}

static void ozone_draw_entries(
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
      bool is_playlist,
      math_matrix_4x4 *mymat)
{
   uint32_t alpha_uint32;
   size_t i;
   float bottom_boundary;
   unsigned video_info_height, video_info_width;
   bool menu_show_sublabels          = settings->bools.menu_show_sublabels;
   bool use_smooth_ticker            = settings->bools.menu_ticker_smooth;
   unsigned show_history_icons       = settings->uints.playlist_show_history_icons;
   enum gfx_animation_ticker_type
         menu_ticker_type            =
         (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   bool old_list                     = selection_buf == &ozone->selection_buf_old;
   int x_offset                      = 0;
   size_t selection_y                = 0; /* 0 means no selection (we assume that no entry has y = 0) */
   size_t old_selection_y            = 0;
   int entry_padding                 = old_list
         ? ozone_get_entries_padding_old_list(ozone)
         : ozone_get_entries_padding(ozone);
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   size_t entries_end                = selection_buf ? selection_buf->size : 0;
   size_t y                          = ozone->dimensions.header_height
         + ozone->dimensions.spacer_1px
         + ozone->dimensions.entry_padding_vertical;
   float sidebar_offset              = ozone->sidebar_offset;
   unsigned entry_width              = video_width
         - (unsigned)ozone->dimensions_sidebar_width
         - ozone->sidebar_offset
         - entry_padding * 2
         - ozone->animations.thumbnail_bar_position;
   unsigned entry_width_max          = entry_width + ozone->animations.thumbnail_bar_position;
   unsigned button_height            = ozone->dimensions.entry_height; /* height of the button (entry minus sublabel) */
   float invert                      = (ozone->flags & OZONE_FLAG_FADE_DIRECTION) ? -1 : 1;
   float alpha_anim                  = old_list ? alpha : 1.0f - alpha;

   video_driver_get_size(&video_info_width, &video_info_height);

   bottom_boundary                   = video_info_height
         - ozone->dimensions.header_height
         - ozone->dimensions.footer_height;

   /* Increase entry width, or rather decrease padding between
    * entries and thumbnails when thumbnail bar is visible */
   if (ozone->show_thumbnail_bar && ozone->depth > 1)
   {
      unsigned entry_padding_old = entry_padding;

      if (     (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            && (!ozone->is_quick_menu))
      {
         entry_width   += entry_padding / 1.25f;
         entry_padding /= 1.25f;
      }
      else
         entry_width   += entry_padding / 1.5f;

      /* Limit entry width to prevent animation bouncing */
      if (entry_width > entry_width_max)
      {
         entry_padding = entry_padding_old;
         entry_width   = entry_width_max;
      }
   }

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

      if (!node || (ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
         goto border_iterate;

      if (y + scroll_y + node->height + 20 * scale_factor < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
         goto border_iterate;
      else if (y + scroll_y - node->height - 20 * scale_factor > bottom_boundary)
         goto border_iterate;

      border_start_x = (unsigned)ozone->dimensions_sidebar_width + x_offset + entry_padding;
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
            ozone->theme_dynamic.entries_border,
            NULL);
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
            ozone->theme_dynamic.entries_border,
            NULL);

border_iterate:
      if (node)
         y += node->height;
   }

   /* Cursor(s) layer - current */
   if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->dimensions_sidebar_width + x_offset + entry_padding + ozone->dimensions.spacer_3px,
            entry_width - ozone->dimensions.spacer_5px,
            button_height + ozone->dimensions.spacer_1px,
            selection_y + scroll_y,
            ozone->animations.cursor_alpha * alpha,
            mymat);

   /* Old */
   if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD))
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            (unsigned)ozone->dimensions_sidebar_width + x_offset + entry_padding + ozone->dimensions.spacer_3px,
            /* TODO/FIXME - undefined behavior reported by ASAN -
             *-35.2358 is outside the range of representable values
             of type 'unsigned int'
             * */
            entry_width - ozone->dimensions.spacer_5px,
            button_height + ozone->dimensions.spacer_1px,
            old_selection_y + scroll_y,
            (1 - ozone->animations.cursor_alpha) * alpha,
            mymat);

   /* Icons + text */
   y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.entry_padding_vertical;

   if (old_list)
      y += ozone->old_list_offset_y;

   for (i = 0; i < entries_end; i++)
   {
      char rich_label[255];
      char entry_value_ticker[255];
      char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
      uintptr_t texture;
      menu_entry_t entry;
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      unsigned ticker_x_offset     = 0;
      unsigned ticker_str_width    = 0;
      int value_x_offset           = 0;
      static const char* const
            ticker_spacer          = OZONE_TICKER_SPACER;
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

      if (y + scroll_y + node->height + 20 * scale_factor < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
      {
         y += node->height;
         continue;
      }
      else if (y + scroll_y - node->height - 20 * scale_factor > bottom_boundary)
      {
         y += node->height;
         continue;
      }

      entry_selected                 = selection == i;

      MENU_ENTRY_INITIALIZE(entry);
      entry.flags    |= MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                      | MENU_ENTRY_FLAG_VALUE_ENABLED
                      | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
      if (ozone->flags & OZONE_FLAG_IS_CONTENTLESS_CORES)
         entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED;
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
         ticker_smooth.selected    = entry_selected && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR));
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
         ticker.selected = entry_selected && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR));
         ticker.len      = (entry_width - entry_padding - (10 * scale_factor) - ozone->dimensions.entry_icon_padding) / ozone->fonts.entries_label.glyph_width;

         gfx_animation_ticker(&ticker);
      }

      if (ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST)
      {
         /* Note: This entry can never be selected, so ticker_x_offset
          * is irrelevant here (i.e. this text will never scroll) */
         unsigned text_width = font_driver_get_message_width(ozone->fonts.entries_label.font, rich_label, strlen(rich_label), 1.0f);
         x_offset            = (video_info_width - (unsigned)ozone->dimensions_sidebar_width - entry_padding * 2) / 2 - (text_width / 2) - (60 * scale_factor);
         y                   = (video_info_height / 2) - (60 * scale_factor);
      }

      sublabel_str = entry.sublabel;

      if (menu_show_sublabels)
      {
         if (node->wrap && !string_is_empty(sublabel_str))
         {
            int sublabel_max_width = ozone_get_sublabel_max_width(ozone, video_info_width, entry_padding);

            wrapped_sublabel_str[0] = '\0';
            (ozone->word_wrap)(wrapped_sublabel_str,
                  sizeof(wrapped_sublabel_str),
                  sublabel_str,
                  strlen(sublabel_str),
                  sublabel_max_width / ozone->fonts.entries_sublabel.glyph_width,
                  ozone->fonts.entries_sublabel.wideglyph_width,
                  0);

            sublabel_str = wrapped_sublabel_str;
         }
      }

      /* Icon */
      texture = ozone_entries_icon_get_texture(ozone,
            entry.enum_idx, entry.path, entry.label, entry.type, entry_selected);

      if (texture)
      {
         /* Console specific icons */
         if (     entry.type == FILE_TYPE_RPL_ENTRY
               && ozone->categories_selection_ptr > ozone->system_tab_end)
         {
            ozone_node_t *sidebar_node = (ozone_node_t*)
                  file_list_get_userdata_at_offset(&ozone->horizontal_list,
                        ozone->categories_selection_ptr - ozone->system_tab_end - 1);

            if (sidebar_node && sidebar_node->content_icon)
               texture = sidebar_node->content_icon;
         }
         /* Playlist manager icons */
         else if (ozone->depth == 3 && entry.enum_idx == MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS)
         {
            if (string_is_equal(entry.rich_label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB)))
               texture = ozone->tab_textures[OZONE_TAB_TEXTURE_HISTORY];
            else if (string_is_equal(entry.rich_label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB)))
               texture = ozone->tab_textures[OZONE_TAB_TEXTURE_FAVORITES];
            else if (i < ozone->horizontal_list.size)
            {
               ozone_node_t *sidebar_node = NULL;
               unsigned offset            = 0;

               /* Ignore Explore Views */
               for (offset = 0; offset < ozone->horizontal_list.size; offset++)
               {
                  char playlist_file_noext[255];
                  strlcpy(playlist_file_noext, ozone->horizontal_list.list[offset].path, sizeof(playlist_file_noext));
                  path_remove_extension(playlist_file_noext);
                  if (string_is_equal(playlist_file_noext, entry.rich_label))
                     break;
               }

               sidebar_node = (ozone_node_t*)file_list_get_userdata_at_offset(&ozone->horizontal_list, offset);
               if (sidebar_node && sidebar_node->icon)
                  texture = sidebar_node->icon;
            }
         }
         /* "Load Content" playlists */
         else if (ozone->tabs[ozone->categories_selection_ptr] == OZONE_SYSTEM_TAB_MAIN)
         {
            if (ozone_is_load_content_playlist(ozone))
            {
               const struct playlist_entry *pl_entry = NULL;
               ozone_node_t *db_node                 = NULL;

               playlist_get_index(playlist_get_cached(),
                     entry.entry_idx, &pl_entry);

               if (pl_entry
                     && !string_is_empty(pl_entry->db_name)
                     && (db_node = RHMAP_GET_STR(ozone->playlist_db_node_map, pl_entry->db_name)))
                  texture = db_node->content_icon;
            }
            else if (ozone->depth == 3 && entry.type == FILE_TYPE_PLAYLIST_COLLECTION)
            {
               ozone_node_t *sidebar_node = (ozone_node_t*)
                     file_list_get_userdata_at_offset(&ozone->horizontal_list,
                           selection_buf->list[i].entry_idx);

               if (sidebar_node && sidebar_node->icon)
                  texture = sidebar_node->icon;
            }
         }
         /* History/Favorite console specific content icons */
         else if (   entry.type == FILE_TYPE_RPL_ENTRY
                  && show_history_icons != PLAYLIST_SHOW_HISTORY_ICONS_DEFAULT)
         {
            switch (ozone->tabs[ozone->categories_selection_ptr])
            {
               case OZONE_SYSTEM_TAB_HISTORY:
               case OZONE_SYSTEM_TAB_FAVORITES:
                  {
                     const struct playlist_entry *pl_entry = NULL;
                     ozone_node_t *db_node                 = NULL;

                     playlist_get_index(playlist_get_cached(),
                           entry.entry_idx, &pl_entry);

                     if (pl_entry
                           && !string_is_empty(pl_entry->db_name)
                           && (db_node = RHMAP_GET_STR(ozone->playlist_db_node_map, pl_entry->db_name)))
                     {
                        switch (show_history_icons)
                        {
                           case PLAYLIST_SHOW_HISTORY_ICONS_MAIN:
                              texture = db_node->icon;
                              break;
                           case PLAYLIST_SHOW_HISTORY_ICONS_CONTENT:
                              texture = db_node->content_icon;
                              break;
                           default:
                              break;
                        }
                     }
                  }
                  break;
               default:
                  break;
            }
         }

         /* Cheevos badges should not be recolored */
         if (!((entry.type >= MENU_SETTINGS_CHEEVOS_START) && (entry.type < MENU_SETTINGS_NETPLAY_ROOMS_START)))
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
                     ozone->dimensions_sidebar_width
                           + x_offset
                           + entry_padding
                           + ozone->dimensions.entry_icon_padding,
                     y
                           + scroll_y
                           + ozone->dimensions.entry_height / 2
                           - ozone->dimensions.entry_icon_size / 2,
                     video_width,
                     video_height,
                     0.0f,
                     1.0f,
                     icon_color,
                     mymat);
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
            ticker_x_offset
                  + text_offset
                  + (unsigned)ozone->dimensions_sidebar_width
                  + x_offset
                  + entry_padding
                  + ozone->dimensions.entry_icon_size
                  + ozone->dimensions.entry_icon_padding * 2,
            y
                  + ozone->dimensions.entry_height / 2.0f
                  + ozone->fonts.entries_label.line_centre_offset
                  + scroll_y,
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
                  (unsigned)ozone->dimensions_sidebar_width
                        + x_offset
                        + entry_padding
                        + ozone->dimensions.entry_icon_padding,
                  y
                        + ozone->dimensions.entry_height
                        - ozone->dimensions.spacer_1px
                        + (node->height
                              - ozone->dimensions.entry_height
                              - (node->sublabel_lines * ozone->fonts.entries_sublabel.line_height)) / 2.0f
                        + ozone->fonts.entries_sublabel.line_ascender
                        + scroll_y,
                  video_width,
                  video_height,
                  COLOR_TEXT_ALPHA(ozone->theme->text_sublabel_rgba, alpha_uint32),
                  TEXT_ALIGN_LEFT,
                  1.0f,
                  false,
                  1.0f,
                  false);
      }

      /* Value */
      if (use_smooth_ticker)
      {
         ticker_smooth.selected    = entry_selected && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR));
         ticker_smooth.field_width = (entry_width
               - ozone->dimensions.entry_icon_size
               - ozone->dimensions.entry_icon_padding * 2
               - ((unsigned)utf8len(entry_rich_label) * ozone->fonts.entries_label.glyph_width));
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
         ticker.selected = entry_selected && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR));
         ticker.len      = (entry_width
               - ozone->dimensions.entry_icon_size
               - ozone->dimensions.entry_icon_padding * 2
               - ((unsigned)utf8len(entry_rich_label) * ozone->fonts.entries_label.glyph_width)) / ozone->fonts.entries_label.glyph_width;

         gfx_animation_ticker(&ticker);
      }

      ozone_draw_entry_value(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            entry_value_ticker,
            value_x_offset
                  + (unsigned)ozone->dimensions_sidebar_width
                  + entry_padding
                  + x_offset
                  + entry_width
                  - ozone->dimensions.entry_icon_padding,
            y
                  + ozone->dimensions.entry_height / 2
                  + ozone->fonts.entries_label.line_centre_offset
                  + scroll_y,
            alpha_uint32,
            &entry,
            mymat);

      y += node->height;
   }

   /* Text layer */
   font_flush(video_width, video_height, &ozone->fonts.entries_label);

   if (menu_show_sublabels)
      font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
}

static void ozone_draw_thumbnail_bar(
      ozone_handle_t *ozone,
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool libretro_running,
      float menu_framebuffer_opacity,
      math_matrix_4x4 *mymat)
{
   enum gfx_thumbnail_alignment right_thumbnail_alignment;
   enum gfx_thumbnail_alignment left_thumbnail_alignment;
   unsigned sidebar_width            = ozone->dimensions.thumbnail_bar_width;
   unsigned thumbnail_width          = sidebar_width - (ozone->dimensions.sidebar_entry_icon_padding * 3);
   int right_thumbnail_y_position    = 0;
   int left_thumbnail_y_position     = 0;
   int bottom_row_y_position         = 0;
   bool show_right_thumbnail         = false;
   bool show_left_thumbnail          = false;
   unsigned sidebar_height           = video_height
         - ozone->dimensions.header_height
         - ozone->dimensions.sidebar_gradient_height * 2
         - ozone->dimensions.footer_height;
   unsigned x_position               = video_width
         - (unsigned)ozone->animations.thumbnail_bar_position
         - ozone->dimensions.sidebar_entry_icon_padding;
   int thumbnail_x_position          = x_position
         + ozone->dimensions.sidebar_entry_icon_padding * 2;
   unsigned thumbnail_height         = (video_height
         - ozone->dimensions.header_height
         - ozone->dimensions.spacer_2px
         - ozone->dimensions.footer_height
         - (ozone->dimensions.sidebar_entry_icon_padding * 3)) / 2;
   float scale_factor                = ozone->last_scale_factor;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* Background */
   if (thumbnail_height)
   {
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            (unsigned)ozone->animations.thumbnail_bar_position + ozone->dimensions.sidebar_entry_icon_padding,
            ozone->dimensions.sidebar_gradient_height,
            video_width,
            video_height,
            ozone->theme->sidebar_top_gradient,
            NULL);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_gradient_height,
            (unsigned)ozone->animations.thumbnail_bar_position + ozone->dimensions.sidebar_entry_icon_padding,
            sidebar_height,
            video_width,
            video_height,
            ozone->theme->sidebar_background,
            NULL);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            video_height - ozone->dimensions.footer_height - ozone->dimensions.sidebar_gradient_height - ozone->dimensions.spacer_1px,
            (unsigned)ozone->animations.thumbnail_bar_position + ozone->dimensions.sidebar_entry_icon_padding,
            ozone->dimensions.sidebar_gradient_height + ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme->sidebar_bottom_gradient,
            NULL);
   }

   /* Thumbnails */
   show_right_thumbnail =
         (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING)
         && gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT);
   show_left_thumbnail  =
         (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING)
         && gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT);

   /* Special "viewer" mode for savestate thumbnails */
   if (     ((ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR) && !string_is_empty(ozone->savestate_thumbnail_file_path))
         || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
   {
      ozone->flags2                  |= OZONE_FLAG2_SELECTION_CORE_IS_VIEWER;
      show_right_thumbnail            = true;
      show_left_thumbnail             = false;

      if (!(ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE ||
            ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_PENDING))
      {
         if (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
            show_right_thumbnail = false;
         else
            return;
      }
   }
   else if (!(ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR))
      return;
   else if (ozone->flags & OZONE_FLAG_NO_THUMBNAIL_AVAILABLE)
   {
      /* Remove useless re-blinking of "No thumbnail available" */
      if (show_right_thumbnail && ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_UNKNOWN)
         show_right_thumbnail = false;
      if (show_left_thumbnail && ozone->thumbnails.left.status == GFX_THUMBNAIL_STATUS_UNKNOWN)
         show_left_thumbnail  = false;
   }

   /* If this entry is associated with the image viewer
    * and no right thumbnail is available, show a centred
    * message and return immediately */
   if (     (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER)
         && (!show_right_thumbnail && !show_left_thumbnail))
   {
      ozone_draw_no_thumbnail_available(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            x_position,
            sidebar_width,
            0,
            mymat);
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
   if (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER)
   {
      right_thumbnail_y_position = ozone->dimensions.header_height
            + (thumbnail_height / 2)
            + (int)(1.5f * (float)ozone->dimensions.sidebar_entry_icon_padding);

      right_thumbnail_alignment  = GFX_THUMBNAIL_ALIGN_CENTRE;
   }
   else
   {
      right_thumbnail_y_position = ozone->dimensions.header_height
            + ozone->dimensions.spacer_1px
            + ozone->dimensions.sidebar_entry_icon_padding;

      right_thumbnail_alignment  = GFX_THUMBNAIL_ALIGN_BOTTOM;
   }

   /* > If we have a right thumbnail, show it */
   if (show_right_thumbnail)
   {
      gfx_thumbnail_draw(
            userdata,
            video_width,
            video_height,
            (ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE ||
             ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_PENDING)
                  ? &ozone->thumbnails.savestate
                  : &ozone->thumbnails.right,
            (float)thumbnail_x_position,
            (float)right_thumbnail_y_position,
            thumbnail_width,
            thumbnail_height,
            right_thumbnail_alignment,
            1.0f,
            1.0f,
            NULL);
      ozone->flags &= ~OZONE_FLAG_NO_THUMBNAIL_AVAILABLE;
   }
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
            thumbnail_height / 2,
            mymat);

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
   bottom_row_y_position = ozone->dimensions.header_height
         + ozone->dimensions.spacer_1px
         + thumbnail_height
         + (ozone->dimensions.sidebar_entry_icon_padding * 2);

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
            1.0f,
            NULL);
      ozone->flags &= ~OZONE_FLAG_NO_THUMBNAIL_AVAILABLE;
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
   if (   (!(ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER))
       && (!show_left_thumbnail || !show_right_thumbnail || (ozone->animations.left_thumbnail_alpha < 1.0f)))
   {
      char ticker_buf[255];
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      static const char* const ticker_spacer = OZONE_TICKER_SPACER;
      unsigned ticker_x_offset               = 0;
      bool scroll_content_metadata           = settings->bools.ozone_scroll_content_metadata;
      bool use_smooth_ticker                 = settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type
            menu_ticker_type                 = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
      bool show_entry_idx                    = settings->bools.playlist_show_entry_idx;
      bool show_entry_core                   = (!(ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST));
      bool show_entry_playtime               = (!(ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST));
      bool show_entry_last_played            = (!(ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST));
      unsigned y                             = (unsigned)bottom_row_y_position;
      unsigned separator_padding             = ozone->dimensions.sidebar_entry_icon_padding * 2;
      unsigned column_x                      = x_position + separator_padding;
      bool metadata_override_enabled         =
            show_left_thumbnail && show_right_thumbnail && (ozone->animations.left_thumbnail_alpha < 1.0f);
      float metadata_alpha                   = metadata_override_enabled
            ? (1.0f - ozone->animations.left_thumbnail_alpha)
            : 1.0f;
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
            sidebar_width - separator_padding * 2,
            ozone->dimensions.spacer_1px,
            video_width,
            video_height,
            ozone->theme_dynamic.entries_border,
            NULL);

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
         if (show_entry_core)
         {
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
         }

         /* Playtime
          * Note: It is essentially impossible for this string
          * to exceed the width of the sidebar, but since we
          * are ticker-texting everything else, we include this
          * by default */
         if (show_entry_playtime)
         {
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
         }

         /* Last played */
         if (show_entry_last_played)
         {
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
                  ozone_count_lines(ozone->selection_entry_enumeration));

         /* Core association */
         if (show_entry_core)
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
         if (show_entry_playtime)
            ozone_content_metadata_line(
                  video_width,
                  video_height,
                  ozone,
                  &y,
                  column_x,
                  ozone->selection_playtime,
                  text_color,
                  ozone_count_lines(ozone->selection_playtime));

         /* Last played */
         if (show_entry_last_played)
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
                     0.0f,
                     1.0f,
                     col,
                     mymat);
            if (dispctx->blend_end)
               dispctx->blend_end(userdata);
         }
      }
   }
}

static void ozone_draw_backdrop(
      void *userdata,
      void *disp_data,
      unsigned video_width,
      unsigned video_height,
      float alpha)
{
   static float ozone_backdrop[16] = {
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
   };
   static float last_alpha         = 0.0f;

   /* TODO: Replace this backdrop by a blur shader
    * on the whole screen if available */
   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone_backdrop, alpha);
      last_alpha = alpha;
   }

   gfx_display_draw_quad(
         (gfx_display_t*)disp_data,
         userdata,
         video_width,
         video_height,
         0,
         0,
         video_width,
         video_height,
         video_width,
         video_height,
         ozone_backdrop,
         NULL);
}

static void ozone_draw_osk(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height,
      const char *label, const char *str)
{
   unsigned i;
   char message[2048];
   gfx_display_t *p_disp               = (gfx_display_t*)disp_userdata;
   const char *text                    = str;
   unsigned text_color                 = 0xffffffff;
   static float ozone_osk_backdrop[16] = {
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
   };
   static retro_time_t last_time  = 0;
   struct string_list list        = {0};
   float scale_factor             = ozone->last_scale_factor;
   unsigned margin                = 75 * scale_factor;
   unsigned padding               = 10 * scale_factor;
   unsigned bottom_end            = video_height / 2;
   unsigned y_offset              = 0;
   bool draw_placeholder          = string_is_empty(str);
   retro_time_t current_time      = menu_driver_get_current_time();

   if (current_time - last_time >= INTERVAL_OSK_CURSOR)
   {
      if (ozone->flags & OZONE_FLAG_OSK_CURSOR)
         ozone->flags            &= ~OZONE_FLAG_OSK_CURSOR;
      else
         ozone->flags            |=  OZONE_FLAG_OSK_CURSOR;
      last_time                   = current_time;
   }

   /* Border */
   /* Top */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         margin,
         video_width - (margin * 2),
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->entries_border,
         NULL);

   /* Bottom */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         bottom_end - margin,
         video_width - (margin * 2),
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->entries_border,
         NULL);

   /* Left */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         margin,
         ozone->dimensions.spacer_1px,
         bottom_end - (margin * 2),
         video_width,
         video_height,
         ozone->theme->entries_border,
         NULL);

   /* Right */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         video_width - margin,
         margin,
         ozone->dimensions.spacer_1px,
         bottom_end - (margin * 2),
         video_width,
         video_height,
         ozone->theme->entries_border,
         NULL);

   /* Backdrop */
   /* TODO: Remove the backdrop if blur shader is available */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin + ozone->dimensions.spacer_1px,
         margin + ozone->dimensions.spacer_1px,
         video_width - (margin * 2) - ozone->dimensions.spacer_2px,
         bottom_end - (margin * 2) - ozone->dimensions.spacer_2px,
         video_width,
         video_height,
         ozone_osk_backdrop,
         NULL);

   /* Placeholder & text*/
   if (draw_placeholder)
   {
      text        = label;
      text_color  = ozone_theme_light.text_sublabel_rgba;
   }

   (ozone->word_wrap)(message,
         sizeof(message),
         text,
         strlen(text),
         (video_width - (margin * 2) - (padding * 2)) / ozone->fonts.entries_label.glyph_width,
         ozone->fonts.entries_label.wideglyph_width,
         0);

   string_list_initialize(&list);
   string_split_noalloc(&list, message, "\n");

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            msg,
            margin + (padding * 2),
            margin + padding + ozone->fonts.entries_label.line_height + y_offset,
            video_width,
            video_height,
            text_color,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

      /* Cursor */
      if (i == list.size - 1)
      {
         if (ozone->flags & OZONE_FLAG_OSK_CURSOR)
         {
            unsigned cursor_x = draw_placeholder
                  ? 0
                  : font_driver_get_message_width(ozone->fonts.entries_label.font, msg, strlen(msg), 1.0f);
            gfx_display_draw_quad(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  margin
                        + (padding * 2)
                        + cursor_x,
                  margin
                        + padding
                        + y_offset
                        + ozone->fonts.entries_label.line_height
                        - ozone->fonts.entries_label.line_ascender
                        + ozone->dimensions.spacer_3px,
                  ozone->dimensions.spacer_1px,
                  ozone->fonts.entries_label.line_ascender,
                  video_width,
                  video_height,
                  ozone->pure_white,
                  NULL);
         }
      }
      else
         y_offset += 25 * scale_factor;
   }

   /* Keyboard */
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      gfx_display_draw_keyboard(
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->theme->name
                  ? ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_STATIC]
                  : ozone->textures[OZONE_TEXTURE_CURSOR_BORDER],
            ozone->fonts.entries_label.font,
            input_st->osk_grid,
            input_st->osk_ptr,
            ozone->theme->text_rgba);
   }

   string_list_deinitialize(&list);
}

static void ozone_draw_messagebox(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      const char *message,
      math_matrix_4x4 *mymat)
{
   size_t x, y;
   char wrapped_message[MENU_SUBLABEL_MAX_LENGTH];
   int longest_width        = 0;
   int usable_width         = 0;
   struct string_list list  = {0};
   float scale_factor       = 0.0f;
   unsigned i               = 0;
   unsigned y_position      = 0;
   unsigned width           = video_width;
   unsigned height          = video_height;
   gfx_display_ctx_driver_t
         *dispctx           = p_disp->dispctx;

   wrapped_message[0]       = '\0';

   /* Sanity check */
   if (string_is_empty(message) || !ozone->fonts.footer.font)
      return;

   scale_factor             = ozone->last_scale_factor;
   usable_width             = (int)width - (48 * 8 * scale_factor);

   if (usable_width < 1)
      return;

   /* Split message into lines */
   (ozone->word_wrap)(
         wrapped_message,
         sizeof(wrapped_message),
         message,
         strlen(message),
         usable_width / (int)ozone->fonts.footer.glyph_width,
         ozone->fonts.footer.wideglyph_width,
         0);

   string_list_initialize(&list);
   if (     !string_split_noalloc(&list, wrapped_message, "\n")
         || list.elems == 0)
   {
      string_list_deinitialize(&list);
      return;
   }

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list.size * ozone->fonts.footer.line_height) / 2;

   /* find the longest line width */
   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      if (!string_is_empty(msg))
      {
         int width = font_driver_get_message_width(ozone->fonts.footer.font, msg, strlen(msg), 1.0f);

         if (width > longest_width)
            longest_width = width;
      }
   }

   gfx_display_set_alpha(ozone->theme_dynamic.message_background, ozone->animations.messagebox_alpha);

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Avoid drawing a black box if there's no assets */
   if (ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS)
   {
      /* Note: The fact that we use a texture slice here
       * makes things very messy
       * > The actual size and offset of a texture slice
       *   is quite 'loose', and depends upon source image
       *   size, draw size and scale factor... */
      size_t slice_new_w   = longest_width + 48 * 2 * scale_factor;
      size_t slice_new_h   = ozone->fonts.footer.line_height * (list.size + 2);
      unsigned slice_w     = 256;
      int slice_x          = x - longest_width / 2 - 48 * scale_factor;
      int slice_y          = y - ozone->fonts.footer.line_height
            + ((slice_new_h >= slice_w)
                  ? (16.0f * scale_factor)
                  : (16.0f * ((float)slice_new_h / (float)slice_w)));

      gfx_display_draw_texture_slice(
            p_disp,
            userdata,
            video_width,
            video_height,
            slice_x,
            slice_y,
            slice_w,
            slice_w,
            (unsigned)slice_new_w,
            (unsigned)slice_new_h,
            width,
            height,
            ozone->theme_dynamic.message_background,
            16,
            scale_factor,
            ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE],
            mymat
            );
   }

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      if (msg)
         gfx_display_draw_text(
               ozone->fonts.footer.font,
               msg,
               x - (longest_width / 2),
               y + (i * ozone->fonts.footer.line_height) + ozone->fonts.footer.line_ascender,
               width,
               height,
               COLOR_TEXT_ALPHA(ozone->theme->text_rgba, (uint32_t)(ozone->animations.messagebox_alpha*255.0f)),
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               false);
   }

   string_list_deinitialize(&list);
}

static void ozone_hide_fullscreen_thumbnails(ozone_handle_t *ozone, bool animate)
{
   uintptr_t alpha_tag                = (uintptr_t)
         &ozone->animations.fullscreen_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Check whether animations are enabled */
   if (animate && (ozone->animations.fullscreen_thumbnail_alpha > 0.0f))
   {
      gfx_animation_ctx_entry_t animation_entry;

      /* Configure fade out animation */
      animation_entry.easing_enum  = OZONE_EASING_ALPHA;
      animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
      animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
      animation_entry.tag          = alpha_tag;
      animation_entry.target_value = 0.0f;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      gfx_animation_push(&animation_entry);
   }
   /* No animation - just set thumbnail alpha to zero */
   else
      ozone->animations.fullscreen_thumbnail_alpha = 0.0f;

   /* Disable fullscreen thumbnails */
   ozone->flags2 &= ~OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS;
}

static void ozone_show_fullscreen_thumbnails(ozone_handle_t *ozone)
{
   gfx_animation_ctx_entry_t animation_entry;
   struct menu_state *menu_st         = menu_state_get_ptr();
   menu_input_t *menu_input           = &menu_st->input_state;
   menu_list_t *menu_list             = menu_st->entries.list;
   file_list_t *selection_buf         = MENU_LIST_GET_SELECTION(menu_list, 0);
   uintptr_t alpha_tag                = (uintptr_t)&ozone->animations.fullscreen_thumbnail_alpha;
   uintptr_t scroll_tag               = (uintptr_t)selection_buf;

   /* Before showing fullscreen thumbnails, must
    * ensure that any existing fullscreen thumbnail
    * view is disabled... */
   ozone_hide_fullscreen_thumbnails(ozone, false);

   /* Sanity check: Return immediately if this is
    * a menu without thumbnail support, or cursor
    * is currently in the sidebar */
   if (  (!(ozone->flags & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE))
       ||  (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
      return;

   /* We can only enable fullscreen thumbnails if
    * current selection has at least one valid thumbnail
    * and all thumbnails for current selection are already
    * loaded/available */
   if (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER)
   {
      /* imageviewer content requires special treatment,
       * since only the right thumbnail is ever loaded */
      if (     !gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
            && !gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
         return;
   }
   else
   {
      bool left_thumbnail_enabled = gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT);

      if (!left_thumbnail_enabled && !gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
         return;

      if (         (ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE)
               && (left_thumbnail_enabled
               && ((ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_MISSING)
               &&  (ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_AVAILABLE))))
         return;

      if (       (ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_MISSING)
            && (!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
            && (!left_thumbnail_enabled || (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE)))
         return;
   }

   /* Menu list must be stationary while fullscreen
    * thumbnails are shown
    * > Kill any existing scroll animations and
    *   reset scroll acceleration */
   gfx_animation_kill_by_tag(&scroll_tag);
   menu_input->pointer.y_accel    = 0.0f;

   /* Cache selected entry label
    * (used as title when fullscreen thumbnails
    * are shown) */
   if (menu_update_fullscreen_thumbnail_label(
         ozone->fullscreen_thumbnail_label,
         sizeof(ozone->fullscreen_thumbnail_label),
         ozone->is_quick_menu,
         ozone->title) == 0)
      ozone->fullscreen_thumbnail_label[0] = '\0';

   /* Configure fade in animation */
   animation_entry.easing_enum  = OZONE_EASING_ALPHA;
   animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
   animation_entry.tag          = alpha_tag;
   animation_entry.target_value = 1.0f;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   gfx_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   ozone->fullscreen_thumbnail_selection = (size_t)ozone->selection;
   ozone->flags2 |=  OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS;
}

static void ozone_draw_fullscreen_thumbnails(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height)
{
   /* Check whether fullscreen thumbnails are visible */
   if (     (ozone->animations.fullscreen_thumbnail_alpha > 0.0f)
         || (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS))
   {
      gfx_thumbnail_t *right_thumbnail  = &ozone->thumbnails.right;
      gfx_thumbnail_t *left_thumbnail   = &ozone->thumbnails.left;
      unsigned width                    = video_width;
      unsigned height                   = video_height;
      int view_width                    = (int)width;
      gfx_display_t *p_disp             = (gfx_display_t*)disp_userdata;

      int view_height                   = (int)height
            - ozone->dimensions.header_height
            - ozone->dimensions.footer_height
            - ozone->dimensions.spacer_1px;
      int thumbnail_margin              = ozone->dimensions.fullscreen_thumbnail_padding;
      bool show_right_thumbnail         = false;
      bool show_left_thumbnail          = false;
      unsigned num_thumbnails           = 0;
      float right_thumbnail_draw_width  = 0.0f;
      float right_thumbnail_draw_height = 0.0f;
      float left_thumbnail_draw_width   = 0.0f;
      float left_thumbnail_draw_height  = 0.0f;
      float background_alpha            = 0.85f;
      static float background_color[16] = {
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
      };
      int frame_width                   = (int)((float)thumbnail_margin / 3.0f);
      float frame_color[16];
      float separator_color[16];
      int thumbnail_box_width;
      int thumbnail_box_height;
      int right_thumbnail_x;
      int left_thumbnail_x;
      int thumbnail_y;

      /* Sanity check: Return immediately if this is
       * a menu without thumbnails and we are not currently
       * 'fading out' the fullscreen thumbnail view */
      if (   (!(ozone->flags  & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE))
            && (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS))
         goto error;

      /* Sanity check: Return immediately if the view
       * width/height is < 1 */
      if ((view_width < 1) || (view_height < 1))
         goto error;

      /* Get number of 'active' thumbnails */
      show_right_thumbnail = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE);
      show_left_thumbnail  = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_AVAILABLE);

      if ((ozone->is_quick_menu && !string_is_empty(ozone->savestate_thumbnail_file_path))
            || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
      {
         left_thumbnail       = &ozone->thumbnails.savestate;
         show_left_thumbnail  = (left_thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE);
         show_right_thumbnail = false;
      }

      if (show_right_thumbnail)
         num_thumbnails++;

      if (show_left_thumbnail)
         num_thumbnails++;

      /* Prevent screen flashing when browsing in fullscreen thumbnail mode */
      if (     (num_thumbnails < 1)
            && (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS)
            && (  (right_thumbnail->status != GFX_THUMBNAIL_STATUS_MISSING)
               && (left_thumbnail->status  != GFX_THUMBNAIL_STATUS_MISSING)))
      {
         /* Darken background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               0,
               ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
               width,
               (unsigned)view_height,
               width,
               height,
               background_color,
               NULL);
         return;
      }

      /* Do nothing if both thumbnails are missing
       * > Note: Baring inexplicable internal errors, this
       *   can never happen...
       * > Return instead of error to keep fullscreen
       *   mode after menu/fullscreen toggle */
      if (num_thumbnails < 1 &&
            (right_thumbnail->status == GFX_THUMBNAIL_STATUS_MISSING &&
             left_thumbnail->status  == GFX_THUMBNAIL_STATUS_MISSING))
         return;

      /* Get base thumbnail dimensions + draw positions */

      /* > Thumbnail bounding box height + y position
       *   are fixed */
      thumbnail_box_height = view_height - (thumbnail_margin * 2);
      thumbnail_y          = ozone->dimensions.header_height + thumbnail_margin + ozone->dimensions.spacer_1px;

      /* Thumbnail bounding box width and x position
       * depend upon number of active thumbnails */
      if (num_thumbnails == 2)
      {
         thumbnail_box_width = (view_width - (thumbnail_margin * 3) - frame_width) >> 1;
         left_thumbnail_x    = thumbnail_margin;
         right_thumbnail_x   = left_thumbnail_x + thumbnail_box_width + frame_width + thumbnail_margin;
      }
      else
      {
         thumbnail_box_width = view_width - (thumbnail_margin * 2);
         left_thumbnail_x    = thumbnail_margin;
         right_thumbnail_x   = left_thumbnail_x;
      }

      /* Sanity check */
      if ((thumbnail_box_width  < 1) ||
          (thumbnail_box_height < 1))
         goto error;

      /* Get thumbnail draw dimensions
       * > Note: The following code is a bit awkward, since
       *   we have to do things in a very specific order
       *   - i.e. we cannot determine proper thumbnail
       *     layout until we have thumbnail draw dimensions.
       *     and we cannot get draw dimensions until we have
       *     the bounding box dimensions...  */
      if (show_right_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               right_thumbnail,
               thumbnail_box_width,
               thumbnail_box_height,
               1.0f,
               &right_thumbnail_draw_width,
               &right_thumbnail_draw_height);

         /* Sanity check */
         if ((right_thumbnail_draw_width  <= 0.0f) ||
             (right_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      if (show_left_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               left_thumbnail,
               thumbnail_box_width,
               thumbnail_box_height,
               1.0f,
               &left_thumbnail_draw_width,
               &left_thumbnail_draw_height);

         /* Sanity check */
         if ((left_thumbnail_draw_width  <= 0.0f) ||
             (left_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      /* Adjust thumbnail draw positions to achieve
       * uniform appearance (accounting for actual
       * draw dimensions...) */
      if (num_thumbnails == 2)
      {
         int left_padding  = (thumbnail_box_width - (int)left_thumbnail_draw_width)  >> 1;
         int right_padding = (thumbnail_box_width - (int)right_thumbnail_draw_width) >> 1;

         /* Move thumbnails as close together as possible,
          * and horizontally centre the resultant 'block'
          * of images */
         left_thumbnail_x  += right_padding;
         right_thumbnail_x -= left_padding;
      }

      /* Set colour values */

      /* > Background */
      gfx_display_set_alpha(
            background_color,
            background_alpha * ozone->animations.fullscreen_thumbnail_alpha);

      /* > Separators */
      memcpy(separator_color, ozone->theme->header_footer_separator, sizeof(separator_color));
      gfx_display_set_alpha(
            separator_color,
            ozone->animations.fullscreen_thumbnail_alpha);

      /* > Thumbnail frame */
      memcpy(frame_color, ozone->theme->sidebar_background, sizeof(frame_color));
      gfx_display_set_alpha(
            frame_color,
            ozone->animations.fullscreen_thumbnail_alpha);

      /* Darken background */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            width,
            (unsigned)view_height,
            width,
            height,
            background_color,
            NULL);

      /* Draw full-width separators */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            ozone->dimensions.header_height,
            width,
            ozone->dimensions.spacer_1px,
            width,
            height,
            separator_color,
            NULL);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            height - ozone->dimensions.footer_height,
            width,
            ozone->dimensions.spacer_1px,
            width,
            height,
            separator_color,
            NULL);

      /* Draw thumbnails */

      /* > Right */
      if (show_right_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               right_thumbnail_x
                     - frame_width
                     + ((thumbnail_box_width  - (int)right_thumbnail_draw_width)  >> 1),
               thumbnail_y
                     - frame_width
                     + ((thumbnail_box_height - (int)right_thumbnail_draw_height) >> 1),
               (unsigned)right_thumbnail_draw_width  + (frame_width << 1),
               (unsigned)right_thumbnail_draw_height + (frame_width << 1),
               width,
               height,
               frame_color,
               NULL);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               right_thumbnail,
               right_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               ozone->animations.fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }

      /* > Left */
      if (show_left_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               left_thumbnail_x
                     - frame_width
                     + ((thumbnail_box_width  - (int)left_thumbnail_draw_width)  >> 1),
               thumbnail_y
                     - frame_width
                     + ((thumbnail_box_height - (int)left_thumbnail_draw_height) >> 1),
               (unsigned)left_thumbnail_draw_width  + (frame_width << 1),
               (unsigned)left_thumbnail_draw_height + (frame_width << 1),
               width,
               height,
               frame_color,
               NULL);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               left_thumbnail,
               left_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               ozone->animations.fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }
   }

   return;

error:
   /* If fullscreen thumbnails are enabled at
    * this point, must disable them immediately... */
   if (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
      ozone_hide_fullscreen_thumbnails(ozone, false);
}

static void ozone_set_thumbnail_content(void *data, const char *s)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;

   if (!ozone)
      return;

   if (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
   {
      /* Playlist content */
      if (string_is_empty(s))
      {
         size_t selection      = menu_st->selection_ptr;
         size_t list_size      = MENU_LIST_GET_SELECTION(menu_list, 0)->size;
         file_list_t *list     = MENU_LIST_GET_SELECTION(menu_list, 0);
         playlist_t *pl        = NULL;

         /* Get playlist index corresponding
          * to the selected entry */
         if (      list
               && (selection < list_size)
               && (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
         {
            selection = list->list[selection].entry_idx;
            pl        = playlist_get_cached();
         }

         gfx_thumbnail_set_content_playlist(menu_st->thumbnail_path_data,
               pl, selection);

         switch (ozone->tabs[ozone->categories_selection_ptr])
         {
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            case OZONE_SYSTEM_TAB_VIDEO:
#endif
            case OZONE_SYSTEM_TAB_MUSIC:
               if (ozone->categories_selection_ptr <= ozone->system_tab_end)
                  ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
               break;

            default:
               ozone->flags |=  OZONE_FLAG_WANT_THUMBNAIL_BAR;
               break;
         }
      }
   }
   else if (ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST)
   {
      /* Database list content */
      if (string_is_empty(s))
      {
         menu_entry_t entry;

         MENU_ENTRY_INITIALIZE(entry);
         entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED;
         menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

         if (!string_is_empty(entry.path))
            gfx_thumbnail_set_content(menu_st->thumbnail_path_data, entry.path);
      }
   }
#if defined(HAVE_LIBRETRODB)
   else if (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
   {
      /* Explore list */
      if (string_is_empty(s))
      {
         /* Selected entry */
         menu_entry_t entry;

         MENU_ENTRY_INITIALIZE(entry);
         entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED;
         menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

         if (menu_explore_set_playlist_thumbnail(entry.type, menu_st->thumbnail_path_data) >= 0)
            ozone->flags |=  OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
         else
            ozone->flags &= ~OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;

         if (ozone->flags & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE)
            ozone->flags |=  OZONE_FLAG_WANT_THUMBNAIL_BAR;
         else
            ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
      }
   }
#endif
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      size_t selection           = menu_st->selection_ptr;
      file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_list, 0);
      ozone_node_t *node         = (ozone_node_t*)selection_buf->list[selection].userdata;

      if (node)
      {
         menu_entry_t entry;

         MENU_ENTRY_INITIALIZE(entry);
         entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED;
         menu_entry_get(&entry, 0, selection, NULL, true);

         if (!string_is_empty(entry.path) && !string_is_empty(node->fullpath))
            gfx_thumbnail_set_content_image(menu_st->thumbnail_path_data, node->fullpath, entry.path);
      }
   }
   else if (!string_is_empty(s))
   {
      /* Annoying leftovers...
       * This is required to ensure that thumbnails are
       * updated correctly when navigating deeply through
       * the sublevels of database manager lists.
       * Showing thumbnails on database entries is a
       * pointless nuisance and a waste of CPU cycles, IMHO... */
      gfx_thumbnail_set_content(menu_st->thumbnail_path_data, s);
   }

   ozone_update_content_metadata(ozone);
}

/* Returns true if specified category is currently
 * displayed on screen */
static bool INLINE ozone_category_onscreen(
      ozone_handle_t *ozone,
      size_t idx)
{
   return    (idx >= ozone->first_onscreen_category)
          && (idx <= ozone->last_onscreen_category);
}

/* If current category is on screen, returns its
 * index. If current category is off screen, returns
 * index of centremost on screen category. */
static size_t ozone_get_onscreen_category_selection(
      ozone_handle_t *ozone)
{
   /* Check whether selected category is already on screen */
   if (ozone_category_onscreen(ozone, ozone->categories_selection_ptr))
      return ozone->categories_selection_ptr;

   /* Return index of centremost category */
   return (ozone->first_onscreen_category >> 1) + (ozone->last_onscreen_category >> 1);
}

/* If currently selected entry is off screen,
 * moves selection to specified on screen target
 * > Does nothing if currently selected item is
 *   already on screen */
static void ozone_auto_select_onscreen_entry(
      ozone_handle_t *ozone,
      enum ozone_onscreen_entry_position_type target_entry)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   /* Update selection index */
   switch (target_entry)
   {
      case OZONE_ONSCREEN_ENTRY_FIRST:
         menu_st->selection_ptr = ozone->first_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_LAST:
         menu_st->selection_ptr = ozone->last_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_CENTRE:
      default:
         menu_st->selection_ptr = (ozone->first_onscreen_entry >> 1) + (ozone->last_onscreen_entry >> 1);
         break;
   }
}

static bool INLINE ozone_metadata_override_available(ozone_handle_t *ozone, settings_t *settings)
{
   /* Ugly construct...
    * Content metadata display override may be
    * toggled if the following are true:
    * - We are viewing playlist thumbnails
    * - This is *not* an image viewer playlist
    * - Both right and left thumbnails are
    *   enabled/available
    * Short circuiting means that in most cases
    * only OZONE_FLAG_IS_PLAYLIST will be evaluated,
    * so this isn't too much of a performance hog... */
   return (
               (ozone->flags   & OZONE_FLAG_IS_PLAYLIST)
            || (ozone->flags   & OZONE_FLAG_IS_EXPLORE_LIST)
         )
         && (ozone->show_thumbnail_bar)
         && (settings->uints.menu_left_thumbnails)
         && (!(ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER))
         && (   ozone->thumbnails.left.status == GFX_THUMBNAIL_STATUS_AVAILABLE ||
               (ozone->thumbnails.left.status       < GFX_THUMBNAIL_STATUS_AVAILABLE &&
               (ozone->thumbnails_left_status_prev <= GFX_THUMBNAIL_STATUS_AVAILABLE)))
         && (   ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE ||
               (ozone->thumbnails.right.status       < GFX_THUMBNAIL_STATUS_AVAILABLE &&
               (ozone->thumbnails_right_status_prev <= GFX_THUMBNAIL_STATUS_AVAILABLE)));
}

static bool INLINE ozone_fullscreen_thumbnails_available(ozone_handle_t *ozone,
      struct menu_state *menu_st)
{
   bool ret =
         ( (ozone->flags & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE))
      && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
      && (ozone->show_thumbnail_bar)
      && (gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
      ||  gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      && (  (ozone->thumbnails.left.status == GFX_THUMBNAIL_STATUS_AVAILABLE
            || (ozone->thumbnails.left.status       < GFX_THUMBNAIL_STATUS_AVAILABLE
            &&  ozone->thumbnails_left_status_prev <= GFX_THUMBNAIL_STATUS_AVAILABLE))
         || (ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE
            || (ozone->thumbnails.right.status       < GFX_THUMBNAIL_STATUS_AVAILABLE
            &&  ozone->thumbnails_right_status_prev <= GFX_THUMBNAIL_STATUS_AVAILABLE)));

   if (!string_is_empty(ozone->savestate_thumbnail_file_path) &&
         ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE)
      ret = true;

   return ret;
}

static bool ozone_help_available(ozone_handle_t *ozone, size_t current_selection, bool menu_show_sublabels)
{
   menu_entry_t last_entry;
   char help_msg[255];
   help_msg[0] = '\0';

   MENU_ENTRY_INITIALIZE(last_entry);
   last_entry.flags |= MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   /* If sublabels are not visible, they can be displayed as help. */
   if (!menu_show_sublabels && !string_is_empty(last_entry.sublabel))
      return true;

   /* Otherwise check if actual help is available. */
   msg_hash_get_help_enum(last_entry.enum_idx, help_msg, sizeof(help_msg));
   if (string_is_equal(help_msg, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
      return false;

   return !(
               (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            || (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
         );
}

static bool ozone_clear_available(ozone_handle_t *ozone, size_t current_selection)
{
   menu_entry_t last_entry;

   MENU_ENTRY_INITIALIZE(last_entry);
   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   if (last_entry.setting_type == ST_BIND)
   {
      return true;
   }
   else if (last_entry.type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && last_entry.type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      return true;
   }
   else if (last_entry.type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
         && last_entry.type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      return true;
   }
   return false;
}

static bool ozone_scan_available(ozone_handle_t *ozone, size_t current_selection)
{
   menu_entry_t last_entry;

   MENU_ENTRY_INITIALIZE(last_entry);
   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   switch (last_entry.type)
   {
      case FILE_TYPE_DIRECTORY:
         return true;
      case FILE_TYPE_CARCHIVE:
      case FILE_TYPE_PLAIN:
         return true;
   }
   return false;
}

static bool ozone_manage_available(ozone_handle_t *ozone, size_t current_selection)
{
   menu_entry_t last_entry;

   if (     (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         && (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
         && ozone->categories_selection_ptr > ozone->system_tab_end)
      return true;

   MENU_ENTRY_INITIALIZE(last_entry);
   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   switch (last_entry.type)
   {
      case FILE_TYPE_PLAYLIST_COLLECTION:
         return true;
   }
   return false;
}

static bool ozone_is_current_entry_settings(size_t current_selection)
{
   menu_entry_t last_entry;
   const char *entry_value;

   unsigned entry_type                = 0;
   enum msg_file_type entry_file_type = FILE_TYPE_NONE;

   MENU_ENTRY_INITIALIZE(last_entry);
   last_entry.flags |= MENU_ENTRY_FLAG_VALUE_ENABLED;

   menu_entry_get(&last_entry, 0, current_selection, NULL, true);

   if (last_entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      entry_value = last_entry.password_value;
   else
      entry_value = last_entry.value;

   entry_file_type = msg_hash_to_file_type(msg_hash_calculate(entry_value));
   entry_type      = last_entry.type;

   /* Logic below taken from materialui_pointer_up_swipe_horz_default */
   if (!string_is_empty(entry_value))
   {
      /* Toggle switch off */
      if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
          string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      {
         return true;
      }
      /* Toggle switch on */
      else if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
               string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))
      {
         return true;
      }
      /* Normal value text */
      else
      {
         switch (entry_file_type)
         {
            case FILE_TYPE_IN_CARCHIVE:
            case FILE_TYPE_MORE:
            case FILE_TYPE_CORE:
            case FILE_TYPE_DIRECT_LOAD:
            case FILE_TYPE_RDB:
            case FILE_TYPE_PLAIN:
            case FILE_TYPE_DIRECTORY:
            case FILE_TYPE_MUSIC:
            case FILE_TYPE_IMAGE:
            case FILE_TYPE_MOVIE:
               break;
            case FILE_TYPE_COMPRESSED:
               /* Note that we have to perform a backup check here,
                * since the 'manual content scan - file extensions'
                * setting may have a value of 'zip' or '7z' etc, which
                * means it would otherwise get incorreclty identified as
                * an archive file... */
               if (entry_type != FILE_TYPE_CARCHIVE)
                  return true;
               break;
            default:
               return true;
               break;
         }
      }
   }

   return false;
}

static void ozone_toggle_metadata_override(ozone_handle_t *ozone)
{
   gfx_animation_ctx_entry_t animation_entry;
   uintptr_t alpha_tag         = (uintptr_t)&ozone->animations.left_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Set common animation parameters */
   animation_entry.easing_enum = OZONE_EASING_ALPHA;
   animation_entry.duration    = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.subject     = &ozone->animations.left_thumbnail_alpha;
   animation_entry.tag         = alpha_tag;
   animation_entry.cb          = NULL;
   animation_entry.userdata    = NULL;

   /* Check whether metadata override is
    * currently enabled */
   if (ozone->flags & OZONE_FLAG_FORCE_METADATA_DISPLAY)
   {
      /* Thumbnail will fade in */
      animation_entry.target_value  = 1.0f;
      ozone->flags                 &= ~OZONE_FLAG_FORCE_METADATA_DISPLAY;
   }
   else
   {
      /* Thumbnail will fade out */
      animation_entry.target_value  = 0.0f;
      ozone->flags                 |=  OZONE_FLAG_FORCE_METADATA_DISPLAY;
   }

   /* Push animation */
   gfx_animation_push(&animation_entry);
}

/**
 * Starts the cursor wiggle animation in the given direction
 * Use ozone_get_cursor_wiggle_offset to read the animation
 * once it has started
 */
static void ozone_start_cursor_wiggle(
      ozone_handle_t* ozone,
      enum menu_action direction)
{
   /* Don't start another wiggle animation on top of another */
   if (!ozone || (ozone->flags2 & OZONE_FLAG2_CURSOR_WIGGLING))
      return;

   /* Don't allow wiggling in invalid directions */
   if (!(
            direction == MENU_ACTION_UP
         || direction == MENU_ACTION_DOWN
         || direction == MENU_ACTION_LEFT
         || direction == MENU_ACTION_RIGHT
        ))
      return;

   /* Start wiggling */
   ozone->cursor_wiggle_state.start_time = menu_driver_get_current_time() / 1000;
   ozone->cursor_wiggle_state.direction  = direction;
   ozone->cursor_wiggle_state.amplitude  = rand() % 15 + 10;
   ozone->flags2                        |= OZONE_FLAG2_CURSOR_WIGGLING;
}

static void ozone_set_thumbnail_delay(bool on)
{
   if (on)
   {
      gfx_thumbnail_set_stream_delay(OZONE_THUMBNAIL_STREAM_DELAY);
      gfx_thumbnail_set_fade_duration(-1.0f);
   }
   else
   {
      gfx_thumbnail_set_stream_delay(0);
      gfx_thumbnail_set_fade_duration(1);
   }
}

/* Common thumbnail switch requires FILE_TYPE_RPL_ENTRY,
 * which only works with playlists, therefore activate it
 * manually for Quick Menu, Explore and Database */
extern int action_switch_thumbnail(
      const char *path,
      const char *label,
      unsigned type,
      size_t idx);

static enum menu_action ozone_parse_menu_entry_action(
      ozone_handle_t *ozone,
      bool menu_navigation_wraparound_enable,
      bool ozone_collapse_sidebar,
      enum menu_action action)
{
   uintptr_t tag;
   int new_selection;
   size_t selection;
   size_t selection_total;
   bool is_current_entry_settings = false;
   struct menu_state *menu_st     = menu_state_get_ptr();
   menu_list_t *menu_list         = menu_st->entries.list;
   size_t menu_stack_size         = MENU_LIST_GET_STACK_SIZE(menu_list, 0);
   settings_t *settings           = config_get_ptr();
   enum menu_action new_action    = action;
   file_list_t *selection_buf     = NULL;
   unsigned horizontal_list_size  = 0;

   /* We have to override the thumbnail stream
    * delay when opening the thumbnail sidebar;
    * ensure that the proper value is restored
    * whenever the user performs regular navigation */
   if (   (action != MENU_ACTION_NOOP)
       && (ozone->thumbnails.stream_delay != OZONE_THUMBNAIL_STREAM_DELAY))
   {
      ozone->thumbnails.stream_delay = OZONE_THUMBNAIL_STREAM_DELAY;
      gfx_thumbnail_set_stream_delay(ozone->thumbnails.stream_delay);
   }

   horizontal_list_size       = (unsigned)ozone->horizontal_list.size;

   if (menu_input_dialog_get_display_kb())
      ozone->flags           |=  OZONE_FLAG_MSGBOX_STATE;
   else
      ozone->flags           &= ~OZONE_FLAG_MSGBOX_STATE;
   selection_buf              = MENU_LIST_GET_SELECTION(menu_list, 0);
   tag                        = (uintptr_t)selection_buf;
   selection                  = menu_st->selection_ptr;
   selection_total            = MENU_LIST_GET_SELECTION(menu_list, 0)->size;

   /* Don't wiggle left or right if the current entry is a setting. This is
      partially wrong because some settings don't use left and right to change their value, such as
      free input fields (passwords...). This is good enough. */
   is_current_entry_settings = ozone_is_current_entry_settings(selection);

   /* Scan user inputs */
   switch (action)
   {
      case MENU_ACTION_START:
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         if (     (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
               || (ozone->is_quick_menu && menu_is_running_quick_menu()))
            break;

         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            char playlist_path[PATH_MAX_LENGTH];
            struct menu_state *menu_st = menu_state_get_ptr();
            size_t new_selection       = 0;
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t tab_selection       = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            if (tab_selection < (size_t)(ozone->system_tab_end + 1))
               break;

            new_selection = tab_selection - ozone->system_tab_end - 1;
            new_action    = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;

            fill_pathname_join(playlist_path,
                  settings->paths.directory_playlist,
                  ozone->horizontal_list.list[new_selection].path,
                  sizeof(playlist_path));

            generic_action_ok_displaylist_push(
                  playlist_path,
                  NULL,
                  msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS),
                  MENU_SETTING_ACTION,
                  ozone->tab_selection[ozone->categories_selection_ptr],
                  0,
                  ACTION_OK_DL_PLAYLIST_MANAGER_SETTINGS);

            ozone->flags &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR;
            ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
            ozone->pending_cursor_in_sidebar = true;

            ozone_refresh_sidebars(ozone, ozone_collapse_sidebar, ozone->last_height);
            if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
               ozone_leave_sidebar(ozone, ozone_collapse_sidebar, tag);

            menu_st->selection_ptr = 0;
            ozone_selection_changed(ozone, false);
            break;
         }

         /* If this is a menu with thumbnails and cursor
          * is not in the sidebar, attempt to show
          * fullscreen thumbnail view */
         if (        ozone_fullscreen_thumbnails_available(ozone, menu_st)
               && (!(ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS))
               && (!(ozone->flags  & OZONE_FLAG_CURSOR_IN_SIDEBAR)))
         {
            ozone_show_fullscreen_thumbnails(ozone);
            ozone->flags2 |= OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            ozone_set_thumbnail_delay(false);
            new_action     = MENU_ACTION_NOOP;
         }
         else if ((ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               || (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS))
         {
            ozone_set_thumbnail_delay(true);
            ozone_hide_fullscreen_thumbnails(ozone, true);
            ozone->flags2 &= ~OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            new_action     = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_SCAN:
         ozone->flags      &= ~(OZONE_FLAG_SKIP_THUMBNAIL_RESET
                              | OZONE_FLAG_CURSOR_MODE);

         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            new_action     = MENU_ACTION_NOOP;
            break;
         }

         if (       (ozone->flags  & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE)
               && (!(ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS))
               && ( (ozone->flags  & OZONE_FLAG_IS_STATE_SLOT)
               ||   (ozone->is_quick_menu && !string_is_empty(ozone->savestate_thumbnail_file_path))))
         {
            ozone_show_fullscreen_thumbnails(ozone);
            ozone->flags2 |= OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            new_action     = MENU_ACTION_NOOP;
         }
         else if (  (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && ( (ozone->flags  & OZONE_FLAG_IS_STATE_SLOT) ||
                    (ozone->is_quick_menu && menu_is_running_quick_menu())))
         {
            ozone_hide_fullscreen_thumbnails(ozone, true);
            ozone->flags2 &= ~OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            new_action     = MENU_ACTION_NOOP;
         }
         else if (  (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
               ||   (ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST)
               ||   (ozone->is_quick_menu))
         {
            action_switch_thumbnail(NULL, NULL, 0, 0);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_DOWN:
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection   = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            new_selection      = (int)(selection + 1);

            if (new_selection >= (int)(ozone->system_tab_end + horizontal_list_size + 1))
               new_selection   = 0;

            ozone_sidebar_goto(ozone, new_selection);
            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(false);
#endif
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == selection_total - 1)
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);

         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && (ozone->is_quick_menu))
            return MENU_ACTION_NOOP;

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->flags & OZONE_FLAG_CURSOR_MODE)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         break;
      case MENU_ACTION_UP:
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection   = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            if ((new_selection = (int)selection - 1) < 0)
               new_selection   = horizontal_list_size + ozone->system_tab_end;

            ozone_sidebar_goto(ozone, new_selection);
            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(true);
#endif
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == 0)
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_UP);

         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && (ozone->is_quick_menu))
            return MENU_ACTION_NOOP;

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->flags & OZONE_FLAG_CURSOR_MODE)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         break;
      case MENU_ACTION_LEFT:
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            new_action      = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_LEFT);
            break;
         }
         else if (ozone->depth > 1)
         {
            /* Pressing left goes up but faster, so
               wiggle up to say that there is nothing more upwards
               even though the user pressed the left button */
            if (!menu_navigation_wraparound_enable && selection == 0 && !is_current_entry_settings)
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);

            if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
                  && (ozone->is_quick_menu && !menu_is_running_quick_menu()))
               return MENU_ACTION_NOOP;

            break;
         }
         else if ((ozone->depth == 1)
               && (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && (ozone->flags  & OZONE_FLAG_IS_PLAYLIST))
         {
            ozone_hide_fullscreen_thumbnails(ozone, true);
            ozone->flags2 &= ~OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
         }

         ozone_go_to_sidebar(ozone, ozone_collapse_sidebar, tag);
         new_action    = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         break;
      case MENU_ACTION_RIGHT:
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
         {
            if (ozone->depth == 1)
            {
               new_action = MENU_ACTION_NOOP;
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_RIGHT);
            }
            /* Pressing right goes down but faster, so
               wiggle down to say that there is nothing more downwards
               even though the user pressed the right button */
            else if (!menu_navigation_wraparound_enable
                  && selection == (selection_total - 1)
                  && !is_current_entry_settings)
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);

            if (         (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
                  && (   (ozone->flags  & OZONE_FLAG_IS_PLAYLIST)
                      || (ozone->is_quick_menu && !menu_is_running_quick_menu())))
               return MENU_ACTION_NOOP;

            break;
         }

         if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
            ozone_leave_sidebar(ozone, ozone_collapse_sidebar, tag);

         new_action    = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
         break;
      case MENU_ACTION_OK:
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;

         if (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
            ozone->flags |= OZONE_FLAG_SKIP_THUMBNAIL_RESET;

         /* Open fullscreen thumbnail with Ok when core is running
            to prevent accidental imageviewer core launch */
         if (     (ozone->flags & OZONE_FLAG_LIBRETRO_RUNNING)
               && (ozone->flags & OZONE_FLAG_IS_FILE_LIST)
               && (ozone->flags & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE)
               && (ozone->show_thumbnail_bar))
         {
            /* Allow launch if already using "imageviewer" core */
            if (string_is_equal(runloop_state_get_ptr()->system.info.library_name, "image display"))
               break;

            if (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               ozone_hide_fullscreen_thumbnails(ozone, true);
            else
               ozone_show_fullscreen_thumbnails(ozone);
            new_action     = MENU_ACTION_NOOP;
            break;
         }

         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               || (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS))
         {
            ozone_set_thumbnail_delay(true);
            ozone_hide_fullscreen_thumbnails(ozone, true);
            ozone->flags2 &= ~OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            if (     (!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
                  && (!(ozone->flags & OZONE_FLAG_IS_PLAYLIST))
                  && (!(ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)))
               new_action = MENU_ACTION_NOOP;
            break;
         }

         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            ozone_refresh_sidebars(ozone, ozone_collapse_sidebar, ozone->last_height);
            if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
               ozone_leave_sidebar(ozone, ozone_collapse_sidebar, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
            break;
         }
         break;
      case MENU_ACTION_CANCEL:
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;

         if (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
            ozone->flags |= OZONE_FLAG_SKIP_THUMBNAIL_RESET;

         /* If this is a playlist, handle 'backing out'
          * of a search, if required */
         if (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            if (menu_entries_search_get_terms())
               break;

         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            /* Go back to main menu tab */
            if (ozone->categories_selection_ptr != 0)
            {
               ozone_sidebar_goto(ozone, 0);
#ifdef HAVE_AUDIOMIXER
               audio_driver_mixer_play_scroll_sound(true);
#endif
            }
            else
            {
               /* Jump to first item on Main Menu */
               ozone->tab_selection[ozone->categories_selection_ptr] = 0;
               menu_st->selection_ptr = 0;
            }

            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               || (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS))
         {
            ozone_set_thumbnail_delay(true);
            ozone_hide_fullscreen_thumbnails(ozone, true);
            ozone->flags  &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;
            ozone->flags2 &= ~OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS;
            new_action     = MENU_ACTION_NOOP;
            break;
         }

         if (menu_stack_size == 1)
         {
            ozone_go_to_sidebar(ozone, ozone_collapse_sidebar, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         }

         /* Return from manage playlist quick access back to sidebar */
         if (ozone->pending_cursor_in_sidebar && ozone->depth == 2)
         {
            ozone->pending_cursor_in_sidebar = false;
            ozone->flags |= OZONE_FLAG_CURSOR_IN_SIDEBAR;
            ozone_sidebar_goto(ozone, ozone->categories_selection_ptr);
         }
         break;

      case MENU_ACTION_SCROLL_UP:
         /* Descend 10 items or to previous alphabet (Z towards A) */

         /* Scroll sidebar tabs */
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            struct menu_state *menu_st = menu_state_get_ptr();

            /* If cursor is active, ensure we target
             * an on screen category */
            size_t tab_selection       = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            new_selection              = (int)tab_selection;

            if (menu_st->scroll.mode == MENU_SCROLL_PAGE)
               new_selection           = (int)(tab_selection - 10);
            else if (ozone->sidebar_index_size)
            {
               /* Alphabetical scroll */
               size_t l                = ozone->sidebar_index_size - 1;

               while (l
                     && ozone->sidebar_index_list[l - 1] >= tab_selection)
                  l--;

               if (l > 0)
                  new_selection        = ozone->sidebar_index_list[l - 1];
            }

            if (tab_selection < (size_t)(ozone->system_tab_end + 1))
               new_selection           = 0;
            else if ((int)tab_selection > (int)ozone->system_tab_end - new_selection
                  || new_selection < 0)
               new_selection           = (int)(ozone->system_tab_end + 1);

            if (new_selection != (int)tab_selection)
               ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_NOOP;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(true);
#endif
            break;
         }
         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && (ozone->is_quick_menu))
            return MENU_ACTION_NOOP;

         /* If pointer is active and current selection
          * is off screen, auto select *last* item */
         if (ozone->flags & OZONE_FLAG_CURSOR_MODE)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_LAST);
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         break;
      case MENU_ACTION_SCROLL_DOWN:
         /* Ascend 10 items or to next alphabet (A towards Z) */

         /* Scroll sidebar tabs */
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            struct menu_state *menu_st = menu_state_get_ptr();

            /* If cursor is active, ensure we target
             * an on screen category */
            size_t tab_selection       = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            new_selection              = (int)tab_selection;

            if (menu_st->scroll.mode == MENU_SCROLL_PAGE)
               new_selection           = (int)(tab_selection + 10);
            else
            {
               /* Alphabetical scroll */
               size_t l                = 0;

               while (  (l < (size_t)(ozone->sidebar_index_size - 1))
                     && (ozone->sidebar_index_list[l + 1] <= tab_selection))
                  l++;

               if (l < (size_t)(ozone->sidebar_index_size - 1))
                  new_selection        = ozone->sidebar_index_list[l + 1];
               else if (l == (size_t)(ozone->sidebar_index_size - 1))
                  new_selection        = ozone->system_tab_end + horizontal_list_size;

               if (tab_selection < (size_t)(ozone->system_tab_end + 1))
                  new_selection        = ozone->system_tab_end + 1;
            }

            if (new_selection > (int)(ozone->system_tab_end + horizontal_list_size))
               new_selection           = (int)(ozone->system_tab_end + horizontal_list_size);

            if (new_selection != (int)tab_selection)
               ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_NOOP;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(false);
#endif
            break;
         }
         if (     (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
               && (ozone->is_quick_menu))
            return MENU_ACTION_NOOP;

         /* If pointer is active and current selection
          * is off screen, auto select *first* item */
         if (ozone->flags & OZONE_FLAG_CURSOR_MODE)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_FIRST);
         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         break;

      case MENU_ACTION_SCROLL_HOME:
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t tab_selection       = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            new_selection              = 0;

            if (tab_selection > ozone->system_tab_end)
               new_selection           = (int)(ozone->system_tab_end + 1);

            if (new_selection != (int)tab_selection)
               ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_NOOP;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(true);
#endif
            break;
         }
         break;
      case MENU_ACTION_SCROLL_END:
         if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t tab_selection       = (ozone->flags & OZONE_FLAG_CURSOR_MODE)
                  ? ozone_get_onscreen_category_selection(ozone)
                  : ozone->categories_selection_ptr;

            new_selection              = ozone->system_tab_end + horizontal_list_size;

            if (new_selection != (int)tab_selection)
               ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_NOOP;
            ozone->flags      &= ~OZONE_FLAG_CURSOR_MODE;

#ifdef HAVE_AUDIOMIXER
            if (new_selection != (int)selection)
               audio_driver_mixer_play_scroll_sound(false);
#endif
            break;
         }
         break;
      case MENU_ACTION_INFO:
         /* If we currently viewing a playlist with
          * dual thumbnails, toggle the content metadata
          * override */
         if (ozone_metadata_override_available(ozone, settings))
         {
            ozone_toggle_metadata_override(ozone);
            new_action = MENU_ACTION_NOOP;
         }
         /* ...and since the user is likely to trigger
          * 'INFO' actions on invalid playlist entries,
          * suppress this action entirely when viewing
          * playlists under all other conditions
          * > Playlists have no 'INFO' entries - the
          *   user is just greeted with a useless
          *   'no information available' message
          * > It is incredibly annoying to inadvertently
          *   trigger this message when you just want to
          *   toggle metadata... */
         else if ((ozone->flags & OZONE_FLAG_IS_PLAYLIST)
               && (ozone->show_thumbnail_bar))
            new_action = MENU_ACTION_NOOP;

         ozone->flags &= ~OZONE_FLAG_CURSOR_MODE;
         break;
      default:
         /* In all other cases, pass through input
          * menu action without intervention */
         break;
   }

   return new_action;
}

/* Menu entry action callback */
static int ozone_menu_entry_action(
      void *userdata,
      menu_entry_t *entry,
      size_t i,
      enum menu_action action)
{
   menu_entry_t new_entry;
   ozone_handle_t *ozone       = (ozone_handle_t*)userdata;
   struct menu_state *menu_st  = menu_state_get_ptr();
   settings_t *settings        = config_get_ptr();
   menu_entry_t *entry_ptr     = entry;
   size_t selection            = i;
   /* Process input action */
   enum menu_action new_action = ozone_parse_menu_entry_action(ozone,
         settings->bools.menu_navigation_wraparound_enable,
         settings->bools.ozone_collapse_sidebar,
         action);
   /* Check whether current selection has changed
    * (due to automatic on screen entry selection...) */
   size_t new_selection        = menu_st->selection_ptr;

   if (new_selection != selection)
   {
      /* Selection has changed - must update
       * entry pointer */
      MENU_ENTRY_INITIALIZE(new_entry);
      menu_entry_get(&new_entry, 0, new_selection, NULL, true);
      entry_ptr                = &new_entry;
   }

   /* Call standard generic_menu_entry_action() function */
   return generic_menu_entry_action(userdata,
         entry_ptr,
         new_selection,
         new_action);
}

static void ozone_menu_animation_update_time(
      float *ticker_pixel_increment,
      unsigned video_width,
      unsigned video_height)
{
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();
   /* Ozone uses DPI scaling
    * > Smooth ticker scaling multiplier is
    *   gfx_display_get_dpi_scale() multiplied by
    *   a small correction factor to achieve a
    *   default scroll speed equal to that of the
    *   non-smooth ticker */
   *(ticker_pixel_increment) *= gfx_display_get_dpi_scale(p_disp,
         settings, video_width, video_height, false, false) * 0.5f;
}

static void *ozone_init(void **userdata, bool video_is_threaded)
{
   unsigned i;
   bool fallback_color_theme           = false;
   unsigned width, height, color_theme = 0;
   ozone_handle_t *ozone               = NULL;
   settings_t *settings                = config_get_ptr();
   gfx_animation_t *p_anim             = anim_get_ptr();
   gfx_display_t *p_disp               = disp_get_ptr();
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));
   const char *directory_assets        = settings->paths.directory_assets;

   if (!menu)
      return NULL;
   if (!(ozone = (ozone_handle_t*)calloc(1, sizeof(ozone_handle_t))))
      goto error;

   *userdata = ozone;

   for (i = 0; i < 15; i++)
      ozone->pure_white[i]                      = 1.00f;

   video_driver_get_size(&width, &height);

   ozone->last_width                            = width;
   ozone->last_height                           = height;
   ozone->last_scale_factor                     = gfx_display_get_dpi_scale(p_disp,
         settings, width, height, false, false);
   ozone->last_thumbnail_scale_factor           = settings->floats.ozone_thumbnail_scale_factor;

   ozone->selection_buf_old.list                = NULL;
   ozone->selection_buf_old.capacity            = 0;
   ozone->selection_buf_old.size                = 0;

   ozone->flags                                |= OZONE_FLAG_DRAW_SIDEBAR;
   ozone->sidebar_offset                        = 0;
   ozone->pending_message                       = NULL;
   ozone->categories_selection_ptr              = 0;
   ozone->pending_message                       = NULL;

   ozone->flags                                |= OZONE_FLAG_FIRST_FRAME;

   ozone->animations.sidebar_text_alpha         = 1.0f;
   ozone->animations.thumbnail_bar_position     = 0.0f;
   ozone->dimensions_sidebar_width              = 0.0f;

   ozone->num_search_terms_old                  = 0;

   ozone->flags2                               &= ~OZONE_FLAG2_CURSOR_WIGGLING;

   if (!(ozone->screensaver = menu_screensaver_init()))
      goto error;

   ozone->savestate_thumbnail_file_path[0]      = '\0';
   ozone->prev_savestate_thumbnail_file_path[0] = '\0';

   ozone->animations.fullscreen_thumbnail_alpha = 0.0f;
   ozone->fullscreen_thumbnail_selection        = 0;
   ozone->fullscreen_thumbnail_label[0]         = '\0';

   ozone->animations.left_thumbnail_alpha       = 1.0f;

   ozone->thumbnails.pending                    = OZONE_PENDING_THUMBNAIL_NONE;
   ozone->thumbnails.stream_delay               = OZONE_THUMBNAIL_STREAM_DELAY;
   gfx_thumbnail_set_stream_delay(ozone->thumbnails.stream_delay);
   gfx_thumbnail_set_fade_duration(-1.0f);
   gfx_thumbnail_set_fade_missing(false);

   ozone_sidebar_update_collapse(ozone, settings->bools.ozone_collapse_sidebar, false);

   ozone->system_tab_end                        = 0;
   ozone->tabs[ozone->system_tab_end]           = OZONE_SYSTEM_TAB_MAIN;
   if (      settings->bools.menu_content_show_settings
         && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_NETPLAY;
#endif

   if (      settings->bools.menu_content_show_add
         && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_ADD;

#if defined(HAVE_DYNAMIC)
   if (settings->uints.menu_content_show_contentless_cores !=
         MENU_CONTENTLESS_CORES_DISPLAY_NONE)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_CONTENTLESS_CORES;
#endif

#if defined(HAVE_LIBRETRODB)
   if (settings->bools.menu_content_show_explore)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_EXPLORE;
#endif

   for (i = 0; i < OZONE_TAB_MAX_LENGTH; i++)
      ozone->tab_selection[i]                   = 0;

   menu_st->flags                 &=  ~MENU_ST_FLAG_PREVENT_POPULATE;

   /* TODO/FIXME - we don't use framebuffer at all
    * for Ozone, we should refactor this dependency
    * away. */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   gfx_display_init_white_texture();

   ozone->horizontal_list.list                  = NULL;
   ozone->horizontal_list.capacity              = 0;
   ozone->horizontal_list.size                  = 0;

   ozone_init_horizontal_list(ozone, settings);

   /* Theme */
   if (settings->bools.menu_use_preferred_system_color_theme)
   {
#ifdef HAVE_LIBNX
      if (R_SUCCEEDED(setsysInitialize()))
      {
         ColorSetId theme;
         setsysGetColorSetId(&theme);
         color_theme = (theme == ColorSetId_Dark) ? 1 : 0;
         ozone_set_color_theme(ozone, color_theme);
         configuration_set_uint(settings,
               settings->uints.menu_ozone_color_theme, color_theme);
         configuration_set_bool(settings,
               settings->bools.menu_preferred_system_color_theme_set, true);
         setsysExit();
      }
      else
#endif
         fallback_color_theme                   = true;
   }
   else
      fallback_color_theme                      = true;

   if (fallback_color_theme)
   {
      color_theme                               = settings->uints.menu_ozone_color_theme;
      ozone_set_color_theme(ozone, color_theme);
   }

   ozone->flags                                &= ~OZONE_FLAG_NEED_COMPUTE;
   ozone->animations.scroll_y                   = 0.0f;
   ozone->animations.scroll_y_sidebar           = 0.0f;

   ozone->first_onscreen_entry                  = 0;
   ozone->last_onscreen_entry                   = 0;
   ozone->first_onscreen_category               = 0;
   ozone->last_onscreen_category                = 0;

   /* Assets path */
   fill_pathname_join_special(
         ozone->assets_path,
         directory_assets,
         "ozone",
         sizeof(ozone->assets_path));

   /* PNG path */
   fill_pathname_join_special(
         ozone->png_path,
         ozone->assets_path,
         "png",
         sizeof(ozone->png_path));

   /* Sidebar path */
   fill_pathname_join_special(
         ozone->tab_path,
         ozone->png_path,
         "sidebar",
         sizeof(ozone->tab_path));

   /* Icons path */
   fill_pathname_application_special(ozone->icons_path,
         sizeof(ozone->icons_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS);
   fill_pathname_join_special(ozone->icons_path_default,
         ozone->icons_path,
         "default",
         sizeof(ozone->icons_path_default));

   if (settings->bools.menu_use_preferred_system_color_theme)
      ozone->flags2 |=  OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME;
   else
      ozone->flags2 &= ~OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME;

   p_anim->updatetime_cb                        = ozone_menu_animation_update_time;

   /* set word_wrap function pointer */
   ozone->word_wrap                             = msg_hash_get_wideglyph_str()
         ? word_wrap_wideglyph
         : word_wrap;

   return menu;

error:
   if (ozone)
   {
      ozone_free_list_nodes(&ozone->horizontal_list, false);
      ozone_free_list_nodes(&ozone->selection_buf_old, false);
      file_list_deinitialize(&ozone->horizontal_list);
      file_list_deinitialize(&ozone->selection_buf_old);
      RHMAP_FREE(ozone->playlist_db_node_map);
   }

   if (menu)
      free(menu);

   return NULL;
}

static void ozone_free(void *data)
{
   ozone_handle_t *ozone   = (ozone_handle_t*) data;

   if (ozone)
   {
      video_coord_array_free(&ozone->fonts.footer.raster_block.carr);
      video_coord_array_free(&ozone->fonts.title.raster_block.carr);
      video_coord_array_free(&ozone->fonts.time.raster_block.carr);
      video_coord_array_free(&ozone->fonts.entries_label.raster_block.carr);
      video_coord_array_free(&ozone->fonts.entries_sublabel.raster_block.carr);
      video_coord_array_free(&ozone->fonts.sidebar.raster_block.carr);

      ozone_free_list_nodes(&ozone->selection_buf_old, false);
      ozone_free_list_nodes(&ozone->horizontal_list, false);
      file_list_deinitialize(&ozone->selection_buf_old);
      file_list_deinitialize(&ozone->horizontal_list);
      RHMAP_FREE(ozone->playlist_db_node_map);

      if (!string_is_empty(ozone->pending_message))
         free(ozone->pending_message);

      menu_screensaver_free(ozone->screensaver);
   }

   gfx_display_deinit_white_texture();

   font_driver_bind_block(NULL, NULL);
}

static void ozone_update_thumbnail_image(void *data)
{
   bool show_thumbnail_bar, want_thumbnail_bar;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();

   if (!ozone)
      return;

   /* Cache previous status to remove footer metadata indicator blinking */
   ozone->thumbnails_left_status_prev  = ozone->thumbnails.left.status;
   ozone->thumbnails_right_status_prev = ozone->thumbnails.right.status;
   ozone->thumbnails.pending           = OZONE_PENDING_THUMBNAIL_NONE;
   gfx_thumbnail_cancel_pending_requests();

   if (!(ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET))
   {
      gfx_thumbnail_reset(&ozone->thumbnails.right);
      gfx_thumbnail_reset(&ozone->thumbnails.left);
   }

   /* Right thumbnail */
   if (gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
      ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_RIGHT;

   /* Left thumbnail
    * > Disabled for image (and video/music) content */
   if (     (!(ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER))
         && (gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT)))
      ozone->thumbnails.pending = (ozone->thumbnails.pending == OZONE_PENDING_THUMBNAIL_RIGHT)
            ? OZONE_PENDING_THUMBNAIL_BOTH
            : OZONE_PENDING_THUMBNAIL_LEFT;

   /* Use left thumbnail as imageviewer failsafe if right is not enabled */
   if (     (ozone->flags2 & OZONE_FLAG2_SELECTION_CORE_IS_VIEWER)
         && ( gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
         && (!gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)))
      ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_LEFT;

   show_thumbnail_bar = ozone->show_thumbnail_bar;
   want_thumbnail_bar = (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR) ? true : false;
   if (show_thumbnail_bar != want_thumbnail_bar)
      ozone->flags |= OZONE_FLAG_NEED_COMPUTE;
}

static void ozone_refresh_thumbnail_image(void *data, unsigned i)
{
   ozone_handle_t *ozone        = (ozone_handle_t*)data;
   struct menu_state   *menu_st = menu_state_get_ptr();

   if (!ozone)
      return;

   ozone->flags &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;
   ozone_unload_thumbnail_textures(ozone);

   /* Refresh metadata */
   if (!i)
      ozone_update_content_metadata(ozone);

   /* Only refresh thumbnails if thumbnails are enabled */
   if (     (  gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
            || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
         && (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR)
         && (  (ozone->is_quick_menu)
            || (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
            || (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)))
      ozone_update_thumbnail_image(ozone);
}

static bool ozone_init_font(
      font_data_impl_t *font_data,
      bool is_threaded,
      char *font_path,
      float font_size)
{
   int glyph_width               = 0;
   gfx_display_t *p_disp         = disp_get_ptr();
   const char *wideglyph_str     = msg_hash_get_wideglyph_str();

   /* Free existing */
   if (font_data->font)
   {
      font_driver_free(font_data->font);
      font_data->font            = NULL;
   }

   /* Cache approximate dimensions */
   font_data->line_height        = (int)(font_size + 0.5f);
   font_data->glyph_width        = (int)((font_size * (3.0f / 4.0f)) + 0.5f);

   /* Create font */
   if (!(font_data->font = gfx_display_font_file(p_disp, font_path, font_size, is_threaded)))
      return false;

   /* Get font metadata */
   if ((glyph_width = font_driver_get_message_width(font_data->font, "a", 1, 1.0f)) > 0)
      font_data->glyph_width     = glyph_width;

   font_data->wideglyph_width    = 100;

   if (wideglyph_str)
   {
      int wideglyph_width        =
            font_driver_get_message_width(font_data->font, wideglyph_str, strlen(wideglyph_str), 1.0f);

      if (wideglyph_width > 0 && glyph_width > 0)
         font_data->wideglyph_width = wideglyph_width * 100 / glyph_width;
   }

   font_data->line_height        = font_driver_get_line_height(font_data->font, 1.0f);
   font_data->line_ascender      = font_driver_get_line_ascender(font_data->font, 1.0f);
   font_data->line_centre_offset = font_driver_get_line_centre_offset(font_data->font, 1.0f);

   return true;
}

static void ozone_cache_footer_label(
      ozone_handle_t *ozone,
      ozone_footer_label_t *label,
      enum msg_hash_enums enum_idx)
{
   const char *str = msg_hash_to_str(enum_idx);
   /* Determine pixel width */
   size_t length   = strlen(str);

   /* Assign string */
   label->str      = str;
   label->width    = font_driver_get_message_width(ozone->fonts.footer.font, label->str, length, 1.0f);
   /* If font_driver_get_message_width() fails,
    * use predetermined glyph_width as a fallback */
   if (label->width < 0)
      label->width = length * ozone->fonts.footer.glyph_width;
}

/* Assigns footer label strings (based on current
 * menu language) and calculates pixel widths */
static void ozone_cache_footer_labels(ozone_handle_t *ozone)
{
   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.ok,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.back,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.search,
         MENU_ENUM_LABEL_VALUE_SEARCH);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.reset_to_default,
         MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.cycle,
         MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.fullscreen_thumbs,
         MSG_TOGGLE_FULLSCREEN_THUMBNAILS);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.metadata_toggle,
         MSG_TOGGLE_CONTENT_METADATA);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.help,
         MENU_ENUM_LABEL_VALUE_HELP);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.clear,
         MENU_ENUM_LABEL_VALUE_CLEAR_SETTING);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.scan,
         MENU_ENUM_LABEL_VALUE_SCAN_ENTRY);

   ozone_cache_footer_label(ozone,
         &ozone->footer_labels.manage,
         MENU_ENUM_LABEL_VALUE_MANAGE);

   /* Record current language setting */
   ozone->footer_labels_language = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
}

/* Determines the size of all menu elements */
static void ozone_set_layout(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      bool is_threaded)
{
   char s1[PATH_MAX_LENGTH];
   char font_path[PATH_MAX_LENGTH];
   settings_t *settings                             = config_get_ptr();
   bool font_inited                                 = false;
   float scale_factor                               = ozone->last_scale_factor;

   /* Calculate dimensions */
   ozone->dimensions.header_height                  = HEADER_HEIGHT * scale_factor;
   ozone->dimensions.footer_height                  = FOOTER_HEIGHT * scale_factor;

   ozone->dimensions.entry_padding_horizontal_half  = ENTRY_PADDING_HORIZONTAL_HALF * scale_factor;
   ozone->dimensions.entry_padding_horizontal_full  = ENTRY_PADDING_HORIZONTAL_FULL * scale_factor;
   ozone->dimensions.entry_padding_vertical         = ENTRY_PADDING_VERTICAL * scale_factor;
   ozone->dimensions.entry_height                   = ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.entry_spacing                  = ENTRY_SPACING * scale_factor;
   ozone->dimensions.entry_icon_size                = ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.entry_icon_padding             = ENTRY_ICON_PADDING * scale_factor;

   ozone->dimensions.sidebar_entry_height           = SIDEBAR_ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.sidebar_padding_horizontal     = SIDEBAR_X_PADDING * scale_factor;
   ozone->dimensions.sidebar_padding_vertical       = SIDEBAR_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_padding_vertical = SIDEBAR_ENTRY_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_icon_size        = SIDEBAR_ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.sidebar_entry_icon_padding     = SIDEBAR_ENTRY_ICON_PADDING * scale_factor;
   ozone->dimensions.sidebar_gradient_height        = SIDEBAR_GRADIENT_HEIGHT * scale_factor;

   ozone->dimensions.sidebar_width_normal           = SIDEBAR_WIDTH * scale_factor;
   ozone->dimensions.sidebar_width_collapsed        = ozone->dimensions.sidebar_entry_icon_size
         + ozone->dimensions.sidebar_entry_icon_padding * 2
         + ozone->dimensions.sidebar_padding_horizontal * 2;

   if (ozone->dimensions_sidebar_width == 0)
      ozone->dimensions_sidebar_width               = (float)ozone->dimensions.sidebar_width_normal;

   ozone->dimensions.thumbnail_bar_width            = ozone->last_thumbnail_scale_factor *
         (ozone->dimensions.sidebar_width_normal -
          ozone->dimensions.sidebar_entry_icon_size +
          ozone->dimensions.sidebar_entry_icon_padding);

   /* Prevent thumbnail sidebar from growing too much and making the UI unusable. */
   if (ozone->dimensions.thumbnail_bar_width > ozone->last_width / 3.0f)
      ozone->dimensions.thumbnail_bar_width         = ozone->last_width / 3.0f;

   ozone->dimensions.cursor_size                    = CURSOR_SIZE * scale_factor;

   ozone->dimensions.fullscreen_thumbnail_padding   = FULLSCREEN_THUMBNAIL_PADDING * scale_factor;

   /* Common spacers */
   ozone->dimensions.spacer_1px = (scale_factor > 1.0f) ? (unsigned)(scale_factor + 0.5f) : 1;
   ozone->dimensions.spacer_2px = ozone->dimensions.spacer_1px * 2;
   ozone->dimensions.spacer_3px = (unsigned)((scale_factor * 3.0f) + 0.5f);
   ozone->dimensions.spacer_5px = (unsigned)((scale_factor * 5.0f) + 0.5f);

   /* Determine movement delta size for activating
    * pointer input (note: not a dimension as such,
    * so not included in the 'dimensions' struct) */
   ozone->pointer_active_delta = CURSOR_ACTIVE_DELTA * scale_factor;

   /* Initialise fonts */
   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join_special(font_path, ozone->assets_path, "bold.ttf", sizeof(font_path));
         break;
   }

   font_inited = ozone_init_font(&ozone->fonts.title,
         is_threaded, font_path, FONT_SIZE_TITLE * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_join_special(s1, settings->paths.directory_assets, "pkg", sizeof(s1));
         fill_pathname_join_special(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join_special(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
         break;
   }

   /* Sidebar */
   font_inited = ozone_init_font(&ozone->fonts.sidebar,
         is_threaded, font_path, FONT_SIZE_SIDEBAR * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   /* Entries */
   font_inited = ozone_init_font(&ozone->fonts.entries_label,
         is_threaded, font_path, FONT_SIZE_ENTRIES_LABEL * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   /* Sublabels */
   font_inited = ozone_init_font(&ozone->fonts.entries_sublabel,
         is_threaded, font_path, FONT_SIZE_ENTRIES_SUBLABEL * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   /* Time */
   font_inited = ozone_init_font(&ozone->fonts.time,
         is_threaded, font_path, FONT_SIZE_TIME * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   /* Footer */
   font_inited = ozone_init_font(&ozone->fonts.footer,
         is_threaded, font_path, FONT_SIZE_FOOTER * scale_factor);
   if (!(((ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS) > 0) && font_inited))
      ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

   /* Cache footer text labels
    * > Fonts have been (re)initialised, so need
    *   to recalculate label widths */
   ozone_cache_footer_labels(ozone);

   /* Multiple sidebar parameters are set via animations
    * > ozone_refresh_sidebars() cancels any existing
    *   animations and 'force updates' the affected
    *   variables with newly scaled values */
   ozone_refresh_sidebars(ozone, ozone_collapse_sidebar, ozone->last_height);

   /* Entry dimensions must be recalculated after
    * updating menu layout */
   ozone->flags |= OZONE_FLAG_NEED_COMPUTE;
}

static void ozone_context_reset(void *data, bool is_threaded)
{
   static const char *OZONE_TAB_TEXTURES_FILES[OZONE_TAB_TEXTURE_LAST] = {
      "retroarch.png", /* MAIN_MENU */
      "settings.png",  /* SETTINGS_TAB */
      "history.png",   /* HISTORY_TAB */
      "favorites.png", /* FAVORITES_TAB */
      "music.png",     /* MUSIC_TAB */
      "video.png",     /* VIDEO_TAB */
      "image.png",     /* IMAGES_TAB */
      "netplay.png",   /* NETPLAY_TAB */
      "add.png",       /* ADD_TAB */
      "core.png",      /* CONTENTLESS_CORES_TAB */
      "database.png"   /* EXPLORE_TAB */
   };
   static const char *OZONE_TEXTURES_FILES[OZONE_TEXTURE_LAST]         = {
      "retroarch.png",
      "cursor_border.png"
   };
   unsigned i;
   ozone_handle_t *ozone      = (ozone_handle_t*) data;

   if (ozone)
   {
      ozone->flags |= OZONE_FLAG_HAS_ALL_ASSETS;

      ozone_set_layout(ozone, config_get_ptr()->bools.ozone_collapse_sidebar, is_threaded);

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[64];
#ifdef HAVE_DISCORD_OWN_AVATAR
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
         {
            size_t _len = strlcpy(filename, discord_get_own_avatar(), sizeof(filename));
            strlcpy(filename + _len, FILE_PATH_PNG_EXTENSION, sizeof(filename) - _len);
         }
         else
#endif
         {
            strlcpy(filename, OZONE_TEXTURES_FILES[i], sizeof(filename));
         }

#ifdef HAVE_DISCORD_OWN_AVATAR
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
         {
            char buf[PATH_MAX_LENGTH];
            fill_pathname_application_special(buf,
               sizeof(buf),
               APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
            gfx_display_reset_textures_list(filename,
                  buf, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
         }
         else
#endif
         {
            if (!gfx_display_reset_textures_list(filename,
                  ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;
         }
      }

      /* Sidebar textures */
      for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      {
         switch (i)
         {
            /* Exceptions for icons that don't exist in 'png/sidebar/' */
            case OZONE_TAB_TEXTURE_CONTENTLESS_CORES:
            case OZONE_TAB_TEXTURE_EXPLORE:
               if (!gfx_display_reset_textures_list(OZONE_TAB_TEXTURES_FILES[i],
                     ozone->icons_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
                  ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;
               break;
            default:
               if (!gfx_display_reset_textures_list(OZONE_TAB_TEXTURES_FILES[i],
                     ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
                  ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;
               break;
         }
      }

      /* Theme textures */
      if (!ozone_reset_theme_textures(ozone))
         ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;

      /* Icons textures init */
      for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      {
         if (!gfx_display_reset_textures_list(ozone_entries_icon_texture_path(i),
               ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
            ozone->flags &= ~OZONE_FLAG_HAS_ALL_ASSETS;
      }

      gfx_display_deinit_white_texture();
      gfx_display_init_white_texture();

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
      ozone->flags                       &= ~(OZONE_FLAG_DRAW_OLD_LIST
                                            | OZONE_FLAG_FADE_DIRECTION
                                            | OZONE_FLAG_CURSOR_IN_SIDEBAR
                                            | OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD
                                            | OZONE_FLAG_MSGBOX_STATE
                                            | OZONE_FLAG_MSGBOX_STATE_OLD
                                             );

      ozone->flags2                      &= ~OZONE_FLAG2_CURSOR_WIGGLING;

      /* Animations */
      ozone->animations.cursor_alpha      = 1.0f;
      ozone->animations.scroll_y          = 0.0f;
      ozone->animations.list_alpha        = 1.0f;

      /* Thumbnails */
      ozone_update_thumbnail_image(ozone);
      ozone_update_savestate_thumbnail_image(ozone);

      if (ozone->flags & OZONE_FLAG_HAS_ALL_ASSETS)
         ozone_restart_cursor_animation(ozone);

      /* Screensaver */
      menu_screensaver_context_destroy(ozone->screensaver);
   }

   video_driver_monitor_reset();
}

static void ozone_collapse_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->flags         &= ~OZONE_FLAG_DRAW_SIDEBAR;
}

static void ozone_unload_thumbnail_textures(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_NONE;
   gfx_thumbnail_cancel_pending_requests();
   gfx_thumbnail_reset(&ozone->thumbnails.right);
   gfx_thumbnail_reset(&ozone->thumbnails.left);
   gfx_thumbnail_reset(&ozone->thumbnails.savestate);
}

static void INLINE ozone_font_free(font_data_impl_t *font_data)
{
   if (font_data->font)
      font_driver_free(font_data->font);

   font_data->font = NULL;
}

static void ozone_context_destroy(void *data)
{
   unsigned i;
   uintptr_t tag;
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return;

   /* Theme */
   ozone_unload_theme_textures(ozone);

   /* Icons */
   for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->icons_textures[i]);

   /* Textures */
   for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->textures[i]);

   /* Icons */
   for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->tab_textures[i]);

   /* Thumbnails */
   ozone_unload_thumbnail_textures(ozone);

   gfx_display_deinit_white_texture();

   /* Fonts */
   ozone_font_free(&ozone->fonts.footer);
   ozone_font_free(&ozone->fonts.title);
   ozone_font_free(&ozone->fonts.time);
   ozone_font_free(&ozone->fonts.entries_label);
   ozone_font_free(&ozone->fonts.entries_sublabel);
   ozone_font_free(&ozone->fonts.sidebar);

   tag = (uintptr_t) &ozone_default_theme;
   gfx_animation_kill_by_tag(&tag);

   /* Horizontal list */
   ozone_context_destroy_horizontal_list(ozone);

   /* Screensaver */
   menu_screensaver_context_destroy(ozone->screensaver);
}

static void *ozone_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   ozone_handle_t* ozone   = (ozone_handle_t*) data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            struct menu_state   *menu_st = menu_state_get_ptr();
            menu_list_t *menu_list       = menu_st->entries.list;
            file_list_t *menu_stack      = MENU_LIST_GET(menu_list, 0);
            size_t list_size             = MENU_LIST_GET_STACK_SIZE(menu_list, 0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         {
            size_t list_size = ozone->horizontal_list.size;
            if (i < list_size)
               return (void*)&ozone->horizontal_list.list[i];
         }
         break;
      default:
         break;
   }

   return NULL;
}

static int ozone_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   int ret                = -1;
   core_info_list_t *list = NULL;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t             *settings = config_get_ptr();
            bool menu_content_show_playlists = settings->bools.menu_content_show_playlists;
            bool kiosk_mode_enable           = settings->bools.kiosk_mode_enable;

            menu_entries_clear(info->list);
            menu_entries_append(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0, NULL);

            core_info_get_list(&list);
            if (list && list->info_count > 0)
            {
               menu_entries_append(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0, NULL);
            }

            if (menu_content_show_playlists)
               menu_entries_append(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0, NULL);

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0, NULL);

            if (!kiosk_mode_enable)
            {
               menu_entries_append(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0, NULL);
            }

            info->flags |= MD_FLAG_NEED_PUSH | MD_FLAG_NEED_REFRESH;
            ret          = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            uint32_t flags              = runloop_get_flags();

            menu_entries_clear(info->list);

            if (flags & RUNLOOP_FLAG_CORE_RUNNING)
            {
               if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_CONTENT_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }
            else
            {
               rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
               if (sys_info && sys_info->load_no_content)
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_START_CORE,
                        PARSE_ACTION,
                        false);
               }

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                           info->list,
                           MENU_ENUM_LABEL_CORE_LIST,
                           PARSE_ACTION,
                           false);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION,
                     false);

               if (menu_displaylist_has_subsystems())
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }

            if (settings->bools.menu_show_load_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_DISC,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_DUMP_DISC,
                     PARSE_ACTION,
                     false);
            }

#ifdef HAVE_LAKKA
            if (settings->bools.menu_show_eject_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_EJECT_DISC,
                     PARSE_ACTION,
                     false);
            }
#endif

            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                  PARSE_ACTION,
                  false);
#ifdef HAVE_QT
            if (settings->bools.desktop_menu_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_SHOW_WIMP,
                     PARSE_ACTION,
                     false);
            }
#endif
#if defined(HAVE_NETWORKING)
#if defined(HAVE_ONLINE_UPDATER)
            if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION,
                     false);
            }
#endif
#endif
#ifdef HAVE_MIST
            if (settings->bools.menu_show_core_manager_steam && !settings->bools.kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,
                  PARSE_ACTION,
                  false);
            }
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_information)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION,
                     false);
            }

#if defined(HAVE_LIBNX)
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
                  PARSE_ACTION,
                  false);
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION,
                     false);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_restart_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_quit_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION,
                     false);
            }
#endif

            if (settings->bools.menu_show_reboot)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_shutdown)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_SHUTDOWN,
                     PARSE_ACTION,
                     false);
            }

            info->flags |= MD_FLAG_NEED_PUSH;
            ret          = 0;
         }
         break;
   }
   return ret;
}

static size_t ozone_list_get_selection(void *data)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;
   if (ozone)
      return ozone->categories_selection_ptr;
   return 0;
}

static void ozone_list_clear(file_list_t *list)
{
   uintptr_t tag = (uintptr_t)list;
   gfx_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(list, false);
}

static void ozone_list_free(file_list_t *list, size_t a, size_t b)
{
   ozone_list_clear(list);
}

static void ozone_render(void *data,
      unsigned width,
      unsigned height,
      bool is_idle)
{
   size_t i;
   /* c.f. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    * On some platforms (e.g. 32-bit x86 without SSE),
    * gcc can produce inconsistent floating point results
    * depending upon optimisation level. This can break
    * floating point variable comparisons. A workaround is
    * to declare the affected variable as 'volatile', which
    * disables optimisations and removes excess precision
    * (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323#c87) */
   volatile float scale_factor;
   volatile float thumbnail_scale_factor;
   struct menu_state *menu_st         = menu_state_get_ptr();
   menu_input_t *menu_input           = &menu_st->input_state;
   menu_list_t *menu_list             = menu_st->entries.list;
   unsigned entries_end               = (unsigned)MENU_LIST_GET_SELECTION(menu_list, 0)->size;
   bool pointer_enabled               = false;
   unsigned language                  = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
   ozone_handle_t *ozone              = (ozone_handle_t*)data;
   gfx_display_t *p_disp              = disp_get_ptr();
   gfx_animation_t          *p_anim   = anim_get_ptr();
   settings_t             *settings   = config_get_ptr();
   bool ozone_collapse_sidebar        = settings->bools.ozone_collapse_sidebar;
   if (!ozone)
      return;

   /* Check whether screen dimensions or menu scale
    * factor have changed */
   scale_factor           = gfx_display_get_dpi_scale(p_disp, settings,
         width, height, false, false);
   thumbnail_scale_factor = settings->floats.ozone_thumbnail_scale_factor;

   if (     (scale_factor != ozone->last_scale_factor)
         || (thumbnail_scale_factor != ozone->last_thumbnail_scale_factor)
         || (width != ozone->last_width)
         || (height != ozone->last_height))
   {
      ozone->last_scale_factor           = scale_factor;
      ozone->last_thumbnail_scale_factor = thumbnail_scale_factor;
      ozone->last_width                  = width;
      ozone->last_height                 = height;

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      ozone_set_layout(ozone, ozone_collapse_sidebar, video_driver_is_threaded());
      video_driver_monitor_reset();
   }

   if (ozone->flags & OZONE_FLAG_NEED_COMPUTE)
   {
      ozone_compute_entries_position(ozone, settings->bools.menu_show_sublabels, entries_end);
      ozone->flags &= ~OZONE_FLAG_NEED_COMPUTE;
   }

   /* Check whether menu language has changed
    * > If so, need to re-cache footer text labels */
   if (ozone->footer_labels_language != language)
   {
      ozone->footer_labels_language = language;
      ozone_cache_footer_labels(ozone);
   }

   ozone->selection        = menu_st->selection_ptr;

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   /* Read pointer state */
   menu_input_get_pointer_state(&ozone->pointer);

   /* If menu screensaver is active, update
    * screensaver and return */
   if (ozone->flags & OZONE_FLAG_SHOW_SCREENSAVER)
   {
      menu_screensaver_iterate(
            ozone->screensaver,
            p_disp, p_anim,
            (enum menu_screensaver_effect)settings->uints.menu_screensaver_animation,
            settings->floats.menu_screensaver_animation_speed,
            ozone->theme->screensaver_tint,
            width, height,
            settings->paths.directory_assets);
      GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
      return;
   }

   /* Check whether pointer is enabled */
   if (ozone->pointer.type != MENU_POINTER_DISABLED)
   {
      /* When using a mouse, entry under pointer is
       * automatically selected
       * > Must therefore filter out small movements,
       *   otherwise scrolling with the mouse wheel
       *   becomes impossible... */
      if (ozone->pointer.type == MENU_POINTER_MOUSE)
      {
         int16_t cursor_x_delta = ozone->pointer.x - ozone->cursor_x_old;
         int16_t cursor_y_delta = ozone->pointer.y - ozone->cursor_y_old;

         if (   (cursor_x_delta >  ozone->pointer_active_delta)
             || (cursor_x_delta < -ozone->pointer_active_delta)
             || (cursor_y_delta >  ozone->pointer_active_delta)
             || (cursor_y_delta < -ozone->pointer_active_delta))
            ozone->flags |=  OZONE_FLAG_CURSOR_MODE;
      }
      /* On touchscreens, just check for any movement */
      else
      {
         if (   (ozone->pointer.x != ozone->cursor_x_old)
             || (ozone->pointer.y != ozone->cursor_y_old))
            ozone->flags |=  OZONE_FLAG_CURSOR_MODE;
      }
   }

   ozone->cursor_x_old = ozone->pointer.x;
   ozone->cursor_y_old = ozone->pointer.y;

   /* Pointer is disabled when:
    * - Showing fullscreen thumbnails
    * - On-screen keyboard is active
    * - A message box is being displayed */
   pointer_enabled =
              (ozone->flags  & OZONE_FLAG_CURSOR_MODE)
         && (!(ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS))
         && !menu_input_dialog_get_display_kb()
         && (!(ozone->flags & OZONE_FLAG_SHOULD_DRAW_MSGBOX));

   /* Process pointer input, if required */
   if (pointer_enabled)
   {
      bool pointer_in_sidebar, last_pointer_in_sidebar;
      file_list_t *selection_buf    = MENU_LIST_GET_SELECTION(menu_list, 0);
      uintptr_t     animation_tag   = (uintptr_t)selection_buf;

      int entry_padding             = (ozone->depth == 1)
            ? ozone->dimensions.entry_padding_horizontal_half
            : ozone->dimensions.entry_padding_horizontal_full;
      float entry_x                 = ozone->dimensions_sidebar_width
            + ozone->sidebar_offset
            + entry_padding;
      float entry_width             = width
            - ozone->dimensions_sidebar_width
            - ozone->sidebar_offset
            - entry_padding * 2
            - ozone->animations.thumbnail_bar_position;
      bool first_entry_found        = false;
      bool last_entry_found         = false;

      unsigned horizontal_list_size = (unsigned)ozone->horizontal_list.size;
      float category_height         = ozone->dimensions.sidebar_entry_height
            + ozone->dimensions.sidebar_entry_padding_vertical;
      bool first_category_found     = false;
      bool last_category_found      = false;

      /* Check whether pointer is operating on entries
       * or sidebar
       * > Note 1: Since touchscreens effectively 'lose their
       *   place' when a touch is released, we can only perform
       *   this this check if the pointer is currently
       *   pressed - i.e. we must preserve the values set the
       *   last time the screen was touched.
       *   With mouse input we have a permanent cursor, so this
       *   is not an issue
       * > Note 2: Windows seems to report negative pointer
       *   coordinates when the cursor goes off the left hand
       *   side of the screen/window, so checking whether
       *   pointer.x is less than the effective sidebar width
       *   generates a false positive when ozone->depth > 1.
       *   We therefore must also check whether the sidebar
       *   is currently being drawn */
      if (ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR)
         ozone->flags2 |=  OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR;
      else
         ozone->flags2 &= ~OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR;
      if ((ozone->pointer.type == MENU_POINTER_MOUSE) ||
           ozone->pointer.pressed)
      {
         if ((ozone->flags & OZONE_FLAG_DRAW_SIDEBAR)
               && (ozone->pointer.x < ozone->dimensions_sidebar_width + ozone->sidebar_offset))
            ozone->flags2 |=  OZONE_FLAG2_POINTER_IN_SIDEBAR;
         else
            ozone->flags2 &= ~OZONE_FLAG2_POINTER_IN_SIDEBAR;
      }

      pointer_in_sidebar      = ozone->flags2 &
         OZONE_FLAG2_POINTER_IN_SIDEBAR;
      last_pointer_in_sidebar = ozone->flags2 &
         OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR;
      /* If pointer has switched from entries to sidebar
       * or vice versa, must reset pointer acceleration */
      if (     (     pointer_in_sidebar)
            != (last_pointer_in_sidebar))
      {
         menu_input->pointer.y_accel = 0.0f;
         ozone->pointer.y_accel      = 0.0f;
      }

      /* If pointer is a mouse, then automatically follow
       * mouse focus from entries to sidebar (and vice versa) */
      if (ozone->pointer.type == MENU_POINTER_MOUSE)
      {
         pointer_in_sidebar      = ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR;
         last_pointer_in_sidebar = ozone->flags2 & OZONE_FLAG2_LAST_POINTER_IN_SIDEBAR;

         if (       (pointer_in_sidebar)
               && (!(last_pointer_in_sidebar))
               && (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)))
            ozone_go_to_sidebar(ozone, ozone_collapse_sidebar, animation_tag);
         else if (   (!pointer_in_sidebar)
                  && (last_pointer_in_sidebar)
                  && (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
            if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
               ozone_leave_sidebar(ozone, ozone_collapse_sidebar, animation_tag);
      }

      /* Update scrolling - must be done first, otherwise
       * cannot determine entry/category positions
       * > Entries */
      if (!(ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR))
      {
         float entry_bottom_boundary = height
               - ozone->dimensions.header_height
               - ozone->dimensions.spacer_1px
               - ozone->dimensions.footer_height
               - ozone->dimensions.entry_padding_vertical * 2;

         ozone->animations.scroll_y += ozone->pointer.y_accel;

         if (ozone->animations.scroll_y + ozone->entries_height < entry_bottom_boundary)
            ozone->animations.scroll_y = entry_bottom_boundary - ozone->entries_height;

         if (ozone->animations.scroll_y > 0.0f)
            ozone->animations.scroll_y = 0.0f;
      }
      /* > Sidebar
       * Only process sidebar input here if the
       * cursor is currently *in* the sidebar */
      else if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
      {
         float sidebar_bottom_boundary = height -
               (ozone->dimensions.header_height + ozone->dimensions.spacer_1px) -
               ozone->dimensions.footer_height -
               ozone->dimensions.sidebar_padding_vertical;
         float sidebar_height          = ozone_get_sidebar_height(ozone);

         ozone->animations.scroll_y_sidebar += ozone->pointer.y_accel;

         if (ozone->animations.scroll_y_sidebar + sidebar_height < sidebar_bottom_boundary)
            ozone->animations.scroll_y_sidebar = sidebar_bottom_boundary - sidebar_height;

         if (ozone->animations.scroll_y_sidebar > 0.0f)
            ozone->animations.scroll_y_sidebar = 0.0f;
      }

      /* Regardless of pointer location, have to process
       * all entries/categories in order to determine
       * the indices of the first and last entries/categories
       * displayed on screen
       * > Needed so we can determine proper cursor positions
       *   when mixing pointer + gamepad/keyboard input */

      /* >> Loop over all entries */
      ozone->first_onscreen_entry   = 0;
      ozone->last_onscreen_entry    = 0;
      if (entries_end > 0)
         ozone->last_onscreen_entry = entries_end - 1;

      for (i = 0; i < entries_end; i++)
      {
         float entry_y;
         ozone_node_t *node = (ozone_node_t*)selection_buf->list[i].userdata;

         /* Sanity check */
         if (!node)
            break;

         /* Get current entry y position */
         entry_y = ozone->dimensions.header_height
               + ozone->dimensions.spacer_1px
               + ozone->dimensions.entry_padding_vertical
               + ozone->animations.scroll_y
               + node->position_y;

         /* Check whether this is the first on screen entry */
         if (!first_entry_found)
         {
            if ((entry_y + node->height) > ozone->dimensions.header_height)
            {
               ozone->first_onscreen_entry = i;
               first_entry_found = true;
            }
         }
         /* Check whether this is the last on screen entry */
         else if (!last_entry_found)
         {
            if (entry_y > (height - ozone->dimensions.footer_height))
            {
               /* Current entry is off screen - get index
                * of previous entry */
               if (i > 0)
               {
                  ozone->last_onscreen_entry = i - 1;
                  last_entry_found = true;
               }
            }
         }

         /* Track pointer input, if required */
         if (     (!(ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR))
               && first_entry_found
               && !last_entry_found)
         {
            /* Check whether pointer is within the bounds
             * of the current entry */
            if (     (ozone->pointer.x > entry_x)
                  && (ozone->pointer.x < entry_x + entry_width)
                  && (ozone->pointer.y > entry_y)
                  && (ozone->pointer.y < entry_y + node->height))
            {
               /* Pointer selection is always updated */
               menu_input->ptr = (unsigned)i;

               /* If pointer is a mouse, then automatically
                * select entry under cursor */
               if (ozone->pointer.type == MENU_POINTER_MOUSE)
               {
                  /* Note the fudge factor - cannot auto select
                   * items while drag-scrolling the entry list,
                   * so have to wait until pointer acceleration
                   * drops below a 'sensible' level... */
                  if (     (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
                        && (i != ozone->selection)
                        && (ozone->pointer.y_accel < ozone->last_scale_factor)
                        && (ozone->pointer.y_accel > -ozone->last_scale_factor))
                  {
                     menu_st->selection_ptr = i;

                     /* If this is a playlist, must update thumbnails */
                     if (    ((ozone->flags & OZONE_FLAG_IS_PLAYLIST) && (ozone->depth == 1 || ozone->depth == 4))
                           || (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST))
                     {
                        ozone->flags &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;
                        ozone_set_thumbnail_content(ozone, "");
                        ozone_update_thumbnail_image(ozone);
                     }
                     /* Also savestate thumbnails need updating */
                     else if ((ozone->is_quick_menu && ozone->depth >= 2)
                           || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
                     {
                        ozone_update_savestate_thumbnail_path(ozone, (unsigned)i);
                        ozone_update_savestate_thumbnail_image(ozone);
                     }
                  }
               }

               /* If pointer is pressed and stationary, and
                * if pointer has been held for at least
                * MENU_INPUT_PRESS_TIME_SHORT ms, automatically
                * select current entry */
               if (      ozone->pointer.pressed
                     && !ozone->pointer.dragged
                     && (ozone->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT)
                     && (i != ozone->selection))
               {
                  menu_st->selection_ptr = i;

                  /* If we are currently in the sidebar, leave it */
                  if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
                  {
                     if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
                        ozone_leave_sidebar(ozone, ozone_collapse_sidebar, animation_tag);
                  }
                  /* If this is a playlist, must update thumbnails */
                  else if ((ozone->flags & OZONE_FLAG_IS_PLAYLIST)
                        && (ozone->depth == 1 || ozone->depth == 4))
                  {
                     ozone_set_thumbnail_content(ozone, "");
                     ozone_update_thumbnail_image(ozone);
                  }
               }
            }
         }

         if (last_entry_found)
            break;
      }

      /* >> Loop over all categories */
      ozone->first_onscreen_category = 0;
      ozone->last_onscreen_category  = ozone->system_tab_end + horizontal_list_size;

      for (i = 0; i < ozone->system_tab_end + horizontal_list_size + 1; i++)
      {
         /* Get current category y position */
         float category_y = ozone->dimensions.header_height
               + ozone->dimensions.spacer_1px
               + ozone->dimensions.sidebar_padding_vertical
               + (category_height * i)
               + ((i > ozone->system_tab_end)
                     ? (ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px)
                     : 0)
               + ozone->animations.scroll_y_sidebar;

         /* Check whether this is the first on screen category */
         if (!first_category_found)
         {
            if ((category_y + category_height) > ozone->dimensions.header_height)
            {
               ozone->first_onscreen_category = i;
               first_category_found = true;
            }
         }
         /* Check whether this is the last on screen category */
         else if (!last_category_found)
         {
            if (category_y > (height - ozone->dimensions.footer_height))
            {
               /* Current category is off screen - get index
                * of previous category */
               if (i > 0)
               {
                  ozone->last_onscreen_category = i - 1;
                  last_category_found = true;
               }
            }
         }

         /* Track pointer input, if required */
         if (     (ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR)
               && (ozone->flags  & OZONE_FLAG_CURSOR_IN_SIDEBAR)
               && first_category_found
               && !last_category_found)
         {
            /* If pointer is within the bounds of the
             * current category, cache category index
             * (for use in next 'pointer up' event) */
            if (     (ozone->pointer.y > category_y)
                  && (ozone->pointer.y < category_y + category_height))
               ozone->pointer_categories_selection = i;
         }

         if (last_category_found)
            break;
      }
   }

   /* Handle any pending thumbnail load requests */
   if (ozone->show_thumbnail_bar && (ozone->thumbnails.pending != OZONE_PENDING_THUMBNAIL_NONE))
   {
      size_t selection                         = menu_st->selection_ptr;
      playlist_t *playlist                     = playlist_get_cached();
      unsigned gfx_thumbnail_upscale_threshold = settings->uints.gfx_thumbnail_upscale_threshold;
      bool network_on_demand_thumbnails        = settings->bools.network_on_demand_thumbnails;

      /* Explore list needs cached selection index */
      if (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
         selection = menu_st->thumbnail_path_data
            ? menu_st->thumbnail_path_data->playlist_index
            : 0;

      switch (ozone->thumbnails.pending)
      {
         case OZONE_PENDING_THUMBNAIL_BOTH:
            gfx_thumbnail_request_streams(
                  menu_st->thumbnail_path_data,
                  p_anim,
                  playlist,
                  selection,
                  &ozone->thumbnails.right,
                  &ozone->thumbnails.left,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
            if (     (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_UNKNOWN)
                  && (ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_UNKNOWN))
               ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_NONE;
            break;
         case OZONE_PENDING_THUMBNAIL_RIGHT:
            gfx_thumbnail_request_stream(
                  menu_st->thumbnail_path_data,
                  p_anim,
                  GFX_THUMBNAIL_RIGHT,
                  playlist,
                  selection,
                  &ozone->thumbnails.right,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
            if (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_UNKNOWN)
               ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_NONE;
            break;
         case OZONE_PENDING_THUMBNAIL_LEFT:
            gfx_thumbnail_request_stream(
                  menu_st->thumbnail_path_data,
                  p_anim,
                  GFX_THUMBNAIL_LEFT,
                  playlist,
                  selection,
                  &ozone->thumbnails.left,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
            if (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_UNKNOWN)
               ozone->thumbnails.pending = OZONE_PENDING_THUMBNAIL_NONE;
            break;
         default:
            break;
      }

      if (ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_UNKNOWN)
         ozone->thumbnails_left_status_prev  = ozone->thumbnails.left.status;
      if (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_UNKNOWN)
         ozone->thumbnails_right_status_prev = ozone->thumbnails.right.status;
   }

   i = menu_st->entries.begin;

   if (i >= entries_end)
      menu_st->entries.begin = 0;

   GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
}

static void ozone_draw_header(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool battery_level_enable,
      bool timedate_enable,
      math_matrix_4x4 *mymat)
{
   char title[255];
   static const char* const ticker_spacer   = OZONE_TICKER_SPACER;
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   unsigned ticker_x_offset                 = 0;
   unsigned timedate_offset                 = 0;
   bool use_smooth_ticker                   = settings->bools.menu_ticker_smooth;
   float scale_factor                       = ozone->last_scale_factor;
   float *col                               = ozone->theme->entries_icon;
   unsigned logo_icon_size                  = 60 * scale_factor;
   unsigned status_icon_size                = 92 * scale_factor;
   unsigned status_row_size                 = 160 * scale_factor;
   unsigned seperator_margin                = 30 * scale_factor;
   enum gfx_animation_ticker_type
         menu_ticker_type                   = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   gfx_display_ctx_driver_t *dispctx        = p_disp->dispctx;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = p_anim->ticker_pixel_idx;
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

   /* Separator */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         seperator_margin,
         ozone->dimensions.header_height,
         video_width - seperator_margin * 2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->header_footer_separator,
         NULL);

   /* Icon */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      if (dispctx->draw)
      {
#ifdef HAVE_DISCORD_OWN_AVATAR
         if (discord_avatar_is_ready())
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  logo_icon_size,
                  logo_icon_size,
                  ozone->textures[OZONE_TEXTURE_DISCORD_OWN_AVATAR],
                  47 * scale_factor,
                  14 * scale_factor, /* Where does this come from...? */
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);
         else
#endif
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  logo_icon_size,
                  logo_icon_size,
                  ozone->textures[OZONE_TEXTURE_RETROARCH],
                  47 * scale_factor,
                  (ozone->dimensions.header_height - logo_icon_size) / 2,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);
      }
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   /* Battery */
   if (battery_level_enable)
   {
      gfx_display_ctx_powerstate_t powerstate;
      char msg[12];

      msg[0] = '\0';

      powerstate.s   = msg;
      powerstate.len = sizeof(msg);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         timedate_offset = 90 * scale_factor;
         status_row_size += timedate_offset;

         gfx_display_draw_text(
               ozone->fonts.time.font,
               msg,
               video_width - 55 * scale_factor,
               ozone->dimensions.header_height / 2 + ozone->fonts.time.line_centre_offset,
               video_width,
               video_height,
               ozone->theme->text_rgba,
               TEXT_ALIGN_RIGHT,
               1.0f,
               false,
               1.0f,
               false);

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
                     status_icon_size,
                     status_icon_size,
                     ozone->icons_textures[powerstate.charging
                           ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING
                           : (powerstate.percent > 80) ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL
                           : (powerstate.percent > 60) ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80
                           : (powerstate.percent > 40) ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60
                           : (powerstate.percent > 20) ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40
                           : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20],
                     video_width - (55 + 32) * scale_factor,
                     0,
                     video_width,
                     video_height,
                     0.0f,
                     1.0f,
                     col,
                     mymat);
            if (dispctx->blend_end)
               dispctx->blend_end(userdata);
         }
      }
   }

   /* Timedate */
   if (timedate_enable)
   {
      gfx_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0]             = '\0';

      datetime.s              = timedate;
      datetime.time_mode      = settings->uints.menu_timedate_style;
      datetime.date_separator = settings->uints.menu_timedate_date_separator;
      datetime.len            = sizeof(timedate);

      menu_display_timedate(&datetime);

      gfx_display_draw_text(
            ozone->fonts.time.font,
            timedate,
            video_width - (64 * scale_factor) - timedate_offset,
            ozone->dimensions.header_height / 2 + ozone->fonts.time.line_centre_offset,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_RIGHT,
            1.0f,
            false,
            1.0f,
            false);

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
                  status_icon_size,
                  status_icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOCK],
                  video_width - (64 + 28) * scale_factor - timedate_offset,
                  0,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);
         if (dispctx->blend_end)
            dispctx->blend_end(userdata);
      }

      status_row_size += 240 * scale_factor;
   }

   /* Title */
   if (use_smooth_ticker)
   {
      ticker_smooth.font        = ozone->fonts.title.font;
      ticker_smooth.selected    = true;
      ticker_smooth.field_width = video_width - status_row_size;
      ticker_smooth.src_str     = (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
            ? ozone->fullscreen_thumbnail_label
            : ozone->title;
      ticker_smooth.dst_str     = title;
      ticker_smooth.dst_str_len = sizeof(title);

      gfx_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = title;
      ticker.len      = video_width - status_row_size / ozone->fonts.title.glyph_width;
      ticker.str      = (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
            ? ozone->fullscreen_thumbnail_label
            : ozone->title;
      ticker.selected = true;

      gfx_animation_ticker(&ticker);
   }

   gfx_display_draw_text(
         ozone->fonts.title.font,
         title,
         ticker_x_offset + 128 * scale_factor,
         ozone->dimensions.header_height / 2 + ozone->fonts.title.line_centre_offset,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);
}

static void ozone_draw_footer(
      ozone_handle_t *ozone,
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool input_menu_swap_ok_cancel_buttons,
      settings_t *settings,
      math_matrix_4x4 *mymat)
{
   bool menu_core_enable                  = settings->bools.menu_core_enable;
   bool search_enabled                    = !settings->bools.menu_disable_search_button;
   size_t selection                       = ozone->selection;
   float scale_factor                     = ozone->last_scale_factor;
   unsigned seperator_margin              = 30 * scale_factor;
   float footer_margin                    = 42 * scale_factor;
   float footer_text_y                    = (float)video_height
         - (ozone->dimensions.footer_height / 2.0f)
         + ozone->fonts.footer.line_centre_offset;
   float icon_size                        = 35 * scale_factor;
   float icon_padding                     = 10 * scale_factor;
   float icon_padding_small               = icon_padding / 5;
   float icon_y                           = (float)video_height
         - (ozone->dimensions.footer_height / 2.0f)
         - (icon_size / 2.0f);
   /* Button enable states
    * > Note: Only show 'metadata_toggle' if
    *   'fullscreen_thumbs' is shown. This condition
    *   should be guaranteed anyway, but enforce it
    *   here to prevent 'gaps' in the button list in
    *   the event of unknown errors */
   bool fs_thumbnails_available   =
         ozone_fullscreen_thumbnails_available(ozone, menu_st);
   bool metadata_override_available       =
            fs_thumbnails_available
         && ozone_metadata_override_available(ozone, settings);
   bool thumbnail_cycle_enabled           =
            fs_thumbnails_available
         && !(ozone->flags & OZONE_FLAG_IS_FILE_LIST)
         && !((ozone->is_quick_menu && menu_is_running_quick_menu())
               || (ozone->flags & OZONE_FLAG_IS_STATE_SLOT));
   bool clear_setting_enabled            =
            !thumbnail_cycle_enabled
         && ozone_clear_available(ozone, selection);
   bool scan_enabled                     =
            !thumbnail_cycle_enabled
         && !clear_setting_enabled
         && ozone_scan_available(ozone, selection);
   bool reset_to_default_available        =
            !fs_thumbnails_available
         && ozone_is_current_entry_settings(selection);
   bool help_available                    =
            !metadata_override_available
         && ozone_help_available(ozone, selection, settings->bools.menu_show_sublabels);
   bool manage_available                  =
         ozone_manage_available(ozone, selection);

   /* Determine x origin positions of each
    * button
    * > From right to left, these are ordered:
    *   - ok
    *   - back
    *   - (X) search
    *   - (Y) cycle thumbnails (playlists only)
    *   - (Y) clear settings (keybinds only)
    *   - (Y) scan entry (certain file types only)
    *   - (Start) toggle fullscreen thumbs (playlists only)
    *   - (Start) reset to default (settings only)
    *   - (Start) manage playlist (sidebar only)
    *   - (Select) toggle metadata (playlists only)
    *   - (Select) help (non-playlist only) */
   float ok_x                             = (float)video_width
         - footer_margin - ozone->footer_labels.ok.width - icon_size - icon_padding;
   float back_x                           = ok_x
         - ozone->footer_labels.back.width - icon_size - (2.0f * icon_padding);
   float search_x                         = (search_enabled)
         ? back_x - ozone->footer_labels.search.width - icon_size - (2.0f * icon_padding)
         : back_x;
   float cycle_x                          = (thumbnail_cycle_enabled)
         ? search_x - ozone->footer_labels.cycle.width - icon_size - (2.0f * icon_padding)
         : search_x;
   float clear_x                          = (clear_setting_enabled)
         ? cycle_x - ozone->footer_labels.clear.width - icon_size - (2.0f * icon_padding)
         : cycle_x;
   float scan_x                           = (scan_enabled)
         ? clear_x - ozone->footer_labels.scan.width - icon_size - (2.0f * icon_padding)
         : clear_x;
   float reset_to_default_x               = (reset_to_default_available)
         ? scan_x - ozone->footer_labels.reset_to_default.width - icon_size - (2.0f * icon_padding)
         : scan_x;
   float help_x                           = (help_available)
         ? reset_to_default_x - ozone->footer_labels.help.width - icon_size - (2.0f * icon_padding)
         : reset_to_default_x;
   float fullscreen_thumbs_x              = (fs_thumbnails_available)
         ? help_x - ozone->footer_labels.fullscreen_thumbs.width - icon_size - (2.0f * icon_padding)
         : help_x;
   float manage_x                         = (manage_available)
         ? fullscreen_thumbs_x - ozone->footer_labels.manage.width - icon_size - (2.0f * icon_padding)
         : fullscreen_thumbs_x;
   float metadata_toggle_x                = manage_x
         - ozone->footer_labels.metadata_toggle.width - icon_size - (2.0f * icon_padding);
   gfx_display_ctx_driver_t *dispctx      = p_disp->dispctx;
   float *col                             = ozone->theme_dynamic.entries_icon;

   /* Separator */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         seperator_margin,
         video_height - ozone->dimensions.footer_height,
         video_width - seperator_margin * 2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->header_footer_separator,
         NULL);

   /* Buttons */

   /* Draw icons */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      gfx_display_set_alpha(ozone->theme_dynamic.entries_icon, 1.0f);

      if (dispctx->draw)
      {
         /* > Ok */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons
                     ? ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D]
                     : ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R],
               ok_x,
               icon_y,
               video_width,
               video_height,
               0.0f,
               1.0f,
               col,
               mymat);

         /* > Back */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons
                     ? ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R]
                     : ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D],
               back_x,
               icon_y,
               video_width,
               video_height,
               0.0f,
               1.0f,
               col,
               mymat);

         /* > Search */
         if (search_enabled)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U],
                  search_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Thumbnail cycle */
         if (thumbnail_cycle_enabled)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L],
                  cycle_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Fullscreen thumbnais */
         if (fs_thumbnails_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  (ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE ||
                   ozone->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_PENDING)
                        ? ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L]
                        : ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START],
                  fullscreen_thumbs_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Metadata toggle */
         if (metadata_override_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT],
                  metadata_toggle_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Reset to default */
         if (reset_to_default_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START],
                  reset_to_default_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Help */
         if (help_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT],
                  help_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Clear setting */
         if (clear_setting_enabled)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L],
                  clear_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Scan entry */
         if (scan_enabled)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L],
                  scan_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > Manage */
         if (manage_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START],
                  manage_x,
                  icon_y,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);
      }

      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   /* Draw labels */

   /* > Ok */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.ok.str,
         ok_x + icon_size + icon_padding_small,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > Back */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.back.str,
         back_x + icon_size + icon_padding_small,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > Search */
   if (search_enabled)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.search.str,
            search_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Thumbnail cycle */
   if (thumbnail_cycle_enabled)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.cycle.str,
            cycle_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Fullscreen thumbnails */
   if (fs_thumbnails_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.fullscreen_thumbs.str,
            fullscreen_thumbs_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Metadata toggle */
   if (metadata_override_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.metadata_toggle.str,
            metadata_toggle_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Reset to default */
   if (reset_to_default_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.reset_to_default.str,
            reset_to_default_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Help */
   if (help_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.help.str,
            help_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Clear settings */
   if (clear_setting_enabled)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.clear.str,
            clear_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Scan entry */
   if (scan_enabled)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.scan.str,
            scan_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > Manage */
   if (manage_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.manage.str,
            manage_x + icon_size + icon_padding_small,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* Core title or Switch icon */
   if (menu_core_enable)
   {
      gfx_animation_ctx_ticker_t ticker;
      gfx_animation_ctx_ticker_smooth_t ticker_smooth;
      char core_title[255];
      char core_title_buf[255];
      static const char* const ticker_spacer          = OZONE_TICKER_SPACER;
      int usable_width;
      unsigned ticker_x_offset                        = 0;
      bool use_smooth_ticker                          =
            settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type menu_ticker_type =
            (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;

      core_title[0]     = '\0';
      core_title_buf[0] = '\0';

      /* Determine available width for core
       * title string */
      usable_width = metadata_override_available
            ? metadata_toggle_x
            : fs_thumbnails_available
            ? fullscreen_thumbs_x
            : search_x;
      usable_width -= footer_margin + (icon_padding * 2);

      if (usable_width > 0)
      {
         /* Get core title */
         menu_entries_get_core_title(core_title, sizeof(core_title));

         /* Configure and run ticker */
         if (use_smooth_ticker)
         {
            ticker_smooth.idx           = p_anim->ticker_pixel_idx;
            ticker_smooth.font_scale    = 1.0f;
            ticker_smooth.type_enum     = menu_ticker_type;
            ticker_smooth.spacer        = ticker_spacer;
            ticker_smooth.x_offset      = &ticker_x_offset;
            ticker_smooth.dst_str_width = NULL;

            ticker_smooth.font          = ozone->fonts.footer.font;
            ticker_smooth.selected      = true;
            ticker_smooth.field_width   = usable_width;
            ticker_smooth.src_str       = core_title;
            ticker_smooth.dst_str       = core_title_buf;
            ticker_smooth.dst_str_len   = sizeof(core_title_buf);

            gfx_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.idx                  = p_anim->ticker_idx;
            ticker.type_enum            = menu_ticker_type;
            ticker.spacer               = ticker_spacer;

            ticker.s                    = core_title_buf;
            ticker.len                  = usable_width / ozone->fonts.footer.glyph_width;
            ticker.str                  = core_title;
            ticker.selected             = true;

            gfx_animation_ticker(&ticker);
         }

         /* Draw text */
         gfx_display_draw_text(
               ozone->fonts.footer.font,
               core_title_buf,
               ticker_x_offset + footer_margin,
               footer_text_y,
               video_width,
               video_height,
               ozone->theme->text_rgba,
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               false);
      }
   }
#ifdef HAVE_LIBNX
   else if (ozone->theme->name)
   {
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
                  69 * scale_factor,
                  30 * scale_factor,
                  ozone->theme->textures[OZONE_THEME_TEXTURE_SWITCH],
                  footer_margin,
                  video_height - ozone->dimensions.footer_height / 2 - 15 * scale_factor,
                  video_width,
                  video_height,
                  0.0f,
                  1.0f,
                  ozone->pure_white,
                  mymat);
         if (dispctx->blend_end)
            dispctx->blend_end(userdata);
      }
   }
#endif
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_list, 0);
   size_t new_selection       = menu_st->selection_ptr;
   ozone_node_t *node         = (ozone_node_t*)selection_buf->list[new_selection].userdata;

   if (!node)
      return;

   if (ozone->selection != new_selection)
   {
      menu_entry_t entry;
      unsigned entry_type;
      uintptr_t tag                = (uintptr_t)selection_buf;
      size_t selection             = new_selection;

      MENU_ENTRY_INITIALIZE(entry);
      menu_entry_get(&entry, 0, selection, NULL, true);

      entry_type                   = entry.type;

      ozone->selection_old         = ozone->selection;
      ozone->selection             = new_selection;

      if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
         ozone->flags             |=  OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;
      else
         ozone->flags             &= ~OZONE_FLAG_CURSOR_IN_SIDEBAR_OLD;

      gfx_animation_kill_by_tag(&tag);
      ozone_update_scroll(ozone, allow_animation, node);

      /* Update thumbnail */
      if (   gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
          || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         bool update_thumbnails = false;

         /* Playlist updates */
         if (     (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
               && (ozone->depth == 1 || ozone->depth == 4))
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails           = true;
            ozone->flags               &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;
         }
         /* Database + Explore list updates */
         else if (  ((ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && (ozone->depth == 4))
               ||    (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST))
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails           = true;
            ozone->flags               &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;
         }
         /* Filebrowser image updates */
         else if (ozone->flags & OZONE_FLAG_IS_FILE_LIST)
         {
            if (   (entry_type == FILE_TYPE_IMAGEVIEWER)
                || (entry_type == FILE_TYPE_IMAGE))
            {
               ozone_set_thumbnail_content(ozone, "imageviewer");
               update_thumbnails                      = true;
               ozone->flags |= OZONE_FLAG_WANT_THUMBNAIL_BAR
                             | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
            }
            else
            {
               /* If this is a file list and current
                * entry is not an image, have to 'reset'
                * content + right/left thumbnails
                * (otherwise last loaded thumbnail will
                * persist, and be shown on the wrong entry) */
               gfx_thumbnail_set_content(menu_st->thumbnail_path_data, NULL);
               ozone_unload_thumbnail_textures(ozone);
               update_thumbnails         = true;
               ozone->flags             &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                                           | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);
            }
         }

         if (update_thumbnails)
            ozone_update_thumbnail_image(ozone);
      }

      ozone_update_savestate_thumbnail_path(ozone, (unsigned)ozone->selection);
      ozone_update_savestate_thumbnail_image(ozone);
   }
}

static void ozone_navigation_clear(void *data, bool pending_push)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!pending_push)
      ozone_selection_changed(ozone, true);
}

static void ozone_navigation_pointer_changed(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_set(void *data, bool scroll)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_alphabet(void *data, size_t *unused)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_messagebox_fadeout_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   free(ozone->pending_message);
   ozone->pending_message        = NULL;
   ozone->flags                 &= ~OZONE_FLAG_SHOULD_DRAW_MSGBOX;
}

static void ozone_frame(void *data, video_frame_info_t *video_info)
{
   math_matrix_4x4 mymat;
   gfx_animation_ctx_entry_t entry;
   bool ozone_last_use_preferred_system_color_theme;
   ozone_handle_t* ozone                  = (ozone_handle_t*)data;
   settings_t  *settings                  = config_get_ptr();
   unsigned color_theme                   = settings->uints.menu_ozone_color_theme;
   bool use_preferred_system_color_theme  = settings->bools.menu_use_preferred_system_color_theme;
   uintptr_t messagebox_tag               = (uintptr_t)ozone->pending_message;
   bool draw_osk                          = menu_input_dialog_get_display_kb();
   static bool draw_osk_old               = false;
   float *background_color                = NULL;
   void *userdata                         = video_info->userdata;
   unsigned video_width                   = video_info->width;
   unsigned video_height                  = video_info->height;
   float menu_framebuffer_opacity         = video_info->menu_framebuffer_opacity;
   bool libretro_running                  = video_info->libretro_running;
   bool video_fullscreen                  = video_info->fullscreen;
   bool mouse_grabbed                     = video_info->input_driver_grab_mouse_state;
   bool menu_mouse_enable                 = video_info->menu_mouse_enable;
   bool input_menu_swap_ok_cancel_buttons = video_info->input_menu_swap_ok_cancel_buttons;
   bool battery_level_enable              = video_info->battery_level_enable;
   bool timedate_enable                   = video_info->timedate_enable;
   gfx_display_t            *p_disp       = (gfx_display_t*)video_info->disp_userdata;
   gfx_animation_t *p_anim                = anim_get_ptr();
   gfx_display_ctx_driver_t *dispctx      = p_disp->dispctx;
   video_driver_state_t *video_st         = video_state_get_ptr();
   struct menu_state *menu_st             = menu_state_get_ptr();
   menu_list_t *menu_list                 = menu_st->entries.list;

#ifdef HAVE_DISCORD_OWN_AVATAR
   static bool reset                      = false;

   if (discord_avatar_is_ready() && !reset)
   {
      ozone_context_reset(data, false);
      reset = true;
   }
#endif

   if (!ozone)
      return;

   /* Reset thumbnail bar when starting/closing content */
   if (((ozone->flags & OZONE_FLAG_LIBRETRO_RUNNING) > 0) != libretro_running)
   {
      if (libretro_running)
         ozone->flags          |=  OZONE_FLAG_LIBRETRO_RUNNING;
      else
         ozone->flags          &= ~OZONE_FLAG_LIBRETRO_RUNNING;
      ozone->flags             |=  OZONE_FLAG_NEED_COMPUTE;

      if (ozone->is_quick_menu)
      {
         if (libretro_running)
            ozone->flags       &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                               |    OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);
         else
            ozone->flags       |=  (OZONE_FLAG_WANT_THUMBNAIL_BAR
                               |    OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);
      }
   }

   if (     (   (size_t)ozone->selection != ozone->fullscreen_thumbnail_selection
            &&  (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS))
         || (   (ozone->flags2 & OZONE_FLAG2_WANT_FULLSCREEN_THUMBNAILS)
            && !(ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)))
   {
      ozone->flags             |= OZONE_FLAG_NEED_COMPUTE;
      ozone_show_fullscreen_thumbnails(ozone);
   }

   if (ozone->flags & OZONE_FLAG_FIRST_FRAME)
   {
      menu_input_get_pointer_state(&ozone->pointer);

      ozone->cursor_x_old = ozone->pointer.x;
      ozone->cursor_y_old = ozone->pointer.y;
      ozone->flags       &= ~OZONE_FLAG_FIRST_FRAME;
   }

   /* OSK Fade detection */
   if (draw_osk != draw_osk_old)
   {
      draw_osk_old = draw_osk;
      if (!draw_osk)
      {
         ozone->flags                       &= ~(OZONE_FLAG_SHOULD_DRAW_MSGBOX
                                               | OZONE_FLAG_MSGBOX_STATE
                                               | OZONE_FLAG_MSGBOX_STATE_OLD);
         ozone->animations.messagebox_alpha  = 0.0f;
      }
   }

   /* Change theme on the fly */
   ozone_last_use_preferred_system_color_theme =
         ozone->flags2 & OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME;

   if (   (color_theme != ozone_last_color_theme)
       || (ozone_last_use_preferred_system_color_theme != use_preferred_system_color_theme))
   {
      if (use_preferred_system_color_theme)
      {
         color_theme                           = ozone_get_system_theme();
         configuration_set_uint(settings,
               settings->uints.menu_ozone_color_theme, color_theme);
      }

      ozone_set_color_theme(ozone, color_theme);
      if (ozone->theme->background_libretro_running)
         ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      if (use_preferred_system_color_theme)
         ozone->flags2 |=  OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME;
      else
         ozone->flags2 &= ~OZONE_FLAG2_LAST_USE_PREFERRED_SYSTEM_COLOR_THEME;
   }

   /* If menu screensaver is active, draw
    * screensaver and return */
   if (ozone->flags & OZONE_FLAG_SHOW_SCREENSAVER)
   {
      menu_screensaver_frame(ozone->screensaver,
            video_info, p_disp);
      return;
   }

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, video_width, video_height, true, false);

   /* Clear text */
   font_bind(&ozone->fonts.footer);
   font_bind(&ozone->fonts.title);
   font_bind(&ozone->fonts.time);
   font_bind(&ozone->fonts.entries_label);
   font_bind(&ozone->fonts.entries_sublabel);
   font_bind(&ozone->fonts.sidebar);

   /* Background (Always use running background due to overlays) */
   if (menu_framebuffer_opacity < 1.0f)
   {
      if (menu_framebuffer_opacity != ozone_last_framebuffer_opacity)
         if (ozone->theme->background_libretro_running)
            ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      background_color = ozone->theme->background_libretro_running;
   }
   else
      background_color = ozone->theme->background;

   gfx_display_draw_quad(p_disp,
         userdata,
         video_width,
         video_height,
         0,
         0,
         video_width,
         video_height,
         video_width,
         video_height,
         background_color,
         NULL);

   if (!p_disp->dispctx->handles_transform)
   {
      float cosine             = 1.0f; /* cos(rad)  = cos(0)  = 1.0f */
      float sine               = 0.0f; /* sine(rad) = sine(0) = 0.0f */
      gfx_display_rotate_z(p_disp, &mymat, cosine, sine, userdata);
   }

   /* Header, footer */
   ozone_draw_header(ozone,
         p_disp,
         p_anim,
         settings,
         userdata,
         video_width,
         video_height,
         battery_level_enable,
         timedate_enable,
         &mymat);

   ozone_draw_footer(ozone,
         menu_st,
         p_disp,
         p_anim,
         userdata,
         video_width,
         video_height,
         input_menu_swap_ok_cancel_buttons,
         settings,
         &mymat);

   /* Sidebar */
   if (ozone->flags & OZONE_FLAG_DRAW_SIDEBAR)
      ozone_draw_sidebar(ozone,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            libretro_running,
            menu_framebuffer_opacity,
            &mymat);

   /* Menu entries */
   if (p_disp->dispctx && p_disp->dispctx->scissor_begin)
      gfx_display_scissor_begin(p_disp,
            userdata,
            video_width,
            video_height,
            ozone->sidebar_offset + (unsigned)ozone->dimensions_sidebar_width,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            video_width - (unsigned)ozone->dimensions_sidebar_width + (-ozone->sidebar_offset),
            video_height
                  - ozone->dimensions.header_height
                  - ozone->dimensions.footer_height
                  - ozone->dimensions.spacer_1px);

   /* Current list */
   ozone_draw_entries(ozone,
         p_disp,
         p_anim,
         settings,
         userdata,
         video_width,
         video_height,
         (unsigned)ozone->selection,
         (unsigned)ozone->selection_old,
         MENU_LIST_GET_SELECTION(menu_list, 0),
         ozone->animations.list_alpha,
         ozone->animations.scroll_y,
         (ozone->flags & OZONE_FLAG_IS_PLAYLIST) ? true : false,
         &mymat
         );

   /* Old list */
   if (ozone->flags & OZONE_FLAG_DRAW_OLD_LIST)
      ozone_draw_entries(ozone,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            (unsigned)ozone->selection_old_list,
            (unsigned)ozone->selection_old_list,
            &ozone->selection_buf_old,
            ozone->animations.list_alpha,
            ozone->scroll_old,
            (ozone->flags & OZONE_FLAG_IS_PLAYLIST_OLD) ? true : false,
            &mymat
      );

   /* Thumbnail bar */
   if (ozone->show_thumbnail_bar)
      ozone_draw_thumbnail_bar(ozone,
            menu_st,
            p_disp,
            p_anim,
            settings,
            userdata,
            video_width,
            video_height,
            libretro_running,
            menu_framebuffer_opacity,
            &mymat);

   if (dispctx && dispctx->scissor_end)
      dispctx->scissor_end(userdata, video_width, video_height);

   /* Flush first layer of text */
   font_flush(video_width, video_height, &ozone->fonts.footer);
   font_flush(video_width, video_height, &ozone->fonts.title);
   font_flush(video_width, video_height, &ozone->fonts.time);
   font_flush(video_width, video_height, &ozone->fonts.entries_label);
   font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
   font_flush(video_width, video_height, &ozone->fonts.sidebar);

   /* Draw fullscreen thumbnails, if required */
   ozone_draw_fullscreen_thumbnails(ozone,
         userdata,
         video_info->disp_userdata,
         video_width,
         video_height);

   /* Message box & OSK - second layer of text */
   if (    (ozone->flags & OZONE_FLAG_SHOULD_DRAW_MSGBOX)
         || draw_osk)
   {
      /* Fade in animation */
      if (     (((ozone->flags & OZONE_FLAG_MSGBOX_STATE_OLD) > 0) != ((ozone->flags & OZONE_FLAG_MSGBOX_STATE) > 0))
            && (((ozone->flags & OZONE_FLAG_MSGBOX_STATE) > 0)))
      {
         if (ozone->flags & OZONE_FLAG_MSGBOX_STATE)
            ozone->flags                   |=  OZONE_FLAG_MSGBOX_STATE_OLD;
         else
            ozone->flags                   &= ~OZONE_FLAG_MSGBOX_STATE_OLD;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 0.0f;

         entry.cb                           = NULL;
         entry.duration                     = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum                  = OZONE_EASING_ALPHA;
         entry.subject                      = &ozone->animations.messagebox_alpha;
         entry.tag                          = messagebox_tag;
         entry.target_value                 = 1.0f;
         entry.userdata                     = NULL;

         gfx_animation_push(&entry);
      }
      /* Fade out animation */
      else if   (((ozone->flags & OZONE_FLAG_MSGBOX_STATE_OLD) > 0) != ((ozone->flags & OZONE_FLAG_MSGBOX_STATE) > 0)
            &&  (!(ozone->flags & OZONE_FLAG_MSGBOX_STATE)))
      {
         if (ozone->flags & OZONE_FLAG_MSGBOX_STATE)
            ozone->flags                   &= ~OZONE_FLAG_MSGBOX_STATE_OLD;
         else
            ozone->flags                   &= ~OZONE_FLAG_MSGBOX_STATE_OLD;
         ozone->flags                      &= ~OZONE_FLAG_MSGBOX_STATE;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 1.0f;

         entry.cb                           = ozone_messagebox_fadeout_cb;
         entry.duration                     = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum                  = OZONE_EASING_ALPHA;
         entry.subject                      = &ozone->animations.messagebox_alpha;
         entry.tag                          = messagebox_tag;
         entry.target_value                 = 0.0f;
         entry.userdata                     = ozone;

         gfx_animation_push(&entry);
      }

      ozone_draw_backdrop(userdata,
            video_info->disp_userdata,
            video_width,
            video_height,
            float_min(ozone->animations.messagebox_alpha, 0.75f));

      if (draw_osk)
      {
         struct menu_state *menu_st  = menu_state_get_ptr();
         const char *label           = menu_st->input_dialog_kb_label;
         const char *str             = menu_input_dialog_get_buffer();

         ozone_draw_osk(ozone,
               userdata,
               video_info->disp_userdata,
               video_width,
               video_height,
               label,
               str);
      }
      else
         ozone_draw_messagebox(ozone,
               p_disp,
               userdata,
               video_width,
               video_height,
               ozone->pending_message,
               &mymat);

      /* Flush second layer of text */
      font_flush(video_width, video_height, &ozone->fonts.footer);
      font_flush(video_width, video_height, &ozone->fonts.entries_label);
   }

   /* Cursor */
   if (     (ozone->flags & OZONE_FLAG_SHOW_CURSOR)
         && (ozone->pointer.type != MENU_POINTER_DISABLED))
   {
      bool cursor_visible = (video_fullscreen || mouse_grabbed) && menu_mouse_enable;

      gfx_display_set_alpha(ozone->pure_white, 1.0f);
      if (cursor_visible)
         gfx_display_draw_cursor(p_disp,
               userdata,
               video_width,
               video_height,
               cursor_visible,
               ozone->pure_white,
               ozone->dimensions.cursor_size,
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POINTER],
               ozone->pointer.x,
               ozone->pointer.y,
               video_width,
               video_height);
   }

   /* Unbind fonts */
   font_unbind(&ozone->fonts.footer);
   font_unbind(&ozone->fonts.title);
   font_unbind(&ozone->fonts.time);
   font_unbind(&ozone->fonts.entries_label);
   font_unbind(&ozone->fonts.entries_sublabel);
   font_unbind(&ozone->fonts.sidebar);

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, video_width, video_height, false, true);
}

static void ozone_set_header(ozone_handle_t *ozone)
{
   if (     (ozone->categories_selection_ptr <= ozone->system_tab_end)
         || (ozone->is_quick_menu && !menu_is_running_quick_menu())
         || (ozone->depth > 1))
      menu_entries_get_title(ozone->title, sizeof(ozone->title));
   else
   {
      ozone_node_t *node = (ozone_node_t*)file_list_get_userdata_at_offset(
            &ozone->horizontal_list,
            ozone->categories_selection_ptr - ozone->system_tab_end - 1);

      if (node && node->console_name)
      {
         strlcpy(ozone->title, node->console_name, sizeof(ozone->title));

         /* Add current search terms */
         menu_entries_search_append_terms_string(ozone->title, sizeof(ozone->title));
      }
   }
}

static void ozone_animation_end(void *userdata)
{
   ozone_handle_t *ozone            = (ozone_handle_t*) userdata;
   ozone->flags                    &= ~(OZONE_FLAG_DRAW_OLD_LIST);
   ozone->animations.cursor_alpha   = 1.0f;
}

static void ozone_list_open(
      ozone_handle_t *ozone,
      bool ozone_collapse_sidebar,
      bool animate)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t sidebar_tag           = (uintptr_t)&ozone->sidebar_offset;

   ozone->flags                   |= (OZONE_FLAG_DRAW_OLD_LIST);

   /* Sidebar animation */
   ozone_sidebar_update_collapse(ozone, ozone_collapse_sidebar, animate);

   /* Kill any existing sidebar slide-in/out animations
    * before pushing a new one
    * > This is required since the 'ozone_collapse_end'
    *   callback from an unfinished slide-out animation
    *   may subsequently override the OZONE_FLAG_DRAW_SIDEBAR
    *   value set at the beginning of the next slide-in
    *   animation... */
   gfx_animation_kill_by_tag(&sidebar_tag);

   if (ozone->depth == 1)
   {
      ozone->flags            |= OZONE_FLAG_DRAW_SIDEBAR;

      if (animate)
      {
         entry.cb              = NULL;
         entry.duration        = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum     = OZONE_EASING_XY;
         entry.subject         = &ozone->sidebar_offset;
         entry.tag             = sidebar_tag;
         entry.target_value    = 0.0f;
         entry.userdata        = NULL;

         gfx_animation_push(&entry);

         /* Skip "left/right" animation if animating already */
         if (ozone->sidebar_offset != entry.target_value)
            animate = false;
      }
      else
         ozone->sidebar_offset = 0.0f;
   }
   else if (ozone->depth > 1)
   {
      if (animate)
      {
         struct gfx_animation_ctx_entry entry;

         entry.cb              = ozone_collapse_end;
         entry.duration        = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum     = OZONE_EASING_XY;
         entry.subject         = &ozone->sidebar_offset;
         entry.tag             = sidebar_tag;
         entry.target_value    = -ozone->dimensions_sidebar_width;
         entry.userdata        = ozone;

         gfx_animation_push(&entry);

         /* Skip "left/right" animation if animating already */
         if (ozone->sidebar_offset != entry.target_value)
            animate = false;
      }
      else
         ozone->sidebar_offset = -ozone->dimensions_sidebar_width;
   }

   /* Left/right animation */
   if (animate)
   {
      ozone->animations.list_alpha = 0.0f;

      entry.cb                     = ozone_animation_end;
      entry.duration               = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum            = OZONE_EASING_ALPHA;
      entry.subject                = &ozone->animations.list_alpha;
      entry.tag                    = (uintptr_t)NULL;
      entry.target_value           = 1.0f;
      entry.userdata               = ozone;

      gfx_animation_push(&entry);
   }
   else
      ozone->animations.list_alpha = 1.0f;
}

static void ozone_populate_entries(
      void *data,
      const char *path,
      const char *label,
      unsigned k)
{
   int new_depth                        = 0;
   bool ozone_collapse_sidebar          = false;
   bool was_db_manager_list             = false;
   bool want_thumbnail_bar              = false;
   bool fs_thumbnails_available         = false;
   bool animate                         = false;
   struct menu_state *menu_st           = menu_state_get_ptr();
   menu_list_t *menu_list               = menu_st->entries.list;
   settings_t *settings                 = NULL;
   ozone_handle_t *ozone                = (ozone_handle_t*) data;

   if (!ozone)
      return;

   settings                             = config_get_ptr();
   ozone_collapse_sidebar               = settings->bools.ozone_collapse_sidebar;

   if ((menu_st->flags & MENU_ST_FLAG_PREVENT_POPULATE) > 0)
   {
      menu_st->flags                 &=  ~MENU_ST_FLAG_PREVENT_POPULATE;
      ozone_selection_changed(ozone, false);

      /* Refresh title for search terms */
      ozone_set_header(ozone);

      /* Handle playlist searches
       * (Ozone is a fickle beast...) */
      if (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
      {
         menu_search_terms_t *menu_search_terms = menu_entries_search_get_terms();
         size_t num_search_terms                = menu_search_terms
               ? menu_search_terms->size
               : 0;

         if (ozone->num_search_terms_old != num_search_terms)
         {
            /* Refresh thumbnails */
            ozone_unload_thumbnail_textures(ozone);

            if (   gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
                || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
            {
               ozone_set_thumbnail_content(ozone, "");
               ozone_update_thumbnail_image(ozone);
            }

            /* If we are currently inside an empty
             * playlist, return to the sidebar */
            if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
            {
               file_list_t *list       = MENU_LIST_GET_SELECTION(menu_list, 0);
               uintptr_t animation_tag = (uintptr_t)&ozone->animations.cursor_alpha;
               bool goto_sidebar       = false;

               if (!list || (list->size < 1))
                  goto_sidebar         = true;
               else if ((list->list[0].type != FILE_TYPE_RPL_ENTRY))
                  goto_sidebar         = true;

               if (goto_sidebar)
               {
                  gfx_animation_kill_by_tag(&animation_tag);
                  ozone->flags |= OZONE_FLAG_EMPTY_PLAYLIST;
                  ozone_go_to_sidebar(ozone, ozone_collapse_sidebar, animation_tag);
               }
            }

            ozone->num_search_terms_old = num_search_terms;
         }
      }
      return;
   }

   ozone->flags               |=  OZONE_FLAG_NEED_COMPUTE;
   ozone->flags               &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;

   if (ozone->is_quick_menu)
      ozone->flags            |=  OZONE_FLAG_WAS_QUICK_MENU;
   else
      ozone->flags            &= ~OZONE_FLAG_WAS_QUICK_MENU;

   ozone->first_onscreen_entry = 0;
   ozone->last_onscreen_entry  = 0;

   new_depth                   = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);
   was_db_manager_list         = (ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && new_depth > ozone->depth;

   animate                     = new_depth != ozone->depth;

   if (new_depth <= ozone->depth)
      ozone->flags            |=  OZONE_FLAG_FADE_DIRECTION;
   else
      ozone->flags            &= ~OZONE_FLAG_FADE_DIRECTION;
   ozone->depth                = new_depth;

   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST)))
      ozone->flags            |=  OZONE_FLAG_IS_DB_MANAGER_LIST;
   else
      ozone->flags            &= ~OZONE_FLAG_IS_DB_MANAGER_LIST;

   if (  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST)) ||
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_TAB)) ||
         ozone_get_horizontal_selection_type(ozone) == MENU_EXPLORE_TAB)
      ozone->flags |=  OZONE_FLAG_IS_EXPLORE_LIST;
   else
      ozone->flags &= ~OZONE_FLAG_IS_EXPLORE_LIST;

   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)))
      ozone->flags |=  OZONE_FLAG_IS_FILE_LIST;
   else
      ozone->flags &= ~OZONE_FLAG_IS_FILE_LIST;

   ozone->is_quick_menu        = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVESTATE_LIST));

   if (  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)) ||
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST)))
      ozone->flags |=  OZONE_FLAG_IS_CONTENTLESS_CORES;
   else
      ozone->flags &= ~OZONE_FLAG_IS_CONTENTLESS_CORES;

   if (string_to_unsigned(path) == MENU_ENUM_LABEL_STATE_SLOT)
      ozone->flags |=  OZONE_FLAG_IS_STATE_SLOT;
   else
      ozone->flags &= ~OZONE_FLAG_IS_STATE_SLOT;

   if (ozone_is_playlist(ozone, true))
      ozone->flags |=  OZONE_FLAG_IS_PLAYLIST;
   else
      ozone->flags &= ~OZONE_FLAG_IS_PLAYLIST;

   ozone_set_header(ozone);

   if (was_db_manager_list)
      ozone->flags |= OZONE_FLAG_IS_DB_MANAGER_LIST
                    | OZONE_FLAG_SKIP_THUMBNAIL_RESET;

#if defined(HAVE_LIBRETRODB)
   if (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
   {
      /* Quick Menu under Explore list must also be Quick Menu */
      ozone->is_quick_menu |= menu_is_nonrunning_quick_menu() || menu_is_running_quick_menu();
      if (!menu_explore_is_content_list() || ozone->is_quick_menu)
         ozone->flags &= ~OZONE_FLAG_IS_EXPLORE_LIST;
   }
#endif

   if (animate)
      if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
         ozone_list_open(ozone, ozone_collapse_sidebar, (!(ozone->flags & OZONE_FLAG_FIRST_FRAME)));

   /* Reset savestate thumbnails always */
   ozone_update_savestate_thumbnail_path(ozone, (unsigned)menu_st->selection_ptr);
   ozone_update_savestate_thumbnail_image(ozone);

   /* Thumbnails
    * > Note: Leave current thumbnails loaded when
    *   opening the quick menu - allows proper fade
    *   out of the fullscreen thumbnail viewer
    * > Do not reset thumbnail when returning from quick menu */
   if (     (!(ozone->is_quick_menu))
         &&   (ozone->flags & OZONE_FLAG_WAS_QUICK_MENU)
         && (!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
         && (  gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
         ||    gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
         && (  ozone->thumbnails.left.status  == GFX_THUMBNAIL_STATUS_AVAILABLE
         ||    ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE
         ||    ozone->thumbnails.left.status  == GFX_THUMBNAIL_STATUS_MISSING
         ||    ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_MISSING))
      ozone->flags |= OZONE_FLAG_SKIP_THUMBNAIL_RESET;

   /* Always allow thumbnail reset in Information page */
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION)))
      ozone->flags &= ~OZONE_FLAG_SKIP_THUMBNAIL_RESET;

   if (ozone->is_quick_menu)
   {
      if (menu_is_running_quick_menu())
      {
         ozone->flags &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                         | OZONE_FLAG_SKIP_THUMBNAIL_RESET);
         ozone_update_savestate_thumbnail_path(ozone, (unsigned)menu_st->selection_ptr);
         ozone_update_savestate_thumbnail_image(ozone);
      }
      else if (   gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
               || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         ozone->flags |=  (OZONE_FLAG_WANT_THUMBNAIL_BAR
                         | OZONE_FLAG_SKIP_THUMBNAIL_RESET);
         ozone_update_thumbnail_image(ozone);
      }
   }
   else if (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
      ozone->flags |= OZONE_FLAG_WANT_THUMBNAIL_BAR;
   else if (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
   {
      ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
      ozone_set_thumbnail_content(ozone, "");
      ozone_update_thumbnail_image(ozone);
   }
   else if ((!(ozone->flags & OZONE_FLAG_IS_STATE_SLOT))
         && (!(ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET)))
   {
      if ((ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && (ozone->depth == 4))
         ozone->flags |= OZONE_FLAG_SKIP_THUMBNAIL_RESET;

      if (!(ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET))
         ozone_unload_thumbnail_textures(ozone);

      if (   gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
          || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         /* Only auto-load thumbnails if we are viewing
          * a playlist or a database manager list
          * > Note that we can ignore file browser lists,
          *   since the first selected item on such a list
          *   can never have a thumbnail */
         if (    (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
             || ((ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && (ozone->depth >= 4)))
         {
            ozone_set_thumbnail_content(ozone, "");
            ozone_update_thumbnail_image(ozone);
         }
         else if (ozone->flags & OZONE_FLAG_IS_FILE_LIST)
            ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
      }
   }

   if (     (ozone->flags & OZONE_FLAG_SKIP_THUMBNAIL_RESET)
         && (     gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
               || gfx_thumbnail_is_enabled(menu_st->thumbnail_path_data, GFX_THUMBNAIL_LEFT)))
      ozone->flags |= OZONE_FLAG_WANT_THUMBNAIL_BAR;

   switch (ozone->tabs[ozone->categories_selection_ptr])
   {
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_SYSTEM_TAB_VIDEO:
#endif
      case OZONE_SYSTEM_TAB_MUSIC:
         if (ozone->categories_selection_ptr <= ozone->system_tab_end)
            ozone->flags &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                            | OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE);
         break;
   }

   /* Fullscreen thumbnails are only available on
    * playlists, database manager lists, file lists
    * and savestate slots */
   if (
            (   (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR))
         && (  ((ozone->flags & OZONE_FLAG_IS_PLAYLIST) && (ozone->depth == 1 || ozone->depth == 4))
            || ((ozone->flags & OZONE_FLAG_IS_DB_MANAGER_LIST) && (ozone->depth >= 4))
            ||  (ozone->flags & OZONE_FLAG_IS_EXPLORE_LIST)
            ||  (ozone->flags & OZONE_FLAG_IS_FILE_LIST)
            ||  (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
            ||  (ozone->is_quick_menu)
            )
      )
      ozone->flags |=  OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
   else
      ozone->flags &= ~OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;

   fs_thumbnails_available = (ozone->flags & OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE) ? true : false;
   want_thumbnail_bar      = (ozone->flags & OZONE_FLAG_WANT_THUMBNAIL_BAR) ? true : false;

   if (fs_thumbnails_available != want_thumbnail_bar)
   {
      if (fs_thumbnails_available)
         ozone->flags |=  OZONE_FLAG_WANT_THUMBNAIL_BAR;
      else
         ozone->flags &= ~OZONE_FLAG_WANT_THUMBNAIL_BAR;
   }

   /* State slots must not allow fullscreen thumbnails when slot has no image */
   if (    (ozone->flags & OZONE_FLAG_IS_STATE_SLOT)
         && string_is_empty(ozone->savestate_thumbnail_file_path))
      ozone->flags &= ~OZONE_FLAG_FULLSCREEN_THUMBNAILS_AVAILABLE;
}

static void ozone_toggle(void *userdata, bool menu_on)
{
   settings_t *settings       = NULL;
   struct menu_state *menu_st = menu_state_get_ptr();
   ozone_handle_t *ozone      = (ozone_handle_t*) userdata;

   if (!ozone)
      return;

   /* Have to reset this, otherwise savestate
    * thumbnail won't update after selecting
    * 'save state' option */
   if (ozone->is_quick_menu)
   {
      ozone->flags              &= ~(OZONE_FLAG_WANT_THUMBNAIL_BAR
                                   | OZONE_FLAG_SKIP_THUMBNAIL_RESET);
      gfx_thumbnail_reset(&ozone->thumbnails.savestate);
      ozone_update_savestate_thumbnail_path(ozone, (unsigned)menu_st->selection_ptr);
      ozone_update_savestate_thumbnail_image(ozone);
   }

   settings                           = config_get_ptr();
   if (MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      menu_st->flags                 &=  ~MENU_ST_FLAG_PREVENT_POPULATE;
   else
      menu_st->flags                 |=   MENU_ST_FLAG_PREVENT_POPULATE;

   if (ozone->depth == 1)
   {
      ozone->flags         |= OZONE_FLAG_DRAW_SIDEBAR;
      ozone->sidebar_offset = 0.0f;
   }

   ozone_sidebar_update_collapse(ozone, settings->bools.ozone_collapse_sidebar, false);
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   settings_t *settings         = config_get_ptr();
   struct menu_state *menu_st   = menu_state_get_ptr();
   menu_list_t *menu_list       = menu_st->entries.list;
   file_list_t *menu_stack      = MENU_LIST_GET(menu_list, 0);
   file_list_t *selection_buf   = MENU_LIST_GET_SELECTION(menu_list, 0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    = strldup("lpl", sizeof("lpl"));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append(menu_stack,
         info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type,
         info.flags,
         0,
         NULL);

   info.list                    = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info, settings))
      goto error;

   info.flags |= MD_FLAG_NEED_PUSH;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static void ozone_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone_node_t *node    = NULL;
   int i                 = (int)list_size;

   if (!ozone || !list)
      return;

   ozone->flags         |= OZONE_FLAG_NEED_COMPUTE;

   if (!(node = (ozone_node_t*)list->list[i].userdata))
   {
      if (!(node = ozone_alloc_node()))
         return;
   }

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath      = strdup(fullpath);
   }

   list->list[i].userdata = node;
}

static int ozone_environ_cb(enum menu_environ_cb type, void *data, void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         ozone->flags |=  OZONE_FLAG_SHOW_CURSOR;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         ozone->flags &= ~OZONE_FLAG_SHOW_CURSOR;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!ozone)
            return -1;
         ozone_refresh_horizontal_list(ozone, config_get_ptr());
         break;
      case MENU_ENVIRON_ENABLE_SCREENSAVER:
         ozone->flags |=  OZONE_FLAG_SHOW_SCREENSAVER;
         break;
      case MENU_ENVIRON_DISABLE_SCREENSAVER:
         ozone->flags &= ~OZONE_FLAG_SHOW_SCREENSAVER;
         break;
      default:
         return -1;
   }

   return 0;
}

static void ozone_messagebox(void *data, const char *message)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone || string_is_empty(message))
      return;

   if (ozone->pending_message)
   {
      free(ozone->pending_message);
      ozone->pending_message     = NULL;
   }

   ozone->pending_message        = strdup(message);
   ozone->flags                 |= OZONE_FLAG_SHOULD_DRAW_MSGBOX
                                 | OZONE_FLAG_MSGBOX_STATE;
}

static int ozone_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   settings_t *settings         = config_get_ptr();
   if (!menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info, settings))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int ozone_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = ozone_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int ozone_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label,
      unsigned type,
      size_t idx)
{
   if (ozone_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int ozone_pointer_up(void *userdata,
      unsigned x,
      unsigned y,
      unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry,
      unsigned action)
{
   unsigned int width, height;
   ozone_handle_t *ozone             = (ozone_handle_t*)userdata;
   struct menu_state *menu_st        = menu_state_get_ptr();
   menu_input_t *menu_input          = &menu_st->input_state;
   menu_list_t *menu_list            = menu_st->entries.list;
   file_list_t *selection_buf        = MENU_LIST_GET_SELECTION(menu_list, 0);
   uintptr_t sidebar_tag             = (uintptr_t)selection_buf;
   size_t selection                  = menu_st->selection_ptr;
   size_t entries_end                = MENU_LIST_GET_SELECTION(menu_list, 0)->size;
   settings_t *settings              = config_get_ptr();
   bool ozone_collapse_sidebar       = settings->bools.ozone_collapse_sidebar;

   if (!ozone)
      return -1;

   /* If fullscreen thumbnail view is enabled,
    * all input will disable it and otherwise
    * be ignored */
   if (ozone->flags2 & OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)
   {
      /* Must reset scroll acceleration, in case
       * user performed a swipe (don't want menu
       * list to 'drift' after hiding fullscreen
       * thumbnails...) */
      menu_input->pointer.y_accel    = 0.0f;

      ozone_hide_fullscreen_thumbnails(ozone, true);
      return 0;
   }

   video_driver_get_size(&width, &height);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         /* Tap/press header or footer: Menu back/cancel */
         if (((int)y < ozone->dimensions.header_height) ||
             ((int)y > (int)height - ozone->dimensions.footer_height))
            return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_CANCEL);
         /* Tap/press entries: Activate and/or select item */
         else if ((ptr < entries_end)
               && ((int)x > (int)(ozone->dimensions_sidebar_width + ozone->sidebar_offset))
               && ((int)x < (int)((float)width - ozone->animations.thumbnail_bar_position)))
         {
            if (gesture == MENU_INPUT_GESTURE_TAP)
            {
               /* A 'tap' always produces a menu action */

               /* If current 'pointer' item is not active,
                * activate it immediately */
               if (ptr != selection)
                  menu_st->selection_ptr = ptr;

               /* If we are not currently in the sidebar,
                * perform a MENU_ACTION_SELECT on currently
                * active item
                * > NOTE 1: Cannot perform a 'leave sidebar' operation
                *   and a MENU_ACTION_SELECT at the same time...
                * > NOTE 2: We still use 'selection' (i.e. old selection
                *   value) here. This ensures that ozone_menu_entry_action()
                *   registers any change due to the above automatic
                *   'pointer item' activation, and thus operates
                *   on the correct target entry */
               if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
                  return ozone_menu_entry_action(ozone, entry,
                        selection, MENU_ACTION_SELECT);

               /* If we currently in the sidebar, leave it */
               if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
                  ozone_leave_sidebar(ozone, ozone_collapse_sidebar, sidebar_tag);
            }
            else
            {
               /* A 'short' press is used only to activate (highlight)
                * an item - it does not invoke a MENU_ACTION_SELECT
                * action */
               menu_input->pointer.y_accel    = 0.0f;

               if (ptr != selection)
                  menu_st->selection_ptr = ptr;

               /* If we are currently in the sidebar, leave it */
               if (ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR)
               {
                  if (!(ozone->flags & OZONE_FLAG_EMPTY_PLAYLIST))
                     ozone_leave_sidebar(ozone, ozone_collapse_sidebar, sidebar_tag);
               }
               /* If this is a playlist and the selection
                * has changed, must update thumbnails */
               else if (   (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
                        && (ozone->depth == 1)
                        && (ptr != selection))
               {
                  ozone_set_thumbnail_content(ozone, "");
                  ozone_update_thumbnail_image(ozone);
               }
            }
         }
         /* Tap/press thumbnail bar: toggle content metadata
          * override */
         else if (x > width - ozone->animations.thumbnail_bar_position)
         {
            /* Want to capture all input here, but only act
             * upon it if the content metadata toggle is
             * available (i.e. viewing a playlist with dual
             * thumbnails) */
            if (ozone_metadata_override_available(ozone, settings))
               return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_INFO);
         }
         /* Tap/press sidebar: return to sidebar or select
          * category */
         else if (ozone->flags2 & OZONE_FLAG2_POINTER_IN_SIDEBAR)
         {
            /* If cursor is not in sidebar, return to sidebar */
            if (!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR))
               ozone_go_to_sidebar(ozone, ozone_collapse_sidebar, sidebar_tag);
            /* Otherwise, select current category */
            else if (
                     ozone->pointer_categories_selection
                  != ozone->categories_selection_ptr)
            {
               unsigned horizontal_list_size = (unsigned)ozone->horizontal_list.size;

               /* Ensure that current category is valid */
               if (ozone->pointer_categories_selection <= ozone->system_tab_end + horizontal_list_size)
                  ozone_sidebar_goto(ozone, (unsigned)ozone->pointer_categories_selection);
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if (     ((int)y > ozone->dimensions.header_height)
               && ((int)y < (int)(height - ozone->dimensions.footer_height))
               && (ptr < entries_end)
               && (ptr == selection)
               && ((int)x > ozone->dimensions_sidebar_width + ozone->sidebar_offset)
               && ((int)x < width - ozone->animations.thumbnail_bar_position))
            return ozone_menu_entry_action(ozone,
                  entry, selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         /* If this is a playlist, descend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if (     (ozone->pointer.type != MENU_POINTER_MOUSE)
               && (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
               && (ozone->depth == 1))
            return ozone_menu_entry_action(ozone, entry, (size_t)ptr, MENU_ACTION_SCROLL_UP);
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         /* If this is a playlist, ascend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if (     (ozone->pointer.type != MENU_POINTER_MOUSE)
               && (ozone->flags & OZONE_FLAG_IS_PLAYLIST)
               && (ozone->depth == 1))
            return ozone_menu_entry_action(ozone, entry, (size_t)ptr, MENU_ACTION_SCROLL_DOWN);
         break;
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_ozone = {
   NULL,                         /* set_texture */
   ozone_messagebox,
   ozone_render,
   ozone_frame,
   ozone_init,
   ozone_free,
   ozone_context_reset,
   ozone_context_destroy,
   ozone_populate_entries,
   ozone_toggle,
   ozone_navigation_clear,
   NULL,
   NULL,
   ozone_navigation_set,
   ozone_navigation_pointer_changed,
   ozone_navigation_alphabet,
   ozone_navigation_alphabet,
   ozone_menu_init_list,
   ozone_list_insert,
   NULL,                         /* list_prepend */
   ozone_list_free,
   ozone_list_clear,
   ozone_list_cache,
   ozone_list_push,
   ozone_list_get_selection,
   ozone_list_get_size,
   ozone_list_get_entry,
   NULL,                         /* list_set_selection */
   ozone_list_bind_init,
   NULL,
   "ozone",
   ozone_environ_cb,
   NULL,
   ozone_update_thumbnail_image,
   ozone_refresh_thumbnail_image,
   ozone_set_thumbnail_content,
   gfx_display_osk_ptr_at_pos,
   ozone_update_savestate_thumbnail_path,
   ozone_update_savestate_thumbnail_image,
   NULL,                         /* pointer_down */
   ozone_pointer_up,
   ozone_menu_entry_action
};
