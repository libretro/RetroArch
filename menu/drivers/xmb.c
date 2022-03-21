/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018 - Alfredo Monclús
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <file/file_path.h>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <gfx/math/matrix_4x4.h>
#include <streams/file_stream.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#include <array/rhmap.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "../menu_driver.h"
#include "../menu_entries.h"
#include "../menu_screensaver.h"

#include "../../gfx/gfx_animation.h"
#include "../../gfx/gfx_thumbnail_path.h"
#include "../../gfx/gfx_thumbnail.h"

#include "../../core_info.h"
#include "../../core.h"

#include "../../input/input_osk.h"

#include "../../file_path_special.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#include "../../tasks/tasks_internal.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos_menu.h"
#endif
#include "../../content.h"

#define XMB_RIBBON_ROWS 64
#define XMB_RIBBON_COLS 64
#define XMB_RIBBON_VERTICES 2*XMB_RIBBON_COLS*XMB_RIBBON_ROWS-2*XMB_RIBBON_COLS

#ifndef XMB_DELAY
#define XMB_DELAY 166.66667f
#endif

/* Specifies minimum period (in usec) between
 * tab switch events when input repeat is
 * active (i.e. when navigating between top level
 * menu categories by *holding* left/right on
 * RetroPad or keyboard)
 * > Note: We want to set a value of 100 ms
 *   here, but doing so leads to bad pacing when
 *   running at 60 Hz (due to random frame time
 *   deviations - input repeat cycles always take
 *   slightly more or less than 100 ms, so tab
 *   switches occur every n or (n + 1) frames,
 *   which gives the appearance of stuttering).
 *   Reducing the delay by 1 ms accommodates
 *   any timing fluctuations, resulting in
 *   smooth motion */
#define XMB_TAB_SWITCH_REPEAT_DELAY 99000

/* XMB does not have a clean colour theme
 * implementation. Until this is available,
 * the menu screensaver tint will be set to
 * a fixed colour: HTML WhiteSmoke */
#define XMB_SCREENSAVER_TINT 0xF5F5F5

#if 0
#define XMB_DEBUG
#endif

enum
{
   XMB_TEXTURE_MAIN_MENU = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_HISTORY,
   XMB_TEXTURE_FAVORITES,
   XMB_TEXTURE_MUSICS,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   XMB_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   XMB_TEXTURE_NETPLAY,
   XMB_TEXTURE_ROOM,
   XMB_TEXTURE_ROOM_LAN,
   XMB_TEXTURE_ROOM_RELAY,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_TEXTURE_IMAGES,
#endif
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_CLOSE,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_UNDO,
   XMB_TEXTURE_CORE_INFO,
   XMB_TEXTURE_BLUETOOTH,
   XMB_TEXTURE_WIFI,
   XMB_TEXTURE_CORE_OPTIONS,
   XMB_TEXTURE_INPUT_REMAPPING_OPTIONS,
   XMB_TEXTURE_CHEAT_OPTIONS,
   XMB_TEXTURE_DISK_OPTIONS,
   XMB_TEXTURE_SHADER_OPTIONS,
   XMB_TEXTURE_ACHIEVEMENT_LIST,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_RENAME,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
   XMB_TEXTURE_FAVORITE,
   XMB_TEXTURE_ADD_FAVORITE,
   XMB_TEXTURE_MUSIC,
   XMB_TEXTURE_IMAGE,
   XMB_TEXTURE_MOVIE,
   XMB_TEXTURE_CORE,
   XMB_TEXTURE_RDB,
   XMB_TEXTURE_CURSOR,
   XMB_TEXTURE_SWITCH_ON,
   XMB_TEXTURE_SWITCH_OFF,
   XMB_TEXTURE_CLOCK,
   XMB_TEXTURE_BATTERY_FULL,
   XMB_TEXTURE_BATTERY_CHARGING,
   XMB_TEXTURE_BATTERY_80,
   XMB_TEXTURE_BATTERY_60,
   XMB_TEXTURE_BATTERY_40,
   XMB_TEXTURE_BATTERY_20,
   XMB_TEXTURE_POINTER,
   XMB_TEXTURE_ADD,
   XMB_TEXTURE_KEY,
   XMB_TEXTURE_KEY_HOVER,
   XMB_TEXTURE_DIALOG_SLICE,
   XMB_TEXTURE_ACHIEVEMENTS,
   XMB_TEXTURE_AUDIO,
   XMB_TEXTURE_EXIT,
   XMB_TEXTURE_FRAMESKIP,
   XMB_TEXTURE_INFO,
   XMB_TEXTURE_HELP,
   XMB_TEXTURE_NETWORK,
   XMB_TEXTURE_POWER,
   XMB_TEXTURE_SAVING,
   XMB_TEXTURE_UPDATER,
   XMB_TEXTURE_VIDEO,
   XMB_TEXTURE_RECORD,
   XMB_TEXTURE_INPUT_SETTINGS,
   XMB_TEXTURE_MIXER,
   XMB_TEXTURE_LOG,
   XMB_TEXTURE_OSD,
   XMB_TEXTURE_UI,
   XMB_TEXTURE_USER,
   XMB_TEXTURE_PRIVACY,
   XMB_TEXTURE_LATENCY,
   XMB_TEXTURE_DRIVERS,
   XMB_TEXTURE_PLAYLIST,
   XMB_TEXTURE_QUICKMENU,
   XMB_TEXTURE_REWIND,
   XMB_TEXTURE_OVERLAY,
   XMB_TEXTURE_OVERRIDE,
   XMB_TEXTURE_NOTIFICATIONS,
   XMB_TEXTURE_STREAM,
   XMB_TEXTURE_SHUTDOWN,
   XMB_TEXTURE_INPUT_DPAD_U,
   XMB_TEXTURE_INPUT_DPAD_D,
   XMB_TEXTURE_INPUT_DPAD_L,
   XMB_TEXTURE_INPUT_DPAD_R,
   XMB_TEXTURE_INPUT_STCK_U,
   XMB_TEXTURE_INPUT_STCK_D,
   XMB_TEXTURE_INPUT_STCK_L,
   XMB_TEXTURE_INPUT_STCK_R,
   XMB_TEXTURE_INPUT_STCK_P,
   XMB_TEXTURE_INPUT_SELECT,
   XMB_TEXTURE_INPUT_START,
   XMB_TEXTURE_INPUT_BTN_U,
   XMB_TEXTURE_INPUT_BTN_D,
   XMB_TEXTURE_INPUT_BTN_L,
   XMB_TEXTURE_INPUT_BTN_R,
   XMB_TEXTURE_INPUT_LB,
   XMB_TEXTURE_INPUT_RB,
   XMB_TEXTURE_INPUT_LT,
   XMB_TEXTURE_INPUT_RT,
   XMB_TEXTURE_INPUT_ADC,
   XMB_TEXTURE_INPUT_BIND_ALL,
   XMB_TEXTURE_INPUT_MOUSE,
   XMB_TEXTURE_INPUT_LGUN,
   XMB_TEXTURE_INPUT_TURBO,
   XMB_TEXTURE_CHECKMARK,
   XMB_TEXTURE_MENU_ADD,
   XMB_TEXTURE_BRIGHTNESS,
   XMB_TEXTURE_PAUSE,
   XMB_TEXTURE_DEFAULT,
   XMB_TEXTURE_DEFAULT_CONTENT,
   XMB_TEXTURE_MENU_APPLY_TOGGLE,
   XMB_TEXTURE_MENU_APPLY_COG,
   XMB_TEXTURE_DISC,
   XMB_TEXTURE_LAST
};

enum
{
   XMB_SYSTEM_TAB_MAIN = 0,
   XMB_SYSTEM_TAB_SETTINGS,
   XMB_SYSTEM_TAB_HISTORY,
   XMB_SYSTEM_TAB_FAVORITES,
   XMB_SYSTEM_TAB_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   XMB_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   XMB_SYSTEM_TAB_NETPLAY,
#endif
   XMB_SYSTEM_TAB_ADD,
#if defined(HAVE_LIBRETRODB)
   XMB_SYSTEM_TAB_EXPLORE,
#endif
   XMB_SYSTEM_TAB_CONTENTLESS_CORES,

   /* End of this enum - use the last one to determine num of possible tabs */
   XMB_SYSTEM_TAB_MAX_LENGTH
};

/* NOTE: If you change this you HAVE to update
 * xmb_alloc_node() and xmb_copy_node() */
typedef struct
{
   char *fullpath;
   uintptr_t icon;
   uintptr_t content_icon;
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
} xmb_node_t;

typedef struct xmb_handle
{
   /* Keeps track of the last time tabs were switched
    * via a MENU_ACTION_LEFT/MENU_ACTION_RIGHT event */
   retro_time_t last_tab_switch_time; /* uint64_t alignment */

   char *box_message;
   char *bg_file_path;

   file_list_t selection_buf_old; /* ptr alignment */
   file_list_t horizontal_list;   /* ptr alignment */
   /* Maps console tabs to playlist database names */
   xmb_node_t **playlist_db_node_map;

   xmb_node_t main_menu_node;
#ifdef HAVE_IMAGEVIEWER
   xmb_node_t images_tab_node;
#endif
   xmb_node_t music_tab_node;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   xmb_node_t video_tab_node;
#endif
   xmb_node_t settings_tab_node;
   xmb_node_t history_tab_node;
   xmb_node_t favorites_tab_node;
   xmb_node_t add_tab_node;
#if defined(HAVE_LIBRETRODB)
   xmb_node_t explore_tab_node;
#endif
   xmb_node_t contentless_cores_tab_node;
   xmb_node_t netplay_tab_node;
   menu_input_pointer_t pointer;

   font_data_t *font;
   font_data_t *font2;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;

   void (*word_wrap)(char *dst, size_t dst_size, const char *src,
      int line_width, int wideglyph_width, unsigned max_lines);

   menu_screensaver_t *screensaver;

   gfx_thumbnail_path_data_t *thumbnail_path_data;
   struct {
      gfx_thumbnail_t right;
      gfx_thumbnail_t left;
      gfx_thumbnail_t savestate;
   } thumbnails;

   struct
   {
      uintptr_t bg;
      uintptr_t list[XMB_TEXTURE_LAST];
   } textures;

   size_t categories_selection_ptr;
   size_t categories_selection_ptr_old;
   size_t selection_ptr_old;
   size_t fullscreen_thumbnail_selection;

   /* size of the current list */
   size_t list_size;

   int depth;
   int old_depth;
   int icon_size;
   int cursor_size;
   int wideglyph_width;

   unsigned categories_active_idx;
   unsigned categories_active_idx_old;

   float fullscreen_thumbnail_alpha;
   float x;
   float alpha;
   float above_subitem_offset;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;
   float shadow_offset;
   float font_size;
   float font2_size;
   float last_scale_factor;

   float margins_screen_left;
   float margins_screen_top;
   float margins_setting_left;
   float margins_title_left;
   float margins_title_top;
   float margins_title_bottom;
   float margins_title;
   float last_margins_title;
   float margins_label_left;
   float margins_label_top;
   float icon_spacing_horizontal;
   float icon_spacing_vertical;
   float items_active_alpha;
   float items_active_zoom;
   float items_passive_alpha;
   float items_passive_zoom;
   float margins_dialog;
   float margins_slice;
   float textures_arrow_alpha;
   float categories_x_pos;
   float categories_passive_alpha;
   float categories_passive_zoom;
   float categories_active_zoom;
   float categories_active_alpha;

   uint8_t system_tab_end;
   uint8_t tabs[XMB_SYSTEM_TAB_MAX_LENGTH];

   char title_name[255];

   /* Cached texts showing current entry index / current list size */
   char entry_index_str[32];

   /* These have to be huge, because runloop_st->name.savestate
    * has a hard-coded size of 8192...
    * (the extra space here is required to silence compiler
    * warnings...) */
   char savestate_thumbnail_file_path[8204];
   char prev_savestate_thumbnail_file_path[8204];
   char fullscreen_thumbnail_label[255];

   bool fullscreen_thumbnails_available;
   bool show_fullscreen_thumbnails;
   bool show_mouse;
   bool show_screensaver;
   bool use_ps3_layout;
   bool last_use_ps3_layout;
   bool assets_missing;

   /* Favorites, History, Images, Music, Videos, user generated */
   bool is_playlist;
   bool is_db_manager_list;
   bool is_contentless_cores;

   /* Load Content file browser */
   bool is_file_list;
   bool is_quick_menu;

   /* Whether to show entry index for current list */
   bool entry_idx_enabled;
} xmb_handle_t;

static float xmb_scale_mod[8] = {
   1, 1, 1, 1, 1, 1, 1, 1
};

static float xmb_coord_shadow[] = {
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

static float xmb_coord_black[] = {
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

static float xmb_coord_white[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1
};

static float item_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1
};

float gradient_dark_purple[16] = {
   20/255.0,  13/255.0,  20/255.0, 1.0,
   20/255.0,  13/255.0,  20/255.0, 1.0,
   92/255.0,  44/255.0,  92/255.0, 1.0,
   148/255.0,  90/255.0, 148/255.0, 1.0,
};

float gradient_midnight_blue[16] = {
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
};

float gradient_golden[16] = {
   174/255.0, 123/255.0,  44/255.0, 1.0,
   205/255.0, 174/255.0,  84/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
};

float gradient_legacy_red[16] = {
   171/255.0,  70/255.0,  59/255.0, 1.0,
   171/255.0,  70/255.0,  59/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
};

float gradient_electric_blue[16] = {
   1/255.0,   2/255.0,  67/255.0, 1.0,
   1/255.0,  73/255.0, 183/255.0, 1.0,
   1/255.0,  93/255.0, 194/255.0, 1.0,
   3/255.0, 162/255.0, 254/255.0, 1.0,
};

float gradient_apple_green[16] = {
   102/255.0, 134/255.0,  58/255.0, 1.0,
   122/255.0, 131/255.0,  52/255.0, 1.0,
   82/255.0, 101/255.0,  35/255.0, 1.0,
   63/255.0,  95/255.0,  30/255.0, 1.0,
};

float gradient_undersea[16] = {
   23/255.0,  18/255.0,  41/255.0, 1.0,
   30/255.0,  72/255.0, 114/255.0, 1.0,
   52/255.0,  88/255.0, 110/255.0, 1.0,
   69/255.0, 125/255.0, 140/255.0, 1.0,

};

float gradient_volcanic_red[16] = {
   1.0, 0.0, 0.1, 1.00,
   1.0, 0.1, 0.0, 1.00,
   0.1, 0.0, 0.1, 1.00,
   0.1, 0.0, 0.1, 1.00,
};

float gradient_dark[16] = {
   0.1, 0.1, 0.1, 1.00,
   0.1, 0.1, 0.1, 1.00,
   0.0, 0.0, 0.0, 1.00,
   0.0, 0.0, 0.0, 1.00,
};

float gradient_light[16] = {
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
};

float gradient_morning_blue[16] = {
   221/255.0, 241/255.0, 254/255.0, 1.00,
   135/255.0, 206/255.0, 250/255.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   170/255.0, 200/255.0, 252/255.0, 1.00,
};

float gradient_sunbeam[16] = {
   20/255.0,  13/255.0,  20/255.0, 1.0,
   30/255.0,  72/255.0, 114/255.0, 1.0,
   1.0, 1.0, 1.0, 1.00,
   0.1, 0.0, 0.1, 1.00,
};

float gradient_lime_green[16] = {
   209/255.0, 255/255.0,  82/255.0, 1.0,
   146/255.0, 232/255.0,  66/255.0, 1.0,
   82/255.0, 101/255.0,  35/255.0, 1.0,
   63/255.0,  95/255.0,  30/255.0, 1.0,
};

float gradient_pikachu_yellow[16] = {
   63/255.0, 63/255.0,  1/255.0, 1.0,
   174/255.0, 174/255.0,  1/255.0, 1.0,
   191/255.0, 194/255.0,  1/255.0, 1.0,
   254/255.0,  221/255.0,  3/255.0, 1.0,
};

float gradient_gamecube_purple[16] = {
   40/255.0, 20/255.0,  91/255.0, 1.0,
   160/255.0, 140/255.0,  211/255.0, 1.0,
   107/255.0, 92/255.0,  177/255.0, 1.0,
   84/255.0,  71/255.0,  132/255.0, 1.0,
};

float gradient_famicom_red[16] = {
   255/255.0, 191/255.0,  171/255.0, 1.0,
   119/255.0, 49/255.0,  28/255.0, 1.0,
   148/255.0, 10/255.0,  36/255.0, 1.0,
   206/255.0,  126/255.0,  110/255.0, 1.0,
};

float gradient_flaming_hot[16] = {
   231/255.0, 53/255.0,  53/255.0, 1.0,
   242/255.0, 138/255.0,  97/255.0, 1.0,
   236/255.0, 97/255.0,  76/255.0, 1.0,
   255/255.0,  125/255.0,  3/255.0, 1.0,
};

float gradient_ice_cold[16] = {
   66/255.0, 183/255.0,  229/255.0, 1.0,
   29/255.0, 164/255.0,  255/255.0, 1.0,
   176/255.0, 255/255.0,  247/255.0, 1.0,
   174/255.0,  240/255.0,  255/255.0, 1.0,
};

float gradient_midgar[16] = {
   255/255.0, 0/255.0,  0/255.0, 1.0,
   0/255.0, 0/255.0,  255/255.0, 1.0,
   0/255.0, 255/255.0,  0/255.0, 1.0,
   32/255.0,  32/255.0,  32/255.0, 1.0,
};

static INLINE float xmb_item_y(const xmb_handle_t *xmb, int i, size_t current)
{
   float iy = xmb->icon_spacing_vertical;

   if (i < (int)current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + xmb->above_subitem_offset);
      else
         iy *= (i - (int)current + xmb->above_item_offset);
   else
      iy    *= (i - (int)current + xmb->under_item_offset);

   if (i == (int)current)
      iy = xmb->icon_spacing_vertical * xmb->active_item_factor;

   return iy;
}


static void xmb_calculate_visible_range(const xmb_handle_t *xmb,
      unsigned height, size_t list_size, unsigned current,
      unsigned *first, unsigned *last)
{
   unsigned j;
   float    base_y = xmb->margins_screen_top;

   *first = 0;
   *last  = (unsigned)(list_size ? list_size - 1 : 0);

   if (current)
   {
      for (j = current; j-- > 0; )
      {
         float bottom = xmb_item_y(xmb, j, current)
            + base_y + xmb->icon_size;

         if (bottom < 0)
            break;

         *first = j;
      }
   }

   for (j = current+1; j < list_size; j++)
   {
      float top = xmb_item_y(xmb, j, current) + base_y;

      if (top > height)
         break;

      *last = j;
   }
}


const char* xmb_theme_ident(void)
{
   settings_t    *settings = config_get_ptr();
   unsigned menu_xmb_theme = settings->uints.menu_xmb_theme;

   switch (menu_xmb_theme)
   {
      case XMB_ICON_THEME_FLATUI:
         return "flatui";
      case XMB_ICON_THEME_RETROACTIVE:
         return "retroactive";
      case XMB_ICON_THEME_RETROSYSTEM:
         return "retrosystem";
      case XMB_ICON_THEME_PIXEL:
         return "pixel";
      case XMB_ICON_THEME_NEOACTIVE:
         return "neoactive";
      case XMB_ICON_THEME_SYSTEMATIC:
         return "systematic";
      case XMB_ICON_THEME_DOTART:
         return "dot-art";
      case XMB_ICON_THEME_CUSTOM:
         return "custom";
      case XMB_ICON_THEME_MONOCHROME_INVERTED:
         return "monochrome";
      case XMB_ICON_THEME_AUTOMATIC:
         return "automatic";
      case XMB_ICON_THEME_AUTOMATIC_INVERTED:
         return "automatic";
      case XMB_ICON_THEME_MONOCHROME:
      default:
         break;
   }
   return "monochrome";
}

/* NOTE: This exists because calloc()ing xmb_node_t is expensive
 * when you can have big lists like MAME and fba playlists */
static xmb_node_t *xmb_alloc_node(void)
{
   xmb_node_t *node = (xmb_node_t*)malloc(sizeof(*node));

   if (!node)
      return NULL;

   node->alpha    = node->label_alpha  = 0;
   node->zoom     = node->x = node->y  = 0;
   node->icon     = node->content_icon = 0;
   node->fullpath = NULL;

   return node;
}

static void xmb_free_node(xmb_node_t *node)
{
   if (!node)
      return;

   if (node->fullpath)
      free(node->fullpath);

   node->fullpath = NULL;

   free(node);
}

/**
 * @brief frees all xmb_node_t in a file_list_t
 *
 * file_list_t asumes userdata holds a simple structure and
 * free()'s it. Can't change this at the time because other
 * code depends on this behavior.
 *
 * @param list
 * @param actiondata whether to free actiondata too
 */
static void xmb_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = list ? (unsigned)list->size : 0;

   for (i = 0; i < size; ++i)
   {
      xmb_free_node((xmb_node_t*)file_list_get_userdata_at_offset(list, i));

      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

static xmb_node_t *xmb_copy_node(const xmb_node_t *old_node)
{
   xmb_node_t *new_node = (xmb_node_t*)malloc(sizeof(*new_node));

   if (!new_node)
      return NULL;

   *new_node            = *old_node;
   new_node->fullpath   = old_node->fullpath ? strdup(old_node->fullpath) : NULL;

   return new_node;
}

static float *xmb_gradient_ident(unsigned xmb_color_theme)
{
   switch (xmb_color_theme)
   {
      case XMB_THEME_DARK_PURPLE:
         return &gradient_dark_purple[0];
      case XMB_THEME_MIDNIGHT_BLUE:
         return &gradient_midnight_blue[0];
      case XMB_THEME_GOLDEN:
         return &gradient_golden[0];
      case XMB_THEME_ELECTRIC_BLUE:
         return &gradient_electric_blue[0];
      case XMB_THEME_APPLE_GREEN:
         return &gradient_apple_green[0];
      case XMB_THEME_UNDERSEA:
         return &gradient_undersea[0];
      case XMB_THEME_VOLCANIC_RED:
         return &gradient_volcanic_red[0];
      case XMB_THEME_DARK:
         return &gradient_dark[0];
      case XMB_THEME_LIGHT:
         return &gradient_light[0];
      case XMB_THEME_MORNING_BLUE:
         return &gradient_morning_blue[0];
      case XMB_THEME_SUNBEAM:
         return &gradient_sunbeam[0];
      case XMB_THEME_LIME:
         return &gradient_lime_green[0];
      case XMB_THEME_MIDGAR:
         return &gradient_midgar[0];
      case XMB_THEME_PIKACHU_YELLOW:
         return &gradient_pikachu_yellow[0];
      case XMB_THEME_GAMECUBE_PURPLE:
         return &gradient_gamecube_purple[0];
      case XMB_THEME_FAMICOM_RED:
         return &gradient_famicom_red[0];
      case XMB_THEME_FLAMING_HOT:
         return &gradient_flaming_hot[0];
      case XMB_THEME_ICE_COLD:
         return &gradient_ice_cold[0];
      case XMB_THEME_LEGACY_RED:
      default:
         break;
   }

   return &gradient_legacy_red[0];
}

static size_t xmb_list_get_selection(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb)
      return 0;

   return xmb->categories_selection_ptr;
}

static size_t xmb_list_get_size(void *data, enum menu_list_type type)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         return xmb->horizontal_list.size;
      case MENU_LIST_TABS:
         return xmb->system_tab_end;
   }

   return 0;
}

