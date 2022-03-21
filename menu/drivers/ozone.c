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

#if 0
#include "../../discord/discord.h"
#endif

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
#include "../../content.h"
#include "../../core_info.h"
#include "../../verbosity.h"

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

#define OZONE_WIGGLE_DURATION 15

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
#if defined(HAVE_LIBRETRODB)
   OZONE_SYSTEM_TAB_EXPLORE,
#endif
   OZONE_SYSTEM_TAB_CONTENTLESS_CORES,

   /* End of this enum - use the last one to determine num of possible tabs */
   OZONE_SYSTEM_TAB_LAST
};

enum OZONE_TEXTURE
{
   OZONE_TEXTURE_RETROARCH = 0,
   OZONE_TEXTURE_CURSOR_BORDER,
#if 0
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
   OZONE_TAB_TEXTURE_EXPLORE,
   OZONE_TAB_TEXTURE_CONTENTLESS_CORES,

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

   /*
      Fancy cursor colors
   */
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
   unsigned sublabel_lines;   /* Entry sublabel lines */
   bool wrap;                 /* Wrap entry? */
} ozone_node_t;

struct ozone_handle
{
   menu_input_pointer_t pointer; /* retro_time_t alignment */

   ozone_theme_t *theme;
   gfx_thumbnail_path_data_t *thumbnail_path_data;
   char *pending_message;
   file_list_t selection_buf_old;                  /* ptr alignment */
   file_list_t horizontal_list; /* console tabs */ /* ptr alignment */
   /* Maps console tabs to playlist database names */
   ozone_node_t **playlist_db_node_map;
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
   bool pending_hide_thumbnail_bar;
   bool fullscreen_thumbnails_available;
   bool show_fullscreen_thumbnails;
   bool selection_core_is_viewer;

   bool force_metadata_display;

   bool is_db_manager_list;
   bool is_file_list;
   bool is_quick_menu;
   bool is_contentless_cores;
   bool first_frame;

   struct
   {
      retro_time_t start_time;
      float amplitude;
      enum menu_action direction;
      bool wiggling;
   } cursor_wiggle_state;
};

typedef struct ozone_handle ozone_handle_t;

static const char *OZONE_TEXTURES_FILES[OZONE_TEXTURE_LAST] = {
   "retroarch",
   "cursor_border"
};

static const char *OZONE_TAB_TEXTURES_FILES[OZONE_TAB_TEXTURE_LAST] = {
   "retroarch", /* MAIN_MENU */
   "settings",  /* SETTINGS_TAB */
   "history",   /* HISTORY_TAB */
   "favorites", /* FAVORITES_TAB */
   "music",     /* MUSIC_TAB */
   "video",     /* VIDEO_TAB */
   "image",     /* IMAGES_TAB */
   "netplay",   /* NETPLAY_TAB */
   "add",       /* ADD_TAB */
   "retroarch", /* EXPLORE_TAB */
   "retroarch"  /* CONTENTLESS_CORES_TAB */
};

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
#ifdef HAVE_LIBRETRODB
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
#endif
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB
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
#ifdef HAVE_LIBRETRODB
   MENU_EXPLORE_TAB,
#endif
   MENU_CONTENTLESS_CORES_TAB
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
   MENU_ENUM_LABEL_ADD_TAB,
#ifdef HAVE_LIBRETRODB
   MENU_ENUM_LABEL_EXPLORE_TAB,
#endif
   MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB
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
   OZONE_TAB_TEXTURE_SCAN_CONTENT,
#ifdef HAVE_LIBRETRODB
   OZONE_TAB_TEXTURE_EXPLORE,
#endif
   OZONE_TAB_TEXTURE_CONTENTLESS_CORES
};

static const char *OZONE_THEME_TEXTURES_FILES[OZONE_THEME_TEXTURE_LAST] = {
   "switch",
   "check",

   "cursor_noborder",
   "cursor_static"
};

static float ozone_sidebar_gradient_top_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
};

static float ozone_sidebar_gradient_bottom_light[16] = {
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_gradient_top_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
};

static float ozone_sidebar_gradient_bottom_dark[16] = {
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_sidebar_gradient_top_nord[16] = {
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_nord[16] = {
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_gradient_top_gruvbox_dark[16] = {
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_gruvbox_dark[16] = {
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

static float ozone_sidebar_background_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_background_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_sidebar_background_nord[16] = {
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_background_gruvbox_dark[16] = {
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

static float ozone_background_libretro_running_light[16] = {
   0.690, 0.690, 0.690, 0.75,
   0.690, 0.690, 0.690, 0.75,
   0.922, 0.922, 0.922, 1.0,
   0.922, 0.922, 0.922, 1.0
};

static float ozone_background_libretro_running_dark[16] = {
   0.176, 0.176, 0.176, 0.75,
   0.176, 0.176, 0.176, 0.75,
   0.178, 0.178, 0.178, 1.0,
   0.178, 0.178, 0.178, 1.0,
};

static float ozone_background_libretro_running_nord[16] = {
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
};

static float ozone_background_libretro_running_gruvbox_dark[16] = {
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
};

static float ozone_background_libretro_running_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
};

static float ozone_sidebar_background_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
};

static float ozone_sidebar_gradient_top_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
};

static float ozone_sidebar_gradient_bottom_boysenberry[16] = {
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,      
};

static float ozone_background_libretro_running_hacking_the_kernel[16] = {
      0.0, 0.0666666f, 0.0, 0.75f,
      0.0, 0.0666666f, 0.0, 0.75f,
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.0666666f, 0.0, 1.0f,
};

static float ozone_sidebar_background_hacking_the_kernel[16] = {
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
};

static float ozone_sidebar_gradient_top_hacking_the_kernel[16] = {
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
};

static float ozone_sidebar_gradient_bottom_hacking_the_kernel[16] = {
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
};

static float ozone_background_libretro_running_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 0.75f,
      0.0078431, 0.0, 0.0156862, 0.75f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_background_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_gradient_top_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_gradient_bottom_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_background_libretro_running_dracula[16] = {
      0.1568627, 0.1647058, 0.2117647, 0.75f,
      0.1568627, 0.1647058, 0.2117647, 0.75f,
      0.1568627, 0.1647058, 0.2117647, 1.0f,
      0.1568627, 0.1647058, 0.2117647, 1.0f,
};

static float ozone_sidebar_background_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
};

static float ozone_sidebar_gradient_top_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
};

static float ozone_sidebar_gradient_bottom_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f, 
};

static float ozone_background_libretro_running_solarized_dark[16] = {
      0.0000000, 0.1294118, 0.1725490, .85f,
      0.0000000, 0.1294118, 0.1725490, .85f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
};

static float ozone_sidebar_background_solarized_dark[16] = {
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
};

static float ozone_sidebar_gradient_top_solarized_dark[16] = {
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
};

static float ozone_sidebar_gradient_bottom_solarized_dark[16] = {
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
      0.0000000, 0.1294118, 0.1725490, 1.0f,
};

static float ozone_background_libretro_running_solarized_light[16] = {
      1.0000000, 1.0000000, 0.9294118, 0.85f,
      1.0000000, 1.0000000, 0.9294118, 0.85f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
};
static float ozone_sidebar_background_solarized_light[16] = {
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
};

static float ozone_sidebar_gradient_top_solarized_light[16] = {
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
};
static float ozone_sidebar_gradient_bottom_solarized_light[16] = {
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
      1.0000000, 1.0000000, 0.9294118, 1.0f,
};
static float ozone_border_0_light[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_light[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_dark[16] = COLOR_HEX_TO_FLOAT(0x198AC6, 1.00);
static float ozone_border_1_dark[16] = COLOR_HEX_TO_FLOAT(0x89F1F2, 1.00);

static float ozone_border_0_nord[16] = COLOR_HEX_TO_FLOAT(0x5E81AC, 1.0f);
static float ozone_border_1_nord[16] = COLOR_HEX_TO_FLOAT(0x88C0D0, 1.0f);

static float ozone_border_0_gruvbox_dark[16] = COLOR_HEX_TO_FLOAT(0xAF3A03, 1.0f);
static float ozone_border_1_gruvbox_dark[16] = COLOR_HEX_TO_FLOAT(0xFE8019, 1.0f);

static float ozone_border_0_boysenberry[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_boysenberry[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x008C00, 1.0f);
static float ozone_border_1_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x00E000, 1.0f);

static float ozone_border_0_twilight_zone[16] = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_twilight_zone[16] = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);

static float ozone_border_0_dracula[16] = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_dracula[16] = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);

static float ozone_border_0_solarized_dark[16] = COLOR_HEX_TO_FLOAT(0x67ECE2, 1.0f);
static float ozone_border_1_solarized_dark[16] = COLOR_HEX_TO_FLOAT(0x2AA198, 1.0f);

static float ozone_border_0_solarized_light[16] = COLOR_HEX_TO_FLOAT(0x8F120F, 1.0f);
static float ozone_border_1_solarized_light[16] = COLOR_HEX_TO_FLOAT(0xDC322F, 1.0f);

ozone_theme_t ozone_theme_light = {
   COLOR_HEX_TO_FLOAT(0xEBEBEB, 1.00),                   /* background */
   ozone_background_libretro_running_light,              /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0x2B2B2B, 1.00),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),                   /* text */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x10BEC5, 1.00),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xCDCDCD, 1.00),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x374CFF, 1.00),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0xF0F0F0, 1.00),                   /* message_background */

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

ozone_theme_t ozone_theme_dark = {
   COLOR_HEX_TO_FLOAT(0x2D2D2D, 1.00),                   /* background */
   ozone_background_libretro_running_dark,               /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* text */
   COLOR_HEX_TO_FLOAT(0x212227, 1.00),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x2DA3CB, 1.00),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x51514F, 1.00),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x00D9AE, 1.00),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x464646, 1.00),                   /* message_background */

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

ozone_theme_t ozone_theme_nord = {
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

ozone_theme_t ozone_theme_gruvbox_dark = {
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

ozone_theme_t ozone_theme_boysenberry = {
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

ozone_theme_t ozone_theme_hacking_the_kernel = {
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

ozone_theme_t ozone_theme_twilight_zone = {
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

ozone_theme_t ozone_theme_dracula = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282A36, 1.0f),                   /* background */
   ozone_background_libretro_running_dracula,            /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xBD93F9, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x6272A4, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xF8F8F2FF,                                           /* text_rgba */
   0xF8F8F2FF,                                           /* text_sidebar_rgba */
   0xFF79C6FF,                                           /* text_selected_rgba */
   0xBD93F9FF,                                           /* text_sublabel_rgba */

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

ozone_theme_t ozone_theme_solarized_dark = {
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

ozone_theme_t ozone_theme_solarized_light = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0xFDF6E3, 1.0f),                   /* background */
   ozone_background_libretro_running_solarized_light,     /* background_libretro_running */

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
   ozone_sidebar_background_solarized_light,              /* sidebar_background */
   ozone_sidebar_gradient_top_solarized_light,            /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_solarized_light,         /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_solarized_light,                        /* cursor_border_0 */
   ozone_border_1_solarized_light,                        /* cursor_border_1 */

   {0},                                                  /* textures */

   "solarized_light"                                      /* name */
};

static float ozone_background_libretro_running_gray[16] = COLOR_HEX_TO_FLOAT(0x101010, 1.0f);
static float ozone_sidebar_background_gray[16]          = COLOR_HEX_TO_FLOAT(0x101010, 0.0f);
static float ozone_border_gray[16]                      = COLOR_HEX_TO_FLOAT(0x303030, 1.0f);

ozone_theme_t ozone_theme_gray_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x101010, 1.0f),                   /* background */
   ozone_background_libretro_running_gray,               /* background_libretro_running */

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
   ozone_sidebar_background_gray,                        /* sidebar_background */
   ozone_sidebar_background_gray,                        /* sidebar_top_gradient */
   ozone_sidebar_background_gray,                        /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_gray,                                    /* cursor_border_0 */
   ozone_border_gray,                                    /* cursor_border_1 */

   {0},                                                  /* textures */

   /* No theme assets */
   NULL,                                                 /* name */
};

ozone_theme_t ozone_theme_gray_light = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x303030, 1.0f),                   /* background */
   ozone_background_libretro_running_gray,               /* background_libretro_running */

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
   ozone_sidebar_background_gray,                        /* sidebar_background */
   ozone_sidebar_background_gray,                        /* sidebar_top_gradient */
   ozone_sidebar_background_gray,                        /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_gray,                                    /* cursor_border_0 */
   ozone_border_gray,                                    /* cursor_border_1 */

   {0},                                                  /* textures */

   /* No theme assets */
   NULL,                                                 /* name */
};

ozone_theme_t *ozone_themes[] = {
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
   &ozone_theme_gray_light
};

static const unsigned ozone_themes_count    = sizeof(ozone_themes) / sizeof(ozone_themes[0]);
/* TODO/FIXME - global variables referenced outside */
static unsigned ozone_last_color_theme      = 0;
static bool 
ozone_last_use_preferred_system_color_theme = false;
static ozone_theme_t *ozone_default_theme   = &ozone_theme_dark; /* also used as a tag for cursor animation */
/* Enable runtime configuration of framebuffer
 * opacity */
static float ozone_last_framebuffer_opacity = -1.0f;

/* Forward declarations */
static void ozone_cursor_animation_cb(void *userdata);

static INLINE unsigned ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static void ozone_animate_cursor(ozone_handle_t *ozone,
      float *dst, float *target)
{
   int i;
   gfx_animation_ctx_entry_t entry;

   entry.easing_enum = EASING_OUT_QUAD;
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

   ozone->theme_dynamic_cursor_state = 
      (ozone->theme_dynamic_cursor_state + 1) % 2;

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, target);
}

static void ozone_restart_cursor_animation(ozone_handle_t *ozone)
{
   uintptr_t tag = (uintptr_t)&ozone_default_theme;

   if (!ozone->has_all_assets)
      return;

   ozone->theme_dynamic_cursor_state = 1;
   memcpy(ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_0,
         sizeof(ozone->theme_dynamic.cursor_border));
   gfx_animation_kill_by_tag(&tag);

   ozone_animate_cursor(ozone,
         ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_1);
}

static void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme)
{
   ozone_theme_t *theme = ozone_default_theme;

   if (!ozone)
      return;

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
      default:
         break;
   }

   ozone->theme = theme;

   memcpy(ozone->theme_dynamic.selection_border, ozone->theme->selection_border, sizeof(ozone->theme_dynamic.selection_border));
   memcpy(ozone->theme_dynamic.selection, ozone->theme->selection, sizeof(ozone->theme_dynamic.selection));
   memcpy(ozone->theme_dynamic.entries_border, ozone->theme->entries_border, sizeof(ozone->theme_dynamic.entries_border));
   memcpy(ozone->theme_dynamic.entries_icon, ozone->theme->entries_icon, sizeof(ozone->theme_dynamic.entries_icon));
   memcpy(ozone->theme_dynamic.entries_checkmark, ozone->pure_white, sizeof(ozone->theme_dynamic.entries_checkmark));
   memcpy(ozone->theme_dynamic.cursor_alpha, ozone->pure_white, sizeof(ozone->theme_dynamic.cursor_alpha));
   memcpy(ozone->theme_dynamic.message_background, ozone->theme->message_background, sizeof(ozone->theme_dynamic.message_background));

   ozone_restart_cursor_animation(ozone);

   ozone_last_color_theme = color_theme;
}

