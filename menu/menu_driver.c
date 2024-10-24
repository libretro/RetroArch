/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#if defined(HAVE_CONFIG_H)
#include "../config.h"
#endif

#include <retro_timers.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>
#include <compat/strcasestr.h>
#include <encodings/utf.h>
#include <streams/file_stream.h>
#include <time/rtime.h>

#ifdef WIIU
#include <wiiu/os/energy.h>
#endif

#ifdef HAVE_ACCESSIBILITY
#include "../accessibility.h"
#endif

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

#include "../audio/audio_driver.h"

#include "menu_driver.h"
#include "menu_cbs.h"
#include "../driver.h"
#include "../list_special.h"
#include "../paths.h"
#include "../tasks/task_powerstate.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"

#include "../frontend/frontend_driver.h"

#ifdef HAVE_LANGEXTRA
/* This file has a UTF8 BOM, we assume HAVE_LANGEXTRA
 * is only enabled for compilers that can support this. */
#include "../input/input_osk_utf8_pages.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos_menu.h"
#endif

#include "../gfx/gfx_animation.h"
#include "../input/input_driver.h"
#include "../input/input_remapping.h"
#include "../performance_counters.h"
#include "../version.h"
#include "../misc/cpufreq/cpufreq.h"

#ifdef HAVE_LIBNX
#include <switch.h>
#include "../switch_performance_profiles.h"
#endif

#ifdef HAVE_MIST
#include "../steam/steam.h"
#endif

typedef struct menu_input_ctx_bind
{
   char *s;
   size_t len;
} menu_input_ctx_bind_t;

#ifdef HAVE_LIBNX
#define LIBNX_SWKBD_LIMIT 500 /* enforced by HOS */

/* TODO/FIXME - public global variable */
extern u32 __nx_applet_type;
#endif

struct key_desc key_descriptors[RARCH_MAX_KEYS] =
{
   {RETROK_FIRST,         "Unmapped"},
   {RETROK_BACKSPACE,     "Backspace"},
   {RETROK_TAB,           "Tab"},
   {RETROK_CLEAR,         "Clear"},
   {RETROK_RETURN,        "Return"},
   {RETROK_PAUSE,         "Pause"},
   {RETROK_ESCAPE,        "Escape"},
   {RETROK_SPACE,         "Space"},
   {RETROK_EXCLAIM,       "!"},
   {RETROK_QUOTEDBL,      "\""},
   {RETROK_HASH,          "#"},
   {RETROK_DOLLAR,        "$"},
   {RETROK_AMPERSAND,     "&"},
   {RETROK_QUOTE,         "\'"},
   {RETROK_LEFTPAREN,     "("},
   {RETROK_RIGHTPAREN,    ")"},
   {RETROK_ASTERISK,      "*"},
   {RETROK_PLUS,          "+"},
   {RETROK_COMMA,         ","},
   {RETROK_MINUS,         "-"},
   {RETROK_PERIOD,        "."},
   {RETROK_SLASH,         "/"},
   {RETROK_0,             "0"},
   {RETROK_1,             "1"},
   {RETROK_2,             "2"},
   {RETROK_3,             "3"},
   {RETROK_4,             "4"},
   {RETROK_5,             "5"},
   {RETROK_6,             "6"},
   {RETROK_7,             "7"},
   {RETROK_8,             "8"},
   {RETROK_9,             "9"},
   {RETROK_COLON,         ":"},
   {RETROK_SEMICOLON,     ";"},
   {RETROK_LESS,          "<"},
   {RETROK_EQUALS,        "="},
   {RETROK_GREATER,       ">"},
   {RETROK_QUESTION,      "?"},
   {RETROK_AT,            "@"},
   {RETROK_LEFTBRACKET,   "["},
   {RETROK_BACKSLASH,     "\\"},
   {RETROK_RIGHTBRACKET,  "]"},
   {RETROK_CARET,         "^"},
   {RETROK_UNDERSCORE,    "_"},
   {RETROK_BACKQUOTE,     "`"},
   {RETROK_a,             "A"},
   {RETROK_b,             "B"},
   {RETROK_c,             "C"},
   {RETROK_d,             "D"},
   {RETROK_e,             "E"},
   {RETROK_f,             "F"},
   {RETROK_g,             "G"},
   {RETROK_h,             "H"},
   {RETROK_i,             "I"},
   {RETROK_j,             "J"},
   {RETROK_k,             "K"},
   {RETROK_l,             "L"},
   {RETROK_m,             "M"},
   {RETROK_n,             "N"},
   {RETROK_o,             "O"},
   {RETROK_p,             "P"},
   {RETROK_q,             "Q"},
   {RETROK_r,             "R"},
   {RETROK_s,             "S"},
   {RETROK_t,             "T"},
   {RETROK_u,             "U"},
   {RETROK_v,             "V"},
   {RETROK_w,             "W"},
   {RETROK_x,             "X"},
   {RETROK_y,             "Y"},
   {RETROK_z,             "Z"},
   {RETROK_DELETE,        "Delete"},

   {RETROK_KP0,           "Numpad 0"},
   {RETROK_KP1,           "Numpad 1"},
   {RETROK_KP2,           "Numpad 2"},
   {RETROK_KP3,           "Numpad 3"},
   {RETROK_KP4,           "Numpad 4"},
   {RETROK_KP5,           "Numpad 5"},
   {RETROK_KP6,           "Numpad 6"},
   {RETROK_KP7,           "Numpad 7"},
   {RETROK_KP8,           "Numpad 8"},
   {RETROK_KP9,           "Numpad 9"},
   {RETROK_KP_PERIOD,     "Numpad ."},
   {RETROK_KP_DIVIDE,     "Numpad /"},
   {RETROK_KP_MULTIPLY,   "Numpad *"},
   {RETROK_KP_MINUS,      "Numpad -"},
   {RETROK_KP_PLUS,       "Numpad +"},
   {RETROK_KP_ENTER,      "Numpad Enter"},
   {RETROK_KP_EQUALS,     "Numpad ="},

   {RETROK_UP,            "Up"},
   {RETROK_DOWN,          "Down"},
   {RETROK_RIGHT,         "Right"},
   {RETROK_LEFT,          "Left"},
   {RETROK_INSERT,        "Insert"},
   {RETROK_HOME,          "Home"},
   {RETROK_END,           "End"},
   {RETROK_PAGEUP,        "Page Up"},
   {RETROK_PAGEDOWN,      "Page Down"},

   {RETROK_F1,            "F1"},
   {RETROK_F2,            "F2"},
   {RETROK_F3,            "F3"},
   {RETROK_F4,            "F4"},
   {RETROK_F5,            "F5"},
   {RETROK_F6,            "F6"},
   {RETROK_F7,            "F7"},
   {RETROK_F8,            "F8"},
   {RETROK_F9,            "F9"},
   {RETROK_F10,           "F10"},
   {RETROK_F11,           "F11"},
   {RETROK_F12,           "F12"},
   {RETROK_F13,           "F13"},
   {RETROK_F14,           "F14"},
   {RETROK_F15,           "F15"},

   {RETROK_NUMLOCK,       "Num Lock"},
   {RETROK_CAPSLOCK,      "Caps Lock"},
   {RETROK_SCROLLOCK,     "Scroll Lock"},
   {RETROK_RSHIFT,        "Right Shift"},
   {RETROK_LSHIFT,        "Left Shift"},
   {RETROK_RCTRL,         "Right Control"},
   {RETROK_LCTRL,         "Left Control"},
   {RETROK_RALT,          "Right Alt"},
   {RETROK_LALT,          "Left Alt"},
   {RETROK_RMETA,         "Right Meta"},
   {RETROK_LMETA,         "Left Meta"},
   {RETROK_RSUPER,        "Right Super"},
   {RETROK_LSUPER,        "Left Super"},
   {RETROK_MODE,          "Mode"},
   {RETROK_COMPOSE,       "Compose"},

   {RETROK_HELP,          "Help"},
   {RETROK_PRINT,         "Print"},
   {RETROK_SYSREQ,        "Sys Req"},
   {RETROK_BREAK,         "Break"},
   {RETROK_MENU,          "Menu"},
   {RETROK_POWER,         "Power"},
   {RETROK_EURO,          {-30, -126, -84, 0}}, /* "�" */
   {RETROK_UNDO,          "Undo"},
   {RETROK_OEM_102,       "OEM-102"},

   {RETROK_BROWSER_BACK,      "Back"},
   {RETROK_BROWSER_FORWARD,   "Forward"},
   {RETROK_BROWSER_REFRESH,   "Refresh"},
   {RETROK_BROWSER_STOP,      "Stop"},
   {RETROK_BROWSER_SEARCH,    "Search"},
   {RETROK_BROWSER_FAVORITES, "Favorites"},
   {RETROK_BROWSER_HOME,      "Home Page"},
   {RETROK_VOLUME_MUTE,       "Mute"},
   {RETROK_VOLUME_DOWN,       "Volume Up"},
   {RETROK_VOLUME_UP,         "Volume Down"},
   {RETROK_MEDIA_NEXT,        "Next Track"},
   {RETROK_MEDIA_PREV,        "Previous Track"},
   {RETROK_MEDIA_STOP,        "Media Stop"},
   {RETROK_MEDIA_PLAY_PAUSE,  "Media Play"},
   {RETROK_LAUNCH_MAIL,       "Launch Email"},
   {RETROK_LAUNCH_MEDIA,      "Launch Media"},
   {RETROK_LAUNCH_APP1,       "Launch App1"},
   {RETROK_LAUNCH_APP2,       "Launch App2"}

};

static void *null_menu_init(void **userdata, bool video_is_threaded)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   if (!menu)
      return NULL;
   return menu;
}

static int null_menu_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx) { return 0; }

static menu_ctx_driver_t menu_ctx_null = {
  NULL,  /* set_texture */
  NULL,  /* render_messagebox */
  NULL,  /* render */
  NULL,  /* frame */
  null_menu_init,
  NULL,  /* free */
  NULL,  /* context_reset */
  NULL,  /* context_destroy */
  NULL,  /* populate_entries */
  NULL,  /* toggle */
  NULL,  /* navigation_clear */
  NULL,  /* navigation_decrement */
  NULL,  /* navigation_increment */
  NULL,  /* navigation_set */
  NULL,  /* navigation_set_last */
  NULL,  /* navigation_descend_alphabet */
  NULL,  /* navigation_ascend_alphabet */
  NULL,  /* lists_init */
  NULL,  /* list_insert */
  NULL,  /* list_prepend */
  NULL,  /* list_delete */
  NULL,  /* list_clear */
  NULL,  /* list_cache */
  NULL,  /* list_push */
  NULL,  /* list_get_selection */
  NULL,  /* list_get_size */
  NULL,  /* list_get_entry */
  NULL,  /* list_set_selection */
  null_menu_list_bind_init,
  NULL,  /* load_image */
  "null",
  NULL,  /* environ */
  NULL,  /* update_thumbnail_path */
  NULL,  /* update_thumbnail_image */
  NULL,  /* refresh_thumbnail_image */
  NULL,  /* set_thumbnail_content */
  NULL,  /* osk_ptr_at_pos */
  NULL,  /* update_savestate_thumbnail_path */
  NULL,  /* update_savestate_thumbnail_image */
  NULL,  /* pointer_down */
  NULL,  /* pointer_up   */
  NULL   /* entry_action */
};

/* Menu drivers */
const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_MATERIALUI)
   &menu_ctx_mui,
#endif
#if defined(HAVE_OZONE)
   &menu_ctx_ozone,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif
#if defined(HAVE_XMB)
   &menu_ctx_xmb,
#endif
   &menu_ctx_null,
   NULL
};

static struct menu_state menu_driver_state = { 0 };

struct menu_state *menu_state_get_ptr(void)
{
   return &menu_driver_state;
}

static bool menu_should_pop_stack(const char *label)
{
   /* > Info box */
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFO_SCREEN)))
      return true;
   /* > Help box */
   if (string_starts_with_size(label, "help", STRLEN_CONST("help")))
      if (
               string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOADING_CONTENT))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SCANNING_CONTENT))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING))
            || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION)))
         return true;
   if (
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION)))
      return true;
   return false;
}

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{
   bool path_enabled;
   char newpath[NAME_MAX_LENGTH];
   const char *path            = NULL;
   const char *entry_label     = NULL;
   menu_file_list_cbs_t *cbs   = NULL;
   struct menu_state *menu_st  = &menu_driver_state;
   file_list_t *selection_buf  = MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, stack_idx);
   file_list_t *list           = (userdata) ? (file_list_t*)userdata : selection_buf;
   uint8_t entry_flags         = entry->flags;

   newpath[0]                  = '\0';

   if (!list)
      return;

   path_enabled               = (entry_flags & MENU_ENTRY_FLAG_PATH_ENABLED) ? true : false;

   path                       = list->list[i].path;
   entry_label                = list->list[i].label;
   entry->type                = list->list[i].type;
   entry->entry_idx           = list->list[i].entry_idx;
   entry->setting_type        = 0;

   cbs                        = (menu_file_list_cbs_t*)list->list[i].actiondata;
   entry->idx                 = (unsigned)i;

   if (    (entry_flags & MENU_ENTRY_FLAG_LABEL_ENABLED)
         && !string_is_empty(entry_label))
      strlcpy(entry->label, entry_label, sizeof(entry->label));

   if (cbs)
   {
      const char *label             = NULL;
      file_list_t *menu_stack       = MENU_LIST_GET(menu_st->entries.list, 0);

      entry->enum_idx               = cbs->enum_idx;

      if (cbs->setting && cbs->setting->type)
         entry->setting_type        = cbs->setting->type;

      /* Exceptions without cbs->setting->type */
      if (!entry->setting_type)
      {
         switch (entry->type)
         {
            case MENU_SETTING_ACTION_CORE_LOCK:
               entry->setting_type  = ST_BOOL;
               break;
            default:
               break;
         }
      }

      if (cbs->checked)
         entry->flags |= MENU_ENTRY_FLAG_CHECKED;

      if (menu_stack && menu_stack->size)
         label = menu_stack->list[menu_stack->size - 1].label;

      if (    (entry_flags & MENU_ENTRY_FLAG_RICH_LABEL_ENABLED)
            && cbs->action_label)
      {
         cbs->action_label(list,
               entry->type, (unsigned)i,
               label, path,
               entry->rich_label,
               sizeof(entry->rich_label));

         if (!path_enabled && string_is_empty(entry->rich_label))
            path_enabled = true;
      }

      if ((path_enabled || (entry_flags & MENU_ENTRY_FLAG_VALUE_ENABLED))
          && cbs->action_get_value
          && use_representation)
      {
         cbs->action_get_value(list,
               &entry->spacing, entry->type,
               (unsigned)i, label,
               entry->value,
               (entry_flags & MENU_ENTRY_FLAG_VALUE_ENABLED)
               ? sizeof(entry->value)
               : 0,
               path,
               newpath,
               path_enabled ? sizeof(newpath) : 0);

         if (!string_is_empty(entry->value))
         {
            if (entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
            {
               size_t j;
               size_t size = strlcpy(entry->password_value, entry->value,
                     sizeof(entry->password_value));
               for (j = 0; j < size; j++)
                  entry->password_value[j] = '*';
            }
         }
      }

      if (entry_flags & MENU_ENTRY_FLAG_SUBLABEL_ENABLED)
      {
         if (!string_is_empty(cbs->action_sublabel_cache))
            strlcpy(entry->sublabel,
                     cbs->action_sublabel_cache, sizeof(entry->sublabel));
         else if (cbs->action_sublabel)
         {
            /* If this function callback returns true,
             * we know that the value won't change - so we
             * can cache it instead. */
            if (cbs->action_sublabel(list,
                     entry->type, (unsigned)i,
                     label, path,
                     entry->sublabel,
                     sizeof(entry->sublabel)) > 0)
               strlcpy(cbs->action_sublabel_cache,
                     entry->sublabel,
                     sizeof(cbs->action_sublabel_cache));
         }
      }
   }

   /* Inspect core options and set entries with only 2 options as
    * boolean for accurate graphical switch icons */
   if (     entry->type >= MENU_SETTINGS_CORE_OPTION_START
         && entry->type < MENU_SETTINGS_CHEEVOS_START)
   {
      struct core_option *option      = NULL;
      core_option_manager_t *coreopts = NULL;
      size_t option_index             = entry->type - MENU_SETTINGS_CORE_OPTION_START;
      retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

      if (coreopts)
         option                       = (struct core_option*)&coreopts->opts[option_index];

      if (option && option->vals && option->vals->size == 2)
         entry->setting_type = ST_BOOL;
   }

   if (path_enabled)
   {
      if (!string_is_empty(path) && !use_representation)
         strlcpy(entry->path, path, sizeof(entry->path));
      else if (
                cbs
            &&  cbs->setting
            &&  cbs->setting->enum_value_idx != MSG_UNKNOWN
            && !(cbs->setting->flags
               & SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION))
         strlcpy(entry->path,
               msg_hash_to_str(cbs->setting->enum_value_idx),
               sizeof(entry->path));
      else
         if (!string_is_empty(newpath))
            strlcpy(entry->path, newpath, sizeof(entry->path));
   }
}

static menu_search_terms_t *menu_entries_search_get_terms_internal(void)
{
   struct menu_state *menu_st   = &menu_driver_state;
   file_list_t *list            = MENU_LIST_GET(menu_st->entries.list, 0);
   if (list && (list->size >= 1))
   {
      menu_file_list_cbs_t *cbs = NULL;
      if ((cbs = (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata))
         return &cbs->search;
   }
   return NULL;
}

/* Searches current menu list for specified 'needle'
 * string. If string is found, returns true and sets
 * 'idx' to the matching list entry index. */
bool menu_entries_list_search(const char *needle, size_t *idx)
{
   struct menu_state *menu_st  = &menu_driver_state;
   menu_list_t *menu_list      = menu_st->entries.list;
   file_list_t *list           = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);
   bool match_found            = false;
   bool char_search            = false;
   char needle_char            = 0;
   size_t i;

   if (   !list
       || string_is_empty(needle)
       || !idx)
      return false;

   /* Check if we are searching for a single
    * Latin alphabet character */
   char_search    = ((needle[1] == '\0') && (ISALPHA(needle[0])));
   if (char_search)
      needle_char = TOLOWER(needle[0]);

   for (i = 0; i < list->size; i++)
   {
      const char *entry_label = NULL;
      menu_entry_t entry;

      /* Note the we have to get the actual menu
       * entry here, since we need the exact label
       * that is currently displayed by the menu
       * driver */
      MENU_ENTRY_INITIALIZE(entry);
      entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                   | MENU_ENTRY_FLAG_LABEL_ENABLED
                   | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
      menu_entry_get(&entry, 0, i, NULL, true);

      /* When using the file browser, one or more
       * 'utility' entries will be added to the top
       * of the list (e.g. 'Parent Directory'). These
       * have no bearing on the actual content of the
       * list, and should be excluded from the search */
      if (   (entry.type == FILE_TYPE_SCAN_DIRECTORY)
          || (entry.type == FILE_TYPE_MANUAL_SCAN_DIRECTORY)
          || (entry.type == FILE_TYPE_USE_DIRECTORY)
          || (entry.type == FILE_TYPE_PARENT_DIRECTORY))
         continue;

      /* Get displayed entry label */
      if (!string_is_empty(entry.rich_label))
         entry_label = entry.rich_label;
      else
         entry_label = entry.path;

      if (string_is_empty(entry_label))
         continue;

      /* If we are performing a single character
       * search, jump to the first entry whose
       * first character matches */
      if (char_search)
      {
         if (needle_char == TOLOWER(entry_label[0]))
         {
            *idx        = i;
            match_found = true;
            break;
         }
      }
      /* Otherwise perform an exhaustive string
       * comparison */
      else
      {
         const char *found_str = (const char *)strcasestr(entry_label, needle);

         /* Found a match with the first characters
          * of the label -> best possible match,
          * so quit immediately */
         if (found_str == entry_label)
         {
            *idx        = i;
            match_found = true;
            break;
         }
         /* Found a mid-string match; this is a valid
          * result, but keep searching for the best
          * possible match */
         else if (found_str)
         {
            *idx        = i;
            match_found = true;
         }
      }
   }

   return match_found;
}

/* Display the date and time - time_mode will influence how
 * the time representation will look like.
 * */
size_t menu_display_timedate(gfx_display_ctx_datetime_t *datetime)
{
   struct menu_state *menu_st  = &menu_driver_state;

   /* Trigger an update, if required */
   if (menu_st->current_time_us - menu_st->datetime_last_time_us >=
         DATETIME_CHECK_INTERVAL)
   {
      time_t time_;
      struct tm tm_;
      bool has_am_pm         = false;
      const char *format_str = "";

      menu_st->datetime_last_time_us = menu_st->current_time_us;

      /* Get current time */
      time(&time_);
      rtime_localtime(&time_, &tm_);

      /* Format string representation */
      switch (datetime->time_mode)
      {
         case MENU_TIMEDATE_STYLE_YMD_HMS: /* YYYY-MM-DD HH:MM:SS */
            /* Using switch statements to set the format
             * string is verbose, but has far less performance
             * impact than setting the date separator dynamically
             * (i.e. no snprintf() or character replacement...) */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %H:%M:%S";
                  break;
               default:
                  format_str = "%Y-%m-%d %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM: /* YYYY-MM-DD HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %H:%M";
                  break;
               default:
                  format_str = "%Y-%m-%d %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD: /* YYYY-MM-DD */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d";
                  break;
               default:
                  format_str = "%Y-%m-%d";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YM: /* YYYY-MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m";
                  break;
               default:
                  format_str = "%Y-%m";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS: /* MM-DD-YYYY HH:MM:SS */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %H:%M:%S";
                  break;
               default:
                  format_str = "%m-%d-%Y %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM: /* MM-DD-YYYY HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %H:%M";
                  break;
               default:
                  format_str = "%m-%d-%Y %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD_HM: /* MM-DD HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d %H:%M";
                  break;
               default:
                  format_str = "%m-%d %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY: /* MM-DD-YYYY */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y";
                  break;
               default:
                  format_str = "%m-%d-%Y";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD: /* MM-DD */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d";
                  break;
               default:
                  format_str = "%m-%d";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS: /* DD-MM-YYYY HH:MM:SS */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %H:%M:%S";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %H:%M:%S";
                  break;
               default:
                  format_str = "%d-%m-%Y %H:%M:%S";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM: /* DD-MM-YYYY HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %H:%M";
                  break;
               default:
                  format_str = "%d-%m-%Y %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM: /* DD-MM HH:MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m %H:%M";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m %H:%M";
                  break;
               default:
                  format_str = "%d-%m %H:%M";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY: /* DD-MM-YYYY */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y";
                  break;
               default:
                  format_str = "%d-%m-%Y";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM: /* DD-MM */
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m";
                  break;
               default:
                  format_str = "%d-%m";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_HMS: /* HH:MM:SS */
            format_str = "%H:%M:%S";
            break;
         case MENU_TIMEDATE_STYLE_HM: /* HH:MM */
            format_str = "%H:%M";
            break;
         case MENU_TIMEDATE_STYLE_YMD_HMS_AMPM: /* YYYY-MM-DD HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %I:%M:%S %p";
                  break;
               default:
                  format_str = "%Y-%m-%d %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM_AMPM: /* YYYY-MM-DD HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%Y/%m/%d %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%Y.%m.%d %I:%M %p";
                  break;
               default:
                  format_str = "%Y-%m-%d %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS_AMPM: /* MM-DD-YYYY HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = "%m-%d-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM_AMPM: /* MM-DD-YYYY HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d/%Y %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d.%Y %I:%M %p";
                  break;
               default:
                  format_str = "%m-%d-%Y %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_MD_HM_AMPM: /* MM-DD HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%m/%d %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%m.%d %I:%M %p";
                  break;
               default:
                  format_str = "%m-%d %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS_AMPM: /* DD-MM-YYYY HH:MM:SS (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %I:%M:%S %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = "%d-%m-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM_AMPM: /* DD-MM-YYYY HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m/%Y %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m.%Y %I:%M %p";
                  break;
               default:
                  format_str = "%d-%m-%Y %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM_AMPM: /* DD-MM HH:MM (AM/PM) */
            has_am_pm = true;
            switch (datetime->date_separator)
            {
               case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
                  format_str = "%d/%m %I:%M %p";
                  break;
               case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
                  format_str = "%d.%m %I:%M %p";
                  break;
               default:
                  format_str = "%d-%m %I:%M %p";
                  break;
            }
            break;
         case MENU_TIMEDATE_STYLE_HMS_AMPM: /* HH:MM:SS (AM/PM) */
            has_am_pm  = true;
            format_str = "%I:%M:%S %p";
            break;
         case MENU_TIMEDATE_STYLE_HM_AMPM: /* HH:MM (AM/PM) */
            has_am_pm  = true;
            format_str = "%I:%M %p";
            break;
      }

      if (has_am_pm)
         strftime_am_pm(menu_st->datetime_cache, sizeof(menu_st->datetime_cache),
               format_str, &tm_);
      else
         strftime(menu_st->datetime_cache, sizeof(menu_st->datetime_cache),
               format_str, &tm_);
   }

   /* Copy cached datetime string to input
    * menu_display_ctx_datetime_t struct */
   return strlcpy(datetime->s, menu_st->datetime_cache, datetime->len);
}

/* Display current (battery) power state */
void menu_display_powerstate(gfx_display_ctx_powerstate_t *powerstate)
{
   int percent                    = 0;
   struct menu_state    *menu_st  = &menu_driver_state;
   enum frontend_powerstate state = FRONTEND_POWERSTATE_NONE;

   /* Trigger an update, if required */
   if (menu_st->current_time_us - menu_st->powerstate_last_time_us >=
         POWERSTATE_CHECK_INTERVAL)
   {
      menu_st->powerstate_last_time_us = menu_st->current_time_us;
      task_push_get_powerstate();
   }

   /* Get last recorded state */
   state                       = get_last_powerstate(&percent);

   /* Populate gfx_display_ctx_powerstate_t */
   powerstate->battery_enabled = (state != FRONTEND_POWERSTATE_NONE) &&
                                 (state != FRONTEND_POWERSTATE_NO_SOURCE);
   powerstate->percent         = 0;
   powerstate->charging        = false;

   if (powerstate->battery_enabled)
   {
      if (state == FRONTEND_POWERSTATE_CHARGING)
         powerstate->charging  = true;
      if (percent > 0)
         powerstate->percent   = (unsigned)percent;
      snprintf(powerstate->s, powerstate->len, "%u%%", powerstate->percent);
   }
}


/* Sets title to what the name of the current menu should be. */
int menu_entries_get_title(char *s, size_t len)
{
   unsigned menu_type            = 0;
   const char *path              = NULL;
   const char *label             = NULL;
   struct menu_state *menu_st    = &menu_driver_state;
   menu_handle_t *menu           = menu_st->driver_data;
   const file_list_t *list       = menu_st->entries.list ?
      MENU_LIST_GET(menu_st->entries.list, 0) : NULL;
   menu_file_list_cbs_t *cbs     = list
      ? (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata
      : NULL;

   if (!cbs)
      return -1;

   if (cbs->action_get_title)
   {
      int ret = 0;
      if (!string_is_empty(cbs->action_title_cache))
      {
         strlcpy(s, cbs->action_title_cache, len);
         return 0;
      }

      if (list->size)
      {
         path      = list->list[list->size - 1].path;
         label     = list->list[list->size - 1].label;
         menu_type = list->list[list->size - 1].type;
      }

      /* Show playlist entry instead of "Quick Menu" */
      if (string_is_equal(label, "deferred_rpl_entry_actions"))
      {
         playlist_t *playlist                  = playlist_get_cached();
         if (playlist)
         {
            const struct playlist_entry *entry = NULL;
            playlist_get_index(playlist, menu->rpl_entry_selection_ptr, &entry);
            if (entry)
               strlcpy(s,
                     !string_is_empty(entry->label) ? entry->label : entry->path,
                     len);
         }
      }
      else
         ret = cbs->action_get_title(path, label, menu_type, s, len);

      if (ret == 1)
         strlcpy(cbs->action_title_cache, s, sizeof(cbs->action_title_cache));
      return ret;
   }
   return 0;
}

/* Used to close an active message box (help or info)
 * TODO/FIXME: The way that message boxes are handled
 * is complete garbage. generic_menu_iterate() and
 * message boxes in general need a total rewrite.
 * I consider this current 'close_messagebox' a hack,
 * but at least it prevents undefined/dangerous
 * behaviour... */
static void menu_input_pointer_close_messagebox(struct menu_state *menu_st)
{
   const file_list_t *list          = MENU_LIST_GET(menu_st->entries.list, 0);
   const char *label                = list->list[list->size - 1].label;

#ifdef HAVE_AUDIOMIXER
   /* Determine whether this is a help or info
    * message box */
   if (list && list->size)
   {
      /* Play sound for closing the info box */
      settings_t *settings          = config_get_ptr();
      bool        audio_enable_menu = settings->bools.audio_enable_menu;
      bool audio_enable_menu_notice = settings->bools.audio_enable_menu_notice;
      if (audio_enable_menu && audio_enable_menu_notice)
         audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_NOTICE_BACK);
   }
#endif

   /* Pop stack, if required */
   if (menu_should_pop_stack(label))
   {
      size_t selection              = menu_st->selection_ptr;
      size_t new_selection          = selection;

      menu_entries_pop_stack(&new_selection, 0, 0);
      menu_st->selection_ptr        = selection;
   }
}