static void *xmb_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size = 0;
   xmb_handle_t *xmb = (xmb_handle_t*)data;

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
         list_size = xmb->horizontal_list.size;
         if (i < list_size)
            return (void*)&xmb->horizontal_list.list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static void xmb_draw_icon(
      void *userdata,
      gfx_display_t *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      unsigned video_width,
      unsigned video_height,
      bool xmb_shadows_enable,
      int icon_size,
      uintptr_t texture,
      float x,
      float y,
      unsigned width,
      unsigned height,
      float alpha,
      float rotation,
      float scale_factor,
      float *color,
      float shadow_offset,
      math_matrix_4x4 *mymat)
{
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;

   if (
         (x < (-icon_size / 2.0f)) ||
         (x > width)               ||
         (y < (icon_size  / 2.0f)) ||
         (y > height + icon_size)
      )
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;

   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.rotation        = rotation;
   draw.scale_factor    = scale_factor;
#if defined(VITA) || defined(WIIU)
   draw.width          *= scale_factor;
   draw.height         *= scale_factor;
#endif
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;

   if (xmb_shadows_enable)
   {
      gfx_display_set_alpha(xmb_coord_shadow, color[3] * 0.35f);

      coords.color      = xmb_coord_shadow;
      draw.x            = x + shadow_offset;
      draw.y            = height - y - shadow_offset;

#if defined(VITA) || defined(WIIU)
      if (scale_factor < 1)
      {
         draw.x         = draw.x + (icon_size-draw.width)/2;
         draw.y         = draw.y + (icon_size-draw.width)/2;
      }
#endif
      if (draw.height > 0 && draw.width > 0)
         if (dispctx && dispctx->draw)
            dispctx->draw(&draw, userdata, video_width, video_height);
   }

   coords.color         = (const float*)color;
   draw.x               = x;
   draw.y               = height - y;

#if defined(VITA) || defined(WIIU)
   if (scale_factor < 1)
   {
      draw.x            = draw.x + (icon_size-draw.width)/2;
      draw.y            = draw.y + (icon_size-draw.width)/2;
   }
#endif
   if (draw.height > 0 && draw.width > 0)
      if (dispctx && dispctx->draw)
         dispctx->draw(&draw, userdata, video_width, video_height);
}

static void xmb_draw_text(
      bool xmb_shadows_enable,
      xmb_handle_t *xmb,
      settings_t *settings,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font)
{
   uint32_t color;
   uint8_t a8;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8       = 255 * alpha;

   /* Avoid drawing 100% transparent text */
   if (a8 == 0)
      return;

   color              = FONT_COLOR_RGBA(
         settings->uints.menu_font_color_red,
         settings->uints.menu_font_color_green,
         settings->uints.menu_font_color_blue, a8);

   gfx_display_draw_text(font, str, x, y,
         width, height, color, text_align, scale_factor,
         xmb_shadows_enable,
         xmb->shadow_offset, false);
}

static void xmb_messagebox(void *data, const char *message)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb || string_is_empty(message))
      return;

   xmb->box_message = strdup(message);
}

static void xmb_render_messagebox_internal(
      void *userdata,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height,
      xmb_handle_t *xmb, const char *message,
      math_matrix_4x4 *mymat)
{
   unsigned i, y_position;
   char wrapped_message[MENU_SUBLABEL_MAX_LENGTH];
   int x, y, longest_width  = 0;
   float line_height        = 0;
   int usable_width         = 0;
   struct string_list list  = {0};
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   bool input_dialog_display_kb      = false;

   wrapped_message[0]       = '\0';

   /* Sanity check */
   if (string_is_empty(message) ||
       !xmb ||
       !xmb->font)
      return;

   usable_width = (int)video_width - (xmb->margins_dialog * 8);

   if (usable_width < 1)
      return;

   /* Split message into lines */
   (xmb->word_wrap)(
         wrapped_message, sizeof(wrapped_message), message,
         usable_width / (xmb->font_size * 0.6f),
         xmb->wideglyph_width, 0);

   string_list_initialize(&list);

   if (!string_split_noalloc(&list, wrapped_message, "\n")
         || list.elems == 0
      )
   {
      string_list_deinitialize(&list);
      return;
   }

   input_dialog_display_kb = menu_input_dialog_get_display_kb();
   line_height             = xmb->font->size * 1.2;

   y_position              = video_height / 2;
   if (input_dialog_display_kb)
      y_position           = video_height / 4;

   x                       = video_width  / 2;
   y                       = y_position - (list.size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      if (!string_is_empty(msg))
      {
         int width = font_driver_get_message_width(
               xmb->font, msg, (unsigned)strlen(msg), 1);

         longest_width = (width > longest_width) ?
               width : longest_width;
      }
   }

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   gfx_display_draw_texture_slice(
         p_disp,
         userdata,
         video_width,
         video_height,
         x - longest_width/2 - xmb->margins_dialog,
         y + xmb->margins_slice - xmb->margins_dialog,
         256, 256,
         longest_width + xmb->margins_dialog * 2,
         line_height * list.size + xmb->margins_dialog * 2,
         video_width, video_height,
         NULL,
         xmb->margins_slice, xmb->last_scale_factor,
         xmb->textures.list[XMB_TEXTURE_DIALOG_SLICE],
         mymat);

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      if (msg)
         gfx_display_draw_text(xmb->font, msg,
               x - longest_width/2.0,
               y + (i+0.75) * line_height,
               video_width, video_height, 0x444444ff,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, false);
   }

   if (input_dialog_display_kb)
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      gfx_display_draw_keyboard(
            p_disp,
            userdata,
            video_width,
            video_height,
            xmb->textures.list[XMB_TEXTURE_KEY_HOVER],
            xmb->font,
            input_st->osk_grid,
            input_st->osk_ptr,
            0xffffffff);
   }

   string_list_deinitialize(&list);
}

static char* xmb_path_dynamic_wallpaper(xmb_handle_t *xmb)
{
   char path[PATH_MAX_LENGTH];
   char       *tmp                    = string_replace_substring(xmb->title_name, "/", " ");
   settings_t *settings               = config_get_ptr();
   const char *dir_dynamic_wallpapers = settings->paths.directory_dynamic_wallpapers;

   path[0]          = '\0';

   if (tmp)
   {
      fill_pathname_join_noext(
            path,
            dir_dynamic_wallpapers,
            tmp,
            sizeof(path));
      free(tmp);
   }

   strlcat(path, FILE_PATH_PNG_EXTENSION, sizeof(path));

   if (!path_is_valid(path))
      fill_pathname_application_special(path, sizeof(path),
            APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

   return strdup(path);
}

static void xmb_update_dynamic_wallpaper(xmb_handle_t *xmb)
{
   char *path;
   settings_t *settings               = config_get_ptr();

   if (!settings->bools.menu_dynamic_wallpaper_enable)
      return;

   path = xmb_path_dynamic_wallpaper(xmb);
   if (!string_is_equal(path, xmb->bg_file_path))
   {
      if (path_is_valid(path))
      {
         task_push_image_load(path,
               video_driver_supports_rgba(), 0,
               menu_display_handle_wallpaper_upload, NULL);
         if (!string_is_empty(xmb->bg_file_path))
            free(xmb->bg_file_path);
         xmb->bg_file_path = strdup(path);
      }
   }

   free(path);
   path = NULL;
}

static void xmb_update_savestate_thumbnail_path(void *data, unsigned i)
{
   settings_t *settings = config_get_ptr();
   xmb_handle_t *xmb    = (xmb_handle_t*)data;
   int state_slot       = settings->ints.state_slot;
   bool savestate_thumbnail_enable
                        = settings->bools.savestate_thumbnail_enable;
   if (!xmb)
      return;

   /* Cache previous savestate thumbnail path */
   strlcpy(
         xmb->prev_savestate_thumbnail_file_path,
         xmb->savestate_thumbnail_file_path,
         sizeof(xmb->prev_savestate_thumbnail_file_path));

   xmb->savestate_thumbnail_file_path[0] = '\0';

   /* Savestate thumbnails are only relevant
    * when viewing the quick menu */
   if (!xmb->is_quick_menu)
      return;

   if (savestate_thumbnail_enable)
   {
      menu_entry_t entry;

      MENU_ENTRY_INIT(entry);
      entry.path_enabled       = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      entry.sublabel_enabled   = false;
      menu_entry_get(&entry, 0, i, NULL, true);

      if (!string_is_empty(entry.label))
      {
         if (string_is_equal(entry.label, "state_slot") ||
             string_is_equal(entry.label, "loadstate") ||
             string_is_equal(entry.label, "savestate"))
         {
            char path[8204];
            runloop_state_t *runloop_st = runloop_state_get_ptr();

            path[0] = '\0';

            if (state_slot > 0)
               snprintf(path, sizeof(path), "%s%d",
                     runloop_st->name.savestate, state_slot);
            else if (state_slot < 0)
               fill_pathname_join_delim(path,
                     runloop_st->name.savestate, "auto", '.', sizeof(path));
            else
               strlcpy(path, runloop_st->name.savestate, sizeof(path));

            strlcat(path, FILE_PATH_PNG_EXTENSION, sizeof(path));

            if (path_is_valid(path))
               strlcpy(
                     xmb->savestate_thumbnail_file_path, path,
                     sizeof(xmb->savestate_thumbnail_file_path));
         }
      }
   }
}

static void xmb_update_thumbnail_image(void *data)
{
   const char *core_name = NULL;
   xmb_handle_t                *xmb     = (xmb_handle_t*)data;
   size_t                selection      = menu_navigation_get_selection();
   playlist_t                *playlist  = playlist_get_cached();
   settings_t                *settings  = config_get_ptr();
   unsigned thumbnail_upscale_threshold = settings->uints.gfx_thumbnail_upscale_threshold;
   bool network_on_demand_thumbnails    = settings->bools.network_on_demand_thumbnails;

   if (!xmb)
      return;

   gfx_thumbnail_cancel_pending_requests();

   /* imageviewer content requires special treatment... */
   gfx_thumbnail_get_core_name(xmb->thumbnail_path_data, &core_name);
   if (string_is_equal(core_name, "imageviewer"))
   {
      gfx_thumbnail_reset(&xmb->thumbnails.right);
      gfx_thumbnail_reset(&xmb->thumbnails.left);

      /* Right thumbnail */
      if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data,
               GFX_THUMBNAIL_RIGHT))
         gfx_thumbnail_request(
            xmb->thumbnail_path_data,
            GFX_THUMBNAIL_RIGHT,
            playlist,
            selection,
            &xmb->thumbnails.right,
            thumbnail_upscale_threshold,
            network_on_demand_thumbnails);
      /* Left thumbnail */
      else if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data,
               GFX_THUMBNAIL_LEFT))
         gfx_thumbnail_request(
            xmb->thumbnail_path_data,
            GFX_THUMBNAIL_LEFT,
            playlist,
            selection,
            &xmb->thumbnails.left,
            thumbnail_upscale_threshold,
            network_on_demand_thumbnails);
   }
   else
   {
      /* Right thumbnail */
      gfx_thumbnail_request(
         xmb->thumbnail_path_data,
         GFX_THUMBNAIL_RIGHT,
         playlist,
         selection,
         &xmb->thumbnails.right,
         thumbnail_upscale_threshold,
         network_on_demand_thumbnails);

      /* Left thumbnail */
      gfx_thumbnail_request(
         xmb->thumbnail_path_data,
         GFX_THUMBNAIL_LEFT,
         playlist,
         selection,
         &xmb->thumbnails.left,
         thumbnail_upscale_threshold,
         network_on_demand_thumbnails);
   }
}

static unsigned xmb_get_system_tab(xmb_handle_t *xmb, unsigned i)
{
   if (i <= xmb->system_tab_end)
      return xmb->tabs[i];
   return UINT_MAX;
}

static void xmb_refresh_thumbnail_image(void *data, unsigned i)
{
   xmb_handle_t *xmb                = (xmb_handle_t*)data;

   if (!xmb)
      return;

   /* Only refresh thumbnails if thumbnails are enabled */
   if (  gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
         gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
   {
      unsigned depth          = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
      unsigned xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);

      /* Only refresh thumbnails if we are viewing a playlist or
       * the quick menu... */
      if (((((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && depth == 1) ||
             (xmb_system_tab < XMB_SYSTEM_TAB_SETTINGS && depth == 4)) &&
              xmb->is_playlist)) ||
            xmb->is_quick_menu)
         xmb_update_thumbnail_image(xmb);
   }
}

static void xmb_set_thumbnail_system(void *data, char*s, size_t len)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   gfx_thumbnail_set_system(
         xmb->thumbnail_path_data, s, playlist_get_cached());
}

static void xmb_get_thumbnail_system(void *data, char*s, size_t len)
{
   xmb_handle_t *xmb  = (xmb_handle_t*)data;
   const char *system = NULL;
   if (!xmb)
      return;

   if (gfx_thumbnail_get_system(xmb->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void xmb_unload_thumbnail_textures(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   gfx_thumbnail_cancel_pending_requests();
   gfx_thumbnail_reset(&xmb->thumbnails.right);
   gfx_thumbnail_reset(&xmb->thumbnails.left);
   gfx_thumbnail_reset(&xmb->thumbnails.savestate);
}

static void xmb_set_thumbnail_content(void *data, const char *s)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   /* Disable fullscreen thumbnails by default,
    * and only enable if thumbnail content is
    * actually set
    * > This is the easiest method for verifying
    *   that we are currently viewing a relevant
    *   menu type */
   xmb->fullscreen_thumbnails_available = false;

   if (xmb->is_playlist)
   {
      /* Playlist content */
      if (string_is_empty(s))
      {
         size_t selection      = menu_navigation_get_selection();
         size_t list_size      = menu_entries_get_size();
         file_list_t *list     = menu_entries_get_selection_buf_ptr(0);
         bool playlist_valid   = false;
         size_t playlist_index = selection;

         /* Get playlist index corresponding
          * to the selected entry */
         if (list &&
             (selection < list_size) &&
             (list->list[selection].type == FILE_TYPE_RPL_ENTRY))
         {
            playlist_valid = true;
            playlist_index = list->list[selection].entry_idx;
         }

         gfx_thumbnail_set_content_playlist(xmb->thumbnail_path_data,
               playlist_valid ? playlist_get_cached() : NULL, playlist_index);
         xmb->fullscreen_thumbnails_available = true;
      }
   }
   else if (xmb->is_db_manager_list)
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
         {
            gfx_thumbnail_set_content(xmb->thumbnail_path_data, entry.path);
            xmb->fullscreen_thumbnails_available = true;
         }
      }
   }
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      menu_entry_t entry;
      size_t selection           = menu_navigation_get_selection();
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      xmb_node_t *node           = (xmb_node_t*)selection_buf->list[selection].userdata;

      if (node)
      {
         MENU_ENTRY_INIT(entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);
         if (  !string_is_empty(entry.path) &&
               !string_is_empty(node->fullpath))
         {
            gfx_thumbnail_set_content_image(xmb->thumbnail_path_data,
                  node->fullpath, entry.path);
            xmb->fullscreen_thumbnails_available = true;
         }
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
      gfx_thumbnail_set_content(xmb->thumbnail_path_data, s);
      xmb->fullscreen_thumbnails_available = true;
   }
}

static void xmb_update_savestate_thumbnail_image(void *data)
{
   xmb_handle_t *xmb    = (xmb_handle_t*)data;
   settings_t *settings = config_get_ptr();
   unsigned thumbnail_upscale_threshold
                        = settings->uints.gfx_thumbnail_upscale_threshold;
   if (!xmb)
      return;

   /* Savestate thumbnails are only relevant
    * when viewing the quick menu */
   if (!xmb->is_quick_menu)
      return;

   /* If path is empty, just reset thumbnail */
   if (string_is_empty(xmb->savestate_thumbnail_file_path))
      gfx_thumbnail_reset(&xmb->thumbnails.savestate);
   else
   {
      /* Only request thumbnail if:
       * > Thumbnail has never been loaded *OR*
       * > Thumbnail path has changed */
      if ((xmb->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_UNKNOWN) ||
          !string_is_equal(xmb->savestate_thumbnail_file_path, xmb->prev_savestate_thumbnail_file_path))
         gfx_thumbnail_request_file(
               xmb->savestate_thumbnail_file_path,
               &xmb->thumbnails.savestate,
               thumbnail_upscale_threshold);
   }
}

/* Is called when the pointer position changes within a list/sub-list (vertically) */
static void xmb_selection_pointer_changed(
      xmb_handle_t *xmb, bool allow_animations)
{
   unsigned i, end, height;
   uintptr_t tag;
   size_t num                 = 0;
   int threshold              = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   if (!xmb)
      return;

   end       = (unsigned)menu_entries_get_size();
   threshold = xmb->icon_size * 10;

   video_driver_get_size(NULL, &height);

   tag       = (uintptr_t)selection_buf;

   gfx_animation_kill_by_tag(&tag);
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &num);

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia         = xmb->items_passive_alpha;
      float iz         = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      iy      = xmb_item_y(xmb, i, selection);
      real_iy = iy + xmb->margins_screen_top;

      if (i == selection)
      {
         unsigned     depth      = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
         unsigned xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);

         /* Update entry index text */
         if (xmb->entry_idx_enabled)
         {
            snprintf(xmb->entry_index_str, sizeof(xmb->entry_index_str),
                     "%lu/%lu", (unsigned long)selection + 1,
                                (unsigned long)xmb->list_size);
         }

         ia                      = xmb->items_active_alpha;
         iz                      = xmb->items_active_zoom;

         if (
               gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
               gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT)
            )
         {
            bool update_thumbnails = false;

            /* Playlist updates */
            if (((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && depth == 1) ||
                 (xmb_system_tab < XMB_SYSTEM_TAB_SETTINGS && depth == 4)) &&
                xmb->is_playlist)
            {
               xmb_set_thumbnail_content(xmb, NULL);
               update_thumbnails = true;
            }
            /* Database list updates
             * (pointless nuisance...) */
            else if (depth == 4 && xmb->is_db_manager_list)
            {
               xmb_set_thumbnail_content(xmb, NULL);
               update_thumbnails = true;
            }
            /* Filebrowser image updates */
            else if (xmb->is_file_list)
            {
               menu_entry_t entry;
               unsigned entry_type;
               MENU_ENTRY_INIT(entry);
               entry.path_enabled       = false;
               entry.label_enabled      = false;
               entry.rich_label_enabled = false;
               entry.value_enabled      = false;
               entry.sublabel_enabled   = false;
               menu_entry_get(&entry, 0, selection, NULL, true);
               entry_type               = entry.type;

               if (  (entry_type == FILE_TYPE_IMAGEVIEWER) ||
                     (entry_type == FILE_TYPE_IMAGE))
               {
                  xmb_set_thumbnail_content(xmb, "imageviewer");
                  update_thumbnails = true;
               }
               else
               {
                  /* If this is a file list and current
                   * entry is not an image, have to 'reset'
                   * content + right/left thumbnails
                   * (otherwise last loaded thumbnail will
                   * persist, and be shown on the wrong entry) */
                  gfx_thumbnail_set_content(xmb->thumbnail_path_data, NULL);
                  gfx_thumbnail_cancel_pending_requests();
                  gfx_thumbnail_reset(&xmb->thumbnails.right);
                  gfx_thumbnail_reset(&xmb->thumbnails.left);
               }
            }

            if (update_thumbnails)
               xmb_update_thumbnail_image(xmb);
         }

         xmb_update_savestate_thumbnail_path(xmb, i);
         xmb_update_savestate_thumbnail_image(xmb);
      }

      if (     (!allow_animations)
            || (real_iy < -threshold
               || real_iy > height+threshold))
      {
         node->alpha = node->label_alpha = ia;
         node->y     = iy;
         node->zoom  = iz;
      }
      else
      {
         settings_t                     *settings = config_get_ptr();
         unsigned menu_xmb_animation_move_up_down = settings->uints.menu_xmb_animation_move_up_down;

         /* Move up/down animation */
         gfx_animation_ctx_entry_t anim_entry;

         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.tag          = tag;
         anim_entry.cb           = NULL;

         switch (menu_xmb_animation_move_up_down)
         {
            case 0:
               anim_entry.duration     = XMB_DELAY;
               anim_entry.easing_enum  = EASING_OUT_QUAD;
               break;
            case 1:
               anim_entry.duration     = XMB_DELAY * 4;
               anim_entry.easing_enum  = EASING_OUT_EXPO;
               break;
         }

         gfx_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         gfx_animation_push(&anim_entry);

         anim_entry.target_value = iz;
         anim_entry.subject      = &node->zoom;

         gfx_animation_push(&anim_entry);

         anim_entry.target_value = iy;
         anim_entry.subject      = &node->y;

         gfx_animation_push(&anim_entry);
      }
   }
}

static void xmb_list_open_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height = 0;
   int        threshold = xmb->icon_size * 10;
   size_t           end = list ? list->size : 0;

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      if (i == current)
         ia = xmb->items_active_alpha;
      if (dir == -1)
         ia = 0;

      real_y = node->y + xmb->margins_screen_top;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = ia;
         node->label_alpha = 0;
         node->x = xmb->icon_size * dir * -2;
      }
      else
      {
         gfx_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = XMB_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         gfx_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->label_alpha;

         gfx_animation_push(&anim_entry);

         anim_entry.target_value = xmb->icon_size * dir * -2;
         anim_entry.subject      = &node->x;

         gfx_animation_push(&anim_entry);
      }
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   unsigned xmb_system_tab = 0;
   size_t skip             = 0;
   int        threshold    = xmb->icon_size * 10;
   size_t end              = list ? list->size : 0;

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      if (dir == 1)
      {
         node->alpha       = 0;
         node->label_alpha = 0;
      }
      else if (dir == -1)
      {
         if (i != current)
            node->alpha    = 0;
         node->label_alpha = 0;
      }

      node->x        = xmb->icon_size * dir * 2;
      node->y        = xmb_item_y(xmb, i, current);
      node->zoom     = xmb->categories_passive_zoom;

      real_y         = node->y + xmb->margins_screen_top;

      if (i == current)
      {
         node->zoom  = xmb->categories_active_zoom;
         ia          = xmb->items_active_alpha;
      }
      else
         ia          = xmb->items_passive_alpha;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x     = 0;
      }
      else
      {
         gfx_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = XMB_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         gfx_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         gfx_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->x;

         gfx_animation_push(&anim_entry);
      }
   }

   xmb->old_depth = xmb->depth;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &skip);

   xmb_system_tab = xmb_get_system_tab(xmb,
         (unsigned)xmb->categories_selection_ptr);

   if (xmb_system_tab <= XMB_SYSTEM_TAB_SETTINGS)
   {
      if (  gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
            gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         /* This code is horrible, full of hacks...
          * This hack ensures that thumbnails are not cleared
          * when selecting an entry from a collection via
          * 'load content'... */
         if (xmb->depth != 5)
            xmb_unload_thumbnail_textures(xmb);

         if (xmb->is_playlist || xmb->is_db_manager_list)
         {
            xmb_set_thumbnail_content(xmb, NULL);
            xmb_update_thumbnail_image(xmb);
         }
      }
   }
}

static xmb_node_t *xmb_node_allocate_userdata(
      xmb_handle_t *xmb, unsigned i)
{
   xmb_node_t *tmp  = NULL;
   xmb_node_t *node = xmb_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node->alpha = xmb->categories_passive_alpha;
   node->zoom  = xmb->categories_passive_zoom;

   if ((i + xmb->system_tab_end) == xmb->categories_active_idx)
   {
      node->alpha = xmb->categories_active_alpha;
      node->zoom  = xmb->categories_active_zoom;
   }

   tmp = (xmb_node_t*)file_list_get_userdata_at_offset(
         &xmb->horizontal_list, i);
   xmb_free_node(tmp);

   xmb->horizontal_list.list[i].userdata = node;

   return node;
}

static xmb_node_t* xmb_get_userdata_from_horizontal_list(
      xmb_handle_t *xmb, unsigned i)
{
   return (xmb_node_t*)
      file_list_get_userdata_at_offset(&xmb->horizontal_list, i);
}

static void xmb_push_animations(xmb_node_t *node,
      uintptr_t tag, float ia, float ix)
{
   gfx_animation_ctx_entry_t anim_entry;

   anim_entry.duration     = XMB_DELAY;
   anim_entry.target_value = ia;
   anim_entry.subject      = &node->alpha;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   anim_entry.tag          = tag;
   anim_entry.cb           = NULL;

   gfx_animation_push(&anim_entry);

   anim_entry.subject      = &node->label_alpha;

   gfx_animation_push(&anim_entry);

   anim_entry.target_value = ix;
   anim_entry.subject      = &node->x;

   gfx_animation_push(&anim_entry);
}

static void xmb_list_switch_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   size_t end          = list ? list->size : 0;
   float ix            = -xmb->icon_spacing_horizontal * dir;
   float ia            = 0;
   unsigned first      = 0;
   unsigned last       = (unsigned)(end > 0 ? end - 1 : 0);

   video_driver_get_size(NULL, &height);
   xmb_calculate_visible_range(xmb, height, end,
         (unsigned)current, &first, &last);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      if (i >= first && i <= last)
         xmb_push_animations(node, (uintptr_t)list, ia, ix);
      else
      {
         node->alpha = node->label_alpha = ia;
         node->x     = ix;
      }
   }
}