static unsigned ozone_get_system_theme(void)
{
#ifdef HAVE_LIBNX
   unsigned ret = 0;
   if (R_SUCCEEDED(setsysInitialize()))
   {
      ColorSetId theme;
      setsysGetColorSetId(&theme);
      ret = (theme == ColorSetId_Dark) ? 1 : 0;
      setsysExit();
   }

   return ret;
#else
   return DEFAULT_OZONE_COLOR_THEME;
#endif
}

static void ozone_set_background_running_opacity(
      ozone_handle_t *ozone, float framebuffer_opacity)
{
   static float background_running_alpha_top    = 1.0f;
   static float background_running_alpha_bottom = 0.75f;
   float *background                            = NULL;

   if (!ozone || !ozone->theme->background_libretro_running)
      return;

   background                      = 
      ozone->theme->background_libretro_running;

   /* When content is running, background is a
    * gradient that from top to bottom transitions
    * from maximum to minimum opacity
    * > RetroArch default 'framebuffer_opacity'
    *   is 0.900. At this setting:
    *   - Background top has an alpha of 1.0
    *   - Background bottom has an alpha of 0.75 */
   background_running_alpha_top    = framebuffer_opacity / 0.9f;
   background_running_alpha_top    = (background_running_alpha_top > 1.0f) ?
         1.0f : (background_running_alpha_top < 0.0f) ?
               0.0f : background_running_alpha_top;

   background_running_alpha_bottom = (2.5f * framebuffer_opacity) - 1.5f;
   background_running_alpha_bottom = (background_running_alpha_bottom > 1.0f) ?
         1.0f : (background_running_alpha_bottom < 0.0f) ?
               0.0f : background_running_alpha_bottom;

   background[11]                  = background_running_alpha_top;
   background[15]                  = background_running_alpha_top;
   background[3]                   = background_running_alpha_bottom;
   background[7]                   = background_running_alpha_bottom;

   ozone_last_framebuffer_opacity  = framebuffer_opacity;
}

static uintptr_t ozone_entries_icon_get_texture(ozone_handle_t *ozone,
      enum msg_hash_enums enum_idx, const char *enum_path,
      const char *enum_label, unsigned type, bool active)
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
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_TRAY_EJECT:
      case MENU_ENUM_LABEL_DISK_TRAY_INSERT:
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
      case MENU_ENUM_LABEL_DISK_INDEX:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_STATE_SLOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_SAVE_STATE:
      case MENU_ENUM_LABEL_CORE_CREATE_BACKUP:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
      case MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];

      /* Menu collection submenus*/
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ZIP];
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE];
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MUSIC];
      case MENU_ENUM_LABEL_GOTO_EXPLORE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];

      /* Menu icons */
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES:
      case MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_SET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_CORE_INFORMATION:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_FILE:
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
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
#ifdef HAVE_VIDEO_LAYOUT
      case MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS:
#endif
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY];
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
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INFO];
      case MENU_ENUM_LABEL_EXPLORE_TAB:
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
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
      case MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_EXIT];
      /* Settings icons*/
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS];
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_VIDEO];
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_AUDIO];
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MIXER];
      case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING];
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
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
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP];
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RECORD];
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_STREAM];
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
      case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
      case MENU_ENUM_LABEL_CORE_DELETE:
      case MENU_ENUM_LABEL_DELETE_PLAYLIST:
      case MENU_ENUM_LABEL_CORE_DELETE_BACKUP_LIST:
      case MENU_ENUM_LABEL_VIDEO_FILTER_REMOVE:
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN_REMOVE:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE:
      case MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_CORE_OPTIONS_RESET:
      case MENU_ENUM_LABEL_REMAP_FILE_RESET:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case MENU_ENUM_LABEL_CORE_LOCK:
      case MENU_ENUM_LABEL_CORE_SET_STANDALONE_EXEMPT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UI];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
      case MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NETWORK];
      case MENU_ENUM_LABEL_BLUETOOTH_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_BLUETOOTH];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_USER];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY];

      case MENU_ENUM_LABEL_REWIND_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_REWIND];
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE];
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
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
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
      case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_SHUTDOWN:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN];
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
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
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG];
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
#ifdef HAVE_LIBRETRODB
      case MENU_ENUM_LABEL_EXPLORE_ITEM:
      {
         uintptr_t icon = menu_explore_get_entry_icon(type);
         if (icon) return icon;
         break;
      }
#endif
      case MENU_ENUM_LABEL_CONTENTLESS_CORE:
      {
         uintptr_t icon = menu_contentless_cores_get_entry_icon(enum_label);
         if (icon) return icon;
         break;
      }
      default:
            break;
   }

   switch(type)
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
      case FILE_TYPE_RDB:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
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
      case FILE_TYPE_RDB_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_VIDEO];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_AUDIO];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS)))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
         else if (string_is_equal(enum_path, "Media"))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
         else if (string_is_equal(enum_path, "System"))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS];
         else
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
                  if (input_config_bind_order[index] == input_num)
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
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R];
         if (type == (input_id + 5))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D];
         if (type == (input_id + 6))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U];
         if (type == (input_id + 7))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L];
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

   return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING];
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

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];
      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
            video_driver_texture_unload(&theme->textures[i]);
   }
}

static bool ozone_reset_theme_textures(ozone_handle_t *ozone)
{
   unsigned i, j;
   char theme_path[255];
   bool result = true;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];

      if (!theme->name)
         continue;

      fill_pathname_join(
         theme_path,
         ozone->png_path,
         theme->name,
         sizeof(theme_path)
      );

      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_THEME_TEXTURES_FILES[i],
               sizeof(filename));
         strlcat(filename, FILE_PATH_PNG_EXTENSION, sizeof(filename));

         if (!gfx_display_reset_textures_list(filename, theme_path, &theme->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
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
   int entries = (int)(ozone->system_tab_end + 1 + (ozone->horizontal_list.size ));
   return entries * ozone->dimensions.sidebar_entry_height + (entries - 1) * ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.sidebar_padding_vertical +
      (ozone->horizontal_list.size > 0 ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px : 0);
}

static unsigned ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone)
{
   return ozone->categories_selection_ptr * ozone->dimensions.sidebar_entry_height +
         (ozone->categories_selection_ptr - 1) * ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.sidebar_padding_vertical +
         (ozone->categories_selection_ptr > ozone->system_tab_end ? ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px : 0);
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

static void ozone_font_flush(
      unsigned video_width, unsigned video_height,
      ozone_font_data_t *font_data)
{
   /* Flushing is slow - only do it if font
    * has actually been used */
   if (!font_data ||
       (font_data->raster_block.carr.coords.vertices == 0))
      return;

   font_driver_flush(video_width, video_height, font_data->font);
   font_data->raster_block.carr.coords.vertices = 0;
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
      *dispctx              = p_disp->dispctx;


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

/**
 * Changes x and y to the current offset of the cursor wiggle animation
 */
static void ozone_apply_cursor_wiggle_offset(ozone_handle_t* ozone, int* x, size_t* y)
{
   retro_time_t cur_time;
   retro_time_t t;

   /* Don't do anything if we are not wiggling */
   if (!ozone || !ozone->cursor_wiggle_state.wiggling)
      return;

   cur_time = menu_driver_get_current_time() / 1000;
   t        = (cur_time - ozone->cursor_wiggle_state.start_time) / 10;

   /* Has the animation ended? */
   if (t >= OZONE_WIGGLE_DURATION)
   {
      ozone->cursor_wiggle_state.wiggling = false;
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
      unsigned width, unsigned height,
      size_t y, float alpha,
      math_matrix_4x4 *mymat)
{
   float scale_factor    = ozone->last_scale_factor;
   int slice_x           = x_offset - 12 * scale_factor;
   int slice_y           = (int)y + 8 * scale_factor;
   unsigned slice_new_w  = width + (24 + 1) * scale_factor;
   unsigned slice_new_h  = height + 20 * scale_factor;
   gfx_display_ctx_driver_t 
      *dispctx           = p_disp->dispctx;
   static float 
      last_alpha         = 0.0f;

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
         80, 80,
         slice_new_w,
         slice_new_h,
         video_width, video_height,
         ozone->theme_dynamic.cursor_alpha,
         20, scale_factor,
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
         80, 80,
         slice_new_w,
         slice_new_h,
         video_width, video_height,
         ozone->theme_dynamic.cursor_border,
         20, scale_factor,
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
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   static float last_alpha           = 0.0f;

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
         y,
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
         y - ozone->dimensions.spacer_3px,
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
         y + height,
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
         y,
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
         y,
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
      unsigned width, unsigned height,
      size_t y, float alpha,
      math_matrix_4x4 *mymat)
{
   int new_x    = x_offset;
   size_t new_y = y;

   /* Apply wiggle animation if needed */
   if (ozone->cursor_wiggle_state.wiggling)
      ozone_apply_cursor_wiggle_offset(ozone, &new_x, &new_y);

   /* Draw the cursor */
   if (ozone->theme->name && ozone->has_all_assets)
      ozone_draw_cursor_slice(ozone, 
            p_disp,
            userdata,
            video_width, video_height,
            new_x, width, height, new_y, alpha,
            mymat);
   else
      ozone_draw_cursor_fallback(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            new_x, width, height, new_y, alpha);
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
      math_matrix_4x4 *mymat
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

   if (p_disp->dispctx && p_disp->dispctx->scissor_begin)
      gfx_display_scissor_begin(
            p_disp,
            userdata,
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
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px + ozone->dimensions.sidebar_gradient_height,
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
            ozone->animations.cursor_alpha,
            mymat);

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
            1-ozone->animations.cursor_alpha,
            mymat);

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

      uint32_t text_color  = COLOR_TEXT_ALPHA((selected ? ozone->theme->text_selected_rgba : ozone->theme->text_sidebar_rgba), text_alpha);
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
            0.0f,
            1.0f,
            col,
            mymat);

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
         bool selected = (ozone->categories_selection_ptr == ozone->system_tab_end + 1 + i);

         uint32_t text_color  = COLOR_TEXT_ALPHA((selected ? ozone->theme->text_selected_rgba : ozone->theme->text_sidebar_rgba), text_alpha);

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

static void ozone_thumbnail_bar_hide_end(void *userdata)
{
   ozone_handle_t *ozone             = (ozone_handle_t*) userdata;
   ozone->show_thumbnail_bar         = false;
   ozone->pending_hide_thumbnail_bar = false;
}

static void ozone_entries_update_thumbnail_bar(
      ozone_handle_t *ozone, bool is_playlist, bool allow_animation)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t tag     = (uintptr_t)&ozone->show_thumbnail_bar;

   entry.duration    = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
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
    *   additional 'pending_hide_thumbnail_bar' parameter
    *   to track mid-animation state changes... */
   if (is_playlist &&
       !ozone->cursor_in_sidebar &&
       (!ozone->show_thumbnail_bar || ozone->pending_hide_thumbnail_bar) &&
       (ozone->depth == 1))
   {
      if (allow_animation)
      {
         ozone->show_thumbnail_bar = true;

         entry.cb                  = NULL;
         entry.userdata            = NULL;
         entry.target_value        = ozone->dimensions.thumbnail_bar_width;

         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = ozone->dimensions.thumbnail_bar_width;
         ozone->show_thumbnail_bar                = true;
      }

      ozone->pending_hide_thumbnail_bar = false;
   }
   /* Hide it */
   else
   {
      if (allow_animation)
      {
         entry.cb                          = ozone_thumbnail_bar_hide_end;
         entry.userdata                    = ozone;
         entry.target_value                = 0.0f;

         ozone->pending_hide_thumbnail_bar = true;
         gfx_animation_push(&entry);
      }
      else
      {
         ozone->animations.thumbnail_bar_position = 0.0f;
         ozone_thumbnail_bar_hide_end(ozone);
      }
   }
}

static bool ozone_is_playlist(ozone_handle_t *ozone, bool depth)
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


static void ozone_sidebar_update_collapse(
      ozone_handle_t *ozone,
      settings_t *settings,
      bool allow_animation)
{
   /* Collapse sidebar if needed */
   struct gfx_animation_ctx_entry entry;
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


static void ozone_go_to_sidebar(ozone_handle_t *ozone,
      settings_t *settings,
      uintptr_t tag)
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

   ozone_sidebar_update_collapse(ozone, settings, true);
}

static void ozone_update_content_metadata(ozone_handle_t *ozone)
{
   const char *core_name             = NULL;
   size_t selection                  = menu_navigation_get_selection();
   playlist_t *playlist              = playlist_get_cached();
   settings_t *settings              = config_get_ptr();
   bool scroll_content_metadata      = settings->bools.ozone_scroll_content_metadata;
   bool show_entry_idx               = settings->bools.playlist_show_entry_idx;
   bool content_runtime_log          = settings->bools.content_runtime_log;
   bool content_runtime_log_aggr     = settings->bools.content_runtime_log_aggregate;
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
   if (gfx_thumbnail_get_core_name(ozone->thumbnail_path_data, &core_name))
      ozone->selection_core_is_viewer = string_is_equal(core_name, "imageviewer")
            || string_is_equal(core_name, "musicplayer")
            || string_is_equal(core_name, "movieplayer");
   else
      ozone->selection_core_is_viewer = false;

   if (ozone->is_playlist && playlist)
   {
      const char *core_label             = NULL;
      const struct playlist_entry *entry = NULL;
      size_t list_size                   = menu_entries_get_size();
      file_list_t *list                  = menu_entries_get_selection_buf_ptr(0);
      bool playlist_valid                = false;
      size_t playlist_index              = selection;

      /* Get playlist index corresponding
       * to the selected entry */
      if (list &&
          (selection < list_size) &&
          (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
      {
         playlist_valid = true;
         playlist_index = list->list[selection].entry_idx;
      }

      /* Fill entry enumeration */
      if (show_entry_idx)
         snprintf(ozone->selection_entry_enumeration, sizeof(ozone->selection_entry_enumeration),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX),
            (unsigned long)(playlist_index + 1), (unsigned long)list_size);
      else
         ozone->selection_entry_enumeration[0] = '\0';

      /* Fill core name */
      if (!core_name || string_is_equal(core_name, "DETECT"))
         core_label = msg_hash_to_str(MSG_AUTODETECT);
      else
         core_label = core_name;

      snprintf(ozone->selection_core_name, sizeof(ozone->selection_core_name),
         "%s %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE), core_label);

      /* Word wrap core name string, if required */
      if (!scroll_content_metadata)
      {
         char tmpstr[sizeof(ozone->selection_core_name)];
         unsigned metadata_len =
               (ozone->dimensions.thumbnail_bar_width - ((ozone->dimensions.sidebar_entry_icon_padding * 2) * 2)) /
                     ozone->fonts.footer.glyph_width;

         strlcpy(tmpstr, ozone->selection_core_name, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_core_name, sizeof(ozone->selection_core_name),
               tmpstr, metadata_len, ozone->fonts.footer.wideglyph_width, 0);
         ozone->selection_core_name_lines = ozone_count_lines(ozone->selection_core_name);
      }
      else
         ozone->selection_core_name_lines = 1;

      /* Fill play time if applicable */
      if (playlist_valid &&
          (content_runtime_log || content_runtime_log_aggr))
         playlist_get_index(playlist, playlist_index, &entry);

      if (entry)
      {
         if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
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
         snprintf(ozone->selection_playtime, sizeof(ozone->selection_playtime), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));

         snprintf(ozone->selection_lastplayed, sizeof(ozone->selection_lastplayed), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));
      }

      /* Word wrap last played string, if required */
      if (!scroll_content_metadata)
      {
         /* Note: Have to use a fixed length of '30' here, to
          * avoid awkward wrapping for certain last played time
          * formats. Last played strings are well defined, however
          * (unlike core names), so this should never overflow the
          * side bar */
         char tmpstr[sizeof(ozone->selection_lastplayed)];

         strlcpy(tmpstr, ozone->selection_lastplayed, sizeof(tmpstr));
         (ozone->word_wrap)(ozone->selection_lastplayed, sizeof(ozone->selection_lastplayed), tmpstr, 30, 100, 0);
         ozone->selection_lastplayed_lines = ozone_count_lines(ozone->selection_lastplayed);
      }
      else
         ozone->selection_lastplayed_lines = 1;
   }
}


static void ozone_leave_sidebar(ozone_handle_t *ozone,
      settings_t *settings,
      uintptr_t tag)
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

   ozone_sidebar_update_collapse(ozone, settings, true);
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
      const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
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


static void ozone_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t y, entries_end;
   unsigned i;
   unsigned video_info_height;
   float bottom_boundary;
   ozone_node_t *first_node;
   float scale_factor;
   unsigned first             = 0;
   unsigned last              = 0;
   file_list_t *selection_buf = NULL;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   scale_factor               = ozone->last_scale_factor;
   ozone->need_compute        = true;
   ozone->selection_old_list  = ozone->selection;
   ozone->scroll_old          = ozone->animations.scroll_y;
   ozone->is_playlist_old     = ozone->is_playlist;

   /* Deep copy visible elements */
   video_driver_get_size(NULL, &video_info_height);
   y                          = ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical;
   entries_end                = menu_entries_get_size();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
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

static void ozone_change_tab(ozone_handle_t *ozone,
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

static void ozone_sidebar_goto(ozone_handle_t *ozone, unsigned new_selection)
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

static void ozone_refresh_sidebars(
      ozone_handle_t *ozone,
      settings_t *settings,
      unsigned video_height)
{
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
   ozone->pending_hide_thumbnail_bar           = false;

   /* If sidebar is on-screen, update scroll position */
   if (ozone->depth == 1)
   {
      ozone->animations.cursor_alpha     = 1.0f;
      ozone->animations.scroll_y_sidebar = ozone_sidebar_get_scroll_y(ozone, video_height);
   }
}

static size_t ozone_list_get_size(void *data, enum menu_list_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return 0;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         return ozone->horizontal_list.size;
      case MENU_LIST_TABS:
         return ozone->system_tab_end;
   }

   return 0;
}


static void ozone_init_horizontal_list(ozone_handle_t *ozone,
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
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_PLAYLISTS_TAB;

   if (menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL,
               &info, settings))
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

static void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone)
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

         /* Add current node to playlist database name map */
         RHMAP_SET_STR(ozone->playlist_db_node_map, path, node);

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