static float menu_input_get_dpi(
      menu_handle_t *menu,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height)
{
   static unsigned last_video_width  = 0;
   static unsigned last_video_height = 0;
   static float dpi                  = 0.0f;
   static bool dpi_cached            = false;

   /* Regardless of menu driver, need 'actual' screen DPI
    * Note: DPI is a fixed hardware property. To minimise performance
    * overheads we therefore only call video_context_driver_get_metrics()
    * on first run, or when the current video resolution changes */
   if (   (!dpi_cached)
       || (video_width  != last_video_width)
       || (video_height != last_video_height))
   {
      gfx_ctx_metrics_t mets;
      /* Note: If video_context_driver_get_metrics() fails,
       * we don't know what happened to dpi - so ensure it
       * is reset to a sane value */

      mets.type         = DISPLAY_METRIC_DPI;
      mets.value        = &dpi;
      if (!video_context_driver_get_metrics(&mets))
         dpi            = 0.0f;

      dpi_cached        = true;
      last_video_width  = video_width;
      last_video_height = video_height;
   }

   /* RGUI uses a framebuffer texture, which means we
    * operate in menu space, not screen space.
    * DPI in a traditional sense is therefore meaningless,
    * so generate a substitute value based upon framebuffer
    * dimensions */
   if (dpi > 0.0f)
   {
      bool menu_has_fb =
            menu->driver_ctx
         && menu->driver_ctx->set_texture;

      /* Read framebuffer info? */
      if (menu_has_fb)
      {
         unsigned fb_height         = p_disp->framebuf_height;
         /* Rationale for current 'DPI' determination method:
          * - Divide screen height by DPI, to get number of vertical
          *   '1 inch' squares
          * - Divide RGUI framebuffer height by number of vertical
          *   '1 inch' squares to get number of menu space pixels
          *   per inch
          * This is crude, but should be sufficient... */
         return ((float)fb_height / (float)video_height) * dpi;
      }
   }

   return dpi;
}

static bool input_event_osk_show_symbol_pages(
      menu_handle_t *menu)
{
#if defined(HAVE_LANGEXTRA)
#if defined(HAVE_RGUI)
   bool menu_has_fb      = (menu &&
         menu->driver_ctx &&
         menu->driver_ctx->set_texture);
   unsigned language     = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
   return    (!menu_has_fb)
          || ((language == RETRO_LANGUAGE_JAPANESE)
          ||  (language == RETRO_LANGUAGE_KOREAN)
          ||  (language == RETRO_LANGUAGE_CHINESE_SIMPLIFIED)
          ||  (language == RETRO_LANGUAGE_CHINESE_TRADITIONAL));
#else  /* HAVE_RGUI */
   return true;
#endif /* HAVE_RGUI */
#else  /* HAVE_LANGEXTRA */
   return false;
#endif /* HAVE_LANGEXTRA */
}

static void menu_driver_list_free(
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_ctx_list_t *list)
{
   if (menu_driver_ctx)
      if (menu_driver_ctx->list_free)
         menu_driver_ctx->list_free(
               list->list, list->idx, list->list_size);

   if (list->list)
   {
      file_list_free_userdata  (list->list, list->idx);
      file_list_free_actiondata(list->list, list->idx);
   }
}

static void menu_list_free_list(
      const menu_ctx_driver_t *menu_driver_ctx,
      file_list_t *list)
{
   unsigned i;

   for (i = 0; i < list->size; i++)
   {
      menu_ctx_list_t list_info;

      list_info.list      = list;
      list_info.idx       = i;
      list_info.list_size = list->size;

      menu_driver_list_free(menu_driver_ctx, &list_info);
   }

   file_list_free(list);
}

static void menu_list_pop_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      menu_list_t *list,
      size_t idx,
      size_t *directory_ptr)
{
   file_list_t *menu_list = MENU_LIST_GET(list, (unsigned)idx);

   if (menu_list)
   {
      if (menu_list->size != 0)
      {
         menu_ctx_list_t list_info;

         list_info.list      = menu_list;
         list_info.idx       = menu_list->size - 1;
         list_info.list_size = menu_list->size - 1;

         menu_driver_list_free(menu_driver_ctx, &list_info);
      }

      file_list_pop(menu_list, directory_ptr);
      if (  menu_driver_ctx &&
            menu_driver_ctx->list_set_selection)
         menu_driver_ctx->list_set_selection(menu_userdata,
               menu_list);
   }
}

static int menu_list_flush_stack_type(const char *needle, const char *label,
      unsigned type, unsigned final_type)
{
   return needle ? !string_is_equal(needle, label) : (type != final_type);
}

static void menu_list_flush_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      struct menu_state *menu_st,
      menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type)
{
   const char *label           = NULL;
   unsigned type               = 0;
   file_list_t *menu_list      = MENU_LIST_GET(list, (unsigned)idx);

   menu_st->flags             |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   menu_contentless_cores_flush_runtime();

   if (menu_list && menu_list->size)
   {
      label     = menu_list->list[menu_list->size - 1].label;
      type      = menu_list->list[menu_list->size - 1].type;
   }

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      size_t new_selection_ptr = menu_st->selection_ptr;
      bool wont_pop_stack      = (MENU_LIST_GET_STACK_SIZE(list, idx) <= 1);
      if (wont_pop_stack)
         break;

      if (menu_driver_ctx->list_cache)
         menu_driver_ctx->list_cache(menu_userdata,
               MENU_LIST_PLAIN, 0);

      menu_list_pop_stack(menu_driver_ctx,
            menu_userdata,
            list, idx, &new_selection_ptr);

      menu_st->flags          |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;

      menu_st->selection_ptr   = new_selection_ptr;
      menu_list                = MENU_LIST_GET(list, (unsigned)idx);

      if (menu_list && menu_list->size)
      {
         label     = menu_list->list[menu_list->size - 1].label;
         type      = menu_list->list[menu_list->size - 1].type;
      }
   }
}

static void menu_list_free(
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   if (menu_list->menu_stack)
   {
      unsigned i;

      for (i = 0; i < menu_list->menu_stack_size; i++)
      {
         if (!menu_list->menu_stack[i])
            continue;

         menu_list_free_list(menu_driver_ctx,
               menu_list->menu_stack[i]);
         menu_list->menu_stack[i]    = NULL;
      }

      free(menu_list->menu_stack);
   }

   if (menu_list->selection_buf)
   {
      unsigned i;

      for (i = 0; i < menu_list->selection_buf_size; i++)
      {
         if (!menu_list->selection_buf[i])
            continue;

         menu_list_free_list(menu_driver_ctx,
               menu_list->selection_buf[i]);
         menu_list->selection_buf[i] = NULL;
      }

      free(menu_list->selection_buf);
   }

   free(menu_list);
}

static menu_list_t *menu_list_new(const menu_ctx_driver_t *menu_driver_ctx)
{
   unsigned i;
   menu_list_t           *list = (menu_list_t*)malloc(sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack_size       = 1;
   list->selection_buf_size    = 1;
   list->selection_buf         = NULL;
   list->menu_stack            = (file_list_t**)
      calloc(list->menu_stack_size, sizeof(*list->menu_stack));

   if (!list->menu_stack)
      goto error;

   list->selection_buf         = (file_list_t**)
      calloc(list->selection_buf_size, sizeof(*list->selection_buf));

   if (!list->selection_buf)
      goto error;

   for (i = 0; i < list->menu_stack_size; i++)
   {
      list->menu_stack[i]           = (file_list_t*)
         malloc(sizeof(*list->menu_stack[i]));
      list->menu_stack[i]->list     = NULL;
      list->menu_stack[i]->capacity = 0;
      list->menu_stack[i]->size     = 0;
   }

   for (i = 0; i < list->selection_buf_size; i++)
   {
      list->selection_buf[i]           = (file_list_t*)
         malloc(sizeof(*list->selection_buf[i]));
      list->selection_buf[i]->list     = NULL;
      list->selection_buf[i]->capacity = 0;
      list->selection_buf[i]->size     = 0;
   }

   return list;

error:
   menu_list_free(menu_driver_ctx, list);
   return NULL;
}

static int menu_input_key_bind_set_mode_common(
      struct menu_state *menu_st,
      struct menu_bind_state *binds,
      enum menu_input_binds_ctl_state state,
      rarch_setting_t  *setting,
      settings_t *settings)
{
   switch (state)
   {
      case MENU_INPUT_BINDS_CTL_BIND_SINGLE:
         {
            unsigned bind_type;
            menu_displaylist_info_t info;
            struct retro_keybind *keybind = (struct retro_keybind*)setting->value.target.keybind;
            menu_list_t *menu_list   = menu_st->entries.list;
            file_list_t *menu_stack  = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
            size_t selection         = menu_st->selection_ptr;

            if (!keybind)
               return -1;

            menu_displaylist_info_init(&info);

            bind_type                = setting->bind_type;

            binds->order             = 0;
            binds->begin             = bind_type;
            binds->last              = bind_type;
            binds->output            = keybind;
            binds->buffer            = *(binds->output);
            binds->user              = setting->index_offset;

            info.list                = menu_stack;
            info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
            info.directory_ptr       = selection;
            info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND;
            info.label               = strdup(
                  msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND));
            if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info, settings))
               menu_displaylist_process(&info);
            menu_displaylist_info_free(&info);
         }
         break;
      case MENU_INPUT_BINDS_CTL_BIND_ALL:
         {
            menu_displaylist_info_t info;
            menu_list_t *menu_list   = menu_st->entries.list;
            file_list_t *menu_stack  = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
            size_t selection         = menu_st->selection_ptr;

            menu_displaylist_info_init(&info);

            binds->order             = 0;
            binds->begin             = MENU_SETTINGS_BIND_BEGIN
                  + input_config_bind_order[0];
            binds->last              = MENU_SETTINGS_BIND_LAST;
            binds->output            = &input_config_binds[setting->index_offset][0]
                  + input_config_bind_order[0];
            binds->buffer            = *(binds->output);

            info.list                = menu_stack;
            info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
            info.directory_ptr       = selection;
            info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND_ALL;
            info.label               = strdup(
                  msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND_ALL));
            if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info, settings))
               menu_displaylist_process(&info);
            menu_displaylist_info_free(&info);
         }
         break;
      default:
      case MENU_INPUT_BINDS_CTL_BIND_NONE:
         break;
   }

   return 0;
}

static bool menu_input_key_bind_poll_find_hold_pad(
      struct menu_bind_state *new_state,
      struct retro_keybind *output,
      unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n =
      (const struct menu_bind_state_port*)
      &new_state->state[p];

   for (b = RETROK_BACKSPACE; b < RETROK_LAST; b++)
   {
      bool found = n->keys[b];

      if (!found)
         continue;

      output->key = (enum retro_key)b;
      return true;
   }

   for (b = 0; b < MENU_MAX_MBUTTONS; b++)
   {
      bool found = n->mouse_buttons[b];

      if (!found)
         continue;

      switch (b)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            output->mbutton = b;
            return true;
      }
   }

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool found = n->buttons[b];

      if (!found)
         continue;

      output->joykey = b;
      output->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      if (     abs(n->axes[a]) >= 20000
            && n->axes[a] != new_state->axis_state[p].rested_axes[a])
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         output->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         output->joykey = NO_BTN;
         return true;
      }
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h];
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         output->joykey  = HAT_MAP(h, sane_trigger);
         output->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

static bool menu_input_key_bind_poll_find_hold(
      unsigned max_users,
      struct menu_bind_state *new_state,
      struct retro_keybind *output)
{
   if (new_state)
   {
      unsigned i;

      for (i = 0; i < max_users; i++)
      {
         if (menu_input_key_bind_poll_find_hold_pad(new_state, output, i))
            return true;
      }
   }

   return false;
}

static bool menu_input_key_bind_poll_find_trigger_pad(
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind *output,
      unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n = (const struct menu_bind_state_port*)
      &new_state->state[p];
   const struct menu_bind_state_port *o = (const struct menu_bind_state_port*)
      &state->state[p];

   for (b = RETROK_BACKSPACE; b < RETROK_LAST; b++)
   {
      bool found = n->keys[b] && !o->keys[b];

      if (!found)
         continue;

      output->key = (enum retro_key)b;
      return true;
   }

   for (b = 0; b < MENU_MAX_MBUTTONS; b++)
   {
      bool found = n->mouse_buttons[b] && !o->mouse_buttons[b];

      if (!found)
         continue;

      switch (b)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            output->mbutton = b;
            return true;
      }
   }

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool found = n->buttons[b] && !o->buttons[b];

      if (!found)
         continue;

      output->joykey = b;
      output->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] -
            new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] -
            new_state->axis_state[p].rested_axes[a]);

      if (     (abs(n->axes[a]) >= 20000)
            && (locked_distance >= 20000)
            && (rested_distance >= 20000))
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         output->joyaxis = (n->axes[a] > 0) ? AXIS_POS(a) : AXIS_NEG(a);
         output->joykey  = NO_BTN;

         /* Lock the current axis */
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0
               ? 0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) /* Unlock the axis. */
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         output->joykey = HAT_MAP(h, sane_trigger);
         output->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

static bool menu_input_key_bind_poll_find_trigger(
      unsigned max_users,
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind *output)
{
   if (state && new_state)
   {
      unsigned i;

      for (i = 0; i < max_users; i++)
      {
         if (menu_input_key_bind_poll_find_trigger_pad(
                  state, new_state, output, i))
            return true;
      }
   }

   return false;
}


static void menu_input_key_bind_poll_bind_get_rested_axes(
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      struct menu_bind_state *state)
{
   unsigned a;
   unsigned port          = state->port;

   if (joypad)
   {
      /* poll only the relevant port */
      for (a = 0; a < MENU_MAX_AXES; a++)
      {
         if (AXIS_POS(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a]  =
               joypad->axis(port, AXIS_POS(a));
         if (AXIS_NEG(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a] +=
               joypad->axis(port, AXIS_NEG(a));
      }
   }

   if (sec_joypad)
   {
      /* poll only the relevant port */
      for (a = 0; a < MENU_MAX_AXES; a++)
      {
         if (AXIS_POS(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a]  = sec_joypad->axis(port, AXIS_POS(a));

         if (AXIS_NEG(a) != AXIS_NONE)
            state->axis_state[port].rested_axes[a] += sec_joypad->axis(port, AXIS_NEG(a));
      }
   }
}

static void input_event_osk_iterate(void *osk_grid, enum osk_type osk_idx)
{
#ifndef HAVE_LANGEXTRA
   /* If HAVE_LANGEXTRA is not defined, define some ASCII-friendly pages. */
   static const char *uppercase_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "Q","W","E","R","T","Y","U","I","O","P","Enter",
      "A","S","D","F","G","H","J","K","L","+","Lower",
      "Z","X","C","V","B","N","M"," ","_","/","Next"};
   static const char *lowercase_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "q","w","e","r","t","y","u","i","o","p","Enter",
      "a","s","d","f","g","h","j","k","l","@","Upper",
      "z","x","c","v","b","n","m"," ","-",".","Next"};
   static const char *symbols_page1_grid[] = {
      "1","2","3","4","5","6","7","8","9","0","Bksp",
      "!","\"","#","$","%","&","'","*","(",")","Enter",
      "+",",","-","~","/",":",";","=","<",">","Lower",
      "?","@","[","\\","]","^","_","|","{","}","Next"};
#endif
   switch (osk_idx)
   {
#ifdef HAVE_LANGEXTRA
      case OSK_HIRAGANA_PAGE1:
         memcpy(osk_grid,
               hiragana_page1_grid,
               sizeof(hiragana_page1_grid));
         break;
      case OSK_HIRAGANA_PAGE2:
         memcpy(osk_grid,
               hiragana_page2_grid,
               sizeof(hiragana_page2_grid));
         break;
      case OSK_KATAKANA_PAGE1:
         memcpy(osk_grid,
               katakana_page1_grid,
               sizeof(katakana_page1_grid));
         break;
      case OSK_KATAKANA_PAGE2:
         memcpy(osk_grid,
               katakana_page2_grid,
               sizeof(katakana_page2_grid));
         break;
      case OSK_KOREAN_PAGE1:
         memcpy(osk_grid,
               korean_page1_grid,
               sizeof(korean_page1_grid));
         break;
#endif
      case OSK_SYMBOLS_PAGE1:
         memcpy(osk_grid,
               symbols_page1_grid,
               sizeof(uppercase_grid));
         break;
      case OSK_UPPERCASE_LATIN:
         memcpy(osk_grid,
               uppercase_grid,
               sizeof(uppercase_grid));
         break;
      case OSK_LOWERCASE_LATIN:
      default:
         memcpy(osk_grid,
               lowercase_grid,
               sizeof(lowercase_grid));
         break;
   }
}

static void menu_input_get_mouse_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool menu_mouse_enable,
      bool input_overlay_enable,
      bool overlay_active,
      menu_input_pointer_hw_state_t *hw_state)
{
   rarch_joypad_info_t joypad_info;
   static int16_t last_x           = 0;
   static int16_t last_y           = 0;
   bool is_select_pressed          = false;
   bool is_cancel_pressed          = false;
   static bool last_select_pressed = false;
   static bool last_cancel_pressed = false;
   bool menu_has_fb                =
      (menu &&
       menu->driver_ctx &&
       menu->driver_ctx->set_texture);
   bool state_inited               = current_input &&
      current_input->input_state;
#ifdef HAVE_OVERLAY
   /* Menu pointer controls are ignored when overlays are enabled. */
   if (overlay_active)
      menu_mouse_enable            = false;
#endif

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   hw_state->x                     = 0;
   hw_state->y                     = 0;
   hw_state->flags                 = 0;

   if (!menu_mouse_enable)
      return;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   /* X/Y position */
   if (state_inited)
   {
      if ((hw_state->x = current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RARCH_DEVICE_MOUSE_SCREEN,
                  0,
                  RETRO_DEVICE_ID_MOUSE_X)) != last_x)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      if ((hw_state->y = current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RARCH_DEVICE_MOUSE_SCREEN,
                  0,
                  RETRO_DEVICE_ID_MOUSE_Y)) != last_y)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
   }

   last_x                          = hw_state->x;
   last_y                          = hw_state->y;

   /* > X/Y position adjustment */
   if (menu_has_fb)
   {
      /* RGUI uses a framebuffer texture + custom viewports,
       * which means we have to convert from screen space to
       * menu space... */
      struct video_viewport vp     = {0};
      /* Read display/framebuffer info */
      unsigned fb_width            = p_disp->framebuf_width;
      unsigned fb_height           = p_disp->framebuf_height;

      video_driver_get_viewport_info(&vp);

      /* Adjust X position */
      hw_state->x                  = (int16_t)(((float)(hw_state->x - vp.x) / (float)vp.width) * (float)fb_width);
      if (hw_state->x < 0)
         hw_state->x               = 0;
      else if (hw_state->x >= (int)fb_width)
         hw_state->x               = (fb_width -1);

      /* Adjust Y position */
      hw_state->y                  = (int16_t)(((float)(hw_state->y - vp.y) / (float)vp.height) * (float)fb_height);
      if (hw_state->y <  0)
         hw_state->y               = 0;
      else if (hw_state->y >= (int)fb_height)
         hw_state->y               = (fb_height-1);
   }

   if (state_inited)
   {
      /* Select (LMB)
       * Note that releasing select also counts as activity */
      if (current_input->input_state(
               input_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               RETRO_DEVICE_MOUSE,
               0,
               RETRO_DEVICE_ID_MOUSE_LEFT))
         hw_state->flags |=  MENU_INP_PTR_FLG_PRESS_SELECT;

      /* Cancel (RMB)
       * Note that releasing cancel also counts as activity */
      if (current_input->input_state(
               input_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               RETRO_DEVICE_MOUSE,
               0,
               RETRO_DEVICE_ID_MOUSE_RIGHT))
         hw_state->flags |=  MENU_INP_PTR_FLG_PRESS_CANCEL;

      /* Up (mouse wheel up) */
      if (current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_WHEELUP))
         hw_state->flags |=  (MENU_INP_PTR_FLG_PRESS_UP
                            | MENU_INP_PTR_FLG_ACTIVE
                             );

      /* Down (mouse wheel down) */
      if (current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_WHEELDOWN))
         hw_state->flags |=  (MENU_INP_PTR_FLG_PRESS_DOWN
                            | MENU_INP_PTR_FLG_ACTIVE
                             );

      /* Left (mouse wheel horizontal left) */
      if (current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN))
         hw_state->flags |=  (MENU_INP_PTR_FLG_PRESS_LEFT
                            | MENU_INP_PTR_FLG_ACTIVE
                             );

      /* Right (mouse wheel horizontal right) */
      if (current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  NULL,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_MOUSE,
                  0,
                  RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP))
         hw_state->flags |=  (MENU_INP_PTR_FLG_PRESS_RIGHT
                            | MENU_INP_PTR_FLG_ACTIVE
                             );
   }

   is_select_pressed    = ((hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT) > 0);
   is_cancel_pressed    = ((hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL) > 0);
   if (is_select_pressed || (is_select_pressed != last_select_pressed))
      hw_state->flags  |= MENU_INP_PTR_FLG_ACTIVE;
   if (is_cancel_pressed || (is_cancel_pressed != last_cancel_pressed))
      hw_state->flags  |= MENU_INP_PTR_FLG_ACTIVE;
   last_select_pressed  = (hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT) > 0;
   last_cancel_pressed  = (hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL) > 0;
}

static void menu_input_get_touchscreen_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool overlay_active,
      bool pointer_enabled,
      unsigned input_touch_scale,
      menu_input_pointer_hw_state_t *hw_state)
{
   rarch_joypad_info_t joypad_info;
   unsigned fb_width, fb_height;
   int pointer_x                                = 0;
   int pointer_y                                = 0;
   const retro_keybind_set *binds[MAX_USERS] = {NULL};
   /* Is a background texture set for the current menu driver?
    * Checks if the menu framebuffer is set.
    * This would usually only return true
    * for framebuffer-based menu drivers, like RGUI. */
   int pointer_device                           =
         (menu && menu->driver_ctx && menu->driver_ctx->set_texture) ?
               RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
   static int16_t last_x                        = 0;
   static int16_t last_y                        = 0;
   static bool last_select_pressed              = false;
   static bool last_cancel_pressed              = false;

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   /* Touch screens don't have mouse wheels,
    * so up/down/left/right are always disabled */
   hw_state->flags  &= ~(MENU_INP_PTR_FLG_ACTIVE
                       | MENU_INP_PTR_FLG_PRESS_UP
                       | MENU_INP_PTR_FLG_PRESS_DOWN
                       | MENU_INP_PTR_FLG_PRESS_LEFT
                       | MENU_INP_PTR_FLG_PRESS_RIGHT
                        );

#ifdef HAVE_OVERLAY
   /* Menu pointer controls are ignored when overlays are enabled. */
   if (overlay_active)
      pointer_enabled   = false;
#endif

   /* If touchscreen is disabled, ignore all input */
   if (!pointer_enabled)
   {
      hw_state->x       = 0;
      hw_state->y       = 0;
      hw_state->flags  &= ~(MENU_INP_PTR_FLG_PRESS_SELECT
                          | MENU_INP_PTR_FLG_PRESS_CANCEL);
      return;
   }

   /* TODO/FIXME - this should only be used for framebuffer-based
    * menu drivers like RGUI. Touchscreen input as a whole should
    * NOT be dependent on this */
   fb_width             = p_disp->framebuf_width;
   fb_height            = p_disp->framebuf_height;

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;
   joypad_info.axis_threshold                   = 0.0f;

   /* X pos */
   if (current_input->input_state)
      pointer_x                  = current_input->input_state(
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, (*binds),
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_X);
   hw_state->x  = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   hw_state->x *= input_touch_scale;

   /* > An annoyance - we get different starting positions
    *   depending upon whether pointer_device is
    *   RETRO_DEVICE_POINTER or RARCH_DEVICE_POINTER_SCREEN,
    *   so different 'activity' checks are required to prevent
    *   false positives on first run */
   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->x != last_x)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_x = hw_state->x;
   }
   else
   {
      if (pointer_x != last_x)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_x = pointer_x;
   }

   /* Y pos */
   if (current_input->input_state)
      pointer_y = current_input->input_state(
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, (*binds),
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_Y);
   hw_state->y  = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;
   hw_state->y *= input_touch_scale;

   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->y != last_y)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_y = hw_state->y;
   }
   else
   {
      if (pointer_y != last_y)
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_y = pointer_y;
   }

   /* Select (touch screen contact)
    * Note that releasing select also counts as activity */
   if (current_input->input_state)
   {
      if (current_input->input_state(
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, (*binds),
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RETRO_DEVICE_ID_POINTER_PRESSED))
         hw_state->flags |=  MENU_INP_PTR_FLG_PRESS_SELECT;
      else
         hw_state->flags &= ~MENU_INP_PTR_FLG_PRESS_SELECT;
   }

   {
      bool select_pressed = ((hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT) > 0);
      if (select_pressed || (select_pressed != last_select_pressed))
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_select_pressed = select_pressed;
   }

   /* Cancel (touch screen 'back' - don't know what is this, but whatever...)
    * Note that releasing cancel also counts as activity */
   if (current_input->input_state)
   {
      if (current_input->input_state(
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info, (*binds),
            keyboard_mapping_blocked,
            0, pointer_device,
            0, RARCH_DEVICE_ID_POINTER_BACK))
         hw_state->flags |=  MENU_INP_PTR_FLG_PRESS_CANCEL;
      else
         hw_state->flags &= ~MENU_INP_PTR_FLG_PRESS_CANCEL;
   }

   {
      bool cancel_pressed = ((hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL) > 0);
      if (cancel_pressed || (cancel_pressed != last_cancel_pressed))
         hw_state->flags |= MENU_INP_PTR_FLG_ACTIVE;
      last_cancel_pressed = cancel_pressed;
   }
}

static void menu_entries_settings_deinit(struct menu_state *menu_st)
{
   menu_setting_free(menu_st->entries.list_settings);
   if (menu_st->entries.list_settings)
      free(menu_st->entries.list_settings);
   menu_st->entries.list_settings = NULL;
}

static bool menu_driver_displaylist_push_internal(
      const char *label,
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_FAVORITES, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SETTINGS_ALL, info, settings))
         return true;
   }
#ifdef HAVE_CHEATS
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST, info, settings))
         return true;
   }