static void xmb_list_switch_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, first, last, height;
   size_t end                         = 0;

   end   = list ? list->size : 0;

   first = 0;
   last  = (unsigned)(end > 0 ? end - 1 : 0);

   video_driver_get_size(NULL, &height);
   xmb_calculate_visible_range(xmb, height,
         end, (unsigned)current, &first, &last);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)list->list[i].userdata;
      float ia         = xmb->items_passive_alpha;

      if (!node)
         continue;

      node->x           = xmb->icon_spacing_horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = xmb->items_active_alpha;

      if (i >= first && i <= last)
         xmb_push_animations(node, (uintptr_t)list, ia, 0);
      else
      {
         node->x     = 0;
         node->alpha = node->label_alpha = ia;
      }
   }
}

static void xmb_set_title(xmb_handle_t *xmb)
{
   if (xmb->categories_selection_ptr <= xmb->system_tab_end)
      menu_entries_get_title(xmb->title_name, sizeof(xmb->title_name));
   else
   {
      const char *path = xmb->horizontal_list.list[
            xmb->categories_selection_ptr - (xmb->system_tab_end + 1)].path;

      if (!path)
         return;

      fill_pathname_base_noext(
            xmb->title_name, path, sizeof(xmb->title_name));

      /* Add current search terms */
      menu_entries_search_append_terms_string(
            xmb->title_name, sizeof(xmb->title_name));
   }
}

static xmb_node_t* xmb_get_node(xmb_handle_t *xmb, unsigned i)
{
   switch (xmb_get_system_tab(xmb, i))
   {
      case XMB_SYSTEM_TAB_SETTINGS:
         return &xmb->settings_tab_node;
#ifdef HAVE_IMAGEVIEWER
      case XMB_SYSTEM_TAB_IMAGES:
         return &xmb->images_tab_node;
#endif
      case XMB_SYSTEM_TAB_MUSIC:
         return &xmb->music_tab_node;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case XMB_SYSTEM_TAB_VIDEO:
         return &xmb->video_tab_node;
#endif
      case XMB_SYSTEM_TAB_HISTORY:
         return &xmb->history_tab_node;
      case XMB_SYSTEM_TAB_FAVORITES:
         return &xmb->favorites_tab_node;
#ifdef HAVE_NETWORKING
      case XMB_SYSTEM_TAB_NETPLAY:
         return &xmb->netplay_tab_node;
#endif
      case XMB_SYSTEM_TAB_ADD:
         return &xmb->add_tab_node;
#if defined(HAVE_LIBRETRODB)
      case XMB_SYSTEM_TAB_EXPLORE:
         return &xmb->explore_tab_node;
#endif
      case XMB_SYSTEM_TAB_CONTENTLESS_CORES:
         return &xmb->contentless_cores_tab_node;
      default:
         if (i > xmb->system_tab_end)
            return xmb_get_userdata_from_horizontal_list(
                  xmb, i - (xmb->system_tab_end + 1));
   }

   return &xmb->main_menu_node;
}

static void xmb_list_switch_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   settings_t *settings = config_get_ptr();
   size_t list_size     = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;
   unsigned xmb_animation_horizontal_highlight =
      settings->uints.menu_xmb_animation_horizontal_highlight;

   for (j = 0; j <= list_size; j++)
   {
      gfx_animation_ctx_entry_t entry;
      float ia                    = xmb->categories_passive_alpha;
      float iz                    = xmb->categories_passive_zoom;
      xmb_node_t *node            = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories_active_idx)
      {
         ia = xmb->categories_active_alpha;
         iz = xmb->categories_active_zoom;
      }

      /* Horizontal icon animation */

      entry.target_value = ia;
      entry.subject      = &node->alpha;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      entry.tag          = -1;
      entry.cb           = NULL;

      switch (xmb_animation_horizontal_highlight)
      {
         case 0:
            entry.duration     = XMB_DELAY;
            entry.easing_enum  = EASING_OUT_QUAD;
            break;
         case 1:
            entry.duration     = XMB_DELAY + (XMB_DELAY / 2);
            entry.easing_enum  = EASING_IN_SINE;
            break;
         case 2:
            entry.duration     = XMB_DELAY * 2;
            entry.easing_enum  = EASING_OUT_BOUNCE;
            break;
      }

      gfx_animation_push(&entry);

      entry.target_value = iz;
      entry.subject      = &node->zoom;

      gfx_animation_push(&entry);
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   gfx_animation_ctx_entry_t anim_entry;
   int dir                        = -1;
   file_list_t *selection_buf     = menu_entries_get_selection_buf_ptr(0);
   size_t selection               = menu_navigation_get_selection();
   settings_t       *settings     = config_get_ptr();
   bool menu_horizontal_animation = settings->bools.menu_horizontal_animation;

   if (xmb->categories_selection_ptr > xmb->categories_selection_ptr_old)
      dir = 1;

   xmb->categories_active_idx += dir;

   xmb_list_switch_horizontal_list(xmb);

   anim_entry.duration     = XMB_DELAY;
   anim_entry.target_value = xmb->icon_spacing_horizontal
      * -(float)xmb->categories_selection_ptr;
   anim_entry.subject      = &xmb->categories_x_pos;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   anim_entry.tag          = -1;
   anim_entry.cb           = NULL;

   if (anim_entry.subject)
      gfx_animation_push(&anim_entry);

   dir = -1;
   if (xmb->categories_selection_ptr > xmb->categories_selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, &xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);

   /* Check if we are to have horizontal animations. */
   if (menu_horizontal_animation)
      xmb_list_switch_new(xmb, selection_buf, dir, selection);

   xmb->categories_active_idx_old = (unsigned)xmb->categories_selection_ptr;

   if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
       gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
   {
      xmb_unload_thumbnail_textures(xmb);

      if (xmb->is_playlist)
      {
         xmb_set_thumbnail_content(xmb, NULL);
         xmb_update_thumbnail_image(xmb);
      }
   }
}

static void xmb_list_open_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      gfx_animation_ctx_entry_t anim_entry;
      float ia          = 0;
      xmb_node_t *node  = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories_active_idx)
         ia = xmb->categories_active_alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories_passive_alpha;

      anim_entry.duration     = XMB_DELAY;
      anim_entry.target_value = ia;
      anim_entry.subject      = &node->alpha;
      anim_entry.easing_enum  = EASING_OUT_QUAD;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      anim_entry.tag          = -1;
      anim_entry.cb           = NULL;

      if (anim_entry.subject)
         gfx_animation_push(&anim_entry);
   }
}

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      xmb_node_t *node = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         continue;
      if (!(path = xmb->horizontal_list.list[i].path))
         continue;
      if (string_ends_with_size(path, ".lpl",
               strlen(path), STRLEN_CONST(".lpl")))
      {
         video_driver_texture_unload(&node->icon);
         video_driver_texture_unload(&node->content_icon);
      }
   }
}

static void xmb_init_horizontal_list(xmb_handle_t *xmb)
{
   menu_displaylist_info_t info;
   settings_t *settings             = config_get_ptr();
   const char *dir_playlist         = settings->paths.directory_playlist;
   bool menu_content_show_playlists = settings->bools.menu_content_show_playlists;

   menu_displaylist_info_init(&info);

   info.list                        = &xmb->horizontal_list;
   info.path                        = strdup(dir_playlist);
   info.label                       = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
   info.exts                    = strdup("lpl");
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_PLAYLISTS_TAB;

   if (menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info,
               settings))
      {
         size_t i;
         for (i = 0; i < xmb->horizontal_list.size; i++)
            xmb_node_allocate_userdata(xmb, (unsigned)i);
         menu_displaylist_process(&info);
      }
   }

   menu_displaylist_info_free(&info);
}

static void xmb_toggle_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

      if (!node)
         continue;

      node->alpha = 0;
      node->zoom  = xmb->categories_passive_zoom;

      if (i == xmb->categories_active_idx)
      {
         node->alpha = xmb->categories_active_alpha;
         node->zoom  = xmb->categories_active_zoom;
      }
      else if (xmb->depth <= 1)
         node->alpha = xmb->categories_passive_alpha;
   }
}

static void xmb_context_reset_horizontal_list(
      xmb_handle_t *xmb)
{
   unsigned i;
   int depth; /* keep this integer */
   size_t list_size                =
      xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   xmb->categories_x_pos           =
      xmb->icon_spacing_horizontal *
      -(float)xmb->categories_selection_ptr;

   depth                           = (xmb->depth > 1) ? 2 : 1;
   xmb->x                          = xmb->icon_size * -(depth*2-2);

   RHMAP_FREE(xmb->playlist_db_node_map);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      xmb_node_t *node =
         xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         if (!(node = xmb_node_allocate_userdata(xmb, i)))
            continue;

      if (!(path = xmb->horizontal_list.list[i].path))
         continue;

      if (string_ends_with_size(path, ".lpl",
               strlen(path), STRLEN_CONST(".lpl")))
      {
         struct texture_image ti;
         char sysname[PATH_MAX_LENGTH];
         char iconpath[PATH_MAX_LENGTH];
         char texturepath[PATH_MAX_LENGTH];
         char content_texturepath[PATH_MAX_LENGTH];

         /* Add current node to playlist database name map */
         RHMAP_SET_STR(xmb->playlist_db_node_map, path, node);

         iconpath[0]       = sysname[0]             =
            texturepath[0] = content_texturepath[0] = '\0';

         fill_pathname_base_noext(sysname, path, sizeof(sysname));
         fill_pathname_application_special(iconpath, sizeof(iconpath),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

         fill_pathname_join_concat(texturepath, iconpath, sysname,
               FILE_PATH_PNG_EXTENSION, sizeof(texturepath));

         /* If the playlist icon doesn't exist return default */

         if (!path_is_valid(texturepath))
               fill_pathname_join_concat(texturepath, iconpath, "default",
               FILE_PATH_PNG_EXTENSION, sizeof(texturepath));

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
               FILE_PATH_CONTENT_BASENAME, '-',
               sizeof(sysname));
         strlcat(content_texturepath, iconpath, sizeof(content_texturepath));
         strlcat(content_texturepath, sysname,  sizeof(content_texturepath));

         /* If the content icon doesn't exist return default-content */

         if (!path_is_valid(content_texturepath))
         {
            strlcat(iconpath, "default", sizeof(iconpath));
            fill_pathname_join_delim(content_texturepath, iconpath,
                  FILE_PATH_CONTENT_BASENAME, '-', sizeof(content_texturepath));
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
      }
   }

   xmb_toggle_horizontal_list(xmb);
}

static void xmb_refresh_horizontal_list(xmb_handle_t *xmb)
{
   xmb_context_destroy_horizontal_list(xmb);

   xmb_free_list_nodes(&xmb->horizontal_list, false);
   file_list_deinitialize(&xmb->horizontal_list);
   RHMAP_FREE(xmb->playlist_db_node_map);

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   xmb_init_horizontal_list(xmb);

   xmb_context_reset_horizontal_list(xmb);
}

static int xmb_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   xmb_handle_t *xmb        = (xmb_handle_t*)userdata;

   if (!xmb)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         xmb->show_mouse = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         xmb->show_mouse = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         xmb_refresh_horizontal_list(xmb);
         break;
      case MENU_ENVIRON_ENABLE_SCREENSAVER:
         xmb->show_screensaver = true;
         break;
      case MENU_ENVIRON_DISABLE_SCREENSAVER:
         xmb->show_screensaver = false;
         break;
      default:
         return -1;
   }

   return 0;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   gfx_animation_ctx_entry_t entry;

   settings_t *settings       = config_get_ptr();
   unsigned
      menu_xmb_animation_opening_main_menu =
      settings->uints.menu_xmb_animation_opening_main_menu;
   int                    dir = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   xmb->depth = (int)xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;
   else
      return; /* If menu hasn't changed, do nothing */

   xmb_list_open_horizontal_list(xmb);

   xmb_list_open_old(xmb, &xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);

   xmb_list_open_new(xmb, selection_buf,
         dir, selection);

   /* Main Menu opening animation */

   entry.target_value = xmb->icon_size * -(xmb->depth*2-2);
   entry.subject      = &xmb->x;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   switch (menu_xmb_animation_opening_main_menu)
   {
      case 0:
         entry.easing_enum  = EASING_OUT_QUAD;
         entry.duration     = XMB_DELAY;
         break;
      case 1:
         entry.easing_enum  = EASING_OUT_CIRC;
         entry.duration     = XMB_DELAY * 2;
         break;
      case 2:
         entry.easing_enum  = EASING_OUT_EXPO;
         entry.duration     = XMB_DELAY * 3;
         break;
      case 3:
         entry.easing_enum  = EASING_OUT_BOUNCE;
         entry.duration     = XMB_DELAY * 4;
         break;
   }

   switch (xmb->depth)
   {
      case 1:
      case 2:
         gfx_animation_push(&entry);

         entry.target_value = xmb->depth - 1;
         entry.subject      = &xmb->textures_arrow_alpha;

         gfx_animation_push(&entry);
         break;
   }

   xmb->old_depth = xmb->depth;
}

/* Is called whenever the list/sub-list changes */
static void xmb_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb    = (xmb_handle_t*)data;
   settings_t *settings = config_get_ptr();
   bool show_entry_idx  = settings ? settings->bools.playlist_show_entry_idx : false;
   unsigned    depth    = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
   unsigned xmb_system_tab;

   if (!xmb)
      return;

   /* Determine whether this is a playlist */
   xmb_system_tab   = xmb_get_system_tab(xmb,
         (unsigned)xmb->categories_selection_ptr);
   xmb->is_playlist = (depth == 1
                      && ((xmb_system_tab == XMB_SYSTEM_TAB_FAVORITES)
                      || (xmb_system_tab == XMB_SYSTEM_TAB_HISTORY)
#ifdef HAVE_IMAGEVIEWER
                      || (xmb_system_tab == XMB_SYSTEM_TAB_IMAGES)
#endif
                      || (xmb_system_tab == XMB_SYSTEM_TAB_MUSIC)
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
                      || (xmb_system_tab == XMB_SYSTEM_TAB_VIDEO)
#endif
                      ))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST))
                      || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST));
   xmb->is_playlist = xmb->is_playlist && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL));

   /* Determine whether this is a database manager list */
   xmb->is_db_manager_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST));

   /* Determine whether this is the contentless cores menu */
   xmb->is_contentless_cores = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)) ||
                               string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST));

   /* Determine whether this is a 'file list'
    * (needed for handling thumbnails when viewing images
    * via 'load content')
    * > Note: MENU_ENUM_LABEL_FAVORITES is always set
    *   as the 'label' when navigating directories after
    *   selecting 'load content' */
   xmb->is_file_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES));

   /* Determine whether this is the quick menu */
   xmb->is_quick_menu = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS)) ||
                        string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS));

   xmb_set_title(xmb);
   xmb_update_dynamic_wallpaper(xmb);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      xmb_selection_pointer_changed(xmb, false);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      return;
   }

   if (xmb->categories_selection_ptr != xmb->categories_active_idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);

   /* Determine whether to show entry index */
   xmb->entry_idx_enabled = show_entry_idx && xmb->is_playlist;

   /* Update list size & entry index texts */
   if (xmb->entry_idx_enabled)
   {
      xmb->list_size = menu_entries_get_size();
      snprintf(xmb->entry_index_str, sizeof(xmb->entry_index_str),
      "%lu/%lu", (unsigned long)menu_navigation_get_selection() + 1,
                 (unsigned long)xmb->list_size);
   }

   /* By default, fullscreen thumbnails are only
    * enabled on playlists, database manager
    * lists and file lists, in cases where ordinary
    * thumbnails would normally be shown
    * > This is refined on a case-by-case basis
    *   inside xmb_set_thumbnail_content() */
   xmb->fullscreen_thumbnails_available =
         (xmb->is_playlist || xmb->is_db_manager_list || xmb->is_file_list) &&
         !xmb->is_quick_menu &&
         !((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS) && (xmb->depth > 2));

   /* Hack: XMB gets into complete muddle when
    * performing 'complex' directory navigation
    * via 'load content'. We have to work around
    * this by resetting thumbnails whenever a
    * file list is populated... */
   if (xmb->is_file_list)
   {
      gfx_thumbnail_set_content(xmb->thumbnail_path_data, NULL);
      gfx_thumbnail_cancel_pending_requests();
      gfx_thumbnail_reset(&xmb->thumbnails.right);
      gfx_thumbnail_reset(&xmb->thumbnails.left);
   }
}

static uintptr_t xmb_icon_get_id(xmb_handle_t *xmb,
      xmb_node_t *core_node, xmb_node_t *node,
      enum msg_hash_enums enum_idx, const char *enum_path,
      const char *enum_label, unsigned type, bool active,
      bool checked)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST:
      case MENU_ENUM_LABEL_REMAP_FILE_MANAGER_LIST:
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return xmb->textures.list[XMB_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
         return xmb->textures.list[XMB_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_TRAY_EJECT:
      case MENU_ENUM_LABEL_DISK_TRAY_INSERT:
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
      case MENU_ENUM_LABEL_DISK_INDEX:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_SAVE_STATE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
      case MENU_ENUM_LABEL_CORE_CREATE_BACKUP:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
      case MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
      case MENU_ENUM_LABEL_REBOOT:
      case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
      case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
      case MENU_ENUM_LABEL_FAVORITES: /* "Start Directory" */
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
         return xmb->textures.list[XMB_TEXTURE_ADD];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return xmb->textures.list[XMB_TEXTURE_RDB];

      /* Menu collection submenus */
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return xmb->textures.list[XMB_TEXTURE_ZIP];
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
         return xmb->textures.list[XMB_TEXTURE_FAVORITE];
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC];
      case MENU_ENUM_LABEL_GOTO_EXPLORE:
         return xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES:
         return xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_LOAD_DISC:
      case MENU_ENUM_LABEL_DUMP_DISC:
#ifdef HAVE_LAKKA
      case MENU_ENUM_LABEL_EJECT_DISC:
#endif
      case MENU_ENUM_LABEL_DISC_INFORMATION:
         return xmb->textures.list[XMB_TEXTURE_DISC];

      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
         return xmb->textures.list[XMB_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
         return xmb->textures.list[XMB_TEXTURE_RUN];
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
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_FILE:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_UPDATER];
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
         return xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
#ifdef HAVE_VIDEO_LAYOUT
      case MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS:
#endif
         return xmb->textures.list[XMB_TEXTURE_OVERLAY];
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_INFORMATION:
      case MENU_ENUM_LABEL_INFORMATION_LIST:
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
         return xmb->textures.list[XMB_TEXTURE_INFO];
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         return xmb->textures.list[XMB_TEXTURE_CURSOR];
      case MENU_ENUM_LABEL_HELP_LIST:
      case MENU_ENUM_LABEL_HELP_CONTROLS:
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
      case MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO:
         return xmb->textures.list[XMB_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         return xmb->textures.list[XMB_TEXTURE_EXIT];
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_DRIVERS];
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_VIDEO];
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_AUDIO];
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_MIXER];
      case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
         return xmb->textures.list[XMB_TEXTURE_SUBSETTING];
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
         return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];
      case MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_TURBO];
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         return xmb->textures.list[XMB_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_FRAMESKIP];
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_RECORD];
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
         return xmb->textures.list[XMB_TEXTURE_STREAM];
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
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_CORE_OPTIONS_RESET:
      case MENU_ENUM_LABEL_REMAP_FILE_RESET:
         return xmb->textures.list[XMB_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case MENU_ENUM_LABEL_CORE_LOCK:
      case MENU_ENUM_LABEL_CORE_SET_STANDALONE_EXEMPT:
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_UI];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
         return xmb->textures.list[XMB_TEXTURE_POWER];
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_USER];
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_PRIVACY];
      case MENU_ENUM_LABEL_REWIND_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_REWIND];
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_OVERRIDE];
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_NOTIFICATIONS];
#ifdef HAVE_NETWORKING
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
         return xmb->textures.list[XMB_TEXTURE_ROOM];
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
#ifdef HAVE_NETPLAYDISCOVERY
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
#endif
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
      case MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_NETWORK];
#endif
      case MENU_ENUM_LABEL_BLUETOOTH_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_BLUETOOTH];
      case MENU_ENUM_LABEL_SHUTDOWN:
         return xmb->textures.list[XMB_TEXTURE_SHUTDOWN];
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return xmb->textures.list[XMB_TEXTURE_CHECKMARK];
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         return xmb->textures.list[XMB_TEXTURE_MENU_ADD];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
         return xmb->textures.list[XMB_TEXTURE_MENU_APPLY_TOGGLE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return xmb->textures.list[XMB_TEXTURE_MENU_APPLY_COG];
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
#ifdef HAVE_LIBRETRODB
      case MENU_ENUM_LABEL_EXPLORE_ITEM:
      {
         uintptr_t icon = menu_explore_get_entry_icon(type);
         if (icon)
            return icon;
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
      default:
         break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case FILE_TYPE_RPL_ENTRY:
         if (core_node)
            return core_node->content_icon;

         switch (xmb_get_system_tab(xmb,
                  (unsigned)xmb->categories_selection_ptr))
         {
            case XMB_SYSTEM_TAB_FAVORITES:
               return xmb->textures.list[XMB_TEXTURE_FAVORITE];
            case XMB_SYSTEM_TAB_MUSIC:
               return xmb->textures.list[XMB_TEXTURE_MUSIC];
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               return xmb->textures.list[XMB_TEXTURE_IMAGE];
#endif
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            case XMB_SYSTEM_TAB_VIDEO:
               return xmb->textures.list[XMB_TEXTURE_MOVIE];
#endif
            default:
               break;
         }
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case MENU_SET_CDROM_INFO:
      case MENU_SET_CDROM_LIST:
      case MENU_SET_LOAD_CDROM_LIST:
         return xmb->textures.list[XMB_TEXTURE_DISC];
      case FILE_TYPE_SHADER:
      case FILE_TYPE_SHADER_PRESET:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case FILE_TYPE_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case FILE_TYPE_RDB:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return xmb->textures.list[XMB_TEXTURE_CURSOR];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_SETTING_ACTION_CLOSE:
      case MENU_SETTING_ACTION_CLOSE_HORIZONTAL:
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case FILE_TYPE_RDB_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS)))
            return xmb->textures.list[XMB_TEXTURE_VIDEO];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS)))
            return xmb->textures.list[XMB_TEXTURE_AUDIO];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)))
            return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];
         else if (string_is_equal(enum_path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS)))
            return xmb->textures.list[XMB_TEXTURE_OSD];
         else if (string_is_equal(enum_path, "Media"))
            return xmb->textures.list[XMB_TEXTURE_RDB];
         else if (string_is_equal(enum_path, "System"))
            return xmb->textures.list[XMB_TEXTURE_DRIVERS];
         else
            return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
         break;
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_RESET:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS:
         return xmb->textures.list[XMB_TEXTURE_PAUSE];
      case MENU_SET_SCREEN_BRIGHTNESS:
         return xmb->textures.list[XMB_TEXTURE_BRIGHTNESS];
      case MENU_SETTING_GROUP:
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_INFO_MESSAGE:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_BLUETOOTH:
         return xmb->textures.list[XMB_TEXTURE_BLUETOOTH];
      case MENU_WIFI:
         return xmb->textures.list[XMB_TEXTURE_WIFI];
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return xmb->textures.list[XMB_TEXTURE_ROOM];
      case MENU_ROOM_LAN:
         return xmb->textures.list[XMB_TEXTURE_ROOM_LAN];
      case MENU_ROOM_RELAY:
         return xmb->textures.list[XMB_TEXTURE_ROOM_RELAY];
#endif
      case MENU_SETTINGS_INPUT_LIBRETRO_DEVICE:
      case MENU_SETTINGS_INPUT_INPUT_REMAP_PORT:
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_SETTINGS_INPUT_ANALOG_DPAD_MODE:
         return xmb->textures.list[XMB_TEXTURE_INPUT_ADC];
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      char buffer[64];
      int index = type - MENU_SETTINGS_CHEEVOS_START;
      uintptr_t badge_texture = rcheevos_menu_get_badge_texture(index);
      if (badge_texture)
         return badge_texture;

      /* no state means its a header - show the info icon */
      if (!rcheevos_menu_get_state(index, buffer, sizeof(buffer)))
         return xmb->textures.list[XMB_TEXTURE_INFO];

      /* placeholder badge image was not found, show generic menu icon */
      return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENTS];
   }