static void ozone_refresh_horizontal_list(ozone_handle_t *ozone,
      settings_t *settings)
{
   ozone_context_destroy_horizontal_list(ozone);
   ozone_free_list_nodes(&ozone->horizontal_list, false);
   file_list_deinitialize(&ozone->horizontal_list);
   RHMAP_FREE(ozone->playlist_db_node_map);

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   file_list_initialize(&ozone->horizontal_list);
   ozone_init_horizontal_list(ozone, settings);

   ozone_context_reset_horizontal_list(ozone);
}


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
      menu_entry_t *entry,
      math_matrix_4x4 *mymat)
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
static void ozone_update_scroll(ozone_handle_t *ozone,
      bool allow_animation, ozone_node_t *node)
{
   unsigned video_info_height;
   gfx_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   uintptr_t tag              = (uintptr_t) selection_buf;

   video_driver_get_size(NULL, &video_info_height);

   if (!node)
      return;

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

static void ozone_compute_entries_position(
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
         uintptr_t         tex = ozone_entries_icon_get_texture(ozone,
               entry.enum_idx, entry.path, entry.label, entry.type, false);
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
   if (!ozone->cursor_in_sidebar)
      ozone_draw_cursor(
            ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            ozone->dimensions_sidebar_width 
            + x_offset + entry_padding + ozone->dimensions.spacer_3px,
            entry_width - ozone->dimensions.spacer_5px,
            button_height + ozone->dimensions.spacer_1px,
            selection_y + scroll_y,
            ozone->animations.cursor_alpha * alpha,
            mymat);

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
            (1-ozone->animations.cursor_alpha) * alpha,
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
      entry.label_enabled            = ozone->is_contentless_cores;
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
      tex = ozone_entries_icon_get_texture(ozone,
            entry.enum_idx, entry.path, entry.label, entry.type, entry_selected);
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

                     if (pl_entry &&
                         !string_is_empty(pl_entry->db_name) &&
                         (db_node = RHMAP_GET_STR(ozone->playlist_db_node_map, pl_entry->db_name)))
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
                     ozone->dimensions_sidebar_width 
                     + x_offset + entry_padding 
                     + ozone->dimensions.entry_icon_padding,
                     y + scroll_y + ozone->dimensions.entry_height 
                     / 2 - ozone->dimensions.entry_icon_size / 2,
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
            &entry,
            mymat);

      y += node->height;
   }

   /* Text layer */
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);

   if (menu_show_sublabels)
      ozone_font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
}

static void ozone_draw_thumbnail_bar(
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
            ozone->theme->sidebar_top_gradient,
            NULL);
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
            ozone->theme->sidebar_background,
            NULL);
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
            ozone->theme->sidebar_bottom_gradient,
            NULL);
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
            x_position, sidebar_width, 0,
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
   static float last_alpha           = 0.0f;

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