#endif
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strldup("lpl", sizeof("lpl"));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_clear(info->list);
      menu_displaylist_ctl(DISPLAYLIST_MUSIC_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strldup("lpl", sizeof("lpl"));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_clear(info->list);
      menu_displaylist_ctl(DISPLAYLIST_VIDEO_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strldup("lpl", sizeof("lpl"));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      menu_entries_clear(info->list);
      menu_displaylist_ctl(DISPLAYLIST_IMAGES_HISTORY, info, settings);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      const char *dir_playlist    = settings->paths.directory_playlist;

      filebrowser_clear_type();
      info->type                  = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strldup("lpl", sizeof("lpl"));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      if (string_is_empty(dir_playlist))
      {
         menu_entries_clear(info->list);
         info->flags |= MD_FLAG_NEED_REFRESH
                      | MD_FLAG_NEED_PUSH_NO_PLAYLIST_ENTRIES
                      | MD_FLAG_NEED_PUSH;
         return true;
      }

      if (!string_is_empty(info->path))
         free(info->path);

      info->path = strdup(dir_playlist);

      if (menu_displaylist_ctl(
               DISPLAYLIST_DATABASE_PLAYLISTS, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SCAN_DIRECTORY_LIST, info, settings))
         return true;
   }
#if defined(HAVE_LIBRETRODB)
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_EXPLORE, info, settings))
         return true;
   }
#endif
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_CONTENTLESS_CORES, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_NETPLAY_ROOM_LIST, info, settings))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL, info, settings))
         return true;
   }

   return false;
}

static bool menu_driver_displaylist_push(
      struct menu_state *menu_st,
      settings_t *settings,
      file_list_t *entry_list,
      file_list_t *entry_stack)
{
   menu_displaylist_info_t info;
   const char *path               = NULL;
   const char *label              = NULL;
   unsigned type                  = 0;
   bool ret                       = false;
   enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
   file_list_t *list              = MENU_LIST_GET(menu_st->entries.list, 0);
   menu_file_list_cbs_t *cbs      = (menu_file_list_cbs_t*)
      list->list[list->size - 1].actiondata;

   menu_displaylist_info_init(&info);

   if (list && list->size)
   {
      path      = list->list[list->size - 1].path;
      label     = list->list[list->size - 1].label;
      type      = list->list[list->size - 1].type;
   }

   if (cbs)
      enum_idx    = cbs->enum_idx;

   info.list      = entry_list;
   info.type      = type;
   info.enum_idx  = enum_idx;

   if (!string_is_empty(path))
      info.path  = strdup(path);

   if (!string_is_empty(label))
      info.label = strdup(label);

   if (!info.list)
      goto error;

   if (menu_driver_displaylist_push_internal(label, &info, settings))
   {
      ret = menu_displaylist_process(&info);
      goto end;
   }

   cbs = (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata;

   if (cbs && cbs->action_deferred_push)
      if (cbs->action_deferred_push(&info) != 0)
         goto error;

   ret = true;

end:
   menu_displaylist_info_free(&info);

   return ret;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static void menu_input_key_bind_poll_bind_state_internal(
      const input_device_driver_t *joypad,
      struct menu_bind_state *state,
      unsigned port,
      bool timed_out)
{
   unsigned i;

   /* poll only the relevant port */
   for (i = 0; i < MENU_MAX_BUTTONS; i++)
      state->state[port].buttons[i] = joypad->button(port, i);

   for (i = 0; i < MENU_MAX_AXES; i++)
   {
      if (AXIS_POS(i) != AXIS_NONE)
         state->state[port].axes[i]  = joypad->axis(port, AXIS_POS(i));

      if (AXIS_NEG(i) != AXIS_NONE)
         state->state[port].axes[i] += joypad->axis(port, AXIS_NEG(i));
   }

   for (i = 0; i < MENU_MAX_HATS; i++)
   {
      if (joypad->button(port, HAT_MAP(i, HAT_UP_MASK)))
         state->state[port].hats[i] |= HAT_UP_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_DOWN_MASK)))
         state->state[port].hats[i] |= HAT_DOWN_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_LEFT_MASK)))
         state->state[port].hats[i] |= HAT_LEFT_MASK;
      if (joypad->button(port, HAT_MAP(i, HAT_RIGHT_MASK)))
         state->state[port].hats[i] |= HAT_RIGHT_MASK;
   }
}

/* This sets up all the callback functions for a menu entry.
 *
 * OK     : When we press the 'OK' button on an entry.
 * Cancel : When we press the 'Cancel' button on an entry.
 * Scan   : When we press the 'Scan' button on an entry.
 * Start  : When we press the 'Start' button on an entry.
 * Select : When we press the 'Select' button on an entry.
 * Info   : When we press the 'Info' button on an entry.
 * Left   : when we press 'Left' on the D-pad while this entry is selected.
 * Right  : when we press 'Right' on the D-pad while this entry is selected.
 * Deferred push : When pressing an entry results in spawning a new list, it waits until the next
 * frame to push this onto the stack. This function callback will then be invoked.
 * Get value: Each entry has associated 'text', which we call the value. This function callback
 * lets us render that text.
 * Title: Each entry can have a custom 'title'.
 * Label: Each entry has a label name. This function callback lets us render that label text.
 * Sublabel: each entry has a sublabel, which consists of one or more lines of additional information.
 * This function callback lets us render that text.
 */
static void menu_cbs_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx,
      file_list_t *list,
      menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label,
      size_t lbl_len,
      unsigned type, size_t idx)
{
   size_t menu_lbl_len;
   const char *menu_lbl           = NULL;
   file_list_t *menu_list         = MENU_LIST_GET(menu_st->entries.list, 0);
#ifdef DEBUG_LOG
   menu_file_list_cbs_t *menu_cbs = (menu_file_list_cbs_t*)
      menu_list->list[list->size - 1].actiondata;
   enum msg_hash_enums enum_idx   = menu_cbs ? menu_cbs->enum_idx : MSG_UNKNOWN;
#endif

   if (menu_list && menu_list->size)
      menu_lbl = menu_list->list[menu_list->size - 1].label;

   if (!label || !menu_lbl)
      return;

   menu_lbl_len = strlen(menu_lbl);

#ifdef DEBUG_LOG
   RARCH_LOG("\n");

   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
      RARCH_LOG("\t\t\tenum_idx %d [%s]\n", cbs->enum_idx, msg_hash_to_str(cbs->enum_idx));
#endif

   /* It will try to find a corresponding callback function inside
    * menu_cbs_ok.c, then map this callback to the entry. */
   menu_cbs_init_bind_ok(cbs, path, label, lbl_len, type, idx, menu_lbl, menu_lbl_len);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_cancel.c, then map this callback to the entry. */
   menu_cbs_init_bind_cancel(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_scan.c, then map this callback to the entry. */
   menu_cbs_init_bind_scan(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_start.c, then map this callback to the entry. */
   menu_cbs_init_bind_start(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_select.c, then map this callback to the entry. */
   menu_cbs_init_bind_select(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_info.c, then map this callback to the entry. */
   menu_cbs_init_bind_info(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_left.c, then map this callback to the entry. */
   menu_cbs_init_bind_left(cbs, path, label, lbl_len, type, idx, menu_lbl, menu_lbl_len);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_right.c, then map this callback to the entry. */
   menu_cbs_init_bind_right(cbs, path, label, lbl_len, type, idx, menu_lbl, menu_lbl_len);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_deferred_push.c, then map this callback to the entry. */
   menu_cbs_init_bind_deferred_push(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_get_string_representation.c, then map this callback to the entry. */
   menu_cbs_init_bind_get_string_representation(cbs, path, label, lbl_len, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_title.c, then map this callback to the entry. */
   menu_cbs_init_bind_title(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_label.c, then map this callback to the entry. */
   menu_cbs_init_bind_label(cbs, path, label, type, idx);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_sublabel.c, then map this callback to the entry. */
   menu_cbs_init_bind_sublabel(cbs, path, label, lbl_len, type, idx);

   if (menu_driver_ctx && menu_driver_ctx->bind_init)
      menu_driver_ctx->bind_init(
            cbs,
            path,
            label,
            type,
            idx);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static void menu_driver_set_last_shader_path_int(
      const char *shader_path,
      enum rarch_shader_type *type,
      char *shader_dir, size_t dir_len,
      char *shader_file, size_t file_len)
{
   const char *file_name = NULL;

   if (   !type
       || !shader_dir
       || (dir_len < 1)
       || !shader_file
       || (file_len < 1))
      return;

   /* Reset existing cache */
   *type          = RARCH_SHADER_NONE;
   shader_dir[0]  = '\0';
   shader_file[0] = '\0';

   /* If path is empty, do nothing */
   if (string_is_empty(shader_path))
      return;

   /* Get shader type */
   /* If type is invalid, do nothing */
   if ((*type = video_shader_parse_type(shader_path)) == RARCH_SHADER_NONE)
      return;

   /* Cache parent directory */
   fill_pathname_parent_dir(shader_dir, shader_path, dir_len);

   /* If parent directory is empty, then file name
    * is only valid if 'shader_path' refers to an
    * existing file in the root of the file system */
   if (    string_is_empty(shader_dir)
       && !path_is_valid(shader_path))
      return;

   /* Cache file name */
   file_name = path_basename_nocompression(shader_path);
   if (!string_is_empty(file_name))
      strlcpy(shader_file, file_name, file_len);
}

void menu_driver_set_last_shader_preset_path(const char *path)
{
   menu_handle_t *menu         = menu_driver_state.driver_data;
   if (menu)
      menu_driver_set_last_shader_path_int(
            path,
            &menu->last_shader_selection.preset_type,
            menu->last_shader_selection.preset_dir,
            sizeof(menu->last_shader_selection.preset_dir),
            menu->last_shader_selection.preset_file_name,
            sizeof(menu->last_shader_selection.preset_file_name));
}

void menu_driver_set_last_shader_pass_path(const char *path)
{
   menu_handle_t *menu         = menu_driver_state.driver_data;
   if (menu)
      menu_driver_set_last_shader_path_int(
            path,
            &menu->last_shader_selection.pass_type,
            menu->last_shader_selection.pass_dir,
            sizeof(menu->last_shader_selection.pass_dir),
            menu->last_shader_selection.pass_file_name,
            sizeof(menu->last_shader_selection.pass_file_name));
}

enum rarch_shader_type menu_driver_get_last_shader_preset_type(void)
{
   menu_handle_t *menu         = menu_driver_state.driver_data;
   if (!menu)
      return RARCH_SHADER_NONE;
   return menu->last_shader_selection.preset_type;
}

static void menu_driver_get_last_shader_path_int(
      settings_t *settings, enum rarch_shader_type type,
      const char *shader_dir, const char *shader_file_name,
      const char **dir_out, const char **file_name_out)
{
   bool remember_last_dir       = settings->bools.video_shader_remember_last_dir;
   const char *video_shader_dir = settings->paths.directory_video_shader;

   /* File name is NULL by default */
   if (file_name_out)
      *file_name_out = NULL;

   /* If any of the following are true:
    * - Directory caching is disabled
    * - No directory has been cached
    * - Cached directory is invalid
    * - Last selected shader is incompatible with
    *   the current video driver
    * ...use default settings */
   if (   (!remember_last_dir)
       || (type == RARCH_SHADER_NONE)
       || string_is_empty(shader_dir)
       || !path_is_directory(shader_dir)
       || !video_shader_is_supported(type))
   {
      if (dir_out)
         *dir_out = video_shader_dir;
      return;
   }

   /* Assign last set directory */
   if (dir_out)
      *dir_out = shader_dir;

   /* Assign file name */
   if (    file_name_out
       && !string_is_empty(shader_file_name))
      *file_name_out = shader_file_name;
}


void menu_driver_get_last_shader_preset_path(
      const char **directory, const char **file_name)
{
   settings_t *settings         = config_get_ptr();
   menu_handle_t *menu          = menu_driver_state.driver_data;
   enum rarch_shader_type type  = RARCH_SHADER_NONE;
   const char *shader_dir       = NULL;
   const char *shader_file_name = NULL;

   if (menu)
   {
      type                      = menu->last_shader_selection.preset_type;
      shader_dir                = menu->last_shader_selection.preset_dir;
      shader_file_name          = menu->last_shader_selection.preset_file_name;
   }

   menu_driver_get_last_shader_path_int(settings, type,
         shader_dir, shader_file_name,
         directory, file_name);
}

void menu_driver_get_last_shader_pass_path(
      const char **directory, const char **file_name)
{
   menu_handle_t *menu          = menu_driver_state.driver_data;
   settings_t *settings         = config_get_ptr();
   enum rarch_shader_type type  = RARCH_SHADER_NONE;
   const char *shader_dir       = NULL;
   const char *shader_file_name = NULL;

   if (menu)
   {
      type                      = menu->last_shader_selection.pass_type;
      shader_dir                = menu->last_shader_selection.pass_dir;
      shader_file_name          = menu->last_shader_selection.pass_file_name;
   }

   menu_driver_get_last_shader_path_int(settings, type,
         shader_dir, shader_file_name,
         directory, file_name);
}

int menu_shader_manager_clear_num_passes(struct video_shader *shader)
{
   if (shader)
   {
      struct menu_state *menu_st  = &menu_driver_state;
      shader->passes              = 0;
      menu_st->flags             |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      video_shader_resolve_parameters(shader);
      shader->flags              |= SHDR_FLAG_MODIFIED;
   }

   return 0;
}

int menu_shader_manager_clear_parameter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_parameter *param = shader ?
      &shader->parameters[i] : NULL;

   if (param)
   {
      param->current = param->initial;
      param->current = MIN(MAX(param->minimum,
               param->current), param->maximum);

      shader->flags |= SHDR_FLAG_MODIFIED;
   }

   return 0;
}

int menu_shader_manager_clear_pass_filter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;
   shader->flags      |= SHDR_FLAG_MODIFIED;

   return 0;
}

void menu_shader_manager_clear_pass_scale(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return;

   shader_pass->fbo.scale_x = 0;
   shader_pass->fbo.scale_y = 0;
   shader_pass->fbo.flags  &= ~FBO_SCALE_FLAG_VALID;

   shader->flags           |=  SHDR_FLAG_MODIFIED;
}

void menu_shader_manager_clear_pass_path(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass
      *shader_pass              = shader
      ? &shader->pass[i]
      : NULL;

   if (shader_pass)
      *shader_pass->source.path = '\0';

   if (shader)
      shader->flags            |= SHDR_FLAG_MODIFIED;
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
static enum rarch_shader_type menu_shader_manager_get_type(
      const struct video_shader *shader)
{
   enum rarch_shader_type type = RARCH_SHADER_NONE;
   /* All shader types must be the same, or we cannot use it. */

   if (shader)
   {
      type = video_shader_parse_type(shader->path);

      if (shader->passes)
      {
         size_t i                 = 0;
         if (type == RARCH_SHADER_NONE)
         {
            type = video_shader_parse_type(shader->pass[0].source.path);
            i    = 1;
         }

         for (; i < shader->passes; i++)
         {
            enum rarch_shader_type pass_type =
               video_shader_parse_type(shader->pass[i].source.path);

            switch (pass_type)
            {
               case RARCH_SHADER_CG:
               case RARCH_SHADER_GLSL:
               case RARCH_SHADER_SLANG:
                  if (type != pass_type)
                     return RARCH_SHADER_NONE;
                  break;
               default:
                  break;
            }
         }
      }
   }

   return type;
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(
      struct video_shader *shader,
      const char *dir_video_shader,
      const char *dir_menu_config)
{
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!shader)
      return;

   type = menu_shader_manager_get_type(shader);

   if (shader->passes
         && type != RARCH_SHADER_NONE
         && !(shader->flags & SHDR_FLAG_DISABLED))
   {
      menu_shader_manager_save_preset(shader, NULL,
            dir_video_shader, dir_menu_config, true);
      return;
   }

   menu_shader_manager_set_preset(NULL, type, NULL, true);
}

static bool menu_shader_manager_save_preset_internal(
      bool save_reference,
      const struct video_shader *shader,
      const char *basename,
      const char *dir_video_shader,
      bool apply,
      const char **target_dirs,
      size_t num_target_dirs)
{
   size_t _len;
   char fullname[NAME_MAX_LENGTH];
   const char *preset_ext         = NULL;
   bool ret                       = false;
   enum rarch_shader_type type    = RARCH_SHADER_NONE;
   char *preset_path              = NULL;
   size_t i                       = 0;
   if (!shader || !shader->passes)
      return false;
   if ((type = menu_shader_manager_get_type(shader)) == RARCH_SHADER_NONE)
      return false;

   preset_ext = video_shader_get_preset_extension(type);

   if (!string_is_empty(basename))
      _len = strlcpy(fullname, basename, sizeof(fullname));
   else
      _len = strlcpy(fullname, "retroarch", sizeof(fullname));
   strlcpy(fullname + _len, preset_ext, sizeof(fullname) - _len);

   if (path_is_absolute(fullname))
   {
      preset_path = fullname;
      if ((ret    = video_shader_write_preset(preset_path, shader, save_reference)))
         RARCH_LOG("[Shaders]: Saved shader preset to \"%s\".\n", preset_path);
      else
         RARCH_ERR("[Shaders]: Failed writing shader preset to \"%s\".\n", preset_path);
   }
   else
   {
      char basedir[DIR_MAX_LENGTH];
      char buffer[DIR_MAX_LENGTH];

      for (i = 0; i < num_target_dirs; i++)
      {
         if (string_is_empty(target_dirs[i]))
            continue;

         fill_pathname_join(buffer, target_dirs[i],
               fullname, sizeof(buffer));

         strlcpy(basedir, buffer, sizeof(basedir));
         path_basedir(basedir);

         if (!path_is_directory(basedir))
         {
            if (!(ret = path_mkdir(basedir)))
            {
               RARCH_WARN("[Shaders]: Failed to create preset directory \"%s\".\n", basedir);
               continue;
            }
         }

         preset_path = buffer;

         if ((ret = video_shader_write_preset(preset_path,
               shader, save_reference)))
         {
            RARCH_LOG("[Shaders]: Saved shader preset to \"%s\".\n", preset_path);
            break;
         }
         else
            RARCH_WARN("[Shaders]: Failed writing shader preset to \"%s\".\n", preset_path);
      }

      if (!ret)
         RARCH_ERR("[Shaders]: Failed to write shader preset. Make sure shader directory "
               "and/or config directory are writable.\n");
   }

   if (ret && apply)
      menu_shader_manager_set_preset(NULL, type, preset_path, true);

   return ret;
}


/**
 * menu_shader_manager_save_preset:
 * @shader                   : shader to save
 * @type                     : type of shader preset which determines save path
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(const struct video_shader *shader,
      const char *basename,
      const char *dir_video_shader,
      const char *dir_menu_config,
      bool apply)
{
   char config_directory[DIR_MAX_LENGTH];
   const char *preset_dirs[3]  = {0};
   settings_t *settings        = config_get_ptr();


   if (!path_is_empty(RARCH_PATH_CONFIG))
   {
      strlcpy(config_directory,
            path_get(RARCH_PATH_CONFIG),
            sizeof(config_directory));
      path_basedir(config_directory);
   }
   else
      config_directory[0]      = '\0';

   preset_dirs[0] = dir_video_shader;
   preset_dirs[1] = dir_menu_config;
   preset_dirs[2] = config_directory;

   return menu_shader_manager_save_preset_internal(
         settings->bools.video_shader_preset_save_reference_enable,
         shader, basename,
         dir_video_shader,
         apply,
         preset_dirs,
         ARRAY_SIZE(preset_dirs));
}

static bool menu_shader_manager_operate_auto_preset(
      enum auto_shader_operation op,
      const struct video_shader *shader,
      const char *dir_video_shader,
      const char *dir_menu_config,
      enum auto_shader_type type, bool apply)
{
   char file[PATH_MAX_LENGTH];
   char old_presets_directory[DIR_MAX_LENGTH];
   char config_directory[DIR_MAX_LENGTH];
   settings_t *settings                           = config_get_ptr();
   bool video_shader_preset_save_reference_enable = settings->bools.video_shader_preset_save_reference_enable;
   struct retro_system_info *sysinfo              = &runloop_state_get_ptr()->system.info;
   static enum rarch_shader_type shader_types[]   =
   {
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG
   };
   const char *core_name              = sysinfo ? sysinfo->library_name : NULL;
   const char *rarch_path_basename    = path_get(RARCH_PATH_BASENAME);
   const char *auto_preset_dirs[3]    = {0};
   bool has_content                   = !string_is_empty(rarch_path_basename);

   if (type != SHADER_PRESET_GLOBAL && string_is_empty(core_name))
      return false;

   if (    !has_content
       && ((type == SHADER_PRESET_GAME)
       ||  (type == SHADER_PRESET_PARENT)))
      return false;

   if (!path_is_empty(RARCH_PATH_CONFIG))
   {
      strlcpy(config_directory,
            path_get(RARCH_PATH_CONFIG),
            sizeof(config_directory));
      path_basedir(config_directory);
   }
   else
      config_directory[0]      = '\0';

   /* We are only including this directory for compatibility purposes with
    * versions 1.8.7 and older. */
   if (op != AUTO_SHADER_OP_SAVE && !string_is_empty(dir_video_shader))
      fill_pathname_join_special(
            old_presets_directory,
            dir_video_shader,
            "presets",
            sizeof(old_presets_directory));
   else
      old_presets_directory[0] = '\0';

   auto_preset_dirs[0] = dir_menu_config;
   auto_preset_dirs[1] = config_directory;
   auto_preset_dirs[2] = old_presets_directory;

   switch (type)
   {
      case SHADER_PRESET_GLOBAL:
         strlcpy(file, "global", sizeof(file));
         break;
      case SHADER_PRESET_CORE:
         fill_pathname_join_special(file, core_name, core_name, sizeof(file));
         break;
      case SHADER_PRESET_PARENT:
         {
            char tmp_dir[DIR_MAX_LENGTH];
            fill_pathname_parent_dir_name(tmp_dir,
                  rarch_path_basename, sizeof(tmp_dir));
            fill_pathname_join_special(file, core_name, tmp_dir, sizeof(file));
         }
         break;
      case SHADER_PRESET_GAME:
         {
            const char *game_name = path_basename(rarch_path_basename);
            if (string_is_empty(game_name))
               return false;
            fill_pathname_join_special(file, core_name, game_name, sizeof(file));
            break;
         }
      default:
         return false;
   }

   switch (op)
   {
      case AUTO_SHADER_OP_SAVE:
         return menu_shader_manager_save_preset_internal(
               video_shader_preset_save_reference_enable,
               shader, file,
               dir_video_shader,
               apply,
               auto_preset_dirs,
               ARRAY_SIZE(auto_preset_dirs));
      case AUTO_SHADER_OP_REMOVE:
         {
            /* remove all supported auto-shaders of given type */
            char *end;
            size_t i, j, m;

            char preset_path[PATH_MAX_LENGTH];

            /* n = amount of relevant shader presets found
             * m = amount of successfully deleted shader presets */
            size_t n = m = 0;

            for (i = 0; i < ARRAY_SIZE(auto_preset_dirs); i++)
            {
               if (string_is_empty(auto_preset_dirs[i]))
                  continue;

               fill_pathname_join(preset_path,
                     auto_preset_dirs[i], file, sizeof(preset_path));
               end = preset_path + strlen(preset_path);

               for (j = 0; j < ARRAY_SIZE(shader_types); j++)
               {
                  const char *preset_ext;

                  if (!video_shader_is_supported(shader_types[j]))
                     continue;

                  preset_ext = video_shader_get_preset_extension(shader_types[j]);
                  strlcpy(end, preset_ext, sizeof(preset_path) - (end - preset_path));

                  if (path_is_valid(preset_path))
                  {
                     n++;

                     if (!filestream_delete(preset_path))
                     {
                        m++;
                        RARCH_LOG("[Shaders]: Deleted shader preset from \"%s\".\n", preset_path);
                     }
                     else
                        RARCH_WARN("[Shaders]: Failed to remove shader preset at \"%s\".\n", preset_path);
                  }
               }
            }

            return n == m;
         }
      case AUTO_SHADER_OP_EXISTS:
         {
            /* test if any supported auto-shaders of given type exists */
            char *end;
            size_t i, j;

            char preset_path[PATH_MAX_LENGTH];

            for (i = 0; i < ARRAY_SIZE(auto_preset_dirs); i++)
            {
               if (string_is_empty(auto_preset_dirs[i]))
                  continue;

               fill_pathname_join(preset_path,
                     auto_preset_dirs[i], file, sizeof(preset_path));
               end = preset_path + strlen(preset_path);

               for (j = 0; j < ARRAY_SIZE(shader_types); j++)
               {
                  const char *preset_ext;

                  if (!video_shader_is_supported(shader_types[j]))
                     continue;

                  preset_ext = video_shader_get_preset_extension(shader_types[j]);
                  strlcpy(end, preset_ext, sizeof(preset_path) - (end - preset_path));

                  if (path_is_valid(preset_path))
                     return true;
               }
            }
         }
         break;
   }

   return false;
}

/**
 * menu_shader_manager_remove_auto_preset:
 * @type                     : type of shader preset to delete
 *
 * Deletes an auto-shader.
 **/
bool menu_shader_manager_remove_auto_preset(
      enum auto_shader_type type,
      const char *dir_video_shader,
      const char *dir_menu_config)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_REMOVE, NULL,
         dir_video_shader,
         dir_menu_config,
         type, false);
}

/**
 * menu_shader_manager_auto_preset_exists:
 * @type                     : type of shader preset
 *
 * Tests if an auto-shader of the given type exists.
 **/
bool menu_shader_manager_auto_preset_exists(
      enum auto_shader_type type,
      const char *dir_video_shader,
      const char *dir_menu_config)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_EXISTS, NULL,
         dir_video_shader,
         dir_menu_config,
         type, false);
}

/**
 * menu_shader_manager_save_auto_preset:
 * @shader                   : shader to save
 * @type                     : type of shader preset which determines save path
 * @apply                    : immediately set preset after saving
 *
 * Save a shader as an auto-shader to it's appropriate path:
 *    SHADER_PRESET_GLOBAL: <target dir>/global
 *    SHADER_PRESET_CORE:   <target dir>/<core name>/<core name>
 *    SHADER_PRESET_PARENT: <target dir>/<core name>/<parent>
 *    SHADER_PRESET_GAME:   <target dir>/<core name>/<game name>
 * Needs to be consistent with video_shader_load_auto_shader_preset()
 * Auto-shaders will be saved as a reference if possible
 **/
bool menu_shader_manager_save_auto_preset(
      const struct video_shader *shader,
      enum auto_shader_type type,
      const char *dir_video_shader,
      const char *dir_menu_config,
      bool apply)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_SAVE, shader,
         dir_video_shader,
         dir_menu_config,
         type, apply);
}
#endif

static enum action_iterate_type action_iterate_type(const char *label)
{
   if (string_is_equal(label, "info_screen"))
      return ITERATE_TYPE_INFO;
   if (string_starts_with_size(label, "help", STRLEN_CONST("help")))
      if (
               string_is_equal(label, "help")
            || string_is_equal(label, "help_controls")
            || string_is_equal(label, "help_what_is_a_core")
            || string_is_equal(label, "help_loading_content")
            || string_is_equal(label, "help_scanning_content")
            || string_is_equal(label, "help_change_virtual_gamepad")
            || string_is_equal(label, "help_audio_video_troubleshooting")
         )
         return ITERATE_TYPE_HELP;
   if (string_is_equal(label, "cheevos_description"))
         return ITERATE_TYPE_HELP;
   if (string_starts_with_size(label, "custom_bind", STRLEN_CONST("custom_bind")))
      if (
               string_is_equal(label, "custom_bind")
            || string_is_equal(label, "custom_bind_all")
            || string_is_equal(label, "custom_bind_defaults")
         )
         return ITERATE_TYPE_BIND;

   return ITERATE_TYPE_DEFAULT;
}

/* Returns true if search filter is enabled
 * for the specified menu list */
bool menu_driver_search_filter_enabled(const char *label, unsigned type)
{
   bool filter_enabled = false;

   /* > Check for playlists */
   filter_enabled =    (type == MENU_SETTING_HORIZONTAL_MENU)
                    || (type == MENU_HISTORY_TAB)
                    || (type == MENU_FAVORITES_TAB)
                    || (type == MENU_IMAGES_TAB)
                    || (type == MENU_MUSIC_TAB)
                    || (type == MENU_VIDEO_TAB)
                    || (type == FILE_TYPE_PLAYLIST_COLLECTION);

   if (!filter_enabled && !string_is_empty(label))
      filter_enabled =    string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST))
                       /* > Core updater */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST))
                       /* > File browser (Load Content) */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES))
                       /* > Shader presets/passes */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PASS))
                       /* > Cheat files */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD))
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND))
                       /* > Cheats */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS))
                       /* > Overlays */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OVERLAY))
                       /* > Manage Cores */
                       || string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST));

   return filter_enabled;
}