#endif

   if (
         (type >= MENU_SETTINGS_INPUT_BEGIN) &&
         (type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
      )
      {
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         /* Input User # Binds only */
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if (type == input_id)
               return xmb->textures.list[XMB_TEXTURE_INPUT_ADC];
#ifdef HAVE_LIBNX
            /* account for the additional split joycon option in Input # Binds */
            input_id++;
#endif
            if (type == input_id + 1)
               return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];
            if (type == input_id + 2)
               return xmb->textures.list[XMB_TEXTURE_INPUT_MOUSE];
            if (type == input_id + 3)
               return xmb->textures.list[XMB_TEXTURE_INPUT_BIND_ALL];
            if (type == input_id + 4)
               return xmb->textures.list[XMB_TEXTURE_RELOAD];
            if (type == input_id + 5)
               return xmb->textures.list[XMB_TEXTURE_SAVING];
            if ((type > (input_id + 29)) && (type < (input_id + 41)))
               return xmb->textures.list[XMB_TEXTURE_INPUT_LGUN];
            if (type == input_id + 41)
               return xmb->textures.list[XMB_TEXTURE_INPUT_TURBO];
            /* align to use the same code of Quickmenu controls */
            input_id = input_id + 6;
         }
         else
         {
            /* Quickmenu controls repeats the same icons for all users */
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

         /* This is utilized for both Input # Binds and Quickmenu controls */
         if (type == input_id)
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_U];
         if (type == (input_id + 1))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_D];
         if (type == (input_id + 2))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_L];
         if (type == (input_id + 3))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_R];
         if (type == (input_id + 4))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_R];
         if (type == (input_id + 5))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_D];
         if (type == (input_id + 6))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_U];
         if (type == (input_id + 7))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_L];
         if (type == (input_id + 8))
            return xmb->textures.list[XMB_TEXTURE_INPUT_SELECT];
         if (type == (input_id + 9))
            return xmb->textures.list[XMB_TEXTURE_INPUT_START];
         if (type == (input_id + 10))
            return xmb->textures.list[XMB_TEXTURE_INPUT_LB];
         if (type == (input_id + 11))
            return xmb->textures.list[XMB_TEXTURE_INPUT_RB];
         if (type == (input_id + 12))
            return xmb->textures.list[XMB_TEXTURE_INPUT_LT];
         if (type == (input_id + 13))
            return xmb->textures.list[XMB_TEXTURE_INPUT_RT];
         if (type == (input_id + 14))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_P];
         if (type == (input_id + 15))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_P];
         if (type == (input_id + 16))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_U];
         if (type == (input_id + 17))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_D];
         if (type == (input_id + 18))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_L];
         if (type == (input_id + 19))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_R];
         if (type == (input_id + 20))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_U];
         if (type == (input_id + 21))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_D];
         if (type == (input_id + 22))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_L];
         if (type == (input_id + 23))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_R];
      }

   if (
         (type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN) &&
         (type <= MENU_SETTINGS_REMAPPING_PORT_END)
      )
      return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];

   if (checked)
      return xmb->textures.list[XMB_TEXTURE_CHECKMARK];

   if (type == MENU_SETTING_ACTION)
      return xmb->textures.list[XMB_TEXTURE_SETTING];

   return xmb->textures.list[XMB_TEXTURE_SUBSETTING];

}