static void ozone_draw_osk(ozone_handle_t *ozone,
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
      ozone->osk_cursor           = !ozone->osk_cursor;
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
         video_width - margin*2,
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
         video_width - margin*2,
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
         bottom_end - margin*2,
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
         bottom_end - margin*2,
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
         video_width - margin*2 - ozone->dimensions.spacer_2px,
         bottom_end - margin*2 - ozone->dimensions.spacer_2px,
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

   (ozone->word_wrap)(message, sizeof(message), text,
         (video_width - margin*2 - padding*2) / ozone->fonts.entries_label.glyph_width,
         ozone->fonts.entries_label.wideglyph_width, 0);

   string_list_initialize(&list);
   string_split_noalloc(&list, message, "\n");

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            msg,
            margin + padding * 2,       /* x */
            margin + padding + 
            ozone->fonts.entries_label.line_height 
            + y_offset,                /* y */
            video_width, video_height,
            text_color,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

      /* Cursor */
      if (i == list.size - 1)
      {
         if (ozone->osk_cursor)
         {
            unsigned cursor_x = draw_placeholder 
               ? 0 
               : font_driver_get_message_width(
                     ozone->fonts.entries_label.font, msg,
                     (unsigned)strlen(msg), 1);
            gfx_display_draw_quad(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                    margin 
                  + padding * 2 
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
   unsigned i, y_position;
   char wrapped_message[MENU_SUBLABEL_MAX_LENGTH];
   int x, y, longest_width  = 0;
   int usable_width         = 0;
   struct string_list list  = {0};
   float scale_factor       = 0.0f;
   unsigned width           = video_width;
   unsigned height          = video_height;
   gfx_display_ctx_driver_t 
      *dispctx              = p_disp->dispctx;

   wrapped_message[0]       = '\0';

   /* Sanity check */
   if (string_is_empty(message) ||
       !ozone->fonts.footer.font)
      return;

   scale_factor = ozone->last_scale_factor;
   usable_width = (int)width - (48 * 8 * scale_factor);

   if (usable_width < 1)
      return;

   /* Split message into lines */
   (ozone->word_wrap)(
         wrapped_message, sizeof(wrapped_message), message,
         usable_width / (int)ozone->fonts.footer.glyph_width,
         ozone->fonts.footer.wideglyph_width, 0);

   string_list_initialize(&list);
   if (
            !string_split_noalloc(&list, wrapped_message, "\n")
         || list.elems == 0)
   {
      string_list_deinitialize(&list);
      return;
   }

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list.size 
         * ozone->fonts.footer.line_height) / 2;

   /* find the longest line width */
   for (i = 0; i < list.size; i++)
   {
      const char *msg  = list.elems[i].data;

      if (!string_is_empty(msg))
      {
         int width = font_driver_get_message_width(
               ozone->fonts.footer.font, msg, (unsigned)strlen(msg), 1);

         if (width > longest_width)
            longest_width = width;
      }
   }

   gfx_display_set_alpha(ozone->theme_dynamic.message_background, ozone->animations.messagebox_alpha);

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Avoid drawing a black box if there's no assets */
   if (ozone->has_all_assets)
   {
      /* Note: The fact that we use a texture slice here
       * makes things very messy
       * > The actual size and offset of a texture slice
       *   is quite 'loose', and depends upon source image
       *   size, draw size and scale factor... */
      unsigned slice_new_w = longest_width + 48 * 2 * scale_factor;
      unsigned slice_new_h = ozone->fonts.footer.line_height * (list.size + 2);
      int slice_x          = x - longest_width/2 - 48 * scale_factor;
      int slice_y          = y - ozone->fonts.footer.line_height +
            ((slice_new_h >= 256) 
             ? (16.0f * scale_factor) 
             : (16.0f * ((float)slice_new_h / 256.0f)));

      gfx_display_draw_texture_slice(
            p_disp,
            userdata,
            video_width,
            video_height,
            slice_x,
            slice_y,
            256, 256,
            slice_new_w,
            slice_new_h,
            width, height,
            ozone->theme_dynamic.message_background,
            16, scale_factor,
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
               x - longest_width/2.0,
               y + (i * ozone->fonts.footer.line_height) + 
               ozone->fonts.footer.line_ascender,
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
      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
      animation_entry.target_value = 0.0f;
      animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      gfx_animation_push(&animation_entry);
   }
   /* No animation - just set thumbnail alpha to zero */
   else
      ozone->animations.fullscreen_thumbnail_alpha = 0.0f;

   /* Disable fullscreen thumbnails */
   ozone->show_fullscreen_thumbnails = false;
}

static void ozone_show_fullscreen_thumbnails(ozone_handle_t *ozone)
{
   menu_entry_t selected_entry;
   gfx_animation_ctx_entry_t animation_entry;
   const char *thumbnail_label        = NULL;
   file_list_t *selection_buf         = menu_entries_get_selection_buf_ptr(0);
   uintptr_t alpha_tag                = (uintptr_t)&ozone->animations.fullscreen_thumbnail_alpha;
   uintptr_t scroll_tag               = (uintptr_t)selection_buf;

   /* Before showing fullscreen thumbnails, must
    * ensure that any existing fullscreen thumbnail
    * view is disabled... */
   ozone_hide_fullscreen_thumbnails(ozone, false);

   /* Sanity check: Return immediately if this is
    * a menu without thumbnail support, or cursor
    * is currently in the sidebar */
   if (!ozone->fullscreen_thumbnails_available ||
       ozone->cursor_in_sidebar)
      return;

   /* We can only enable fullscreen thumbnails if
    * current selection has at least one valid thumbnail
    * and all thumbnails for current selection are already
    * loaded/available */
   if (ozone->selection_core_is_viewer)
   {
      /* imageviewer content requires special treatment,
       * since only the right thumbnail is ever loaded */
      if (!gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
         return;

      if (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_AVAILABLE)
         return;
   }
   else
   {
      bool left_thumbnail_enabled = gfx_thumbnail_is_enabled(
            ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT);

      if (!left_thumbnail_enabled &&
          !gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
         return;

      if ((ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE) &&
          (left_thumbnail_enabled &&
               ((ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING) &&
                (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE))))
         return;

      if ((ozone->thumbnails.right.status == GFX_THUMBNAIL_STATUS_MISSING) &&
          (!left_thumbnail_enabled ||
               (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE)))
         return;
   }

   /* Menu list must be stationary while fullscreen
    * thumbnails are shown
    * > Kill any existing scroll animations and
    *   reset scroll acceleration */
   gfx_animation_kill_by_tag(&scroll_tag);
   menu_input_set_pointer_y_accel(0.0f);

   /* Cache selected entry label
    * (used as title when fullscreen thumbnails
    * are shown) */
   ozone->fullscreen_thumbnail_label[0] = '\0';

   /* > Get menu entry */
   MENU_ENTRY_INIT(selected_entry);
   selected_entry.path_enabled     = false;
   selected_entry.value_enabled    = false;
   selected_entry.sublabel_enabled = false;
   menu_entry_get(&selected_entry, 0, (size_t)ozone->selection, NULL, true);

   /* > Get entry label */
   if (!string_is_empty(selected_entry.rich_label))
      thumbnail_label  = selected_entry.rich_label;
   else
      thumbnail_label  = selected_entry.path;

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      strlcpy(
            ozone->fullscreen_thumbnail_label,
            thumbnail_label,
            sizeof(ozone->fullscreen_thumbnail_label));

   /* Configure fade in animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &ozone->animations.fullscreen_thumbnail_alpha;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   gfx_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   ozone->fullscreen_thumbnail_selection = (size_t)ozone->selection;
   ozone->show_fullscreen_thumbnails     = true;
}



static void ozone_draw_fullscreen_thumbnails(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height)
{
   /* Check whether fullscreen thumbnails are visible */
   if (ozone->animations.fullscreen_thumbnail_alpha > 0.0f)
   {
      /* Note: right thumbnail is drawn at the top
       * in the sidebar, so it becomes the *left*
       * thumbnail when viewed fullscreen */
      gfx_thumbnail_t *right_thumbnail  = &ozone->thumbnails.left;
      gfx_thumbnail_t *left_thumbnail   = &ozone->thumbnails.right;
      unsigned width                    = video_width;
      unsigned height                   = video_height;
      int view_width                    = (int)width;
      gfx_display_t *p_disp             = (gfx_display_t*)disp_userdata;

      int view_height                   = (int)height - ozone->dimensions.header_height - ozone->dimensions.footer_height - ozone->dimensions.spacer_1px;
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
      if (!ozone->fullscreen_thumbnails_available &&
          ozone->show_fullscreen_thumbnails)
         goto error;

      /* Safety check: ensure that current
       * selection matches the entry selected when
       * fullscreen thumbnails were enabled
       * > Note that we exclude this check if we are
       *   currently viewing the quick menu and the
       *   thumbnail view is fading out. This enables
       *   a smooth transition if the user presses
       *   RetroPad A or keyboard 'return' to enter the
       *   quick menu while fullscreen thumbnails are
       *   being displayed */
      if (((size_t)ozone->selection != ozone->fullscreen_thumbnail_selection) &&
          (!ozone->is_quick_menu || ozone->show_fullscreen_thumbnails))
         goto error;

      /* Sanity check: Return immediately if the view
       * width/height is < 1 */
      if ((view_width < 1) || (view_height < 1))
         goto error;

      /* Get number of 'active' thumbnails */
      show_right_thumbnail = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE);
      show_left_thumbnail  = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_AVAILABLE);

      if (show_right_thumbnail)
         num_thumbnails++;

      if (show_left_thumbnail)
         num_thumbnails++;

      /* Do nothing if both thumbnails are missing
       * > Note: Baring inexplicable internal errors, this
       *   can never happen... */
      if (num_thumbnails < 1)
         goto error;

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
      if ((thumbnail_box_width < 1) ||
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
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &right_thumbnail_draw_width, &right_thumbnail_draw_height);

         /* Sanity check */
         if ((right_thumbnail_draw_width <= 0.0f) ||
             (right_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      if (show_left_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               left_thumbnail,
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &left_thumbnail_draw_width, &left_thumbnail_draw_height);

         /* Sanity check */
         if ((left_thumbnail_draw_width <= 0.0f) ||
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
            separator_color, ozone->animations.fullscreen_thumbnail_alpha);

      /* > Thumbnail frame */
      memcpy(frame_color, ozone->theme->sidebar_background, sizeof(frame_color));
      gfx_display_set_alpha(
            frame_color, ozone->animations.fullscreen_thumbnail_alpha);

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
               right_thumbnail_x - frame_width +
                     ((thumbnail_box_width - (int)right_thumbnail_draw_width) >> 1),
               thumbnail_y - frame_width +
                     ((thumbnail_box_height - (int)right_thumbnail_draw_height) >> 1),
               (unsigned)right_thumbnail_draw_width + (frame_width << 1),
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
               left_thumbnail_x - frame_width +
                     ((thumbnail_box_width - (int)left_thumbnail_draw_width) >> 1),
               thumbnail_y - frame_width +
                     ((thumbnail_box_height - (int)left_thumbnail_draw_height) >> 1),
               (unsigned)left_thumbnail_draw_width + (frame_width << 1),
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
   if (ozone->show_fullscreen_thumbnails)
      ozone_hide_fullscreen_thumbnails(ozone, false);
}

static void ozone_set_thumbnail_content(void *data, const char *s)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   if (ozone->is_playlist)
   {
      /* Playlist content */
      if (string_is_empty(s))
      {
         size_t selection      = menu_navigation_get_selection();
         size_t list_size      = menu_entries_get_size();
         file_list_t *list     = menu_entries_get_selection_buf_ptr(0);

         /* Get playlist index corresponding
          * to the selected entry */
         if (list &&
             (selection < list_size) &&
             (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
         {
            size_t playlist_index = list->list[selection].entry_idx;
            gfx_thumbnail_set_content_playlist(ozone->thumbnail_path_data,
                  playlist_get_cached(), playlist_index);
         }
         else
            gfx_thumbnail_set_content_playlist(ozone->thumbnail_path_data,
                  NULL, selection);
      }
   }
   else if (ozone->is_db_manager_list)
   {
      /* Database list content */
      if (string_is_empty(s))
      {
         menu_entry_t entry;
         size_t selection         = menu_navigation_get_selection();

         MENU_ENTRY_INIT(entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);

         if (!string_is_empty(entry.path))
            gfx_thumbnail_set_content(ozone->thumbnail_path_data, entry.path);
      }
   }
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      menu_entry_t entry;
      size_t selection           = menu_navigation_get_selection();
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      ozone_node_t *node         = (ozone_node_t*)selection_buf->list[selection].userdata;

      if (node)
      {
         MENU_ENTRY_INIT(entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);
         if (!string_is_empty(entry.path) && !string_is_empty(node->fullpath))
            gfx_thumbnail_set_content_image(ozone->thumbnail_path_data, node->fullpath, entry.path);
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
      gfx_thumbnail_set_content(ozone->thumbnail_path_data, s);
   }

   ozone_update_content_metadata(ozone);
}

/* Returns true if specified category is currently
 * displayed on screen */
static bool INLINE ozone_category_onscreen(
      ozone_handle_t *ozone, size_t idx)
{
   return (idx >= ozone->first_onscreen_category) &&
          (idx <= ozone->last_onscreen_category);
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
   return (ozone->first_onscreen_category >> 1) +
         (ozone->last_onscreen_category >> 1);
}

/* If currently selected entry is off screen,
 * moves selection to specified on screen target
 * > Does nothing if currently selected item is
 *   already on screen */
static void ozone_auto_select_onscreen_entry(
      ozone_handle_t *ozone,
      enum ozone_onscreen_entry_position_type target_entry)
{
   size_t selection = 0;

   /* Update selection index */
   switch (target_entry)
   {
      case OZONE_ONSCREEN_ENTRY_FIRST:
         selection = ozone->first_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_LAST:
         selection = ozone->last_onscreen_entry;
         break;
      case OZONE_ONSCREEN_ENTRY_CENTRE:
      default:
         selection = (ozone->first_onscreen_entry >> 1) +
               (ozone->last_onscreen_entry >> 1);
         break;
   }

   /* Apply new selection */
   menu_navigation_set_selection(selection);
}

static bool INLINE ozone_metadata_override_available(ozone_handle_t *ozone)
{
   /* Ugly construct...
    * Content metadata display override may be
    * toggled if the following are true:
    * - We are viewing playlist thumbnails
    * - This is *not* an image viewer playlist
    * - Both right and left thumbnails are
    *   enabled/available
    * Short circuiting means that in most cases
    * only 'ozone->is_playlist' will be evaluated,
    * so this isn't too much of a performance hog... */
   return ozone->is_playlist
      &&  ozone->show_thumbnail_bar 
      && !ozone->selection_core_is_viewer 
      && (ozone->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING)
      && gfx_thumbnail_is_enabled(ozone->thumbnail_path_data,
            GFX_THUMBNAIL_LEFT)
      && (ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING) 
      && gfx_thumbnail_is_enabled(ozone->thumbnail_path_data,
            GFX_THUMBNAIL_RIGHT);
}

static bool ozone_is_current_entry_settings(size_t current_selection)
{
   menu_entry_t last_entry;
   const char *entry_value;

   unsigned entry_type                = 0;
   enum msg_file_type entry_file_type = FILE_TYPE_NONE;

   MENU_ENTRY_INIT(last_entry);
   last_entry.path_enabled       = false;
   last_entry.label_enabled      = false;
   last_entry.rich_label_enabled = false;
   last_entry.sublabel_enabled   = false;

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
            case FILE_TYPE_CURSOR:
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
   uintptr_t alpha_tag                = (uintptr_t)
      &ozone->animations.left_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Set common animation parameters */
   animation_entry.easing_enum = EASING_OUT_QUAD;
   animation_entry.tag         = alpha_tag;
   animation_entry.duration    = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.subject     = &ozone->animations.left_thumbnail_alpha;
   animation_entry.cb          = NULL;
   animation_entry.userdata    = NULL;

   /* Check whether metadata override is
    * currently enabled */
   if (ozone->force_metadata_display)
   {
      /* Thumbnail will fade in */
      animation_entry.target_value  = 1.0f;
      ozone->force_metadata_display = false;
   }
   else
   {
      /* Thumbnail will fade out */
      animation_entry.target_value  = 0.0f;
      ozone->force_metadata_display = true;
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
      ozone_handle_t* ozone, enum menu_action direction)
{
   /* Don't start another wiggle animation on top of another */
   if (!ozone || ozone->cursor_wiggle_state.wiggling)
      return;

   /* Don't allow wiggling in invalid directions */
   if (!(
         direction == MENU_ACTION_UP ||
         direction == MENU_ACTION_DOWN ||
         direction == MENU_ACTION_LEFT ||
         direction == MENU_ACTION_RIGHT
   ))
      return;

   /* Start wiggling */
   ozone->cursor_wiggle_state.start_time = menu_driver_get_current_time() / 1000;
   ozone->cursor_wiggle_state.direction  = direction;
   ozone->cursor_wiggle_state.amplitude  = rand() % 15 + 10;
   ozone->cursor_wiggle_state.wiggling   = true;
}


static enum menu_action ozone_parse_menu_entry_action(
      ozone_handle_t *ozone, settings_t *settings,
      enum menu_action action)
{
   uintptr_t tag;
   int new_selection;
   enum menu_action new_action   = action;
   file_list_t *selection_buf    = NULL;
   unsigned horizontal_list_size = 0;
   bool menu_navigation_wraparound_enable;
   bool is_current_entry_settings;
   size_t selection;
   size_t selection_total;

   /* If fullscreen thumbnail view is active, any
    * valid menu action will disable it... */
   if (ozone->show_fullscreen_thumbnails)
   {
      if (action != MENU_ACTION_NOOP)
      {
         ozone_hide_fullscreen_thumbnails(ozone, true);

         /* ...and any action other than Select/OK
          * is ignored
          * > We allow pass-through of Select/OK since
          *   users may want to run content directly
          *   after viewing fullscreen thumbnails,
          *   and having to press RetroPad A or the Return
          *   key twice is navigationally confusing
          * > Note that we can only do this for non-pointer
          *   input
          * > Note that we don't do this when viewing a
          *   file list, since there is no quick menu
          *   in this case - i.e. content loads directly,
          *   and a sudden transition from fullscreen
          *   thumbnail to content is jarring...
          * > We also don't do this when viewing a database
          *   manager list, because the menu transition
          *   detection becomes too cumbersome... */
         if (ozone->is_file_list ||
             ozone->is_db_manager_list ||
             ((action != MENU_ACTION_SELECT) &&
              (action != MENU_ACTION_OK)))
            return MENU_ACTION_NOOP;
      }
   }

   horizontal_list_size       = (unsigned)ozone->horizontal_list.size;

   ozone->messagebox_state    = menu_input_dialog_get_display_kb();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   tag                        = (uintptr_t)selection_buf;
   selection                  = menu_navigation_get_selection();
   selection_total            = menu_entries_get_size();

   menu_navigation_wraparound_enable   = settings->bools.menu_navigation_wraparound_enable;

   /* Don't wiggle left or right if the current entry is a setting. This is
      partially wrong because some settings don't use left and right to change their value, such as
      free input fields (passwords...). This is good enough. */
   is_current_entry_settings = ozone_is_current_entry_settings(selection);

   /* Scan user inputs */
   switch (action)
   {
      case MENU_ACTION_START:
         ozone->cursor_mode = false;
         /* If this is a menu with thumbnails and cursor
          * is not in the sidebar, attempt to show
          * fullscreen thumbnail view */
         if (ozone->fullscreen_thumbnails_available &&
             !ozone->cursor_in_sidebar)
         {
            ozone_show_fullscreen_thumbnails(ozone);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_DOWN:
         if (ozone->cursor_in_sidebar)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection = (ozone->cursor_mode)
               ? ozone_get_onscreen_category_selection(ozone)
               : ozone->categories_selection_ptr;

            new_selection    = (int)(selection + 1);

            if (new_selection >= (int)(ozone->system_tab_end
                     + horizontal_list_size + 1))
               new_selection = 0;

            ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->cursor_mode = false;
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == selection_total - 1)
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_UP:
         if (ozone->cursor_in_sidebar)
         {
            /* If cursor is active, ensure we target
             * an on screen category */
            size_t selection   = (ozone->cursor_mode)
               ? ozone_get_onscreen_category_selection(ozone)
               : ozone->categories_selection_ptr;
            new_selection      = (int)selection - 1;

            if (new_selection < 0)
               new_selection   = horizontal_list_size + ozone->system_tab_end;

            ozone_sidebar_goto(ozone, new_selection);

            new_action         = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            ozone->cursor_mode = false;
            break;
         }
         else if (!menu_navigation_wraparound_enable && selection == 0)
            ozone_start_cursor_wiggle(ozone, MENU_ACTION_UP);

         /* If pointer is active and current selection
          * is off screen, auto select *centre* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_CENTRE);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_LEFT:
         ozone->cursor_mode = false;

         if (ozone->cursor_in_sidebar)
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

            break;
         }

         ozone_go_to_sidebar(ozone, settings, tag);

         new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         break;
      case MENU_ACTION_RIGHT:
         ozone->cursor_mode = false;
         if (!ozone->cursor_in_sidebar)
         {
            if (ozone->depth == 1)
            {
               new_action = MENU_ACTION_NOOP;
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_RIGHT);
            }
            /* Pressing right goes down but faster, so
               wiggle down to say that there is nothing more downwards
               even though the user pressed the right button */
            else if (!menu_navigation_wraparound_enable && selection == selection_total - 1 && !is_current_entry_settings)
               ozone_start_cursor_wiggle(ozone, MENU_ACTION_DOWN);

            break;
         }

         ozone_leave_sidebar(ozone, settings, tag);

         new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
         break;
      case MENU_ACTION_OK:
         ozone->cursor_mode = false;
         if (ozone->cursor_in_sidebar)
         {
            ozone_leave_sidebar(ozone, settings, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL;
            break;
         }
         break;
      case MENU_ACTION_CANCEL:
         ozone->cursor_mode = false;

         /* If this is a playlist, handle 'backing out'
          * of a search, if required */
         if (ozone->is_playlist)
            if (menu_entries_search_get_terms())
               break;

         if (ozone->cursor_in_sidebar)
         {
            /* Go back to main menu tab */
            if (ozone->categories_selection_ptr != 0)
               ozone_sidebar_goto(ozone, 0);

            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         if (menu_entries_get_stack_size(0) == 1)
         {
            ozone_go_to_sidebar(ozone, settings, tag);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
         }
         break;

      case MENU_ACTION_SCROLL_UP:
         /* Descend alphabet (Z towards A) */

         /* Ignore if cursor is in sidebar */
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         /* If pointer is active and current selection
          * is off screen, auto select *last* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_LAST);
         ozone->cursor_mode = false;
         break;
      case MENU_ACTION_SCROLL_DOWN:
         /* Ascend alphabet (A towards Z) */

         /* > Ignore if cursor is in sidebar */
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE;
            break;
         }

         /* If pointer is active and current selection
          * is off screen, auto select *first* item */
         if (ozone->cursor_mode)
            if (!OZONE_ENTRY_ONSCREEN(ozone, selection))
               ozone_auto_select_onscreen_entry(ozone,
                     OZONE_ONSCREEN_ENTRY_FIRST);
         ozone->cursor_mode = false;
         break;

      case MENU_ACTION_INFO:
         /* If we currently viewing a playlist with
          * dual thumbnails, toggle the content metadata
          * override */
         if (ozone_metadata_override_available(ozone))
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
         else if (ozone->is_playlist && ozone->show_thumbnail_bar)
            new_action = MENU_ACTION_NOOP;

         ozone->cursor_mode = false;
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
      void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   menu_entry_t new_entry;
   ozone_handle_t *ozone       = (ozone_handle_t*)userdata;
   menu_entry_t *entry_ptr     = entry;
   settings_t *settings        = config_get_ptr();
   size_t selection            = i;
   /* Process input action */
   enum menu_action new_action = ozone_parse_menu_entry_action(ozone, settings,
         action);
   /* Check whether current selection has changed
    * (due to automatic on screen entry selection...) */
   size_t new_selection        = menu_navigation_get_selection();

   if (new_selection != selection)
   {
      /* Selection has changed - must update
       * entry pointer */
      MENU_ENTRY_INIT(new_entry);
      new_entry.path_enabled       = false;
      new_entry.label_enabled      = false;
      new_entry.rich_label_enabled = false;
      new_entry.value_enabled      = false;
      new_entry.sublabel_enabled   = false;
      menu_entry_get(&new_entry, 0, new_selection, NULL, true);
      entry_ptr                    = &new_entry;
   }

   /* Call standard generic_menu_entry_action() function */
   return generic_menu_entry_action(userdata, entry_ptr,
         new_selection, new_action);
}

static void ozone_menu_animation_update_time(
      float *ticker_pixel_increment,
      unsigned video_width, unsigned video_height)
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
   menu_handle_t *menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));
   const char *directory_assets        = settings->paths.directory_assets;

   if (!menu)
      return NULL;

   video_driver_get_size(&width, &height);

   ozone = (ozone_handle_t*)calloc(1, sizeof(ozone_handle_t));

   if (!ozone)
      goto error;

   *userdata = ozone;

   for (i = 0; i < 15; i++)
      ozone->pure_white[i]  = 1.00f;

   ozone->last_width        = width;
   ozone->last_height       = height;
   ozone->last_scale_factor = gfx_display_get_dpi_scale(p_disp,
         settings, width, height, false, false);
   ozone->last_thumbnail_scale_factor = settings->floats.ozone_thumbnail_scale_factor;

   file_list_initialize(&ozone->selection_buf_old);

   ozone->draw_sidebar                          = true;
   ozone->sidebar_offset                        = 0;
   ozone->pending_message                       = NULL;
   ozone->is_playlist                           = false;
   ozone->categories_selection_ptr              = 0;
   ozone->pending_message                       = NULL;
   ozone->show_cursor                           = false;
   ozone->show_screensaver                      = false;

   ozone->first_frame                           = true;
   ozone->cursor_mode                           = false;

   ozone->sidebar_collapsed                     = false;
   ozone->animations.sidebar_text_alpha         = 1.0f;

   ozone->animations.thumbnail_bar_position     = 0.0f;
   ozone->show_thumbnail_bar                    = false;
   ozone->pending_hide_thumbnail_bar            = false;
   ozone->dimensions_sidebar_width              = 0.0f;

   ozone->num_search_terms_old                  = 0;

   ozone->cursor_wiggle_state.wiggling          = false;

   ozone->thumbnail_path_data = gfx_thumbnail_path_init();
   if (!ozone->thumbnail_path_data)
      goto error;

   ozone->screensaver = menu_screensaver_init();
   if (!ozone->screensaver)
      goto error;

   ozone->fullscreen_thumbnails_available       = false;
   ozone->show_fullscreen_thumbnails            = false;
   ozone->animations.fullscreen_thumbnail_alpha = 0.0f;
   ozone->fullscreen_thumbnail_selection        = 0;
   ozone->fullscreen_thumbnail_label[0]         = '\0';

   ozone->animations.left_thumbnail_alpha       = 1.0f;
   ozone->force_metadata_display                = false;

   gfx_thumbnail_set_stream_delay(-1.0f);
   gfx_thumbnail_set_fade_duration(-1.0f);
   gfx_thumbnail_set_fade_missing(false);

   ozone_sidebar_update_collapse(ozone, settings, false);

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

#if defined(HAVE_LIBRETRODB)
   if (settings->bools.menu_content_show_explore)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_EXPLORE;
#endif

#if defined(HAVE_DYNAMIC)
   if (settings->uints.menu_content_show_contentless_cores !=
         MENU_CONTENTLESS_CORES_DISPLAY_NONE)
      ozone->tabs[++ozone->system_tab_end]      = OZONE_SYSTEM_TAB_CONTENTLESS_CORES;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   gfx_display_set_width(width);
   gfx_display_set_height(height);

   gfx_display_init_white_texture();

   file_list_initialize(&ozone->horizontal_list);

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
         fallback_color_theme = true;
   }
   else
      fallback_color_theme    = true;

   if (fallback_color_theme)
   {
      color_theme = settings->uints.menu_ozone_color_theme;
      ozone_set_color_theme(ozone, color_theme);
   }

   ozone->need_compute                 = false;
   ozone->animations.scroll_y          = 0.0f;
   ozone->animations.scroll_y_sidebar  = 0.0f;

   ozone->first_onscreen_entry         = 0;
   ozone->last_onscreen_entry          = 0;
   ozone->first_onscreen_category      = 0;
   ozone->last_onscreen_category       = 0;

   /* Assets path */
   fill_pathname_join(
      ozone->assets_path,
      directory_assets,
      "ozone",
      sizeof(ozone->assets_path)
   );

   /* PNG path */
   fill_pathname_join(
      ozone->png_path,
      ozone->assets_path,
      "png",
      sizeof(ozone->png_path)
   );

   /* Sidebar path */
   fill_pathname_join(
      ozone->tab_path,
      ozone->png_path,
      "sidebar",
      sizeof(ozone->tab_path)
   );

   /* Icons path */
   fill_pathname_application_special(ozone->icons_path,
       sizeof(ozone->icons_path),
       APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS);

   ozone_last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;
   p_anim->updatetime_cb = ozone_menu_animation_update_time;

   /* set word_wrap function pointer */
   ozone->word_wrap = msg_hash_get_wideglyph_str() ? word_wrap_wideglyph : word_wrap;

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

      if (ozone->thumbnail_path_data)
         free(ozone->thumbnail_path_data);

      menu_screensaver_free(ozone->screensaver);
   }

   gfx_display_deinit_white_texture();

   font_driver_bind_block(NULL, NULL);
}

static void ozone_update_thumbnail_image(void *data)
{
   ozone_handle_t *ozone             = (ozone_handle_t*)data;
   size_t selection                  = menu_navigation_get_selection();
   settings_t *settings              = config_get_ptr();
   playlist_t *playlist              = playlist_get_cached();
   unsigned gfx_thumbnail_upscale_threshold = settings->uints.gfx_thumbnail_upscale_threshold;
   bool network_on_demand_thumbnails = settings->bools.network_on_demand_thumbnails;

   if (!ozone)
      return;

   gfx_thumbnail_cancel_pending_requests();

   gfx_thumbnail_request(
         ozone->thumbnail_path_data,
         GFX_THUMBNAIL_RIGHT,
         playlist,
         selection,
         &ozone->thumbnails.right,
         gfx_thumbnail_upscale_threshold,
         network_on_demand_thumbnails
         );

   /* Image (and video/music) content requires special
    * treatment... */
   if (ozone->selection_core_is_viewer)
   {
      /* Left thumbnail is simply reset */
      gfx_thumbnail_reset(&ozone->thumbnails.left);
   }
   else
   {
      /* Left thumbnail */
      gfx_thumbnail_request(
         ozone->thumbnail_path_data,
         GFX_THUMBNAIL_LEFT,
         playlist,
         selection,
         &ozone->thumbnails.left,
         gfx_thumbnail_upscale_threshold,
         network_on_demand_thumbnails);
   }
}

static void ozone_refresh_thumbnail_image(void *data, unsigned i)
{
   ozone_handle_t *ozone            = (ozone_handle_t*)data;

   if (!ozone)
      return;

   /* Only refresh thumbnails if thumbnails are enabled
    * and we are currently viewing a playlist */
   if ((gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
        gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT)) &&
       (ozone->is_playlist && ozone->depth == 1))
      ozone_update_thumbnail_image(ozone);
}

static bool ozone_init_font(
      ozone_font_data_t *font_data,
      bool is_threaded, char *font_path, float font_size)
{
   int glyph_width       = 0;
   gfx_display_t *p_disp = disp_get_ptr();
   const char *wideglyph_str = msg_hash_get_wideglyph_str();

   /* Free existing */
   if (font_data->font)
   {
      gfx_display_font_free(font_data->font);
      font_data->font = NULL;
   }

   /* Cache approximate dimensions */
   font_data->line_height = (int)(font_size + 0.5f);
   font_data->glyph_width = (int)((font_size * (3.0f / 4.0f)) + 0.5f);

   /* Create font */
   font_data->font = gfx_display_font_file(p_disp, 
         font_path, font_size, is_threaded);

   if (!font_data->font)
      return false;

   /* Get font metadata */
   glyph_width = font_driver_get_message_width(font_data->font, "a", 1, 1.0f);
   if (glyph_width > 0)
      font_data->glyph_width     = glyph_width;

   font_data->wideglyph_width = 100;

   if (wideglyph_str)
   {
      int wideglyph_width =
         font_driver_get_message_width(font_data->font, wideglyph_str, (unsigned)strlen(wideglyph_str), 1.0f);
      
      if (wideglyph_width > 0 && glyph_width > 0) 
         font_data->wideglyph_width = wideglyph_width * 100 / glyph_width;
   }

   font_data->line_height        = font_driver_get_line_height(font_data->font, 1.0f);
   font_data->line_ascender      = font_driver_get_line_ascender(font_data->font, 1.0f);
   font_data->line_centre_offset = font_driver_get_line_centre_offset(font_data->font, 1.0f);

   return true;
}

static void ozone_cache_footer_label(ozone_handle_t *ozone,
      ozone_footer_label_t *label, enum msg_hash_enums enum_idx)
{
   const char *str = msg_hash_to_str(enum_idx);
   /* Determine pixel width */
   unsigned length = (unsigned)strlen(str);

   /* Assign string */
   label->str      = str;
   label->width    = font_driver_get_message_width(
         ozone->fonts.footer.font,
         label->str, length, 1.0f);
   /* If font_driver_get_message_width() fails,
    * use predetermined glyph_width as a fallback */
   if (label->width < 0)
      label->width = length * ozone->fonts.footer.glyph_width;
}

/* Assigns footer label strings (based on current
 * menu language) and calculates pixel widths */
static void ozone_cache_footer_labels(ozone_handle_t *ozone)
{
   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.ok,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.back,
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.search,
         MENU_ENUM_LABEL_VALUE_SEARCH);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.fullscreen_thumbs,
         MSG_TOGGLE_FULLSCREEN_THUMBNAILS);

   ozone_cache_footer_label(
         ozone, &ozone->footer_labels.metadata_toggle,
         MSG_TOGGLE_CONTENT_METADATA);

   /* Record current language setting */
   ozone->footer_labels_language = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
}

/* Determines the size of all menu elements */
static void ozone_set_layout(
      ozone_handle_t *ozone, 
      settings_t *settings,
      bool is_threaded)
{
   char s1[PATH_MAX_LENGTH];
   char font_path[PATH_MAX_LENGTH];
   float scale_factor  = 0.0f;
   bool font_inited    = false;

   font_path[0] = s1[0]= '\0';

   if (!ozone)
      return;

   scale_factor = ozone->last_scale_factor;

   /* Calculate dimensions */
   ozone->dimensions.header_height                 = HEADER_HEIGHT * scale_factor;
   ozone->dimensions.footer_height                 = FOOTER_HEIGHT * scale_factor;

   ozone->dimensions.entry_padding_horizontal_half = ENTRY_PADDING_HORIZONTAL_HALF * scale_factor;
   ozone->dimensions.entry_padding_horizontal_full = ENTRY_PADDING_HORIZONTAL_FULL * scale_factor;
   ozone->dimensions.entry_padding_vertical        = ENTRY_PADDING_VERTICAL * scale_factor;
   ozone->dimensions.entry_height                  = ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.entry_spacing                 = ENTRY_SPACING * scale_factor;
   ozone->dimensions.entry_icon_size               = ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.entry_icon_padding            = ENTRY_ICON_PADDING * scale_factor;

   ozone->dimensions.sidebar_entry_height           = SIDEBAR_ENTRY_HEIGHT * scale_factor;
   ozone->dimensions.sidebar_padding_horizontal     = SIDEBAR_X_PADDING * scale_factor;
   ozone->dimensions.sidebar_padding_vertical       = SIDEBAR_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_padding_vertical = SIDEBAR_ENTRY_Y_PADDING * scale_factor;
   ozone->dimensions.sidebar_entry_icon_size        = SIDEBAR_ENTRY_ICON_SIZE * scale_factor;
   ozone->dimensions.sidebar_entry_icon_padding     = SIDEBAR_ENTRY_ICON_PADDING * scale_factor;
   ozone->dimensions.sidebar_gradient_height        = SIDEBAR_GRADIENT_HEIGHT * scale_factor;

   ozone->dimensions.sidebar_width_normal             = SIDEBAR_WIDTH * scale_factor;
   ozone->dimensions.sidebar_width_collapsed          =
         ozone->dimensions.sidebar_entry_icon_size +
         ozone->dimensions.sidebar_entry_icon_padding * 2 +
         ozone->dimensions.sidebar_padding_horizontal * 2;

   if (ozone->dimensions_sidebar_width == 0)
      ozone->dimensions_sidebar_width = (float)ozone->dimensions.sidebar_width_normal;

   ozone->dimensions.thumbnail_bar_width          =
         (ozone->dimensions.sidebar_width_normal -
          ozone->dimensions.sidebar_entry_icon_size -
          ozone->dimensions.sidebar_entry_icon_padding) *
         ozone->last_thumbnail_scale_factor;
   /* Prevent the thumbnail sidebar from growing too much and make the UI unusable. */
   if (ozone->dimensions.thumbnail_bar_width > ozone->last_width / 2.0f)
      ozone->dimensions.thumbnail_bar_width = ozone->last_width / 2.0f;

   ozone->dimensions.cursor_size                  = CURSOR_SIZE * scale_factor;

   ozone->dimensions.fullscreen_thumbnail_padding = FULLSCREEN_THUMBNAIL_PADDING * scale_factor;

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
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join(font_path, ozone->assets_path, "bold.ttf", sizeof(font_path));
   }

   font_inited = ozone_init_font(&ozone->fonts.title,
         is_threaded, font_path, FONT_SIZE_TITLE * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "chinese-fallback-font.ttf", sizeof(font_path));
         break;
      case RETRO_LANGUAGE_KOREAN:
         fill_pathname_application_special(s1, sizeof(s1),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG);
         fill_pathname_join(font_path, s1, "korean-fallback-font.ttf", sizeof(font_path));
         break;
      default:
         fill_pathname_join(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
   }

   font_inited = ozone_init_font(&ozone->fonts.footer,
         is_threaded, font_path, FONT_SIZE_FOOTER * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.time,
         is_threaded, font_path, FONT_SIZE_TIME * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.entries_label,
         is_threaded, font_path, FONT_SIZE_ENTRIES_LABEL * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.entries_sublabel,
         is_threaded, font_path, FONT_SIZE_ENTRIES_SUBLABEL * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   font_inited = ozone_init_font(&ozone->fonts.sidebar,
         is_threaded, font_path, FONT_SIZE_SIDEBAR * scale_factor);
   ozone->has_all_assets = ozone->has_all_assets && font_inited;

   /* Cache footer text labels
    * > Fonts have been (re)initialised, so need
    *   to recalculate label widths */
   ozone_cache_footer_labels(ozone);

   /* Multiple sidebar parameters are set via animations
    * > ozone_refresh_sidebars() cancels any existing
    *   animations and 'force updates' the affected
    *   variables with newly scaled values */
   ozone_refresh_sidebars(ozone, settings, ozone->last_height);

   /* Entry dimensions must be recalculated after
    * updating menu layout */
   ozone->need_compute = true;
}

static void ozone_context_reset(void *data, bool is_threaded)
{
   unsigned i;
   ozone_handle_t *ozone = (ozone_handle_t*) data;
   settings_t *settings  = config_get_ptr();

   if (ozone)
   {
      ozone->has_all_assets = true;

      ozone_set_layout(ozone, settings, is_threaded);

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         filename[0] = '\0';
#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
            strlcpy(filename, discord_get_own_avatar(), sizeof(filename));
         else
#endif
            strlcpy(filename, OZONE_TEXTURES_FILES[i], sizeof(filename));

         strlcat(filename, FILE_PATH_PNG_EXTENSION, sizeof(filename));

#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
         {
            char buf[PATH_MAX_LENGTH];
            fill_pathname_application_special(buf,
               sizeof(buf),
               APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
            if (!gfx_display_reset_textures_list(filename, buf, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               RARCH_WARN("[OZONE]: Asset missing: \"%s%s%s\".\n", ozone->png_path,
                     PATH_DEFAULT_SLASH(), filename);
         }
         else
         {
#endif
            if (!gfx_display_reset_textures_list(filename, ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               ozone->has_all_assets = false;
#if 0
         }
#endif
      }

      /* Sidebar textures */
      for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];

         filename[0]        = '\0';
         strlcpy(filename,
               OZONE_TAB_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, FILE_PATH_PNG_EXTENSION, sizeof(filename));

         if (!gfx_display_reset_textures_list(filename, ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE]: Asset missing: \"%s%s%s\".\n", ozone->tab_path,
                  PATH_DEFAULT_SLASH(), filename);
         }
      }

      /* Theme textures */
      if (!ozone_reset_theme_textures(ozone))
         ozone->has_all_assets = false;

      /* Icons textures init */
      for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      {
         if (!gfx_display_reset_textures_list(ozone_entries_icon_texture_path(i), ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE]: Asset missing: \"%s%s%s\".\n", ozone->icons_path,
                  PATH_DEFAULT_SLASH(), ozone_entries_icon_texture_path(i));
         }
      }

      gfx_display_deinit_white_texture();
      gfx_display_init_white_texture();

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
      ozone->fade_direction               = false;
      ozone->cursor_in_sidebar            = false;
      ozone->cursor_in_sidebar_old        = false;
      ozone->draw_old_list                = false;
      ozone->messagebox_state             = false;
      ozone->messagebox_state_old         = false;

      ozone->cursor_wiggle_state.wiggling = false;

      /* Animations */
      ozone->animations.cursor_alpha   = 1.0f;
      ozone->animations.scroll_y       = 0.0f;
      ozone->animations.list_alpha     = 1.0f;

      /* Missing assets message */
      if (!ozone->has_all_assets)
         runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      /* Thumbnails */
      ozone_update_thumbnail_image(ozone);

      /* TODO: update savestate thumbnail image */

      ozone_restart_cursor_animation(ozone);

      /* Screensaver */
      menu_screensaver_context_destroy(ozone->screensaver);
   }
   video_driver_monitor_reset();
}

static void ozone_collapse_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_sidebar   = false;
}