static void menu_input_key_bind_poll_bind_state(
      input_driver_state_t *input_st,
      const retro_keybind_set *binds,
      float input_axis_threshold,
      unsigned joy_idx,
      struct menu_bind_state *state,
      bool timed_out,
      bool keyboard_mapping_blocked)
{
   unsigned b;
   rarch_joypad_info_t joypad_info;
   input_driver_t *current_input           = input_st->current_driver;
   unsigned port                           = state->port;
   const input_device_driver_t *joypad     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = input_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   memset(state->state, 0, sizeof(state->state));

   joypad_info.axis_threshold           = input_axis_threshold;
   joypad_info.joy_idx                  = joy_idx;
   joypad_info.auto_binds               = input_autoconf_binds[joy_idx];

   if (current_input->input_state)
   {
      /* Poll mouse (on the relevant port)
       *
       * Check if key was being pressed by
       * user with mouse number 'port'
       *
       * NOTE: We start iterating on 2 (RETRO_DEVICE_ID_MOUSE_LEFT),
       * because we want to skip the axes
       */
      for (b = 2; b < MENU_MAX_MBUTTONS; b++)
      {
         state->state[port].mouse_buttons[b] =
            current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  binds,
                  keyboard_mapping_blocked,
                  port,
                  RETRO_DEVICE_MOUSE, 0, b);
      }

      for (b = RETROK_BACKSPACE; b < RETROK_LAST; b++)
      {
         state->state[port].keys[b] =
            current_input->input_state(
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  binds,
                  keyboard_mapping_blocked,
                  0,
                  RETRO_DEVICE_KEYBOARD, 0, b);
      }
   }

   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = NULL;
   joypad_info.axis_threshold = 0.0f;

   state->skip                = timed_out;

   if (joypad)
   {
      if (joypad->poll)
         joypad->poll();
      menu_input_key_bind_poll_bind_state_internal(
            joypad, state, port, timed_out);
   }

   if (sec_joypad)
   {
      if (sec_joypad->poll)
         sec_joypad->poll();
      menu_input_key_bind_poll_bind_state_internal(
            sec_joypad, state, port, timed_out);
   }
}

static int menu_dialog_iterate(
      menu_dialog_t *p_dialog,
      settings_t *settings,
      char *s, size_t len,
      retro_time_t current_time
)
{
   switch (p_dialog->current_type)
   {
      case MENU_DIALOG_WELCOME:
         {
            static rarch_timer_t timer;

            if (!timer.timer_begin)
            {
               timer.timeout_us  = 3 * 1000000;
               timer.current     = cpu_features_get_time_usec();
               timer.timeout_end = timer.current + timer.timeout_us;
               timer.timer_begin = true;
               timer.timer_end   = false;
            }

            timer.current    = current_time;
            timer.timeout_us = (timer.timeout_end = timer.current);

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_WELCOME_TO_RETROARCH,
                  s, len);

            if (!timer.timer_end && (timer.timeout_us <= 0))
            {
               timer.timer_end        = true;
               timer.timer_begin      = false;
               timer.timeout_end      = 0;
               p_dialog->current_type = MENU_DIALOG_NONE;
               return 1;
            }
         }
         break;
      case MENU_DIALOG_HELP_CONTROLS:
         {
            unsigned i;
            char s2[PATH_MAX_LENGTH];
            const unsigned binds[] = {
               RETRO_DEVICE_ID_JOYPAD_UP,
               RETRO_DEVICE_ID_JOYPAD_DOWN,
               RETRO_DEVICE_ID_JOYPAD_A,
               RETRO_DEVICE_ID_JOYPAD_B,
               RETRO_DEVICE_ID_JOYPAD_SELECT,
               RETRO_DEVICE_ID_JOYPAD_START,
               RARCH_MENU_TOGGLE,
               RARCH_QUIT_KEY,
               RETRO_DEVICE_ID_JOYPAD_X,
               RETRO_DEVICE_ID_JOYPAD_Y,
            };
            char desc[ARRAY_SIZE(binds)][64];

            for (i = 0; i < ARRAY_SIZE(binds); i++)
               desc[i][0] = '\0';

            for (i = 0; i < ARRAY_SIZE(binds); i++)
            {
               const struct retro_keybind *keybind = &input_config_binds[0][binds[i]];
               const struct retro_keybind *auto_bind =
                  (const struct retro_keybind*)
                  input_config_get_bind_auto(0, binds[i]);

               input_config_get_bind_string(settings, desc[i],
                     keybind, auto_bind, sizeof(desc[i]));
            }

            s2[0] = '\0';

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG,
                  s2, sizeof(s2));

            snprintf(s, len,
                  "%s"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n",

                  s2,

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP),
                  desc[0],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN),
                  desc[1],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM),
                  desc[2],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
                  desc[3],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO),
                  desc[4],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START),
                  desc[5],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU),
                  desc[6],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT),
                  desc[7],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD),
                  desc[8]

                  );
         }
         break;

#ifdef HAVE_CHEEVOS
      case MENU_DIALOG_HELP_CHEEVOS_DESCRIPTION:
         if (!rcheevos_menu_get_sublabel(p_dialog->current_id, s, len))
            return 1;
         break;
#endif

      case MENU_DIALOG_HELP_WHAT_IS_A_CORE:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_LOADING_CONTENT:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               s, len);
         break;
      case MENU_DIALOG_HELP_CHANGE_VIRTUAL_GAMEPAD:
         msg_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         msg_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_SCANNING_CONTENT:
         msg_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_EXTRACT:
         {
            bool bundle_finished        = settings->bools.bundle_finished;

            msg_hash_get_help_enum(
                  MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT,
                  s, len);

            if (bundle_finished)
            {
               configuration_set_bool(settings,
                     settings->bools.bundle_finished, false);
               p_dialog->current_type = MENU_DIALOG_NONE;
               return 1;
            }
         }
         break;
      case MENU_DIALOG_QUIT_CONFIRM:
      case MENU_DIALOG_INFORMATION:
      case MENU_DIALOG_QUESTION:
      case MENU_DIALOG_WARNING:
      case MENU_DIALOG_ERROR:
         msg_hash_get_help_enum(MSG_UNKNOWN,
               s, len);
         break;
      case MENU_DIALOG_NONE:
      default:
         break;
   }

   return 0;
}

static bool menu_entries_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx)
{
   if (!(menu_st->entries.list = (menu_list_t*)menu_list_new(menu_driver_ctx)))
      return false;
   if (!(menu_st->entries.list_settings = menu_setting_new()))
      return false;
   return true;
}

static void generic_menu_init_list(struct menu_state *menu_st,
      settings_t *settings)
{
   menu_displaylist_info_t info;
   menu_list_t *menu_list       = menu_st->entries.list;
   file_list_t *menu_stack      = NULL;
   file_list_t *selection_buf   = NULL;

   if (menu_list)
   {
      menu_stack                = MENU_LIST_GET(menu_list, (unsigned)0);
      selection_buf             = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);
   }

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append(menu_stack,
         info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0, NULL);

   info.list                    = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info, settings))
      menu_displaylist_process(&info);

   menu_displaylist_info_free(&info);
}

/* This function gets called at first startup on Android/iOS
 * when we need to extract the APK contents/zip file. This
 * file contains assets which then get extracted to the
 * user's asset directories. */
static void bundle_decompressed(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   settings_t        *settings = config_get_ptr();
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (!err)
         command_event(CMD_EVENT_REINIT, NULL);

      /* delete bundle? */
      free(dec->source_file);
      free(dec);
   }

   configuration_set_uint(settings,
         settings->uints.bundle_assets_extract_last_version,
         settings->uints.bundle_assets_extract_version_current);

   configuration_set_bool(settings, settings->bools.bundle_finished, true);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
}

static bool rarch_menu_init(
      struct menu_state *menu_st,
      menu_dialog_t        *p_dialog,
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_input_t *menu_input,
      menu_input_pointer_hw_state_t *pointer_hw_state,
      settings_t *settings
      )
{
#ifdef HAVE_CONFIGFILE
   bool menu_show_start_screen = settings->bools.menu_show_start_screen;
   bool config_save_on_exit    = settings->bools.config_save_on_exit;
#endif

   /* thumbnail initialization */
   if (!(menu_st->thumbnail_path_data = gfx_thumbnail_path_init()))
      return false;

   /* Ensure that menu pointer input is correctly
    * initialised */
   memset(menu_input, 0, sizeof(menu_input_t));
   memset(pointer_hw_state, 0, sizeof(menu_input_pointer_hw_state_t));

   if (!menu_entries_init(menu_st, menu_driver_ctx))
   {
      menu_entries_settings_deinit(menu_st);
      if (menu_st->entries.list)
         menu_list_free(menu_driver_ctx, menu_st->entries.list);
      menu_st->entries.list          = NULL;
      return false;
   }

#ifdef HAVE_CONFIGFILE
   if (menu_show_start_screen)
   {
      /* We don't want the welcome dialog screen to show up
       * again after the first startup, so we save to config
       * file immediately. */
      p_dialog->current_type         = MENU_DIALOG_WELCOME;

      configuration_set_bool(settings,
            settings->bools.menu_show_start_screen, false);
      if (config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
   }
#endif

#ifdef HAVE_COMPRESSION
   if (      settings->bools.bundle_assets_extract_enable
         && !string_is_empty(settings->paths.bundle_assets_src)
         && !string_is_empty(settings->paths.bundle_assets_dst)
         && (settings->uints.bundle_assets_extract_version_current
            != settings->uints.bundle_assets_extract_last_version)
      )
   {
      p_dialog->current_type         = MENU_DIALOG_HELP_EXTRACT;
      task_push_decompress(
            settings->paths.bundle_assets_src,
            settings->paths.bundle_assets_dst,
            NULL,
            settings->paths.bundle_assets_dst_subdir,
            NULL,
            bundle_decompressed,
            NULL,
            NULL,
            false);
      /* Support only 1 version - setting this would prevent the assets from being extracted every time */
      configuration_set_int(settings,
            settings->uints.bundle_assets_extract_last_version, 1);
   }
#endif

   return true;
}

static void menu_input_set_pointer_visibility(
      menu_input_pointer_hw_state_t *pointer_hw_state,
      menu_input_t *menu_input,
      retro_time_t current_time)
{
   static bool cursor_shown          = false;
   static bool cursor_hidden         = false;
   static retro_time_t end_time      = 0;
   struct menu_state       *menu_st  = &menu_driver_state;

   /* Ensure that mouse cursor is hidden when not in use */
   if (     (menu_input->pointer.type == MENU_POINTER_MOUSE)
         && ((pointer_hw_state->flags & MENU_INP_PTR_FLG_ACTIVE) > 0))
   {
      /* Show cursor */
      if ((current_time > end_time) && !cursor_shown)
      {
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_ENABLE_MOUSE_CURSOR,
                     NULL, menu_st->userdata);
         cursor_shown  = true;
         cursor_hidden = false;
      }

      end_time = current_time + MENU_INPUT_HIDE_CURSOR_DELAY;
   }
   else
   {
      /* Hide cursor */
      if ((current_time > end_time) && !cursor_hidden)
      {
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_MOUSE_CURSOR,
                  NULL, menu_st->userdata);
         cursor_shown  = false;
         cursor_hidden = true;
      }
   }
}

void menu_entries_build_scroll_indices(
      struct menu_state *menu_st,
      file_list_t *list)
{
   bool current_is_dir             = false;
   size_t i                        = 0;
   const char *path                = list->list[0].alt
                                   ? list->list[0].alt
                                   : list->list[0].path;
   int ret                         = path ? TOLOWER((int)*path) : 0;
   int current                     = ELEM_GET_FIRST_CHAR(ret);
   unsigned type                   = list->list[0].type;

   menu_st->scroll.index_list[0]   = 0;
   menu_st->scroll.index_size      = 1;

   if (type == FILE_TYPE_DIRECTORY)
      current_is_dir               = true;

   for (i = 1; i < list->size; i++)
   {
      int first;
      bool is_dir  = false;
      unsigned idx = (unsigned)i;
      path         = list->list[i].alt
                   ? list->list[i].alt
                   : list->list[i].path;
      ret          = path ? TOLOWER((int)*path) : 0;
      first        = ELEM_GET_FIRST_CHAR(ret);
      type         = list->list[idx].type;

      if (type == FILE_TYPE_DIRECTORY)
         is_dir    = true;

      if ((current_is_dir && !is_dir) || (first != current))
      {
         /* Add scroll index */
         menu_st->scroll.index_list[menu_st->scroll.index_size]   = i;
         if (!((menu_st->scroll.index_size + 1) >= SCROLL_INDEX_SIZE))
            menu_st->scroll.index_size++;
      }

      current        = first;
      current_is_dir = is_dir;
   }

   /* Add scroll index */
   menu_st->scroll.index_list[menu_st->scroll.index_size]   = list->size - 1;
   if (!((menu_st->scroll.index_size + 1) >= SCROLL_INDEX_SIZE))
      menu_st->scroll.index_size++;
}

void menu_display_common_image_upload(void *data, void *user_data, unsigned type)
{
   struct texture_image                *img = (struct texture_image*)data;
   struct menu_state               *menu_st = &menu_driver_state;
   const menu_ctx_driver_t *menu_driver_ctx = menu_st->driver_ctx;
   void                      *menu_userdata = menu_st->userdata;
   if (     menu_driver_ctx
         && menu_driver_ctx->load_image)
      menu_driver_ctx->load_image(menu_userdata,
            img, (enum menu_image_type)type);

   image_texture_free(img);
   free(img);
   free(user_data);
}

static enum menu_driver_id_type menu_driver_set_id(
      const char *driver_name)
{
   if (!string_is_empty(driver_name))
   {
      if (string_is_equal(driver_name, "rgui"))
         return MENU_DRIVER_ID_RGUI;
      else if (string_is_equal(driver_name, "ozone"))
         return MENU_DRIVER_ID_OZONE;
      else if (string_is_equal(driver_name, "glui"))
         return MENU_DRIVER_ID_GLUI;
      else if (string_is_equal(driver_name, "xmb"))
         return MENU_DRIVER_ID_XMB;
   }
   return MENU_DRIVER_ID_UNKNOWN;
}

static bool menu_entries_search_push(const char *search_term)
{
   size_t i;
   char search_term_clipped[MENU_SEARCH_FILTER_MAX_LENGTH];
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   /* Sanity check + verify whether we have reached
    * the maximum number of allowed search terms */
   if (  !search
       || string_is_empty(search_term)
       || (search->size >= MENU_SEARCH_FILTER_MAX_TERMS))
      return false;

   /* Check whether search term already exists
    * > Note that we clip the input search term
    *   to MENU_SEARCH_FILTER_MAX_LENGTH characters
    *   *before* comparing existing entries */
   strlcpy(search_term_clipped, search_term,
         sizeof(search_term_clipped));

   for (i = 0; i < search->size; i++)
   {
      if (string_is_equal(search_term_clipped,
            search->terms[i]))
         return false;
   }

   /* Add search term */
   strlcpy(search->terms[search->size], search_term_clipped,
         sizeof(search->terms[search->size]));
   search->size++;

   return true;
}

bool menu_entries_search_pop(void)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   /* Do nothing if list of search terms is empty */
   if (   !search
       || (search->size == 0))
      return false;

   /* Remove last item from the list */
   search->size--;
   search->terms[search->size][0] = '\0';

   return true;
}

menu_search_terms_t *menu_entries_search_get_terms(void)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   if (   !search
       || (search->size == 0))
      return NULL;

   return search;
}

void menu_entries_search_append_terms_string(char *s, size_t len)
{
   menu_search_terms_t *search = menu_entries_search_get_terms_internal();

   if (    search
       && (search->size > 0)
       && s)
   {
      size_t current_len = strlen_size(s, len);
      size_t i;

      /* If buffer is already 'full', nothing
       * further can be added */
      if (current_len >= len)
         return;

      s   += current_len;
      len -= current_len;

      for (i = 0; i < search->size; i++)
      {
         strlcat(s, " > ", len);
         strlcat(s, search->terms[i], len);
      }
   }
}

void get_current_menu_value(struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;
   const char*      entry_label;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags    |= MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

   if (entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      entry_label  = entry.password_value;
   else
      entry_label  = entry.value;

   strlcpy(s, entry_label, len);
}

static void get_current_menu_type(struct menu_state *menu_st,
      uint8_t *setting_type)
{
   menu_entry_t     entry;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags    = MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

   *setting_type  = entry.setting_type;
}

#ifdef HAVE_ACCESSIBILITY
static void menu_driver_get_current_menu_label(struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;
   const char*      entry_label;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                | MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                | MENU_ENTRY_FLAG_VALUE_ENABLED
                | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);

   if (!string_is_empty(entry.rich_label))
      entry_label              = entry.rich_label;
   else
      entry_label              = entry.path;

   strlcpy(s, entry_label, len);
}
#endif

static void menu_driver_get_current_menu_sublabel(
      struct menu_state *menu_st,
      char *s, size_t len)
{
   menu_entry_t     entry;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);
   strlcpy(s, entry.sublabel, len);
}

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx)
{
   file_list_t *list              = NULL;
   struct menu_state    *menu_st  = &menu_driver_state;
   if (!menu_st->entries.list)
      return;

   list                           = MENU_LIST_GET(menu_st->entries.list, 0);

   if (list && list->size)
   {
      if (path)
         *path      = list->list[list->size - 1].path;
      if (label)
         *label     = list->list[list->size - 1].label;
      if (file_type)
         *file_type = list->list[list->size - 1].type;
      if (entry_idx)
         *entry_idx = list->list[list->size - 1].entry_idx;
   }

   if (enum_idx)
   {
      menu_file_list_cbs_t *cbs  = (menu_file_list_cbs_t*)
         list->list[list->size - 1].actiondata;

      if (cbs)
         *enum_idx = cbs->enum_idx;
   }
}

int menu_driver_deferred_push_content_list(file_list_t *list)
{
   settings_t *settings           = config_get_ptr();
   struct menu_state    *menu_st  = &menu_driver_state;
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *selection_buf     = MENU_LIST_GET_SELECTION(menu_list, (unsigned)0);

   menu_st->selection_ptr         = 0;
   menu_st->contentless_core_ptr  = 0;

   menu_contentless_cores_flush_runtime();

   if (!menu_driver_displaylist_push(
            menu_st,
            settings,
            list,
            selection_buf))
      return -1;
   return 0;
}

retro_time_t menu_driver_get_current_time(void)
{
   struct menu_state    *menu_st  = &menu_driver_state;
   return menu_st->current_time_us;
}

void menu_driver_set_pending_selection(const char *pending_selection)
{
   struct menu_state *menu_st  = &menu_driver_state;
   char *selection             = menu_st->pending_selection;

   /* Reset existing cache */
   selection[0] = '\0';
   if (!string_is_empty(pending_selection))
      strlcpy(selection, pending_selection,
            sizeof(menu_st->pending_selection));
}

static void menu_input_search_cb(void *userdata, const char *str)
{
   const char *label           = NULL;
   unsigned type               = MENU_SETTINGS_NONE;
   struct menu_state *menu_st  = &menu_driver_state;
   const file_list_t *list     = MENU_LIST_GET(menu_st->entries.list, 0);

   if (string_is_empty(str))
      goto end;

   /* Determine whether we are currently
    * viewing a menu list with 'search
    * filter' support */
   if (list && list->size)
   {
      label     = list->list[list->size - 1].label;
      type      = list->list[list->size - 1].type;
   }

   /* Do not apply search filter if string
    * consists of a single Latin alphabet
    * character */
   if ( ((str[1] != '\0') || (!ISALPHA(str[0])))
       && menu_driver_search_filter_enabled(label, type))
   {
      /* Add search term */
      if (menu_entries_search_push(str))
      {
         /* Reset navigation pointer */
         menu_st->selection_ptr     = 0;
         if (menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, false);
         /* Refresh menu */
         menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                                    |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      }
   }
   /* Perform a regular search: jump to the
    * first matching entry */
   else
   {
      size_t idx = 0;

      if (menu_entries_list_search(str, &idx))
      {
         menu_st->selection_ptr = idx;
         if (menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
      }
   }

end:
   menu_input_dialog_end();
}

int menu_entry_action(menu_entry_t *entry, size_t i, enum menu_action action)
{
   struct menu_state *menu_st     = &menu_driver_state;
   if (     menu_st->driver_ctx
         && menu_st->driver_ctx->entry_action)
      return menu_st->driver_ctx->entry_action(
            menu_st->userdata, entry, i, action);
   return -1;
}

bool menu_entries_append(
      file_list_t *list,
      const char *path,
      const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type,
      size_t directory_ptr,
      size_t entry_idx,
      rarch_setting_t *setting)
{
   menu_ctx_list_t list_info;
   size_t i;
   size_t idx, lbl_len;
   const char *menu_path       = NULL;
   menu_file_list_cbs_t *cbs   = NULL;
   struct menu_state  *menu_st = &menu_driver_state;
   const file_list_t *mlist    = MENU_LIST_GET(menu_st->entries.list, 0);

   if (!list || !label)
      return false;

   file_list_append(list, path, label, type, directory_ptr, entry_idx);
   if (mlist && mlist->size)
      menu_path          = mlist->list[mlist->size - 1].path;
   idx                   = list->size - 1;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   if (  menu_st->driver_ctx &&
         menu_st->driver_ctx->list_insert)
      menu_st->driver_ctx->list_insert(
            menu_st->userdata,
            list_info.list,
            list_info.path,
            list_info.fullpath,
            list_info.label,
            list_info.idx,
            list_info.entry_type);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);

   if (!(cbs = (menu_file_list_cbs_t*)
      malloc(sizeof(menu_file_list_cbs_t))))
      return false;

   cbs->action_sublabel_cache[0]   = '\0';
   cbs->action_title_cache[0]      = '\0';
   cbs->enum_idx                   = enum_idx;
   cbs->checked                    = false;
   cbs->setting                    = setting;
   cbs->action_iterate             = NULL;
   cbs->action_deferred_push       = NULL;
   cbs->action_select              = NULL;
   cbs->action_get_title           = NULL;
   cbs->action_ok                  = NULL;
   cbs->action_cancel              = NULL;
   cbs->action_scan                = NULL;
   cbs->action_start               = NULL;
   cbs->action_info                = NULL;
   cbs->action_left                = NULL;
   cbs->action_right               = NULL;
   cbs->action_label               = NULL;
   cbs->action_sublabel            = NULL;
   cbs->action_get_value           = NULL;

   cbs->search.size                = 0;
   for (i = 0; i < MENU_SEARCH_FILTER_MAX_TERMS; i++)
      cbs->search.terms[i][0]      = '\0';

   list->list[idx].actiondata      = cbs;

   if (!cbs->setting && enum_idx != MSG_UNKNOWN)
   {
      if (     enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY
            && enum_idx != MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY
            && enum_idx != MENU_ENUM_LABEL_EXPLORE_ITEM
            && enum_idx != MENU_ENUM_LABEL_CONTENTLESS_CORE
            && enum_idx != MENU_ENUM_LABEL_RDB_ENTRY)
         cbs->setting                 = menu_setting_find_enum(enum_idx);
   }

   lbl_len  = strlen(label);

   menu_cbs_init(menu_st,
         menu_st->driver_ctx,
         list, cbs, path, label, lbl_len, type, idx);

   return true;
}

void menu_entries_prepend(file_list_t *list,
      const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   size_t lbl_len;
   menu_ctx_list_t list_info;
   size_t i;
   size_t idx                  = 0;
   const char *menu_path       = NULL;
   menu_file_list_cbs_t *cbs   = NULL;
   struct menu_state  *menu_st = &menu_driver_state;
   const file_list_t *mlist    = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!list || !label)
      return;

   file_list_insert(list, path, label, type, directory_ptr, entry_idx, 0);
   if (mlist && mlist->size)
      menu_path          = mlist->list[mlist->size - 1].path;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   if (  menu_st->driver_ctx &&
         menu_st->driver_ctx->list_insert)
      menu_st->driver_ctx->list_insert(
            menu_st->userdata,
            list_info.list,
            list_info.path,
            list_info.fullpath,
            list_info.label,
            list_info.idx,
            list_info.entry_type);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs                             = (menu_file_list_cbs_t*)
      malloc(sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   cbs->action_sublabel_cache[0]   = '\0';
   cbs->action_title_cache[0]      = '\0';
   cbs->enum_idx                   = enum_idx;
   cbs->checked                    = false;
   cbs->setting                    = menu_setting_find_enum(cbs->enum_idx);
   cbs->action_iterate             = NULL;
   cbs->action_deferred_push       = NULL;
   cbs->action_select              = NULL;
   cbs->action_get_title           = NULL;
   cbs->action_ok                  = NULL;
   cbs->action_cancel              = NULL;
   cbs->action_scan                = NULL;
   cbs->action_start               = NULL;
   cbs->action_info                = NULL;
   cbs->action_left                = NULL;
   cbs->action_right               = NULL;
   cbs->action_label               = NULL;
   cbs->action_sublabel            = NULL;
   cbs->action_get_value           = NULL;

   cbs->search.size                = 0;
   for (i = 0; i < MENU_SEARCH_FILTER_MAX_TERMS; i++)
      cbs->search.terms[i][0]      = '\0';

   list->list[idx].actiondata      = cbs;

   lbl_len  = strlen(label);

   menu_cbs_init(menu_st,
         menu_st->driver_ctx,
         list, cbs, path, label, lbl_len, type, idx);
}

void menu_entries_flush_stack(const char *needle, unsigned final_type)
{
   struct menu_state  *menu_st    = &menu_driver_state;
   menu_list_t *menu_list         = menu_st->entries.list;
   if (menu_list)
      menu_list_flush_stack(
            menu_st->driver_ctx,
            menu_st->userdata,
            menu_st,
            menu_list, 0, needle, final_type);
}

void menu_entries_pop_stack(size_t *ptr, size_t idx, bool animate)
{
   struct menu_state    *menu_st            = &menu_driver_state;
   const menu_ctx_driver_t *menu_driver_ctx = menu_st->driver_ctx;
   menu_list_t *menu_list                   = menu_st->entries.list;
   if (!menu_list)
      return;

   if (MENU_LIST_GET_STACK_SIZE(menu_list, idx) > 1)
   {
      if (animate)
      {
         if (menu_driver_ctx->list_cache)
            menu_driver_ctx->list_cache(menu_st->userdata,
                  MENU_LIST_PLAIN, 0);
      }
      menu_list_pop_stack(menu_driver_ctx,
            menu_st->userdata, menu_list, idx, ptr);

      if (animate)
         menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }
}

bool menu_entries_clear(file_list_t *list)
{
   size_t i;
   struct menu_state    *menu_st  = &menu_driver_state;

   if (!list)
      return false;

   /* Clear all the menu lists. */
   if (menu_st->driver_ctx->list_clear)
      menu_st->driver_ctx->list_clear(list);

   for (i = 0; i < list->size; i++)
   {
      if (list->list[i].actiondata)
         free(list->list[i].actiondata);
      list->list[i].actiondata = NULL;
   }

   file_list_clear(list);
   return true;
}

/* Function that gets called when we want to load in a
 * new menu wallpaper.
 */
void menu_display_handle_wallpaper_upload(
      retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   menu_display_common_image_upload(
         (struct texture_image*)task_data,
         user_data,
         MENU_IMAGE_WALLPAPER);
}