static int xmb_draw_item(
      void *userdata,
      gfx_display_t   *p_disp,
      gfx_animation_t *p_anim,
      gfx_display_ctx_driver_t *dispctx,
      settings_t *settings,
      unsigned video_width,
      unsigned video_height,
      bool xmb_shadows_enable,
      math_matrix_4x4 *mymat,
      xmb_handle_t *xmb,
      xmb_node_t *core_node,
      file_list_t *list,
      float *color,
      size_t i,
      size_t current,
      unsigned width,
      unsigned height
      )
{
   menu_entry_t entry;
   float icon_x, icon_y, label_offset;
   gfx_animation_ctx_ticker_t ticker;
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;
   char tmp[255];
   unsigned ticker_x_offset            = 0;
   const char *ticker_str              = NULL;
   unsigned entry_type                 = 0;
   const float half_size               = xmb->icon_size / 2.0f;
   uintptr_t texture_switch            = 0;
   bool do_draw_text                   = false;
   unsigned ticker_limit               = 35 * xmb_scale_mod[0];
   unsigned line_ticker_width          = 45 * xmb_scale_mod[3];
   xmb_node_t *   node                 = (xmb_node_t*)list->list[i].userdata;
   bool use_smooth_ticker              = settings->bools.menu_ticker_smooth;
   enum gfx_animation_ticker_type
      menu_ticker_type                 = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   unsigned xmb_thumbnail_scale_factor =
      settings->uints.menu_xmb_thumbnail_scale_factor;
   bool menu_xmb_vertical_thumbnails   = settings->bools.menu_xmb_vertical_thumbnails;
   bool menu_show_sublabels            = settings->bools.menu_show_sublabels;
   unsigned show_history_icons         = settings->uints.playlist_show_history_icons;
   unsigned menu_xmb_vertical_fade_factor
                                       = settings->uints.menu_xmb_vertical_fade_factor;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = p_anim->ticker_pixel_idx;
      ticker_smooth.font          = xmb->font;
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = menu_ticker_type;
      ticker_smooth.spacer        = NULL;
      ticker_smooth.x_offset      = &ticker_x_offset;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx       = p_anim->ticker_idx;
      ticker.type_enum = menu_ticker_type;
      ticker.spacer    = NULL;
   }

   if (!node)
      return 0;

   tmp[0] = '\0';

   icon_y = xmb->margins_screen_top + node->y + half_size;

   if (icon_y < half_size)
      return 0;

   if (icon_y > height + xmb->icon_size)
      return -1;

   icon_x = node->x + xmb->margins_screen_left +
      xmb->icon_spacing_horizontal - half_size;

   if (icon_x < -half_size || icon_x > width)
      return 0;

   MENU_ENTRY_INIT(entry);
   entry.label_enabled      = xmb->is_contentless_cores;
   entry.sublabel_enabled   = (i == current);
   menu_entry_get(&entry, 0, i, list, true);
   entry_type               = entry.type;

   if (entry_type == FILE_TYPE_CONTENTLIST_ENTRY)
   {
      char entry_path[PATH_MAX_LENGTH];
      entry_path[0] = '\0';
      strlcpy(entry_path, entry.path, sizeof(entry_path));

      fill_short_pathname_representation(entry_path, entry_path,
            sizeof(entry_path));

      if (!string_is_empty(entry_path))
         strlcpy(entry.path, entry_path, sizeof(entry.path));
   }

   if (string_is_equal(entry.value,
            msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(entry.value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF])
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF];
      else
         do_draw_text = true;
   }
   else if (string_is_equal(entry.value,
            msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(entry.value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON])
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON];
      else
         do_draw_text = true;
   }
   else
   {
      if (!string_is_empty(entry.value))
      {
         bool found = false;

         if (string_is_equal(entry.value, "..."))
            found = true;
         else if (string_starts_with_size(entry.value, "(", STRLEN_CONST("(")) &&
             string_ends_with  (entry.value, ")")
            )
         {
            if (
                  string_is_equal(entry.value, "(PRESET)") ||
                  string_is_equal(entry.value, "(SHADER)") ||
                  string_is_equal(entry.value, "(COMP)")   ||
                  string_is_equal(entry.value, "(CORE)")   ||
                  string_is_equal(entry.value, "(MOVIE)")  ||
                  string_is_equal(entry.value, "(MUSIC)")  ||
                  string_is_equal(entry.value, "(DIR)")    ||
                  string_is_equal(entry.value, "(RDB)")    ||
                  string_is_equal(entry.value, "(CURSOR)") ||
                  string_is_equal(entry.value, "(CFILE)")  ||
                  string_is_equal(entry.value, "(FILE)")   ||
                  string_is_equal(entry.value, "(IMAGE)")
               )
               found = true;
         }

         if (!found)
            do_draw_text = true;
      }
      else
         do_draw_text = true;

   }

   if (string_is_empty(entry.value))
   {
      if ((xmb->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE) ||
            !xmb->use_ps3_layout ||
            (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT)
             && ((xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE)
                  || (xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_PENDING))) ||
            (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT)
             && ((xmb->thumbnails.left.status == GFX_THUMBNAIL_STATUS_AVAILABLE)
                  || (xmb->thumbnails.left.status == GFX_THUMBNAIL_STATUS_PENDING))
             && menu_xmb_vertical_thumbnails)
         )
      {
         ticker_limit      = 40 * xmb_scale_mod[1];
         line_ticker_width = 50 * xmb_scale_mod[3];

         /* Can increase text length if thumbnail is downscaled */
         if (xmb_thumbnail_scale_factor < 100)
         {
            float ticker_scale_factor =
                  1.0f - ((float)xmb_thumbnail_scale_factor / 100.0f);

            ticker_limit +=
                  (unsigned)(ticker_scale_factor * 15.0f * xmb_scale_mod[1]);

            line_ticker_width +=
                  (unsigned)(ticker_scale_factor * 10.0f * xmb_scale_mod[3]);
         }
      }
      else
      {
         ticker_limit      = 70 * xmb_scale_mod[2];
         line_ticker_width = 60 * xmb_scale_mod[3];
      }
   }

   if (!string_is_empty(entry.rich_label))
      ticker_str               = entry.rich_label;
   else
      ticker_str               = entry.path;

   if (use_smooth_ticker)
   {
      ticker_smooth.selected    = (i == current);
      ticker_smooth.field_width = xmb->font_size * 0.5f * ticker_limit;
      ticker_smooth.src_str     = ticker_str;
      ticker_smooth.dst_str     = tmp;
      ticker_smooth.dst_str_len = sizeof(tmp);

      if (ticker_smooth.src_str)
         gfx_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = tmp;
      ticker.len      = ticker_limit;
      ticker.str      = ticker_str;
      ticker.selected = (i == current);

      if (ticker.str)
         gfx_animation_ticker(&ticker);
   }

   label_offset = xmb->margins_label_top;

   if (menu_xmb_vertical_fade_factor)
   {
      float min_alpha  = 0.1f;
      float max_alpha  = (i == current) ? xmb->items_active_alpha : xmb->items_passive_alpha;
      float new_alpha  = node->alpha;
      float icon_space = xmb->icon_spacing_vertical;
      float icon_ratio = icon_space / height / icon_space * 4;
      float scr_margin = xmb->margins_screen_top + (icon_space / icon_ratio / 400);
      float factor     = menu_xmb_vertical_fade_factor / 100.0f / icon_ratio;

      /* Top */
      if (i < current)
         new_alpha = (node->y + scr_margin) / factor;
      /* Bottom */
      else if (i > current)
         new_alpha = (height - node->y - scr_margin + icon_space) / factor;
      /* Rest need to reset after vertical wrap-around */
      else if (node->x == 0 && node->alpha > 0 && node->alpha != max_alpha)
         new_alpha = max_alpha;

      /* Limits */
      new_alpha = (new_alpha < min_alpha) ? min_alpha : new_alpha;
      new_alpha = (new_alpha > max_alpha) ? max_alpha : new_alpha;

      /* Horizontal animation requires breathing room on x-axis */
      if (new_alpha != node->alpha && node->x > (-icon_space * 2) && node->x < (icon_space * 2))
         node->alpha = node->label_alpha = new_alpha;
   }

   if (menu_show_sublabels)
   {
      if (i == current && width > 320 && height > 240
            && !string_is_empty(entry.sublabel))
      {
         char entry_sublabel[MENU_SUBLABEL_MAX_LENGTH];
         char entry_sublabel_top_fade[MENU_SUBLABEL_MAX_LENGTH >> 2];
         char entry_sublabel_bottom_fade[MENU_SUBLABEL_MAX_LENGTH >> 2];
         gfx_animation_ctx_line_ticker_t line_ticker;
         gfx_animation_ctx_line_ticker_smooth_t line_ticker_smooth;
         float ticker_y_offset             = 0.0f;
         float ticker_top_fade_y_offset    = 0.0f;
         float ticker_bottom_fade_y_offset = 0.0f;
         float ticker_top_fade_alpha       = 0.0f;
         float ticker_bottom_fade_alpha    = 0.0f;
         float sublabel_x                  = node->x + xmb->margins_screen_left +
               xmb->icon_spacing_horizontal + xmb->margins_label_left;
         float sublabel_y                  = xmb->margins_screen_top +
               node->y + (xmb->margins_label_top * 3.5f);

         entry_sublabel[0]             = '\0';
         entry_sublabel_top_fade[0]    = '\0';
         entry_sublabel_bottom_fade[0] = '\0';

         if (use_smooth_ticker)
         {
            line_ticker_smooth.fade_enabled         = true;
            line_ticker_smooth.type_enum            = menu_ticker_type;
            line_ticker_smooth.idx                  = p_anim->ticker_pixel_line_idx;

            line_ticker_smooth.font                 = xmb->font2;
            line_ticker_smooth.font_scale           = 1.0f;

            line_ticker_smooth.field_width          = (unsigned)(xmb->font2_size * 0.6f * line_ticker_width);
            /* The calculation here is incredibly obtuse. I think
             * this is correct... (c.f. xmb_item_y()) */
            line_ticker_smooth.field_height         = (unsigned)(
                  (xmb->icon_spacing_vertical * ((1 + xmb->under_item_offset) - xmb->active_item_factor)) -
                     (xmb->margins_label_top * 3.5f) -
                     xmb->under_item_offset); /* This last one is just a little extra padding (seems to help) */

            line_ticker_smooth.src_str              = entry.sublabel;
            line_ticker_smooth.dst_str              = entry_sublabel;
            line_ticker_smooth.dst_str_len          = sizeof(entry_sublabel);
            line_ticker_smooth.y_offset             = &ticker_y_offset;

            line_ticker_smooth.top_fade_str         = entry_sublabel_top_fade;
            line_ticker_smooth.top_fade_str_len     = sizeof(entry_sublabel_top_fade);
            line_ticker_smooth.top_fade_y_offset    = &ticker_top_fade_y_offset;
            line_ticker_smooth.top_fade_alpha       = &ticker_top_fade_alpha;

            line_ticker_smooth.bottom_fade_str      = entry_sublabel_bottom_fade;
            line_ticker_smooth.bottom_fade_str_len  = sizeof(entry_sublabel_bottom_fade);
            line_ticker_smooth.bottom_fade_y_offset = &ticker_bottom_fade_y_offset;
            line_ticker_smooth.bottom_fade_alpha    = &ticker_bottom_fade_alpha;

            gfx_animation_line_ticker_smooth(&line_ticker_smooth);
         }
         else
         {
            line_ticker.type_enum = menu_ticker_type;
            line_ticker.idx       = p_anim->ticker_idx;

            line_ticker.line_len  = (size_t)(line_ticker_width);
            /* Note: max_lines should be calculated at runtime,
             * but this is a nuisance. There is room for 4 lines
             * to be displayed when using all existing XMB themes,
             * so leave this value hard coded for now. */
            line_ticker.max_lines = 4;

            line_ticker.s         = entry_sublabel;
            line_ticker.len       = sizeof(entry_sublabel);
            line_ticker.str       = entry.sublabel;

            gfx_animation_line_ticker(&line_ticker);
         }

         label_offset = - xmb->margins_label_top;

         /* Draw sublabel */
         xmb_draw_text(xmb_shadows_enable, xmb, settings,
               entry_sublabel,
               sublabel_x, ticker_y_offset + sublabel_y,
               1, node->label_alpha, TEXT_ALIGN_LEFT,
               width, height, xmb->font2);

         /* Draw top/bottom line fade effect, if required */
         if (use_smooth_ticker)
         {
            if (!string_is_empty(entry_sublabel_top_fade) &&
                ticker_top_fade_alpha > 0.0f)
               xmb_draw_text(xmb_shadows_enable, xmb, settings,
                     entry_sublabel_top_fade,
                     sublabel_x, ticker_top_fade_y_offset + sublabel_y,
                     1, ticker_top_fade_alpha * node->label_alpha, TEXT_ALIGN_LEFT,
                     width, height, xmb->font2);

            if (!string_is_empty(entry_sublabel_bottom_fade) &&
                ticker_bottom_fade_alpha > 0.0f)
               xmb_draw_text(xmb_shadows_enable, xmb, settings,
                     entry_sublabel_bottom_fade,
                     sublabel_x, ticker_bottom_fade_y_offset + sublabel_y,
                     1, ticker_bottom_fade_alpha * node->label_alpha, TEXT_ALIGN_LEFT,
                     width, height, xmb->font2);
         }
      }
   }

   /* Draw entry index of current selection */
   if (i == current && xmb->entry_idx_enabled)
   {
      /* Calculate position depending on the current
       * list and if Thumbnail Vertical Disposition
       * is enabled (branchless version) */
      float x_position         = (video_width - xmb->margins_title_left) *
                                       !menu_xmb_vertical_thumbnails +
                                 (node->x + xmb->margins_screen_left +
                                 xmb->icon_spacing_horizontal -
                                 xmb->margins_label_left) *
                                       menu_xmb_vertical_thumbnails;
      float y_position         = (video_height - xmb->margins_title_bottom) *
                                       !menu_xmb_vertical_thumbnails +
                                 (xmb->margins_screen_top + xmb->margins_label_top +
                                 xmb->icon_spacing_vertical * xmb->active_item_factor) *
                                       menu_xmb_vertical_thumbnails;

      xmb_draw_text(xmb_shadows_enable, xmb, settings,
            xmb->entry_index_str, x_position, y_position,
            1, menu_xmb_vertical_thumbnails ? node->label_alpha : 1,
            TEXT_ALIGN_RIGHT, width, height, xmb->font);
   }

   xmb_draw_text(xmb_shadows_enable, xmb, settings, tmp,
         (float)ticker_x_offset + node->x + xmb->margins_screen_left +
         xmb->icon_spacing_horizontal + xmb->margins_label_left,
         xmb->margins_screen_top + node->y + label_offset,
         1, node->label_alpha, TEXT_ALIGN_LEFT,
         width, height, xmb->font);

   tmp[0]          = '\0';

   if (use_smooth_ticker)
   {
      ticker_smooth.selected    = (i == current);
      ticker_smooth.field_width = xmb->font_size * 0.5f * 35 * xmb_scale_mod[7];
      ticker_smooth.src_str     = entry.value;
      ticker_smooth.dst_str     = tmp;
      ticker_smooth.dst_str_len = sizeof(tmp);

      if (!string_is_empty(entry.value))
         gfx_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = tmp;
      ticker.len      = 35 * xmb_scale_mod[7];
      ticker.selected = (i == current);
      ticker.str      = entry.value;

      if (!string_is_empty(entry.value))
         gfx_animation_ticker(&ticker);
   }

   if (do_draw_text)
      xmb_draw_text(xmb_shadows_enable, xmb, settings, tmp,
            (float)ticker_x_offset + node->x +
            + xmb->margins_screen_left
            + xmb->icon_spacing_horizontal
            + xmb->margins_label_left
            + xmb->margins_setting_left,
            xmb->margins_screen_top + node->y + xmb->margins_label_top,
            1,
            node->label_alpha,
            TEXT_ALIGN_LEFT,
            width, height, xmb->font);

   gfx_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

   if (
         (!xmb->assets_missing) &&
         (color[3] != 0) &&
         (
            (entry.checked) ||
            !((entry_type >= MENU_SETTING_DROPDOWN_ITEM) && (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL))
         )
      )
   {
      math_matrix_4x4 mymat_tmp;
      gfx_display_ctx_rotate_draw_t rotate_draw;
      uintptr_t texture        = xmb_icon_get_id(xmb, core_node, node,
            entry.enum_idx, entry.path, entry.label,
            entry_type, (i == current), entry.checked);
      float x                  = icon_x;
      float y                  = icon_y;
      float scale_factor       = node->zoom;

      /* History/Favorite console specific content icons */
      if (  entry_type == FILE_TYPE_RPL_ENTRY
            && show_history_icons != PLAYLIST_SHOW_HISTORY_ICONS_DEFAULT)
      {
         switch (xmb_get_system_tab(xmb, xmb->categories_selection_ptr))
         {
            case XMB_SYSTEM_TAB_HISTORY:
            case XMB_SYSTEM_TAB_FAVORITES:
               {
                  const struct playlist_entry *pl_entry = NULL;
                  xmb_node_t *db_node                   = NULL;

                  playlist_get_index(playlist_get_cached(),
                        entry.entry_idx, &pl_entry);

                  if (pl_entry &&
                      !string_is_empty(pl_entry->db_name) &&
                      (db_node = RHMAP_GET_STR(xmb->playlist_db_node_map, pl_entry->db_name)))
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

      rotate_draw.matrix       = &mymat_tmp;
      rotate_draw.rotation     = 0;
      rotate_draw.scale_x      = scale_factor;
      rotate_draw.scale_y      = scale_factor;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

      xmb_draw_icon(
            userdata,
            p_disp,
            dispctx,
            video_width,
            video_height,
            xmb_shadows_enable,
            xmb->icon_size,
            texture,
            x,
            y,
            width,
            height,
            1.0,
            0, /* rotation */
            scale_factor,
            &color[0],
            xmb->shadow_offset,
            &mymat_tmp);
   }

   gfx_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

   if (texture_switch != 0 && color[3] != 0 && !xmb->assets_missing)
      xmb_draw_icon(
            userdata,
            p_disp,
            dispctx,
            video_width,
            video_height,
            xmb_shadows_enable,
            xmb->icon_size,
            texture_switch,
            node->x + xmb->margins_screen_left
            + xmb->icon_spacing_horizontal
            + xmb->icon_size / 2.0 + xmb->margins_setting_left,
            xmb->margins_screen_top + node->y + xmb->icon_size / 2.0,
            width, height,
            node->alpha,
            0,
            1,
            &color[0],
            xmb->shadow_offset,
            mymat);

   return 0;
}

static void xmb_draw_items(
      void *userdata,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      unsigned video_width,
      unsigned video_height,
      bool xmb_shadows_enable,
      xmb_handle_t *xmb,
      file_list_t *list,
      size_t current, size_t cat_selection_ptr, float *color,
      unsigned width, unsigned height,
      math_matrix_4x4 *mymat)
{
   size_t i;
   unsigned first, last;
   xmb_node_t *core_node             = NULL;
   size_t end                        = 0;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   if (!list || !list->size || !xmb)
      return;

   if (cat_selection_ptr > xmb->system_tab_end)
      core_node = xmb_get_userdata_from_horizontal_list(
            xmb, (unsigned)(cat_selection_ptr - (xmb->system_tab_end + 1)));

   end                      = list ? list->size : 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (list == &xmb->selection_buf_old)
   {
      xmb_node_t *node = (xmb_node_t*)list->list[current].userdata;

      if (node && (uint8_t)(255 * node->alpha) == 0)
         return;

      i = 0;
   }

   first = (unsigned)i;
   last  = (unsigned)(end - 1);

   xmb_calculate_visible_range(xmb, height,
         end, (unsigned)current, &first, &last);

   for (i = first; i <= last; i++)
   {
      if (xmb_draw_item(
            userdata,
            p_disp,
            p_anim,
            dispctx,
            settings,
            video_width,
            video_height,
            xmb_shadows_enable,
            mymat,
            xmb, core_node,
            list, color,
            i, current,
            width, height) == -1)
         break;
   }
}

static INLINE bool xmb_use_ps3_layout(
      settings_t *settings, unsigned width, unsigned height)
{
   unsigned menu_xmb_layout = settings->uints.menu_xmb_layout;

   switch (menu_xmb_layout)
   {
      case 1:
         /* PS3 */
         return true;
      case 2:
         /* PSP */
         return false;
      case 0:
      default:
         /* Automatic
          * > Use PSP layout on tiny screens */
         return (width > 320) && (height > 240);
   }
}

static INLINE float xmb_get_scale_factor(
      settings_t *settings, bool use_ps3_layout, unsigned width)
{
   float scale_factor;
   float menu_scale_factor = settings->floats.menu_scale_factor;

   /* PS3 Layout */
   if (use_ps3_layout)
      scale_factor = ((menu_scale_factor * (float)width) / 1920.0f);
   /* PSP Layout */
   else
   {
#ifdef _3DS
      scale_factor = menu_scale_factor / 4.0f;
#else
      scale_factor = ((menu_scale_factor * (float)width) / 1920.0f) * 1.5f;
#endif
   }

   /* Apply safety limit */
   if (scale_factor < 0.1f)
      return 0.1f;
   return scale_factor;
}

static void xmb_context_reset_internal(xmb_handle_t *xmb,
      bool is_threaded, bool reinit_textures);

/* Disables the fullscreen thumbnail view, with
 * an optional fade out animation */
static void xmb_hide_fullscreen_thumbnails(
      xmb_handle_t *xmb, bool animate)
{
   uintptr_t alpha_tag = (uintptr_t)&xmb->fullscreen_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Check whether animations are enabled */
   if (animate && (xmb->fullscreen_thumbnail_alpha > 0.0f))
   {
      gfx_animation_ctx_entry_t animation_entry;

      /* Configure fade out animation */
      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
      animation_entry.target_value = 0.0f;
      animation_entry.subject      = &xmb->fullscreen_thumbnail_alpha;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      gfx_animation_push(&animation_entry);
   }
   /* No animation - just set thumbnail alpha to zero */
   else
      xmb->fullscreen_thumbnail_alpha = 0.0f;

   /* Disable fullscreen thumbnails */
   xmb->show_fullscreen_thumbnails = false;
}

/* Enables (and triggers a fade in of) the fullscreen
 * thumbnail view */
static void xmb_show_fullscreen_thumbnails(
      xmb_handle_t *xmb, size_t selection)
{
   menu_entry_t selected_entry;
   gfx_animation_ctx_entry_t animation_entry;
   const char *core_name              = NULL;
   const char *thumbnail_label        = NULL;
   uintptr_t              alpha_tag   = (uintptr_t)
      &xmb->fullscreen_thumbnail_alpha;

   /* Before showing fullscreen thumbnails, must
    * ensure that any existing fullscreen thumbnail
    * view is disabled... */
   xmb_hide_fullscreen_thumbnails(xmb, false);

   /* Sanity check: Return immediately if this is
    * a menu without thumbnail support */
   if (!xmb->fullscreen_thumbnails_available)
      return;

   /* We can only enable fullscreen thumbnails if
    * current selection has at least one valid thumbnail
    * and all thumbnails for current selection are already
    * loaded/available */
   gfx_thumbnail_get_core_name(xmb->thumbnail_path_data, &core_name);
   if (string_is_equal(core_name, "imageviewer"))
   {
      /* imageviewer content requires special treatment,
       * since only one thumbnail can ever be loaded
       * at a time */
      if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
      {
         if (xmb->thumbnails.right.status != GFX_THUMBNAIL_STATUS_AVAILABLE)
            return;
      }
      else if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      {
         if (xmb->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE)
            return;
      }
      else
         return;
   }
   else
   {
      bool left_thumbnail_enabled = gfx_thumbnail_is_enabled(
            xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT);

      if ((xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE) &&
          (left_thumbnail_enabled &&
               ((xmb->thumbnails.left.status != GFX_THUMBNAIL_STATUS_MISSING) &&
                (xmb->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE))))
         return;

      if ((xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_MISSING) &&
          (!left_thumbnail_enabled ||
               (xmb->thumbnails.left.status != GFX_THUMBNAIL_STATUS_AVAILABLE)))
         return;
   }

   /* Cache selected entry label
    * (used as title when fullscreen thumbnails
    * are shown) */
   xmb->fullscreen_thumbnail_label[0] = '\0';

   /* > Get menu entry */
   MENU_ENTRY_INIT(selected_entry);
   selected_entry.path_enabled     = false;
   selected_entry.value_enabled    = false;
   selected_entry.sublabel_enabled = false;
   menu_entry_get(&selected_entry, 0, selection, NULL, true);

   /* > Get entry label */
   if (!string_is_empty(selected_entry.rich_label))
      thumbnail_label          = selected_entry.rich_label;
   else
      thumbnail_label          = selected_entry.path;

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      strlcpy(
            xmb->fullscreen_thumbnail_label,
            thumbnail_label,
            sizeof(xmb->fullscreen_thumbnail_label));

   /* Configure fade in animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &xmb->fullscreen_thumbnail_alpha;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   gfx_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   xmb->fullscreen_thumbnail_selection = selection;
   xmb->show_fullscreen_thumbnails     = true;
}


static enum menu_action xmb_parse_menu_entry_action(
      xmb_handle_t *xmb, enum menu_action action)
{
   enum menu_action new_action = action;

   /* If fullscreen thumbnail view is active, any
    * valid menu action will disable it... */
   if (xmb->show_fullscreen_thumbnails)
   {
      if (action != MENU_ACTION_NOOP)
      {
         xmb_hide_fullscreen_thumbnails(xmb, true);

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
          *   thumbnail to content is jarring... */
         if (xmb->is_file_list ||
               ((action != MENU_ACTION_SELECT) &&
                (action != MENU_ACTION_OK)))
            return MENU_ACTION_NOOP;
      }
   }

   /* Scan user inputs */
   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         /* Check whether left/right action will
          * trigger a tab switch event */
         if (xmb->depth == 1)
         {
            retro_time_t current_time = menu_driver_get_current_time();
            size_t scroll_accel       = 0;

            /* Determine whether input repeat is
             * currently active
             * > This is always true when scroll
             *   acceleration is greater than zero */
            menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
                  &scroll_accel);

            if (scroll_accel > 0)
            {
               /* Ignore input action if tab switch period
                * is less than defined limit */
               if ((current_time - xmb->last_tab_switch_time) <
                     XMB_TAB_SWITCH_REPEAT_DELAY)
               {
                  new_action = MENU_ACTION_NOOP;
                  break;
               }
            }
            xmb->last_tab_switch_time = current_time;
         }
         break;
      case MENU_ACTION_START:
         /* If this is a menu with thumbnails, attempt
          * to show fullscreen thumbnail view */
         if (xmb->fullscreen_thumbnails_available)
         {
            size_t selection = menu_navigation_get_selection();

            xmb_show_fullscreen_thumbnails(xmb, selection);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      default:
         /* In all other cases, pass through input
          * menu action without intervention */
         break;
   }

   return new_action;
}


/* Menu entry action callback */
static int xmb_menu_entry_action(
      void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   xmb_handle_t *xmb           = (xmb_handle_t*)userdata;
   /* Process input action */
   enum menu_action new_action = xmb_parse_menu_entry_action(xmb, action);

   /* Call standard generic_menu_entry_action() function */
   return generic_menu_entry_action(userdata, entry, i, new_action);
}


static void xmb_render(void *data,
      unsigned width, unsigned height, bool is_idle)
{
   /* 'i' must be of 'size_t', since it is passed
    * by reference to menu_entries_ctl() */
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
   xmb_handle_t *xmb        = (xmb_handle_t*)data;
   settings_t *settings     = config_get_ptr();
   size_t      end          = menu_entries_get_size();
   gfx_display_t *p_disp    = disp_get_ptr();
   gfx_animation_t *p_anim  = anim_get_ptr();

   if (!xmb)
      return;

   xmb->use_ps3_layout = xmb_use_ps3_layout(settings, width, height);
   scale_factor        = xmb_get_scale_factor(settings, xmb->use_ps3_layout, width);

   if ((xmb->use_ps3_layout != xmb->last_use_ps3_layout) ||
       (xmb->margins_title  != xmb->last_margins_title) ||
       (scale_factor        != xmb->last_scale_factor))
   {
      xmb->last_use_ps3_layout = xmb->use_ps3_layout;
      xmb->last_margins_title  = xmb->margins_title;
      xmb->last_scale_factor   = scale_factor;

      xmb_context_reset_internal(xmb, video_driver_is_threaded(),
            false);
   }

   /* This must be set every frame when using a pointer,
    * otherwise touchscreen input breaks when changing
    * orientation */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   /* Read pointer state */
   menu_input_get_pointer_state(&xmb->pointer);

   /* If menu screensaver is active, update
    * screensaver and return */
   if (xmb->show_screensaver)
   {
      menu_screensaver_iterate(
            xmb->screensaver,
            p_disp, p_anim,
            (enum menu_screensaver_effect)settings->uints.menu_screensaver_animation,
            settings->floats.menu_screensaver_animation_speed,
            XMB_SCREENSAVER_TINT,
            width, height,
            settings->paths.directory_assets);
      GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
      return;
   }

   if (xmb->pointer.type != MENU_POINTER_DISABLED)
   {
      size_t selection     = menu_navigation_get_selection();
      int16_t margin_top   = (int16_t)xmb->margins_screen_top;
      int16_t margin_left  = (int16_t)xmb->margins_screen_left;
      int16_t margin_right = (int16_t)((float)width - xmb->margins_screen_left);
      int16_t pointer_x    = xmb->pointer.x;
      int16_t pointer_y    = xmb->pointer.y;

      /* When determining current pointer selection, we
       * only track pointer movements between the left
       * and right screen margins */
      if ((pointer_x > margin_left) && (pointer_x < margin_right))
      {
         unsigned first = 0;
         unsigned last  = (unsigned)end;

         if (height)
            xmb_calculate_visible_range(xmb, height,
                  end, (unsigned)selection, &first, &last);

         for (i = (size_t)first; i <= (size_t)last; i++)
         {
            float entry_size      = (i == (unsigned)selection) ?
                  xmb->icon_spacing_vertical * xmb->active_item_factor : xmb->icon_spacing_vertical;
            float half_entry_size = entry_size * 0.5f;
            float y_curr          = xmb_item_y(xmb, (int)i, selection) + xmb->margins_screen_top;
            int y1                = (int)((y_curr - half_entry_size) + 0.5f);
            int y2                = (int)((y_curr + half_entry_size) + 0.5f);

            if ((pointer_y > y1) && (pointer_y < y2))
            {
               menu_input_set_pointer_selection((unsigned)i);
               break;
            }
         }
      }

      /* Areas beyond the top/right margins are used
       * as a sort of virtual dpad:
       * - Above top margin: navigate left/right
       * - Beyond right margin: navigate up/down */
      if ((pointer_y < margin_top) || (pointer_x > margin_right))
      {
         menu_entry_t entry;
         bool get_entry = false;

         switch (xmb->pointer.press_direction)
         {
            case MENU_INPUT_PRESS_DIRECTION_UP:
               if (pointer_x > margin_right)
                  get_entry = true;
               break;
            case MENU_INPUT_PRESS_DIRECTION_DOWN:
               /* Note: Direction is inverted, since 'down' should
                * move list downwards */
               if (pointer_x > margin_right)
                  get_entry = true;
               break;
            case MENU_INPUT_PRESS_DIRECTION_LEFT:
               /* Navigate left
                * Note: At the top level, navigating left
                * means switching to the 'next' horizontal list,
                * which is actually a movement to the *right* */
               if (pointer_y < margin_top)
                  get_entry = true;
               break;
            case MENU_INPUT_PRESS_DIRECTION_RIGHT:
               /* Navigate right
                * Note: At the top level, navigating right
                * means switching to the 'previous' horizontal list,
                * which is actually a movement to the *left* */
               if (pointer_y < margin_top)
                  get_entry = true;
               break;
            case MENU_INPUT_PRESS_DIRECTION_NONE:
            default:
               break;
         }

         if (get_entry)
         {
            MENU_ENTRY_INIT(entry);
            entry.path_enabled       = false;
            entry.label_enabled      = false;
            entry.rich_label_enabled = false;
            entry.value_enabled      = false;
            entry.sublabel_enabled   = false;
            menu_entry_get(&entry, 0, selection, NULL, true);
         }

         switch (xmb->pointer.press_direction)
         {
            case MENU_INPUT_PRESS_DIRECTION_UP:
               /* Note: Direction is inverted, since 'up' should
                * move list upwards */
               if (pointer_x > margin_right)
                  xmb_menu_entry_action(xmb, &entry, selection, MENU_ACTION_DOWN);
               break;
            case MENU_INPUT_PRESS_DIRECTION_DOWN:
               /* Note: Direction is inverted, since 'down' should
                * move list downwards */
               if (pointer_x > margin_right)
                  xmb_menu_entry_action(xmb, &entry, selection, MENU_ACTION_UP);
               break;
            case MENU_INPUT_PRESS_DIRECTION_LEFT:
               /* Navigate left
                * Note: At the top level, navigating left
                * means switching to the 'next' horizontal list,
                * which is actually a movement to the *right* */
               if (pointer_y < margin_top)
                  xmb_menu_entry_action(xmb,
                        &entry, selection, (xmb->depth == 1) ? MENU_ACTION_RIGHT : MENU_ACTION_LEFT);
               break;
            case MENU_INPUT_PRESS_DIRECTION_RIGHT:
               /* Navigate right
                * Note: At the top level, navigating right
                * means switching to the 'previous' horizontal list,
                * which is actually a movement to the *left* */
               if (pointer_y < margin_top)
                  xmb_menu_entry_action(xmb,
                        &entry, selection, (xmb->depth == 1) ? MENU_ACTION_LEFT : MENU_ACTION_RIGHT);
               break;
            default:
               /* Do nothing */
               break;
         }
      }
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
}

static bool xmb_shader_pipeline_active(unsigned menu_shader_pipeline)
{
   if (string_is_not_equal(menu_driver_ident(), "xmb"))
      return false;
   if (menu_shader_pipeline == XMB_SHADER_PIPELINE_WALLPAPER)
      return false;
   return true;
}

static void xmb_draw_bg(
      void *userdata,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height,
      unsigned menu_shader_pipeline,
      unsigned xmb_color_theme,
      float menu_wallpaper_opacity,
      bool libretro_running,
      float alpha,
      uintptr_t texture_id,
      float *coord_black,
      float *coord_white)
{
   gfx_display_ctx_draw_t draw;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   draw.x                    = 0;
   draw.y                    = 0;
   draw.texture              = texture_id;
   draw.width                = video_width;
   draw.height               = video_height;
   draw.color                = &coord_black[0];
   draw.vertex               = NULL;
   draw.tex_coord            = NULL;
   draw.vertex_count         = 4;
   draw.prim_type            = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id          = 0;
   draw.pipeline_active      = xmb_shader_pipeline_active(menu_shader_pipeline);

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

#ifdef HAVE_SHADERPIPELINE
   if (menu_shader_pipeline > XMB_SHADER_PIPELINE_WALLPAPER
         &&
         (xmb_color_theme != XMB_THEME_WALLPAPER))
   {
      draw.color = xmb_gradient_ident(xmb_color_theme);

      if (libretro_running)
         gfx_display_set_alpha(draw.color, coord_black[3]);
      else
         gfx_display_set_alpha(draw.color, coord_white[3]);

      /* Draw gradient */
      draw.texture       = 0;
      draw.x             = 0;
      draw.y             = 0;

      gfx_display_draw_bg(p_disp, &draw, userdata, false,
            menu_wallpaper_opacity);
      if (draw.height > 0 && draw.width > 0)
         if (dispctx && dispctx->draw)
            dispctx->draw(&draw, userdata, video_width, video_height);

      draw.pipeline_id = VIDEO_SHADER_MENU_2;

      switch (menu_shader_pipeline)
      {
         case XMB_SHADER_PIPELINE_RIBBON:
            draw.pipeline_id  = VIDEO_SHADER_MENU;
            break;
#if !defined(VITA)
         case XMB_SHADER_PIPELINE_SIMPLE_SNOW:
            draw.pipeline_id  = VIDEO_SHADER_MENU_3;
            break;
         case XMB_SHADER_PIPELINE_SNOW:
            draw.pipeline_id  = VIDEO_SHADER_MENU_4;
            break;
         case XMB_SHADER_PIPELINE_BOKEH:
            draw.pipeline_id  = VIDEO_SHADER_MENU_5;
            break;
         case XMB_SHADER_PIPELINE_SNOWFLAKE:
            draw.pipeline_id  = VIDEO_SHADER_MENU_6;
            break;
#endif
         default:
            break;
      }

      if (dispctx && dispctx->draw_pipeline)
         dispctx->draw_pipeline(&draw, p_disp,
               userdata, video_width, video_height);
   }
   else
#endif
   {
      uintptr_t texture           = draw.texture;

      if (xmb_color_theme != XMB_THEME_WALLPAPER)
         draw.color = xmb_gradient_ident(xmb_color_theme);

      if (libretro_running)
         gfx_display_set_alpha(draw.color, coord_black[3]);
      else
         gfx_display_set_alpha(draw.color, coord_white[3]);

      if (xmb_color_theme != XMB_THEME_WALLPAPER)
      {
         /* Draw gradient */
         draw.texture       = 0;
         draw.x             = 0;
         draw.y             = 0;

         gfx_display_draw_bg(p_disp, &draw, userdata, false,
               menu_wallpaper_opacity);
         if (draw.height > 0 && draw.width > 0)
            if (dispctx && dispctx->draw)
               dispctx->draw(&draw, userdata, video_width, video_height);
      }

      {
         bool add_opacity       = false;

         draw.texture           = texture;
         gfx_display_set_alpha(draw.color, coord_white[3]);

         if (draw.texture)
            draw.color = &coord_white[0];

         if (libretro_running || xmb_color_theme == XMB_THEME_WALLPAPER)
            add_opacity = true;

         gfx_display_draw_bg(p_disp, &draw, userdata,
               add_opacity, menu_wallpaper_opacity);
      }
   }

   if (dispctx)
   {
      if (dispctx->draw)
         if (draw.height > 0 && draw.width > 0)
            dispctx->draw(&draw, userdata, video_width, video_height);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

static void xmb_draw_dark_layer(
      xmb_handle_t *xmb,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned width,
      unsigned height)
{
   gfx_display_ctx_draw_t draw;
   float black[16]      = {
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
   };
   gfx_display_ctx_driver_t
      *dispctx          = p_disp->dispctx;

   draw.x               = 0;
   draw.y               = 0;
   draw.width           = width;
   draw.height          = height;
   draw.color           = &black[0];
   draw.vertex          = NULL;
   draw.matrix_data     = NULL;
   draw.tex_coord       = NULL;
   draw.vertex_count    = 4;
   draw.texture         = 0;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;
   draw.pipeline_active = false;

   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      gfx_display_draw_bg(p_disp, &draw, userdata,
            true, MIN(xmb->alpha, 0.75));
      if (draw.height > 0 && draw.width > 0)
         if (dispctx && dispctx->draw)
            dispctx->draw(&draw, userdata, width, height);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

static void xmb_draw_fullscreen_thumbnails(
      xmb_handle_t *xmb,
      gfx_animation_t *p_anim,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool xmb_shadows_enable,
      unsigned xmb_color_theme,
      settings_t *settings, size_t selection)
{
   /* Check whether fullscreen thumbnails are visible */
   if (xmb->fullscreen_thumbnail_alpha > 0.0f)
   {
      int header_margin;
      int thumbnail_box_width;
      int thumbnail_box_height;
      int right_thumbnail_x;
      int left_thumbnail_x;
      int thumbnail_y;
      gfx_thumbnail_shadow_t thumbnail_shadow;
      gfx_thumbnail_t *right_thumbnail = NULL;
      gfx_thumbnail_t *left_thumbnail  = NULL;
      int view_width                    = (int)video_width;
      int view_height                   = (int)video_height;
      int thumbnail_margin              = (int)(xmb->icon_size / 2.0f);
      bool show_right_thumbnail         = false;
      bool show_left_thumbnail          = false;
      unsigned num_thumbnails           = 0;
      float right_thumbnail_draw_width  = 0.0f;
      float right_thumbnail_draw_height = 0.0f;
      float left_thumbnail_draw_width   = 0.0f;
      float left_thumbnail_draw_height  = 0.0f;
      float *menu_color                 = xmb_gradient_ident(xmb_color_theme);
      /* XMB doesn't have a proper theme interface, so
       * hard-code this alpha value for now... */
      float background_alpha            = 0.75f;
      float background_color[16]        = {
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
      };
      uint32_t title_color              = 0xFFFFFF00;
      float header_alpha                = 0.6f;
      float header_color[16]            = {
         0.0f, 0.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f, 0.0f,
      };
      int frame_width                   = (int)(xmb->icon_size / 6.0f);
      float frame_color[16]             = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
      };
      bool show_header                  = !string_is_empty(xmb->fullscreen_thumbnail_label);
      int header_height                 = show_header ? (int)((float)xmb->font_size * 1.2f) + (frame_width * 2) : 0;
      bool xmb_vertical_thumbnails      = settings->bools.menu_xmb_vertical_thumbnails;
      bool menu_ticker_smooth           = settings->bools.menu_ticker_smooth;
      enum gfx_animation_ticker_type
         menu_ticker_type               = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;

      /* Sanity check: Return immediately if this is
       * a menu without thumbnails and we are not currently
       * 'fading out' the fullscreen thumbnail view */
      if (!xmb->fullscreen_thumbnails_available &&
          xmb->show_fullscreen_thumbnails)
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
      if ((selection != xmb->fullscreen_thumbnail_selection) &&
          (!xmb->is_quick_menu || xmb->show_fullscreen_thumbnails))
         goto error;

      /* Get thumbnail pointers
       * > Order is swapped when using 'vertical disposition' */
      if (xmb_vertical_thumbnails)
      {
         right_thumbnail = &xmb->thumbnails.left;
         left_thumbnail  = &xmb->thumbnails.right;
      }
      else
      {
         right_thumbnail = &xmb->thumbnails.right;
         left_thumbnail  = &xmb->thumbnails.left;
      }

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
      header_margin = (header_height > thumbnail_margin) ?
            header_height : thumbnail_margin;
      thumbnail_box_height = view_height - header_margin - thumbnail_margin;
      thumbnail_y          = header_margin;

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
            background_color, background_alpha * xmb->fullscreen_thumbnail_alpha);

      /* > Header background */
      header_color[11] = header_alpha * xmb->fullscreen_thumbnail_alpha;
      header_color[15] = header_color[11];

      /* > Title text */
      title_color |= (unsigned)((255.0f * xmb->fullscreen_thumbnail_alpha) + 0.5f);

      /* > Thumbnail frame */
      if (menu_color)
      {
         float mean_menu_color[3];

         /* The menu gradients are not entirely consistent...
          * The best we can do here is take the mean of the
          * first and last vertex colours... */
         mean_menu_color[0] = (menu_color[0] + menu_color[12]) / 2.0f;
         mean_menu_color[1] = (menu_color[1] + menu_color[13]) / 2.0f;
         mean_menu_color[2] = (menu_color[2] + menu_color[14]) / 2.0f;

         memcpy(frame_color,      mean_menu_color, sizeof(mean_menu_color));
         memcpy(frame_color + 4,  mean_menu_color, sizeof(mean_menu_color));
         memcpy(frame_color + 8,  mean_menu_color, sizeof(mean_menu_color));
         memcpy(frame_color + 12, mean_menu_color, sizeof(mean_menu_color));
      }
      gfx_display_set_alpha(
            frame_color, xmb->fullscreen_thumbnail_alpha);

      /* Darken background */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            0,
            (unsigned)view_width,
            (unsigned)view_height,
            (unsigned)view_width,
            (unsigned)view_height,
            background_color,
            NULL);

      /* Draw header */
      if (show_header)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               0,
               0,
               (unsigned)view_width,
               (unsigned)(header_height - frame_width),
               (unsigned)view_width,
               (unsigned)view_height,
               header_color,
               NULL);

         /* Title text */
         if (menu_ticker_smooth)
         {
            char title_buf[255];
            gfx_animation_ctx_ticker_smooth_t ticker_smooth;
            int title_x               = 0;
            unsigned ticker_x_offset  = 0;
            unsigned ticker_str_width = 0;

            title_buf[0] = '\0';

            ticker_smooth.idx           = p_anim->ticker_pixel_idx;
            ticker_smooth.font          = xmb->font;
            ticker_smooth.font_scale    = 1.0f;
            ticker_smooth.type_enum     = menu_ticker_type;
            ticker_smooth.spacer        = NULL;
            ticker_smooth.x_offset      = &ticker_x_offset;
            ticker_smooth.dst_str_width = &ticker_str_width;
            ticker_smooth.selected      = true;
            ticker_smooth.field_width   = (unsigned)view_width;
            ticker_smooth.src_str       = xmb->fullscreen_thumbnail_label;
            ticker_smooth.dst_str       = title_buf;
            ticker_smooth.dst_str_len   = sizeof(title_buf);

            /* If ticker is not active, centre the title text */
            if (!gfx_animation_ticker_smooth(&ticker_smooth))
               title_x = (view_width - (int)ticker_str_width) >> 1;

            title_x += (int)ticker_x_offset;

            gfx_display_draw_text(
                  xmb->font,
                  title_buf,
                  title_x,
                  xmb->font_size,
                  (unsigned)view_width,
                  (unsigned)view_height,
                  title_color,
                  TEXT_ALIGN_LEFT,
                  1.0f, false, 0.0f, false);
         }
         /* Note: The non-smooth ticker is a complete failure
          * here, since actual text width is unknown. This
          * causes the text to be horizontally offset - we
          * cannot fix this.
          * All we can do in this case is just draw the text
          * as-is, horizontally centred, and if the ends get
          * clipped than so be it... */
         else
            gfx_display_draw_text(
                  xmb->font,
                  xmb->fullscreen_thumbnail_label,
                  view_width >> 1,
                  xmb->font_size,
                  (unsigned)view_width,
                  (unsigned)view_height,
                  title_color,
                  TEXT_ALIGN_CENTER,
                  1.0f, false, 0.0f, false);
      }

      /* Draw thumbnails */

      /* > Configure shadow effect */
      if (xmb_shadows_enable)
      {
         float shadow_offset            = xmb->icon_size / 24.0f;

         thumbnail_shadow.type          = GFX_THUMBNAIL_SHADOW_DROP;
         thumbnail_shadow.alpha         = 0.35f;
         thumbnail_shadow.drop.x_offset = shadow_offset;
         thumbnail_shadow.drop.y_offset = shadow_offset;
      }
      else
         thumbnail_shadow.type = GFX_THUMBNAIL_SHADOW_NONE;

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
               (unsigned)view_width,
               (unsigned)view_height,
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
               xmb->fullscreen_thumbnail_alpha,
               1.0f,
               &thumbnail_shadow);
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
               (unsigned)view_width,
               (unsigned)view_height,
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
               xmb->fullscreen_thumbnail_alpha,
               1.0f,
               &thumbnail_shadow);
      }
   }

   return;