static void ozone_unload_thumbnail_textures(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   gfx_thumbnail_cancel_pending_requests();
   gfx_thumbnail_reset(&ozone->thumbnails.right);
   gfx_thumbnail_reset(&ozone->thumbnails.left);
}

static void INLINE ozone_font_free(ozone_font_data_t *font_data)
{
   if (font_data->font)
      gfx_display_font_free(font_data->font);

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
   size_t list_size        = 0;
   ozone_handle_t* ozone   = (ozone_handle_t*) data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         list_size = ozone->horizontal_list.size;
         if (i < list_size)
            return (void*)&ozone->horizontal_list.list[i];
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

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0);

            core_info_get_list(&list);
            if (list && list->info_count > 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

            if (menu_content_show_playlists)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0);

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = &runloop_state_get_ptr()->system;
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
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
               if (system && system->load_no_content)
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

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
                  PARSE_ACTION,
                  false);
#endif

#ifdef HAVE_LAKKA_SWITCH
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
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

            if (settings->bools.menu_show_help)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
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

            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static size_t ozone_list_get_selection(void *data)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return 0;

   return ozone->categories_selection_ptr;
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
      unsigned width, unsigned height,
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
   unsigned entries_end             = (unsigned)menu_entries_get_size();
   bool pointer_enabled             = false;
   unsigned language                = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
   ozone_handle_t *ozone            = (ozone_handle_t*)data;
   gfx_display_t *p_disp            = disp_get_ptr();
   gfx_animation_t          *p_anim = anim_get_ptr();
   settings_t             *settings = config_get_ptr();
   if (!ozone)
      return;

   /* Check whether screen dimensions or menu scale
    * factor have changed */
   scale_factor = gfx_display_get_dpi_scale(p_disp, settings,
         width, height, false, false);
   thumbnail_scale_factor = settings->floats.ozone_thumbnail_scale_factor;

   if ((scale_factor != ozone->last_scale_factor) ||
       (thumbnail_scale_factor != ozone->last_thumbnail_scale_factor) ||
       (width != ozone->last_width) ||
       (height != ozone->last_height))
   {
      ozone->last_scale_factor = scale_factor;
      ozone->last_thumbnail_scale_factor = thumbnail_scale_factor;
      ozone->last_width        = width;
      ozone->last_height       = height;

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      ozone_set_layout(ozone, settings, video_driver_is_threaded());
      video_driver_monitor_reset();
   }

   if (ozone->need_compute)
   {
      ozone_compute_entries_position(ozone, settings, entries_end);
      ozone->need_compute = false;
   }

   /* Check whether menu language has changed
    * > If so, need to re-cache footer text labels */
   if (ozone->footer_labels_language != language)
   {
      ozone->footer_labels_language = language;
      ozone_cache_footer_labels(ozone);
   }

   ozone->selection = menu_navigation_get_selection();

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   /* Read pointer state */
   menu_input_get_pointer_state(&ozone->pointer);

   /* If menu screensaver is active, update
    * screensaver and return */
   if (ozone->show_screensaver)
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

         if ((cursor_x_delta > ozone->pointer_active_delta) ||
             (cursor_x_delta < -ozone->pointer_active_delta) ||
             (cursor_y_delta > ozone->pointer_active_delta) ||
             (cursor_y_delta < -ozone->pointer_active_delta))
            ozone->cursor_mode = true;
      }
      /* On touchscreens, just check for any movement */
      else
      {
         if ((ozone->pointer.x != ozone->cursor_x_old) ||
             (ozone->pointer.y != ozone->cursor_y_old))
            ozone->cursor_mode = true;
      }
   }

   ozone->cursor_x_old = ozone->pointer.x;
   ozone->cursor_y_old = ozone->pointer.y;

   /* Pointer is disabled when:
    * - Showing fullscreen thumbnails
    * - On-screen keyboard is active
    * - A message box is being displayed */
   pointer_enabled = ozone->cursor_mode &&
         !ozone->show_fullscreen_thumbnails &&
         !menu_input_dialog_get_display_kb() &&
         !ozone->should_draw_messagebox;

   /* Process pointer input, if required */
   if (pointer_enabled)
   {
      file_list_t *selection_buf  = menu_entries_get_selection_buf_ptr(0);
      uintptr_t     animation_tag = (uintptr_t)selection_buf;

      int entry_padding           = (ozone->depth == 1) ?
            ozone->dimensions.entry_padding_horizontal_half :
                  ozone->dimensions.entry_padding_horizontal_full;
      float entry_x               = ozone->dimensions_sidebar_width +
            ozone->sidebar_offset + entry_padding;
      float entry_width           = width - ozone->dimensions_sidebar_width -
            ozone->sidebar_offset - entry_padding * 2 -
            ozone->animations.thumbnail_bar_position;
      bool first_entry_found      = false;
      bool last_entry_found       = false;

      unsigned horizontal_list_size = (unsigned)ozone->horizontal_list.size;
      float category_height         = ozone->dimensions.sidebar_entry_height +
            ozone->dimensions.sidebar_entry_padding_vertical;
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
      ozone->last_pointer_in_sidebar = ozone->pointer_in_sidebar;
      if ((ozone->pointer.type == MENU_POINTER_MOUSE) ||
           ozone->pointer.pressed)
         ozone->pointer_in_sidebar   = ozone->draw_sidebar &&
               (ozone->pointer.x < ozone->dimensions_sidebar_width 
                + ozone->sidebar_offset);

      /* If pointer has switched from entries to sidebar
       * or vice versa, must reset pointer acceleration */
      if (ozone->pointer_in_sidebar != ozone->last_pointer_in_sidebar)
      {
         menu_input_set_pointer_y_accel(0.0f);
         ozone->pointer.y_accel = 0.0f;
      }

      /* If pointer is a mouse, then automatically follow
       * mouse focus from entries to sidebar (and vice versa) */
      if (ozone->pointer.type == MENU_POINTER_MOUSE)
      {
         if (ozone->pointer_in_sidebar &&
             !ozone->last_pointer_in_sidebar &&
             !ozone->cursor_in_sidebar)
            ozone_go_to_sidebar(ozone, settings, animation_tag);
         else if (!ozone->pointer_in_sidebar &&
                  ozone->last_pointer_in_sidebar &&
                  ozone->cursor_in_sidebar)
            ozone_leave_sidebar(ozone, settings, animation_tag);
      }

      /* Update scrolling - must be done first, otherwise
       * cannot determine entry/category positions
       * > Entries */
      if (!ozone->pointer_in_sidebar)
      {
         float entry_bottom_boundary = height - ozone->dimensions.header_height -
               ozone->dimensions.spacer_1px - ozone->dimensions.footer_height -
               ozone->dimensions.entry_padding_vertical * 2;

         ozone->animations.scroll_y += ozone->pointer.y_accel;

         if (ozone->animations.scroll_y + ozone->entries_height < entry_bottom_boundary)
            ozone->animations.scroll_y = entry_bottom_boundary - ozone->entries_height;

         if (ozone->animations.scroll_y > 0.0f)
            ozone->animations.scroll_y = 0.0f;
      }
      /* > Sidebar
       * Only process sidebar input here if the
       * cursor is currently *in* the sidebar */
      else if (ozone->cursor_in_sidebar)
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
      ozone->first_onscreen_entry = 0;
      ozone->last_onscreen_entry  = (entries_end > 0) ? entries_end - 1 : 0;

      for (i = 0; i < entries_end; i++)
      {
         float entry_y;
         ozone_node_t *node = (ozone_node_t*)selection_buf->list[i].userdata;

         /* Sanity check */
         if (!node)
            break;

         /* Get current entry y position */
         entry_y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
               ozone->dimensions.entry_padding_vertical + ozone->animations.scroll_y +
               node->position_y;

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
         if (!ozone->pointer_in_sidebar &&
             first_entry_found &&
             !last_entry_found)
         {
            /* Check whether pointer is within the bounds
             * of the current entry */
            if ((ozone->pointer.x > entry_x) &&
                (ozone->pointer.x < entry_x + entry_width) &&
                (ozone->pointer.y > entry_y) &&
                (ozone->pointer.y < entry_y + node->height))
            {
               /* Pointer selection is always updated */
               menu_input_set_pointer_selection((unsigned)i);

               /* If pointer is a mouse, then automatically
                * select entry under cursor */
               if (ozone->pointer.type == MENU_POINTER_MOUSE)
               {
                  /* Note the fudge factor - cannot auto select
                   * items while drag-scrolling the entry list,
                   * so have to wait until pointer acceleration
                   * drops below a 'sensible' level... */
                  if (!ozone->cursor_in_sidebar &&
                      (i != ozone->selection) &&
                      (ozone->pointer.y_accel < ozone->last_scale_factor) &&
                      (ozone->pointer.y_accel > -ozone->last_scale_factor))
                  {
                     menu_navigation_set_selection(i);

                     /* If this is a playlist, must update thumbnails */
                     if (ozone->is_playlist && (ozone->depth == 1))
                     {
                        ozone_set_thumbnail_content(ozone, "");
                        ozone_update_thumbnail_image(ozone);
                     }
                  }
               }

               /* If pointer is pressed and stationary, and
                * if pointer has been held for at least
                * MENU_INPUT_PRESS_TIME_SHORT ms, automatically
                * select current entry */
               if (ozone->pointer.pressed &&
                   !ozone->pointer.dragged &&
                   (ozone->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT) &&
                   (i != ozone->selection))
               {
                  menu_navigation_set_selection(i);

                  /* If we are currently in the sidebar, leave it */
                  if (ozone->cursor_in_sidebar)
                     ozone_leave_sidebar(ozone, settings, animation_tag);
                  /* If this is a playlist, must update thumbnails */
                  else if (ozone->is_playlist && (ozone->depth == 1))
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
         float category_y = ozone->dimensions.header_height + ozone->dimensions.spacer_1px +
               ozone->dimensions.sidebar_padding_vertical + (category_height * i) +
               ((i > ozone->system_tab_end) ?
                     (ozone->dimensions.sidebar_entry_padding_vertical + ozone->dimensions.spacer_1px) : 0) +
               ozone->animations.scroll_y_sidebar;

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
         if (ozone->pointer_in_sidebar &&
             ozone->cursor_in_sidebar &&
             first_category_found &&
             !last_category_found)
         {
            /* If pointer is within the bounds of the
             * current category, cache category index
             * (for use in next 'pointer up' event) */
            if ((ozone->pointer.y > category_y) &&
                (ozone->pointer.y < category_y + category_height))
               ozone->pointer_categories_selection = i;
         }

         if (last_category_found)
            break;
      }
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= entries_end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

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
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const ticker_spacer = OZONE_TICKER_SPACER;
   unsigned ticker_x_offset  = 0;
   unsigned timedate_offset  = 0;
   bool use_smooth_ticker    = settings->bools.menu_ticker_smooth;
   float scale_factor        = ozone->last_scale_factor;
   unsigned logo_icon_size   = 60 * scale_factor;
   unsigned status_icon_size = 92 * scale_factor;
   unsigned seperator_margin = 30 * scale_factor;
   enum gfx_animation_ticker_type
      menu_ticker_type       = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float *col                        = ozone->theme->entries_icon;

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

   /* Title */
   if (use_smooth_ticker)
   {
      ticker_smooth.font        = ozone->fonts.title.font;
      ticker_smooth.selected    = true;
      ticker_smooth.field_width = (video_width - (128 + 47 + 180) * scale_factor);
      ticker_smooth.src_str     = ozone->show_fullscreen_thumbnails ? ozone->fullscreen_thumbnail_label : ozone->title;
      ticker_smooth.dst_str     = title;
      ticker_smooth.dst_str_len = sizeof(title);

      gfx_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = title;
      ticker.len      = (video_width - (128 + 47 + 180) * scale_factor) / ozone->fonts.title.glyph_width;
      ticker.str      = ozone->show_fullscreen_thumbnails ? ozone->fullscreen_thumbnail_label : ozone->title;
      ticker.selected = true;

      gfx_animation_ticker(&ticker);
   }

   gfx_display_draw_text(
         ozone->fonts.title.font,
         title,
         ticker_x_offset + 128 * scale_factor,
           ozone->dimensions.header_height / 2 
         + ozone->fonts.title.line_centre_offset,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* Icon */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      if (dispctx->draw)
      {
#if 0
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
         timedate_offset = 95 * scale_factor;

         gfx_display_draw_text(
               ozone->fonts.time.font,
               msg,
               video_width - 85 * scale_factor,
                 ozone->dimensions.header_height / 2 
               + ozone->fonts.time.line_centre_offset,
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
                     ozone->icons_textures[powerstate.charging? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING : (powerstate.percent > 80)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL : (powerstate.percent > 60)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80 : (powerstate.percent > 40)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60 : (powerstate.percent > 20)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40 : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20],
                     video_width - (60 + 56) * scale_factor,
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
            video_width - (85 * scale_factor) - timedate_offset,
              ozone->dimensions.header_height / 2 
            + ozone->fonts.time.line_centre_offset,
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
                  video_width - (60 + 56) * scale_factor - timedate_offset,
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

static void ozone_draw_footer(
      ozone_handle_t *ozone,
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
   float scale_factor                     = ozone->last_scale_factor;
   unsigned seperator_margin              = 30 * scale_factor;
   float footer_margin                    = 59 * scale_factor;
   float footer_text_y                    = (float)video_height -
         (ozone->dimensions.footer_height / 2.0f) +
         ozone->fonts.footer.line_centre_offset;
   float icon_size                        = 35 * scale_factor;
   float icon_padding                     = 12 * scale_factor;
   float icon_y                           = (float)video_height -
         (ozone->dimensions.footer_height / 2.0f) -
         (icon_size / 2.0f);
   /* Button enable states
    * > Note: Only show 'metadata_toggle' if
    *   'fullscreen_thumbs' is shown. This condition
    *   should be guaranteed anyway, but enforce it
    *   here to prevent 'gaps' in the button list in
    *   the event of unknown errors */
   bool fullscreen_thumbnails_available =
         ozone->fullscreen_thumbnails_available &&
         !ozone->cursor_in_sidebar &&
         ozone->show_thumbnail_bar &&
         ((ozone->thumbnails.right.status != GFX_THUMBNAIL_STATUS_MISSING) ||
          (ozone->thumbnails.left.status  != GFX_THUMBNAIL_STATUS_MISSING)) &&
         (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT));
   bool metadata_override_available     =
         fullscreen_thumbnails_available &&
         ozone_metadata_override_available(ozone);
   /* Determine x origin positions of each
    * button
    * > From right to left, these are ordered:
    *   - ok
    *   - back
    *   - search
    *   - toggle fullscreen thumbs (playlists only)
    *   - toggle metadata (playlists only) */
   float ok_x                = (float)video_width - footer_margin -
         ozone->footer_labels.ok.width - icon_size - icon_padding;
   float back_x              = ok_x -
         ozone->footer_labels.back.width - icon_size - (2.0f * icon_padding);
   float search_x            = back_x -
         ozone->footer_labels.search.width - icon_size - (2.0f * icon_padding);
   float fullscreen_thumbs_x = search_x -
         ozone->footer_labels.fullscreen_thumbs.width - icon_size - (2.0f * icon_padding);
   float metadata_toggle_x   = fullscreen_thumbs_x -
         ozone->footer_labels.metadata_toggle.width - icon_size - (2.0f * icon_padding);
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   float *col                        = ozone->theme_dynamic.entries_icon;

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
         /* > ok */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons ?
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D] :
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R],
               ok_x,
               icon_y,
               video_width,
               video_height,
               0.0f,
               1.0f,
               col,
               mymat);

         /* > back */
         ozone_draw_icon(
               p_disp,
               userdata,
               video_width,
               video_height,
               icon_size,
               icon_size,
               input_menu_swap_ok_cancel_buttons ?
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R] :
               ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D],
               back_x,
               icon_y,
               video_width,video_height,
               0.0f,
               1.0f,
               col,
               mymat);

         /* > search */
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
               video_width,video_height,
               0.0f,
               1.0f,
               col,
               mymat);

         /* > fullscreen_thumbs */
         if (fullscreen_thumbnails_available)
            ozone_draw_icon(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  icon_size,
                  icon_size,
                  ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START],
                  fullscreen_thumbs_x,
                  icon_y,
                  video_width,video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);

         /* > metadata_toggle */
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
                  video_width,video_height,
                  0.0f,
                  1.0f,
                  col,
                  mymat);
      }

      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   /* Draw labels */

   /* > ok */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.ok.str,
         ok_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > back */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.back.str,
         back_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > search */
   gfx_display_draw_text(
         ozone->fonts.footer.font,
         ozone->footer_labels.search.str,
         search_x + icon_size + icon_padding,
         footer_text_y,
         video_width,
         video_height,
         ozone->theme->text_rgba,
         TEXT_ALIGN_LEFT,
         1.0f,
         false,
         1.0f,
         false);

   /* > fullscreen_thumbs */
   if (fullscreen_thumbnails_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.fullscreen_thumbs.str,
            fullscreen_thumbs_x + icon_size + icon_padding,
            footer_text_y,
            video_width,
            video_height,
            ozone->theme->text_rgba,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

   /* > metadata_toggle */
   if (metadata_override_available)
      gfx_display_draw_text(
            ozone->fonts.footer.font,
            ozone->footer_labels.metadata_toggle.str,
            metadata_toggle_x + icon_size + icon_padding,
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
      int usable_width;
      bool use_smooth_ticker                          =
            settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type menu_ticker_type =
            (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
            static const char* const ticker_spacer    = OZONE_TICKER_SPACER;
      unsigned ticker_x_offset                        = 0;

      core_title[0]     = '\0';
      core_title_buf[0] = '\0';

      /* Determine available width for core
       * title string */
      usable_width = metadata_override_available ?
            metadata_toggle_x :
                  fullscreen_thumbnails_available ?
                        fullscreen_thumbs_x :
                        search_x;
      usable_width -= footer_margin + (icon_padding * 3);

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

static void ozone_set_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   gfx_thumbnail_set_system(
         ozone->thumbnail_path_data, s, playlist_get_cached());
}

static void ozone_get_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   const char *system    = NULL;
   if (!ozone)
      return;

   if (gfx_thumbnail_get_system(ozone->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t new_selection       = menu_navigation_get_selection();
   ozone_node_t *node         = (ozone_node_t*)selection_buf->list[new_selection].userdata;

   if (!node)
      return;

   if (ozone->selection != new_selection)
   {
      menu_entry_t entry;
      unsigned entry_type;
      uintptr_t tag                = (uintptr_t)selection_buf;
      size_t selection             = menu_navigation_get_selection();

      MENU_ENTRY_INIT(entry);
      entry.path_enabled           = false;
      entry.label_enabled          = false;
      entry.rich_label_enabled     = false;
      entry.value_enabled          = false;
      entry.sublabel_enabled       = false;
      menu_entry_get(&entry, 0, selection, NULL, true);

      entry_type                   = entry.type;

      ozone->selection_old         = ozone->selection;
      ozone->selection             = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      gfx_animation_kill_by_tag(&tag);
      ozone_update_scroll(ozone, allow_animation, node);

      /* Update thumbnail */
      if (gfx_thumbnail_is_enabled(
               ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(
             ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         bool update_thumbnails = false;

         /* Playlist updates */
         if (ozone->is_playlist && ozone->depth == 1)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Database list updates
          * (pointless nuisance...) */
         else if (ozone->depth == 4 && ozone->is_db_manager_list)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Filebrowser image updates */
         else if (ozone->is_file_list)
         {
            if ((entry_type == FILE_TYPE_IMAGEVIEWER) ||
                (entry_type == FILE_TYPE_IMAGE))
            {
               ozone_set_thumbnail_content(ozone, "imageviewer");
               update_thumbnails = true;
            }
            else
            {
               /* If this is a file list and current
                * entry is not an image, have to 'reset'
                * content + right/left thumbnails
                * (otherwise last loaded thumbnail will
                * persist, and be shown on the wrong entry) */
               gfx_thumbnail_set_content(ozone->thumbnail_path_data, NULL);
               ozone_unload_thumbnail_textures(ozone);
            }
         }

         if (update_thumbnails)
            ozone_update_thumbnail_image(ozone);
      }

      /* TODO: update savestate thumbnail and path */
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
   ozone->pending_message = NULL;

   ozone->should_draw_messagebox = false;
}

static void INLINE ozone_font_bind(ozone_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, &font_data->raster_block);
   font_data->raster_block.carr.coords.vertices = 0;
}

static void INLINE ozone_font_unbind(ozone_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, NULL);
}

static void ozone_frame(void *data, video_frame_info_t *video_info)
{
   gfx_animation_ctx_entry_t entry;
   ozone_handle_t* ozone                  = (ozone_handle_t*) data;
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
   math_matrix_4x4 mymat;

#if 0
   static bool reset                      = false;

   if (discord_avatar_is_ready() && !reset)
   {
      ozone_context_reset(data, false);
      reset = true;
   }
#endif


   if (!ozone)
      return;

   if (ozone->first_frame)
   {
      menu_input_get_pointer_state(&ozone->pointer);

      ozone->cursor_x_old = ozone->pointer.x;
      ozone->cursor_y_old = ozone->pointer.y;
      ozone->first_frame  = false;
   }

   /* OSK Fade detection */
   if (draw_osk != draw_osk_old)
   {
      draw_osk_old = draw_osk;
      if (!draw_osk)
      {
         ozone->should_draw_messagebox       = false;
         ozone->messagebox_state             = false;
         ozone->messagebox_state_old         = false;
         ozone->animations.messagebox_alpha  = 0.0f;
      }
   }

   /* Change theme on the fly */
   if ((color_theme != ozone_last_color_theme) ||
       (ozone_last_use_preferred_system_color_theme != use_preferred_system_color_theme))
   {
      if (use_preferred_system_color_theme)
      {
         color_theme                            = ozone_get_system_theme();
         configuration_set_uint(settings,
               settings->uints.menu_ozone_color_theme, color_theme);
      }

      ozone_set_color_theme(ozone, color_theme);
      ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      ozone_last_use_preferred_system_color_theme = use_preferred_system_color_theme;
   }

   /* If menu screensaver is active, draw
    * screensaver and return */
   if (ozone->show_screensaver)
   {
      menu_screensaver_frame(ozone->screensaver,
            video_info, p_disp);
      return;
   }

   video_driver_set_viewport(video_width, video_height, true, false);

   /* Clear text */
   ozone_font_bind(&ozone->fonts.footer);
   ozone_font_bind(&ozone->fonts.title);
   ozone_font_bind(&ozone->fonts.time);
   ozone_font_bind(&ozone->fonts.entries_label);
   ozone_font_bind(&ozone->fonts.entries_sublabel);
   ozone_font_bind(&ozone->fonts.sidebar);

   /* Background */
   if (libretro_running &&
       (menu_framebuffer_opacity < 1.0f))
   {
      if (menu_framebuffer_opacity != ozone_last_framebuffer_opacity)
         ozone_set_background_running_opacity(ozone, menu_framebuffer_opacity);

      background_color = ozone->theme->background_libretro_running;
   }
   else
      background_color = ozone->theme->background;

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0, 0, video_width, video_height,
         video_width, video_height,
         background_color,
         NULL);

   {
      gfx_display_ctx_rotate_draw_t rotate_draw;
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1.0f;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   /* Header, footer */
   ozone_draw_header(
         ozone,
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
         p_disp, p_anim,
         userdata,
         video_width,
         video_height,
         input_menu_swap_ok_cancel_buttons,
         settings, &mymat);

   /* Sidebar */
   if (ozone->draw_sidebar)
      ozone_draw_sidebar(
            ozone,
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
              ozone->sidebar_offset 
            + (unsigned)ozone->dimensions_sidebar_width,
              ozone->dimensions.header_height 
            + ozone->dimensions.spacer_1px,
            video_width 
            - (unsigned) ozone->dimensions_sidebar_width 
            + (-ozone->sidebar_offset),
              video_height 
            - ozone->dimensions.header_height 
            - ozone->dimensions.footer_height 
            - ozone->dimensions.spacer_1px);

   /* Current list */
   ozone_draw_entries(
         ozone,
         p_disp,
         p_anim,
         settings,
         userdata,
         video_width,
         video_height,
         (unsigned)ozone->selection,
         (unsigned)ozone->selection_old,
         menu_entries_get_selection_buf_ptr(0),
         ozone->animations.list_alpha,
         ozone->animations.scroll_y,
         ozone->is_playlist,
         &mymat
         );

   /* Old list */
   if (ozone->draw_old_list)
      ozone_draw_entries(
            ozone,
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
            ozone->is_playlist_old,
            &mymat
      );

   /* Thumbnail bar */
   if (ozone->show_thumbnail_bar)
      ozone_draw_thumbnail_bar(ozone,
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
      dispctx->scissor_end(userdata,
            video_width, video_height);

   /* Flush first layer of text */
   ozone_font_flush(video_width, video_height, &ozone->fonts.footer);
   ozone_font_flush(video_width, video_height, &ozone->fonts.title);
   ozone_font_flush(video_width, video_height, &ozone->fonts.time);
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);
   ozone_font_flush(video_width, video_height, &ozone->fonts.entries_sublabel);
   ozone_font_flush(video_width, video_height, &ozone->fonts.sidebar);

   /* Draw fullscreen thumbnails, if required */
   ozone_draw_fullscreen_thumbnails(ozone,
         userdata,
         video_info->disp_userdata,
         video_width,
         video_height);

   /* Message box & OSK - second layer of text */
   if (ozone->should_draw_messagebox || draw_osk)
   {
      /* Fade in animation */
      if (ozone->messagebox_state_old != ozone->messagebox_state && ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 0.0f;

         entry.cb = NULL;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 1.0f;
         entry.userdata = NULL;

         gfx_animation_push(&entry);
      }
      /* Fade out animation */
      else if (ozone->messagebox_state_old != ozone->messagebox_state && !ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;
         ozone->messagebox_state = false;

         gfx_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 1.0f;

         entry.cb = ozone_messagebox_fadeout_cb;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 0.0f;
         entry.userdata = ozone;

         gfx_animation_push(&entry);
      }

      ozone_draw_backdrop(
            userdata,
            video_info->disp_userdata,
            video_width,
            video_height,
            float_min(ozone->animations.messagebox_alpha, 0.75f));

      if (draw_osk)
      {
         const char *label = menu_input_dialog_get_label_buffer();
         const char *str   = menu_input_dialog_get_buffer();

         ozone_draw_osk(ozone,
               userdata,
               video_info->disp_userdata,
               video_width,
               video_height,
               label, str);
      }
      else
         ozone_draw_messagebox(
               ozone,
               p_disp,
               userdata,
               video_width,
               video_height,
               ozone->pending_message,
               &mymat);

      /* Flush second layer of text */
      ozone_font_flush(video_width, video_height, &ozone->fonts.footer);
      ozone_font_flush(video_width, video_height, &ozone->fonts.entries_label);
   }

   /* Cursor */
   if (ozone->show_cursor && (ozone->pointer.type != MENU_POINTER_DISABLED))
   {
      bool cursor_visible = (video_fullscreen || mouse_grabbed) &&
            menu_mouse_enable;

      gfx_display_set_alpha(ozone->pure_white, 1.0f);
      if (cursor_visible)
         gfx_display_draw_cursor(
               p_disp,
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
               video_height
               );
   }

   /* Unbind fonts */
   ozone_font_unbind(&ozone->fonts.footer);
   ozone_font_unbind(&ozone->fonts.title);
   ozone_font_unbind(&ozone->fonts.time);
   ozone_font_unbind(&ozone->fonts.entries_label);
   ozone_font_unbind(&ozone->fonts.entries_sublabel);
   ozone_font_unbind(&ozone->fonts.sidebar);

   video_driver_set_viewport(video_width, video_height, false, true);
}

static void ozone_set_header(ozone_handle_t *ozone)
{
   if (ozone->categories_selection_ptr <= ozone->system_tab_end)
      menu_entries_get_title(ozone->title, sizeof(ozone->title));
   else
   {
      ozone_node_t *node = (ozone_node_t*)file_list_get_userdata_at_offset(&ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

      if (node && node->console_name)
      {
         strlcpy(ozone->title, node->console_name, sizeof(ozone->title));

         /* Add current search terms */
         menu_entries_search_append_terms_string(
               ozone->title, sizeof(ozone->title));
      }
   }
}

static void ozone_animation_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_old_list             = false;
   ozone->animations.cursor_alpha   = 1.0f;
}

static void ozone_list_open(ozone_handle_t *ozone, settings_t *settings)
{
   struct gfx_animation_ctx_entry entry;
   uintptr_t sidebar_tag        = (uintptr_t)&ozone->sidebar_offset;

   ozone->draw_old_list         = true;

   /* Left/right animation */
   ozone->animations.list_alpha = 0.0f;

   entry.cb                     = ozone_animation_end;
   entry.duration               = ANIMATION_PUSH_ENTRY_DURATION;
   entry.easing_enum            = EASING_OUT_QUAD;
   entry.subject                = &ozone->animations.list_alpha;
   entry.tag                    = (uintptr_t)NULL;
   entry.target_value           = 1.0f;
   entry.userdata               = ozone;

   gfx_animation_push(&entry);

   /* Sidebar animation */
   ozone_sidebar_update_collapse(ozone, settings, true);

   /* Kill any existing sidebar slide-in/out animations
    * before pushing a new one
    * > This is required since the 'ozone_collapse_end'
    *   callback from an unfinished slide-out animation
    *   may subsequently override the 'draw_sidebar'
    *   value set at the beginning of the next slide-in
    *   animation... */
   gfx_animation_kill_by_tag(&sidebar_tag);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;

      entry.cb            = NULL;
      entry.duration      = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum   = EASING_OUT_QUAD;
      entry.subject       = &ozone->sidebar_offset;
      entry.tag           = sidebar_tag;
      entry.target_value  = 0.0f;
      entry.userdata      = NULL;

      gfx_animation_push(&entry);
   }
   else if (ozone->depth > 1)
   {
      struct gfx_animation_ctx_entry entry;

      entry.cb           = ozone_collapse_end;
      entry.duration     = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum  = EASING_OUT_QUAD;
      entry.subject      = &ozone->sidebar_offset;
      entry.tag          = sidebar_tag;
      entry.target_value = -ozone->dimensions_sidebar_width;
      entry.userdata     = (void*)ozone;

      gfx_animation_push(&entry);
   }
}

static void ozone_populate_entries(void *data,
      const char *path, const char *label, unsigned k)
{
   settings_t *settings  = NULL;
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   int new_depth;
   bool animate;

   if (!ozone)
      return;

   settings              = config_get_ptr();

   ozone_set_header(ozone);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      ozone_selection_changed(ozone, false);

      /* Handle playlist searches
       * (Ozone is a fickle beast...) */
      if (ozone->is_playlist)
      {
         menu_search_terms_t *menu_search_terms=
               menu_entries_search_get_terms();
         size_t num_search_terms               =
               menu_search_terms ? menu_search_terms->size : 0;

         if (ozone->num_search_terms_old != num_search_terms)
         {
            /* Refresh thumbnails */
            ozone_unload_thumbnail_textures(ozone);

            if (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
                gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
            {
               ozone_set_thumbnail_content(ozone, "");
               ozone_update_thumbnail_image(ozone);
            }

            /* If we are currently inside an empty
             * playlist, return to the sidebar */
            if (!ozone->cursor_in_sidebar)
            {
               file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
               uintptr_t animation_tag = (uintptr_t)&ozone->animations.cursor_alpha;
               bool goto_sidebar       = false;

               if (!list || (list->size < 1))
                  goto_sidebar = true;

               if (!goto_sidebar &&
                   (list->list[0].type != FILE_TYPE_RPL_ENTRY))
                  goto_sidebar = true;

               if (goto_sidebar)
               {
                  gfx_animation_kill_by_tag(&animation_tag);
                  ozone->empty_playlist = true;
                  ozone_go_to_sidebar(ozone, settings, animation_tag);
               }
            }

            ozone->num_search_terms_old = num_search_terms;
         }
      }

      return;
   }

   ozone->need_compute = true;

   ozone->first_onscreen_entry    = 0;
   ozone->last_onscreen_entry     = 0;

   new_depth = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);

   animate                     = new_depth != ozone->depth;
   ozone->fade_direction       = new_depth <= ozone->depth;
   ozone->depth                = new_depth;
   ozone->is_playlist          = ozone_is_playlist(ozone, true);
   ozone->is_db_manager_list   = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST));
   ozone->is_file_list         = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES));
   ozone->is_quick_menu        = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS));
   ozone->is_contentless_cores = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST));

   if (animate)
      if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
         ozone_list_open(ozone, settings);

   /* Thumbnails
    * > Note: Leave current thumbnails loaded when
    *   opening the quick menu - allows proper fade
    *   out of the fullscreen thumbnail viewer */
   if (!ozone->is_quick_menu)
   {
      ozone_unload_thumbnail_textures(ozone);

      if (gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
          gfx_thumbnail_is_enabled(ozone->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         /* Only auto-load thumbnails if we are viewing
          * a playlist or a database manager list
          * > Note that we can ignore file browser lists,
          *   since the first selected item on such a list
          *   can never have a thumbnail */
         if (ozone->is_playlist ||
             (ozone->depth == 4 && ozone->is_db_manager_list))
         {
            ozone_set_thumbnail_content(ozone, "");
            ozone_update_thumbnail_image(ozone);
         }
      }
   }

   /* Fullscreen thumbnails are only enabled on
    * playlists, database manager lists and file
    * lists */
   ozone->fullscreen_thumbnails_available =
         (ozone->is_playlist        && ozone->depth == 1) ||
         (ozone->is_db_manager_list && ozone->depth == 4) ||
          ozone->is_file_list;
}