void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info)
{
   struct menu_state    *menu_st = &menu_driver_state;
   if (menu_is_alive && menu_st->driver_ctx->frame)
      menu_st->driver_ctx->frame(menu_st->userdata, video_info);
}

/* Teardown function for the menu driver. */
void menu_driver_destroy(
      struct menu_state *menu_st)
{
   menu_st->flags &= ~(MENU_ST_FLAG_PENDING_QUICK_MENU
                     | MENU_ST_FLAG_PREVENT_POPULATE
                     | MENU_ST_FLAG_DATA_OWN
                     | MENU_ST_FLAG_ALIVE);
   menu_st->driver_ctx                  = NULL;
   menu_st->userdata                    = NULL;
   menu_st->input_driver_flushing_input = 0;
}

void menu_input_get_pointer_state(menu_input_pointer_t *copy_target)
{
   struct menu_state    *menu_st  = &menu_driver_state;
   menu_input_t       *menu_input = &menu_st->input_state;

   /* Copy parameters from global menu_input_state
    * (i.e. don't pass by reference)
    * This is a fast operation */
   if (copy_target)
      memcpy(copy_target, &menu_input->pointer, sizeof(menu_input_pointer_t));
}

const char *menu_input_dialog_get_buffer(void)
{
   struct menu_state    *menu_st  = &menu_driver_state;
   if (!(*menu_st->input_dialog_keyboard_buffer))
      return "";
   return *menu_st->input_dialog_keyboard_buffer;
}

/* This callback gets triggered by the keyboard whenever
 * we press or release a keyboard key. When a keyboard
 * key is being pressed down, 'down' will be true. If it
 * is being released, 'down' will be false.
 */
static void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   struct menu_state *menu_st  = &menu_driver_state;
   enum retro_key          key = (enum retro_key)keycode;

   if (key == RETROK_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < RETROK_LAST; i++)
         menu_st->kb_key_state[i] =
            (menu_st->kb_key_state[(enum retro_key)i] & 1) << 1;
   }
   else
      menu_st->kb_key_state[key]  =
         ((menu_st->kb_key_state[key] & 1) << 1) | down;
}

void menu_input_dialog_end(void)
{
   struct menu_state *menu_st                 = &menu_driver_state;
   menu_st->input_dialog_kb_type              = 0;
   menu_st->input_dialog_kb_idx               = 0;
   menu_st->flags                            &= ~MENU_ST_FLAG_INP_DLG_KB_DISPLAY;
   menu_st->input_dialog_kb_label[0]          = '\0';
   menu_st->input_dialog_kb_label_setting[0]  = '\0';

   /* Avoid triggering states on pressing return. */
   /* Inhibits input for 2 frames
    * > Required, since input is ignored for 1 frame
    *   after certain events - e.g. closing the OSK */
   menu_st->input_driver_flushing_input       = 2;
}

#if defined(_MSC_VER)
static const char * msvc_vercode_to_str(const unsigned vercode)
{
   switch (vercode)
   {
      case 1200:
         return " msvc6";
      case 1300:
         return " msvc2002";
      case 1310:
         return " msvc2003";
      case 1400:
         return " msvc2005";
      case 1500:
         return " msvc2008";
      case 1600:
         return " msvc2010";
      case 1700:
         return " msvc2012";
      case 1800:
         return " msvc2013";
      case 1900:
         return " msvc2015";
      default:
         if (vercode >= 1910 && vercode < 1920)
            return " msvc2017";
         else if (vercode >= 1920 && vercode < 1930)
            return " msvc2019";
         else if (vercode >= 1930)
            return " msvc2022";
         break;
   }

   return "";
}
#endif

/* Sets 's' to the name of the current core
 * (shown at the top of the UI). */
void menu_entries_get_core_title(char *s, size_t len)
{
   struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
   const char *core_name             =
       (sysinfo && !string_is_empty(sysinfo->library_name))
      ? sysinfo->library_name
      : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
   const char *core_version          =
      (sysinfo && sysinfo->library_version)
      ? sysinfo->library_version
      : "";
   size_t _len = strlcpy(s, PACKAGE_VERSION, len);
#if defined(_MSC_VER)
   _len += strlcpy(s + _len, msvc_vercode_to_str(_MSC_VER), len - _len);
#endif
   _len += strlcpy(s + _len, " - ",     len - _len);
   _len += strlcpy(s + _len, core_name, len - _len);
   if (!string_is_empty(core_version))
      snprintf(s + _len, len - _len, " (%s)", core_version);
}

static bool menu_driver_init_internal(
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      settings_t *settings,
      bool video_is_threaded)
{
   if (menu_st->driver_ctx)
   {
      const char *ident = menu_st->driver_ctx->ident;
      /* ID must be set first, since it is required for
       * the proper determination of pixel/DPI scaling
       * parameters (and some menu drivers fetch the
       * current pixel/dpi scale during 'menu_driver_ctx->init()') */
      if (ident)
         p_disp->menu_driver_id             = menu_driver_set_id(ident);
      else
         p_disp->menu_driver_id             = MENU_DRIVER_ID_UNKNOWN;

      if (menu_st->driver_ctx->init)
      {
         menu_st->driver_data               = (menu_handle_t*)
            menu_st->driver_ctx->init(&menu_st->userdata,
                  video_is_threaded);
         menu_st->driver_data->userdata     = menu_st->userdata;
         menu_st->driver_data->driver_ctx   = menu_st->driver_ctx;
      }
   }

   if (!menu_st->driver_data || !rarch_menu_init(
            menu_st,
            &menu_st->dialog_st,
            menu_st->driver_ctx,
            &menu_st->input_state,
            &menu_st->input_pointer_hw_state,
            settings))
      return false;

   gfx_display_init();

   /* TODO/FIXME - can we get rid of this? Is this needed? */
   configuration_set_string(settings,
         settings->arrays.menu_driver, menu_st->driver_ctx->ident);

   if (menu_st->driver_ctx->lists_init)
   {
      if (!menu_st->driver_ctx->lists_init(menu_st->driver_data))
         return false;
   }
   else
      generic_menu_init_list(menu_st, settings);

   /* Initialise menu screensaver */
   menu_st->input_last_time_us    = cpu_features_get_time_usec();
   menu_st->flags                &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
   if (     menu_st->driver_ctx->environ_cb
         && (menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
               NULL, menu_st->userdata) == 0))
      menu_st->flags |=  MENU_ST_FLAG_SCREENSAVER_SUPPORTED;
   else
      menu_st->flags &= ~MENU_ST_FLAG_SCREENSAVER_SUPPORTED;

   return true;
}

bool menu_driver_init(bool video_is_threaded)
{
   gfx_display_t            *p_disp  = disp_get_ptr();
   settings_t             *settings  = config_get_ptr();
   struct menu_state       *menu_st  = &menu_driver_state;

   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);

   if (     menu_st->driver_data
         || menu_driver_init_internal(
            menu_st, p_disp, settings,
            video_is_threaded))
   {
      if (menu_st->driver_ctx && menu_st->driver_ctx->context_reset)
      {
         menu_st->driver_ctx->context_reset(menu_st->userdata,
               video_is_threaded);
         return true;
      }
   }

   /* If driver initialisation failed, must reset
    * driver id to 'unknown' */
   p_disp->menu_driver_id = MENU_DRIVER_ID_UNKNOWN;

   return false;
}

const char *menu_driver_ident(void)
{
   struct menu_state    *menu_st  = &menu_driver_state;
   if (menu_st->driver_ctx && menu_st->driver_ctx->ident)
      return menu_st->driver_ctx->ident;
   return NULL;
}

const menu_ctx_driver_t *menu_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled)
{
   int i = (int)driver_find_index("menu_driver",
         settings->arrays.menu_driver);

   if (i >= 0)
      return (const menu_ctx_driver_t*)menu_ctx_drivers[i];

   if (verbosity_enabled)
   {
      unsigned d;
      RARCH_WARN("Couldn't find any %s named \"%s\".\n", prefix,
            settings->arrays.menu_driver);
      RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
      for (d = 0; menu_ctx_drivers[d]; d++)
      {
         if (menu_ctx_drivers[d])
         {
            RARCH_LOG_OUTPUT("\t%s\n", menu_ctx_drivers[d]->ident);
         }
      }
      RARCH_WARN("Going to default to first %s..\n", prefix);
   }

   return (const menu_ctx_driver_t*)menu_ctx_drivers[0];
}

#ifdef USE_CUSTOM_BIND_KEYBOARD_CB
static bool menu_input_key_bind_custom_bind_keyboard_cb(
      void *data, unsigned code)
{
   settings_t     *settings         = config_get_ptr();
   struct menu_state *menu_st       = &menu_driver_state;
   struct menu_bind_state *binds    = &menu_st->input_binds;
   uint64_t input_bind_hold_us      = settings->uints.input_bind_hold    * 1000000;
   uint64_t input_bind_timeout_us   = settings->uints.input_bind_timeout * 1000000;
   uint64_t current_usec            = cpu_features_get_time_usec();

   /* Clear old mapping bit */
   input_keyboard_mapping_bits(0, binds->buffer.key);

   /* Store key in bind */
   binds->buffer.key                = (enum retro_key)code;

   /* Store new mapping bit */
   input_keyboard_mapping_bits(1, binds->buffer.key);

   /* Write out the bind */
   *(binds->output)                 = binds->buffer;

   /* Next bind */
   binds->begin++;
   binds->output++;
   binds->buffer                    =* (binds->output);

   binds->timer_hold.timeout_us     = input_bind_hold_us;
   binds->timer_hold.current        = current_usec;
   binds->timer_hold.timeout_end    = current_usec + input_bind_hold_us;

   binds->timer_timeout.timeout_us  = input_bind_timeout_us;
   binds->timer_timeout.current     = current_usec;
   binds->timer_timeout.timeout_end = current_usec + input_bind_timeout_us;

   return (binds->begin <= binds->last);
}
#endif

bool menu_input_key_bind_set_mode(
      enum menu_input_binds_ctl_state state, void *data)
{
   uint64_t current_usec;
   unsigned index_offset;
   rarch_setting_t  *setting           = (rarch_setting_t*)data;
   input_driver_state_t *input_st      = input_state_get_ptr();
   struct menu_state *menu_st          = &menu_driver_state;
   menu_handle_t       *menu           = menu_st->driver_data;
   const input_device_driver_t
      *joypad                          = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t
      *sec_joypad                      = input_st->secondary_joypad;
#else
   const input_device_driver_t
      *sec_joypad                      = NULL;
#endif
   menu_input_t *menu_input            = &menu_st->input_state;
   settings_t     *settings            = config_get_ptr();
   struct menu_bind_state *binds       = &menu_st->input_binds;
   uint64_t input_bind_hold_us         = settings->uints.input_bind_hold
      * 1000000;
   uint64_t input_bind_timeout_us      = settings->uints.input_bind_timeout
      * 1000000;

   if (!setting || !menu)
      return false;

   if (menu_input_key_bind_set_mode_common(menu_st,
            binds, state, setting, settings) == -1)
      return false;

   index_offset                        = setting->index_offset;
   binds->port                         = settings->uints.input_joypad_index[
      index_offset];

   menu_input_key_bind_poll_bind_get_rested_axes(
         joypad,
         sec_joypad,
         binds);
   menu_input_key_bind_poll_bind_state(
         input_st,
         (*input_st->libretro_input_binds),
         settings->floats.input_axis_threshold,
         settings->uints.input_joypad_index[binds->port],
         binds, false,
         (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false);

   current_usec                        = cpu_features_get_time_usec();

   binds->timer_hold   .timeout_us     = input_bind_hold_us;
   binds->timer_hold   .current        = current_usec;
   binds->timer_hold   .timeout_end    = current_usec + input_bind_hold_us;

   binds->timer_timeout.timeout_us     = input_bind_timeout_us;
   binds->timer_timeout.current        = current_usec;
   binds->timer_timeout.timeout_end    = current_usec + input_bind_timeout_us;

#ifdef USE_CUSTOM_BIND_KEYBOARD_CB
   input_st->keyboard_press_cb         = menu_input_key_bind_custom_bind_keyboard_cb;
   input_st->keyboard_press_data       = menu;
#endif

   /* While waiting for input, we have to block all hotkeys. */
   input_st->flags                    |= INP_FLAG_KB_MAPPING_BLOCKED;

   /* Wait until keys are released before starting bind timeout. */
   input_st->flags                    |= INP_FLAG_WAIT_INPUT_RELEASE;

   /* Upon triggering an input bind operation,
    * pointer input must be inhibited - otherwise
    * attempting to bind mouse buttons will cause
    * spurious menu actions */
   menu_input->select_inhibit         = true;
   menu_input->cancel_inhibit         = true;

   return true;
}

static bool menu_input_key_bind_iterate(
      settings_t *settings,
      menu_input_ctx_bind_t *bind,
      retro_time_t current_time)
{
   bool               timed_out   = false;
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = &menu_driver_state;
   struct menu_bind_state *_binds = &menu_st->input_binds;
   menu_input_t *menu_input       = &menu_st->input_state;
   uint64_t input_bind_hold_us    = settings->uints.input_bind_hold * 1000000;
   uint64_t input_bind_timeout_us = settings->uints.input_bind_timeout * 1000000;

   snprintf(bind->s, bind->len,
         "%s..\n(%s %1.1f %s)\n \n%s\n \n",
         msg_hash_to_str(MSG_INPUT_BIND_PRESS),
         msg_hash_to_str(MSG_INPUT_BIND_TIMEOUT),
         ((float)_binds->timer_timeout.timeout_us / 1000000),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS),
         input_config_bind_map_get_desc(_binds->begin - MENU_SETTINGS_BIND_BEGIN));

   /* Tick main timers */
   _binds->timer_hold   .current    = current_time;
   _binds->timer_hold   .timeout_us = _binds->timer_hold   .timeout_end - current_time;

   _binds->timer_timeout.current    = current_time;
   _binds->timer_timeout.timeout_us = _binds->timer_timeout.timeout_end - current_time;

   if (_binds->timer_timeout.timeout_us <= 0)
   {
      input_st->flags                   &= ~INP_FLAG_KB_MAPPING_BLOCKED;

#if 1
      /* Give up on first timeout */
      return true;
#else
      /* Skip to next bind */
      _binds->begin++;
      _binds->output++;

      _binds->timer_hold   .timeout_us  = input_bind_hold_us;
      _binds->timer_hold   .current     = current_time;
      _binds->timer_hold   .timeout_end = current_time + input_bind_hold_us;

      _binds->timer_timeout.timeout_us  = input_bind_timeout_us;
      _binds->timer_timeout.current     = current_time;
      _binds->timer_timeout.timeout_end = current_time + input_bind_timeout_us;

      timed_out = true;
#endif
   }

   /* binds.begin is updated in keyboard_press callback. */
   if (_binds->begin > _binds->last)
   {
      /* Avoid new binds triggering things right away. */
      /* Inhibits input for 2 frames
       * > Required, since input is ignored for 1 frame
       *   after certain events - e.g. closing the OSK */
      menu_st->input_driver_flushing_input  = 2;

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
      {
         input_st->keyboard_press_cb        = NULL;
         input_st->keyboard_press_data      = NULL;
         input_st->flags                   &= ~INP_FLAG_KB_MAPPING_BLOCKED;
      }

      return true;
   }

   {
      bool complete                         = false;
      struct menu_bind_state new_binds      = *_binds;
      unsigned bind_index                   = _binds->begin - MENU_SETTINGS_BIND_BEGIN;
      const struct retro_keybind *old_binds = &input_config_binds[new_binds.port][bind_index];
      unsigned old_key                      = old_binds->key;

      input_st->flags                      &= ~INP_FLAG_KB_MAPPING_BLOCKED;

      menu_input_key_bind_poll_bind_state(
            input_st,
            (*input_st->libretro_input_binds),
            settings->floats.input_axis_threshold,
            settings->uints.input_joypad_index[new_binds.port],
            &new_binds, timed_out,
            (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false);

      /* Wait until keys and buttons are released */
      if (input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
      {
         if (input_bind_hold_us)
         {
            if (!menu_input_key_bind_poll_find_hold(
                  settings->uints.input_max_users,
                  &new_binds, &(new_binds.buffer)))
               input_st->flags &= ~INP_FLAG_WAIT_INPUT_RELEASE;
         }
         else
         {
            if (!menu_input_key_bind_poll_find_trigger(
                  settings->uints.input_max_users,
                  _binds, &new_binds, &(new_binds.buffer)))
               input_st->flags &= ~INP_FLAG_WAIT_INPUT_RELEASE;
         }

         if (!(input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE))
         {
            /* Reset timeout */
            new_binds.timer_timeout.timeout_us  = input_bind_timeout_us;
            new_binds.timer_timeout.current     = current_time;
            new_binds.timer_timeout.timeout_end = current_time + input_bind_timeout_us;
         }
         else
            snprintf(bind->s, bind->len,
                  "%s..\n(%s %1.1f %s)\n \n--- %s ---\n \n",
                  msg_hash_to_str(MSG_INPUT_BIND_PRESS),
                  msg_hash_to_str(MSG_INPUT_BIND_TIMEOUT),
                  ((float)_binds->timer_timeout.timeout_us / 1000000),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS),
                  msg_hash_to_str(MSG_INPUT_BIND_RELEASE)
                  );
      }
      else if (input_bind_hold_us)
      {
         /* Keep resetting bind during the hold period,
          * or we'll potentially bind joystick and mouse, etc. */
         new_binds.buffer                       = *(new_binds.output);

         if (menu_input_key_bind_poll_find_hold(
               settings->uints.input_max_users,
               &new_binds, &new_binds.buffer))
         {
            char hold_label[NAME_MAX_LENGTH];

            hold_label[0]                       = '\0';

            /* Inhibit timeout */
            new_binds.timer_timeout.timeout_us  = input_bind_timeout_us;
            new_binds.timer_timeout.current     = current_time;
            new_binds.timer_timeout.timeout_end = current_time + input_bind_timeout_us;

            /* Run hold timer */
            new_binds.timer_hold.current        = current_time;
            new_binds.timer_hold.timeout_us     = new_binds.timer_hold.timeout_end - current_time;

            input_config_get_bind_string(settings, hold_label,
                  &new_binds.buffer, NULL, sizeof(hold_label));

            snprintf(bind->s, bind->len,
                  "%s..\n(%s %1.1f %s)\n \n%s\n   %s",
                  msg_hash_to_str(MSG_INPUT_BIND_PRESS),
                  msg_hash_to_str(MSG_INPUT_BIND_HOLD),
                  ((float)new_binds.timer_hold.timeout_us / 1000000),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS),
                  input_config_bind_map_get_desc(_binds->begin - MENU_SETTINGS_BIND_BEGIN),
                  hold_label);

            /* Hold complete? */
            if (new_binds.timer_hold.timeout_us <= 0)
               complete = true;
         }
         else
         {
            /* Reset hold countdown */
            new_binds.timer_hold.timeout_us     = input_bind_hold_us;
            new_binds.timer_hold.current        = current_time;
            new_binds.timer_hold.timeout_end    = current_time + input_bind_hold_us;
         }
      }
      else if ((new_binds.skip && !_binds->skip)
            || menu_input_key_bind_poll_find_trigger(
               settings->uints.input_max_users,
               _binds, &new_binds, &(new_binds.buffer)))
         complete = true;

      if (complete)
      {
         /* Always stop binding when not binding all */
         bool stop_binding                   = new_binds.order == 0 && new_binds.begin == new_binds.last;

         /* Update bind */
         *(new_binds.output)                 = new_binds.buffer;

         /* Update keyboard mapping bits */
         if (new_binds.buffer.key)
         {
            input_keyboard_mapping_bits(0, old_key);
            input_keyboard_mapping_bits(1, new_binds.buffer.key);
         }

         /* Avoid new binds triggering things right away. */
         /* Inhibits input for 2 frames
          * > Required, since input is ignored for 1 frame
          *   after certain events - e.g. closing the OSK */
         menu_st->input_driver_flushing_input = 2;

         /* Use human readable order instead */
         new_binds.order++;
         new_binds.begin = MENU_SETTINGS_BIND_BEGIN + input_config_bind_order[new_binds.order];

         if (     new_binds.order > ARRAY_SIZE(input_config_bind_order) - 1
               || stop_binding)
         {
            input_st->keyboard_press_cb      = NULL;
            input_st->keyboard_press_data    = NULL;
            input_st->flags                 &= ~INP_FLAG_KB_MAPPING_BLOCKED;
            return true;
         }

         input_st->flags                    &= ~INP_FLAG_KB_MAPPING_BLOCKED;
         input_st->flags                    |= INP_FLAG_WAIT_INPUT_RELEASE;

         /* Next bind */
         new_binds.output                    =
                 &input_config_binds[new_binds.port][0]
               + input_config_bind_order[new_binds.order];
         new_binds.buffer = *(new_binds.output);
         new_binds.timer_hold   .timeout_us  = input_bind_hold_us;
         new_binds.timer_hold   .current     = current_time;
         new_binds.timer_hold   .timeout_end = current_time + input_bind_hold_us;
         new_binds.timer_timeout.timeout_us  = input_bind_timeout_us;
         new_binds.timer_timeout.current     = current_time;
         new_binds.timer_timeout.timeout_end = current_time + input_bind_timeout_us;
      }

      *(_binds) = new_binds;
   }

   /* Pointer input must be inhibited on each
    * frame that the bind operation is active -
    * otherwise attempting to bind mouse buttons
    * will cause spurious menu actions */
   menu_input->select_inhibit     = true;
   menu_input->cancel_inhibit     = true;

   /* Menu screensaver should be inhibited on each
    * frame that the bind operation is active */
   menu_st->input_last_time_us    = menu_st->current_time_us;

   return false;
}

bool menu_input_dialog_get_display_kb(void)
{
   struct menu_state *menu_st     = &menu_driver_state;
#ifdef HAVE_LIBNX
   input_driver_state_t *input_st = input_state_get_ptr();
   SwkbdConfig kbd;
   Result rc;
   /* Indicates that we are "typing" from the swkbd
    * result to RetroArch with repeated calls to input_keyboard_event
    * This prevents input_keyboard_event from calling back
    * menu_input_dialog_get_display_kb, looping indefinintely */
   static bool typing             = false;

   if (typing)
      return false;

   /* swkbd only works on "real" titles */
   if (     __nx_applet_type != AppletType_Application
         && __nx_applet_type != AppletType_SystemApplication)
      return ((menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY) > 0);

   if (!(menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY))
      return false;

   rc = swkbdCreate(&kbd, 0);

   if (R_SUCCEEDED(rc))
   {
      unsigned i;
      char buf[LIBNX_SWKBD_LIMIT] = {'\0'};
      swkbdConfigMakePresetDefault(&kbd);

      swkbdConfigSetGuideText(&kbd,
            menu_st->input_dialog_kb_label);

      rc = swkbdShow(&kbd, buf, sizeof(buf));

      swkbdClose(&kbd);

      /* RetroArch uses key-by-key input
         so we need to simulate it */
      typing = true;
      for (i = 0; i < LIBNX_SWKBD_LIMIT; i++)
      {
         /* In case a previous "Enter" press closed the keyboard */
         if (!(menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY))
            break;

         if (buf[i] == '\n' || buf[i] == '\0')
            input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
         else
         {
            const char *word = &buf[i];
            /* input_keyboard_line_append expects a null-terminated
               string, so just make one (yes, the touch keyboard is
               a list of "NULL-terminated characters") */
            char oldchar     = buf[i+1];
            buf[i+1]         = '\0';

            input_keyboard_line_append(&input_st->keyboard_line,
                  word, strlen(word));

            osk_update_last_codepoint(
                  &input_st->osk_last_codepoint,
                  &input_st->osk_last_codepoint_len,
                  word);
            buf[i+1]     = oldchar;
         }
      }

      /* fail-safe */
      if (menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY)
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      typing = false;
      libnx_apply_overclock();
      return false;
   }
   libnx_apply_overclock();
#endif /* HAVE_LIBNX */
   return ((menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY) > 0);
}