error:
   /* If fullscreen thumbnails are enabled at
    * this point, must disable them immediately... */
   if (xmb->show_fullscreen_thumbnails)
      xmb_hide_fullscreen_thumbnails(xmb, false);
}

static void xmb_frame(void *data, video_frame_info_t *video_info)
{
   math_matrix_4x4 mymat;
   unsigned i;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   char msg[1024];
   char title_msg[255];
   char title_truncated[255];
   gfx_thumbnail_shadow_t thumbnail_shadow;
   size_t selection                        = 0;
   size_t percent_width                    = 0;
   bool render_background                  = false;
   file_list_t *selection_buf              = NULL;
   const float under_thumb_margin          = 0.96f;
   float left_thumbnail_margin_width       = 0.0f;
   float right_thumbnail_margin_width      = 0.0f;
   float thumbnail_margin_height_under     = 0.0f;
   float thumbnail_margin_height_full      = 0.0f;
   float left_thumbnail_margin_x           = 0.0f;
   float right_thumbnail_margin_x          = 0.0f;
   float pseudo_font_length                = 0.0f;
   xmb_handle_t *xmb                       = (xmb_handle_t*)data;
   settings_t *settings                    = config_get_ptr();
   unsigned xmb_system_tab                 = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);
   bool fade_tab_icons                     = false;
   float fade_tab_icons_x_threshold        = 0.0f;
   bool menu_core_enable                   = settings->bools.menu_core_enable;
   float thumbnail_scale_factor            = (float)settings->uints.menu_xmb_thumbnail_scale_factor / 100.0f;
   bool menu_xmb_vertical_thumbnails       = settings->bools.menu_xmb_vertical_thumbnails;
   unsigned menu_xmb_vertical_fade_factor  = settings->uints.menu_xmb_vertical_fade_factor;
   void *userdata                          = video_info->userdata;
   unsigned video_width                    = video_info->width;
   unsigned video_height                   = video_info->height;
   bool xmb_shadows_enable                 = video_info->xmb_shadows_enable;
   float xmb_alpha_factor                  = video_info->xmb_alpha_factor;
   bool timedate_enable                    = video_info->timedate_enable;
   bool battery_level_enable               = video_info->battery_level_enable;
   bool video_fullscreen                   = video_info->fullscreen;
   bool mouse_grabbed                      = video_info->input_driver_grab_mouse_state;
   bool menu_mouse_enable                  = video_info->menu_mouse_enable;
   unsigned xmb_color_theme                = video_info->xmb_color_theme;
   bool libretro_running                   = video_info->libretro_running;
   unsigned menu_shader_pipeline           = video_info->menu_shader_pipeline;
   float menu_wallpaper_opacity            = video_info->menu_wallpaper_opacity;
   gfx_display_t            *p_disp        = (gfx_display_t*)video_info->disp_userdata;
   gfx_animation_t          *p_anim        = anim_get_ptr();
   gfx_display_ctx_driver_t *dispctx       = p_disp->dispctx;
   bool input_dialog_display_kb            = menu_input_dialog_get_display_kb();

   if (!xmb)
      return;

   msg[0]             = '\0';
   title_msg[0]       = '\0';
   title_truncated[0] = '\0';

   /* If menu screensaver is active, draw
    * screensaver and return */
   if (xmb->show_screensaver)
   {
      menu_screensaver_frame(xmb->screensaver,
            video_info, p_disp);
      return;
   }

   video_driver_set_viewport(video_width, video_height, true, false);

   pseudo_font_length                      = xmb->icon_spacing_horizontal * 4 - xmb->icon_size / 4.0f;
   left_thumbnail_margin_width             = xmb->icon_size * 3.4f;
   right_thumbnail_margin_width            =
         (float)video_width - (xmb->icon_size / 6) -
         (xmb->margins_screen_left * xmb_scale_mod[5]) -
         xmb->icon_spacing_horizontal - pseudo_font_length;
   thumbnail_margin_height_under           = ((float)video_height * under_thumb_margin) - xmb->margins_screen_top - xmb->icon_size;
   thumbnail_margin_height_full            = (float)video_height - xmb->margins_title_top - ((xmb->icon_size / 4.0f) * 2.0f);
   left_thumbnail_margin_x                 = xmb->icon_size / 6.0f;
   right_thumbnail_margin_x                = (float)video_width - (xmb->icon_size / 6.0f) - right_thumbnail_margin_width;
   xmb->margins_title                      = (float)settings->uints.menu_xmb_title_margin * 10.0f;

   /* Configure shadow effect */
   if (xmb_shadows_enable)
   {
      /* Drop shadow for thumbnails needs to be larger
       * than for text/icons, and also needs to scale
       * with screen dimensions */
      float shadow_offset = xmb->shadow_offset * 1.5f * xmb->last_scale_factor;
      shadow_offset       = (shadow_offset > xmb->shadow_offset)
         ? shadow_offset
         : xmb->shadow_offset;

      thumbnail_shadow.type          = GFX_THUMBNAIL_SHADOW_DROP;
      thumbnail_shadow.alpha         = 0.35f;
      thumbnail_shadow.drop.x_offset = shadow_offset;
      thumbnail_shadow.drop.y_offset = shadow_offset;
   }
   else
      thumbnail_shadow.type          = GFX_THUMBNAIL_SHADOW_NONE;

   font_driver_bind_block(xmb->font,  &xmb->raster_block);
   font_driver_bind_block(xmb->font2, &xmb->raster_block2);

   xmb->raster_block.carr.coords.vertices  = 0;
   xmb->raster_block2.carr.coords.vertices = 0;

   gfx_display_set_alpha(xmb_coord_black, MIN(
            (float)xmb_alpha_factor / 100,
            xmb->alpha));
   gfx_display_set_alpha(xmb_coord_white, xmb->alpha);

   xmb_draw_bg(
         userdata,
         p_disp,
         video_width,
         video_height,
         menu_shader_pipeline,
         xmb_color_theme,
         menu_wallpaper_opacity,
         libretro_running,
         xmb->alpha,
         xmb->textures.bg,
         xmb_coord_black,
         xmb_coord_white);

   selection = menu_navigation_get_selection();

   strlcpy(title_truncated,
         xmb->title_name, sizeof(title_truncated));

   if (!menu_xmb_vertical_fade_factor && selection > 1)
   {
      /* skip 25 UTF8 multi-byte chars */
      char *end = title_truncated;

      for (i = 0; i < 25 && *end; i++)
      {
         end++;
         while ((*end & 0xC0) == 0x80)
            end++;
      }

      *end = '\0';
   }

   /* Title text */
   xmb_draw_text(xmb_shadows_enable, xmb, settings,
         title_truncated, xmb->margins_title_left,
         xmb->margins_title_top,
         1, 1, TEXT_ALIGN_LEFT,
         video_width, video_height, xmb->font);

   if (menu_core_enable)
   {
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      xmb_draw_text(xmb_shadows_enable, xmb, settings,
            title_msg, xmb->margins_title_left,
            video_height - xmb->margins_title_bottom, 1, 1, TEXT_ALIGN_LEFT,
            video_width, video_height, xmb->font);
   }

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

   /**************************/
   /* Draw thumbnails: START */
   /**************************/

   /* Note: This is incredibly ugly, but there are
    * so many combinations here that we would go insane
    * trying to rationalise this any further... */

   /* Save state thumbnail, right side */
   if (xmb->is_quick_menu &&
         ((xmb->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_AVAILABLE) ||
         (xmb->thumbnails.savestate.status == GFX_THUMBNAIL_STATUS_PENDING)))
   {
      float thumb_width         = right_thumbnail_margin_width;
      float thumb_height        = thumbnail_margin_height_full;
      float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
      float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
      float thumb_x             = right_thumbnail_margin_x + ((thumb_width - scaled_thumb_width) / 2.0f);
      float thumb_y             = xmb->margins_title_top + (xmb->icon_size / 4.0f) + ((thumb_height - scaled_thumb_height) / 2.0f);

      gfx_thumbnail_draw(
            userdata,
            video_width,
            video_height,
            &xmb->thumbnails.savestate,
            thumb_x,
            thumb_y,
            scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
            scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
            GFX_THUMBNAIL_ALIGN_CENTRE,
            1.0f, 1.0f, &thumbnail_shadow);
   }
   /* This is used for hiding thumbnails when going into sub-levels in the
    * Quick Menu as well as when selecting "Information" for a playlist entry.
    * NOTE: This is currently a pretty crude check, simply going by menu depth
    * and not specifically identifying which menu we're actually in. */
   else if (!((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS) && (xmb->depth > 2)))
   {
      bool show_right_thumbnail =
            (xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_AVAILABLE) ||
            (xmb->thumbnails.right.status == GFX_THUMBNAIL_STATUS_PENDING);
      bool show_left_thumbnail  =
            (xmb->thumbnails.left.status == GFX_THUMBNAIL_STATUS_AVAILABLE) ||
            (xmb->thumbnails.left.status == GFX_THUMBNAIL_STATUS_PENDING);

      /* Check if we are using the proper PS3 layout,
       * or the aborted PSP layout */
      if (xmb->use_ps3_layout)
      {
         /* Check if user has selected vertically
          * stacked thumbnails */
         if (menu_xmb_vertical_thumbnails)
         {
            /* Right + left thumbnails, right side */
            if (show_right_thumbnail && show_left_thumbnail)
            {
               float thumb_width         = right_thumbnail_margin_width;
               float thumb_height        = (thumbnail_margin_height_full - (xmb->icon_size / 4.0f)) / 2.0f;
               float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
               float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
               float thumb_x             = right_thumbnail_margin_x + ((thumb_width - scaled_thumb_width) / 2.0f);
               float thumb_y_base        = xmb->margins_title_top + (xmb->icon_size / 4.0f);
               float thumb_y_offset      = (thumb_height - scaled_thumb_height) / 2.0f;
               float right_thumb_y       = thumb_y_base + thumb_y_offset;
               float left_thumb_y        = thumb_y_base + thumb_height + (xmb->icon_size / 4) + thumb_y_offset;

               gfx_thumbnail_draw(
                     userdata,
                     video_width,
                     video_height,
                     &xmb->thumbnails.right,
                     thumb_x,
                     right_thumb_y,
                     scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                     scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                     GFX_THUMBNAIL_ALIGN_CENTRE,
                     1.0f, 1.0f, &thumbnail_shadow);

               gfx_thumbnail_draw(
                     userdata,
                     video_width,
                     video_height,
                     &xmb->thumbnails.left,
                     thumb_x,
                     left_thumb_y,
                     scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                     scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                     GFX_THUMBNAIL_ALIGN_CENTRE,
                     1.0f, 1.0f, &thumbnail_shadow);

               /* Horizontal tab icons overlapping the top
                * right image must be faded out */
               fade_tab_icons             = true;
               fade_tab_icons_x_threshold = thumb_x;
            }
            /* Right *or* left, right side */
            else if (show_right_thumbnail || show_left_thumbnail)
            {
               float thumb_width         = right_thumbnail_margin_width;
               float thumb_height        = thumbnail_margin_height_under;
               float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
               float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
               float thumb_x             = right_thumbnail_margin_x + ((thumb_width - scaled_thumb_width) / 2.0f);
               float thumb_y             = xmb->margins_screen_top + xmb->icon_size;

               gfx_thumbnail_draw(
                     userdata,
                     video_width,
                     video_height,
                     show_right_thumbnail ? &xmb->thumbnails.right : &xmb->thumbnails.left,
                     thumb_x,
                     thumb_y,
                     scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                     scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                     GFX_THUMBNAIL_ALIGN_TOP,
                     1.0f, 1.0f, &thumbnail_shadow);
            }
         }
         else
         {
            /* Right thumbnail, right side */
            if (show_right_thumbnail)
            {
               float thumb_width         = right_thumbnail_margin_width;
               float thumb_height        = thumbnail_margin_height_under;
               float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
               float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
               float thumb_x             = right_thumbnail_margin_x + ((thumb_width - scaled_thumb_width) / 2.0f);
               float thumb_y             = xmb->margins_screen_top + xmb->icon_size;

               gfx_thumbnail_draw(
                     userdata,
                     video_width,
                     video_height,
                     &xmb->thumbnails.right,
                     thumb_x,
                     thumb_y,
                     scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                     scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                     GFX_THUMBNAIL_ALIGN_TOP,
                     1.0f, 1.0f, &thumbnail_shadow);
            }

            /* Left thumbnail, left side */
            if (show_left_thumbnail)
            {
               float y_offset            = ((xmb->depth != 1) ? 1.2f : 0.0f) * xmb->icon_size;
               float thumb_width         = left_thumbnail_margin_width;
               float thumb_height        = thumbnail_margin_height_under - xmb->margins_title_bottom - y_offset;
               float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
               float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
               float thumb_x             = left_thumbnail_margin_x + ((thumb_width - scaled_thumb_width) / 2.0f);
               float thumb_y             = xmb->margins_screen_top + xmb->icon_size + y_offset;

               gfx_thumbnail_draw(
                     userdata,
                     video_width,
                     video_height,
                     &xmb->thumbnails.left,
                     thumb_x,
                     thumb_y,
                     scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                     scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                     GFX_THUMBNAIL_ALIGN_TOP,
                     1.0f, 1.0f, &thumbnail_shadow);
            }
         }
      }
      /* This is the PSP layout - thumbnails are only
       * drawn on the left hand side
       * > If left thumbnail is available, show it
       * > If not, show right thumbnail instead
       *   (if available) */
      else if (show_right_thumbnail || show_left_thumbnail)
      {
         float y_offset            = ((xmb->depth != 1) ? 1.2f : -0.25f) * xmb->icon_size;
         float thumb_width         = xmb->icon_size * 2.4f;
         float thumb_height        = thumbnail_margin_height_under - xmb->margins_title_bottom - (xmb->icon_size / 6.0f) - y_offset;
         float scaled_thumb_width  = thumb_width * thumbnail_scale_factor;
         float scaled_thumb_height = thumb_height * thumbnail_scale_factor;
         float thumb_x             = (thumb_width - scaled_thumb_width) / 2.0f;
         float thumb_y             = xmb->margins_screen_top + xmb->icon_size + y_offset;

         /* Very small thumbnails look ridiculous
          * > Impose a minimum size limit */
         if (thumb_height > xmb->icon_size)
            gfx_thumbnail_draw(
                  userdata,
                  video_width,
                  video_height,
                  show_left_thumbnail ? &xmb->thumbnails.left : &xmb->thumbnails.right,
                  thumb_x,
                  thumb_y,
                  scaled_thumb_width  > 0.0f ? (unsigned)scaled_thumb_width  : 0,
                  scaled_thumb_height > 0.0f ? (unsigned)scaled_thumb_height : 0,
                  GFX_THUMBNAIL_ALIGN_TOP,
                  1.0f, 1.0f, &thumbnail_shadow);
      }
   }

   /**************************/
   /* Draw thumbnails: END   */
   /**************************/

   /* Clock image */
   gfx_display_set_alpha(item_color, MIN(xmb->alpha, 1.00f));

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
         size_t x_pos      = xmb->icon_size / 6;

         if (!xmb->assets_missing)
         {
            float margin_offset = -(xmb->icon_size / 2) - (7 * xmb->last_scale_factor);

            if (dispctx && dispctx->blend_begin)
               dispctx->blend_begin(userdata);
            xmb_draw_icon(
                  userdata,
                  p_disp,
                  dispctx,
                  video_width,
                  video_height,
                  xmb_shadows_enable,
                  xmb->icon_size,
                  xmb->textures.list[
                  powerstate.charging? XMB_TEXTURE_BATTERY_CHARGING   :
                  (powerstate.percent > 80)? XMB_TEXTURE_BATTERY_FULL :
                  (powerstate.percent > 60)? XMB_TEXTURE_BATTERY_80   :
                  (powerstate.percent > 40)? XMB_TEXTURE_BATTERY_60   :
                  (powerstate.percent > 20)? XMB_TEXTURE_BATTERY_40   :
                  XMB_TEXTURE_BATTERY_20
                  ],
                  video_width - xmb->margins_title_left + margin_offset,
                  xmb->icon_size + xmb->margins_title_top + margin_offset,
                  video_width,
                  video_height,
                  1,
                  0,
                  1,
                  &item_color[0],
                  xmb->shadow_offset,
                  &mymat);
                  if (dispctx && dispctx->blend_end)
                     dispctx->blend_end(userdata);
         }

         percent_width = (unsigned)
            font_driver_get_message_width(
                  xmb->font, msg, (unsigned)strlen(msg), 1);

         xmb_draw_text(xmb_shadows_enable, xmb, settings, msg,
               video_width - xmb->margins_title_left - x_pos,
               xmb->margins_title_top, 1, 1, TEXT_ALIGN_RIGHT,
               video_width, video_height, xmb->font);
      }
   }

   if (timedate_enable)
   {
      gfx_display_ctx_datetime_t datetime;
      char timedate[255];
      size_t x_pos = 2;

      if (percent_width)
         x_pos = percent_width + (xmb->icon_size / 2.5);

      if (!xmb->assets_missing)
      {
         float margin_offset = -(xmb->icon_size / 2) - (7 * xmb->last_scale_factor);

         if (dispctx && dispctx->blend_begin)
            dispctx->blend_begin(userdata);
         xmb_draw_icon(
               userdata,
               p_disp,
               dispctx,
               video_width,
               video_height,
               xmb_shadows_enable,
               xmb->icon_size,
               xmb->textures.list[XMB_TEXTURE_CLOCK],
               video_width - xmb->margins_title_left + margin_offset - x_pos,
               xmb->icon_size + xmb->margins_title_top + margin_offset,
               video_width,
               video_height,
               1,
               0,
               1,
               &item_color[0],
               xmb->shadow_offset,
               &mymat);
         if (dispctx && dispctx->blend_end)
            dispctx->blend_end(userdata);
      }

      timedate[0]             = '\0';

      datetime.s              = timedate;
      datetime.len            = sizeof(timedate);
      datetime.time_mode      = settings->uints.menu_timedate_style;
      datetime.date_separator = settings->uints.menu_timedate_date_separator;

      menu_display_timedate(&datetime);

      xmb_draw_text(xmb_shadows_enable, xmb, settings, timedate,
            video_width - xmb->margins_title_left - xmb->icon_size / 4 - x_pos,
            xmb->margins_title_top, 1, 1, TEXT_ALIGN_RIGHT,
            video_width, video_height, xmb->font);
   }

   /* Arrow image */
   gfx_display_set_alpha(item_color,
         MIN(xmb->textures_arrow_alpha, xmb->alpha));

   if (!xmb->assets_missing)
   {
      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      xmb_draw_icon(
            userdata,
            p_disp,
            dispctx,
            video_width,
            video_height,
            xmb_shadows_enable,
            xmb->icon_size,
            xmb->textures.list[XMB_TEXTURE_ARROW],
            xmb->x + xmb->margins_screen_left +
            xmb->icon_spacing_horizontal -
            xmb->icon_size / 2.0 + xmb->icon_size,
            xmb->margins_screen_top +
            xmb->icon_size / 2.0 + xmb->icon_spacing_vertical
            * xmb->active_item_factor,
            video_width,
            video_height,
            xmb->textures_arrow_alpha,
            0,
            1,
            &item_color[0],
            xmb->shadow_offset,
            &mymat);
      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
   }

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Horizontal tab icons */
   if (!xmb->assets_missing)
   {

      for (i = 0; i <= xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
            + xmb->system_tab_end; i++)
      {
         xmb_node_t *node = xmb_get_node(xmb, i);

         if (!node)
            continue;

         gfx_display_set_alpha(item_color, MIN(node->alpha, xmb->alpha));

         if (item_color[3] != 0)
         {
            gfx_display_ctx_rotate_draw_t rotate_draw;
            math_matrix_4x4 mymat_tmp;
            uintptr_t texture        = node->icon;
            float x                  = xmb->x + xmb->categories_x_pos +
               xmb->margins_screen_left +
               xmb->icon_spacing_horizontal
               * (i + 1) - xmb->icon_size / 2.0;
            float y                  = xmb->margins_screen_top
               + xmb->icon_size / 2.0;
            float scale_factor       = node->zoom;

            /* Check whether we need to fade out icons
             * overlapping the top right thumbnail image */
            if (fade_tab_icons)
            {
               float x_threshold = fade_tab_icons_x_threshold - (xmb->icon_size * 1.5f);

               if (x > x_threshold)
               {
                  float fade_alpha      = item_color[3];
                  float fade_offset     = (x - x_threshold) * 2.0f;

                  fade_offset = (fade_offset > xmb->icon_size) ? xmb->icon_size : fade_offset;
                  fade_alpha *= 1.0f - (fade_offset / xmb->icon_size);

                  if (fade_alpha <= 0.0f)
                     continue;

                  gfx_display_set_alpha(item_color, fade_alpha);
               }
            }

            rotate_draw.matrix       = &mymat_tmp;
            rotate_draw.rotation     = 0;
            rotate_draw.scale_x      = scale_factor;
            rotate_draw.scale_y      = scale_factor;
            rotate_draw.scale_z      = 1;
            rotate_draw.scale_enable = true;

            gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

            xmb_draw_icon(
                  userdata,
                  p_disp,
                  dispctx,
                  video_width,
                  video_height,
                  xmb_shadows_enable,
                  xmb->icon_size,
                  texture,
                  x,
                  y,
                  video_width,
                  video_height,
                  1.0,
                  0, /* rotation */
                  scale_factor,
                  &item_color[0],
                  xmb->shadow_offset,
                  &mymat_tmp);
         }
      }

   }

   /* Vertical icons */
   xmb_draw_items(
         userdata,
         p_disp,
         p_anim,
         settings,
         video_width,
         video_height,
         xmb_shadows_enable,
         xmb,
         &xmb->selection_buf_old,
         xmb->selection_ptr_old,
         (xmb_list_get_size(xmb, MENU_LIST_PLAIN) > 1)
         ? xmb->categories_selection_ptr :
         xmb->categories_selection_ptr_old,
         &item_color[0],
         video_width,
         video_height,
         &mymat);

   selection_buf = menu_entries_get_selection_buf_ptr(0);

   xmb_draw_items(
         userdata,
         p_disp,
         p_anim,
         settings,
         video_width,
         video_height,
         xmb_shadows_enable,
         xmb,
         selection_buf,
         selection,
         xmb->categories_selection_ptr,
         &item_color[0],
         video_width,
         video_height,
         &mymat);

   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);

   font_driver_flush(video_width, video_height, xmb->font);
   font_driver_flush(video_width, video_height, xmb->font2);
   font_driver_bind_block(xmb->font, NULL);
   font_driver_bind_block(xmb->font2, NULL);

   /* Draw fullscreen thumbnails, if required */
   xmb_draw_fullscreen_thumbnails(
         xmb,
         p_anim,
         p_disp,
         userdata,
         video_width,
         video_height,
         xmb_shadows_enable,
         xmb_color_theme,
         settings, selection);

   if (input_dialog_display_kb)
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      render_background = true;
   }

   if (!string_is_empty(xmb->box_message))
   {
      strlcpy(msg, xmb->box_message,
            sizeof(msg));
      free(xmb->box_message);
      xmb->box_message  = NULL;
      render_background = true;
   }

   if (render_background)
   {
      xmb_draw_dark_layer(xmb, p_disp,
            userdata, video_width, video_height);
      xmb_render_messagebox_internal(userdata, p_disp,
            video_width, video_height,
            xmb, msg, &mymat);
   }

   /* Cursor image */
   if (xmb->show_mouse)
   {
      bool cursor_visible = (video_fullscreen || mouse_grabbed) &&
            menu_mouse_enable;

      gfx_display_set_alpha(xmb_coord_white, MIN(xmb->alpha, 1.00f));
      if (cursor_visible)
         gfx_display_draw_cursor(
               p_disp,
               userdata,
               video_width,
               video_height,
               cursor_visible,
               &xmb_coord_white[0],
               xmb->cursor_size,
               xmb->textures.list[XMB_TEXTURE_POINTER],
               xmb->pointer.x,
               xmb->pointer.y,
               video_width,
               video_height);
   }

   video_driver_set_viewport(video_width, video_height, false, true);
}