/* TODO: Fancy toggle animation */

static void ozone_toggle(void *userdata, bool menu_on)
{
   settings_t *settings  = NULL;
   bool tmp              = false;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return;

   settings              = config_get_ptr();
   tmp                   = !menu_entries_ctl(
         MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;
      ozone->sidebar_offset = 0.0f;
   }

   ozone_sidebar_update_collapse(ozone, settings, false);
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   settings_t *settings         = config_get_ptr();
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info, settings))
      goto error;

   info.need_push = true;

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
   ozone_node_t *node = NULL;
   int i = (int)list_size;

   if (!ozone || !list)
      return;

   ozone->need_compute = true;

   node = (ozone_node_t*)list->list[i].userdata;

   if (!node)
      node = ozone_alloc_node();

   if (!node)
   {
      RARCH_ERR("ozone node could not be allocated.\n");
      return;
   }

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath = strdup(fullpath);
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
         ozone->show_cursor = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         ozone->show_cursor = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!ozone)
            return -1;
         {
            settings_t *settings              = config_get_ptr();
            ozone_refresh_horizontal_list(ozone, settings);
         }
         break;
      case MENU_ENVIRON_ENABLE_SCREENSAVER:
         ozone->show_screensaver = true;
         break;
      case MENU_ENVIRON_DISABLE_SCREENSAVER:
         ozone->show_screensaver = false;
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
      ozone->pending_message = NULL;
   }

   ozone->pending_message        = strdup(message);
   ozone->messagebox_state       = true;
   ozone->should_draw_messagebox = true;
}