unsigned menu_event(
      settings_t *settings,
      input_bits_t *p_input,
      input_bits_t *p_trigger_input,
      bool display_kb)
{
   int i;
   /* Used for key repeat */
   static retro_time_t last_time_us                = 0;
   static float delay_timer                        = 0.0f;
   static float delay_count                        = 0.0f;
   static bool hold_initial                        = true;
   static bool hold_reset                          = true;
   static unsigned ok_old                          = 0;
   unsigned ret                                    = MENU_ACTION_NOOP;
   bool set_scroll                                 = false;
   size_t new_scroll_accel                         = 0;
   struct menu_state *menu_st                      = &menu_driver_state;
   menu_input_t *menu_input                        = &menu_st->input_state;
   input_driver_state_t *input_st                  = input_state_get_ptr();
   input_driver_t *current_input                   = input_st->current_driver;
   const input_device_driver_t *joypad             = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad         = input_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad         = NULL;
#endif
   gfx_display_t *p_disp                           = disp_get_ptr();
   menu_input_pointer_hw_state_t *pointer_hw_state = &menu_st->input_pointer_hw_state;
   menu_handle_t *menu                             = menu_st->driver_data;
   bool keyboard_mapping_blocked                   = (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false;
   bool menu_mouse_enable                          = settings->bools.menu_mouse_enable;
   bool menu_pointer_enable                        = settings->bools.menu_pointer_enable;
   bool swap_ok_cancel_btns                        = settings->bools.input_menu_swap_ok_cancel_buttons;
   bool swap_scroll_btns                           = settings->bools.input_menu_swap_scroll_buttons;
   bool menu_scroll_fast                           = settings->bools.menu_scroll_fast;
   bool pointer_enabled                            = settings->bools.menu_pointer_enable;
   unsigned input_touch_scale                      = settings->uints.input_touch_scale;
   unsigned menu_scroll_delay                      = settings->uints.menu_scroll_delay;
#ifdef HAVE_OVERLAY
   bool input_overlay_enable                       = settings->bools.input_overlay_enable;
   bool overlay_active                             = input_overlay_enable
         && (input_st->overlay_ptr)
         && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE);
#else
   bool input_overlay_enable                       = false;
   bool overlay_active                             = false;
#endif
   unsigned menu_ok_btn                            = swap_ok_cancel_btns ?
         RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A;
   unsigned menu_cancel_btn                        = swap_ok_cancel_btns ?
         RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;
   unsigned ok_current                             = BIT256_GET_PTR(p_input, menu_ok_btn);
   unsigned ok_trigger                             = ok_current & ~ok_old;
   static unsigned navigation_initial              = 0;
   unsigned navigation_current                     = 0;
   unsigned navigation_buttons[8]                  =
   {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2
   };

   ok_old                                          = ok_current;

   /* Get pointer (mouse + touchscreen) input
    * Note: Must be done regardless of menu screensaver
    *       state */

   /* > If pointer input is disabled, do nothing */
   if (!menu_mouse_enable && !menu_pointer_enable)
      menu_input->pointer.type = MENU_POINTER_DISABLED;
   else
   {
      menu_input_pointer_hw_state_t mouse_hw_state       = {0};
      menu_input_pointer_hw_state_t touchscreen_hw_state = {0};

      /* Read mouse */
      if (menu_mouse_enable)
         menu_input_get_mouse_hw_state(
               p_disp,
               menu,
               input_st,
               current_input,
               joypad,
               sec_joypad,
               keyboard_mapping_blocked,
               menu_mouse_enable,
               input_overlay_enable,
               overlay_active,
               &mouse_hw_state);

      /* Read touchscreen
       * Note: Could forgo this if mouse is currently active,
       * but this is 'cleaner' code... (if performance is a
       * concern - and it isn't - user can just disable touch
       * screen support) */
      if (menu_pointer_enable)
         menu_input_get_touchscreen_hw_state(
               p_disp,
               menu,
               input_st,
               current_input,
               joypad,
               sec_joypad,
               keyboard_mapping_blocked,
               overlay_active,
               pointer_enabled,
               input_touch_scale,
               &touchscreen_hw_state);

      /* Mouse takes precedence */
      if (mouse_hw_state.flags & MENU_INP_PTR_FLG_ACTIVE)
         menu_input->pointer.type = MENU_POINTER_MOUSE;
      else if (touchscreen_hw_state.flags & MENU_INP_PTR_FLG_ACTIVE)
         menu_input->pointer.type = MENU_POINTER_TOUCHSCREEN;

      /* Copy input from the current device */
      if (menu_input->pointer.type == MENU_POINTER_MOUSE)
         memcpy(pointer_hw_state, &mouse_hw_state, sizeof(menu_input_pointer_hw_state_t));
      else if (menu_input->pointer.type == MENU_POINTER_TOUCHSCREEN)
         memcpy(pointer_hw_state, &touchscreen_hw_state, sizeof(menu_input_pointer_hw_state_t));

      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_ACTIVE)
         menu_st->input_last_time_us = menu_st->current_time_us;
   }

   /* Populate menu_input_state
    * Note: dx, dy, ptr, y_accel, etc. entries are set elsewhere */
   menu_input->pointer.x          = pointer_hw_state->x;
   menu_input->pointer.y          = pointer_hw_state->y;
   if (menu_input->select_inhibit || menu_input->cancel_inhibit)
      menu_input->pointer.flags &= ~(MENU_INP_PTR_FLG_ACTIVE
                                   | MENU_INP_PTR_FLG_PRESS_SELECT);
   else
   {
      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_ACTIVE)
         menu_input->pointer.flags |=  (MENU_INP_PTR_FLG_ACTIVE);
      else
         menu_input->pointer.flags &= ~(MENU_INP_PTR_FLG_ACTIVE);

      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT)
         menu_input->pointer.flags |=  (MENU_INP_PTR_FLG_PRESSED);
      else
         menu_input->pointer.flags &= ~(MENU_INP_PTR_FLG_PRESSED);
   }

   /* If menu screensaver is active, any input
    * is intercepted and used to switch it off */
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      /* Check pointer input */
      bool input_active = ( (menu_input->pointer.type != MENU_POINTER_DISABLED)
                       &&  ((menu_input->pointer.flags & MENU_INP_PTR_FLG_ACTIVE) > 0));

      /* Check regular input */
      if (!input_active)
         input_active = bits_any_set(p_input->data, ARRAY_SIZE(p_input->data));

      if (!input_active)
         input_active = bits_any_set(p_trigger_input->data, ARRAY_SIZE(p_trigger_input->data));

      /* Disable screensaver if required */
      if (input_active)
      {
         menu_st->flags             &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
                     NULL, menu_st->userdata);
      }

      /* Annul received input */
      menu_input->pointer.flags      &= ~(MENU_INP_PTR_FLG_ACTIVE
                                        | MENU_INP_PTR_FLG_PRESSED);
      pointer_hw_state->flags        &= ~(MENU_INP_PTR_FLG_PRESS_UP
                                        | MENU_INP_PTR_FLG_PRESS_DOWN
                                        | MENU_INP_PTR_FLG_PRESS_LEFT
                                        | MENU_INP_PTR_FLG_PRESS_RIGHT);
      menu_input->select_inhibit      = true;
      menu_input->cancel_inhibit      = true;
      return MENU_ACTION_NOOP;
   }

   /* Accelerate only navigation buttons */
   for (i = 0; i < 6; i++)
   {
      if (BIT256_GET_PTR(p_input, navigation_buttons[i]))
         navigation_current        |= (1 << navigation_buttons[i]);
   }

   if (navigation_current)
   {
      float delta_time              = (float)(menu_st->current_time_us - last_time_us) / 1000;

      last_time_us                  = menu_st->current_time_us;

      /* Store first direction in order to block "diagonals" */
      if (!navigation_initial)
         navigation_initial         = navigation_current;

      if (hold_reset)
      {
         /* Don't run anything first frame */
         hold_reset                 = false;
         delay_timer                = (hold_initial) ? menu_scroll_delay : 33.33f;
         delay_count                = 0;
      }
      else
      {
         hold_initial               = false;
         delay_count               += delta_time;
      }

      if (delay_count >= delay_timer)
      {
         uint32_t input_repeat      = 0;
         for (i = 0; i < 6; i++)
            BIT32_SET(input_repeat, navigation_buttons[i]);

         p_trigger_input->data[0]  |= p_input->data[0] & input_repeat;
         set_scroll                 = true;
         hold_reset                 = true;
         new_scroll_accel           = MIN(menu_st->scroll.acceleration + 1, (menu_scroll_fast) ? 25 : 5);
      }
   }
   else
   {
      set_scroll                    = true;
      hold_reset                    = true;
      hold_initial                  = true;
      navigation_initial            = 0;
   }

   if (set_scroll)
      menu_st->scroll.acceleration  = (unsigned)(new_scroll_accel);

   if (display_kb)
   {
#ifdef HAVE_MIST
      /* Do not process input events if the Steam OSK is open */
      if (!steam_has_osk_open())
      {
#endif
      bool show_osk_symbols = input_event_osk_show_symbol_pages(menu_st->driver_data);

      input_event_osk_iterate(input_st->osk_grid, input_st->osk_idx);

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_ptr < 33)
            input_st->osk_ptr += OSK_CHARS_PER_LINE;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_ptr >= OSK_CHARS_PER_LINE)
            input_st->osk_ptr -= OSK_CHARS_PER_LINE;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_ptr < 43)
            input_st->osk_ptr += 1;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_ptr >= 1)
            input_st->osk_ptr -= 1;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_idx > OSK_TYPE_UNKNOWN + 1)
            input_st->osk_idx = ((enum osk_type)
                  (input_st->osk_idx - 1));
         else
            input_st->osk_idx = ((enum osk_type)(show_osk_symbols
                     ? OSK_TYPE_LAST - 1
                     : OSK_SYMBOLS_PAGE1));
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
      {
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (input_st->osk_idx < (show_osk_symbols
                  ? OSK_TYPE_LAST - 1
                  : OSK_SYMBOLS_PAGE1))
            input_st->osk_idx = ((enum osk_type)(
                     input_st->osk_idx + 1));
         else
            input_st->osk_idx = ((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
      }

      if (BIT256_GET_PTR(p_trigger_input, menu_ok_btn))
      {
         if (input_st->osk_ptr >= 0)
            input_event_osk_append(
                  &input_st->keyboard_line,
                  &input_st->osk_idx,
                  &input_st->osk_last_codepoint,
                  &input_st->osk_last_codepoint_len,
                  input_st->osk_ptr,
                  show_osk_symbols,
                  input_st->osk_grid[input_st->osk_ptr],
                  strlen(input_st->osk_grid[input_st->osk_ptr]));
      }

      /* Cancel: Send backspace if buffer is not empty, otherwise close window */
      if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
      {
         if (input_st->keyboard_line.size)
            input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
         else
            input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
      }

      /* Select: Clear and close the keyboard input window */
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         input_keyboard_line_clear(input_st);
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
      }

      /* Scan: Clear the keyboard input window */
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_Y))
         input_keyboard_line_clear(input_st);

      /* Start + Search: Send return key to close keyboard input window */
      if (     BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START)
            || BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_X))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

#ifdef HAVE_MIST
      }
#endif

      BIT256_CLEAR_ALL_PTR(p_trigger_input);
   }
   else
   {
      static uint8_t switch_old = 0;
      uint8_t switch_current    = BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_LEFT)
                                | BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_RIGHT);
      uint8_t switch_trigger    = switch_current & ~switch_old;

      switch_old                = switch_current;

      /* Prevent holding down left/right with boolean settings */
      if (switch_current)
      {
         uint8_t setting_type   = 0;

         get_current_menu_type(menu_st, &setting_type);

         if (setting_type == ST_BOOL)
         {
            char value[8];

            get_current_menu_value(menu_st, value, sizeof(value));

            /* Ignore direction if switch is already in that position */
            if (     (  string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))
                     && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
                  || (  string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))
                     && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
               )
               switch_trigger   = 0;
         }
         else
            /* Always allow repeat direction */
            switch_trigger      = 1;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (navigation_initial == (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            ret = MENU_ACTION_UP;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (navigation_initial == (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            ret = MENU_ACTION_DOWN;
      }
      if (     BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT)
            && switch_trigger)
      {
         if (navigation_initial == (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            ret = MENU_ACTION_LEFT;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT)
            && switch_trigger)
      {
         if (navigation_initial == (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            ret = MENU_ACTION_RIGHT;
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
      {
         menu_st->scroll.mode = (swap_scroll_btns) ? MENU_SCROLL_START_LETTER : MENU_SCROLL_PAGE;
         ret = MENU_ACTION_SCROLL_UP;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
      {
         menu_st->scroll.mode = (swap_scroll_btns) ? MENU_SCROLL_START_LETTER : MENU_SCROLL_PAGE;
         ret = MENU_ACTION_SCROLL_DOWN;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L2))
      {
         menu_st->scroll.mode = (swap_scroll_btns) ? MENU_SCROLL_PAGE : MENU_SCROLL_START_LETTER;
         ret = MENU_ACTION_SCROLL_UP;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R2))
      {
         menu_st->scroll.mode = (swap_scroll_btns) ? MENU_SCROLL_PAGE : MENU_SCROLL_START_LETTER;
         ret = MENU_ACTION_SCROLL_DOWN;
      }
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L3))
         ret = MENU_ACTION_SCROLL_HOME;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R3))
         ret = MENU_ACTION_SCROLL_END;
      else if (ok_trigger)
         ret = MENU_ACTION_OK;
      else if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
         ret = MENU_ACTION_CANCEL;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_X))
      {
         if (!settings->bools.menu_disable_search_button)
            ret = MENU_ACTION_SEARCH;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_Y))
         ret = MENU_ACTION_SCAN;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
         ret = MENU_ACTION_START;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
         if (!settings->bools.menu_disable_info_button)
            ret = MENU_ACTION_INFO;
      }
      else if (BIT256_GET_PTR(p_trigger_input, RARCH_MENU_TOGGLE))
         ret = MENU_ACTION_TOGGLE;

      if (ret != MENU_ACTION_NOOP)
         menu_st->input_last_time_us = menu_st->current_time_us;
   }

   /* Menu must be alive, and input must be released after menu toggle. */
   if (     !(menu_st->flags & MENU_ST_FLAG_ALIVE)
         || menu_st->input_driver_flushing_input > 0)
      return MENU_ACTION_NOOP;

   return ret;
}

static int menu_input_post_iterate(
      gfx_display_t *p_disp,
      struct menu_state *menu_st,
      unsigned action,
      retro_time_t current_time)
{
   menu_entry_t entry;
   static retro_time_t start_time                  = 0;
   static int16_t start_x                          = 0;
   static int16_t start_y                          = 0;
   static int16_t last_x                           = 0;
   static int16_t last_y                           = 0;
   static uint16_t dx_start_right_max              = 0;
   static uint16_t dx_start_left_max               = 0;
   static uint16_t dy_start_up_max                 = 0;
   static uint16_t dy_start_down_max               = 0;
   static bool last_select_pressed                 = false;
   static bool last_cancel_pressed                 = false;
   static bool last_left_pressed                   = false;
   static bool last_right_pressed                  = false;
   static retro_time_t last_left_action_time       = 0;
   static retro_time_t last_right_action_time      = 0;
   static retro_time_t last_press_direction_time   = 0;
   bool attenuate_y_accel                          = true;
   bool osk_active                                 = menu_input_dialog_get_display_kb();
   bool messagebox_active                          = false;
   int ret                                         = 0;
   menu_input_pointer_hw_state_t *pointer_hw_state = &menu_st->input_pointer_hw_state;
   menu_input_t *menu_input                        = &menu_st->input_state;
   menu_handle_t *menu                             = menu_st->driver_data;
   video_driver_state_t *video_st                  = video_state_get_ptr();
   input_driver_state_t *input_st                  = input_state_get_ptr();
   menu_list_t *menu_list                          = menu_st->entries.list;
   file_list_t *selection_buf                      = menu_list ? MENU_LIST_GET_SELECTION(menu_list, (unsigned)0) : NULL;
   size_t selection                                = menu_st->selection_ptr;
   menu_file_list_cbs_t *cbs                       = selection_buf
      ? (menu_file_list_cbs_t*)selection_buf->list[selection].actiondata
      : NULL;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                | MENU_ENTRY_FLAG_LABEL_ENABLED;
   menu_entry_get(&entry, 0, selection, NULL, false);

   /* Check whether a message box is currently
    * being shown
    * > Note: This ignores input bind dialogs,
    *   since input binding overrides normal input
    *   and must be handled separately... */
   if (menu)
      messagebox_active = BIT64_GET(
            menu->state, MENU_STATE_RENDER_MESSAGEBOX)
            && !string_is_empty(menu->menu_state_msg);

   /* If onscreen keyboard is shown and we currently have
    * active mouse input, highlight key under mouse cursor */
   if (    osk_active
       && (menu_input->pointer.type == MENU_POINTER_MOUSE)
       && ((pointer_hw_state->flags & MENU_INP_PTR_FLG_ACTIVE) > 0))
   {
      menu_ctx_pointer_t point;

      point.x       = pointer_hw_state->x;
      point.y       = pointer_hw_state->y;
      point.ptr     = 0;
      point.cbs     = NULL;
      point.entry   = NULL;
      point.action  = 0;
      point.gesture = MENU_INPUT_GESTURE_NONE;
      point.retcode = 0;

      menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
      if (point.retcode > -1)
         input_st->osk_ptr = point.retcode;
   }

   /* Select + X/Y position */
   if (!menu_input->select_inhibit)
   {
      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT)
      {
         int16_t x           = pointer_hw_state->x;
         int16_t y           = pointer_hw_state->y;
         static float accel0 = 0.0f;
         static float accel1 = 0.0f;

         /* Transition from select unpressed to select pressed */
         if (!last_select_pressed)
         {
            menu_ctx_pointer_t point;

            /* Initialise variables */
            start_time                = current_time;
            start_x                   = x;
            start_y                   = y;
            last_x                    = x;
            last_y                    = y;
            dx_start_right_max        = 0;
            dx_start_left_max         = 0;
            dy_start_up_max           = 0;
            dy_start_down_max         = 0;
            accel0                    = 0.0f;
            accel1                    = 0.0f;
            last_press_direction_time = 0;

            /* If we are not currently showing the onscreen
             * keyboard or a message box, trigger a 'pointer
             * down' event */
            if (!osk_active && !messagebox_active)
            {
               point.x       = x;
               point.y       = y;
               /* Note: menu_input->ptr is meaningless here when
                * using a touchscreen... */
               point.ptr     = menu_input->ptr;
               point.cbs     = cbs;
               point.entry   = &entry;
               point.action  = action;
               point.gesture = MENU_INPUT_GESTURE_NONE;

               menu_driver_ctl(RARCH_MENU_CTL_POINTER_DOWN, &point);
               ret = point.retcode;
            }
         }
         else
         {
            /* Pointer is being held down
             * (i.e. for more than one frame) */
            float dpi = menu ? menu_input_get_dpi(menu, p_disp,
                  video_st->width, video_st->height) : 0.0f;

            /* > Update deltas + acceleration & detect press direction
             *   Note: We only do this if the pointer has moved above
             *   a certain threshold - this requires dpi info */
            if (dpi > 0.0f)
            {
               uint16_t dpi_threshold_drag =
                     (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_DRAG) + 0.5f);

               int16_t dx_start            = x - start_x;
               int16_t dy_start            = y - start_y;
               uint16_t dx_start_abs       = dx_start < 0 ? dx_start * -1 : dx_start;
               uint16_t dy_start_abs       = dy_start < 0 ? dy_start * -1 : dy_start;

               if (   (dx_start_abs > dpi_threshold_drag)
                   || (dy_start_abs > dpi_threshold_drag))
               {
                  uint16_t dpi_threshold_press_direction_min     =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MIN) + 0.5f);
                  uint16_t dpi_threshold_press_direction_max     =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MAX) + 0.5f);
                  uint16_t dpi_threshold_press_direction_tangent =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_TANGENT) + 0.5f);

                  enum menu_input_pointer_press_direction
                        press_direction                  = MENU_INPUT_PRESS_DIRECTION_NONE;
                  float press_direction_amplitude        = 0.0f;
                  retro_time_t press_direction_delay     = MENU_INPUT_PRESS_DIRECTION_DELAY_MAX;

                  /* Pointer has moved a sufficient distance to
                   * trigger a 'dragged' state */
                  menu_input->pointer.flags             |= MENU_INP_PTR_FLG_DRAGGED;

                  /* Here we diverge:
                   * > If onscreen keyboard or a message box is
                   *   active, pointer deltas, acceleration and
                   *   press direction must be inhibited
                   * > If not, input is processed normally */
                  if (osk_active || messagebox_active)
                  {
                     /* Inhibit normal pointer input */
                     menu_input->pointer.dx              = 0;
                     menu_input->pointer.dy              = 0;
                     menu_input->pointer.y_accel         = 0.0f;
                     menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
                     accel0                              = 0.0f;
                     accel1                              = 0.0f;
                     attenuate_y_accel                   = false;
                  }
                  else
                  {
                     /* Assign current deltas */
                     menu_input->pointer.dx              = x - last_x;
                     menu_input->pointer.dy              = y - last_y;

                     /* Update maximum start->current deltas */
                     if (dx_start > 0)
                        dx_start_right_max = (dx_start_abs > dx_start_right_max)
                              ? dx_start_abs : dx_start_right_max;
                     else
                        dx_start_left_max = (dx_start_abs > dx_start_left_max)
                              ? dx_start_abs : dx_start_left_max;

                     if (dy_start > 0)
                        dy_start_down_max = (dy_start_abs > dy_start_down_max)
                              ? dy_start_abs : dy_start_down_max;
                     else
                        dy_start_up_max = (dy_start_abs > dy_start_up_max)
                              ? dy_start_abs : dy_start_up_max;

                     /* Magic numbers... */
                     menu_input->pointer.y_accel = (accel0 + accel1 + (float)menu_input->pointer.dy) / 3.0f;
                     accel0                      = accel1;
                     accel1                      = menu_input->pointer.y_accel;

                     /* Acceleration decays over time - but if the value
                      * has been set on this frame, attenuation should
                      * be skipped */
                     attenuate_y_accel           = false;

                     /* Check if pointer is being held in a particular
                      * direction */
                     menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;

                     /* > Press directions are actually triggered as a pulse train,
                      *   since a continuous direction prevents fine control in the
                      *   context of menu actions (i.e. it would be the same
                      *   as always holding down a cursor key all the time - too fast
                      *   to control). We therefore apply a low pass filter, with
                      *   a variable frequency based upon the distance the user has
                      *   dragged the pointer */

                     /* > Horizontal */
                     if ((dx_start_abs >= dpi_threshold_press_direction_min) &&
                         (dy_start_abs <  dpi_threshold_press_direction_tangent))
                     {
                        press_direction = (dx_start > 0)
                              ? MENU_INPUT_PRESS_DIRECTION_RIGHT
                              : MENU_INPUT_PRESS_DIRECTION_LEFT;

                        /* Get effective amplitude of press direction offset */
                        press_direction_amplitude =
                              (float)(dx_start_abs - dpi_threshold_press_direction_min) /
                              (float)(dpi_threshold_press_direction_max - dpi_threshold_press_direction_min);
                     }
                     /* > Vertical */
                     else if (   (dy_start_abs >= dpi_threshold_press_direction_min)
                              && (dx_start_abs <  dpi_threshold_press_direction_tangent))
                     {
                        press_direction = (dy_start > 0)
                              ? MENU_INPUT_PRESS_DIRECTION_DOWN
                              : MENU_INPUT_PRESS_DIRECTION_UP;

                        /* Get effective amplitude of press direction offset */
                        press_direction_amplitude =
                              (float)(dy_start_abs - dpi_threshold_press_direction_min) /
                              (float)(dpi_threshold_press_direction_max - dpi_threshold_press_direction_min);
                     }

                     if (press_direction != MENU_INPUT_PRESS_DIRECTION_NONE)
                     {
                        /* > Update low pass filter frequency */
                        if (press_direction_amplitude > 1.0f)
                           press_direction_delay = MENU_INPUT_PRESS_DIRECTION_DELAY_MIN;
                        else
                           press_direction_delay = MENU_INPUT_PRESS_DIRECTION_DELAY_MIN +
                                 ((MENU_INPUT_PRESS_DIRECTION_DELAY_MAX - MENU_INPUT_PRESS_DIRECTION_DELAY_MIN)*
                                  (1.0f - press_direction_amplitude));

                        /* > Apply low pass filter */
                        if (current_time - last_press_direction_time > press_direction_delay)
                        {
                           menu_input->pointer.press_direction = press_direction;
                           last_press_direction_time = current_time;
                        }
                     }
                  }
               }
               else
               {
                  /* Pointer is stationary */
                  menu_input->pointer.dx              = 0;
                  menu_input->pointer.dy              = 0;
                  menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;

                  /* Standard behaviour (on Android, at least) is to stop
                   * scrolling when the user touches the screen. We should
                   * therefore 'reset' y acceleration whenever the pointer
                   * is stationary - with two caveats:
                   * - We only disable scrolling if the pointer *remains*
                   *   stationary. If the pointer is held down then
                   *   subsequently moves, normal scrolling should resume
                   * - Halting the scroll immediately produces a very
                   *   unpleasant 'jerky' user experience. To avoid this,
                   *   we add a small delay between detecting a pointer
                   *   down event and forcing y acceleration to zero
                   * NOTE: Of course, we must also 'reset' y acceleration
                   * whenever the onscreen keyboard or a message box is
                   * shown */
                  if (  (!(menu_input->pointer.flags & MENU_INP_PTR_FLG_DRAGGED)
                      && (menu_input->pointer.press_duration > MENU_INPUT_Y_ACCEL_RESET_DELAY))
                      || (osk_active || messagebox_active))
                  {
                     menu_input->pointer.y_accel = 0.0f;
                     accel0                      = 0.0f;
                     accel1                      = 0.0f;
                     attenuate_y_accel           = false;
                  }
               }
            }
            else
            {
               /* No dpi info - just fallback to zero... */
               menu_input->pointer.dx              = 0;
               menu_input->pointer.dy              = 0;
               menu_input->pointer.y_accel         = 0.0f;
               menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
               accel0                              = 0.0f;
               accel1                              = 0.0f;
               attenuate_y_accel                   = false;
            }

            /* > Update remaining variables */
            menu_input->pointer.press_duration = current_time - start_time;
            last_x                             = x;
            last_y                             = y;
         }
      }
      else if (last_select_pressed)
      {
         /* Transition from select pressed to select unpressed */
         int16_t x;
         int16_t y;
         menu_ctx_pointer_t point;

         if (menu_input->pointer.flags & MENU_INP_PTR_FLG_DRAGGED)
         {
            /* Pointer has moved.
             * When using a touchscreen, releasing a press
             * resets the x/y position - so cannot use
             * current hardware x/y values. Instead, use
             * previous position from last time that a
             * press was active */
            x          = last_x;
            y          = last_y;
         }
         else
         {
            /* Pointer is considered stationary,
             * so use start position */
            x          = start_x;
            y          = start_y;
         }

         point.x       = x;
         point.y       = y;
         point.ptr     = menu_input->ptr;
         point.cbs     = cbs;
         point.entry   = &entry;
         point.action  = action;
         point.gesture = MENU_INPUT_GESTURE_NONE;

         /* On screen keyboard overrides normal menu input... */
         if (osk_active)
         {
#ifdef HAVE_MIST
         /* Disable OSK pointer input if the Steam OSK is used */
         if (!steam_has_osk_open())
         {
#endif
            /* If pointer has been 'dragged', then it counts as
             * a miss. Only register 'release' event if pointer
             * has remained stationary */
            if (!(menu_input->pointer.flags & MENU_INP_PTR_FLG_DRAGGED))
            {
               menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
               if (point.retcode > -1)
               {
                  bool show_osk_symbols = input_event_osk_show_symbol_pages(menu_st->driver_data);
                  input_st->osk_ptr     = point.retcode;
                  input_event_osk_append(
                        &input_st->keyboard_line,
                        &input_st->osk_idx,
                        &input_st->osk_last_codepoint,
                        &input_st->osk_last_codepoint_len,
                        point.retcode,
                        show_osk_symbols,
                        input_st->osk_grid[input_st->osk_ptr],
                        strlen(input_st->osk_grid[input_st->osk_ptr]));
               }
            }
#ifdef HAVE_MIST
            }
#endif
         }
         /* Message boxes override normal menu input...
          * > If a message box is shown, any kind of pointer
          *   gesture should close it */
         else if (messagebox_active)
            menu_input_pointer_close_messagebox(
                  menu_st);
         /* Normal menu input */
         else
         {
            /* Detect gesture type */
            if (!(menu_input->pointer.flags & MENU_INP_PTR_FLG_DRAGGED))
            {
               /* Pointer hasn't moved - check press duration */
               if (menu_input->pointer.press_duration
                     < MENU_INPUT_PRESS_TIME_SHORT)
                  point.gesture = MENU_INPUT_GESTURE_TAP;
               else if (menu_input->pointer.press_duration
                     < MENU_INPUT_PRESS_TIME_LONG)
                  point.gesture = MENU_INPUT_GESTURE_SHORT_PRESS;
               else
                  point.gesture = MENU_INPUT_GESTURE_LONG_PRESS;
            }
            else
            {
               /* Pointer has moved - check if this is a swipe */
               float dpi = menu ? menu_input_get_dpi(menu, p_disp,
                     video_st->width, video_st->height) : 0.0f;

               if (     (dpi > 0.0f)
                     && (menu_input->pointer.press_duration <
                      MENU_INPUT_SWIPE_TIMEOUT))
               {
                  uint16_t dpi_threshold_swipe         =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_SWIPE) + 0.5f);
                  uint16_t dpi_threshold_swipe_tangent =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_SWIPE_TANGENT) + 0.5f);

                  int16_t dx_start                     = x - start_x;
                  int16_t dy_start                     = y - start_y;
                  uint16_t dx_start_right_final        = 0;
                  uint16_t dx_start_left_final         = 0;
                  uint16_t dy_start_up_final           = 0;
                  uint16_t dy_start_down_final         = 0;

                  /* Get final deltas */
                  if (dx_start > 0)
                     dx_start_right_final              = (uint16_t)dx_start;
                  else
                     dx_start_left_final               = (uint16_t)(dx_start * -1);

                  if (dy_start > 0)
                     dy_start_down_final               = (uint16_t)dy_start;
                  else
                     dy_start_up_final                 = (uint16_t)(dy_start * -1);

                  /* Swipe right */
                  if (     (dx_start_right_final > dpi_threshold_swipe)
                        && (dx_start_left_max    < dpi_threshold_swipe_tangent)
                        && (dy_start_up_max      < dpi_threshold_swipe_tangent)
                        && (dy_start_down_max    < dpi_threshold_swipe_tangent)
                     )
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_RIGHT;
                  /* Swipe left */
                  else if (
                           (dx_start_right_max  < dpi_threshold_swipe_tangent)
                        && (dx_start_left_final > dpi_threshold_swipe)
                        && (dy_start_up_max     < dpi_threshold_swipe_tangent)
                        && (dy_start_down_max   < dpi_threshold_swipe_tangent)
                        )
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_LEFT;
                  /* Swipe up */
                  else if (
                           (dx_start_right_max < dpi_threshold_swipe_tangent)
                        && (dx_start_left_max  < dpi_threshold_swipe_tangent)
                        && (dy_start_up_final  > dpi_threshold_swipe)
                        && (dy_start_down_max  < dpi_threshold_swipe_tangent)
                        )
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_UP;
                  /* Swipe down */
                  else if (
                           (dx_start_right_max  < dpi_threshold_swipe_tangent)
                        && (dx_start_left_max   < dpi_threshold_swipe_tangent)
                        && (dy_start_up_max     < dpi_threshold_swipe_tangent)
                        && (dy_start_down_final > dpi_threshold_swipe)
                        )
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_DOWN;
               }
            }

            /* Trigger a 'pointer up' event */
            menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
            ret = point.retcode;
         }

         /* Reset variables */
         start_x                             = 0;
         start_y                             = 0;
         last_x                              = 0;
         last_y                              = 0;
         dx_start_right_max                  = 0;
         dx_start_left_max                   = 0;
         dy_start_up_max                     = 0;
         dy_start_down_max                   = 0;
         last_press_direction_time           = 0;
         menu_input->pointer.press_duration  = 0;
         menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
         menu_input->pointer.dx              = 0;
         menu_input->pointer.dy              = 0;
         menu_input->pointer.flags          &= ~(MENU_INP_PTR_FLG_DRAGGED);
      }
   }

   /* Adjust acceleration
    * > If acceleration has not been set on this frame,
    *   apply normal attenuation */
   if (attenuate_y_accel)
      menu_input->pointer.y_accel *= MENU_INPUT_Y_ACCEL_DECAY_FACTOR;

   /* If select has been released, disable any existing
    * select inhibit */
   if (!(pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT))
      menu_input->select_inhibit   = false;

   /* Cancel */
   if (   !menu_input->cancel_inhibit
       && (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL)
       && !last_cancel_pressed)
   {
      /* If currently showing a message box, close it */
      if (messagebox_active)
         menu_input_pointer_close_messagebox(menu_st);
      /* If onscreen keyboard is shown, send a 'backspace' */
      else if (osk_active)
         input_keyboard_event(true, '\x7f', '\x7f',
               0, RETRO_DEVICE_KEYBOARD);
      /* ...otherwise, invoke standard MENU_ACTION_CANCEL
       * action */
      else
      {
         size_t selection = menu_st->selection_ptr;
         ret = menu_entry_action(&entry, selection, MENU_ACTION_CANCEL);
      }
   }

   /* If cancel has been released, disable any existing
    * cancel inhibit */
   if (!(pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL))
      menu_input->cancel_inhibit = false;

   if (!messagebox_active)
   {
      /* Up/Down
       * > Note 1: These always correspond to a mouse wheel, which
       *   handles differently from other inputs - i.e. we don't
       *   want a 'last pressed' check
       * > Note 2: If a message box is currently shown, must
       *   inhibit input */

      /* > Up */
      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_UP)
      {
         size_t selection = menu_st->selection_ptr;
         ret              = menu_entry_action(
               &entry, selection, MENU_ACTION_UP);
      }

      /* > Down */
      if (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_DOWN)
      {
         size_t selection = menu_st->selection_ptr;
         ret              = menu_entry_action(
               &entry, selection, MENU_ACTION_DOWN);
      }

      /* Left/Right
       * > Note 1: These also always correspond to a mouse wheel...
       *   In this case, it's a mouse wheel *tilt* operation, which
       *   is incredibly annoying because holding a tilt direction
       *   rapidly toggles the input state. The repeat speed is so
       *   high that any sort of useable control is impossible - so
       *   we have to apply a 'low pass' filter by ignoring inputs
       *   that occur below a certain frequency...
       * > Note 2: If a message box is currently shown, must
       *   inhibit input */

      /* > Left */
      if (     (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_LEFT)
            && !last_left_pressed)
      {
         if (current_time - last_left_action_time
               > MENU_INPUT_HORIZ_WHEEL_DELAY)
         {
            size_t selection      = menu_st->selection_ptr;
            last_left_action_time = current_time;
            ret                   = menu_entry_action(
                  &entry, selection, MENU_ACTION_LEFT);
         }
      }

      /* > Right */
      if (
               (pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_RIGHT)
            && !last_right_pressed)
      {
         if (current_time - last_right_action_time
               > MENU_INPUT_HORIZ_WHEEL_DELAY)
         {
            size_t selection       = menu_st->selection_ptr;
            last_right_action_time = current_time;
            ret                    = menu_entry_action(
                  &entry, selection, MENU_ACTION_RIGHT);
         }
      }
   }

   last_select_pressed = ((pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_SELECT) > 0);
   last_cancel_pressed = ((pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_CANCEL) > 0);
   last_left_pressed   = ((pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_LEFT)   > 0);
   last_right_pressed  = ((pointer_hw_state->flags & MENU_INP_PTR_FLG_PRESS_RIGHT)  > 0);

   return ret;
}