static void xmb_layout_ps3(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size;
   float scale_factor            = xmb->last_scale_factor;
   float margins_title           = xmb->margins_title;

   xmb->above_subitem_offset     =  1.5;
   xmb->above_item_offset        = -1.0;
   xmb->active_item_factor       =  3.0;
   xmb->under_item_offset        =  5.0;

   xmb->categories_active_zoom   = 1.0;
   xmb->categories_passive_zoom  = 0.5;
   xmb->items_active_zoom        = 1.0;
   xmb->items_passive_zoom       = 0.5;

   xmb->categories_active_alpha  = 1.0;
   xmb->categories_passive_alpha = 0.85;
   xmb->items_active_alpha       = 1.0;
   xmb->items_passive_alpha      = 0.85;

   xmb->shadow_offset            = 2.0;

   new_font_size                 = 32.0 * scale_factor;
   xmb->font2_size               = 24.0 * scale_factor;

   xmb->cursor_size              = 64.0 * scale_factor;

   xmb->icon_spacing_horizontal  = 200.0 * scale_factor;
   xmb->icon_spacing_vertical    = 64.0 * scale_factor;

   xmb->margins_screen_top       = (256+32) * scale_factor;
   xmb->margins_screen_left      = 336.0 * scale_factor;

   xmb->margins_title_left       = (margins_title * scale_factor) + (4 * scale_factor);
   xmb->margins_title_top        = (margins_title * scale_factor) + (new_font_size - (new_font_size / 6) * scale_factor);
   xmb->margins_title_bottom     = (margins_title * scale_factor) + (4 * scale_factor);

   xmb->margins_label_left       = 85.0 * scale_factor;
   xmb->margins_label_top        = new_font_size / 3.0;

   xmb->margins_setting_left     = 600.0 * scale_factor * xmb_scale_mod[6];
   xmb->margins_dialog           = 48 * scale_factor;

   xmb->margins_slice            = 16 * scale_factor;

   xmb->icon_size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;
}

static void xmb_layout_psp(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size;
   float scale_factor            = xmb->last_scale_factor;
   float margins_title           = xmb->margins_title;

   xmb->above_subitem_offset     =  1.5;
   xmb->above_item_offset        = -1.0;
   xmb->active_item_factor       =  2.0;
   xmb->under_item_offset        =  3.0;

   xmb->categories_active_zoom   = 1.0;
   xmb->categories_passive_zoom  = 1.0;
   xmb->items_active_zoom        = 1.0;
   xmb->items_passive_zoom       = 1.0;

   xmb->categories_active_alpha  = 1.0;
   xmb->categories_passive_alpha = 0.85;
   xmb->items_active_alpha       = 1.0;
   xmb->items_passive_alpha      = 0.85;

   xmb->shadow_offset            = 1.0;

   new_font_size                 = 32.0 * scale_factor;
   xmb->font2_size               = 24.0 * scale_factor;

   xmb->cursor_size              = 64.0;

   xmb->icon_spacing_horizontal  = 250.0 * scale_factor;
   xmb->icon_spacing_vertical    = 108.0 * scale_factor;

   xmb->margins_screen_top       = (256+32) * scale_factor;
   xmb->margins_screen_left      = 136.0 * scale_factor;

   xmb->margins_title_left       = (margins_title * scale_factor) + (4 * scale_factor);
   xmb->margins_title_top        = (margins_title * scale_factor) + (new_font_size - (new_font_size / 6) * scale_factor);
   xmb->margins_title_bottom     = (margins_title * scale_factor) + (4 * scale_factor);

   xmb->margins_label_left       = 85.0 * scale_factor;
   xmb->margins_label_top        = new_font_size / 3.0;

   xmb->margins_setting_left     = 600.0 * scale_factor;
   xmb->margins_dialog           = 48 * scale_factor;

   xmb->margins_slice            = 16 * scale_factor;

   xmb->icon_size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;
}

static void xmb_layout(xmb_handle_t *xmb)
{
   unsigned width, height, i, current, end;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   video_driver_get_size(&width, &height);

   if (xmb->use_ps3_layout)
      xmb_layout_ps3(xmb, width);
   else
      xmb_layout_psp(xmb, width);

#ifdef XMB_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  xmb->margins_screen_left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  xmb->margins_screen_top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  xmb->margins_title_left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  xmb->margins_title_top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  xmb->margins_title_bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  xmb->margins_label_left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  xmb->margins_label_top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  xmb->margins_setting_left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  xmb->icon_spacing_horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  xmb->icon_spacing_vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  xmb->icon_size);
#endif

   current = (unsigned)selection;
   end     = (unsigned)menu_entries_get_size();

   for (i = 0; i < end; i++)
   {
      float ia         = xmb->items_passive_alpha;
      float iz         = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)selection_buf->list[i].userdata;

      if (!node)
         continue;

      if (i == current)
      {
         ia             = xmb->items_active_alpha;
         iz             = xmb->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = ia;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
   }

   if (xmb->depth <= 1)
      return;

   current = (unsigned)xmb->selection_ptr_old;
   end     = (unsigned)xmb->selection_buf_old.size;

   for (i = 0; i < end; i++)
   {
      float         ia = 0;
      float         iz = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            &xmb->selection_buf_old, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia             = xmb->items_active_alpha;
         iz             = xmb->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = 0;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
      node->x           = xmb->icon_size * 1 * -2;
   }
}

static void xmb_ribbon_set_vertex(float *ribbon_verts,
      unsigned idx, unsigned row, unsigned col)
{
   ribbon_verts[idx++] = ((float)col) / (XMB_RIBBON_COLS-1) * 2.0f - 1.0f;
   ribbon_verts[idx++] = ((float)row) / (XMB_RIBBON_ROWS-1) * 2.0f - 1.0f;
}

static void xmb_init_ribbon(xmb_handle_t * xmb)
{
   video_coords_t coords;
   unsigned r, c, col;
   unsigned i                = 0;
   gfx_display_t *p_disp     = disp_get_ptr();
   video_coord_array_t *ca   = &p_disp->dispca;
   unsigned   vertices_total = XMB_RIBBON_VERTICES;
   float *dummy              = (float*)calloc(4 * vertices_total, sizeof(float));
   float *ribbon_verts       = (float*)calloc(2 * vertices_total, sizeof(float));

   /* Set up vertices */
   for (r = 0; r < XMB_RIBBON_ROWS - 1; r++)
   {
      for (c = 0; c < XMB_RIBBON_COLS; c++)
      {
         col = r % 2 ? XMB_RIBBON_COLS - c - 1 : c;
         xmb_ribbon_set_vertex(ribbon_verts, i,     r,     col);
         xmb_ribbon_set_vertex(ribbon_verts, i + 2, r + 1, col);
         i  += 4;
      }
   }

   coords.color         = dummy;
   coords.vertex        = ribbon_verts;
   coords.tex_coord     = dummy;
   coords.lut_tex_coord = dummy;
   coords.vertices      = vertices_total;

   video_coord_array_append(ca, &coords, coords.vertices);

   free(dummy);
   free(ribbon_verts);
}

static void xmb_menu_animation_update_time(
      float *ticker_pixel_increment,
      unsigned video_width, unsigned video_height)
{
   menu_handle_t *menu = menu_state_get_ptr()->driver_data;
   xmb_handle_t *xmb   = NULL;

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   *(ticker_pixel_increment) *= xmb->last_scale_factor;
}

static void *xmb_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   int i;
   xmb_handle_t *xmb          = NULL;
   settings_t *settings       = config_get_ptr();
   gfx_animation_t *p_anim    = anim_get_ptr();
   menu_handle_t *menu        = (menu_handle_t*)calloc(1, sizeof(*menu));
   float scale_value          = settings->floats.menu_scale_factor * 100.0f;

   /* scaling multiplier formulas made from these values:     */
   /* xmb_scale 50 = {2.5, 2.5,   2, 1.7, 2.5,   4, 2.4, 2.5} */
   /* xmb_scale 75 = {  2, 1.6, 1.6, 1.4, 1.5, 2.3, 1.9, 1.3} */

   if (scale_value < 100)
   {
      /* text length & word wrap (base 35 apply to file browser, 1st column) */
      xmb_scale_mod[0] = -0.03 * scale_value + 4.083;
      /* playlist text length when thumbnail is ON (small, base 40) */
      xmb_scale_mod[1] = -0.03 * scale_value + 3.95;
      /* playlist text length when thumbnail is OFF (large, base 70) */
      xmb_scale_mod[2] = -0.02 * scale_value + 3.033;
      /* sub-label length & word wrap */
      xmb_scale_mod[3] = -0.014 * scale_value + 2.416;
      /* thumbnail size & vertical margin from top */
      xmb_scale_mod[4] = -0.03 * scale_value + 3.916;
      /* thumbnail horizontal left margin (horizontal positioning) */
      xmb_scale_mod[5] = -0.06 * scale_value + 6.933;
      /* margin before 2nd column start (shaders parameters, cheats...) */
      xmb_scale_mod[6] = -0.028 * scale_value + 3.866;
      /* text length & word wrap (base 35 apply to 2nd column in cheats, shaders, etc) */
      xmb_scale_mod[7] = 134.179 * pow(scale_value, -1.0778);

      for (i = 0; i < 8; i++)
         if (xmb_scale_mod[i] < 1)
            xmb_scale_mod[i] = 1;
   }

   if (!menu)
      return NULL;

   video_driver_get_size(&width, &height);

   xmb = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!xmb)
   {
      free(menu);
      return NULL;
   }

   *userdata = xmb;

   file_list_initialize(&xmb->selection_buf_old);

   xmb->categories_active_idx         = 0;
   xmb->categories_active_idx_old     = 0;
   xmb->x                             = 0;
   xmb->categories_x_pos              = 0;
   xmb->textures_arrow_alpha          = 0;
   xmb->depth                         = 1;
   xmb->old_depth                     = 1;
   xmb->alpha                         = 0;

   xmb->system_tab_end                = 0;
   xmb->tabs[xmb->system_tab_end]     = XMB_SYSTEM_TAB_MAIN;

   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_NETPLAY;
#endif

   if (      settings->bools.menu_content_show_add
         && !settings->bools.kiosk_mode_enable)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_ADD;

#if defined(HAVE_LIBRETRODB)
   if (settings->bools.menu_content_show_explore)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_EXPLORE;
#endif

#if defined(HAVE_DYNAMIC)
   if (settings->uints.menu_content_show_contentless_cores !=
         MENU_CONTENTLESS_CORES_DISPLAY_NONE)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_CONTENTLESS_CORES;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   gfx_display_set_width(width);
   gfx_display_set_height(height);

   gfx_display_init_white_texture();

   file_list_initialize(&xmb->horizontal_list);

   xmb_init_horizontal_list(xmb);

   xmb_init_ribbon(xmb);

   /* Initialise screensaver */
   xmb->screensaver = menu_screensaver_init();
   if (!xmb->screensaver)
      goto error;

   /* Thumbnail initialisation */
   xmb->thumbnail_path_data = gfx_thumbnail_path_init();
   if (!xmb->thumbnail_path_data)
      goto error;

   xmb->savestate_thumbnail_file_path[0]      = '\0';
   xmb->prev_savestate_thumbnail_file_path[0] = '\0';

   xmb->fullscreen_thumbnails_available = false;
   xmb->show_fullscreen_thumbnails      = false;
   xmb->fullscreen_thumbnail_alpha      = 0.0f;
   xmb->fullscreen_thumbnail_selection  = 0;
   xmb->fullscreen_thumbnail_label[0]   = '\0';

   gfx_thumbnail_set_stream_delay(-1.0f);
   gfx_thumbnail_set_fade_duration(-1.0f);
   gfx_thumbnail_set_fade_missing(false);

   xmb->use_ps3_layout      = xmb_use_ps3_layout(settings, width, height);
   xmb->last_use_ps3_layout = xmb->use_ps3_layout;
   xmb->last_scale_factor   = xmb_get_scale_factor(settings, xmb->use_ps3_layout, width);

   p_anim->updatetime_cb    = xmb_menu_animation_update_time;

   /* set word_wrap function pointer */
   xmb->word_wrap = msg_hash_get_wideglyph_str() ? word_wrap_wideglyph : word_wrap;

   return menu;

error:
   free(menu);

   xmb_free_list_nodes(&xmb->horizontal_list, false);
   file_list_deinitialize(&xmb->selection_buf_old);
   file_list_deinitialize(&xmb->horizontal_list);
   RHMAP_FREE(xmb->playlist_db_node_map);
   return NULL;
}

static void xmb_free(void *data)
{
   xmb_handle_t       *xmb = (xmb_handle_t*)data;

   if (xmb)
   {
      xmb_free_list_nodes(&xmb->selection_buf_old, false);
      xmb_free_list_nodes(&xmb->horizontal_list, false);
      file_list_deinitialize(&xmb->selection_buf_old);
      file_list_deinitialize(&xmb->horizontal_list);
      RHMAP_FREE(xmb->playlist_db_node_map);

      video_coord_array_free(&xmb->raster_block.carr);
      video_coord_array_free(&xmb->raster_block2.carr);

      if (!string_is_empty(xmb->box_message))
         free(xmb->box_message);
      if (!string_is_empty(xmb->bg_file_path))
         free(xmb->bg_file_path);

      if (xmb->thumbnail_path_data)
         free(xmb->thumbnail_path_data);

      menu_screensaver_free(xmb->screensaver);
   }

   gfx_display_deinit_white_texture();
   font_driver_bind_block(NULL, NULL);
}

static void xmb_context_bg_destroy(xmb_handle_t *xmb)
{
   if (!xmb)
      return;
   video_driver_texture_unload(&xmb->textures.bg);
   gfx_display_deinit_white_texture();
}

static bool xmb_load_image(void *userdata, void *data,
      enum menu_image_type type)
{
   xmb_handle_t *xmb = (xmb_handle_t*)userdata;

   if (!xmb)
      return false;

   switch (type)
   {
      case MENU_IMAGE_WALLPAPER:
         xmb_context_bg_destroy(xmb);
         video_driver_texture_unload(&xmb->textures.bg);
         gfx_display_deinit_white_texture();
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR,
               &xmb->textures.bg);
         gfx_display_init_white_texture();
         break;
      case MENU_IMAGE_NONE:
      default:
         break;
   }

   return true;
}

static const char *xmb_texture_path(unsigned id)
{
   switch (id)
   {
      case XMB_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case XMB_TEXTURE_SETTINGS:
         return "settings.png";
      case XMB_TEXTURE_HISTORY:
         return "history.png";
      case XMB_TEXTURE_FAVORITES:
         return "favorites.png";
      case XMB_TEXTURE_ADD_FAVORITE:
         return "add-favorite.png";
      case XMB_TEXTURE_MUSICS:
         return "musics.png";
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case XMB_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case XMB_TEXTURE_IMAGES:
         return "images.png";
#endif
      case XMB_TEXTURE_SETTING:
         return "setting.png";
      case XMB_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case XMB_TEXTURE_ARROW:
         return "arrow.png";
      case XMB_TEXTURE_RUN:
         return "run.png";
      case XMB_TEXTURE_CLOSE:
         return "close.png";
      case XMB_TEXTURE_RESUME:
         return "resume.png";
      case XMB_TEXTURE_CLOCK:
         return "clock.png";
      case XMB_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case XMB_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case XMB_TEXTURE_BATTERY_80:
         return "battery-80.png";
      case XMB_TEXTURE_BATTERY_60:
         return "battery-60.png";
      case XMB_TEXTURE_BATTERY_40:
         return "battery-40.png";
      case XMB_TEXTURE_BATTERY_20:
         return "battery-20.png";
      case XMB_TEXTURE_POINTER:
         return "pointer.png";
      case XMB_TEXTURE_SAVESTATE:
         return "savestate.png";
      case XMB_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case XMB_TEXTURE_UNDO:
         return "undo.png";
      case XMB_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case XMB_TEXTURE_BLUETOOTH:
         return "bluetooth.png";
      case XMB_TEXTURE_WIFI:
         return "wifi.png";
      case XMB_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case XMB_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case XMB_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case XMB_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case XMB_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case XMB_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case XMB_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case XMB_TEXTURE_RELOAD:
         return "reload.png";
      case XMB_TEXTURE_RENAME:
         return "rename.png";
      case XMB_TEXTURE_FILE:
         return "file.png";
      case XMB_TEXTURE_FOLDER:
         return "folder.png";
      case XMB_TEXTURE_ZIP:
         return "zip.png";
      case XMB_TEXTURE_MUSIC:
         return "music.png";
      case XMB_TEXTURE_FAVORITE:
         return "favorites-content.png";
      case XMB_TEXTURE_IMAGE:
         return "image.png";
      case XMB_TEXTURE_MOVIE:
         return "movie.png";
      case XMB_TEXTURE_CORE:
         return "core.png";
      case XMB_TEXTURE_RDB:
         return "database.png";
      case XMB_TEXTURE_CURSOR:
         return "cursor.png";
      case XMB_TEXTURE_SWITCH_ON:
         return "on.png";
      case XMB_TEXTURE_SWITCH_OFF:
         return "off.png";
      case XMB_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case XMB_TEXTURE_NETPLAY:
         return "netplay.png";
      case XMB_TEXTURE_ROOM:
         return "menu_room.png";
      case XMB_TEXTURE_ROOM_LAN:
         return "menu_room_lan.png";
      case XMB_TEXTURE_ROOM_RELAY:
         return "menu_room_relay.png";
#endif
      case XMB_TEXTURE_KEY:
         return "key.png";
      case XMB_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case XMB_TEXTURE_DIALOG_SLICE:
         return "dialog-slice.png";
      case XMB_TEXTURE_ACHIEVEMENTS:
         return "menu_achievements.png";
      case XMB_TEXTURE_AUDIO:
         return "menu_audio.png";
      case XMB_TEXTURE_DRIVERS:
         return "menu_drivers.png";
      case XMB_TEXTURE_EXIT:
         return "menu_exit.png";
      case XMB_TEXTURE_FRAMESKIP:
         return "menu_frameskip.png";
      case XMB_TEXTURE_HELP:
         return "menu_help.png";
      case XMB_TEXTURE_INFO:
         return "menu_info.png";
      case XMB_TEXTURE_INPUT_SETTINGS:
         return "Libretro - Pad.png";
      case XMB_TEXTURE_LATENCY:
         return "menu_latency.png";
      case XMB_TEXTURE_NETWORK:
         return "menu_network.png";
      case XMB_TEXTURE_POWER:
         return "menu_power.png";
      case XMB_TEXTURE_RECORD:
         return "menu_record.png";
      case XMB_TEXTURE_SAVING:
         return "menu_saving.png";
      case XMB_TEXTURE_UPDATER:
         return "menu_updater.png";
      case XMB_TEXTURE_VIDEO:
         return "menu_video.png";
      case XMB_TEXTURE_MIXER:
         return "menu_mixer.png";
      case XMB_TEXTURE_LOG:
         return "menu_log.png";
      case XMB_TEXTURE_OSD:
         return "menu_osd.png";
      case XMB_TEXTURE_UI:
         return "menu_ui.png";
      case XMB_TEXTURE_USER:
         return "menu_user.png";
      case XMB_TEXTURE_PRIVACY:
         return "menu_privacy.png";
      case XMB_TEXTURE_PLAYLIST:
         return "menu_playlist.png";
      case XMB_TEXTURE_DISC:
         return "disc.png";
      case XMB_TEXTURE_QUICKMENU:
         return "menu_quickmenu.png";
      case XMB_TEXTURE_REWIND:
         return "menu_rewind.png";
      case XMB_TEXTURE_OVERLAY:
         return "menu_overlay.png";
      case XMB_TEXTURE_OVERRIDE:
         return "menu_override.png";
      case XMB_TEXTURE_NOTIFICATIONS:
         return "menu_notifications.png";
      case XMB_TEXTURE_STREAM:
         return "menu_stream.png";
      case XMB_TEXTURE_SHUTDOWN:
         return "menu_shutdown.png";
      case XMB_TEXTURE_INPUT_DPAD_U:
         return "input_DPAD-U.png";
      case XMB_TEXTURE_INPUT_DPAD_D:
         return "input_DPAD-D.png";
      case XMB_TEXTURE_INPUT_DPAD_L:
         return "input_DPAD-L.png";
      case XMB_TEXTURE_INPUT_DPAD_R:
         return "input_DPAD-R.png";
      case XMB_TEXTURE_INPUT_STCK_U:
         return "input_STCK-U.png";
      case XMB_TEXTURE_INPUT_STCK_D:
         return "input_STCK-D.png";
      case XMB_TEXTURE_INPUT_STCK_L:
         return "input_STCK-L.png";
      case XMB_TEXTURE_INPUT_STCK_R:
         return "input_STCK-R.png";
      case XMB_TEXTURE_INPUT_STCK_P:
         return "input_STCK-P.png";
      case XMB_TEXTURE_INPUT_BTN_U:
         return "input_BTN-U.png";
      case XMB_TEXTURE_INPUT_BTN_D:
         return "input_BTN-D.png";
      case XMB_TEXTURE_INPUT_BTN_L:
         return "input_BTN-L.png";
      case XMB_TEXTURE_INPUT_BTN_R:
         return "input_BTN-R.png";
      case XMB_TEXTURE_INPUT_LB:
         return "input_LB.png";
      case XMB_TEXTURE_INPUT_RB:
         return "input_RB.png";
      case XMB_TEXTURE_INPUT_LT:
         return "input_LT.png";
      case XMB_TEXTURE_INPUT_RT:
         return "input_RT.png";
      case XMB_TEXTURE_INPUT_SELECT:
         return "input_SELECT.png";
      case XMB_TEXTURE_INPUT_START:
         return "input_START.png";
      case XMB_TEXTURE_INPUT_ADC:
         return "input_ADC.png";
      case XMB_TEXTURE_INPUT_BIND_ALL:
         return "input_BIND_ALL.png";
      case XMB_TEXTURE_INPUT_MOUSE:
         return "input_MOUSE.png";
      case XMB_TEXTURE_INPUT_LGUN:
         return "input_LGUN.png";
      case XMB_TEXTURE_INPUT_TURBO:
         return "input_TURBO.png";
      case XMB_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case XMB_TEXTURE_MENU_ADD:
         return "menu_add.png";
      case XMB_TEXTURE_BRIGHTNESS:
         return "menu_brightness.png";
      case XMB_TEXTURE_PAUSE:
         return "menu_pause.png";
      case XMB_TEXTURE_DEFAULT:
         return "default.png";
      case XMB_TEXTURE_DEFAULT_CONTENT:
         return "default-content.png";
      case XMB_TEXTURE_MENU_APPLY_TOGGLE:
         return "menu_apply_toggle.png";
      case XMB_TEXTURE_MENU_APPLY_COG:
         return "menu_apply_cog.png";
   }
   return NULL;
}