static int ozone_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   settings_t *settings         = config_get_ptr();
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info, settings))
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
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (ozone_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int ozone_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width, height;
   ozone_handle_t *ozone             = (ozone_handle_t*)userdata;
   file_list_t *selection_buf        = menu_entries_get_selection_buf_ptr(0);
   uintptr_t sidebar_tag             = (uintptr_t)selection_buf;
   size_t selection                  = menu_navigation_get_selection();
   size_t entries_end                = menu_entries_get_size();
   settings_t *settings              = config_get_ptr();

   if (!ozone)
      return -1;

   /* If fullscreen thumbnail view is enabled,
    * all input will disable it and otherwise
    * be ignored */
   if (ozone->show_fullscreen_thumbnails)
   {
      /* Must reset scroll acceleration, in case
       * user performed a swipe (don't want menu
       * list to 'drift' after hiding fullscreen
       * thumbnails...) */
      menu_input_set_pointer_y_accel(0.0f);

      ozone_hide_fullscreen_thumbnails(ozone, true);
      return 0;
   }

   video_driver_get_size(&width, &height);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         /* Tap/press header or footer: Menu back/cancel */
         if ((y < ozone->dimensions.header_height) ||
             (y > height - ozone->dimensions.footer_height))
            return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_CANCEL);
         /* Tap/press entries: Activate and/or select item */
         else if ((ptr < entries_end) &&
                  (x > ozone->dimensions_sidebar_width 
                   + ozone->sidebar_offset) &&
                  (x < width - ozone->animations.thumbnail_bar_position))
         {
            if (gesture == MENU_INPUT_GESTURE_TAP)
            {
               /* A 'tap' always produces a menu action */

               /* If current 'pointer' item is not active,
                * activate it immediately */
               if (ptr != selection)
                  menu_navigation_set_selection(ptr);

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
               if (!ozone->cursor_in_sidebar)
                  return ozone_menu_entry_action(ozone, entry,
                        selection, MENU_ACTION_SELECT);

               /* If we currently in the sidebar, leave it */
               ozone_leave_sidebar(ozone, settings, sidebar_tag);
            }
            else
            {
               /* A 'short' press is used only to activate (highlight)
                * an item - it does not invoke a MENU_ACTION_SELECT
                * action */
               menu_input_set_pointer_y_accel(0.0f);

               if (ptr != selection)
                  menu_navigation_set_selection(ptr);

               /* If we are currently in the sidebar, leave it */
               if (ozone->cursor_in_sidebar)
                  ozone_leave_sidebar(ozone, settings, sidebar_tag);
               /* If this is a playlist and the selection
                * has changed, must update thumbnails */
               else if (ozone->is_playlist &&
                        (ozone->depth == 1) &&
                        (ptr != selection))
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
            if (ozone_metadata_override_available(ozone))
               return ozone_menu_entry_action(ozone, entry, selection, MENU_ACTION_INFO);
         }
         /* Tap/press sidebar: return to sidebar or select
          * category */
         else if (ozone->pointer_in_sidebar)
         {
            /* If cursor is not in sidebar, return to sidebar */
            if (!ozone->cursor_in_sidebar)
               ozone_go_to_sidebar(ozone, settings, sidebar_tag);
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
         if ((y > ozone->dimensions.header_height) &&
             (y < height - ozone->dimensions.footer_height) &&
             (ptr < entries_end) &&
             (ptr == selection) &&
             (x > ozone->dimensions_sidebar_width + ozone->sidebar_offset) &&
             (x < width - ozone->animations.thumbnail_bar_position))
            return ozone_menu_entry_action(ozone,
                  entry, selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         /* If this is a playlist, descend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if ((ozone->pointer.type != MENU_POINTER_MOUSE) &&
             ozone->is_playlist &&
             (ozone->depth == 1))
            return ozone_menu_entry_action(ozone, entry, (size_t)ptr, MENU_ACTION_SCROLL_UP);
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         /* If this is a playlist, ascend alphabet
          * > Note: Can only do this if we are not using
          *   a mouse, since it conflicts with auto selection
          *   of entry under cursor */
         if ((ozone->pointer.type != MENU_POINTER_MOUSE) &&
             ozone->is_playlist &&
             (ozone->depth == 1))
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
   ozone_set_thumbnail_system,
   ozone_get_thumbnail_system,
   ozone_set_thumbnail_content,
   gfx_display_osk_ptr_at_pos,
   NULL,                         /* update_savestate_thumbnail_path */
   NULL,                         /* update_savestate_thumbnail_image */
   NULL,                         /* pointer_down */
   ozone_pointer_up,
   ozone_menu_entry_action
};