void menu_driver_toggle(
      void *curr_video_data,
      void *video_driver_data,
      menu_handle_t *menu,
      menu_input_t *menu_input,
      settings_t *settings,
      bool menu_driver_alive,
      bool overlay_alive,
      retro_keyboard_event_t *key_event,
      retro_keyboard_event_t *frontend_key_event,
      bool on)
{
   /* TODO/FIXME - retroarch_main_quit calls menu_driver_toggle -
    * we might have to redesign this to avoid EXXC_BAD_ACCESS errors
    * on OSX - for now we work around this by checking if the settings
    * struct is NULL
    */
   video_driver_t *current_video      = (video_driver_t*)curr_video_data;
   bool menu_pause_libretro           = false;
   bool audio_enable_menu             = false;
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   struct menu_state *menu_st         = &menu_driver_state;
   bool runloop_shutdown_initiated    = (runloop_st->flags &
         RUNLOOP_FLAG_SHUTDOWN_INITIATED) ? true : false;
#ifdef HAVE_OVERLAY
   bool input_overlay_hide_in_menu    = false;
   bool input_overlay_enable          = false;
#endif
   bool video_adaptive_vsync          = false;

   if (settings)
   {
#ifdef HAVE_NETWORKING
      menu_pause_libretro             = settings->bools.menu_pause_libretro
            && netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
      menu_pause_libretro             = settings->bools.menu_pause_libretro;
#endif
#ifdef HAVE_AUDIOMIXER
      audio_enable_menu               = settings->bools.audio_enable_menu;
#endif
#ifdef HAVE_OVERLAY
      input_overlay_hide_in_menu      = settings->bools.input_overlay_hide_in_menu;
      input_overlay_enable            = settings->bools.input_overlay_enable;
#endif
   }

   if (on)
   {
#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_MENU);
#endif
#ifdef HAVE_OVERLAY
      /* If an overlay was displayed before the toggle
       * and overlays are disabled in menu, need to
       * inhibit 'select' input */
      if (input_overlay_hide_in_menu)
      {
         if (input_overlay_enable && overlay_alive)
         {
            /* Inhibits pointer 'select' and 'cancel' actions
             * (until the next time 'select'/'cancel' are released) */
            menu_input->select_inhibit = true;
            menu_input->cancel_inhibit = true;
         }
      }
#endif
   }
   else
   {
#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_CORE);
#endif
#ifdef HAVE_OVERLAY
      /* Inhibits pointer 'select' and 'cancel' actions
       * (until the next time 'select'/'cancel' are released) */
      menu_input->select_inhibit      = false;
      menu_input->cancel_inhibit      = false;
#endif
   }

   if (menu_driver_alive)
   {
      video_adaptive_vsync          = settings->bools.video_adaptive_vsync
            && video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC);

#ifdef WIIU
      /* Enable burn-in protection menu is running */
      IMEnableDim();
#endif

      menu_st->flags               |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;

      /* Menu should always run with swap interval 1 if vsync is on. */
      if (     settings->bools.video_vsync
            && current_video->set_nonblock_state)
         current_video->set_nonblock_state(
               video_driver_data,
               false,
               video_adaptive_vsync,
               1);

      /* Stop all rumbling before entering the menu. */
      command_event(CMD_EVENT_RUMBLE_STOP, NULL);

      if (menu_pause_libretro)
      {
         if (!audio_enable_menu)
            command_event(CMD_EVENT_AUDIO_STOP, NULL);
#ifdef HAVE_MICROPHONE
         command_event(CMD_EVENT_MICROPHONE_STOP, NULL);
#endif
      }

      /* Override keyboard callback to redirect to menu instead.
       * We'll use this later for something ... */

      if (key_event && frontend_key_event)
      {
         *frontend_key_event          = *key_event;
         *key_event                   = menu_input_key_event;

         runloop_st->frame_time_last  = 0;
      }
   }
   else
   {
#ifdef WIIU
      /* Disable burn-in protection while core is running; this is needed
       * because HID inputs don't count for the purpose of Wii U
       * power-saving. */
      IMDisableDim();
#endif

      if (!runloop_shutdown_initiated)
         driver_set_nonblock_state();

      if (menu_pause_libretro)
      {
         if (!audio_enable_menu)
            command_event(CMD_EVENT_AUDIO_START, NULL);
#ifdef HAVE_MICROPHONE
         command_event(CMD_EVENT_MICROPHONE_START, NULL);
#endif
      }

      /* Restore libretro keyboard callback. */
      if (key_event && frontend_key_event)
         *key_event = *frontend_key_event;
   }

   /* Ignore frame delay target temporarily */
   if (settings->bools.video_frame_delay_auto)
      video_state_get_ptr()->frame_delay_pause = true;
}

void retroarch_menu_running(void)
{
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   input_driver_state_t *input_st  = input_state_get_ptr();
#ifdef HAVE_OVERLAY
   bool input_overlay_hide_in_menu = settings->bools.input_overlay_hide_in_menu;
#endif
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu          = settings->bools.audio_enable_menu;
   bool audio_enable_menu_bgm      = settings->bools.audio_enable_menu_bgm;
#endif
   struct menu_state *menu_st      = &menu_driver_state;
   menu_handle_t *menu             = menu_st->driver_data;
   menu_input_t *menu_input        = &menu_st->input_state;
   if (menu)
   {
      if (menu->driver_ctx && menu->driver_ctx->toggle)
         menu->driver_ctx->toggle(menu->userdata, true);

      menu_st->flags |= MENU_ST_FLAG_ALIVE;
      menu_driver_toggle(
            video_st->current_video,
            video_st->data,
            menu,
            menu_input,
            settings,
            (menu_st->flags & MENU_ST_FLAG_ALIVE) ? true : false,
#ifdef HAVE_OVERLAY
                input_st->overlay_ptr
            && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE),
#else
            false,
#endif
            &runloop_st->key_event,
            &runloop_st->frontend_key_event,
            true);
   }

   /* Prevent stray input */
   menu_st->input_driver_flushing_input = 2;

#ifdef HAVE_AUDIOMIXER
   if (audio_enable_menu && audio_enable_menu_bgm)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

   /* Ensure that game focus mode is disabled when
    * running the menu (note: it is not currently
    * possible for game focus to be enabled at this
    * point, but must safeguard against future changes) */
   if (input_st->game_focus_state.enabled)
   {
      enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_OFF;
      command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);
   }

   /* Ensure that menu screensaver is disabled when
    * first switching to the menu */
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      menu_st->flags &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
      if (menu_st->driver_ctx->environ_cb)
         menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
                  NULL, menu_st->userdata);
   }
   menu_st->input_last_time_us = cpu_features_get_time_usec();

#ifdef HAVE_OVERLAY
   if (input_overlay_hide_in_menu)
      command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
#endif
}

void retroarch_menu_running_finished(bool quit)
{
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   video_driver_state_t*video_st   = video_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   input_driver_state_t *input_st  = input_state_get_ptr();
   struct menu_state *menu_st      = &menu_driver_state;
   menu_handle_t *menu             = menu_st->driver_data;
   menu_input_t *menu_input        = &menu_st->input_state;
   if (menu)
   {
      if (menu->driver_ctx && menu->driver_ctx->toggle)
         menu->driver_ctx->toggle(menu->userdata, false);

      menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
      menu_driver_toggle(
            video_st->current_video,
            video_st->data,
            menu,
            menu_input,
            settings,
            menu_st->flags & MENU_ST_FLAG_ALIVE,
#ifdef HAVE_OVERLAY
                input_st->overlay_ptr
            && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE),
#else
            false,
#endif
            &runloop_st->key_event,
            &runloop_st->frontend_key_event,
            false);
   }

   /* Prevent stray input */
   menu_st->input_driver_flushing_input = 2;

   if (!quit)
   {
#ifdef HAVE_AUDIOMIXER
      /* Stop menu background music before we exit the menu */
      if (     settings
            && settings->bools.audio_enable_menu
            && settings->bools.audio_enable_menu_bgm
         )
         audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

      /* Enable game focus mode, if required */
      if (runloop_st->current_core_type != CORE_TYPE_DUMMY)
      {
         enum input_auto_game_focus_type auto_game_focus_type = settings
            ? (enum input_auto_game_focus_type)settings->uints.input_auto_game_focus
            : AUTO_GAME_FOCUS_OFF;

         if (      (auto_game_focus_type == AUTO_GAME_FOCUS_ON)
               || ((auto_game_focus_type == AUTO_GAME_FOCUS_DETECT)
               && input_st->game_focus_state.core_requested))
         {
            enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_ON;
            command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);
         }
      }

#if HAVE_RUNAHEAD
      /* Preemptive Frames isn't run behind the menu,
       * so its savestate buffer is out of date. */
      if (!settings->bools.menu_pause_libretro)
         command_event(CMD_EVENT_PREEMPT_RESET_BUFFER, NULL);
#endif
   }

   /* Ensure that menu screensaver is disabled when
    * switching off the menu */
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      menu_st->flags &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
      if (menu_st->driver_ctx->environ_cb)
         menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
                  NULL, menu_st->userdata);
   }
   if (video_st->poke && video_st->poke->set_texture_enable)
      video_st->poke->set_texture_enable(video_st->data,
            false, false);
#ifdef HAVE_OVERLAY
   if (!quit)
      if (settings && settings->bools.input_overlay_hide_in_menu)
         input_overlay_init();
#endif
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static void menu_shader_manager_free(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (video_st->menu_driver_shader)
      free(video_st->menu_driver_shader);
   video_st->menu_driver_shader = NULL;
}
#endif

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   struct menu_state    *menu_st  = &menu_driver_state;

   switch (state)
   {
      case RARCH_MENU_CTL_SET_PENDING_QUICK_MENU:
         {
            bool flush_stack = !data ? true : *((bool *)data);
            if (flush_stack)
               menu_entries_flush_stack(NULL, MENU_SETTINGS);
            menu_st->flags |= MENU_ST_FLAG_PENDING_QUICK_MENU;
         }
         break;
      case RARCH_MENU_CTL_DEINIT:
         if (     menu_st->driver_ctx
               && menu_st->driver_ctx->context_destroy)
            menu_st->driver_ctx->context_destroy(menu_st->userdata);

         if (menu_st->flags & MENU_ST_FLAG_DATA_OWN)
            return true;

         playlist_free_cached();
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         menu_shader_manager_free();
#endif
#ifdef HAVE_NETWORKING
         core_updater_list_free_cached();
#endif
#if defined(HAVE_MENU)
#if defined(HAVE_LIBRETRODB)
         /* Before freeing the explore menu, we
          * must wait for any explore menu initialisation
          * tasks to complete */
         menu_explore_wait_for_init_task();
         menu_explore_free();
#endif
         menu_contentless_cores_free();
#endif

         if (menu_st->driver_data)
         {
            unsigned i;
            gfx_display_t *p_disp         = disp_get_ptr();

            menu_st->scroll.acceleration  = 0;
            menu_st->selection_ptr        = 0;
            menu_st->contentless_core_ptr = 0;
            menu_st->scroll.index_size    = 0;

            menu_contentless_cores_flush_runtime();

            for (i = 0; i < SCROLL_INDEX_SIZE; i++)
               menu_st->scroll.index_list[i] = 0;

            memset(&menu_st->input_state, 0, sizeof(menu_input_t));
            memset(&menu_st->input_pointer_hw_state, 0, sizeof(menu_input_pointer_hw_state_t));

            if (     menu_st->driver_ctx
                  && menu_st->driver_ctx->free)
               menu_st->driver_ctx->free(menu_st->userdata);

            if (menu_st->userdata)
               free(menu_st->userdata);
            menu_st->userdata = NULL;
            p_disp->menu_driver_id = MENU_DRIVER_ID_UNKNOWN;

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
               libretro_free_system_info(&sys_info->info);
               memset(&sys_info->info, 0, sizeof(struct retro_system_info));
            }

            gfx_animation_deinit();
            gfx_display_free();

            menu_entries_settings_deinit(menu_st);
            if (menu_st->entries.list)
               menu_list_free(menu_st->driver_ctx, menu_st->entries.list);
            menu_st->entries.list           = NULL;

            if (menu_st->thumbnail_path_data)
               free(menu_st->thumbnail_path_data);
            menu_st->thumbnail_path_data    = NULL;

            if (menu_st->driver_data->core_buf)
               free(menu_st->driver_data->core_buf);
            menu_st->driver_data->core_buf  = NULL;

            menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                              | MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH);
            menu_st->entries.begin               = 0;

            command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
            retroarch_favorites_deinit();

            menu_st->dialog_st.pending_push  = false;
            menu_st->dialog_st.current_id    = 0;
            menu_st->dialog_st.current_type  = MENU_DIALOG_NONE;

            free(menu_st->driver_data);
         }
         menu_st->driver_data = NULL;
         break;
      case RARCH_MENU_CTL_POINTER_DOWN:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_st->driver_ctx || !menu_st->driver_ctx->pointer_down)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_st->driver_ctx->pointer_down(
                  menu_st->userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_POINTER_UP:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_st->driver_ctx || !menu_st->driver_ctx->pointer_up)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_st->driver_ctx->pointer_up(
                  menu_st->userdata,
                  point->x, point->y, point->ptr,
                  point->gesture,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_OSK_PTR_AT_POS:
         {
            video_driver_state_t
               *video_st              = video_state_get_ptr();
            unsigned width            = video_st->width;
            unsigned height           = video_st->height;
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_st->driver_ctx || !menu_st->driver_ctx->osk_ptr_at_pos)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_st->driver_ctx->osk_ptr_at_pos(
                  menu_st->userdata,
                  point->x, point->y, width, height);
         }
         break;
      case MENU_NAVIGATION_CTL_CLEAR:
         {
            bool *pending_push     = (bool*)data;

            /* Always set current selection to first entry */
            menu_st->selection_ptr = 0;

            /* navigation_set() will be called at the next 'push'.
             * If a push is *not* pending, have to do it here
             * instead */
            if (!(*pending_push))
            {
               if (menu_st->driver_ctx->navigation_set)
                  menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
               if (menu_st->driver_ctx->navigation_clear)
                  menu_st->driver_ctx->navigation_clear(
                        menu_st->userdata, *pending_push);
            }
         }
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return true;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
struct video_shader *menu_shader_get(void)
{
   if (video_shader_any_supported())
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      if (video_st)
         return video_st->menu_driver_shader;
   }
   return NULL;
}

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
bool menu_shader_manager_init(void)
{
   video_driver_state_t *video_st   = video_state_get_ptr();
   enum rarch_shader_type type      = RARCH_SHADER_NONE;
   bool ret                         = true;
   bool is_preset                   = false;
   const char *path_shader          = NULL;
   struct video_shader *menu_shader = NULL;
   /* We get the shader preset directly from the video driver, so that
    * we are in sync with it (it could fail loading an auto-shader)
    * If we can't (e.g. get_current_shader is not implemented),
    * we'll load video_shader_get_current_shader_preset() like always */
   video_shader_ctx_t shader_info   = {0};

   video_shader_driver_get_current_shader(&shader_info);

   if (shader_info.data)
      /* Use the path of the originally loaded preset because it could
       * have been a preset with a #reference in it to another preset */
      path_shader                 = shader_info.data->loaded_preset_path;
   else
      path_shader                 = video_shader_get_current_shader_preset();

   menu_shader_manager_free();

   menu_shader                    = (struct video_shader*)
      calloc(1, sizeof(*menu_shader));

   if (!menu_shader)
   {
      ret = false;
      goto end;
   }

   if (string_is_empty(path_shader))
      goto end;

   type = video_shader_get_type_from_ext(path_get_extension(path_shader),
         &is_preset);

   if (!video_shader_is_supported(type))
   {
      ret = false;
      goto end;
   }

   if (is_preset)
   {
      if (!video_shader_load_preset_into_shader(path_shader, menu_shader))
      {
         ret = false;
         goto end;
      }
      menu_shader->flags   &= ~SHDR_FLAG_MODIFIED;
   }
   else
   {
      strlcpy(menu_shader->pass[0].source.path, path_shader,
            sizeof(menu_shader->pass[0].source.path));
      menu_shader->passes = 1;
   }

end:
   video_st->menu_driver_shader = menu_shader;
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;
}

/**
 * menu_shader_manager_set_preset:
 * @menu_shader              : Shader handle to the menu shader.
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 * @apply                    : Whether to apply the shader or just update shader information
 *
 * Sets shader preset.
 **/
bool menu_shader_manager_set_preset(struct video_shader *menu_shader,
      enum rarch_shader_type type, const char *preset_path, bool apply)
{
   bool ret                      = false;
   struct menu_state *menu_st    = &menu_driver_state;
   settings_t *settings          = config_get_ptr();

   if (apply && !video_shader_apply_shader(settings, type, preset_path, true))
      goto clear;

   if (string_is_empty(preset_path))
   {
      ret = true;
      goto clear;
   }

   /* Load stored Preset into menu on success.
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was
    * created from the menu itself. */
   if (     !menu_shader
         || !(video_shader_load_preset_into_shader(preset_path, menu_shader)))
      goto end;

   /* TODO/FIXME - localize */
   RARCH_LOG("[Shaders]: Menu shader set to: \"%s\".\n", preset_path);

   ret = true;

end:
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;

clear:
   /* We don't want to disable shaders entirely here,
    * just reset number of passes
    * > Note: Disabling shaders at this point would in
    *   fact be dangerous, since it changes the number of
    *   entries in the shader options menu which can in
    *   turn lead to the menu selection pointer going out
    *   of bounds. This causes undefined behaviour/segfaults */
   menu_shader_manager_clear_num_passes(menu_shader);
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;
}

/**
 * menu_shader_manager_append_preset:
 * @shader                   : current shader
 * @preset_path              : path to the preset to append
 * @dir_video_shader         : temporary directory
 *
 * combine current shader with a shader preset on disk
 **/
bool menu_shader_manager_append_preset(struct video_shader *shader,
      const char* preset_path, const bool prepend)
{
   bool ret                      = false;
   settings_t* settings          = config_get_ptr();
   const char *dir_video_shader  = settings->paths.directory_video_shader;
   enum rarch_shader_type type   = menu_shader_manager_get_type(shader);
   struct menu_state *menu_st    = &menu_driver_state;

   if (string_is_empty(preset_path))
   {
      ret = true;
      goto clear;
   }

   if (!video_shader_combine_preset_and_apply(settings,
            type, shader, preset_path, dir_video_shader, prepend, true))
      goto clear;

   /* TODO/FIXME - localize */
   RARCH_LOG("[Shaders]: Menu shader set to: \"%s\".\n", preset_path);

   ret             = true;

   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;

clear:
   /* We don't want to disable shaders entirely here,
    * just reset number of passes
    * > Note: Disabling shaders at this point would in
    *   fact be dangerous, since it changes the number of
    *   entries in the shader options menu which can in
    *   turn lead to the menu selection pointer going out
    *   of bounds. This causes undefined behaviour/segfaults */
   menu_shader_manager_clear_num_passes(shader);
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;
}
#endif


/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop.
 **/