static void xmb_context_reset_textures(
      xmb_handle_t *xmb, const char *iconpath)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   xmb->assets_missing = false;

   gfx_display_deinit_white_texture();
   gfx_display_init_white_texture();

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
   {
      if (!gfx_display_reset_textures_list(xmb_texture_path(i), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
      {
         RARCH_WARN("[XMB]: Asset missing: \"%s%s\".\n", iconpath, xmb_texture_path(i));
         /* New extra battery icons could be missing */
         if (i == XMB_TEXTURE_BATTERY_80 || i == XMB_TEXTURE_BATTERY_60 || i == XMB_TEXTURE_BATTERY_40 || i == XMB_TEXTURE_BATTERY_20)
         {
            if (  /* If there are no extra battery icons revert to the old behaviour */
                  !gfx_display_reset_textures_list(xmb_texture_path(XMB_TEXTURE_BATTERY_FULL), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL)
                  && !(settings->uints.menu_xmb_theme == XMB_ICON_THEME_CUSTOM)
               )
               goto error;
            else continue;
         }
         /* If the icon is missing return the subsetting (because some themes are incomplete) */
         if (!(i == XMB_TEXTURE_DIALOG_SLICE || i == XMB_TEXTURE_KEY_HOVER || i == XMB_TEXTURE_KEY))
         {
            /* OSD Warning only if subsetting icon is missing */
            if (
                  !gfx_display_reset_textures_list(xmb_texture_path(XMB_TEXTURE_SUBSETTING), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL)
                  && !(settings->uints.menu_xmb_theme == XMB_ICON_THEME_CUSTOM)
               )
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               /* Do not draw icons if subsetting is missing */
               goto error;
            }
            /* Do not draw icons if this ones are is missing */
            switch (i)
            {
               case XMB_TEXTURE_POINTER:
               case XMB_TEXTURE_ARROW:
               case XMB_TEXTURE_CLOCK:
               case XMB_TEXTURE_BATTERY_CHARGING:
               case XMB_TEXTURE_BATTERY_FULL:
               case XMB_TEXTURE_DEFAULT:
               case XMB_TEXTURE_DEFAULT_CONTENT:
                  goto error;
            }
         }
      }
   }

   xmb->main_menu_node.icon     = xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
   xmb->main_menu_node.alpha    = xmb->categories_active_alpha;
   xmb->main_menu_node.zoom     = xmb->categories_active_zoom;

   xmb->settings_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS];
   xmb->settings_tab_node.alpha = xmb->categories_active_alpha;
   xmb->settings_tab_node.zoom  = xmb->categories_active_zoom;

   xmb->history_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_HISTORY];
   xmb->history_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->history_tab_node.zoom   = xmb->categories_active_zoom;

   xmb->favorites_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_FAVORITES];
   xmb->favorites_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->favorites_tab_node.zoom   = xmb->categories_active_zoom;

   xmb->music_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MUSICS];
   xmb->music_tab_node.alpha    = xmb->categories_active_alpha;
   xmb->music_tab_node.zoom     = xmb->categories_active_zoom;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   xmb->video_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MOVIES];
   xmb->video_tab_node.alpha    = xmb->categories_active_alpha;
   xmb->video_tab_node.zoom     = xmb->categories_active_zoom;
#endif

#ifdef HAVE_IMAGEVIEWER
   xmb->images_tab_node.icon    = xmb->textures.list[XMB_TEXTURE_IMAGES];
   xmb->images_tab_node.alpha   = xmb->categories_active_alpha;
   xmb->images_tab_node.zoom    = xmb->categories_active_zoom;
#endif

   xmb->add_tab_node.icon       = xmb->textures.list[XMB_TEXTURE_ADD];
   xmb->add_tab_node.alpha      = xmb->categories_active_alpha;
   xmb->add_tab_node.zoom       = xmb->categories_active_zoom;

#if defined(HAVE_LIBRETRODB)
   xmb->explore_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
   xmb->explore_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->explore_tab_node.zoom   = xmb->categories_active_zoom;
#endif

   xmb->contentless_cores_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
   xmb->contentless_cores_tab_node.alpha = xmb->categories_active_alpha;
   xmb->contentless_cores_tab_node.zoom  = xmb->categories_active_zoom;

#ifdef HAVE_NETWORKING
   xmb->netplay_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_NETPLAY];
   xmb->netplay_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->netplay_tab_node.zoom   = xmb->categories_active_zoom;
#endif

   /* Recolor */
   if (
         (settings->uints.menu_xmb_theme == XMB_ICON_THEME_MONOCHROME_INVERTED) ||
         (settings->uints.menu_xmb_theme == XMB_ICON_THEME_AUTOMATIC_INVERTED)
      )
      memcpy(item_color, xmb_coord_black, sizeof(item_color));
   else
   {
      if (
            (settings->uints.menu_xmb_theme == XMB_ICON_THEME_MONOCHROME) ||
            (settings->uints.menu_xmb_theme == XMB_ICON_THEME_AUTOMATIC)
         )
      {
         for (i=0;i<16;i++)
         {
            if ((i==3) || (i==7) || (i==11) || (i==15))
            {
               item_color[i] = 1;
               continue;
            }
            item_color[i] = 0.95;
         }
      }
      else
         memcpy(item_color, xmb_coord_white, sizeof(item_color));
   }

   return;

error:
   xmb->assets_missing = true;
   RARCH_WARN("[XMB]: Critical asset missing, no icons will be drawn.\n");
}

static void xmb_context_reset_background(xmb_handle_t *xmb, const char *iconpath)
{
   char path[PATH_MAX_LENGTH];
   settings_t *settings        = config_get_ptr();
   const char *path_menu_wp    = settings->paths.path_menu_wallpaper;

   path[0] = '\0';

   /* Dynamic wallpaper takes precedence as reset background,
    * then comes 'menu_wallpaper', and then iconset 'bg.png' */
   if (settings->bools.menu_dynamic_wallpaper_enable)
      strlcpy(path, xmb_path_dynamic_wallpaper(xmb), sizeof(path));

   if (!string_is_empty(path) && path_is_valid(path))
   {
      task_push_image_load(path,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);
   }
   else if (!string_is_empty(path_menu_wp))
   {
      if (path_is_valid(path_menu_wp))
         task_push_image_load(path_menu_wp,
               video_driver_supports_rgba(), 0,
               menu_display_handle_wallpaper_upload, NULL);
   }
   else if (!string_is_empty(iconpath))
   {
      fill_pathname_join(path, iconpath,
            FILE_PATH_BACKGROUND_IMAGE, sizeof(path));

      if (path_is_valid(path))
         task_push_image_load(path,
               video_driver_supports_rgba(), 0,
               menu_display_handle_wallpaper_upload, NULL);
   }

#ifdef ORBIS
   /* To avoid weird behaviour on orbis with remote host */
   sleep(5);
#endif
}

static void xmb_context_reset_internal(xmb_handle_t *xmb,
      bool is_threaded, bool reinit_textures)
{
   char iconpath[PATH_MAX_LENGTH];
   char bg_file_path[PATH_MAX_LENGTH];
   gfx_display_t *p_disp               = disp_get_ptr();
   const char *wideglyph_str = msg_hash_get_wideglyph_str();
   iconpath[0]       = bg_file_path[0] = '\0';

   fill_pathname_application_special(bg_file_path,
         sizeof(bg_file_path), APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

   if (!string_is_empty(bg_file_path))
   {
      if (!string_is_empty(xmb->bg_file_path))
         free(xmb->bg_file_path);
      xmb->bg_file_path = strdup(bg_file_path);
   }

   fill_pathname_application_special(iconpath, sizeof(iconpath),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

   xmb_layout(xmb);
   if (xmb->font)
   {
      gfx_display_font_free(xmb->font);
      xmb->font = NULL;
   }
   if (xmb->font2)
   {
      gfx_display_font_free(xmb->font2);
      xmb->font2 = NULL;
   }
   xmb->font = gfx_display_font(p_disp,
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
         xmb->font_size,
         is_threaded);
   xmb->font2 = gfx_display_font(p_disp,
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
         xmb->font2_size,
         is_threaded);

   xmb->wideglyph_width = 100;

   if (wideglyph_str)
   {
      int char_width =
         font_driver_get_message_width(xmb->font, "a", 1, 1);
      int wideglyph_width =
         font_driver_get_message_width(xmb->font, wideglyph_str, (unsigned)strlen(wideglyph_str), 1);

      if (wideglyph_width > 0 && char_width > 0)
         xmb->wideglyph_width = wideglyph_width * 100 / char_width;
   }

   if (reinit_textures)
   {
      xmb_context_reset_textures(xmb, iconpath);
      xmb_context_reset_background(xmb, iconpath);
   }

   xmb_context_reset_horizontal_list(xmb);

   menu_screensaver_context_destroy(xmb->screensaver);

   /* Only reload thumbnails if:
    * > Thumbnails are enabled
    * > This is a playlist, a database list, a file list
    *   or the quick menu */
   if (gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT) ||
       gfx_thumbnail_is_enabled(xmb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
   {
      unsigned depth          = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
      unsigned xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);

      if (((((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && depth == 1) ||
             (xmb_system_tab < XMB_SYSTEM_TAB_SETTINGS && depth == 4)) &&
              xmb->is_playlist)) ||
            xmb->is_db_manager_list ||
            xmb->is_file_list ||
            xmb->is_quick_menu)
         xmb_update_thumbnail_image(xmb);
   }

   xmb_update_savestate_thumbnail_image(xmb);
}

static void xmb_context_reset(void *data, bool is_threaded)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (xmb)
      xmb_context_reset_internal(xmb, is_threaded, true);
   video_driver_monitor_reset();
}

static void xmb_navigation_clear(void *data, bool pending_push)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   if (!pending_push)
      xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_pointer_changed(void *data)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_set(void *data, bool scroll)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_alphabet(void *data, size_t *unused)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
      size_t list_size,
      unsigned entry_type)
{
   int current            = 0;
   int i                  = (int)list_size;
   xmb_node_t *node       = NULL;
   xmb_handle_t *xmb      = (xmb_handle_t*)userdata;
   size_t selection       = menu_navigation_get_selection();

   if (!xmb || !list)
      return;

   node = (xmb_node_t*)list->list[i].userdata;

   if (!node)
      node = xmb_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   current           = (int)selection;

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath = strdup(fullpath);
   }

   node->alpha       = xmb->items_passive_alpha;
   node->zoom        = xmb->items_passive_zoom;
   node->label_alpha = node->alpha;
   node->y           = xmb_item_y(xmb, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = xmb->items_active_alpha;
      node->label_alpha = xmb->items_active_alpha;
      node->zoom        = xmb->items_active_alpha;
   }

   list->list[i].userdata = node;
}

static void xmb_list_clear(file_list_t *list)
{
   uintptr_t tag = (uintptr_t)list;

   gfx_animation_kill_by_tag(&tag);

   xmb_free_list_nodes(list, false);
}

static void xmb_list_free(file_list_t *list, size_t a, size_t b)
{
   xmb_list_clear(list);
}

static void xmb_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j   = 0;
   uintptr_t tag = (uintptr_t)dst;

   gfx_animation_kill_by_tag(&tag);

   xmb_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];

      void *src_udata = s->userdata;
      void *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         dst->list[j].userdata = (void*)xmb_copy_node((const xmb_node_t*)src_udata);

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

static void xmb_list_cache(void *data, enum menu_list_type type, unsigned action)
{
   size_t stack_size, list_size;
   xmb_handle_t      *xmb         = (xmb_handle_t*)data;
   file_list_t *menu_stack        = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf     = menu_entries_get_selection_buf_ptr(0);
   size_t selection               = menu_navigation_get_selection();
   settings_t *settings           = config_get_ptr();
   bool menu_horizontal_animation = settings->bools.menu_horizontal_animation;

   if (!xmb)
      return;

   /* Check whether to enable the horizontal animation. */
   if (menu_horizontal_animation)
   {
      unsigned first  = 0, last = 0;
      unsigned height = 0;
      video_driver_get_size(NULL, &height);

      /* FIXME: this shouldn't be happening at all */
      if (selection >= selection_buf->size)
         selection = selection_buf->size ? selection_buf->size - 1 : 0;

      xmb->selection_ptr_old = selection;

      xmb_calculate_visible_range(xmb, height, selection_buf->size,
            (unsigned)xmb->selection_ptr_old, &first, &last);

      xmb_list_deep_copy(selection_buf,
            &xmb->selection_buf_old, first, last);

      xmb->selection_ptr_old -= first;
      last                   -= first;
      first                   = 0;
   }
   else
      xmb->selection_ptr_old = 0;

   list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         xmb->categories_selection_ptr_old = xmb->categories_selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (xmb->categories_selection_ptr == 0)
               {
                  xmb->categories_selection_ptr = list_size;
                  xmb->categories_active_idx    = (unsigned)(list_size - 1);
               }
               else
                  xmb->categories_selection_ptr--;
               break;
            default:
               if (xmb->categories_selection_ptr == list_size)
               {
                  xmb->categories_selection_ptr = 0;
                  xmb->categories_active_idx    = 1;
               }
               else
                  xmb->categories_selection_ptr++;
               break;
         }

         stack_size = menu_stack->size;

         if (menu_stack->list[stack_size - 1].label)
            free(menu_stack->list[stack_size - 1].label);
         menu_stack->list[stack_size - 1].label = NULL;

         switch (xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr))
         {
            case XMB_SYSTEM_TAB_MAIN:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS;
               break;
            case XMB_SYSTEM_TAB_SETTINGS:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS_TAB;
               break;
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_IMAGES_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_MUSIC:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_MUSIC_TAB;
               break;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            case XMB_SYSTEM_TAB_VIDEO:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_VIDEO_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_HISTORY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_HISTORY_TAB;
               break;
            case XMB_SYSTEM_TAB_FAVORITES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_FAVORITES_TAB;
               break;
#ifdef HAVE_NETWORKING
            case XMB_SYSTEM_TAB_NETPLAY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_NETPLAY_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_ADD:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_ADD_TAB;
               break;
#if defined(HAVE_LIBRETRODB)
            case XMB_SYSTEM_TAB_EXPLORE:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_EXPLORE_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_CONTENTLESS_CORES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_CONTENTLESS_CORES_TAB;
               break;
            default:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTING_HORIZONTAL_MENU;
               break;
         }
         break;
      default:
         break;
   }
}

static void xmb_context_destroy(void *data)
{
   unsigned i;
   xmb_handle_t *xmb   = (xmb_handle_t*)data;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&xmb->textures.list[i]);

   xmb_unload_thumbnail_textures(xmb);

   xmb_context_destroy_horizontal_list(xmb);
   xmb_context_bg_destroy(xmb);

   gfx_display_font_free(xmb->font);
   gfx_display_font_free(xmb->font2);

   xmb->font = NULL;
   xmb->font2 = NULL;

   menu_screensaver_context_destroy(xmb->screensaver);
}

static void xmb_toggle(void *userdata, bool menu_on)
{
   gfx_animation_ctx_entry_t entry;
   bool tmp             = false;
   xmb_handle_t *xmb    = (xmb_handle_t*)userdata;

   if (!xmb)
      return;

   xmb->depth         = (int)xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (!menu_on)
   {
      xmb->alpha = 0;
      return;
   }

   /* Have to reset this, otherwise savestate
    * thumbnail won't update after selecting
    * 'save state' option */
   gfx_thumbnail_reset(&xmb->thumbnails.savestate);

   entry.duration     = XMB_DELAY * 2;
   entry.target_value = 1.0f;
   entry.subject      = &xmb->alpha;
   entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   gfx_animation_push(&entry);

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   xmb_toggle_horizontal_list(xmb);
}

static int xmb_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   settings_t *settings = config_get_ptr();
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info, settings))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int xmb_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = xmb_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int xmb_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (xmb_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int xmb_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   int ret                         = -1;
   core_info_list_t *list          = NULL;
   settings_t *settings            = config_get_ptr();
   bool menu_show_load_core        = settings->bools.menu_show_load_core;
   bool menu_show_load_content     = settings->bools.menu_show_load_content;
   bool menu_content_show_pl       = settings->bools.menu_content_show_playlists;
   bool menu_show_configurations   = settings->bools.menu_show_configurations;
   bool menu_show_load_disc        = settings->bools.menu_show_load_disc;
   bool menu_show_dump_disc        = settings->bools.menu_show_dump_disc;
#ifdef HAVE_LAKKA
   bool menu_show_eject_disc        = settings->bools.menu_show_eject_disc;
#endif
   bool menu_show_shutdown         = settings->bools.menu_show_shutdown;
   bool menu_show_reboot           = settings->bools.menu_show_reboot;
#if !defined(IOS)
   bool menu_show_quit_retroarch   = settings->bools.menu_show_quit_retroarch;
   bool menu_show_restart_ra       = settings->bools.menu_show_restart_retroarch;
#endif
   bool menu_show_information      = settings->bools.menu_show_information;
   bool menu_show_help             = settings->bools.menu_show_help;
   bool kiosk_mode_enable          = settings->bools.kiosk_mode_enable;
#ifdef HAVE_QT
   bool desktop_menu_enable        = settings->bools.desktop_menu_enable;
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_ONLINE_UPDATER)
   bool menu_show_online_updater   = settings->bools.menu_show_online_updater;
#endif
#if defined(HAVE_MIST)
   bool menu_show_core_manager_steam = settings->bools.menu_show_core_manager_steam;
#endif
   bool menu_content_show_settings = settings->bools.menu_content_show_settings;
   const char *menu_content_show_settings_password =
      settings->paths.menu_content_show_settings_password;
   const char *kiosk_mode_password = settings->paths.kiosk_mode_password;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
               msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
               MENU_ENUM_LABEL_FAVORITES,
               MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0);

         core_info_get_list(&list);
         if (list->info_count > 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                  MENU_SETTING_ACTION, 0, 0);

         if (menu_content_show_pl)
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
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                  MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                  MENU_SETTING_ACTION, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         ret = 0;
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
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
                  if (menu_show_load_core)
                  {
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                           info->list,
                           MENU_ENUM_LABEL_CORE_LIST,
                           PARSE_ACTION,
                           false);
                  }
               }
            }

            if (menu_show_load_content)
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

            if (menu_show_load_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_DISC,
                     PARSE_ACTION,
                     false);
            }

            if (menu_show_dump_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_DUMP_DISC,
                     PARSE_ACTION,
                     false);
            }

#ifdef HAVE_LAKKA
            if (menu_show_eject_disc)
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
            if (desktop_menu_enable)
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
            if (menu_show_online_updater && !kiosk_mode_enable)
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
            if (menu_show_core_manager_steam && !kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,
                  PARSE_ACTION,
                  false);
            }
#endif
            if (  !menu_content_show_settings &&
                  !string_is_empty(menu_content_show_settings_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
                     PARSE_ACTION,
                     false);
            }

            if (kiosk_mode_enable && !string_is_empty(kiosk_mode_password))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE,
                     PARSE_ACTION,
                     false);
            }

            if (menu_show_information)
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

            if (menu_show_configurations && !kiosk_mode_enable)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION,
                     false);
            }

            if (menu_show_help)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION,
                     false);
            }

#if !defined(IOS)
            if (menu_show_restart_ra)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION,
                     false);
            }

            if (menu_show_quit_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION,
                     false);
            }
#endif
            if (menu_show_reboot)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION,
                     false);
            }

            if (menu_show_shutdown)
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

static bool xmb_menu_init_list(void *data)
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

static int xmb_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width;
   unsigned height;
   int16_t margin_top;
   int16_t margin_left;
   int16_t margin_right;
   xmb_handle_t *xmb      = (xmb_handle_t*)userdata;
   size_t selection       = menu_navigation_get_selection();
   unsigned end           = (unsigned)menu_entries_get_size();

   if (!xmb)
      return -1;

   /* If fullscreen thumbnail view is enabled,
    * all input will disable it and otherwise
    * be ignored */
   if (xmb->show_fullscreen_thumbnails)
   {
      xmb_hide_fullscreen_thumbnails(xmb, true);
      return 0;
   }

   video_driver_get_size(&width, &height);
   margin_top   = (int16_t)xmb->margins_screen_top;
   margin_left  = (int16_t)xmb->margins_screen_left;
   margin_right = (int16_t)((float)width - xmb->margins_screen_left);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         /* - A touch in the left margin:
          *   > ...triggers a 'cancel' action beneath the top margin
          *   > ...triggers a 'search' action above the top margin
          * - A touch in the right margin triggers a 'select' action
          *   for the current item
          * - Between the left/right margins input is handled normally */
         if (x < margin_left)
         {
            if (y >= margin_top)
               return xmb_menu_entry_action(xmb,
                     entry, selection, MENU_ACTION_CANCEL);
            return menu_input_dialog_start_search() ? 0 : -1;
         }
         else if (x > margin_right)
            return xmb_menu_entry_action(xmb,
                  entry, selection, MENU_ACTION_SELECT);
         else if (ptr <= (end - 1))
         {
            /* If pointer item is already 'active', perform 'select' action */
            if (ptr == selection)
               return xmb_menu_entry_action(xmb,
                     entry, selection, MENU_ACTION_SELECT);

            /* ...otherwise navigate to the current pointer item */
            menu_navigation_set_selection(ptr);
            xmb_navigation_set(xmb, false);
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if ((ptr <= end - 1) && (ptr == selection))
            return xmb_menu_entry_action(xmb,
                  entry, selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         /* Navigate left
          * Note: At the top level, navigating left
          * means switching to the 'next' horizontal list,
          * which is actually a movement to the *right* */
         if (y > margin_top)
            xmb_menu_entry_action(xmb,
                  entry, selection,
                  (xmb->depth == 1) ? MENU_ACTION_RIGHT : MENU_ACTION_LEFT);
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         /* Navigate right
          * Note: At the top level, navigating right
          * means switching to the 'previous' horizontal list,
          * which is actually a movement to the *left* */
         if (y > margin_top)
            xmb_menu_entry_action(xmb,
                  entry, selection,
                  (xmb->depth == 1) ? MENU_ACTION_LEFT : MENU_ACTION_RIGHT);
         break;
      case MENU_INPUT_GESTURE_SWIPE_UP:
         /* Swipe up in left margin: ascend alphabet */
         if (x < margin_left)
            xmb_menu_entry_action(xmb,
                  entry, selection, MENU_ACTION_SCROLL_DOWN);
         else if (x < margin_right)
         {
            /* Swipe up between left and right margins:
             * move selection pointer down by 1 'page' */
            unsigned first = 0;
            unsigned last  = end;

            if (height)
               xmb_calculate_visible_range(xmb, height,
                     end, (unsigned)selection, &first, &last);

            if (last < end)
            {
               menu_navigation_set_selection((size_t)last);
               xmb_navigation_set(xmb, true);
            }
            else
               menu_driver_ctl(MENU_NAVIGATION_CTL_SET_LAST, NULL);
         }
         break;
      case MENU_INPUT_GESTURE_SWIPE_DOWN:
         /* Swipe down in left margin: descend alphabet */
         if (x < margin_left)
            xmb_menu_entry_action(xmb,
                  entry, selection, MENU_ACTION_SCROLL_UP);
         else if (x < margin_right)
         {
            /* Swipe down between left and right margins:
             * move selection pointer up by 1 'page' */
            unsigned bottom_idx = (unsigned)selection + 1;
            size_t new_idx      = 0;
            unsigned step       = 0;

            /* Determine index of entry at bottom of screen
             * Note: cannot use xmb_calculate_visible_range()
             * here because there may not be sufficient entries
             * to reach the bottom of the screen - i.e. we just
             * want an index offset to subtract from the current
             * selection... */
            for (;;)
            {
               float top = xmb_item_y(xmb, bottom_idx, selection)
                  + xmb->margins_screen_top;

               if (top > height)
               {
                  /* Since this checks the top position, the
                   * final index is always 1 greater than it
                   * should be... */
                  bottom_idx--;
                  break;
               }

               bottom_idx++;
            }

            if (bottom_idx >= selection)
               step     = bottom_idx - selection;
            if (selection > step)
               new_idx  = selection - step;

            if (new_idx > 0)
            {
               menu_navigation_set_selection(new_idx);
               xmb_navigation_set(xmb, true);
            }
            else
            {
               bool pending_push = false;
               menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
            }
         }
         break;
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_messagebox,
   xmb_render,
   xmb_frame,
   xmb_init,
   xmb_free,
   xmb_context_reset,
   xmb_context_destroy,
   xmb_populate_entries,
   xmb_toggle,
   xmb_navigation_clear,
   NULL, /*xmb_navigation_pointer_changed,*/ /* Note: navigation_set() is called each time navigation_increment/decrement() */
   NULL, /*xmb_navigation_pointer_changed,*/ /* is called, so linking these just duplicates work... */
   xmb_navigation_set,
   xmb_navigation_pointer_changed,
   xmb_navigation_alphabet,
   xmb_navigation_alphabet,
   xmb_menu_init_list,
   xmb_list_insert,
   NULL,
   xmb_list_free,
   xmb_list_clear,
   xmb_list_cache,
   xmb_list_push,
   xmb_list_get_selection,
   xmb_list_get_size,
   xmb_list_get_entry,
   NULL,
   xmb_list_bind_init,
   xmb_load_image,
   "xmb",
   xmb_environ,
   NULL,
   xmb_update_thumbnail_image,
   xmb_refresh_thumbnail_image,
   xmb_set_thumbnail_system,
   xmb_get_thumbnail_system,
   xmb_set_thumbnail_content,
   gfx_display_osk_ptr_at_pos,
   xmb_update_savestate_thumbnail_path,
   xmb_update_savestate_thumbnail_image,
   NULL, /* pointer_down */
   xmb_pointer_up,
   xmb_menu_entry_action
};