static int generic_menu_iterate(
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      menu_handle_t *menu,
      void *userdata, enum menu_action action,
      retro_time_t current_time)
{
#ifdef HAVE_ACCESSIBILITY
   static enum action_iterate_type
      last_iterate_type            = ITERATE_TYPE_DEFAULT;
   access_state_t *access_st       = access_state_get_ptr();
   bool accessibility_enable       = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   enum action_iterate_type iterate_type;
   int ret                         = 0;
   const char *label               = NULL;
   file_list_t *list               = MENU_LIST_GET(menu_st->entries.list, 0);

   if (list && list->size)
      label                        = list->list[list->size - 1].label;

   menu->menu_state_msg[0]         = '\0';

   iterate_type                    = action_iterate_type(label);
   menu_st->flags                 &= ~MENU_ST_FLAG_IS_BINDING;

   if (     action != MENU_ACTION_NOOP
         || MENU_ENTRIES_NEEDS_REFRESH(menu_st)
         || GFX_DISPLAY_GET_UPDATE_PENDING(p_anim, p_disp))
   {
      BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   }

   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = menu_dialog_iterate(
               &menu_st->dialog_st,  settings,
               menu->menu_state_msg, sizeof(menu->menu_state_msg),
               current_time);

#ifdef HAVE_ACCESSIBILITY
         if (     (iterate_type != last_iterate_type)
               && is_accessibility_enabled(
                  accessibility_enable,
                  access_st->enabled))
            accessibility_speak_priority(
                  accessibility_enable,
                  accessibility_narrator_speech_speed,
                  menu->menu_state_msg, 10);
#endif

         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

         if (     (ret    == 1)
               || (action == MENU_ACTION_OK)
               || (action == MENU_ACTION_CANCEL)
            )
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         break;
      case ITERATE_TYPE_BIND:
         {
            menu_input_ctx_bind_t bind;

            menu_st->flags     |= MENU_ST_FLAG_IS_BINDING;

            bind.s              = menu->menu_state_msg;
            bind.len            = sizeof(menu->menu_state_msg);

            if (menu_input_key_bind_iterate(
                     settings,
                     &bind, current_time))
            {
               size_t selection = menu_st->selection_ptr;
               menu_entries_pop_stack(&selection, 0, 0);
               menu_st->selection_ptr      = selection;
            }
            else
               BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         }
         break;
      case ITERATE_TYPE_INFO:
         {
            menu_list_t *menu_list     = menu_st->entries.list;
            file_list_t *selection_buf = menu_list ? MENU_LIST_GET_SELECTION(menu_list, (unsigned)0) : NULL;
            size_t selection           = menu_st->selection_ptr;
            menu_file_list_cbs_t *cbs  = selection_buf ?
               (menu_file_list_cbs_t*)selection_buf->list[selection].actiondata
               : NULL;

            if (cbs && cbs->enum_idx != MSG_UNKNOWN)
            {
               /* Core updater/manager entries require special treatment */
               switch (cbs->enum_idx)
               {
#ifdef HAVE_NETWORKING
                  case MENU_ENUM_LABEL_CORE_UPDATER_ENTRY:
                     {
                        core_updater_list_t *core_list         =
                           core_updater_list_get_cached();
                        const core_updater_list_entry_t *entry = NULL;
                        const char *path                       =
                           selection_buf->list[selection].path;

                        /* Search for specified core */
                        if (
                                 core_list
                              && path
                              && core_updater_list_get_filename(core_list,
                                 path, &entry)
                              && !string_is_empty(entry->description)
                           )
                           strlcpy(menu->menu_state_msg, entry->description,
                                 sizeof(menu->menu_state_msg));
                        else
                           strlcpy(menu->menu_state_msg,
                                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                                 sizeof(menu->menu_state_msg));

                        ret = 0;
                     }
                     break;
#endif
                  case MENU_ENUM_LABEL_CORE_MANAGER_ENTRY:
                  case MENU_ENUM_LABEL_CONTENTLESS_CORE:
                     {
                        core_info_t *core_info = NULL;
                        const char *path       = selection_buf->list[selection].path;

                        /* Search for specified core */
                        if (     path
                              && core_info_find(path, &core_info)
                              && !string_is_empty(core_info->description))
                           strlcpy(menu->menu_state_msg,
                                 core_info->description,
                                 sizeof(menu->menu_state_msg));
                        else
                           strlcpy(menu->menu_state_msg,
                                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                                 sizeof(menu->menu_state_msg));

                        ret = 0;
                     }
                     break;
                  default:
                     ret = msg_hash_get_help_enum(cbs->enum_idx,
                           menu->menu_state_msg, sizeof(menu->menu_state_msg));

                     if (string_is_equal(menu->menu_state_msg,
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
                     {
                        menu_driver_get_current_menu_sublabel(
                              menu_st,
                              menu->menu_state_msg, sizeof(menu->menu_state_msg));
                        if (string_is_equal(menu->menu_state_msg, ""))
                           strlcpy(menu->menu_state_msg,
                                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                                 sizeof(menu->menu_state_msg));
                     }
                     break;
               }

#ifdef HAVE_ACCESSIBILITY
               if (  (iterate_type != last_iterate_type) &&
                     is_accessibility_enabled(
                        accessibility_enable,
                        access_st->enabled))
               {
                  if (string_is_equal(menu->menu_state_msg,
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
                  {
                     char current_sublabel[NAME_MAX_LENGTH];
                     menu_driver_get_current_menu_sublabel(
                           menu_st,
                           current_sublabel, sizeof(current_sublabel));
                     if (string_is_equal(current_sublabel, ""))
                        accessibility_speak_priority(
                              accessibility_enable,
                              accessibility_narrator_speech_speed,
                              menu->menu_state_msg, 10);
                     else
                        accessibility_speak_priority(
                              accessibility_enable,
                              accessibility_narrator_speech_speed,
                              current_sublabel, 10);
                  }
                  else
                     accessibility_speak_priority(
                           accessibility_enable,
                           accessibility_narrator_speech_speed,
                           menu->menu_state_msg, 10);
               }
#endif
            }
            else
            {
               enum msg_hash_enums enum_idx = MSG_UNKNOWN;
               size_t selection             = menu_st->selection_ptr;
               unsigned type                = selection_buf->list[selection].type;

               switch (type)
               {
                  case FILE_TYPE_FONT:
                  case FILE_TYPE_VIDEO_FONT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_FONT;
                     break;
                  case FILE_TYPE_RDB:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RDB;
                     break;
                  case FILE_TYPE_OVERLAY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY;
                     break;
                  case FILE_TYPE_CHEAT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CHEAT;
                     break;
                  case FILE_TYPE_SHADER_PRESET:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET;
                     break;
                  case FILE_TYPE_SHADER:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER;
                     break;
                  case FILE_TYPE_REMAP:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_REMAP;
                     break;
                  case FILE_TYPE_RECORD_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG;
                     break;
                  case FILE_TYPE_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CONFIG;
                     break;
                  case FILE_TYPE_CARCHIVE:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE;
                     break;
                  case FILE_TYPE_DIRECTORY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
                     break;
                  case FILE_TYPE_VIDEOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_AUDIOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_SLANG:           /* TODO/FIXME */
                  case FILE_TYPE_SHADER_GLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_HLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_CG:              /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_GLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_HLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_CGP:      /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_SLANGP:   /* TODO/FIXME */
                  case FILE_TYPE_PLAIN:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE;
                     break;
                  default:
                     break;
               }

               if (enum_idx != MSG_UNKNOWN)
                  ret = msg_hash_get_help_enum(enum_idx,
                        menu->menu_state_msg, sizeof(menu->menu_state_msg));
               else
               {
                  /* Special handling for input and remap items */
                  if (     (  type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN
                           && type <= MENU_SETTINGS_REMAPPING_PORT_END)
                        || type == MENU_SETTINGS_INPUT_LIBRETRO_DEVICE
                        || type == MENU_SETTINGS_INPUT_INPUT_REMAP_PORT)
                  {
                     menu_driver_get_current_menu_sublabel(
                           menu_st,
                           menu->menu_state_msg, sizeof(menu->menu_state_msg));

                     ret = 0;
                  }
                  /* Use detailed help text for 'Analog to Digital', which
                   * is the first item in global input settings */
                  else if (type == MENU_SETTINGS_INPUT_ANALOG_DPAD_MODE
                        || type == MENU_SETTINGS_INPUT_BEGIN)
                     ret = msg_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
                           menu->menu_state_msg, sizeof(menu->menu_state_msg));
                  else
                  {
                     strlcpy(menu->menu_state_msg,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                           sizeof(menu->menu_state_msg));

                     ret = 0;
                  }
               }
            }
         }
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (     action == MENU_ACTION_OK
               || action == MENU_ACTION_CANCEL
               || action == MENU_ACTION_INFO)
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         break;
      case ITERATE_TYPE_DEFAULT:
         {
            menu_entry_t entry;
            menu_list_t *menu_list = menu_st->entries.list;
            size_t selection       = menu_st->selection_ptr;
            size_t menu_list_size  = menu_st->entries.list ? MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size : 0;
            /* FIXME: Crappy hack, needed for mouse controls
             * to not be completely broken in case we press back.
             *
             * We need to fix this entire mess, mouse controls
             * should not rely on a hack like this in order to work. */
            selection = MAX(MIN(selection, (menu_list_size - 1)), 0);

            MENU_ENTRY_INITIALIZE(entry);
            /* NOTE: If menu_entry_action() is modified,
             * will have to verify that these parameters
             * remain unused... */
            entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                         | MENU_ENTRY_FLAG_LABEL_ENABLED;
            menu_entry_get(&entry, 0, selection, NULL, false);
            if ((ret = menu_entry_action(&entry,
                  selection, (enum menu_action)action)))
               return -1;

            BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

            /* Have to defer it so we let settings refresh. */
            if (menu_st->dialog_st.pending_push)
            {
               const char *label;
               menu_displaylist_info_t info;

               menu_displaylist_info_init(&info);

               info.list                 = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
               info.enum_idx             = MENU_ENUM_LABEL_HELP;

               /* Set the label string, if it exists. */
               label                     = msg_hash_to_str(MENU_ENUM_LABEL_HELP);
               if (label)
                  info.label             = strdup(label);

               menu_displaylist_ctl(DISPLAYLIST_HELP, &info, settings);
            }
         }
         break;
   }

#ifdef HAVE_ACCESSIBILITY
   if ((last_iterate_type == ITERATE_TYPE_HELP
            || last_iterate_type == ITERATE_TYPE_INFO)
         && last_iterate_type != iterate_type
         && is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
      accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            "Closed dialog.", 10);

   last_iterate_type = iterate_type;
#endif

   BIT64_SET(menu->state, MENU_STATE_BLIT);

   if (BIT64_GET(menu->state, MENU_STATE_POP_STACK))
   {
      size_t selection         = menu_st->selection_ptr;
      size_t new_selection_ptr = selection;
      menu_entries_pop_stack(&new_selection_ptr, 0, 0);
      menu_st->selection_ptr   = selection;
      /* Play sound for closing the info box */
#ifdef HAVE_AUDIOMIXER
      {
         bool        audio_enable_menu = settings->bools.audio_enable_menu;
         bool audio_enable_menu_notice = settings->bools.audio_enable_menu_notice;
         if (audio_enable_menu && audio_enable_menu_notice &&
               string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFO_SCREEN)))
            audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_NOTICE_BACK);
      }
#endif
   }

   if (BIT64_GET(menu->state, MENU_STATE_POST_ITERATE))
   {
      menu_input_t     *menu_input  = &menu_st->input_state;
      /* If pointer devices are disabled, just ensure mouse
       * cursor is hidden */
      if (menu_input->pointer.type == MENU_POINTER_DISABLED)
         ret = 0;
      else
         ret = menu_input_post_iterate(p_disp, menu_st, action,
               current_time);
      menu_input_set_pointer_visibility(
            &menu_st->input_pointer_hw_state,
             menu_input, current_time);
   }

   if (ret)
      return -1;
   return 0;
}

int generic_menu_entry_action(
      void *userdata, menu_entry_t *entry, size_t i,
      enum menu_action action)
{
   int ret                        = 0;
   struct menu_state *menu_st     = &menu_driver_state;
   const menu_ctx_driver_t
      *menu_driver_ctx            = menu_st->driver_ctx;
   menu_handle_t  *menu           = menu_st->driver_data;
   settings_t   *settings         = config_get_ptr();
   void *menu_userdata            = menu_st->userdata;
   bool wraparound_enable         = settings->bools.menu_navigation_wraparound_enable;
   bool scroll_mode               = menu_st->scroll.mode;
   size_t scroll_accel            = menu_st->scroll.acceleration;
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *selection_buf     = menu_list ? MENU_LIST_GET_SELECTION(menu_list, (unsigned)0) : NULL;
   file_list_t *menu_stack        = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
   size_t entries_size            = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0)->size : 0;
   size_t selection_buf_size      = selection_buf ? selection_buf->size : 0;
   menu_file_list_cbs_t *cbs      = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
#ifdef HAVE_ACCESSIBILITY
   bool accessibility_enable      = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   access_state_t *access_st      = access_state_get_ptr();
#endif

   switch (action)
   {
      case MENU_ACTION_UP:
         if (selection_buf_size > 0)
         {
            unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
            if (!(menu_st->selection_ptr == 0 && !wraparound_enable))
            {
               size_t idx             = 0;
               if (menu_st->selection_ptr >= scroll_speed)
                  idx = menu_st->selection_ptr - scroll_speed;
               else
               {
                  idx  = selection_buf_size - 1;
                  if (!wraparound_enable)
                     idx = 0;
               }

               menu_st->selection_ptr = idx;
               if (menu_st->driver_ctx->navigation_set)
                  menu_st->driver_ctx->navigation_set(menu_st->userdata, true);

               if (menu_driver_ctx->navigation_decrement)
                  menu_driver_ctx->navigation_decrement(menu_userdata);
#ifdef HAVE_AUDIOMIXER
               if (entries_size != 1)
                  audio_driver_mixer_play_scroll_sound(true);
#endif
            }
         }
         break;
      case MENU_ACTION_DOWN:
         if (selection_buf_size > 0)
         {
            unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
            if (!(menu_st->selection_ptr >= selection_buf_size - 1
                  && !wraparound_enable))
            {
               if ((menu_st->selection_ptr + scroll_speed) < selection_buf_size)
               {
                  size_t idx             = menu_st->selection_ptr + scroll_speed;
                  menu_st->selection_ptr = idx;
                  if (menu_st->driver_ctx->navigation_set)
                     menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
               }
               else
               {
                  if (wraparound_enable)
                  {
                     bool pending_push = false;
                     menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
                  }
                  else
                  {
                     size_t menu_list_size     = menu_st->entries.list ? MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size : 0;
                     size_t new_selection      = menu_list_size - 1;

                     menu_st->selection_ptr    = new_selection;

                     if (menu_st->driver_ctx->navigation_set_last)
                        menu_st->driver_ctx->navigation_set_last(menu_st->userdata);
                  }
               }

               if (menu_driver_ctx->navigation_increment)
                  menu_driver_ctx->navigation_increment(menu_userdata);
#ifdef HAVE_AUDIOMIXER
               if (entries_size != 1)
                  audio_driver_mixer_play_scroll_sound(false);
#endif
            }
         }
         break;
      case MENU_ACTION_SCROLL_UP:
         if (scroll_mode == MENU_SCROLL_PAGE)
         {
            if (selection_buf_size > 0)
            {
               unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 10);
#ifdef HAVE_AUDIOMIXER
               if (menu_st->selection_ptr != 0)
                  audio_driver_mixer_play_scroll_sound(true);
#endif
               if (!(menu_st->selection_ptr == 0 && !wraparound_enable))
               {
                  size_t idx             = 0;
                  if (menu_st->selection_ptr >= scroll_speed)
                     idx                 = menu_st->selection_ptr - scroll_speed;

                  menu_st->selection_ptr = idx;
                  if (menu_st->driver_ctx->navigation_set)
                     menu_st->driver_ctx->navigation_set(menu_st->userdata, true);

                  if (menu_driver_ctx->navigation_decrement)
                     menu_driver_ctx->navigation_decrement(menu_userdata);
               }
            }
         }
         else /* MENU_SCROLL_START_LETTER */
         {
#ifdef HAVE_AUDIOMIXER
            size_t selection_old = menu_st->selection_ptr;
#endif
            if (
                     menu_st->scroll.index_size
                  && menu_st->selection_ptr != 0
               )
            {
               size_t l   = menu_st->scroll.index_size - 1;

               while (l
                     && menu_st->scroll.index_list[l - 1]
                     >= menu_st->selection_ptr)
                  l--;

               if (l > 0)
                  menu_st->selection_ptr = menu_st->scroll.index_list[l - 1];

               if (menu_driver_ctx->navigation_descend_alphabet)
                  menu_driver_ctx->navigation_descend_alphabet(
                        menu_userdata, &menu_st->selection_ptr);
            }
#ifdef HAVE_AUDIOMIXER
            if (menu_st->selection_ptr != selection_old)
               audio_driver_mixer_play_scroll_sound(true);
#endif
         }
         break;
      case MENU_ACTION_SCROLL_DOWN:
         if (scroll_mode == MENU_SCROLL_PAGE)
         {
            if (selection_buf_size > 0)
            {
               unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 10);
#ifdef HAVE_AUDIOMIXER
               if (menu_st->selection_ptr != entries_size - 1)
                  audio_driver_mixer_play_scroll_sound(false);
#endif
               if (!(menu_st->selection_ptr >= selection_buf_size - 1
                     && !wraparound_enable))
               {
                  if ((menu_st->selection_ptr + scroll_speed) < selection_buf_size)
                  {
                     size_t idx             = menu_st->selection_ptr + scroll_speed;
                     menu_st->selection_ptr = idx;
                     if (menu_st->driver_ctx->navigation_set)
                        menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
                  }
                  else
                  {
                     size_t menu_list_size     = menu_st->entries.list ? MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size : 0;
                     size_t new_selection      = menu_list_size - 1;

                     menu_st->selection_ptr    = new_selection;

                     if (menu_st->driver_ctx->navigation_set_last)
                        menu_st->driver_ctx->navigation_set_last(menu_st->userdata);
                  }

                  if (menu_driver_ctx->navigation_increment)
                     menu_driver_ctx->navigation_increment(menu_userdata);
               }
            }
         }
         else /* MENU_SCROLL_START_LETTER */
         {
            if (menu_st->scroll.index_size)
            {
#ifdef HAVE_AUDIOMIXER
               size_t selection_old = menu_st->selection_ptr;
#endif
               if (menu_st->selection_ptr == menu_st->scroll.index_list[menu_st->scroll.index_size - 1])
                  menu_st->selection_ptr = selection_buf_size - 1;
               else
               {
                  size_t l               = 0;
                  while (l < menu_st->scroll.index_size - 1
                        && menu_st->scroll.index_list[l + 1] <= menu_st->selection_ptr)
                     l++;
                  menu_st->selection_ptr = menu_st->scroll.index_list[l + 1];

                  if (menu_st->selection_ptr >= selection_buf_size)
                     menu_st->selection_ptr = selection_buf_size - 1;
               }

               if (menu_driver_ctx->navigation_ascend_alphabet)
                  menu_driver_ctx->navigation_ascend_alphabet(
                        menu_userdata, &menu_st->selection_ptr);
#ifdef HAVE_AUDIOMIXER
               if (menu_st->selection_ptr != selection_old)
                  audio_driver_mixer_play_scroll_sound(false);
#endif
            }
         }
         break;
      case MENU_ACTION_SCROLL_HOME:
      {
#ifdef HAVE_AUDIOMIXER
         size_t selection_old = menu_st->selection_ptr;
#endif
         menu_st->selection_ptr = 0;
         if (menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, true);

#ifdef HAVE_AUDIOMIXER
         if (menu_st->selection_ptr != selection_old)
            audio_driver_mixer_play_scroll_sound(true);
#endif
         break;
      }
      case MENU_ACTION_SCROLL_END:
      {
#ifdef HAVE_AUDIOMIXER
         size_t selection_old   = menu_st->selection_ptr;
#endif
         menu_st->selection_ptr = entries_size ? entries_size - 1 : 0;
         if (menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, true);

#ifdef HAVE_AUDIOMIXER
         if (menu_st->selection_ptr != selection_old)
            audio_driver_mixer_play_scroll_sound(false);
#endif
         break;
      }
      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path,
                  entry->label, entry->type, i);
         break;
      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_SEARCH:
         menu_input_dialog_start_search();
         break;
      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;
      default:
         break;
   }

   if (MENU_ENTRIES_NEEDS_REFRESH(menu_st))
   {
      menu_driver_displaylist_push(
            menu_st,
            settings,
            selection_buf,
            menu_stack);
      menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }

#ifdef HAVE_ACCESSIBILITY
   if (     action != 0
         && is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled)
         && !menu_input_dialog_get_display_kb())
   {
      char current_label[128];
      char current_value[128];
      char title_name[NAME_MAX_LENGTH];
      char speak_string[512];

      speak_string[0]  = '\0';
      title_name  [0]  = '\0';
      current_label[0] = '\0';

      get_current_menu_value(menu_st,
            current_value, sizeof(current_value));

      switch (action)
      {
         case MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE:
            menu_entries_get_title(title_name, sizeof(title_name));
            break;
         case MENU_ACTION_START:
            /* if equal to '..' we break, else we fall-through */
            if (string_is_equal(current_value, "..."))
               break;
            /* fall-through */
         case MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE_LABEL:
         case MENU_ACTION_OK:
         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_CANCEL:
            menu_entries_get_title(title_name, sizeof(title_name));
            /* fall-through */
         case MENU_ACTION_UP:
         case MENU_ACTION_DOWN:
         case MENU_ACTION_SCROLL_UP:
         case MENU_ACTION_SCROLL_DOWN:
         case MENU_ACTION_SELECT:
         case MENU_ACTION_SEARCH:
         case MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL:
            menu_driver_get_current_menu_label(menu_st, current_label, sizeof(current_label));
            break;
         case MENU_ACTION_SCAN:
         case MENU_ACTION_INFO:
         default:
            break;
      }

      if (!string_is_empty(title_name))
      {
         size_t _len             = strlcpy(speak_string,
               title_name, sizeof(speak_string));
         speak_string[  _len]    = ' ';
         speak_string[++_len]    = '\0';
         _len += strlcpy(speak_string + _len,
               current_label,
               sizeof(speak_string)   - _len);
         if (!string_is_equal(current_value, "..."))
         {
            speak_string[  _len] = ' ';
            speak_string[++_len] = '\0';
            strlcpy(speak_string       + _len,
                  current_value,
                  sizeof(speak_string) - _len);
         }
      }
      else
      {
         size_t _len = strlcpy(speak_string,
               current_label, sizeof(speak_string));
         if (!string_is_equal(current_value, "..."))
         {
            speak_string[  _len] = ' ';
            speak_string[++_len] = '\0';
            strlcpy(speak_string       + _len,
                  current_value,
                  sizeof(speak_string) - _len);
         }
      }

      if (!string_is_empty(speak_string))
         accessibility_speak_priority(
               accessibility_enable,
               accessibility_narrator_speech_speed,
               speak_string, 10);
   }
#endif

   if (   (menu_st->flags & MENU_ST_FLAG_PENDING_CLOSE_CONTENT)
       || (menu_st->flags & MENU_ST_FLAG_PENDING_ENV_SHUTDOWN_FLUSH))
   {
      const char *content_path  = (menu_st->flags
            & MENU_ST_FLAG_PENDING_ENV_SHUTDOWN_FLUSH)
            ? menu_st->pending_env_shutdown_content_path
            : path_get(RARCH_PATH_CONTENT);
      const char *deferred_path = menu ? menu->deferred_path : NULL;
      const char *flush_target  = msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);
      size_t stack_offset       = 1;
      unsigned i                = 0;
      bool reset_navigation     = true;

      /* Loop backwards through the menu stack to
       * find a known reference point */
      while (menu_stack && (menu_stack->size >= stack_offset))
      {
         const char *parent_label = menu_stack->list[
            menu_stack->size - stack_offset].label;

         if (string_is_empty(parent_label))
            continue;

         /* If core was launched via a playlist, flush
          * to playlist entry menu */
         if (    string_is_equal(parent_label,
                 msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS))
              && (!string_is_empty(deferred_path)
              && !string_is_empty(content_path)
              && string_is_equal(deferred_path, content_path))
             )
         {
            flush_target = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);
            break;
         }
         /* If core was launched via 'Contentless Cores' menu,
          * flush to 'Contentless Cores' menu */
         else if (   string_is_equal(parent_label,
                        msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB))
                  || string_is_equal(parent_label,
                        msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST)))
         {
            flush_target     = parent_label;
            reset_navigation = false;
            break;
         }

         stack_offset++;
      }

      if (!(menu_st->flags & MENU_ST_FLAG_PENDING_ENV_SHUTDOWN_FLUSH))
         command_event(CMD_EVENT_UNLOAD_CORE, NULL);

      menu_entries_flush_stack(flush_target, 0);
      /* An annoyance - some menu drivers (Ozone...) set
       * MENU_ST_FLAG_PREVENT_POPULATE in awkward
       * places, which can cause breakage here when flushing
       * the menu stack. We therefore have to unset
       * MENU_ST_FLAG_PREVENT_POPULATE */
      menu_st->flags &= ~MENU_ST_FLAG_PREVENT_POPULATE;

      /* Ozone requires thumbnail refreshing */
      if (menu_st->driver_ctx && menu_st->driver_ctx->refresh_thumbnail_image)
         menu_st->driver_ctx->refresh_thumbnail_image(
               menu_st->userdata, i);

      if (reset_navigation)
         menu_st->selection_ptr = 0;

      menu_st->flags &= ~(MENU_ST_FLAG_PENDING_CLOSE_CONTENT
                        | MENU_ST_FLAG_PENDING_ENV_SHUTDOWN_FLUSH);
      menu_st->pending_env_shutdown_content_path[0] = '\0';
   }

   return ret;
}

/* Iterate the menu driver for one frame. */
bool menu_driver_iterate(
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      enum menu_action action,
      retro_time_t current_time)
{
   return ( menu_st->driver_data
         && generic_menu_iterate(
            menu_st,
            p_disp,
            p_anim,
            settings,
            menu_st->driver_data,
            menu_st->userdata, action,
            current_time) != -1);
}

bool menu_input_dialog_start_search(void)
{
   input_driver_state_t *input_st          = input_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings                    = config_get_ptr();
   bool accessibility_enable               = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   access_state_t *access_st               = access_state_get_ptr();
#endif
   struct menu_state *menu_st              = &menu_driver_state;
   menu_handle_t         *menu             = menu_st->driver_data;

   if (!menu)
      return false;

#ifdef HAVE_MIST
   steam_open_osk();
#endif
   menu_st->flags                         |= MENU_ST_FLAG_INP_DLG_KB_DISPLAY;
   strlcpy(menu_st->input_dialog_kb_label,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH),
         sizeof(menu_st->input_dialog_kb_label));

   input_keyboard_line_free(input_st);

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
         accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            (char*)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH), 10);
#endif

   menu_st->input_dialog_keyboard_buffer   =
      input_keyboard_start_line(menu,
            &input_st->keyboard_line,
            menu_input_search_cb);
   /* While reading keyboard line input, we have to block all hotkeys. */
   input_st->flags                        |= INP_FLAG_KB_MAPPING_BLOCKED;

   return true;
}

bool menu_input_dialog_start(menu_input_ctx_line_t *line)
{
   input_driver_state_t *input_st   = input_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings             = config_get_ptr();
   bool accessibility_enable        = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   access_state_t *access_st        = access_state_get_ptr();
#endif
   struct menu_state *menu_st       = &menu_driver_state;
   menu_handle_t         *menu      = menu_st->driver_data;
   if (!line || !menu)
      return false;

#ifdef HAVE_MIST
   steam_open_osk();
#endif
   menu_st->flags |= MENU_ST_FLAG_INP_DLG_KB_DISPLAY;

   /* Only copy over the menu label and setting if they exist. */
   if (line->label)
      strlcpy(menu_st->input_dialog_kb_label,
            line->label,
            sizeof(menu_st->input_dialog_kb_label));
   if (line->label_setting)
      strlcpy(menu_st->input_dialog_kb_label_setting,
            line->label_setting,
            sizeof(menu_st->input_dialog_kb_label_setting));

   menu_st->input_dialog_kb_type   = line->type;
   menu_st->input_dialog_kb_idx    = line->idx;

   input_keyboard_line_free(input_st);

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
      accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            "Keyboard input:", 10);
#endif

   menu_st->input_dialog_keyboard_buffer =
      input_keyboard_start_line(menu,
            &input_st->keyboard_line,
            line->cb);
   /* While reading keyboard line input, we have to block all hotkeys. */
   input_st->flags |= INP_FLAG_KB_MAPPING_BLOCKED;

   return true;
}

size_t menu_update_fullscreen_thumbnail_label(
      char *s, size_t len,
      bool is_quick_menu, const char *title)
{
   char tmpstr[64];
   menu_entry_t selected_entry;
   struct menu_state *menu_st      = &menu_driver_state;
   const char *thumbnail_label     = NULL;

   /* > Get menu entry */
   MENU_ENTRY_INITIALIZE(selected_entry);
   selected_entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED
                         | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
   menu_entry_get(&selected_entry, 0, menu_st->selection_ptr, NULL, true);

   /* > Get entry label */
   if (!string_is_empty(selected_entry.rich_label))
      thumbnail_label = selected_entry.rich_label;
   /* > State slot label */
   else if (   is_quick_menu
            && (
               string_is_equal(selected_entry.label, "state_slot")
            || string_is_equal(selected_entry.label, "loadstate")
            || string_is_equal(selected_entry.label, "savestate")
               )
           )
   {
      size_t _len = strlcpy(tmpstr, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STATE_SLOT),
            sizeof(tmpstr));
      snprintf(tmpstr + _len, sizeof(tmpstr) - _len, " %d",
            config_get_ptr()->ints.state_slot);
      thumbnail_label = tmpstr;
   }
   else if (   is_quick_menu
            && (
               string_is_equal(selected_entry.label, "replay_slot")
            || string_is_equal(selected_entry.label, "record_replay")
            || string_is_equal(selected_entry.label, "play_replay")
            || string_is_equal(selected_entry.label, "halt_replay")
         ))
   {
      size_t _len = strlcpy(tmpstr, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REPLAY_SLOT),
            sizeof(tmpstr));
      snprintf(tmpstr + _len, sizeof(tmpstr) - _len, " %d",
               config_get_ptr()->ints.replay_slot);
      thumbnail_label = tmpstr;
   }
   else if (string_to_unsigned(selected_entry.label) == MENU_ENUM_LABEL_STATE_SLOT)
   {
      size_t _len = strlcpy(tmpstr, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STATE_SLOT),
            sizeof(tmpstr));
      snprintf(tmpstr + _len, sizeof(tmpstr) - _len, " %d",
            string_to_unsigned(selected_entry.path));
      thumbnail_label = tmpstr;
   }
   /* > Quick Menu playlist label */
   else if (is_quick_menu && title)
      thumbnail_label = title;
   else
      thumbnail_label = selected_entry.path;

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      return strlcpy(s, thumbnail_label, len);
   return 0;
}

bool menu_is_running_quick_menu(void)
{
   menu_entry_t entry;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
   menu_entry_get(&entry, 0, 0, NULL, true);

   return    string_is_equal(entry.label, "resume_content")
          || string_is_equal(entry.label, "state_slot");
}

bool menu_is_nonrunning_quick_menu(void)
{
   menu_entry_t entry;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED;
   menu_entry_get(&entry, 0, 0, NULL, true);

   return string_is_equal(entry.label, "collection");
}

void menu_driver_set_thumbnail_system(void *data, char *s, size_t len)
{
   struct menu_state               *menu_st = &menu_driver_state;
   gfx_thumbnail_set_system(
         menu_st->thumbnail_path_data, s, playlist_get_cached());
}

size_t menu_driver_get_thumbnail_system(void *data, char *s, size_t len)
{
   const char *system         = NULL;
   struct menu_state *menu_st = &menu_driver_state;
   if (!gfx_thumbnail_get_system(menu_st->thumbnail_path_data, &system))
      return 0;
   return strlcpy(s, system, len);
}

#ifdef HAVE_RUNAHEAD
/**
 * menu_update_runahead_mode
 *
 * Updates the menu runahead mode to match current settings for runahead
 * and preemptive frames.
 */
void menu_update_runahead_mode(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (settings->bools.run_ahead_enabled)
   {
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
      if (settings->bools.run_ahead_secondary_instance)
         menu_st->runahead_mode = MENU_RUNAHEAD_MODE_SECOND_INSTANCE;
      else
#endif
         menu_st->runahead_mode = MENU_RUNAHEAD_MODE_SINGLE_INSTANCE;
   }
   else if (settings->bools.preemptive_frames_enable)
      menu_st->runahead_mode = MENU_RUNAHEAD_MODE_PREEMPTIVE_FRAMES;
   else
      menu_st->runahead_mode = MENU_RUNAHEAD_MODE_OFF;
}
#endif
