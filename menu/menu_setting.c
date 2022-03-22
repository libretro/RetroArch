/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <libretro.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <audio/audio_resampler.h>

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#include "../config.def.h"
#include "../config.def.keybinds.h"

#if !defined(__PSL1GHT__) && defined(__PS3__)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_TRANSLATE
#include "../translation_defines.h"
#endif

#include "../frontend/frontend_driver.h"

#include "menu_input_bind_dialog.h"

#include "menu_setting.h"
#include "menu_cbs.h"
#include "menu_driver.h"
#include "../camera/camera_driver.h"
#include "../gfx/gfx_animation.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif
#include "menu_input.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu_shader.h"
#endif

#include "../core.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../defaults.h"
#include "../driver.h"
#include "../paths.h"
#include "../dynamic.h"
#include "../list_special.h"
#include "../audio/audio_driver.h"
#ifdef HAVE_BLUETOOTH
#include "../bluetooth/bluetooth_driver.h"
#endif
#include "../midi_driver.h"
#include "../location_driver.h"
#include "../record/record_driver.h"
#include "../tasks/tasks_internal.h"
#include "../config.def.h"
#include "../ui/ui_companion_driver.h"
#include "../performance_counters.h"
#include "../setting_list.h"
#include "../lakka.h"
#include "../retroarch.h"
#include "../gfx/video_display_server.h"
#ifdef HAVE_CHEATS
#include "../cheat_manager.h"
#endif
#include "../verbosity.h"
#include "../playlist.h"
#include "../manual_content_scan.h"
#include "../input/input_remapping.h"

#include "../tasks/tasks_internal.h"

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#ifdef HAVE_WIFI
#include "../network/wifi_driver.h"
#endif
#endif

#ifdef HAVE_VIDEO_LAYOUT
#include "../gfx/video_layout.h"
#endif

#if defined(HAVE_OVERLAY)
#include "../input/input_overlay.h"
#endif

/* Required for 3DS display mode setting */
#if defined(_3DS)
#include "gfx/common/ctr_common.h"
#include <3ds/services/cfgu.h>
#endif

#if defined(DINGUX)
#include "../dingux/dingux_utils.h"
#endif

#if defined(ANDROID)
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#define _3_SECONDS  3000000
#define _6_SECONDS  6000000
#define _9_SECONDS  9000000
#define _12_SECONDS 12000000
#define _15_SECONDS 15000000
#define _18_SECONDS 18000000
#define _21_SECONDS 21000000

#define CONFIG_SIZE(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_size(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_BOOL_ALT(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_bool_alt(a, b, c, d, e, f, g, h, i, j, k, l, m, n)

#define CONFIG_BOOL(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_bool(a, b, c, d, e, f, g, h, i, j, k, l, m, n)

#define CONFIG_INT(a, b, c, d, e, f, g, h, i, j, k) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_int(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_UINT_ALT(a, b, c, d, e, f, g, h, i, j, k) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_uint_alt(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_UINT(a, b, c, d, e, f, g, h, i, j, k) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_uint(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_STRING(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_string(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_FLOAT(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_float(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_DIR(a, b, c, d, e, f, g, h, i, j, k, l, m) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_dir(a, b, c, d, e, f, g, h, i, j, k, l, m)

#define CONFIG_PATH(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_path(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_ACTION_ALT(a, b, c, d, e, f, g) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_action_alt(a, b, c, d, e, f, g)

#define CONFIG_ACTION(a, b, c, d, e, f, g) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_action(a, b, c, d, e, f, g)

#define END_GROUP(a, b, c) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      end_group(a, b, c)

#define START_SUB_GROUP(a, b, c, d, e, f) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      start_sub_group(a, b, c, d, e, f)

#define END_SUB_GROUP(a, b, c) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      end_sub_group(a, b, c)

#define CONFIG_STRING_OPTIONS(a, b, c, d, e, f, g, h, i, j, k, l, m) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_string_options(a, b, c, d, e, f, g, h, i, j, k, l, m)

#define CONFIG_HEX(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_hex(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_BIND_ALT(a, b, c, d, e, f, g, h, i, j, k) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_bind_alt(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_BIND(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_bind(a, b, c, d, e, f, g, h, i, j, k, l)

#define SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(a, b, c) \
   (*a)[b->index - 1].free_flags |= c

#define MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(a, b, c) \
   (*a)[b->index - 1].enum_idx = c

#define MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(a, b, c) \
   (*a)[b->index - 1].enum_value_idx = c

#define SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(a, b, c) \
{ \
   (*a)[b->index - 1].flags |= c; \
   setting_add_special_callbacks(a, b, c); \
}

#define SETTINGS_LIST_APPEND(a, b) ((a && *a && b) ? ((((b)->index == (b)->size)) ? SETTINGS_LIST_APPEND_internal(a, b) : true) : false)

#define MENU_SETTINGS_LIST_CURRENT_IDX(list_info) (list_info->index - 1)

#define MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, str) ((*(list))[MENU_SETTINGS_LIST_CURRENT_IDX((list_info))].values = (str))

#define MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, str) (*(list))[MENU_SETTINGS_LIST_CURRENT_IDX(list_info)].cmd_trigger_idx = (str)

#define CONFIG_UINT_CBS(var, label, left, right, msg_enum_base, string_rep, min, max, step) \
      CONFIG_UINT( \
            list, list_info, \
            &(var), \
            MENU_ENUM_LABEL_##label, \
            MENU_ENUM_LABEL_VALUE_##label, \
            var, \
            &group_info, \
            &subgroup_info, \
            parent_group, \
            general_write_handler, \
            general_read_handler); \
      (*list)[list_info->index - 1].action_left = left; \
      (*list)[list_info->index - 1].action_right = right; \
      (*list)[list_info->index - 1].get_string_representation = string_rep; \
      (*list)[list_info->index - 1].index_offset = msg_enum_base; \
      menu_settings_list_current_add_range(list, list_info, min, max, step, true, true);

enum settings_list_type
{
   SETTINGS_LIST_NONE = 0,
   SETTINGS_LIST_MAIN_MENU,
   SETTINGS_LIST_DRIVERS,
   SETTINGS_LIST_CORE,
   SETTINGS_LIST_CONFIGURATION,
   SETTINGS_LIST_LOGGING,
   SETTINGS_LIST_SAVING,
   SETTINGS_LIST_REWIND,
   SETTINGS_LIST_CHEAT_DETAILS,
   SETTINGS_LIST_CHEAT_SEARCH,
   SETTINGS_LIST_CHEATS,
   SETTINGS_LIST_VIDEO,
   SETTINGS_LIST_CRT_SWITCHRES,
   SETTINGS_LIST_AUDIO,
   SETTINGS_LIST_INPUT,
   SETTINGS_LIST_INPUT_TURBO_FIRE,
   SETTINGS_LIST_INPUT_HOTKEY,
   SETTINGS_LIST_RECORDING,
   SETTINGS_LIST_FRAME_THROTTLING,
   SETTINGS_LIST_FRAME_TIME_COUNTER,
   SETTINGS_LIST_ONSCREEN_NOTIFICATIONS,
   SETTINGS_LIST_OVERLAY,
#ifdef HAVE_VIDEO_LAYOUT
   SETTINGS_LIST_VIDEO_LAYOUT,
#endif
   SETTINGS_LIST_MENU,
   SETTINGS_LIST_MENU_FILE_BROWSER,
   SETTINGS_LIST_MULTIMEDIA,
   SETTINGS_LIST_AI_SERVICE,
   SETTINGS_LIST_ACCESSIBILITY,
   SETTINGS_LIST_USER_INTERFACE,
   SETTINGS_LIST_POWER_MANAGEMENT,
   SETTINGS_LIST_WIFI_MANAGEMENT,
   SETTINGS_LIST_MENU_SOUNDS,
   SETTINGS_LIST_PLAYLIST,
   SETTINGS_LIST_CHEEVOS,
   SETTINGS_LIST_CORE_UPDATER,
   SETTINGS_LIST_NETPLAY,
   SETTINGS_LIST_LAKKA_SERVICES,
   SETTINGS_LIST_USER,
   SETTINGS_LIST_USER_ACCOUNTS,
   SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
   SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE,
   SETTINGS_LIST_USER_ACCOUNTS_TWITCH,
   SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK,
   SETTINGS_LIST_DIRECTORY,
   SETTINGS_LIST_PRIVACY,
   SETTINGS_LIST_MIDI,
   SETTINGS_LIST_MANUAL_CONTENT_SCAN
};

struct bool_entry
{
   bool *target;
   uint32_t flags;
   enum msg_hash_enums name_enum_idx;
   enum msg_hash_enums SHORT_enum_idx;
   enum msg_hash_enums off_enum_idx;
   enum msg_hash_enums on_enum_idx;
   bool default_value;
};

struct string_options_entry
{
   const char *default_value;
   const char *values;
   char *target;
   size_t len;
   enum msg_hash_enums name_enum_idx;
   enum msg_hash_enums SHORT_enum_idx;
};

typedef struct rarch_setting_info
{
   int index;
   int size;
} rarch_setting_info_t;

/* SETTINGS LIST */

static void menu_input_st_uint_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      const char *ptr          = NULL;
      unsigned value           = 0;
      int chars_read           = 0;
      int ret                  = 0;
      bool minus_found         = false;

      /* Ensure that input string contains a valid
       * unsigned value
       * Note: sscanf() will accept negative number
       * strings here and overflow, so have to check
       * for minus characters first... */
      for (ptr = str; *ptr != '\0'; ptr++)
      {
         if (*ptr == '-')
         {
            minus_found = true;
            break;
         }
      }

      if (!minus_found)
         ret = sscanf(str, "%u %n", &value, &chars_read);

      if ((ret == 1) && !str[chars_read])
         setting_set_with_string_representation(setting, str);
   }

   menu_input_dialog_end();
}

static void menu_input_st_int_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      int value                = 0;
      int chars_read           = 0;
      int ret                  = 0;

      /* Ensure that input string contains a valid
       * unsigned value */
      ret = sscanf(str, "%d %n", &value, &chars_read);

      if ((ret == 1) && !str[chars_read])
         setting_set_with_string_representation(setting, str);
   }

   menu_input_dialog_end();
}

/* TODO/FIXME - get rid of this eventually */
static void *setting_get_ptr(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;

   switch (setting->type)
   {
      case ST_BOOL:
         return setting->value.target.boolean;
      case ST_INT:
         return setting->value.target.integer;
      case ST_UINT:
         return setting->value.target.unsigned_integer;
      case ST_SIZE:
         return setting->value.target.sizet;
      case ST_FLOAT:
         return setting->value.target.fraction;
      case ST_BIND:
         return setting->value.target.keybind;
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_PATH:
      case ST_DIR:
         return setting->value.target.string;
      default:
         break;
   }

   return NULL;
}


static void menu_input_st_hex_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      if (setting)
      {
         unsigned *ptr = (unsigned*)setting_get_ptr(setting);
         if (str[0] == '#')
            str++;
         if (ptr)
            *ptr = (unsigned)strtoul(str, NULL, 16);
      }
   }

   menu_input_dialog_end();
}

static void menu_input_st_float_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      float value              = 0.0f;
      int chars_read           = 0;
      int ret                  = 0;

      /* Ensure that input string contains a valid
       * floating point value */
      ret = sscanf(str, "%f %n", &value, &chars_read);

      if ((ret == 1) && !str[chars_read])
         setting_set_with_string_representation(setting, str);
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t *setting = NULL;
      const char        *label = menu_input_dialog_get_label_setting_buffer();

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, 0, false);
      }
   }

   menu_input_dialog_end();
}


static int setting_generic_action_ok_linefeed(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   menu_input_ctx_line_t line;
   input_keyboard_line_complete_t cb = NULL;

   if (!setting)
      return -1;

   (void)wraparound;

   switch (setting->type)
   {
      case ST_SIZE:
      case ST_UINT:
         cb = menu_input_st_uint_cb;
         break;
      case ST_INT:
         cb = menu_input_st_int_cb;
         break;
      case ST_HEX:
         cb = menu_input_st_hex_cb;
         break;
      case ST_FLOAT:
         cb = menu_input_st_float_cb;
         break;
      case ST_STRING:
      case ST_STRING_OPTIONS:
         cb = menu_input_st_string_cb;
         break;
      default:
         break;
   }

   line.label         = setting->short_description;
   line.label_setting = setting->name;
   line.type          = 0;
   line.idx           = 0;
   line.cb            = cb;

   if (!menu_input_dialog_start(&line))
      return -1;
   return 0;
}

static void setting_add_special_callbacks(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   unsigned idx = list_info->index - 1;

   if (values & SD_FLAG_ALLOW_INPUT)
   {
      (*list)[idx].action_ok     = setting_generic_action_ok_linefeed;
      (*list)[idx].action_select = setting_generic_action_ok_linefeed;

      switch ((*list)[idx].type)
      {
         case ST_SIZE:
         case ST_UINT:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_INT:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_HEX:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_FLOAT:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_STRING:
            (*list)[idx].action_start  = setting_string_action_start_generic;
            (*list)[idx].action_cancel = NULL;
            break;
         default:
            break;
      }
   }
}

static bool SETTINGS_LIST_APPEND_internal(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   unsigned new_size              = list_info->size * 2;
   rarch_setting_t *list_settings = (rarch_setting_t*)
      realloc(*list, sizeof(rarch_setting_t) * new_size);

   if (!list_settings)
      return false;
   list_info->size                = new_size;
   *list                          = list_settings;

   return true;
}

unsigned setting_get_bind_type(rarch_setting_t *setting)
{
   if (!setting)
      return 0;
   return setting->bind_type;
}

static int setting_bind_action_ok(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   (void)wraparound; /* TODO/FIXME - handle this */

   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_SINGLE, setting))
      return -1;
   return 0;
}

static int setting_int_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               max = 0.0f;

   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.integer =
      *setting->value.target.integer + setting->step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.integer > max)
      {
         settings_t *settings = config_get_ptr();
         double          min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.integer = min;
         else
            *setting->value.target.integer = max;
      }
   }

   return 0;
}

static int setting_bind_action_start(rarch_setting_t *setting)
{
   unsigned bind_type;
   struct retro_keybind *keybind   = NULL;
   struct retro_keybind *def_binds = (struct retro_keybind *)retro_keybinds_1;

   if (!setting)
      return -1;

   keybind = (struct retro_keybind*)setting->value.target.keybind;
   if (!keybind)
      return -1;

   keybind->joykey  = NO_BTN;
   keybind->joyaxis = AXIS_NONE;

   /* Clear old mapping bit */
   input_keyboard_mapping_bits(0, keybind->key);

   if (setting->index_offset)
      def_binds = (struct retro_keybind*)retro_keybinds_rest;

   bind_type    = setting_get_bind_type(setting);
   keybind->key = def_binds[bind_type - MENU_SETTINGS_BIND_BEGIN].key;

   keybind->mbutton = NO_BTN;

   /* Store new mapping bit */
   input_keyboard_mapping_bits(1, keybind->key);

   return 0;
}

#if 0
static void setting_get_string_representation_hex(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%08x",
            *setting->value.target.unsigned_integer);
}
#endif

void setting_get_string_representation_hex_and_uint(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%u (%08X)",
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
}

void setting_get_string_representation_uint(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
}

void setting_get_string_representation_size(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%" PRI_SIZET,
            *setting->value.target.sizet);
}

void setting_get_string_representation_size_in_mb(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%" PRI_SIZET,
            (*setting->value.target.sizet)/(1024*1024));
}

#ifdef HAVE_CHEATS
static void setting_get_string_representation_uint_as_enum(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%s",
            msg_hash_to_str((enum msg_hash_enums)(
               setting->index_offset+(
                  *setting->value.target.unsigned_integer))));
}
#endif

static float recalc_step_based_on_length_of_action(rarch_setting_t *setting)
{
   float                     step = setting->step;
   struct menu_state *menu_st     = menu_state_get_ptr();
   retro_time_t action_press_time = menu_st->action_press_time;
   if      (action_press_time  > _21_SECONDS)
      return step * 1000000.0f;
   else if (action_press_time  > _18_SECONDS)
      return step * 100000.0f;
   else if (action_press_time  > _15_SECONDS)
      return step * 10000.0f;
   else if (action_press_time  > _12_SECONDS)
      return step * 1000.0f;
   else if (action_press_time  > _9_SECONDS)
      return step * 100.0f;
   else if (action_press_time  > _6_SECONDS)
      return step * 10.0f;
   else if (action_press_time  > _3_SECONDS)
      return step * 5.0f;
   return step;
}

int setting_uint_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               min        = 0.0f;
   bool                 overflowed = false;
   float                step       = 0.0f;

   if (!setting)
      return -1;

   min = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */

   step       = recalc_step_based_on_length_of_action(setting);
   overflowed = step > *setting->value.target.unsigned_integer;

   if (!overflowed)
      *setting->value.target.unsigned_integer =
         *setting->value.target.unsigned_integer - step;

   if (setting->enforce_minrange)
   {
      if (overflowed || *setting->value.target.unsigned_integer < min)
      {
         settings_t *settings = config_get_ptr();
         double           max = setting->max;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.unsigned_integer = max;
         else
            *setting->value.target.unsigned_integer = min;
      }
   }

   return 0;
}

int setting_uint_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               max  = 0.0f;
   float                step = 0.0f;

   if (!setting)
      return -1;

   max                       = setting->max;
   step                      =
      recalc_step_based_on_length_of_action(setting);

   *setting->value.target.unsigned_integer =
      *setting->value.target.unsigned_integer + step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.unsigned_integer > max)
      {
         settings_t *settings = config_get_ptr();
         double           min = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.unsigned_integer = min;
         else
            *setting->value.target.unsigned_integer = max;
      }
   }

   return 0;
}

int setting_bool_action_right_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh      = false;

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return 0;
}

int setting_uint_action_right_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int retval        = setting_uint_action_right_default(setting, idx, wraparound);
   bool refresh      = false;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return retval;
}

int setting_bool_action_left_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh      = false;

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return 0;
}

int setting_uint_action_left_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int retval = setting_uint_action_left_default(setting, idx, wraparound);
   bool refresh      = false;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return retval;

}

static int setting_size_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               min        = 0.0f;
   bool                 overflowed = false;
   float                step       = 0.0f;

   if (!setting)
      return -1;

   min = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */

   step       = recalc_step_based_on_length_of_action(setting);
   overflowed = step > *setting->value.target.sizet;

   if (!overflowed)
      *setting->value.target.sizet = *setting->value.target.sizet - step;

   if (setting->enforce_minrange)
   {
      if (overflowed || *setting->value.target.sizet < min)
      {
         settings_t *settings = config_get_ptr();
         double           max = setting->max;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.sizet = max;
         else
            *setting->value.target.sizet = min;
      }
   }

   return 0;
}

static int setting_size_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               max  = 0.0f;
   float                step = 0.0f;

   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */

   step = recalc_step_based_on_length_of_action(setting);

   *setting->value.target.sizet =
      *setting->value.target.sizet + step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.sizet > max)
      {
         settings_t *settings = config_get_ptr();
         double           min = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.sizet = min;
         else
            *setting->value.target.sizet = max;
      }
   }

   return 0;
}

int setting_generic_action_ok_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   if (setting->cmd_trigger_idx != CMD_EVENT_NONE)
      setting->cmd_trigger_event_triggered = true;

   return 0;
}

void setting_generic_handle_change(rarch_setting_t *setting)
{
   settings_t *settings = config_get_ptr();
   settings->modified   = true;

   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->cmd_trigger_idx && !setting->cmd_trigger_event_triggered)
      command_event(setting->cmd_trigger_idx, NULL);
}


static void setting_get_string_representation_int_gpu_index(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      struct string_list *list = video_driver_get_gpu_api_devices(video_context_driver_get_api());

      if (list && (*setting->value.target.integer < (int)list->size) && !string_is_empty(list->elems[*setting->value.target.integer].data))
         snprintf(s, len, "%d - %s", *setting->value.target.integer, list->elems[*setting->value.target.integer].data);
      else
         snprintf(s, len, "%d", *setting->value.target.integer);
   }
}

static void setting_get_string_representation_int(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%d", *setting->value.target.integer);
}

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
int setting_set_with_string_representation(rarch_setting_t* setting,
      const char* value)
{
   double min, max;
   uint64_t flags;
   if (!setting || !value)
      return -1;

   min          = setting->min;
   max          = setting->max;
   flags        = setting->flags;

   switch (setting->type)
   {
      case ST_INT:
         sscanf(value, "%d", setting->value.target.integer);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.integer < min)
               *setting->value.target.integer = min;
            if (setting->enforce_maxrange && *setting->value.target.integer > max)
            {
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.integer = min;
               else
                  *setting->value.target.integer = max;
            }
         }
         break;
      case ST_UINT:
         sscanf(value, "%u", setting->value.target.unsigned_integer);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.unsigned_integer < min)
               *setting->value.target.unsigned_integer = min;
            if (setting->enforce_maxrange && *setting->value.target.unsigned_integer > max)
            {
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.unsigned_integer = min;
               else
                  *setting->value.target.unsigned_integer = max;
            }
         }
         break;
      case ST_SIZE:
         sscanf(value, "%" PRI_SIZET, setting->value.target.sizet);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.sizet < min)
               *setting->value.target.sizet = min;
            if (setting->enforce_maxrange && *setting->value.target.sizet > max)
            {
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.sizet = min;
               else
                  *setting->value.target.sizet = max;
            }
         }
         break;
      case ST_FLOAT:
         sscanf(value, "%f", setting->value.target.fraction);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.fraction < min)
               *setting->value.target.fraction = min;
            if (setting->enforce_maxrange && *setting->value.target.fraction > max)
            {
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.fraction = min;
               else
                  *setting->value.target.fraction = max;
            }
         }
         break;
      case ST_PATH:
      case ST_DIR:
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_ACTION:
         if (setting->value.target.string)
            strlcpy(setting->value.target.string, value, setting->size);
         break;
      case ST_BOOL:
         if (string_is_equal(value, "true"))
            *setting->value.target.boolean = true;
         else if (string_is_equal(value, "false"))
            *setting->value.target.boolean = false;
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int setting_fraction_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               min = 0.0f;

   if (!setting)
      return -1;

   min = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.fraction = *setting->value.target.fraction - setting->step;

   if (setting->enforce_minrange)
   {
      if (*setting->value.target.fraction < min)
      {
         settings_t *settings = config_get_ptr();
         double           max = setting->max;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.fraction = max;
         else
            *setting->value.target.fraction = min;
      }
   }

   return 0;
}

static int setting_fraction_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               max = 0.0f;

   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.fraction =
      *setting->value.target.fraction + setting->step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.fraction > max)
      {
         settings_t *settings = config_get_ptr();
         double          min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.fraction = min;
         else
            *setting->value.target.fraction = max;
      }
   }

   return 0;
}

/**
 * setting_reset_setting:
 * @setting            : pointer to setting
 *
 * Reset a setting's value to its defaults.
 **/
static void setting_reset_setting(rarch_setting_t* setting)
{
   if (!setting)
      return;

   switch (setting->type)
   {
      case ST_BOOL:
         *setting->value.target.boolean          = setting->default_value.boolean;
         break;
      case ST_INT:
         *setting->value.target.integer          = setting->default_value.integer;
         break;
      case ST_UINT:
         *setting->value.target.unsigned_integer = setting->default_value.unsigned_integer;
         break;
      case ST_SIZE:
         *setting->value.target.sizet            = setting->default_value.sizet;
         break;
      case ST_FLOAT:
         *setting->value.target.fraction         = setting->default_value.fraction;
         break;
      case ST_BIND:
         *setting->value.target.keybind          = *setting->default_value.keybind;
         break;
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_PATH:
      case ST_DIR:
         if (setting->default_value.string)
         {
            if (setting->type == ST_STRING)
               setting_set_with_string_representation(setting, setting->default_value.string);
            else
               fill_pathname_expand_special(setting->value.target.string,
                     setting->default_value.string, setting->size);
         }
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

int setting_generic_action_start_default(rarch_setting_t *setting)
{
   bool refresh                = false;
   if (!setting)
      return -1;

   setting_reset_setting(setting);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static void setting_get_string_representation_default(rarch_setting_t *setting,
      char *s, size_t len)
{
   strcpy_literal(s, "...");
}

/**
 * setting_get_string_representation_st_bool:
 * @setting            : pointer to setting
 * @s                  : string for the type to be represented on-screen as
 *                       a label.
 * @len                : size of @s
 *
 * Set a settings' label value. The setting is of type ST_BOOL.
 **/
static void setting_get_string_representation_st_bool(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, *setting->value.target.boolean ? setting->boolean.on_label :
            setting->boolean.off_label, len);
}

/**
 * setting_get_string_representation_st_float:
 * @setting            : pointer to setting
 * @s                  : string for the type to be represented on-screen as
 *                       a label.
 * @len                : size of @s
 *
 * Set a settings' label value. The setting is of type ST_FLOAT.
 **/
static void setting_get_string_representation_st_float(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, setting->rounding_fraction,
            *setting->value.target.fraction);
}

static void setting_get_string_representation_st_dir(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s,
            *setting->value.target.string ?
            setting->value.target.string : setting->dir.empty_path,
            len);
}

static void setting_get_string_representation_st_path(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      fill_short_pathname_representation(s, setting->value.target.string, len);
}

static void setting_get_string_representation_st_string(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, setting->value.target.string, len);
}

static void setting_get_string_representation_st_bind(rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned index_offset                 = 0;
   const struct retro_keybind* keybind   = NULL;
   const struct retro_keybind* auto_bind = NULL;
   settings_t *settings                  = config_get_ptr();

   if (!setting)
      return;

   index_offset = setting->index_offset;
   keybind      = (const struct retro_keybind*)setting->value.target.keybind;
   auto_bind    = (const struct retro_keybind*)
      input_config_get_bind_auto(index_offset, keybind->id);

   input_config_get_bind_string(settings, s, keybind, auto_bind, len);
}

static int setting_action_action_ok(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   if (setting->cmd_trigger_idx != CMD_EVENT_NONE)
      command_event(setting->cmd_trigger_idx, NULL);

   return 0;
}

/**
 * setting_action_setting:
 * @name               : Name of setting.
 * @short_description  : Short description of setting.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_ACTION.
 *
 * Returns: setting of type ST_ACTION.
 **/
static rarch_setting_t setting_action_setting(const char* name,
      const char* short_description,
      const char *group, const char *subgroup,
      const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_ACTION;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_action_action_ok;
   result.action_select             = setting_action_action_ok;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_group_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 *
 * Initializes a setting of type ST_GROUP.
 *
 * Returns: setting of type ST_GROUP.
 **/
static rarch_setting_t setting_group_setting(
      enum setting_type type, const char* name,
      const char *parent_group)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = name;
   result.group                     = NULL;
   result.subgroup                  = NULL;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = false;

   return result;
}

/**
 * setting_float_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of float setting.
 * @default_value      : Default value (in float).
 * @rounding           : Rounding (for float-to-string representation).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_FLOAT.
 *
 * Returns: setting of type ST_FLOAT.
 **/
static rarch_setting_t setting_float_setting(const char* name,
      const char* short_description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_FLOAT;

   result.size                      = sizeof(float);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_fraction_action_left_default;
   result.action_right              = setting_fraction_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_st_float;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = rounding;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.fraction     = target;
   result.original_value.fraction   = *target;
   result.default_value.fraction    = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_uint_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of unsigned integer setting.
 * @default_value      : Default value (in unsigned integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_UINT.
 *
 * Returns: setting of type ST_UINT.
 **/
static rarch_setting_t setting_uint_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_UINT;

   result.size                      = sizeof(unsigned int);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_uint_action_left_default;
   result.action_right              = setting_uint_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_uint;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.unsigned_integer   = target;
   result.original_value.unsigned_integer = *target;
   result.default_value.unsigned_integer  = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_size_setting:
 * @name                          : name of setting.
 * @short_description             : Short description of setting.
 * @target                        : Target of size_t setting.
 * @default_value                 : Default value (in size_t format).
 * @group                         : Group that the setting belongs to.
 * @subgroup                      : Subgroup that the setting belongs to.
 * @change_handler                : Function callback for change handler function pointer.
 * @read_handler                  : Function callback for read handler function pointer.
 * @dont_use_enum_idx             : Boolean indicating whether or not to use the enum idx
 * @string_representation_handler : Function callback for converting the setting to a string
 *
 * Initializes a setting of type ST_SIZE.
 *
 * Returns: setting of type ST_SIZE.
 **/
static rarch_setting_t setting_size_setting(const char* name,
      const char* short_description, size_t* target,
      size_t default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx, get_string_representation_t string_representation_handler)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_SIZE;

   result.size                      = sizeof(size_t);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_size_action_left_default;
   result.action_right              = setting_size_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = string_representation_handler;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.sizet   = target;
   result.original_value.sizet = *target;
   result.default_value.sizet  = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_hex_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of unsigned integer setting.
 * @default_value      : Default value (in unsigned integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_HEX.
 *
 * Returns: setting of type ST_HEX.
 **/
#if 0
static rarch_setting_t setting_hex_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_HEX;

   result.size                      = sizeof(unsigned int);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_hex;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.unsigned_integer   = target;
   result.original_value.unsigned_integer = *target;
   result.default_value.unsigned_integer  = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}
#endif

/**
 * setting_bind_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @idx                : Index of bind setting.
 * @idx_offset         : Index offset of bind setting.
 * @default_value      : Default value (in bind format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_BIND.
 *
 * Returns: setting of type ST_BIND.
 **/
static rarch_setting_t setting_bind_setting(const char* name,
      const char* short_description, struct retro_keybind* target,
      uint32_t idx, uint32_t idx_offset,
      const struct retro_keybind* default_value,
      const char *group, const char *subgroup,
      const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BIND;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = idx;
   result.index_offset              = idx_offset;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = setting_bind_action_start;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_bind_action_ok;
   result.action_select             = setting_bind_action_ok;
   result.get_string_representation = &setting_get_string_representation_st_bind;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.keybind      = target;
   result.default_value.keybind     = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

static int setting_int_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double               min = 0.0f;

   if (!setting)
      return -1;

   min = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.integer = *setting->value.target.integer - setting->step;

   if (setting->enforce_minrange)
   {
      if (*setting->value.target.integer < min)
      {
         settings_t *settings = config_get_ptr();
         double           max = setting->max;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.integer = max;
         else
            *setting->value.target.integer = min;
      }
   }

   return 0;
}

static int setting_bool_action_ok_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   return 0;
}

static int setting_bool_action_toggle_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   return 0;
}

int setting_string_action_start_generic(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   setting->value.target.string[0] = '\0';

   return 0;
}

/**
 * setting_string_setting:
 * @type               : type of setting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of string setting.
 * @size               : Size of string setting.
 * @default_value      : Default value (in string format).
 * @empty              : TODO/FIXME: ???
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string setting (of type @type).
 *
 * Returns: String setting of type @type.
 **/
static rarch_setting_t setting_string_setting(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = size;

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_st_string;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.dir.empty_path            = empty;
   result.value.target.string       = target;
   result.default_value.string      = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   switch (type)
   {
      case ST_DIR:
         result.action_start              = setting_string_action_start_generic;
         result.browser_selection_type    = ST_DIR;
         result.get_string_representation = &setting_get_string_representation_st_dir;
         break;
      case ST_PATH:
         result.action_start              = setting_string_action_start_generic;
         result.browser_selection_type    = ST_PATH;
         result.get_string_representation = &setting_get_string_representation_st_path;
         break;
      default:
         break;
   }

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_string_setting_options:
 * @type               : type of settting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @size               : Size of string setting.
 * @default_value      : Default value.
 * @empty              : N/A.
 * @values             : Values, separated by a delimiter.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string options list setting.
 *
 * Returns: string option list setting.
 **/
static rarch_setting_t setting_string_setting_options(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value,
      const char *empty, const char *values,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
  rarch_setting_t result = setting_string_setting(type, name,
        short_description, target, size, default_value, empty, group,
        subgroup, parent_group, change_handler, read_handler,
        dont_use_enum_idx);

  result.action_start    = setting_generic_action_start_default;

  result.parent_group    = parent_group;
  result.values          = values;
  return result;
}

/**
 * setting_subgroup_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 * @parent_name        : group that the subgroup setting belongs to.
 *
 * Initializes a setting of type ST_SUBGROUP.
 *
 * Returns: setting of type ST_SUBGROUP.
 **/
static rarch_setting_t setting_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name, const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = name;
   result.group                     = parent_name;
   result.parent_group              = parent_group;
   result.values                    = NULL;
   result.subgroup                  = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_bool_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bool setting.
 * @default_value      : Default value (in bool format).
 * @off                : String value for "Off" label.
 * @on                 : String value for "On"  label.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_BOOL.
 *
 * Returns: setting of type ST_BOOL.
 **/
static rarch_setting_t setting_bool_setting(const char* name,
      const char* short_description, bool* target, bool default_value,
      const char *off, const char *on,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BOOL;

   result.size                      = sizeof(bool);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_bool_action_toggle_default;
   result.action_right              = setting_bool_action_toggle_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_bool_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_st_bool;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.boolean      = target;
   result.original_value.boolean    = *target;
   result.default_value.boolean     = default_value;
   result.boolean.off_label         = off;
   result.boolean.on_label          = on;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_int_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of signed integer setting.
 * @default_value      : Default value (in signed integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_INT.
 *
 * Returns: setting of type ST_INT.
 **/
static rarch_setting_t setting_int_setting(const char* name,
      const char* short_description, int* target,
      int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_INT;

   result.size                      = sizeof(int);

   result.name                      = name;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_int_action_left_default;
   result.action_right              = setting_int_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_int;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.integer      = target;
   result.original_value.integer    = *target;
   result.default_value.integer     = default_value;

   result.cmd_trigger_idx                  = CMD_EVENT_NONE;
   result.cmd_trigger_event_triggered      = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

#ifdef HAVE_NETWORKING
static void config_bool_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      bool *target,
      const char *name, const char *SHORT,
      bool default_value,
      enum msg_hash_enums off_enum_idx,
      enum msg_hash_enums on_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      uint32_t flags)
{
   (*list)[list_info->index++] = setting_bool_setting(name, SHORT, target,
         default_value,
         msg_hash_to_str(off_enum_idx), msg_hash_to_str(on_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, true);
   if (flags != SD_FLAG_NONE)
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, flags);
}
#endif

static void config_bool(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      bool *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      bool default_value,
      enum msg_hash_enums off_enum_idx,
      enum msg_hash_enums on_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      uint32_t flags)
{
   (*list)[list_info->index++]             = setting_bool_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target,
         default_value,
         msg_hash_to_str(off_enum_idx),
         msg_hash_to_str(on_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_CHECKBOX;
   if (flags != SD_FLAG_NONE)
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, flags);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_int(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++] = setting_int_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_uint_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      const char *name, const char *SHORT,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++] = setting_uint_setting(
         name, SHORT, target, default_value,
         group_info->name,
         subgroup_info->name, parent_group, change_handler, read_handler,
         true);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
}

static void config_uint(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++]             = setting_uint_setting  (
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name,
         subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_size(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      size_t *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      size_t default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
	  get_string_representation_t string_representation_handler)
{
   (*list)[list_info->index++] = setting_size_setting  (
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name,
         subgroup_info->name, parent_group,
         change_handler, read_handler,
         false, string_representation_handler);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_SIZE_SPINBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_float(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      float default_value, const char *rounding,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++]             = setting_float_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx), target, default_value, rounding,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_FLOAT_SPINBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_path(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++]             = setting_string_setting(ST_PATH,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_FILE_SELECTOR;
   SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_EMPTY);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_dir(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      enum msg_hash_enums empty_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++]             = setting_string_setting(ST_DIR,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value,
         msg_hash_to_str(empty_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_DIRECTORY_SELECTOR;
   SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_string(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++] = setting_string_setting(ST_STRING,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_string_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value, const char *values,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++]                = setting_string_setting_options(
         ST_STRING_OPTIONS,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "", values,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   (*list)[list_info->index - 1].ui_type      = ST_UI_TYPE_STRING_COMBOBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
   /* Request values to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_VALUES);
}

#if 0
static void config_hex(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   (*list)[list_info->index++] = setting_hex_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

/* Please strdup() NAME and SHORT */
static void config_bind(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *target,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_bind_setting(name, SHORT, target,
         player, player_offset, default_value,
         group_info->name, subgroup_info->name, parent_group,
         false);
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
}
#endif

/* Please strdup() NAME and SHORT */
static void config_bind_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *target,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_bind_setting(name, SHORT, target,
         player, player_offset, default_value,
         group_info->name, subgroup_info->name, parent_group,
         true);
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
}

static void config_action_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *name, const char *SHORT,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_action_setting(name, SHORT,
         group_info->name, subgroup_info->name, parent_group,
         true);
}

static void config_action(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_action_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         group_info->name,
         subgroup_info->name, parent_group,
         false);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void START_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      rarch_setting_group_info_t *group_info,
      const char *name, const char *parent_group)
{
   group_info->name = name;
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;
   (*list)[list_info->index++] = setting_group_setting (ST_GROUP, name, parent_group);
}

static void end_group(rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_group_setting (ST_END_GROUP, 0, parent_group);
}

static bool start_sub_group(rarch_setting_t **list,
      rarch_setting_info_t *list_info, const char *name,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   subgroup_info->name = name;

   if (!SETTINGS_LIST_APPEND(list, list_info))
      return false;
   (*list)[list_info->index++] = setting_subgroup_setting (ST_SUB_GROUP,
         name, group_info->name, parent_group, false);
   return true;
}

static void end_sub_group(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   (*list)[list_info->index++] = setting_group_setting (ST_END_SUB_GROUP, 0, parent_group);
}

/* MENU SETTINGS */

static int setting_action_ok_bind_all(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   (void)wraparound;
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_ALL, setting))
      return -1;
   return 0;
}

#ifdef HAVE_CONFIGFILE
static int setting_action_ok_bind_all_save_autoconfig(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned index_offset     = 0;
   unsigned map              = 0;
   const char *name          = NULL;
   settings_t      *settings = config_get_ptr();

   (void)wraparound;

   if (!setting)
      return -1;

   index_offset = setting->index_offset;
   map          = settings->uints.input_joypad_index[index_offset];
   name         = input_config_get_device_name(map);

   if (!string_is_empty(name) &&
         config_save_autoconf_profile(name, index_offset))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY), 1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_ERROR_SAVING), 1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}
#endif

static int setting_action_ok_bind_defaults(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned i;
   menu_input_ctx_bind_limits_t lim;
   struct retro_keybind *target          = NULL;
   const struct retro_keybind *def_binds = NULL;

   (void)wraparound;

   if (!setting)
      return -1;

   target    =  &input_config_binds[setting->index_offset][0];
   def_binds =  (setting->index_offset) ?
      retro_keybinds_rest : retro_keybinds_1;

   lim.min   = MENU_SETTINGS_BIND_BEGIN;
   lim.max   = MENU_SETTINGS_BIND_LAST;

   menu_input_key_bind_set_min_max(&lim);

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      target->key     = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      target->joykey  = NO_BTN;
      target->joyaxis = AXIS_NONE;
      target->mbutton = NO_BTN;
   }

   return 0;
}

static int setting_action_ok_video_refresh_rate_auto(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;

   if (!setting)
      return -1;

   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      float video_refresh_rate_float = (float)video_refresh_rate;
      driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &video_refresh_rate_float);
      /* Incase refresh rate update forced non-block video. */
      command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
   }

   /* Send NULL instead of setting to prevent duplicate notifications */
   if (setting_generic_action_ok_default(NULL, idx, wraparound) != 0)
      return -1;

   return 0;
}

static int setting_action_ok_video_refresh_rate_polled(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   float refresh_rate = 0.0;

   if (!setting)
     return -1;

   if ((refresh_rate = video_driver_get_refresh_rate()) == 0.0)
      return -1;

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
   /* Incase refresh rate update forced non-block video. */
   command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);

   /* Send NULL instead of setting to prevent duplicate notifications */
   if (setting_generic_action_ok_default(NULL, idx, wraparound) != 0)
      return -1;

   return 0;
}

static int setting_action_ok_uint_special(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_SPECIAL);
   return 0;
}

static int setting_action_ok_uint(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST);
   return 1;
}

static int setting_action_ok_libretro_device_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE);
   return 0;
}

static int setting_action_ok_bind_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DEVICE_INDEX);
   return 0;
}

static int setting_string_action_left_string_options(
   rarch_setting_t* setting, size_t idx, bool wraparound)
{
   struct string_list tmp_str_list = { 0 };
   size_t i;

   if (!setting)
      return -1;

   string_list_initialize(&tmp_str_list);
   string_split_noalloc(&tmp_str_list,
      setting->values, "|");

   for (i = 0; i < tmp_str_list.size; ++i)
   {
      if (string_is_equal(tmp_str_list.elems[i].data, setting->value.target.string))
      {
         i = (i + tmp_str_list.size - 1) % tmp_str_list.size;
         strlcpy(setting->value.target.string,
            tmp_str_list.elems[i].data, setting->size);

         if (setting->change_handler)
            setting->change_handler(setting);

         string_list_deinitialize(&tmp_str_list);
         return 0;
      }
   }

   string_list_deinitialize(&tmp_str_list);
   return -1;
}

static int setting_string_action_right_string_options(
   rarch_setting_t* setting, size_t idx, bool wraparound)
{
   struct string_list tmp_str_list = { 0 };
   size_t i;

   if (!setting)
      return -1;

   string_list_initialize(&tmp_str_list);
   string_split_noalloc(&tmp_str_list,
      setting->values, "|");

   for (i = 0; i < tmp_str_list.size; ++i)
   {
      if (string_is_equal(tmp_str_list.elems[i].data, setting->value.target.string))
      {
         i = (i + 1) % tmp_str_list.size;
         strlcpy(setting->value.target.string,
            tmp_str_list.elems[i].data, setting->size);

         if (setting->change_handler)
            setting->change_handler(setting);

         string_list_deinitialize(&tmp_str_list);
         return 0;
      }
   }

   string_list_deinitialize(&tmp_str_list);
   return -1;
}

#if defined(HAVE_GFX_WIDGETS)
static int setting_action_ok_mapped_string(
   rarch_setting_t* setting, size_t idx, bool wraparound)
{
   /* this is functionally the same as setting_action_ok_uint.
    * the mapping happens in menu_displaylist_ctl */
   return setting_action_ok_uint(setting, idx, wraparound);
}
#endif

static void setting_get_string_representation_streaming_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   /* TODO/FIXME - localize this */
   switch (*setting->value.target.unsigned_integer)
   {
      case STREAMING_MODE_TWITCH:
         strcpy_literal(s, "Twitch");
         break;
      case STREAMING_MODE_YOUTUBE:
         strcpy_literal(s, "YouTube");
         break;
      case STREAMING_MODE_FACEBOOK:
         strcpy_literal(s, "Facebook Gaming");
         break;         
      case STREAMING_MODE_LOCAL:
         strlcpy(s, "Local", len);
         break;
      case STREAMING_MODE_CUSTOM:
         strlcpy(s, "Custom", len);
         break;
   }
}

static void setting_get_string_representation_video_stream_quality(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   /* TODO/FIXME - localize this */
   switch (*setting->value.target.unsigned_integer)
   {
      case RECORD_CONFIG_TYPE_STREAMING_CUSTOM:
         strlcpy(s, "Custom", len);
         break;
      case RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY:
         strlcpy(s, "Low", len);
         break;
      case RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY:
         strlcpy(s, "Medium", len);
         break;
      case RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY:
         strlcpy(s, "High", len);
         break;
   }
}

static void setting_get_string_representation_video_record_quality(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   /* TODO/FIXME - localize this */
   switch (*setting->value.target.unsigned_integer)
   {
      case RECORD_CONFIG_TYPE_RECORDING_CUSTOM:
         strlcpy(s, "Custom", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY:
         strlcpy(s, "Low", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY:
         strlcpy(s, "Medium", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY:
         strlcpy(s, "High", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY:
         strlcpy(s, "Lossless", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST:
         strlcpy(s, "WebM Fast", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY:
         strlcpy(s, "WebM High Quality", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_GIF:
         strlcpy(s, "GIF", len);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_APNG:
         strlcpy(s, "APNG", len);
         break;
   }
}

static void setting_get_string_representation_video_filter(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   fill_short_pathname_representation(s, setting->value.target.string, len);
}

static void setting_get_string_representation_state_slot(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   snprintf(s, len, "%d", *setting->value.target.integer);
   if (*setting->value.target.integer == -1)
      strlcat(s, " (Auto)", len);
}

static void setting_get_string_representation_percentage(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   snprintf(s, len, "%d%%", *setting->value.target.integer);
}

static void setting_get_string_representation_float_video_msg_color(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   snprintf(s, len, "%d", (int)(*setting->value.target.fraction * 255.0f));
}

static void setting_get_string_representation_max_users(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}

#ifdef HAVE_CHEEVOS
static void setting_get_string_representation_cheevos_password(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (!string_is_empty(setting->value.target.string))
      strcpy_literal(s, "********");
   else
   {
      settings_t *settings = config_get_ptr();
      if (settings->arrays.cheevos_token[0])
         strcpy_literal(s, "********");
      else
         *setting->value.target.string = '\0';
   }
}
#endif

#if TARGET_OS_IPHONE
static void setting_get_string_representation_uint_keyboard_gamepad_mapping_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
         break;
      case 1:
         strcpy_literal(s, "iPega PG-9017");
         break;
      case 2:
         strcpy_literal(s, "8-bitty");
         break;
      case 3:
         strcpy_literal(s, "SNES30 8bitdo");
         break;
   }
}
#endif

#ifdef HAVE_TRANSLATE
static void setting_get_string_representation_uint_ai_service_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         enum_idx = MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE;
         break;
      case 1:
         enum_idx = MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE;
         break;
      case 2:
         enum_idx = MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE;
         break;
      default:
         break;
   }

   if (enum_idx != 0)
      strlcpy(s, msg_hash_to_str(enum_idx), len);
}

static void setting_get_string_representation_uint_ai_service_lang(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case TRANSLATION_LANG_EN:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ENGLISH;
         break;
      case TRANSLATION_LANG_ES:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SPANISH;
         break;
      case TRANSLATION_LANG_FR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_FRENCH;
         break;
      case TRANSLATION_LANG_IT:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ITALIAN;
         break;
      case TRANSLATION_LANG_DE:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_GERMAN;
         break;
      case TRANSLATION_LANG_JP:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_JAPANESE;
         break;
      case TRANSLATION_LANG_NL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_DUTCH;
         break;
      case TRANSLATION_LANG_CS:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_CZECH;
         break;
      case TRANSLATION_LANG_DA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_DANISH;
         break;
         /* TODO/FIXME */
      case TRANSLATION_LANG_SV:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SWEDISH;
         break;
      case TRANSLATION_LANG_HR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_CROATIAN;
         break;
      case TRANSLATION_LANG_CA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_CATALAN;
         break;
      case TRANSLATION_LANG_AST:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ASTURIAN;
         break;
      case TRANSLATION_LANG_BG:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN;
         break;
      case TRANSLATION_LANG_BN:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_BENGALI;
         break;
      case TRANSLATION_LANG_EU:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_BASQUE;
         break;
      case TRANSLATION_LANG_AZ:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI;
         break;
      case TRANSLATION_LANG_SQ:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN;
         break;
      case TRANSLATION_LANG_AF:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS;
         break;
      case TRANSLATION_LANG_ET:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN;
         break;
      case TRANSLATION_LANG_TL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_FILIPINO;
         break;
      case TRANSLATION_LANG_FI:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_FINNISH;
         break;
      case TRANSLATION_LANG_GL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_GALICIAN;
         break;
      case TRANSLATION_LANG_KA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN;
         break;
      case TRANSLATION_LANG_GU:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_GUJARATI;
         break;
      case TRANSLATION_LANG_HT:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE;
         break;
      case TRANSLATION_LANG_HE:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_HEBREW;
         break;
      case TRANSLATION_LANG_HI:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_HINDI;
         break;
      case TRANSLATION_LANG_HU:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN;
         break;
      case TRANSLATION_LANG_IS:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC;
         break;
      case TRANSLATION_LANG_ID:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN;
         break;
      case TRANSLATION_LANG_GA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_IRISH;
         break;
      case TRANSLATION_LANG_KN:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_KANNADA;
         break;
      case TRANSLATION_LANG_LA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_LATIN;
         break;
      case TRANSLATION_LANG_LV:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_LATVIAN;
         break;
      case TRANSLATION_LANG_LT:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN;
         break;
      case TRANSLATION_LANG_MK:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN;
         break;
      case TRANSLATION_LANG_MS:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_MALAY;
         break;
      case TRANSLATION_LANG_MT:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_MALTESE;
         break;
      case TRANSLATION_LANG_NO:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN;
         break;
      case TRANSLATION_LANG_FA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_PERSIAN;
         break;
      case TRANSLATION_LANG_RO:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN;
         break;
      case TRANSLATION_LANG_SR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SERBIAN;
         break;
      case TRANSLATION_LANG_SK:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SLOVAK;
         break;
      case TRANSLATION_LANG_SL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN;
         break;
      case TRANSLATION_LANG_SW:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_SWAHILI;
         break;
      case TRANSLATION_LANG_TA:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_TAMIL;
         break;
      case TRANSLATION_LANG_TE:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_TELUGU;
         break;
      case TRANSLATION_LANG_TH:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_THAI;
         break;
      case TRANSLATION_LANG_UK:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN;
         break;
      case TRANSLATION_LANG_UR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_URDU;
         break;
      case TRANSLATION_LANG_CY:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_WELSH;
         break;
      case TRANSLATION_LANG_YI:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_YIDDISH;
         break;
      case TRANSLATION_LANG_RU:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN;
         break;
      case TRANSLATION_LANG_PT:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL;
         break;
      case TRANSLATION_LANG_TR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_TURKISH;
         break;
      case TRANSLATION_LANG_PL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_POLISH;
         break;
      case TRANSLATION_LANG_VI:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE;
         break;
      case TRANSLATION_LANG_EL:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_GREEK;
         break;
      case TRANSLATION_LANG_KO:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_KOREAN;
         break;
      case TRANSLATION_LANG_EO:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO;
         break;
      case TRANSLATION_LANG_AR:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_ARABIC;
         break;
      case TRANSLATION_LANG_ZH_CN:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED;
         break;
      case TRANSLATION_LANG_ZH_TW:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL;
         break;
      case TRANSLATION_LANG_DONT_CARE:
         enum_idx = MENU_ENUM_LABEL_VALUE_DONT_CARE;
         break;
      default:
         break;
   }

   if (enum_idx != 0)
      strlcpy(s, msg_hash_to_str(enum_idx), len);
   else
      snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}
#endif

static void setting_get_string_representation_uint_menu_thumbnails(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case 1:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS), len);
         break;
      case 2:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS), len);
         break;
      case 3:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS), len);
         break;
   }
}

static void setting_get_string_representation_uint_menu_left_thumbnails(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case 1:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS), len);
         break;
      case 2:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS), len);
         break;
      case 3:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS), len);
         break;
   }
}

static void setting_set_string_representation_timedate_date_seperator(char *s)
{
   settings_t *settings                  = config_get_ptr();
   unsigned menu_timedate_date_separator = settings ?
         settings->uints.menu_timedate_date_separator :
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN;

   switch (menu_timedate_date_separator)
   {
      case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
         string_replace_all_chars(s, '-', '/');
         break;
      case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
         string_replace_all_chars(s, '-', '.');
         break;
      case MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN:
      default:
         break;
   }
}

static void setting_get_string_representation_uint_menu_timedate_style(
   rarch_setting_t *setting,
   char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MENU_TIMEDATE_STYLE_YMD_HMS:
         strlcpy(s, msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS), len);
         break;
      case MENU_TIMEDATE_STYLE_YMD_HM:
         strlcpy(s, msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_YMD:
         strlcpy(s, msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD), len);
         break;
      case MENU_TIMEDATE_STYLE_YM:
         strlcpy(s, msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_YM), len);
         break;
      case MENU_TIMEDATE_STYLE_MDYYYY_HMS:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS), len);
         break;
      case MENU_TIMEDATE_STYLE_MDYYYY_HM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_MD_HM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_MDYYYY:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY), len);
         break;
      case MENU_TIMEDATE_STYLE_MD:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MD), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMMYYYY_HM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMM_HM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMMYYYY:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM), len);
         break;
      case MENU_TIMEDATE_STYLE_HMS:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS), len);
         break;
      case MENU_TIMEDATE_STYLE_HM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_HM), len);
         break;
      case MENU_TIMEDATE_STYLE_YMD_HMS_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_YMD_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_MDYYYY_HMS_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_MDYYYY_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_MD_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMMYYYY_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_DDMM_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_HMS_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM), len);
         break;
      case MENU_TIMEDATE_STYLE_HM_AMPM:
         strlcpy(s,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM), len);
         break;
   }

   /* Change date separator, if required */
   setting_set_string_representation_timedate_date_seperator(s);
}

static void setting_get_string_representation_uint_menu_timedate_date_separator(
   rarch_setting_t *setting,
   char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN:
         strcpy_literal(s, "'-'");
         break;
      case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
         strcpy_literal(s, "'/'");
         break;
      case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
         strcpy_literal(s, "'.'");
         break;
   }
}

static void setting_get_string_representation_uint_menu_add_content_entry_display_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MENU_ADD_CONTENT_ENTRY_DISPLAY_HIDDEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF),
               len);
         break;
      case MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB),
               len);
         break;
      case MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_menu_contentless_cores_display_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MENU_CONTENTLESS_CORES_DISPLAY_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF),
               len);
         break;
      case MENU_CONTENTLESS_CORES_DISPLAY_ALL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL),
               len);
         break;
      case MENU_CONTENTLESS_CORES_DISPLAY_SINGLE_PURPOSE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE),
               len);
         break;
      case MENU_CONTENTLESS_CORES_DISPLAY_CUSTOM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_rgui_menu_color_theme(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_THEME_CUSTOM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM),
               len);
         break;
      case RGUI_THEME_CLASSIC_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED),
               len);
         break;
      case RGUI_THEME_CLASSIC_ORANGE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE),
               len);
         break;
      case RGUI_THEME_CLASSIC_YELLOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW),
               len);
         break;
      case RGUI_THEME_CLASSIC_GREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN),
               len);
         break;
      case RGUI_THEME_CLASSIC_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE),
               len);
         break;
      case RGUI_THEME_CLASSIC_VIOLET:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET),
               len);
         break;
      case RGUI_THEME_CLASSIC_GREY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY),
               len);
         break;
      case RGUI_THEME_LEGACY_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED),
               len);
         break;
      case RGUI_THEME_DARK_PURPLE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE),
               len);
         break;
      case RGUI_THEME_MIDNIGHT_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE),
               len);
         break;
      case RGUI_THEME_GOLDEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN),
               len);
         break;
      case RGUI_THEME_ELECTRIC_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE),
               len);
         break;
      case RGUI_THEME_APPLE_GREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN),
               len);
         break;
      case RGUI_THEME_VOLCANIC_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED),
               len);
         break;
      case RGUI_THEME_LAGOON:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON),
               len);
         break;
      case RGUI_THEME_BROGRAMMER:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER),
               len);
         break;
      case RGUI_THEME_DRACULA:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA),
               len);
         break;
      case RGUI_THEME_FAIRYFLOSS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS),
               len);
         break;
      case RGUI_THEME_FLATUI:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI),
               len);
         break;
      case RGUI_THEME_GRUVBOX_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK),
               len);
         break;
      case RGUI_THEME_GRUVBOX_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT),
               len);
         break;
      case RGUI_THEME_HACKING_THE_KERNEL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL),
               len);
         break;
      case RGUI_THEME_NORD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD),
               len);
         break;
      case RGUI_THEME_NOVA:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA),
               len);
         break;
      case RGUI_THEME_ONE_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK),
               len);
         break;
      case RGUI_THEME_PALENIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT),
               len);
         break;
      case RGUI_THEME_SOLARIZED_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK),
               len);
         break;
      case RGUI_THEME_SOLARIZED_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT),
               len);
         break;
      case RGUI_THEME_TANGO_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK),
               len);
         break;
      case RGUI_THEME_TANGO_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT),
               len);
         break;
      case RGUI_THEME_ZENBURN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN),
               len);
         break;
      case RGUI_THEME_ANTI_ZENBURN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN),
               len);
         break;
      case RGUI_THEME_FLUX:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX),
               len);
         break;
      case RGUI_THEME_DYNAMIC:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC),
               len);
         break;
      case RGUI_THEME_GRAY_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK),
               len);
         break;
      case RGUI_THEME_GRAY_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_rgui_thumbnail_scaler(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_THUMB_SCALE_POINT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT),
               len);
         break;
      case RGUI_THUMB_SCALE_BILINEAR:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR),
               len);
         break;
      case RGUI_THUMB_SCALE_SINC:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_rgui_internal_upscale_level(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_UPSCALE_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE),
               len);
         break;
      case RGUI_UPSCALE_AUTO:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO),
               len);
         break;
      case RGUI_UPSCALE_X2:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X2),
               len);
         break;
      case RGUI_UPSCALE_X3:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X3),
               len);
         break;
      case RGUI_UPSCALE_X4:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X4),
               len);
         break;
      case RGUI_UPSCALE_X5:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X5),
               len);
         break;
      case RGUI_UPSCALE_X6:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X6),
               len);
         break;
      case RGUI_UPSCALE_X7:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X7),
               len);
         break;
      case RGUI_UPSCALE_X8:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X8),
               len);
         break;
      case RGUI_UPSCALE_X9:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X9),
               len);
         break;
   }
}

#if !defined(DINGUX)
static void setting_get_string_representation_uint_rgui_aspect_ratio(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_ASPECT_RATIO_4_3:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_4_3),
               len);
         break;
      case RGUI_ASPECT_RATIO_16_9:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9),
               len);
         break;
      case RGUI_ASPECT_RATIO_16_9_CENTRE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE),
               len);
         break;
      case RGUI_ASPECT_RATIO_16_10:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10),
               len);
         break;
      case RGUI_ASPECT_RATIO_16_10_CENTRE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE),
               len);
         break;
      case RGUI_ASPECT_RATIO_3_2:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2),
               len);
         break;
      case RGUI_ASPECT_RATIO_3_2_CENTRE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE),
               len);
         break;
      case RGUI_ASPECT_RATIO_5_3:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3),
               len);
         break;
      case RGUI_ASPECT_RATIO_5_3_CENTRE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE),
               len);
         break;

   }
}

static void setting_get_string_representation_uint_rgui_aspect_ratio_lock(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_ASPECT_RATIO_LOCK_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE),
               len);
         break;
      case RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN),
               len);
         break;
      case RGUI_ASPECT_RATIO_LOCK_INTEGER:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER),
               len);
         break;
      case RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN),
               len);
         break;
   }
}
#endif

static void setting_get_string_representation_uint_rgui_particle_effect(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RGUI_PARTICLE_EFFECT_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE),
               len);
         break;
      case RGUI_PARTICLE_EFFECT_SNOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW),
               len);
         break;
      case RGUI_PARTICLE_EFFECT_SNOW_ALT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT),
               len);
         break;
      case RGUI_PARTICLE_EFFECT_RAIN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN),
               len);
         break;
      case RGUI_PARTICLE_EFFECT_VORTEX:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX),
               len);
         break;
      case RGUI_PARTICLE_EFFECT_STARFIELD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD),
               len);
         break;
   }
}

#ifdef HAVE_XMB
static void setting_get_string_representation_uint_menu_xmb_animation_move_up_down(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s, "Easing Out Quad", len);
         break;
      case 1:
         strlcpy(s, "Easing Out Expo", len);
         break;
   }
}

static void setting_get_string_representation_uint_menu_xmb_animation_opening_main_menu(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s, "Easing Out Quad", len);
         break;
      case 1:
         strlcpy(s, "Easing Out Circ", len);
         break;
      case 2:
         strlcpy(s, "Easing Out Expo", len);
         break;
      case 3:
         strlcpy(s, "Easing Out Bounce", len);
         break;
   }
}

static void setting_get_string_representation_uint_menu_xmb_animation_horizontal_highlight(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strcpy_literal(s, "Easing Out Quad");
         break;
      case 1:
         strcpy_literal(s, "Easing In Sine");
         break;
      case 2:
         strcpy_literal(s, "Easing Out Bounce");
         break;
   }
}
#endif

static void setting_get_string_representation_uint_menu_ticker_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case TICKER_TYPE_BOUNCE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE),
               len);
         break;
      case TICKER_TYPE_LOOP:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP),
               len);
         break;
   }
}

#ifdef HAVE_XMB
static void setting_get_string_representation_uint_xmb_icon_theme(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case XMB_ICON_THEME_MONOCHROME:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME), len);
         break;
      case XMB_ICON_THEME_FLATUI:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI), len);
         break;
      case XMB_ICON_THEME_RETROACTIVE:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE), len);
         break;
      case XMB_ICON_THEME_RETROSYSTEM:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM), len);
         break;
      case XMB_ICON_THEME_PIXEL:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL), len);
         break;
      case XMB_ICON_THEME_NEOACTIVE:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE), len);
         break;
      case XMB_ICON_THEME_SYSTEMATIC:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC), len);
         break;
      case XMB_ICON_THEME_DOTART:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART), len);
         break;
      case XMB_ICON_THEME_MONOCHROME_INVERTED:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED), len);
         break;
      case XMB_ICON_THEME_CUSTOM:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM), len);
         break;
      case XMB_ICON_THEME_AUTOMATIC:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC), len);
         break;
      case XMB_ICON_THEME_AUTOMATIC_INVERTED:
         strlcpy(s,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED), len);
         break;
   }
}

static void setting_get_string_representation_uint_xmb_layout(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strcpy_literal(s, "Auto");
         break;
      case 1:
         strcpy_literal(s, "Console");
         break;
      case 2:
         strcpy_literal(s, "Handheld");
         break;
   }
}

static void setting_get_string_representation_uint_xmb_menu_color_theme(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case XMB_THEME_WALLPAPER:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN),
               len);
         break;
      case XMB_THEME_LEGACY_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED),
               len);
         break;
      case XMB_THEME_DARK_PURPLE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE),
               len);
         break;
      case XMB_THEME_MIDNIGHT_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE),
               len);
         break;
      case XMB_THEME_GOLDEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN),
               len);
         break;
      case XMB_THEME_ELECTRIC_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE),
               len);
         break;
      case XMB_THEME_APPLE_GREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN),
               len);
         break;
      case XMB_THEME_UNDERSEA:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA),
               len);
         break;
      case XMB_THEME_VOLCANIC_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED),
               len);
         break;
      case XMB_THEME_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK),
               len);
         break;
      case XMB_THEME_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT),
               len);
         break;
      case XMB_THEME_MORNING_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE),
               len);
         break;
      case XMB_THEME_SUNBEAM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM),
               len);
         break;
	  case XMB_THEME_LIME:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME),
               len);
         break;
	  case XMB_THEME_MIDGAR:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR),
               len);
         break;
	  case XMB_THEME_PIKACHU_YELLOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW),
               len);
         break;
	  case XMB_THEME_GAMECUBE_PURPLE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE),
               len);
         break;
	  case XMB_THEME_FAMICOM_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED),
               len);
         break;
	  case XMB_THEME_FLAMING_HOT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT),
               len);
         break;
	  case XMB_THEME_ICE_COLD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD),
               len);
         break;
   }
}
#endif

#ifdef HAVE_MATERIALUI
static void setting_get_string_representation_uint_materialui_menu_color_theme(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MATERIALUI_THEME_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE), len);
         break;
      case MATERIALUI_THEME_BLUE_GREY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY), len);
         break;
      case MATERIALUI_THEME_GREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN), len);
         break;
      case MATERIALUI_THEME_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED), len);
         break;
      case MATERIALUI_THEME_YELLOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW), len);
         break;
      case MATERIALUI_THEME_DARK_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE), len);
         break;
      case MATERIALUI_THEME_NVIDIA_SHIELD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD), len);
         break;
      case MATERIALUI_THEME_MATERIALUI:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI), len);
         break;
      case MATERIALUI_THEME_MATERIALUI_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK), len);
         break;
      case MATERIALUI_THEME_OZONE_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK), len);
         break;
      case MATERIALUI_THEME_NORD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD), len);
         break;
      case MATERIALUI_THEME_GRUVBOX_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK), len);
         break;
      case MATERIALUI_THEME_SOLARIZED_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK), len);
         break;
      case MATERIALUI_THEME_CUTIE_BLUE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE), len);
         break;
      case MATERIALUI_THEME_CUTIE_CYAN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN), len);
         break;
      case MATERIALUI_THEME_CUTIE_GREEN:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN), len);
         break;
      case MATERIALUI_THEME_CUTIE_ORANGE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE), len);
         break;
      case MATERIALUI_THEME_CUTIE_PINK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK), len);
         break;
      case MATERIALUI_THEME_CUTIE_PURPLE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE), len);
         break;
      case MATERIALUI_THEME_CUTIE_RED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED), len);
         break;
      case MATERIALUI_THEME_VIRTUAL_BOY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY), len);
         break;
      case MATERIALUI_THEME_HACKING_THE_KERNEL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL), len);
         break;
      case MATERIALUI_THEME_GRAY_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK), len);
         break;
      case MATERIALUI_THEME_GRAY_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT), len);
         break;
      default:
         break;
   }
}

static void setting_get_string_representation_uint_materialui_menu_transition_animation(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MATERIALUI_TRANSITION_ANIM_AUTO:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO), len);
         break;
      case MATERIALUI_TRANSITION_ANIM_FADE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE), len);
         break;
      case MATERIALUI_TRANSITION_ANIM_SLIDE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE), len);
         break;
      case MATERIALUI_TRANSITION_ANIM_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE), len);
         break;
      default:
         break;
   }
}

static void setting_get_string_representation_uint_materialui_menu_thumbnail_view_portrait(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON), len);
         break;
      default:
         break;
   }
}

static void setting_get_string_representation_uint_materialui_menu_thumbnail_view_landscape(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE), len);
         break;
      case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP), len);
         break;
      default:
         break;
   }
}

static void setting_get_string_representation_uint_materialui_landscape_layout_optimization(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED), len);
         break;
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS), len);
         break;
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS), len);
         break;
      default:
         break;
   }
}
#endif

#ifdef HAVE_OZONE
static void setting_get_string_representation_uint_ozone_menu_color_theme(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case OZONE_COLOR_THEME_BASIC_BLACK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK), len);
         break;
      case OZONE_COLOR_THEME_NORD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD), len);
         break;
      case OZONE_COLOR_THEME_GRUVBOX_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK), len);
         break;
      case OZONE_COLOR_THEME_BOYSENBERRY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY), len);
         break;
      case OZONE_COLOR_THEME_HACKING_THE_KERNEL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL), len);
         break;
      case OZONE_COLOR_THEME_TWILIGHT_ZONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE), len);
         break;
      case OZONE_COLOR_THEME_DRACULA:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA), len);
         break;
      case OZONE_COLOR_THEME_SOLARIZED_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK), len);
         break;
      case OZONE_COLOR_THEME_SOLARIZED_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT), len);
         break;
      case OZONE_COLOR_THEME_GRAY_DARK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK), len);
         break;
      case OZONE_COLOR_THEME_GRAY_LIGHT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT), len);
         break;
      case OZONE_COLOR_THEME_BASIC_WHITE:
      default:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE), len);
         break;
   }
}
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#if defined(HAVE_XMB) && defined(HAVE_SHADERPIPELINE)
static void setting_get_string_representation_uint_xmb_shader_pipeline(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case XMB_SHADER_PIPELINE_WALLPAPER:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case XMB_SHADER_PIPELINE_SIMPLE_RIBBON:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED), len);
         break;
      case XMB_SHADER_PIPELINE_RIBBON:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON), len);
         break;
      case XMB_SHADER_PIPELINE_SIMPLE_SNOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW), len);
         break;
      case XMB_SHADER_PIPELINE_SNOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW), len);
         break;
      case XMB_SHADER_PIPELINE_BOKEH:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH), len);
         break;
      case XMB_SHADER_PIPELINE_SNOWFLAKE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE), len);
         break;
   }
}
#endif
#endif

#ifdef HAVE_SCREENSHOTS
#ifdef HAVE_GFX_WIDGETS
static void setting_get_string_representation_uint_notification_show_screenshot_duration(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL), len);
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST), len);
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST), len);
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT), len);
         break;
   }
}

static void setting_get_string_representation_uint_notification_show_screenshot_flash(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL), len);
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST), len);
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_OFF:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
   }
}
#endif
#endif

static void setting_get_string_representation_uint_video_monitor_index(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer)
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
   else
      strlcpy(s, "0 (Auto)", len);
}

static void setting_get_string_representation_uint_custom_viewport_width(rarch_setting_t *setting,
      char *s, size_t len)
{
   struct retro_game_geometry  *geom    = NULL;
   struct retro_system_av_info *av_info = NULL;
   unsigned int rotation                = retroarch_get_rotation();
   if (!setting)
      return;

   av_info = video_viewport_get_system_av_info();
   geom    = (struct retro_game_geometry*)&av_info->geometry;

   if (!(rotation % 2) && (*setting->value.target.unsigned_integer%geom->base_width == 0))
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_width);
   else if ((rotation % 2) && (*setting->value.target.unsigned_integer%geom->base_height == 0))
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_height);
   else
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_custom_viewport_height(rarch_setting_t *setting,
      char *s, size_t len)
{
   struct retro_game_geometry  *geom    = NULL;
   struct retro_system_av_info *av_info = NULL;
   unsigned int rotation                = retroarch_get_rotation();
   if (!setting)
      return;

   av_info = video_viewport_get_system_av_info();
   geom    = (struct retro_game_geometry*)&av_info->geometry;

   if (!(rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_height == 0))
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_height);
   else  if ((rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_width == 0))
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_width);
   else
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
}

#ifdef HAVE_WASAPI
static void setting_get_string_representation_int_audio_wasapi_sh_buffer_length(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (*setting->value.target.integer > 0)
      snprintf(s, len, "%d",
            *setting->value.target.integer);
   else if (*setting->value.target.integer == 0)
      strlcpy(s, "0 (Off)", len);
   else
      strlcpy(s, "Auto", len);
}
#endif

static void setting_get_string_representation_crt_switch_resolution_super(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer == 0)
      strlcpy(s, "NATIVE", len);
   else if (*setting->value.target.unsigned_integer == 1)
      strlcpy(s, "DYNAMIC", len);
   else
      snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_playlist_sublabel_runtime_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case PLAYLIST_RUNTIME_PER_CORE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE),
               len);
         break;
      case PLAYLIST_RUNTIME_AGGREGATE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_playlist_sublabel_last_played_style(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_YMD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_YM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MD_HM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MD),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_MD_HM_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM),
               len);
         break;
      case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM_AMPM:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM),
               len);
         break;
   }

   /* Change date separator, if required */
   setting_set_string_representation_timedate_date_seperator(s);
}

static void setting_get_string_representation_uint_playlist_inline_core_display_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV),
               len);
         break;
      case PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS),
               len);
         break;
      case PLAYLIST_INLINE_CORE_DISPLAY_NEVER:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_playlist_entry_remove_enable(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV),
               len);
         break;
      case PLAYLIST_ENTRY_REMOVE_ENABLE_ALL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL),
               len);
         break;
      case PLAYLIST_ENTRY_REMOVE_ENABLE_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE),
               len);
         break;
   }
}

#if defined(_3DS)
static void setting_get_string_representation_uint_video_3ds_display_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case CTR_VIDEO_MODE_3D:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_3D),
               len);
         break;
      case CTR_VIDEO_MODE_2D:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D),
               len);
         break;
      case CTR_VIDEO_MODE_2D_400X240:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240),
               len);
         break;
      case CTR_VIDEO_MODE_2D_800X240:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240),
               len);
         break;
   }
}
#endif

#if defined(DINGUX)
static void setting_get_string_representation_uint_video_dingux_ipu_filter_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case DINGUX_IPU_FILTER_BICUBIC:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC),
               len);
         break;
      case DINGUX_IPU_FILTER_BILINEAR:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR),
               len);
         break;
      case DINGUX_IPU_FILTER_NEAREST:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST),
               len);
         break;
   }
}

#if defined(DINGUX_BETA)
static void setting_get_string_representation_uint_video_dingux_refresh_rate(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case DINGUX_REFRESH_RATE_60HZ:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_60HZ),
               len);
         break;
      case DINGUX_REFRESH_RATE_50HZ:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_50HZ),
               len);
         break;
   }
}
#endif

#if defined(RS90) || defined(MIYOO)
static void setting_get_string_representation_uint_video_dingux_rs90_softfilter_type(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case DINGUX_RS90_SOFTFILTER_POINT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT),
               len);
         break;
      case DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ),
               len);
         break;
   }
}
#endif
#endif

static void setting_get_string_representation_uint_input_auto_game_focus(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case AUTO_GAME_FOCUS_OFF:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF),
               len);
         break;
      case AUTO_GAME_FOCUS_ON:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON),
               len);
         break;
      case AUTO_GAME_FOCUS_DETECT:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT),
               len);
         break;
   }
}

#if defined(HAVE_OVERLAY)
static void setting_get_string_representation_uint_input_overlay_show_inputs(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case OVERLAY_SHOW_INPUT_NONE:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF),
               len);
         break;
      case OVERLAY_SHOW_INPUT_TOUCHED:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED),
               len);
         break;
      case OVERLAY_SHOW_INPUT_PHYSICAL:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_input_overlay_show_inputs_port(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer + 1);
}
#endif

/* A protected driver is such that the user cannot set to "null" using the UI.
 * Can prevent the user from locking him/herself out of the program. */
static bool setting_is_protected_driver(rarch_setting_t *setting)
{
   if (setting)
   {
      switch (setting->enum_idx)
      {
         case MENU_ENUM_LABEL_INPUT_DRIVER:
         case MENU_ENUM_LABEL_JOYPAD_DRIVER:
         case MENU_ENUM_LABEL_VIDEO_DRIVER:
         case MENU_ENUM_LABEL_MENU_DRIVER:
            return true;
         default:
            break;
      }
   }

   return false;
}

static int setting_action_left_analog_dpad_mode(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned        port = 0;
   settings_t *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   configuration_set_uint(settings, settings->uints.input_analog_dpad_mode[port],
      (settings->uints.input_analog_dpad_mode
       [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST);

   return 0;
}

unsigned libretro_device_get_size(unsigned *devices, size_t devices_size, unsigned port)
{
   unsigned types                           = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_system_info_t              *system = &runloop_state_get_ptr()->system;

   devices[types++]                         = RETRO_DEVICE_NONE;
   devices[types++]                         = RETRO_DEVICE_JOYPAD;

   if (system)
   {
      /* Only push RETRO_DEVICE_ANALOG as default if we use an
       * older core which doesn't use SET_CONTROLLER_INFO. */
      if (!system->ports.size)
         devices[types++] = RETRO_DEVICE_ANALOG;

      if (port < system->ports.size)
         desc = &system->ports.data[port];
   }

   if (desc)
   {
      unsigned i;
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < devices_size &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   return types;
}

static int setting_action_left_libretro_device_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh = false;
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;

   if (!setting)
      return -1;

   port           = setting->index_offset;
   types          = libretro_device_get_size(devices, ARRAY_SIZE(devices), port);
   current_device = input_config_get_device(port);
   current_idx    = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device != devices[i])
         continue;

      current_idx = i;
      break;
   }

   current_device = devices
      [(current_idx + types - 1) % types];

   input_config_set_device(port, current_device);

   pad.port   = port;
   pad.device = current_device;

   core_set_controller_port_device(&pad);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static int setting_action_left_input_remap_port(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh         = false;
   unsigned port        = 0;
   settings_t *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   if (settings->uints.input_remap_ports[port] > 0)
      settings->uints.input_remap_ports[port]--;
   else
      settings->uints.input_remap_ports[port] = MAX_USERS - 1;

   /* Must be called whenever settings->uints.input_remap_ports
    * is modified */
   input_remapping_update_port_map();

   /* Changing mapped port may leave a core port unused;
    * reinitialise controllers to ensure that any such
    * ports are set to 'RETRO_DEVICE_NONE' */
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static int setting_uint_action_left_crt_switch_resolution_super(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         *setting->value.target.unsigned_integer = 3840;
         break;
      case 1: /* for dynamic super resolution switching - best fit */
         *setting->value.target.unsigned_integer = 0;
         break;
      case 1920:
         *setting->value.target.unsigned_integer = 1;
         break;
      case 2560:
         *setting->value.target.unsigned_integer = 1920;
         break;
      case 3840:
         *setting->value.target.unsigned_integer = 2560;
         break;
   }

   return 0;
}

static int setting_action_left_bind_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned               *p = NULL;
   unsigned index_offset     = 0;
   unsigned max_devices      = input_config_get_device_count();
   settings_t      *settings = config_get_ptr();

   if (!setting || max_devices == 0)
      return -1;

   index_offset = setting->index_offset;

   p = &settings->uints.input_joypad_index[index_offset];

   if ((*p) >= max_devices)
      *p = max_devices - 1;
   else if ((*p) > 0)
      (*p)--;

   return 0;
}

static int setting_action_left_mouse_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned index_offset    = 0;
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   index_offset             = setting->index_offset;

   if (settings->uints.input_mouse_index[index_offset])
      --settings->uints.input_mouse_index[index_offset];
   else
      settings->uints.input_mouse_index[index_offset] = MAX_USERS - 1;

   settings->modified = true;
   return 0;
}

static int setting_uint_action_left_custom_viewport_width(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->width <= setting->min)
      custom->width = setting->min;
   else if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
      {
         if (custom->width > geom->base_height)
            custom->width -= geom->base_height;
      }
      else
      {
         if (custom->width > geom->base_width)
            custom->width -= geom->base_width;
      }
   }
   else
      custom->width -= 1;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

static int setting_uint_action_left_custom_viewport_height(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->height <= setting->min)
      custom->height = setting->min;
   else if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
      {
         if (custom->height > geom->base_width)
            custom->height -= geom->base_width;
      }
      else
      {
         if (custom->height > geom->base_height)
            custom->height -= geom->base_height;
      }
   }
   else
      custom->height -= 1;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_left_audio_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int audio_device_index;
   struct string_list *ptr  = NULL;

   if (!audio_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   audio_device_index = string_list_find_elem(
         ptr, setting->value.target.string) - 1;
   audio_device_index--;

   /* Reset index if needed */
   if (audio_device_index < 0)
      audio_device_index = (int)(ptr->size - 1);

   strlcpy(setting->value.target.string, ptr->elems[audio_device_index].data, setting->size);

   return 0;
}
#endif

static int setting_string_action_left_driver(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   driver_ctx_info_t drv;
   bool success = false;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   /* Find the previous driver in the array of drivers.
    * If the driver is protected, keep finding more previous drivers while the
    * driver is null or until there are no more previous drivers. */
   success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv);
   if (setting_is_protected_driver(setting))
   {
      while (success &&
             string_is_equal(drv.s, "null") &&
             (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
   }

   if (!success)
   {
      settings_t                   *settings = config_get_ptr();
      bool menu_navigation_wraparound_enable = settings ? settings->bools.menu_navigation_wraparound_enable: false;

      if (menu_navigation_wraparound_enable)
      {
         /* If wraparound is enabled, find the LAST driver in the array of drivers.
          * If the driver is protected, find the previous driver and keep finding more
          * previous drivers while the driver is null or until there are no more previous drivers. */
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_LAST, &drv);
         if (setting_is_protected_driver(setting))
         {
            while (success &&
                   string_is_equal(drv.s, "null") &&
                   (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
         }
      }
      else if (setting_is_protected_driver(setting))
      {
         /* If wraparound is disabled and if the driver is protected,
          * find the next driver in the array of drivers and keep finding more
          * next drivers while the driver is null or until there are no more next drivers. */
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv);
         while (success &&
                string_is_equal(drv.s, "null") &&
                (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

#ifdef HAVE_NETWORKING
static int setting_string_action_left_netplay_mitm_server(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned i;
   int offset               = 0;
   bool               found = false;
   unsigned        list_len = ARRAY_SIZE(netplay_mitm_server_list);
   if (!setting)
      return -1;

   for (i = 0; i < list_len; i++)
   {
      /* find the currently selected server in the list */
      if (string_is_equal(setting->value.target.string, netplay_mitm_server_list[i].name))
      {
         /* move to the previous one in the list, wrap around if necessary */
         if (i >= 1)
         {
            found  = true;
            offset = i - 1;
         }
         else if (wraparound)
         {
            found  = true;
            offset = list_len - 1;
         }

         if (found)
            break;
      }
   }

   /* current entry was invalid, go back to the end */
   if (!found)
      offset = list_len - 1;

   if (offset >= 0)
      strlcpy(setting->value.target.string,
            netplay_mitm_server_list[offset].name, setting->size);

   return 0;
}

static int setting_string_action_right_netplay_mitm_server(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned i;
   int offset               = 0;
   bool               found = false;
   unsigned        list_len = ARRAY_SIZE(netplay_mitm_server_list);
   if (!setting)
      return -1;

   for (i = 0; i < list_len; i++)
   {
      /* find the currently selected server in the list */
      if (string_is_equal(setting->value.target.string, netplay_mitm_server_list[i].name))
      {
         /* move to the next one in the list, wrap around if necessary */
         if (i + 1 < list_len)
         {
            offset = i + 1;
            found  = true;
         }
         else if (wraparound)
            found = true;

         if (found)
            break;
      }
   }

   /* current entry was invalid, go back to the start */
   if (!found)
      offset = 0;

   strlcpy(setting->value.target.string,
         netplay_mitm_server_list[offset].name, setting->size);

   return 0;
}
#endif

static int setting_uint_action_right_crt_switch_resolution_super(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         *setting->value.target.unsigned_integer = 1;
         break;
      case 1: /* for dynamic super resolution switching - best fit */
         *setting->value.target.unsigned_integer = 1920;
         break;
      case 1920:
         *setting->value.target.unsigned_integer = 2560;
         break;
      case 2560:
         *setting->value.target.unsigned_integer = 3840;
         break;
      case 3840:
         *setting->value.target.unsigned_integer = 0;
         break;
   }

   return 0;
}

static int setting_uint_action_right_custom_viewport_width(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->width >= setting->max)
      custom->width = setting->max;
   else if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
         custom->width += geom->base_height;
      else
         custom->width += geom->base_width;
   }
   else
      custom->width += 1;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

static int setting_uint_action_right_custom_viewport_height(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->height >= setting->max)
      custom->height = setting->max;
   else if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
         custom->height += geom->base_width;
      else
         custom->height += geom->base_height;
   }
   else
      custom->height += 1;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_right_audio_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int audio_device_index;
   struct string_list *ptr  = NULL;

   if (!audio_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   audio_device_index = string_list_find_elem(ptr,setting->value.target.string) -1;
   audio_device_index++;

   /* Reset index if needed */
   if (audio_device_index == (signed)ptr->size)
      audio_device_index = 0;

   strlcpy(setting->value.target.string,
         ptr->elems[audio_device_index].data, setting->size);

   return 0;
}
#endif

static int setting_string_action_right_driver(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   driver_ctx_info_t drv;
   bool success = false;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   /* Find the next driver in the array of drivers.
    * If the driver is protected, keep finding next drivers while the
    * driver is null or until there are no more next drivers. */
   success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv);
   if (setting_is_protected_driver(setting))
   {
      while (success &&
             string_is_equal(drv.s, "null") &&
             (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
   }

   if (!success)
   {
      settings_t                   *settings = config_get_ptr();
      bool menu_navigation_wraparound_enable = settings ? settings->bools.menu_navigation_wraparound_enable: false;

      if (menu_navigation_wraparound_enable)
      {
         /* If wraparound is enabled, find the first driver in the array of drivers.
          * If the driver is protected, find the next driver and keep finding more
          * next drivers while the driver is null or until there are no more next drivers. */
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_FIRST, &drv);
         if (setting_is_protected_driver(setting))
         {
            while (success &&
                   string_is_equal(drv.s, "null") &&
                   (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
         }
      }
      else if (setting_is_protected_driver(setting))
      {
         /* If wraparound is disabled and if the driver is protected,
          * find the previous driver in the array of drivers and keep finding more
          * previous drivers while the driver is null or until there are no more previous drivers. */
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv);
         while (success &&
                string_is_equal(drv.s, "null") &&
                (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int setting_string_action_left_midi_input(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_inputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string) - 2;

      if (wraparound && i == -1)
         i = (int)list->size - 1;
      if (i >= 0)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   return -1;
}

static int setting_string_action_right_midi_input(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_inputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string);

      if (wraparound && i == (int)list->size)
         i = 0;
      if (i >= 0 && i < (int)list->size)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   return -1;
}

static int setting_string_action_left_midi_output(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_outputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string) - 2;

      if (wraparound && i == -1)
         i = (int)list->size - 1;
      if (i >= 0)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   return -1;
}

static int setting_string_action_right_midi_output(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_outputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string);

      if (wraparound && i == (int)list->size)
         i = 0;
      if (i >= 0 && i < (int)list->size)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   return -1;
}

#ifdef HAVE_CHEATS
static void setting_get_string_representation_uint_cheat_exact(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_cheat_lt(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL), len);
}

static void setting_get_string_representation_uint_cheat_gt(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL), len);
}

static void setting_get_string_representation_uint_cheat_lte(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL), len);
}

static void setting_get_string_representation_uint_cheat_gte(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL), len);
}

static void setting_get_string_representation_uint_cheat_eq(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL), len);
}

static void setting_get_string_representation_uint_cheat_neq(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL), len);
}

static void setting_get_string_representation_uint_cheat_eqplus(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_cheat_eqminus(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_cheat_browse_address(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned int address      = cheat_manager_state.browse_address;
   unsigned int address_mask = 0;
   unsigned int prev_val     = 0;
   unsigned int curr_val     = 0;

   if (setting)
      snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);

   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_BROWSE, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val);

   snprintf(s, len, "Prev: %u Curr: %u", prev_val, curr_val);

}
#endif

static void setting_get_string_representation_uint_video_rotation(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      char rotation_lut[4][32] =
      {
         "Normal",
         "90 deg",
         "180 deg",
         "270 deg"
      };

      strlcpy(s, rotation_lut[*setting->value.target.unsigned_integer],
            len);
   }
}

static void setting_get_string_representation_uint_screen_orientation(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      char rotation_lut[4][32] =
      {
         "Normal",
         "90 deg",
         "180 deg",
         "270 deg"
      };

      strlcpy(s, rotation_lut[*setting->value.target.unsigned_integer],
            len);
   }
}

static void setting_get_string_representation_uint_aspect_ratio_index(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      strlcpy(s,
            aspectratio_lut[*setting->value.target.unsigned_integer].name,
            len);
}

static void setting_get_string_representation_uint_crt_switch_resolutions(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case CRT_SWITCH_NONE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case CRT_SWITCH_15KHZ:
         strlcpy(s, "15 KHz", len);
         break;
      case CRT_SWITCH_31KHZ:
         strlcpy(s, "31 KHz, Standard", len);
         break;
      case CRT_SWITCH_32_120:
         strlcpy(s, "31 KHz, 120Hz", len);
         break;
      case CRT_SWITCH_INI:
         strlcpy(s, "INI", len);
         break;
   }
}

static void setting_get_string_representation_uint_audio_resampler_quality(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RESAMPLER_QUALITY_DONTCARE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE),
               len);
         break;
      case RESAMPLER_QUALITY_LOWEST:
         strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWEST),
               len);
         break;
      case RESAMPLER_QUALITY_LOWER:
         strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWER),
               len);
         break;
      case RESAMPLER_QUALITY_HIGHER:
         strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHER),
               len);
         break;
      case RESAMPLER_QUALITY_HIGHEST:
         strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHEST),
               len);
         break;
      case RESAMPLER_QUALITY_NORMAL:
         strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_NORMAL),
               len);
         break;
   }
}

static void setting_get_string_representation_uint_libretro_device(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned index_offset, device;
   const struct retro_controller_description *desc = NULL;
   const char *name            = NULL;
   rarch_system_info_t *system = &runloop_state_get_ptr()->system;

   if (!setting)
      return;

   index_offset                = setting->index_offset;
   device                      = input_config_get_device(index_offset);

   if (system)
   {
      if (index_offset < system->ports.size)
         desc = libretro_find_controller_description(
               &system->ports.data[index_offset],
               device);
   }

   if (desc)
      name = desc->desc;

   if (!name)
   {
      /* Find generic name. */
      switch (device)
      {
         case RETRO_DEVICE_NONE:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
            break;
         case RETRO_DEVICE_JOYPAD:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETROPAD);
            break;
         case RETRO_DEVICE_ANALOG:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG);
            break;
         default:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);
            break;
      }
   }

   if (!string_is_empty(name))
      strlcpy(s, name, len);
}

static void setting_get_string_representation_uint_analog_dpad_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   const char *modes[5];

   modes[0] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
   modes[1] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG);
   modes[2] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG);
   modes[3] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED);
   modes[4] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED);

   strlcpy(s, modes[*setting->value.target.unsigned_integer % ANALOG_DPAD_LAST], len);
}

static void setting_get_string_representation_uint_input_remap_port(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "%u", *setting->value.target.unsigned_integer + 1);
}

#ifdef HAVE_THREADS
static void setting_get_string_representation_uint_autosave_interval(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer)
      snprintf(s, len, "%u %s",
            *setting->value.target.unsigned_integer, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS));
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
}
#endif

#if defined(HAVE_NETWORKING)
static void setting_get_string_representation_netplay_mitm_server(
      rarch_setting_t *setting,
      char *s, size_t len)
{

}

static void setting_get_string_representation_netplay_share_digital(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RARCH_NETPLAY_SHARE_DIGITAL_NO_PREFERENCE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE), len);
         break;

      case RARCH_NETPLAY_SHARE_DIGITAL_OR:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR), len);
         break;

      case RARCH_NETPLAY_SHARE_DIGITAL_XOR:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR), len);
         break;

      case RARCH_NETPLAY_SHARE_DIGITAL_VOTE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE), len);
         break;

      default:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE), len);
         break;
   }
}

static void setting_get_string_representation_netplay_share_analog(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case RARCH_NETPLAY_SHARE_ANALOG_NO_PREFERENCE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE), len);
         break;

      case RARCH_NETPLAY_SHARE_ANALOG_MAX:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX), len);
         break;

      case RARCH_NETPLAY_SHARE_ANALOG_AVERAGE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE), len);
         break;

      default:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE), len);
         break;
   }
}
#endif

static void setting_get_string_representation_gamepad_combo(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case INPUT_COMBO_NONE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
         break;
      case INPUT_COMBO_DOWN_Y_L_R:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R), len);
         break;
      case INPUT_COMBO_L3_R3:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L3_R3), len);
         break;
      case INPUT_COMBO_L1_R1_START_SELECT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L1_R1_START_SELECT), len);
         break;
      case INPUT_COMBO_START_SELECT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_SELECT), len);
         break;
      case INPUT_COMBO_L3_R:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L3_R), len);
         break;
      case INPUT_COMBO_L_R:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L_R), len);
         break;
      case INPUT_COMBO_HOLD_START:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HOLD_START), len);
         break;
      case INPUT_COMBO_HOLD_SELECT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HOLD_SELECT), len);
         break;
      case INPUT_COMBO_DOWN_SELECT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWN_SELECT), len);
         break;
      case INPUT_COMBO_L2_R2:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L2_R2), len);
         break;
   }
}

static void setting_get_string_representation_turbo_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case INPUT_TURBO_MODE_CLASSIC:
         strlcpy(s, "Classic", len);
         break;
      case INPUT_TURBO_MODE_SINGLEBUTTON:
         strlcpy(s, "Single Button (Toggle)", len);
         break;
      case INPUT_TURBO_MODE_SINGLEBUTTON_HOLD:
         strlcpy(s, "Single Button (Hold)", len);
         break;
   }
}

static void setting_get_string_representation_turbo_default_button(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case INPUT_TURBO_DEFAULT_BUTTON_B:
         strlcpy(s, "B", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_Y:
         strlcpy(s, "Y", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_A:
         strlcpy(s, "A", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_X:
         strlcpy(s, "X", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_L:
         strlcpy(s, "L", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_R:
         strlcpy(s, "R", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_L2:
         strlcpy(s, "L2", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_R2:
         strlcpy(s, "R2", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_L3:
         strlcpy(s, "L3", len);
         break;
      case INPUT_TURBO_DEFAULT_BUTTON_R3:
         strlcpy(s, "R3", len);
         break;
   }
}


static void setting_get_string_representation_poll_type_behavior(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY), len);
         break;
      case 1:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL), len);
         break;
      case 2:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE), len);
         break;
   }
}

static void setting_get_string_representation_input_touch_scale(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      snprintf(s, len, "x%d", *setting->value.target.unsigned_integer);
}

#ifdef HAVE_LANGEXTRA
static void setting_get_string_representation_uint_user_language(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   const char *modes[RETRO_LANGUAGE_LAST];

   modes[RETRO_LANGUAGE_ENGLISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH);
   modes[RETRO_LANGUAGE_JAPANESE]               = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE);
   modes[RETRO_LANGUAGE_FRENCH]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_FRENCH);
   modes[RETRO_LANGUAGE_SPANISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_SPANISH);
   modes[RETRO_LANGUAGE_GERMAN]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_GERMAN);
   modes[RETRO_LANGUAGE_ITALIAN]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN);
   modes[RETRO_LANGUAGE_DUTCH]                  = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_DUTCH);
   modes[RETRO_LANGUAGE_PORTUGUESE_BRAZIL]      = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL);
   modes[RETRO_LANGUAGE_PORTUGUESE_PORTUGAL]    = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL);
   modes[RETRO_LANGUAGE_RUSSIAN]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN);
   modes[RETRO_LANGUAGE_KOREAN]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_KOREAN);
   modes[RETRO_LANGUAGE_CHINESE_TRADITIONAL]    = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL);
   modes[RETRO_LANGUAGE_CHINESE_SIMPLIFIED]     = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED);
   modes[RETRO_LANGUAGE_ESPERANTO]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO);
   modes[RETRO_LANGUAGE_POLISH]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_POLISH);
   modes[RETRO_LANGUAGE_VIETNAMESE]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE);
   modes[RETRO_LANGUAGE_ARABIC]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ARABIC);
   modes[RETRO_LANGUAGE_GREEK]                  = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_GREEK);
   modes[RETRO_LANGUAGE_TURKISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_TURKISH);
   modes[RETRO_LANGUAGE_SLOVAK]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_SLOVAK);
   modes[RETRO_LANGUAGE_PERSIAN]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PERSIAN);
   modes[RETRO_LANGUAGE_HEBREW]                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_HEBREW);
   modes[RETRO_LANGUAGE_ASTURIAN]               = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ASTURIAN);
   modes[RETRO_LANGUAGE_FINNISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_FINNISH);
   modes[RETRO_LANGUAGE_INDONESIAN]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN);
   modes[RETRO_LANGUAGE_SWEDISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_SWEDISH);
   modes[RETRO_LANGUAGE_UKRAINIAN]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN);
   modes[RETRO_LANGUAGE_CZECH]                  = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CZECH);
   strlcpy(s, modes[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)], len);
}
#endif

static void setting_get_string_representation_uint_libretro_log_level(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      static const char *modes[] = {
         "0 (Debug)",
         "1 (Info)",
         "2 (Warning)",
         "3 (Error)"
      };
      strlcpy(s, modes[*setting->value.target.unsigned_integer],
            len);
   }
}

static void setting_get_string_representation_uint_quit_on_close_content(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case QUIT_ON_CLOSE_CONTENT_DISABLED:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case QUIT_ON_CLOSE_CONTENT_ENABLED:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
         break;
      case QUIT_ON_CLOSE_CONTENT_CLI:
         strlcpy(s, "CLI", len);
         break;
   }
}

static void setting_get_string_representation_uint_playlist_show_history_icons(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case PLAYLIST_SHOW_HISTORY_ICONS_DEFAULT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
         break;
      case PLAYLIST_SHOW_HISTORY_ICONS_MAIN:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MAIN), len);
         break;
      case PLAYLIST_SHOW_HISTORY_ICONS_CONTENT:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT), len);
         break;
   }
}

static void setting_get_string_representation_uint_menu_screensaver_timeout(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer == 0)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   else
      snprintf(s, len, "%u %s",
            *setting->value.target.unsigned_integer,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS));
}

#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
static void setting_get_string_representation_uint_menu_screensaver_animation(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case MENU_SCREENSAVER_BLANK:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_OFF),
               len);
         break;
      case MENU_SCREENSAVER_SNOW:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW),
               len);
         break;
      case MENU_SCREENSAVER_STARFIELD:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD),
               len);
         break;
      case MENU_SCREENSAVER_VORTEX:
         strlcpy(s,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX),
               len);
         break;
   }
}
#endif

enum setting_type menu_setting_get_browser_selection_type(rarch_setting_t *setting)
{
   if (!setting)
      return ST_NONE;
   return setting->browser_selection_type;
}

static void menu_settings_list_current_add_range(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float min, float max, float step,
      bool enforce_minrange_enable, bool enforce_maxrange_enable)
{
   unsigned idx                   = list_info->index - 1;

   if ((*list)[idx].type == ST_FLOAT)
      (*list)[list_info->index - 1].ui_type
                                  = ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX;

   (*list)[idx].min               = min;
   (*list)[idx].step              = step;
   (*list)[idx].max               = max;
   (*list)[idx].enforce_minrange  = enforce_minrange_enable;
   (*list)[idx].enforce_maxrange  = enforce_maxrange_enable;

   (*list)[list_info->index - 1].flags |= SD_FLAG_HAS_RANGE;
}

int menu_setting_generic(rarch_setting_t *setting, size_t idx, bool wraparound)
{
   uint64_t flags = setting->flags;
   if (setting_generic_action_ok_default(setting, idx, wraparound) != 0)
      return -1;

   if (setting->change_handler)
      setting->change_handler(setting);

   if ((flags & SD_FLAG_EXIT) && setting->cmd_trigger_event_triggered)
   {
      setting->cmd_trigger_event_triggered = false;
      return -1;
   }

   return 0;
}

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound)
{
   if (!setting)
      return -1;

   switch (setting->type)
   {
      case ST_PATH:
         if (action == MENU_ACTION_OK)
         {
            menu_displaylist_info_t  info;
            settings_t *settings          = config_get_ptr();
            file_list_t       *menu_stack = menu_entries_get_menu_stack_ptr(0);
            const char      *name         = setting->name;
            size_t selection              = menu_navigation_get_selection();

            menu_displaylist_info_init(&info);

            info.path                     = strdup(setting->default_value.string);
            info.label                    = strdup(name);
            info.type                     = type;
            info.directory_ptr            = selection;
            info.list                     = menu_stack;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info, settings))
               menu_displaylist_process(&info);

            menu_displaylist_info_free(&info);
         }
         /* fall-through. */
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_SIZE:
      case ST_HEX:
      case ST_FLOAT:
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_DIR:
      case ST_BIND:
      case ST_ACTION:
         {
            int ret                       = -1;
            size_t selection              = menu_navigation_get_selection();
            switch (action)
            {
               case MENU_ACTION_UP:
                  if (setting->action_up)
                     ret = setting->action_up(setting);
                  break;
               case MENU_ACTION_DOWN:
                  if (setting->action_down)
                     ret = setting->action_down(setting);
                  break;
               case MENU_ACTION_LEFT:
                  if (setting->action_left)
                  {
                     ret = setting->action_left(setting, selection, false);
                     menu_driver_ctl(
                           RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH,
                           NULL);
                     menu_driver_ctl(
                           RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE,
                           NULL);
                  }
                  break;
               case MENU_ACTION_RIGHT:
                  if (setting->action_right)
                  {
                     ret = setting->action_right(setting, selection, false);
                     menu_driver_ctl(
                           RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH,
                           NULL);
                     menu_driver_ctl(
                           RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE,
                           NULL);
                  }
                  break;
               case MENU_ACTION_SELECT:
                  if (setting->action_select)
                     ret = setting->action_select(setting, selection, true);
                  break;
               case MENU_ACTION_OK:
                  if (setting->action_ok)
                     ret = setting->action_ok(setting, selection, false);
                  break;
               case MENU_ACTION_CANCEL:
                  if (setting->action_cancel)
                     ret = setting->action_cancel(setting);
                  break;
               case MENU_ACTION_START:
                  if (setting->action_start)
                     ret = setting->action_start(setting);
                  break;
            }

            if (ret == 0)
               return menu_setting_generic(setting, selection, wraparound);
         }
         break;
      default:
         break;
   }

   return -1;
}

/**
 * menu_setting_find:
 * @settings           : pointer to settings
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t *menu_setting_find(const char *label)
{
   rarch_setting_t *setting = NULL;
   rarch_setting_t **list   = &setting;

   if (!label)
      return NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting)
      return NULL;

   for (; setting->type != ST_NONE; (*list = *list + 1))
   {
      const char *name              = setting->name;
      const char *short_description = setting->short_description;

      if (
            string_is_equal(label, name) &&
            (setting->type <= ST_GROUP))
      {
         if (string_is_empty(short_description))
            break;

         if (setting->read_handler)
            setting->read_handler(setting);

         return setting;
      }
   }

   return NULL;
}

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums enum_idx)
{
   rarch_setting_t *setting = NULL;
   rarch_setting_t **list   = &setting;

   if (enum_idx == 0)
      return NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting)
      return NULL;
   for (; setting->type != ST_NONE; (*list = *list + 1))
   {
      if (  setting->enum_idx == enum_idx &&
            setting->type <= ST_GROUP)
      {
         const char *short_description = setting->short_description;
         if (string_is_empty(short_description))
            return NULL;

         if (setting->read_handler)
            setting->read_handler(setting);

         return setting;
      }
   }

   return NULL;
}

int menu_setting_set(unsigned type, unsigned action, bool wraparound)
{
   int ret                    = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf, selection) : NULL;

   if (!cbs)
      return 0;

   ret = menu_action_handle_setting(cbs->setting,
         type, action, wraparound);

   if (ret == -1)
      return 0;
   return ret;
}

/**
 * setting_action_start_savestates:
 * @data               : pointer to setting
 *
 * Function callback for 'Savestate' action's 'Action Start'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_action_start_bind_device(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   configuration_set_uint(settings,
         settings->uints.input_joypad_index[setting->index_offset], setting->index_offset);
   return 0;
}

static int setting_action_start_custom_viewport_width(rarch_setting_t *setting)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
         custom->width = ((custom->width + geom->base_height - 1) /
               geom->base_height) * geom->base_height;
      else
         custom->width = ((custom->width + geom->base_width - 1) /
               geom->base_width) * geom->base_width;
   }
   else
      custom->width = vp.full_width - custom->x;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

static int setting_action_start_custom_viewport_height(rarch_setting_t *setting)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->bools.video_scale_integer)
   {
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
      {
         custom->height = ((custom->height + geom->base_width - 1) /
               geom->base_width) * geom->base_width;
      }
      else
         custom->height = ((custom->height + geom->base_height - 1) /
               geom->base_height) * geom->base_height;
   }
   else
      custom->height = vp.full_height - custom->y;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

static int setting_action_start_analog_dpad_mode(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   *setting->value.target.unsigned_integer = 0;

   return 0;
}

static int setting_action_start_libretro_device_type(rarch_setting_t *setting)
{
   retro_ctx_controller_info_t pad;
   unsigned port = 0;

   if (!setting || setting_generic_action_start_default(setting) != 0)
      return -1;

   port             = setting->index_offset;

   input_config_set_device(port, RETRO_DEVICE_JOYPAD);

   pad.port         = port;
   pad.device       = RETRO_DEVICE_JOYPAD;

   core_set_controller_port_device(&pad);

   return 0;
}

static int setting_action_start_input_remap_port(rarch_setting_t *setting)
{
   bool refresh         = false;
   settings_t *settings = config_get_ptr();
   unsigned port;

   if (!setting)
      return -1;

   port                                    = setting->index_offset;
   settings->uints.input_remap_ports[port] = port;

   /* Must be called whenever settings->uints.input_remap_ports
    * is modified */
   input_remapping_update_port_map();

   /* Changing mapped port may leave a core port unused;
    * reinitialise controllers to ensure that any such
    * ports are set to 'RETRO_DEVICE_NONE' */
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static int setting_action_start_video_refresh_rate_auto(
      rarch_setting_t *setting)
{
   (void)setting;

   video_driver_monitor_reset();
   return 0;
}

static int setting_action_start_video_refresh_rate_polled(
      rarch_setting_t *setting)
{
   /* Relay action to ok to prevent duplicate notifications */
   return setting_action_ok_video_refresh_rate_polled(setting, 0, false);
}

static int setting_action_start_mouse_index(rarch_setting_t *setting)
{
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   settings->uints.input_mouse_index[setting->index_offset] = setting->index_offset;
   settings->modified = true;
   return 0;
}

/**
 ******* ACTION TOGGLE CALLBACK FUNCTIONS *******
**/

static int setting_action_right_analog_dpad_mode(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned port = 0;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   settings->modified                     = true;
   settings->uints.input_analog_dpad_mode[port] =
      (settings->uints.input_analog_dpad_mode[port] + 1)
      % ANALOG_DPAD_LAST;

   return 0;
}

static int setting_action_right_libretro_device_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh = false;
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;

   if (!setting)
      return -1;

   port           = setting->index_offset;
   types          = libretro_device_get_size(devices, ARRAY_SIZE(devices), port);
   current_device = input_config_get_device(port);
   current_idx    = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device != devices[i])
         continue;

      current_idx = i;
      break;
   }

   current_device = devices
      [(current_idx + 1) % types];

   input_config_set_device(port, current_device);

   pad.port   = port;
   pad.device = current_device;

   core_set_controller_port_device(&pad);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static int setting_action_right_input_remap_port(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool refresh         = false;
   unsigned port        = 0;
   settings_t *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   if (settings->uints.input_remap_ports[port] < MAX_USERS - 1)
      settings->uints.input_remap_ports[port]++;
   else
      settings->uints.input_remap_ports[port] = 0;

   /* Must be called whenever settings->uints.input_remap_ports
    * is modified */
   input_remapping_update_port_map();

   /* Changing mapped port may leave a core port unused;
    * reinitialise controllers to ensure that any such
    * ports are set to 'RETRO_DEVICE_NONE' */
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0;
}

static int setting_action_right_bind_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned index_offset;
   unsigned               *p = NULL;
   unsigned max_devices      = input_config_get_device_count();
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   index_offset = setting->index_offset;

   p = &settings->uints.input_joypad_index[index_offset];

   if (*p < max_devices)
      (*p)++;

   return 0;
}

static int setting_action_right_mouse_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   if (settings->uints.input_mouse_index[setting->index_offset] < MAX_USERS - 1)
      ++settings->uints.input_mouse_index[setting->index_offset];
   else
      settings->uints.input_mouse_index[setting->index_offset] = 0;

   settings->modified = true;
   return 0;
}

/**
 ******* ACTION OK CALLBACK FUNCTIONS *******
**/

static void
setting_get_string_representation_st_float_video_refresh_rate_polled(
      rarch_setting_t *setting, char *s, size_t len)
{
    snprintf(s, len, "%.3f Hz", video_driver_get_refresh_rate());
}

static void
setting_get_string_representation_st_float_video_refresh_rate_auto(
      rarch_setting_t *setting, char *s, size_t len)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;
   if (!setting)
      return;

   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      gfx_animation_t *p_anim   = anim_get_ptr();
      snprintf(s, len, "%.3f Hz (%.1f%% dev, %u samples)",
            video_refresh_rate, 100.0 * deviation, sample_points);
      GFX_ANIMATION_SET_ACTIVE(p_anim);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

#ifdef HAVE_LIBNX
static void get_string_representation_split_joycon(rarch_setting_t *setting, char *s,
      size_t len)
{
   settings_t      *settings = config_get_ptr();
   unsigned index_offset     = setting->index_offset;
   unsigned map              = settings->uints.input_split_joycon[index_offset];

   if (map == 0)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
}
#endif

static void get_string_representation_bind_device(rarch_setting_t *setting, char *s,
      size_t len)
{
   unsigned index_offset, map = 0;
   unsigned max_devices      = input_config_get_device_count();
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return;

   index_offset = setting->index_offset;
   map          = settings->uints.input_joypad_index[index_offset];

   if (map < max_devices)
   {
      const char *device_name = input_config_get_device_display_name(map) ?
         input_config_get_device_display_name(map) : input_config_get_device_name(map);

      if (!string_is_empty(device_name))
      {
         unsigned idx = input_config_get_device_name_index(map);

         /*if idx is non-zero, it's part of a set*/
         if ( idx > 0)
            snprintf(s, len,
                  "%s (#%u)",
                  device_name,
                  idx);
         else
            strlcpy(s, device_name, len);
      }
      else
         snprintf(s, len, "%s (%s %u)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
               map);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
}

static void get_string_representation_mouse_index(rarch_setting_t *setting, char *s,
      size_t len)
{
   unsigned index_offset, map = 0;
   unsigned max_devices       = MAX_USERS;
   settings_t      *settings  = config_get_ptr();

   if (!setting)
      return;

   index_offset = setting->index_offset;
   map          = settings->uints.input_mouse_index[index_offset];

   if (map < max_devices)
   {
      const char *device_name = input_config_get_mouse_display_name(map);

      if (!string_is_empty(device_name))
         strlcpy(s, device_name, len);
      else if (map > 0)
         snprintf(s, len,
               "%s (#%u)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               map);
      else
         snprintf(s, len,
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE));
   }
   else
      snprintf(s, len,
         "#%u", map);
}

static void read_handler_audio_rate_control_delta(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || setting->enum_idx == MSG_UNKNOWN)
      return;

   *setting->value.target.fraction = *(audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA));
   if (*setting->value.target.fraction < 0.0005)
   {
      configuration_set_bool(settings, settings->bools.audio_rate_control, false);
      audio_set_float(AUDIO_ACTION_RATE_CONTROL_DELTA, 0.0f);
   }
   else
   {
      configuration_set_bool(settings, settings->bools.audio_rate_control, true);
      audio_set_float(AUDIO_ACTION_RATE_CONTROL_DELTA, *setting->value.target.fraction);
   }
}

static void general_read_handler(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || setting->enum_idx == MSG_UNKNOWN)
      return;

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         *setting->value.target.fraction = settings->floats.audio_max_timing_skew;
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         *setting->value.target.fraction = settings->floats.video_refresh_rate;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_index[setting->enum_idx - MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX];
         break;
      default:
         break;
   }
}

static enum event_command write_handler_get_cmd(rarch_setting_t *setting)
{
   if (setting && setting->cmd_trigger_idx != CMD_EVENT_NONE)
   {
      if (setting->flags & SD_FLAG_EXIT)
         if (*setting->value.target.boolean)
            *setting->value.target.boolean = false;

      if (setting->cmd_trigger_event_triggered ||
            (setting->flags & SD_FLAG_CMD_APPLY_AUTO))
         return setting->cmd_trigger_idx;
   }
   return CMD_EVENT_NONE;
}

static void write_handler_audio_rate_control_delta(rarch_setting_t *setting)
{
   settings_t *settings         = config_get_ptr();
   enum event_command rarch_cmd = CMD_EVENT_NONE;

   if (!setting)
      return;

   rarch_cmd                    = write_handler_get_cmd(setting);

   if (*setting->value.target.fraction < 0.0005)
   {
      configuration_set_bool(settings,
            settings->bools.audio_rate_control, false);
      audio_set_float(AUDIO_ACTION_RATE_CONTROL_DELTA, 0.0f);
   }
   else
   {
      configuration_set_bool(settings, settings->bools.audio_rate_control, true);
      audio_set_float(AUDIO_ACTION_RATE_CONTROL_DELTA, *setting->value.target.fraction);
   }

   if (rarch_cmd || setting->cmd_trigger_event_triggered)
      command_event(rarch_cmd, NULL);
}

static void write_handler_logging_verbosity(rarch_setting_t *setting)
{
   enum event_command rarch_cmd = CMD_EVENT_NONE;

   if (!setting)
      return;

   rarch_cmd                    = write_handler_get_cmd(setting);

   if (!verbosity_is_enabled())
   {
      settings_t *settings = config_get_ptr();
      rarch_log_file_init(
            settings->bools.log_to_file,
            settings->bools.log_to_file_timestamp,
            settings->paths.log_dir);
      verbosity_enable();
   }
   else
   {
      verbosity_disable();
      rarch_log_file_deinit();
   }
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);

   if (rarch_cmd || setting->cmd_trigger_event_triggered)
      command_event(rarch_cmd, NULL);
}

static void general_write_handler(rarch_setting_t *setting)
{
   enum event_command rarch_cmd = CMD_EVENT_NONE;

   if (!setting)
      return;

   rarch_cmd                    = write_handler_get_cmd(setting);

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            if (*setting->value.target.boolean)
            {
               bool refresh                = false;
               menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
               menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
            }
            else
            {
               bool refresh                = false;
               settings_t *settings        = config_get_ptr();
               struct video_shader *shader = menu_shader_get();

               shader->passes              = 0;
               shader->modified            = true;

               menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
               menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
               command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
               configuration_set_bool(settings, settings->bools.video_shader_enable, false);
            }
         }
#endif
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         {
            if (*setting->value.target.boolean)
               task_queue_set_threaded();
            else
               task_queue_unset_threaded();
         }
         break;
      case MENU_ENUM_LABEL_GAMEMODE_ENABLE:
         if (frontend_driver_has_gamemode())
         {
            bool on = *setting->value.target.boolean;

            if (!frontend_driver_set_gamemode(on) && on)
            {
               settings_t *settings = config_get_ptr();

               /* If we failed to enable game mode, display
                * a notification and force disable the feature */
               runloop_msg_queue_push(
#ifdef __linux__
                     msg_hash_to_str(MSG_FAILED_TO_ENTER_GAMEMODE_LINUX),
#else
                     msg_hash_to_str(MSG_FAILED_TO_ENTER_GAMEMODE),
#endif
                     1, 180, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               configuration_set_bool(settings,
                     settings->bools.gamemode_enable, false);
            }
         }
         break;
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         core_set_poll_type(*setting->value.target.integer);
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         {
            video_viewport_t vp;
            struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
            video_viewport_t            *custom  = video_viewport_get_custom();
            struct retro_game_geometry     *geom = (struct retro_game_geometry*)
               &av_info->geometry;

            video_driver_get_viewport_info(&vp);

            if (*setting->value.target.boolean)
            {
               unsigned int rotation = retroarch_get_rotation();

               custom->x             = 0;
               custom->y             = 0;

               if (rotation % 2)
               {
                  custom->width  = ((custom->width + geom->base_height - 1) / geom->base_height)  * geom->base_height;
                  custom->height = ((custom->height + geom->base_width - 1) / geom->base_width)   * geom->base_width;
               }
               else
               {
                  custom->width  = ((custom->width + geom->base_width   - 1) / geom->base_width)  * geom->base_width;
                  custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;
               }
               aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (float)custom->width / custom->height;
            }
         }
         break;
      case MENU_ENUM_LABEL_HELP:
         if (*setting->value.target.boolean)
         {
            menu_displaylist_info_t info;
            settings_t *settings         = config_get_ptr();
            file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);

            menu_displaylist_info_init(&info);

            info.enum_idx                = MENU_ENUM_LABEL_HELP;
            info.label                   = strdup(
                  msg_hash_to_str(MENU_ENUM_LABEL_HELP));
            info.list                    = menu_stack;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info, settings))
               menu_displaylist_process(&info);
            menu_displaylist_info_free(&info);
            setting_set_with_string_representation(setting, "false");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         {
            settings_t *settings         = config_get_ptr();
            configuration_set_float(settings,
                  settings->floats.audio_max_timing_skew,
                  *setting->value.target.fraction);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, setting->value.target.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = CMD_EVENT_VIDEO_SET_BLOCKING_STATE;
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
         driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, setting->value.target.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = CMD_EVENT_VIDEO_SET_BLOCKING_STATE;
         break;
#if defined(DINGUX) && defined(DINGUX_BETA)
      case MENU_ENUM_LABEL_VIDEO_DINGUX_REFRESH_RATE:
         {
            settings_t *settings         = config_get_ptr();
            enum dingux_refresh_rate 
               current_refresh_rate      = DINGUX_REFRESH_RATE_60HZ;
            enum dingux_refresh_rate 
               target_refresh_rate       =
                  (enum dingux_refresh_rate)settings->uints.video_dingux_refresh_rate;
            bool refresh_rate_valid                       = false;

            /* Get current refresh rate */
            refresh_rate_valid = dingux_get_video_refresh_rate(&current_refresh_rate);

            /* Check if refresh rate has changed */
            if (!refresh_rate_valid ||
                (current_refresh_rate != target_refresh_rate))
            {
               /* Get floating point representation of
                * new refresh rate */
               float hw_refresh_rate = 60.0f;

               switch (target_refresh_rate)
               {
                  case DINGUX_REFRESH_RATE_50HZ:
                     hw_refresh_rate = 50.0f;
                  default:
                     break;
               }

               /* Manually update 'settings->floats.video_refresh_rate'
                * (required for correct timing adjustments when
                * reinitialising drivers) */
               configuration_set_float(settings,
                     settings->floats.video_refresh_rate,
                     hw_refresh_rate);

               /* Trigger driver reinitialisation */
               rarch_cmd = CMD_EVENT_REINIT;
            }
         }
         break;
#endif
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         {
            settings_t *settings         = config_get_ptr();
            settings->modified           = true;
            settings->floats.video_scale = roundf(*setting->value.target.fraction);

            if (!settings->bools.video_fullscreen)
               rarch_cmd = CMD_EVENT_REINIT;
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_ENABLE:
         {
            settings_t *settings             = config_get_ptr();
            settings->modified               = true;
            settings->bools.video_hdr_enable = *setting->value.target.boolean;

            rarch_cmd = CMD_EVENT_REINIT;            
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_MAX_NITS:
         {
            settings_t *settings                = config_get_ptr();
            settings->modified                  = true;
            settings->floats.video_hdr_max_nits = roundf(*setting->value.target.fraction);

            video_driver_set_hdr_max_nits(settings->floats.video_hdr_max_nits);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS:
         {
            settings_t *settings                         = config_get_ptr();
            settings->modified                           = true;
            settings->floats.video_hdr_paper_white_nits  = roundf(*setting->value.target.fraction);

            video_driver_set_hdr_paper_white_nits(settings->floats.video_hdr_paper_white_nits);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_CONTRAST:
         {
            settings_t *settings                = config_get_ptr();
            settings->modified                  = true;
            settings->floats.video_hdr_display_contrast = *setting->value.target.fraction;

            video_driver_set_hdr_contrast(settings->floats.video_hdr_display_contrast);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT:
         {
            settings_t *settings                   = config_get_ptr();
            settings->modified                     = true;
            settings->bools.video_hdr_expand_gamut = *setting->value.target.boolean;;

            video_driver_set_hdr_expand_gamut(settings->bools.video_hdr_expand_gamut);
         }
         break;
      case MENU_ENUM_LABEL_INPUT_MAX_USERS:
         command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
      case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
         {
            settings_t *settings         = config_get_ptr();
            settings->modified           = true;
            settings->uints.input_joypad_index[setting->enum_idx - MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX]             = *setting->value.target.integer;
         }
         break;
      case MENU_ENUM_LABEL_LOG_TO_FILE:
         if (verbosity_is_enabled())
         {
            settings_t *settings       = config_get_ptr();
            bool log_to_file           = settings->bools.log_to_file;
            bool log_to_file_timestamp = settings->bools.log_to_file_timestamp;
            const char *log_dir        = settings->paths.log_dir;
            bool logging_to_file       = is_logging_to_file();

            if (log_to_file && !logging_to_file)
               rarch_log_file_init(
                     log_to_file,
                     log_to_file_timestamp,
                     log_dir);
            else if (!log_to_file && logging_to_file)
               rarch_log_file_deinit();
         }
         retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL);
         break;
      case MENU_ENUM_LABEL_LOG_DIR:
      case MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP:
         if (verbosity_is_enabled() && is_logging_to_file())
         {
            settings_t *settings       = config_get_ptr();
            bool log_to_file           = settings->bools.log_to_file;
            bool log_to_file_timestamp = settings->bools.log_to_file_timestamp;
            const char *log_dir        = settings->paths.log_dir;

            rarch_log_file_deinit();
            rarch_log_file_init(
                  log_to_file,
                  log_to_file_timestamp,
                  log_dir);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
      case MENU_ENUM_LABEL_VIDEO_CTX_SCALING:
#if defined(DINGUX)
      case MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_FILTER_TYPE:
#if defined(RS90) || defined(MIYOO)
      case MENU_ENUM_LABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE:
#endif
#endif
         {
            settings_t *settings       = config_get_ptr();
            video_driver_set_filtering(1, settings->bools.video_smooth,
                  settings->bools.video_ctx_scaling);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         {
            video_viewport_t vp;
	    rarch_system_info_t *system          = &runloop_state_get_ptr()->system;
            struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
            video_viewport_t            *custom  = video_viewport_get_custom();
            struct retro_game_geometry     *geom = (struct retro_game_geometry*)
               &av_info->geometry;

            if (system)
            {
               unsigned int rotation = retroarch_get_rotation();

               video_driver_set_rotation(
                     (*setting->value.target.unsigned_integer +
                      system->rotation) % 4);

               /* Update Custom Aspect Ratio values */
               video_driver_get_viewport_info(&vp);
               custom->x      = 0;
               custom->y      = 0;
               /* Round down when rotation is "horizontal", round up when rotation is "vertical"
                  to avoid expanding viewport each time user rotates */
               if (rotation % 2)
               {
                  custom->width  = MAX(1,(custom->width / geom->base_height))  * geom->base_height;
                  custom->height = MAX(1,(custom->height/ geom->base_width ))  * geom->base_width;
               }
               else
               {
                  custom->width  = ((custom->width + geom->base_width   - 1) / geom->base_width)  * geom->base_width;
                  custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;
               }
               aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (float)custom->width / custom->height;

               /* Update Aspect Ratio (only useful for 1:1 PAR) */
               video_driver_set_aspect_ratio();
            }
         }
         break;
      case MENU_ENUM_LABEL_SCREEN_ORIENTATION:
         {
#ifndef ANDROID
             /* FIXME: Changing at runtime on Android causes setting to somehow be incremented again, many times */
             video_display_server_set_screen_orientation(
                   (enum rotation)(*setting->value.target.unsigned_integer));
#endif
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         audio_set_float(AUDIO_ACTION_VOLUME_GAIN, *setting->value.target.fraction);
         break;
      case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
#ifdef HAVE_AUDIOMIXER
         audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, *setting->value.target.fraction);
#endif
         break;
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
      case MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE:
      case MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT:
      case MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH:
         rarch_cmd = CMD_EVENT_AUDIO_REINIT;
         break;
      case MENU_ENUM_LABEL_PAL60_ENABLE:
         {
            global_t *global             = global_get_ptr();
            if (*setting->value.target.boolean && global->console.screen.pal_enable)
               rarch_cmd = CMD_EVENT_REINIT;
            else
               setting_set_with_string_representation(setting, "false");
         }
         break;
      case MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE:
         if (*setting->value.target.boolean)
         {
#if !defined(__PSL1GHT__) && defined(__PS3__)
            cellSysutilEnableBgmPlayback();
#endif
         }
         else
         {
#if !defined(__PSL1GHT__) && defined(__PS3__)
            cellSysutilDisableBgmPlayback();
#endif
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_ENABLE_MENU:
         {
#ifdef HAVE_AUDIOMIXER
            settings_t *settings       = config_get_ptr();
            if (settings->bools.audio_enable_menu)
               audio_driver_load_system_sounds();
            else
               audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif
         }
         break;
      case MENU_ENUM_LABEL_MENU_SOUND_BGM:
         {
#ifdef HAVE_AUDIOMIXER
            settings_t *settings       = config_get_ptr();
            if (settings->bools.audio_enable_menu)
            {
               if (settings->bools.audio_enable_menu_bgm)
                  audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
               else
                  audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
            }
#endif
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY:
         {
            settings_t *settings       = config_get_ptr();
            video_display_server_set_window_opacity(settings->uints.video_window_opacity);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS:
         {
            settings_t *settings       = config_get_ptr();
            video_display_server_set_window_decorations(settings->bools.video_window_show_decorations);
         }
         break;
      case MENU_ENUM_LABEL_MIDI_INPUT:
         {
            settings_t *settings       = config_get_ptr();
            midi_driver_set_input(settings->arrays.midi_input);
         }
         break;
      case MENU_ENUM_LABEL_MIDI_OUTPUT:
         {
            settings_t *settings       = config_get_ptr();
            midi_driver_set_output(settings,
                  settings->arrays.midi_output);
         }
         break;
      case MENU_ENUM_LABEL_MIDI_VOLUME:
         {
            settings_t *settings       = config_get_ptr();
            midi_driver_set_volume(settings->uints.midi_volume);
         }
         break;
      case MENU_ENUM_LABEL_SUSTAINED_PERFORMANCE_MODE:
         {
            settings_t *settings       = config_get_ptr();
            frontend_driver_set_sustained_performance_mode(settings->bools.sustained_performance_mode);
         }
         break;
      case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
         {
            rarch_setting_t *buffer_size_setting = menu_setting_find_enum(MENU_ENUM_LABEL_REWIND_BUFFER_SIZE);
            if (buffer_size_setting)
               buffer_size_setting->step = (*setting->value.target.unsigned_integer)*1024*1024;
         }
         break;
      case MENU_ENUM_LABEL_CHEAT_MEMORY_SEARCH_SIZE:
#ifdef HAVE_CHEATS
         {
            rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_VALUE);
            if (setting)
            {
               *(setting->value.target.unsigned_integer) = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size);
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size);
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION);
            if (setting)
            {
               int max_bit_position;
               *setting->value.target.unsigned_integer = 0;
               max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0;
               setting->max     = max_bit_position;
            }
         }
#endif
         break;
      case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
#ifdef HAVE_CHEATS
         {
            rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            }
         }
#endif
         break;
      case MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE:
         {
            unsigned new_capacity;
            settings_t *settings       = config_get_ptr();
            int content_favorites_size = settings->ints.content_favorites_size;

            /* Get new size */
            if (content_favorites_size < 0)
               new_capacity = COLLECTION_SIZE;
            else
               new_capacity = (unsigned)content_favorites_size;

            /* Check whether capacity has changed */
            if (new_capacity != playlist_capacity(g_defaults.content_favorites))
            {
               /* Remove excess entries, if required */
               while (playlist_size(g_defaults.content_favorites) > new_capacity)
                  playlist_delete_index(
                        g_defaults.content_favorites,
                        playlist_size(g_defaults.content_favorites) - 1);

               /* In all cases, need to close and reopen
                * playlist file (to update maximum capacity) */
               rarch_favorites_deinit();
               rarch_favorites_init();
            }
         }
         break;
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM:
         /* Ensure that custom system name includes no
          * invalid characters */
         manual_content_scan_scrub_system_name_custom();
         break;
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_FILE_EXTS:
         /* Ensure that custom file extension list includes
          * no period (full stop) characters, and converts
          * string to lower case */
         manual_content_scan_scrub_file_exts_custom();
         break;
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE:
         /* Ensure that DAT file path is valid
          * (cannot check file contents here - DAT
          * files are too large to load in the UI
          * thread - have to wait until the scan task
          * is pushed) */
         switch (manual_content_scan_validate_dat_file_path())
         {
            case MANUAL_CONTENT_SCAN_DAT_FILE_INVALID:
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID),
                     1, 100, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               break;
            case MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE:
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE),
                     1, 100, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               break;
            default:
               /* No action required */
               break;
         }
         break;
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         {
            settings_t *settings       = config_get_ptr();
            /* when changing the username, clear out the password and token */
            settings->arrays.cheevos_password[0] = '\0';
            settings->arrays.cheevos_token[0] = '\0';
         }
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         {
            settings_t *settings       = config_get_ptr();
            /* when changing the password, clear out the token */
            settings->arrays.cheevos_token[0] = '\0';
         }
         break;
      case MENU_ENUM_LABEL_CHEEVOS_UNLOCK_SOUND_ENABLE:
         {
#ifdef HAVE_AUDIOMIXER
            settings_t *settings       = config_get_ptr();
            if (settings->bools.cheevos_unlock_sound_enable)
               audio_driver_load_system_sounds();
#endif
         }
         break;
      case MENU_ENUM_LABEL_INPUT_SENSORS_ENABLE:
         /* When toggling sensor input off, ensure
          * that all sensors are actually disabled */
         if (!*setting->value.target.boolean)
         {
            unsigned i;

            for (i = 0; i < DEFAULT_MAX_PADS; i++)
            {
               /* Event rate does not matter when disabling
                * sensors - set to zero */
               input_set_sensor_state(i,
                     RETRO_SENSOR_ACCELEROMETER_DISABLE, 0);
               input_set_sensor_state(i,
                     RETRO_SENSOR_GYROSCOPE_DISABLE, 0);
               input_set_sensor_state(i,
                     RETRO_SENSOR_ILLUMINANCE_DISABLE, 0);
            }
         }
         break;
      case MENU_ENUM_LABEL_BRIGHTNESS_CONTROL:
         {
            frontend_driver_set_screen_brightness(
               *setting->value.target.unsigned_integer);
         }
         break;
      case MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN:
         {
            input_set_rumble_gain(
               *setting->value.target.unsigned_integer);
         }
         break;
      case MENU_ENUM_LABEL_WIFI_ENABLED:
#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
         if (*setting->value.target.boolean)
            task_push_wifi_enable(NULL);
         else
            task_push_wifi_disable(NULL);
#endif
#endif
         break;
      case MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE:
         {
            settings_t *settings           = config_get_ptr();
            const char *dir_libretro       = settings->paths.directory_libretro;
            const char *path_libretro_info = settings->paths.path_libretro_info;

            /* When enabling the core info cache,
             * force a cache refresh on the next
             * core info initialisation */
            if (*setting->value.target.boolean)
               if (!core_info_cache_force_refresh(!string_is_empty(path_libretro_info) ?
                     path_libretro_info : dir_libretro))
               {
                  /* core_info_cache_force_refresh() will fail
                   * if we cannot write to the the core_info
                   * directory. This will typically only happen
                   * on platforms where the core_info directory
                   * is explicitly (and intentionally) placed on
                   * read-only storage. In this case, core info
                   * caching cannot function correctly anyway,
                   * so we simply force-disable the feature */
                  configuration_set_bool(settings,
                        settings->bools.core_info_cache_enable, false);
                  runloop_msg_queue_push(
                        msg_hash_to_str(MSG_CORE_INFO_CACHE_UNSUPPORTED),
                        1, 100, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH:
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         {
            /* Whenever custom viewport dimensions are
             * changed, ASPECT_RATIO_CUSTOM must be
             * recalculated */
            video_viewport_t *custom_vp = video_viewport_get_custom();
            float default_aspect        = aspectratio_lut[ASPECT_RATIO_CORE].value;

            aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (custom_vp && custom_vp->width && custom_vp->height) ?
                     ((float)custom_vp->width / (float)custom_vp->height) :
                           default_aspect;
         }
         break;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
      case MENU_ENUM_LABEL_RUN_AHEAD_ENABLED:
      case MENU_ENUM_LABEL_RUN_AHEAD_FRAMES:
      case MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE:
         {
            settings_t *settings              = config_get_ptr();
            bool run_ahead_enabled            = settings->bools.run_ahead_enabled;
            unsigned run_ahead_frames         = settings->uints.run_ahead_frames;
            bool run_ahead_secondary_instance = settings->bools.run_ahead_secondary_instance;

            /* If any changes here will cause second
             * instance runahead to be enabled, must
             * re-apply cheats to ensure that they
             * propagate to the newly-created secondary
             * core */
            if (run_ahead_enabled &&
                (run_ahead_frames > 0) &&
                run_ahead_secondary_instance &&
                !retroarch_ctl(RARCH_CTL_IS_SECOND_CORE_LOADED, NULL) &&
                retroarch_ctl(RARCH_CTL_IS_SECOND_CORE_AVAILABLE, NULL) &&
                command_event(CMD_EVENT_LOAD_SECOND_CORE, NULL))
               command_event(CMD_EVENT_CHEATS_APPLY, NULL);
         }
         break;
#endif
      default:
         /* Special cases */

         /* > Mapped Port (virtual -> 'physical' port mapping)
          *   Occupies a range of enum indices, so cannot
          *   simply switch on the value */
         if ((setting->enum_idx >= MENU_ENUM_LABEL_INPUT_REMAP_PORT) &&
             (setting->enum_idx <= MENU_ENUM_LABEL_INPUT_REMAP_PORT_LAST))
         {
            /* Must be called whenever settings->uints.input_remap_ports
             * is modified */
            input_remapping_update_port_map();

            /* Changing mapped port may leave a core port unused;
             * reinitialise controllers to ensure that any such
             * ports are set to 'RETRO_DEVICE_NONE' */
            command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
         }

         break;
   }

   if (rarch_cmd || setting->cmd_trigger_event_triggered)
      command_event(rarch_cmd, NULL);
}

static void frontend_log_level_change_handler(rarch_setting_t *setting)
{
   if (!setting)
      return;

   verbosity_set_log_level(*setting->value.target.unsigned_integer);
}

#ifdef HAVE_OVERLAY
static void overlay_enable_toggle_change_handler(rarch_setting_t *setting)
{
   settings_t *settings  = config_get_ptr();
   bool input_overlay_hide_in_menu = settings ? settings->bools.input_overlay_hide_in_menu : false;

   if (!setting)
      return;

   if (input_overlay_hide_in_menu)
   {
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
      return;
   }

   if (setting->value.target.boolean)
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   else
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
}

static void overlay_auto_rotate_toggle_change_handler(rarch_setting_t *setting)
{
   settings_t *settings  = config_get_ptr();

   if (!setting || !settings)
      return;

   /* This is very simple...
    * The menu is currently active, so if:
    * - Overlays are enabled
    * - Overlays are not hidden in menus
    * ...we just need to de-initialise then
    * initialise the current overlay and the
    * auto-rotate setting will be applied
    * (i.e. There's no need to explicitly
    * call the internal 'rotate overlay'
    * function - saves having to expose it
    * via the API) */
   if (settings->bools.input_overlay_enable &&
       !settings->bools.input_overlay_hide_in_menu)
   {
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   }
}
#endif

#ifdef HAVE_VIDEO_LAYOUT
static void change_handler_video_layout_enable(rarch_setting_t *setting)
{
   if (*setting->value.target.boolean)
   {
      settings_t *settings = config_get_ptr();
      void         *driver = video_driver_get_ptr();

      video_layout_init(driver, video_driver_layout_render_interface());
      video_layout_load(settings->paths.path_video_layout);
      video_layout_view_select(settings->uints.video_layout_selected_view);
   }
   else
   {
      video_layout_deinit();
   }
}

static void change_handler_video_layout_path(rarch_setting_t *setting)
{
   settings_t *settings = config_get_ptr();
   configuration_set_uint(settings,
         settings->uints.video_layout_selected_view, 0);

   video_layout_load(setting->value.target.string);
}

static void change_handler_video_layout_selected_view(rarch_setting_t *setting)
{
   unsigned *v = setting->value.target.unsigned_integer;
   *v = video_layout_view_select(*v);
}
#endif

#ifdef HAVE_CHEEVOS
static void achievement_hardcore_mode_write_handler(rarch_setting_t *setting)
{
   rcheevos_hardcore_enabled_changed();
}

static void achievement_leaderboards_enabled_write_handler(rarch_setting_t* setting)
{
   rcheevos_leaderboards_enabled_changed();
}

static void achievement_leaderboards_get_string_representation(rarch_setting_t* setting, char* s, size_t len)
{
   const char* value = setting->value.target.string;
#if defined(HAVE_GFX_WIDGETS)
   if (string_is_equal(value, "true"))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ENABLED), len);
   else if (string_is_equal(value, "trackers"))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY), len);
   else if (string_is_equal(value, "notifications"))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
#else
   /* using these enum strings makes the widget behave like a boolean toggle */
   if (string_is_equal(value, "true"))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
#endif
}

#endif

static void update_streaming_url_write_handler(rarch_setting_t *setting)
{
   recording_driver_update_streaming_url();
}

#ifdef HAVE_LAKKA
static void systemd_service_toggle(const char *path, char *unit, bool enable)
{
   pid_t pid    = fork();
   char* args[] = {(char*)"systemctl",
                   enable ? (char*)"start" : (char*)"stop",
                   unit,
                   NULL};

   if (pid == 0)
   {
      if (enable)
         filestream_close(filestream_open(path,
                  RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE));
      else
         filestream_delete(path);

      execvp(args[0], args);
   }
}

static void ssh_enable_toggle_change_handler(rarch_setting_t *setting)
{
   systemd_service_toggle(LAKKA_SSH_PATH, (char*)"sshd.service",
         *setting->value.target.boolean);
}

static void samba_enable_toggle_change_handler(rarch_setting_t *setting)
{
   systemd_service_toggle(LAKKA_SAMBA_PATH, (char*)"smbd.service",
         *setting->value.target.boolean);
}

#ifdef HAVE_BLUETOOTH
static void bluetooth_enable_toggle_change_handler(
      rarch_setting_t *setting)
{
   systemd_service_toggle(LAKKA_BLUETOOTH_PATH,
         (char*)"bluetooth.service",
         *setting->value.target.boolean);
}
#endif

#ifdef HAVE_WIFI
static void localap_enable_toggle_change_handler(rarch_setting_t *setting)
{
   driver_wifi_tether_start_stop(*setting->value.target.boolean,
         LAKKA_LOCALAP_PATH);
}
#endif

static void timezone_change_handler(rarch_setting_t *setting)
{
   if (!setting)
      return;

   config_set_timezone(setting->value.target.string);

   RFILE *tzfp = filestream_open(LAKKA_TIMEZONE_PATH,
                       RETRO_VFS_FILE_ACCESS_WRITE,
                       RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (tzfp != NULL)
   {
      filestream_printf(tzfp, "TIMEZONE=%s", setting->value.target.string);
      filestream_close(tzfp);
   }
}
#endif

#ifdef _3DS
static void new3ds_speedup_change_handler(rarch_setting_t *setting)
{
   settings_t *settings             = config_get_ptr();

   if (!setting)
      return;

   osSetSpeedupEnable(*setting->value.target.boolean);
}
#endif

static bool setting_append_list_input_player_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group,
      unsigned user)
{
   /* This constants matches the string length.
    * Keep it up to date or you'll get some really obvious bugs.
    * 2 is the length of '99'; we don't need more users than that.
    */
   static char buffer[MAX_USERS][13+2+1];
   static char group_lbl[MAX_USERS][255];
   unsigned i, j;
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *system                = &runloop_state_get_ptr()->system;
   const struct retro_keybind* const defaults =
      (user == 0) ? retro_keybinds_1 : retro_keybinds_rest;
   const char *temp_value                     = msg_hash_to_str
      ((enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));

   group_info.name                            = NULL;
   subgroup_info.name                         = NULL;

   strlcat(buffer[user], "", sizeof(buffer[user]));

   strlcpy(group_lbl[user], temp_value, sizeof(group_lbl[user]));

   START_GROUP(list, list_info, &group_info, group_lbl[user], parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(
         list,
         list_info,
         buffer[user],
         &group_info,
         &subgroup_info,
         parent_group);

   {
      char tmp_string[PATH_MAX_LENGTH];
      /* These constants match the string lengths.
       * Keep them up to date or you'll get some really obvious bugs.
       * 2 is the length of '99'; we don't need more users than that.
       */
      /* FIXME/TODO - really need to clean up this mess in some way. */
      static char key[MAX_USERS][64];
      static char key_analog[MAX_USERS][64];
      static char key_bind_all[MAX_USERS][64];
      static char key_bind_all_save_autoconfig[MAX_USERS][64];
      static char split_joycon[MAX_USERS][64];
      static char split_joycon_lbl[MAX_USERS][64];
      static char key_bind_defaults[MAX_USERS][64];
      static char mouse_index[MAX_USERS][64];

      static char label[MAX_USERS][64];
      static char label_analog[MAX_USERS][64];
      static char label_bind_all[MAX_USERS][64];
      static char label_bind_all_save_autoconfig[MAX_USERS][64];
      static char label_bind_defaults[MAX_USERS][64];
      static char label_mouse_index[MAX_USERS][64];

      tmp_string[0] = '\0';

      snprintf(tmp_string, sizeof(tmp_string), "input_player%u", user + 1);

      fill_pathname_join_delim(key[user], tmp_string, "joypad_index", '_',
            sizeof(key[user]));
      snprintf(key_analog[user], sizeof(key_analog[user]),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE),
               user + 1);
      snprintf(split_joycon[user], sizeof(split_joycon[user]),
            "%s_%u",
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SPLIT_JOYCON),
               user + 1);
      fill_pathname_join_delim(key_bind_all[user], tmp_string, "bind_all", '_',
            sizeof(key_bind_all[user]));
      fill_pathname_join_delim(key_bind_all_save_autoconfig[user],
            tmp_string, "bind_all_save_autoconfig", '_',
            sizeof(key_bind_all_save_autoconfig[user]));
      fill_pathname_join_delim(key_bind_defaults[user],
            tmp_string, "bind_defaults", '_',
            sizeof(key_bind_defaults[user]));
      fill_pathname_join_delim(mouse_index[user], tmp_string, "mouse_index", '_',
            sizeof(mouse_index[user]));

      snprintf(split_joycon_lbl[user], sizeof(label[user]),
               "%s %u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON), user + 1);

      snprintf(label[user], sizeof(label[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX));
      snprintf(label_analog[user], sizeof(label_analog[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE));
      snprintf(label_bind_all[user], sizeof(label_bind_all[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL));
      snprintf(label_bind_defaults[user], sizeof(label_bind_defaults[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL));
      snprintf(label_bind_all_save_autoconfig[user], sizeof(label_bind_all_save_autoconfig[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG));
      snprintf(label_mouse_index[user], sizeof(label_mouse_index[user]),
               "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX));

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_analog_dpad_mode[user],
            key_analog[user],
            label_analog[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_analog_dpad_mode;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_select = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_analog_dpad_mode;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
      (*list)[list_info->index - 1].get_string_representation =
         &setting_get_string_representation_uint_analog_dpad_mode;
      menu_settings_list_current_add_range(list, list_info, 0, ANALOG_DPAD_LAST-1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE + user));

#ifdef HAVE_LIBNX
      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_split_joycon[user],
            split_joycon[user],
            split_joycon_lbl[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
#if 0
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_bind_device;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_bind_device;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].action_select = &setting_action_right_bind_device;
#endif
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_split_joycon;
      menu_settings_list_current_add_range(list, list_info, 0, 1, 1.0, true, true);
#endif

      CONFIG_ACTION_ALT(
            list, list_info,
            key[user],
            label[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_bind_device;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_bind_device;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].action_select = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_bind_device;
      (*list)[list_info->index - 1].get_string_representation =
         &get_string_representation_bind_device;
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_DEVICE_INDEX + user));

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_mouse_index[user],
            mouse_index[user],
            label_mouse_index[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_mouse_index;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_mouse_index;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_mouse_index;
      (*list)[list_info->index - 1].action_select = &setting_action_right_mouse_index;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
      (*list)[list_info->index - 1].get_string_representation =
         &get_string_representation_mouse_index;
      menu_settings_list_current_add_range(list, list_info, 0, MAX_USERS - 1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_MOUSE_INDEX + user));

      CONFIG_ACTION_ALT(
            list, list_info,
            key_bind_all[user],
            label_bind_all[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_all;
      (*list)[list_info->index - 1].action_cancel  = NULL;

      CONFIG_ACTION_ALT(
            list, list_info,
            key_bind_defaults[user],
            label_bind_defaults[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_defaults;
      (*list)[list_info->index - 1].action_cancel  = NULL;

#ifdef HAVE_CONFIGFILE
      CONFIG_ACTION_ALT(
            list, list_info,
            key_bind_all_save_autoconfig[user],
            label_bind_all_save_autoconfig[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_all_save_autoconfig;
      (*list)[list_info->index - 1].action_cancel  = NULL;
#endif
   }

   for (j = 0; j < RARCH_BIND_LIST_END; j++)
   {
      char label[255];
      char name[255];

      i = (j < RARCH_ANALOG_BIND_LIST_END) ? input_config_bind_order[j] : j;

      if (input_config_bind_map_get_meta(i))
         continue;

      label[0] = name[0]          = '\0';

      if (!string_is_empty(buffer[user]))
         fill_pathname_noext(label, buffer[user],
               " ",
               sizeof(label));

      if (
            settings->bools.input_descriptor_label_show
            && (i < RARCH_FIRST_META_KEY)
            && core_has_set_input_descriptor()
            && (i != RARCH_TURBO_ENABLE)
         )
      {
         if (system->input_desc_btn[user][i])
            strlcat(label,
                  system->input_desc_btn[user][i],
                  sizeof(label));
         else
         {
            strlcat(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
                  sizeof(label));

            if (settings->bools.input_descriptor_hide_unbound)
               continue;
         }
      }
      else
         strlcat(label, input_config_bind_map_get_desc(i), sizeof(label));

      snprintf(name, sizeof(name), "p%u_%s", user + 1, input_config_bind_map_get_base(i));

      CONFIG_BIND_ALT(
            list, list_info,
            &input_config_binds[user][i],
            user + 1,
            user,
            strdup(name),
            strdup(label),
            &defaults[i],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].bind_type = i + MENU_SETTINGS_BIND_BEGIN;
   }

   END_SUB_GROUP(list, list_info, parent_group);
   END_GROUP(list, list_info, parent_group);

   return true;
}

static bool setting_append_list_input_libretro_device_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   settings_t *settings = config_get_ptr();
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   static char key_device_type[MAX_USERS][64];
   static char label_device_type[MAX_USERS][64];
   unsigned user;

   group_info.name    = NULL;
   subgroup_info.name = NULL;

   START_GROUP(list, list_info, &group_info,
         "Libretro Device Type", parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(list, list_info, "State", &group_info,
         &subgroup_info, parent_group);

   for (user = 0; user < MAX_USERS; user++)
   {
      key_device_type[user][0]   = '\0';
      label_device_type[user][0] = '\0';

      snprintf(key_device_type[user], sizeof(key_device_type[user]),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE),
            user + 1);

      snprintf(label_device_type[user], sizeof(label_device_type[user]),
            "%s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE));

      CONFIG_UINT_ALT(
            list, list_info,
            input_config_get_device_ptr(user),
            key_device_type[user],
            label_device_type[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_libretro_device_type;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_select = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_libretro_device_type;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_libretro_device_type;
      (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_libretro_device;
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE + user));
   }

   END_SUB_GROUP(list, list_info, parent_group);
   END_GROUP(list, list_info, parent_group);

   return true;
}

static bool setting_append_list_input_remap_port_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   settings_t *settings = config_get_ptr();
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   static char key_port[MAX_USERS][64];
   static char label_port[MAX_USERS][64];
   unsigned user;

   group_info.name    = NULL;
   subgroup_info.name = NULL;

   START_GROUP(list, list_info, &group_info,
         "Mapped Ports", parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(list, list_info, "State", &group_info,
         &subgroup_info, parent_group);

   for (user = 0; user < MAX_USERS; user++)
   {
      key_port[user][0]   = '\0';
      label_port[user][0] = '\0';

      snprintf(key_port[user], sizeof(key_port[user]),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_REMAP_PORT),
               user + 1);

      snprintf(label_port[user], sizeof(label_port[user]), "%s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT));

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_remap_ports[user],
            key_port[user],
            label_port[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_input_remap_port;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_input_remap_port;
      (*list)[list_info->index - 1].action_select = &setting_action_right_input_remap_port;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_input_remap_port;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
      (*list)[list_info->index - 1].get_string_representation =
         &setting_get_string_representation_uint_input_remap_port;
      menu_settings_list_current_add_range(list, list_info, 0, MAX_USERS-1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_REMAP_PORT + user));
   }

   END_SUB_GROUP(list, list_info, parent_group);
   END_GROUP(list, list_info, parent_group);

   return true;
}

/**
 * config_get_audio_resampler_driver_options:
 *
 * Get an enumerated list of all resampler driver names, separated by '|'.
 *
 * Returns: string listing of all resampler driver names, separated by '|'.
 **/
static const char* config_get_audio_resampler_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_RESAMPLER_DRIVERS, NULL);
}

static int directory_action_start_generic(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   setting_set_with_string_representation(setting,
         setting->default_value.string);

   return 0;
}

static bool setting_append_list(
      settings_t *settings,
      global_t *global,
      enum settings_list_type type,
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   unsigned user;
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   recording_state_t *recording_st           = recording_state_get_ptr();

   group_info.name                           = NULL;
   subgroup_info.name                        = NULL;


   switch (type)
   {
      case SETTINGS_LIST_MAIN_MENU:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU), parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_MAIN_MENU);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_INT(
               list, list_info,
               &settings->ints.state_slot,
               MENU_ENUM_LABEL_STATE_SLOT,
               MENU_ENUM_LABEL_VALUE_STATE_SLOT,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = -1;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_state_slot;
         menu_settings_list_current_add_range(list, list_info, -1, 999, 1, true, true);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_START_CORE,
               MENU_ENUM_LABEL_VALUE_START_CORE,
               &group_info,
               &subgroup_info,
               parent_group);

#if defined(HAVE_VIDEOPROCESSOR)
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_START_VIDEO_PROCESSOR,
               MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_START_NET_RETROPAD,
               MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONTENT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE,
               MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
               &group_info,
               &subgroup_info,
               parent_group);

#ifndef HAVE_DYNAMIC
         if (frontend_driver_has_fork())
#endif
         {
            char ext_name[255];

            ext_name[0] = '\0';

            if (frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
            {
               CONFIG_ACTION(
                     list, list_info,
                     MENU_ENUM_LABEL_CORE_LIST,
                     MENU_ENUM_LABEL_VALUE_CORE_LIST,
                     &group_info,
                     &subgroup_info,
                     parent_group);
               (*list)[list_info->index - 1].size                = (uint32_t)path_get_realsize(RARCH_PATH_CORE);
               (*list)[list_info->index - 1].value.target.string = path_get_ptr(RARCH_PATH_CORE);
               (*list)[list_info->index - 1].values       = ext_name;
               MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_LOAD_CORE);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_BROWSER_ACTION);
            }
         }

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         if (settings->bools.history_list_enable)
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                  MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
                  &group_info,
                  &subgroup_info,
                  parent_group);
         }

#ifdef HAVE_CDROM
         /* TODO/FIXME - add check seeing if CDROM is inserted into tray
          */
         {
            struct string_list *drive_list = cdrom_get_available_drives();

            if (drive_list)
            {
               if (drive_list->size)
               {
                  CONFIG_ACTION(
                        list, list_info,
                        MENU_ENUM_LABEL_LOAD_DISC,
                        MENU_ENUM_LABEL_VALUE_LOAD_DISC,
                        &group_info,
                        &subgroup_info,
                        parent_group);

                  CONFIG_ACTION(
                        list, list_info,
                        MENU_ENUM_LABEL_DUMP_DISC,
                        MENU_ENUM_LABEL_VALUE_DUMP_DISC,
                        &group_info,
                        &subgroup_info,
                        parent_group);

#ifdef HAVE_LAKKA
                  CONFIG_ACTION(
                        list, list_info,
                        MENU_ENUM_LABEL_EJECT_DISC,
                        MENU_ENUM_LABEL_VALUE_EJECT_DISC,
                        &group_info,
                        &subgroup_info,
                        parent_group);
#endif
               }

               string_list_free(drive_list);
            }
         }
#endif

         if (string_is_not_equal(settings->arrays.menu_driver, "xmb") && string_is_not_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                  MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
                  &group_info,
                  &subgroup_info,
                  parent_group);
         }

#if defined(HAVE_NETWORKING)
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_NETPLAY,
               MENU_ENUM_LABEL_VALUE_NETPLAY,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONLINE_UPDATER,
               MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

#ifdef HAVE_MIST
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,
               MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_INFORMATION_LIST,
               MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
               &group_info,
               &subgroup_info,
               parent_group);

#if !defined(IOS) && !defined(HAVE_LAKKA)
         if (frontend_driver_has_fork())
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_RESTART_RETROARCH,
                  MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
                  &group_info,
                  &subgroup_info,
                  parent_group);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_RESTART_RETROARCH);
         }
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
               MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATIONS,
               MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG,
               MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_NEW_CONFIG,
               MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_SAVE_CONFIG);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_HELP_LIST,
               MENU_ENUM_LABEL_VALUE_HELP_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#ifdef HAVE_QT
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SHOW_WIMP,
               MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_UI_COMPANION_TOGGLE);
#endif
#if !defined(IOS)
         /* Apple rejects iOS apps that let you forcibly quit them. */
#ifdef HAVE_LAKKA
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_QUIT_RETROARCH,
               MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
               &group_info,
               &subgroup_info,
               parent_group);
#else
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_QUIT_RETROARCH,
               MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
               &group_info,
               &subgroup_info,
               parent_group);
#endif
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_QUIT);
#endif

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
        CONFIG_ACTION(
              list, list_info,
              MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
              MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
              &group_info,
              &subgroup_info,
              parent_group);
#endif

#if defined(HAVE_LAKKA)
#ifdef HAVE_LAKKA_SWITCH
        CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
               MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
               &group_info,
               &subgroup_info,
               parent_group);
#endif
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_REBOOT,
               MENU_ENUM_LABEL_VALUE_REBOOT,
               &group_info,
               &subgroup_info,
               parent_group);

         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REBOOT);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SHUTDOWN,
               MENU_ENUM_LABEL_VALUE_SHUTDOWN,
               &group_info,
               &subgroup_info,
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_SHUTDOWN);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_DRIVER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_VIDEO_SETTINGS,
               MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_VIDEO_OUTPUT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

#ifdef HAVE_AUDIOMIXER
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

#ifdef HAVE_AUDIOMIXER
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_SOUNDS,
               MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_INPUT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LATENCY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CORE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATION_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVING_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LOGGING_SETTINGS,
               MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_REWIND_SETTINGS,
               MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CHEAT_DETAILS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         if (string_is_not_equal(settings->arrays.record_driver, "null"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_RECORDING_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_OVERLAY
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

#ifdef HAVE_VIDEO_LAYOUT
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_SETTINGS,
               MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AI_SERVICE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

#ifdef HAVE_ACCESSIBILITY
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_CHEEVOS
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_UPDATER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
#ifdef HAVE_LAKKA
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
#endif

#ifdef HAVE_BLUETOOTH
         if (string_is_not_equal(
                  settings->arrays.bluetooth_driver, "null"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_BLUETOOTH_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);
         }
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_WIFI)
         if (string_is_not_equal(settings->arrays.wifi_driver, "null"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_WIFI_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);
         }
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_NETWORK_SETTINGS,
               MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_LAKKA
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LAKKA_SERVICES,
               MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_PLAYLIST_SETTINGS,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_USER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_DIRECTORY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_PRIVACY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_RESAMPLER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         if (string_is_not_equal(settings->arrays.midi_driver, "null"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_MIDI_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         for (user = 0; user < MAX_USERS; user++)
            setting_append_list_input_player_options(list, list_info, parent_group, user);

         setting_append_list_input_libretro_device_options(list, list_info, parent_group);
         setting_append_list_input_remap_port_options(list, list_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DRIVERS:
         {
            unsigned i, j = 0;
            struct string_options_entry string_options_entries[12] = {0};

            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS), parent_group);
            MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_DRIVER_SETTINGS);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info,
                  &subgroup_info, parent_group);

            string_options_entries[j].target         = settings->arrays.input_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.input_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_INPUT_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_DRIVER;
            string_options_entries[j].default_value  = config_get_default_input();
            string_options_entries[j].values         = config_get_input_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.input_joypad_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.input_joypad_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_JOYPAD_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER;
            string_options_entries[j].default_value  = config_get_default_joypad();
            string_options_entries[j].values         = config_get_joypad_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.video_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.video_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER;
            string_options_entries[j].default_value  = config_get_default_video();
            string_options_entries[j].values         = config_get_video_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.audio_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.audio_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_AUDIO_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER;
            string_options_entries[j].default_value  = config_get_default_audio();
            string_options_entries[j].values         = config_get_audio_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.audio_resampler;
            string_options_entries[j].len            = sizeof(settings->arrays.audio_resampler);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER;
            string_options_entries[j].default_value  = config_get_default_audio_resampler();
            string_options_entries[j].values         = config_get_audio_resampler_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.camera_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.camera_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_CAMERA_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER;
            string_options_entries[j].default_value  = config_get_default_camera();
            string_options_entries[j].values         = config_get_camera_driver_options();

            j++;

#ifdef HAVE_BLUETOOTH
            string_options_entries[j].target         = settings->arrays.bluetooth_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.bluetooth_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_BLUETOOTH_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_BLUETOOTH_DRIVER;
            string_options_entries[j].default_value  = config_get_default_bluetooth();
            string_options_entries[j].values         = config_get_bluetooth_driver_options();

            j++;
#endif

#ifdef HAVE_WIFI
            string_options_entries[j].target         = settings->arrays.wifi_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.wifi_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_WIFI_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_WIFI_DRIVER;
            string_options_entries[j].default_value  = config_get_default_wifi();
            string_options_entries[j].values         = config_get_wifi_driver_options();

            j++;
#endif

            string_options_entries[j].target         = settings->arrays.location_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.location_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_LOCATION_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER;
            string_options_entries[j].default_value  = config_get_default_location();
            string_options_entries[j].values         = config_get_location_driver_options();

            j++;

            string_options_entries[j].target         = settings->arrays.menu_driver;
            string_options_entries[j].len            = sizeof(settings->arrays.menu_driver);
            string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_MENU_DRIVER;
            string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_MENU_DRIVER;
            string_options_entries[j].default_value  = config_get_default_menu();
            string_options_entries[j].values         = config_get_menu_driver_options();

            j++;

            string_options_entries[j].target          = settings->arrays.record_driver;
            string_options_entries[j].len             = sizeof(settings->arrays.record_driver);
            string_options_entries[j].name_enum_idx   = MENU_ENUM_LABEL_RECORD_DRIVER;
            string_options_entries[j].SHORT_enum_idx  = MENU_ENUM_LABEL_VALUE_RECORD_DRIVER;
            string_options_entries[j].default_value   = config_get_default_record();
            string_options_entries[j].values          = config_get_record_driver_options();

            j++;

            string_options_entries[j].target          = settings->arrays.midi_driver;
            string_options_entries[j].len             = sizeof(settings->arrays.midi_driver);
            string_options_entries[j].name_enum_idx   = MENU_ENUM_LABEL_MIDI_DRIVER;
            string_options_entries[j].SHORT_enum_idx  = MENU_ENUM_LABEL_VALUE_MIDI_DRIVER;
            string_options_entries[j].default_value   = config_get_default_midi();
            string_options_entries[j].values          = config_get_midi_driver_options();

            j++;

            for (i = 0; i < j; i++)
            {
               CONFIG_STRING_OPTIONS(
                     list, list_info,
                     string_options_entries[i].target,
                     string_options_entries[i].len,
                     string_options_entries[i].name_enum_idx,
                     string_options_entries[i].SHORT_enum_idx,
                     string_options_entries[i].default_value,
                     string_options_entries[i].values,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_read_handler,
                     general_write_handler);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
               (*list)[list_info->index - 1].action_ok    = setting_action_ok_uint;
               (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
               (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
            }

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CORE:
         {
            unsigned i, listing = 0;
#ifndef HAVE_DYNAMIC
            struct bool_entry bool_entries[9];
#else
            struct bool_entry bool_entries[8];
#endif
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS), parent_group);
            MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_CORE_SETTINGS);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            bool_entries[listing].target         = &settings->bools.video_shared_context;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT;
            bool_entries[listing].default_value  = DEFAULT_VIDEO_SHARED_CONTEXT;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.driver_switch_enable;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_DRIVER_SWITCH_ENABLE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE;
            bool_entries[listing].default_value  = DEFAULT_DRIVER_SWITCH_ENABLE;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.load_dummy_on_core_shutdown;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN;
            bool_entries[listing].default_value  = DEFAULT_LOAD_DUMMY_ON_CORE_SHUTDOWN;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.set_supports_no_game_enable;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
            bool_entries[listing].default_value  = true;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.check_firmware_before_loading;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE;
            bool_entries[listing].default_value  = DEFAULT_CHECK_FIRMWARE_BEFORE_LOADING;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.video_allow_rotate;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE;
            bool_entries[listing].default_value  = DEFAULT_ALLOW_ROTATE;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

            bool_entries[listing].target         = &settings->bools.core_option_category_enable;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_OPTION_CATEGORY_ENABLE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE;
            bool_entries[listing].default_value  = DEFAULT_CORE_OPTION_CATEGORY_ENABLE;
            bool_entries[listing].flags          = SD_FLAG_NONE;
            listing++;

            bool_entries[listing].target         = &settings->bools.core_info_cache_enable;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE;
            bool_entries[listing].default_value  = DEFAULT_CORE_INFO_CACHE_ENABLE;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;

#ifndef HAVE_DYNAMIC
            bool_entries[listing].target         = &settings->bools.always_reload_core_on_run_content;
            bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT;
            bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT;
            bool_entries[listing].default_value  = DEFAULT_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT;
            bool_entries[listing].flags          = SD_FLAG_ADVANCED;
            listing++;
#endif
            for (i = 0; i < ARRAY_SIZE(bool_entries); i++)
            {
#if !defined(HAVE_CORE_INFO_CACHE)
               if (bool_entries[i].name_enum_idx ==
                     MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE)
                  continue;
#endif
               CONFIG_BOOL(
                     list, list_info,
                     bool_entries[i].target,
                     bool_entries[i].name_enum_idx,
                     bool_entries[i].SHORT_enum_idx,
                     bool_entries[i].default_value,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     bool_entries[i].flags);
            }

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CONFIGURATION:
         {
            uint8_t i;
            struct bool_entry bool_entries[7];
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS), parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            bool_entries[0].target         = &settings->bools.config_save_on_exit;
            bool_entries[0].name_enum_idx  = MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT;
            bool_entries[0].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT;
            bool_entries[0].default_value  = DEFAULT_CONFIG_SAVE_ON_EXIT;
            bool_entries[0].flags          = SD_FLAG_NONE;

            bool_entries[1].target         = &settings->bools.show_hidden_files;
            bool_entries[1].name_enum_idx  = MENU_ENUM_LABEL_SHOW_HIDDEN_FILES;
            bool_entries[1].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES;
            bool_entries[1].default_value  = DEFAULT_SHOW_HIDDEN_FILES;
            bool_entries[1].flags          = SD_FLAG_NONE;

            bool_entries[2].target         = &settings->bools.game_specific_options;
            bool_entries[2].name_enum_idx  = MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS;
            bool_entries[2].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS;
            bool_entries[2].default_value  = default_game_specific_options;
            bool_entries[2].flags          = SD_FLAG_ADVANCED;

            bool_entries[3].target         = &settings->bools.auto_overrides_enable;
            bool_entries[3].name_enum_idx  = MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE;
            bool_entries[3].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE;
            bool_entries[3].default_value  = default_auto_overrides_enable;
            bool_entries[3].flags          = SD_FLAG_ADVANCED;

            bool_entries[4].target         = &settings->bools.auto_remaps_enable;
            bool_entries[4].name_enum_idx  = MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE;
            bool_entries[4].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE;
            bool_entries[4].default_value  = default_auto_remaps_enable;
            bool_entries[4].flags          = SD_FLAG_ADVANCED;

            bool_entries[5].target         = &settings->bools.auto_shaders_enable;
            bool_entries[5].name_enum_idx  = MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE;
            bool_entries[5].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE;
            bool_entries[5].default_value  = default_auto_shaders_enable;
            bool_entries[5].flags          = SD_FLAG_NONE;

            bool_entries[6].target         = &settings->bools.global_core_options;
            bool_entries[6].name_enum_idx  = MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS;
            bool_entries[6].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS;
            bool_entries[6].default_value  = default_global_core_options;
            bool_entries[6].flags          = SD_FLAG_NONE;

            for (i = 0; i < ARRAY_SIZE(bool_entries); i++)
            {
               CONFIG_BOOL(
                     list, list_info,
                     bool_entries[i].target,
                     bool_entries[i].name_enum_idx,
                     bool_entries[i].SHORT_enum_idx,
                     bool_entries[i].default_value,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     bool_entries[i].flags);
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_shader_enable,
                  MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
                  DEFAULT_SHADER_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_shader_preset_save_reference_enable,
                  MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
                  DEFAULT_VIDEO_SHADER_PRESET_SAVE_REFERENCE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_LOGGING:
         {
            bool *tmp_b = NULL;
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS), parent_group);
            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS);

            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  verbosity_get_ptr(),
                  MENU_ENUM_LABEL_LOG_VERBOSITY,
                  MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  write_handler_logging_verbosity,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.frontend_log_level,
                  MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL,
                  MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
                  DEFAULT_FRONTEND_LOG_LEVEL,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].change_handler = frontend_log_level_change_handler;
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_libretro_log_level;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.libretro_log_level,
                  MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL,
                  MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
                  DEFAULT_LIBRETRO_LOG_LEVEL,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_libretro_log_level;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.log_to_file,
                  MENU_ENUM_LABEL_LOG_TO_FILE,
                  MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
                  DEFAULT_LOG_TO_FILE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.log_to_file_timestamp,
                  MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP,
                  MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
                  DEFAULT_LOG_TO_FILE_TIMESTAMP,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Performance Counters", &group_info, &subgroup_info,
                  parent_group);

            retroarch_ctl(RARCH_CTL_GET_PERFCNT, &tmp_b);

            CONFIG_BOOL(
                  list, list_info,
                  tmp_b,
                  MENU_ENUM_LABEL_PERFCNT_ENABLE,
                  MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
         }
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_SAVING:
         {
            uint8_t i;
            struct bool_entry bool_entries[13];

            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS), parent_group);
            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SAVING_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            bool_entries[0].target         = &settings->bools.sort_savefiles_enable;
            bool_entries[0].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE;
            bool_entries[0].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE;
            bool_entries[0].default_value  = default_sort_savefiles_enable;
            bool_entries[0].flags          = SD_FLAG_ADVANCED;

            bool_entries[1].target         = &settings->bools.sort_savestates_enable;
            bool_entries[1].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE;
            bool_entries[1].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE;
            bool_entries[1].default_value  = default_sort_savestates_enable;
            bool_entries[1].flags          = SD_FLAG_ADVANCED;

            bool_entries[2].target         = &settings->bools.sort_savefiles_by_content_enable;
            bool_entries[2].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE;
            bool_entries[2].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE;
            bool_entries[2].default_value  = default_sort_savefiles_by_content_enable;
            bool_entries[2].flags          = SD_FLAG_ADVANCED;

            bool_entries[3].target         = &settings->bools.sort_savestates_by_content_enable;
            bool_entries[3].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE;
            bool_entries[3].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE;
            bool_entries[3].default_value  = default_sort_savestates_by_content_enable;
            bool_entries[3].flags          = SD_FLAG_ADVANCED;

            bool_entries[4].target         = &settings->bools.sort_screenshots_by_content_enable;
            bool_entries[4].name_enum_idx  = MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE;
            bool_entries[4].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE;
            bool_entries[4].default_value  = default_sort_screenshots_by_content_enable;
            bool_entries[4].flags          = SD_FLAG_ADVANCED;

            bool_entries[5].target         = &settings->bools.block_sram_overwrite;
            bool_entries[5].name_enum_idx  = MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE;
            bool_entries[5].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE;
            bool_entries[5].default_value  = DEFAULT_BLOCK_SRAM_OVERWRITE;
            bool_entries[5].flags          = SD_FLAG_NONE;

            bool_entries[6].target         = &settings->bools.savestate_auto_save;
            bool_entries[6].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE;
            bool_entries[6].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE;
            bool_entries[6].default_value  = savestate_auto_save;
            bool_entries[6].flags          = SD_FLAG_NONE;

            bool_entries[7].target         = &settings->bools.savestate_auto_load;
            bool_entries[7].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD;
            bool_entries[7].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD;
            bool_entries[7].default_value  = savestate_auto_load;
            bool_entries[7].flags          = SD_FLAG_NONE;

            bool_entries[8].target         = &settings->bools.savestate_thumbnail_enable;
            bool_entries[8].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE;
            bool_entries[8].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE;
            bool_entries[8].default_value  = savestate_thumbnail_enable;
            bool_entries[8].flags          = SD_FLAG_ADVANCED;

            bool_entries[9].target         = &settings->bools.savefiles_in_content_dir;
            bool_entries[9].name_enum_idx  = MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[9].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[9].default_value  = default_savefiles_in_content_dir;
            bool_entries[9].flags          = SD_FLAG_ADVANCED;

            bool_entries[10].target         = &settings->bools.savestates_in_content_dir;
            bool_entries[10].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE;
            bool_entries[10].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE;
            bool_entries[10].default_value  = default_savestates_in_content_dir;
            bool_entries[10].flags          = SD_FLAG_ADVANCED;

            bool_entries[11].target         = &settings->bools.systemfiles_in_content_dir;
            bool_entries[11].name_enum_idx  = MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[11].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[11].default_value  = default_systemfiles_in_content_dir;
            bool_entries[11].flags          = SD_FLAG_ADVANCED;

            bool_entries[12].target         = &settings->bools.screenshots_in_content_dir;
            bool_entries[12].name_enum_idx  = MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
            bool_entries[12].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
            bool_entries[12].default_value  = default_screenshots_in_content_dir;
            bool_entries[12].flags          = SD_FLAG_ADVANCED;

            for (i = 0; i < ARRAY_SIZE(bool_entries); i++)
            {
               CONFIG_BOOL(
                     list, list_info,
                     bool_entries[i].target,
                     bool_entries[i].name_enum_idx,
                     bool_entries[i].SHORT_enum_idx,
                     bool_entries[i].default_value,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     bool_entries[i].flags);
            }

#ifdef HAVE_THREADS
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.autosave_interval,
                  MENU_ENUM_LABEL_AUTOSAVE_INTERVAL,
                  MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
                  DEFAULT_AUTOSAVE_INTERVAL,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_AUTOSAVE_INIT);
            menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_autosave_interval;
#endif
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.savestate_auto_index,
                  MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX,
                  MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
                  savestate_auto_index,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.savestate_max_keep,
                  MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP,
                  MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
                  DEFAULT_SAVESTATE_MAX_KEEP,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 999, 1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.content_runtime_log,
                  MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG,
                  MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
                  DEFAULT_CONTENT_RUNTIME_LOG,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.content_runtime_log_aggregate,
                  MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
                  MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
                  DEFAULT_CONTENT_RUNTIME_LOG_AGGREGATE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#if defined(HAVE_ZLIB)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.save_file_compression,
                  MENU_ENUM_LABEL_SAVE_FILE_COMPRESSION,
                  MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
                  DEFAULT_SAVE_FILE_COMPRESSION,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.savestate_file_compression,
                  MENU_ENUM_LABEL_SAVESTATE_FILE_COMPRESSION,
                  MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
                  DEFAULT_SAVESTATE_FILE_COMPRESSION,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

            /* TODO/FIXME: This is in the wrong group... */
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.scan_without_core_match,
                  MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH,
                  MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
                  DEFAULT_SCAN_WITHOUT_CORE_MATCH,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }

         break;
      case SETTINGS_LIST_FRAME_TIME_COUNTER:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.frame_time_counter_reset_after_fastforwarding,
               MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
               MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.frame_time_counter_reset_after_load_state,
               MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
               MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.frame_time_counter_reset_after_save_state,
               MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
               MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_REWIND:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.rewind_enable,
               MENU_ENUM_LABEL_REWIND_ENABLE,
               MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
               DEFAULT_REWIND_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REWIND_TOGGLE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.rewind_granularity,
                  MENU_ENUM_LABEL_REWIND_GRANULARITY,
                  MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
                  DEFAULT_REWIND_GRANULARITY,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by     = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 32768, 1, true, true);

            CONFIG_SIZE(
                  list, list_info,
                  &settings->sizes.rewind_buffer_size,
                  MENU_ENUM_LABEL_REWIND_BUFFER_SIZE,
                  MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
                  DEFAULT_REWIND_BUFFER_SIZE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
				  &setting_get_string_representation_size_in_mb);
            menu_settings_list_current_add_range(list, list_info, 1024*1024, 1024*1024*1024, settings->uints.rewind_buffer_size_step*1024*1024, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.rewind_buffer_size_step,
                  MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP,
                  MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
                  DEFAULT_REWIND_BUFFER_SIZE_STEP,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by     = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 100, 1, true, true);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_CHEATS:
         {
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS), parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.apply_cheats_after_load,
                  MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD,
                  MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
                  DEFAULT_APPLY_CHEATS_AFTER_LOAD,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.apply_cheats_after_toggle,
                  MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE,
                  MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
                  DEFAULT_APPLY_CHEATS_AFTER_TOGGLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
            break;
         }
      case SETTINGS_LIST_CHEAT_DETAILS:
#ifdef HAVE_CHEATS
         {
            int max_bit_position;
            if (!cheat_manager_state.cheats)
               break;

            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS), parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DETAILS_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.idx, CHEAT_IDX,
                  NULL,NULL,
                  0,&setting_get_string_representation_uint,0,cheat_manager_get_size()-1,1);

            CONFIG_BOOL(
                  list, list_info,
                  &cheat_manager_state.working_cheat.state,
                  MENU_ENUM_LABEL_CHEAT_STATE,
                  MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
                  cheat_manager_state.working_cheat.state,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_STRING(
                  list, list_info,
                  cheat_manager_state.working_desc,
                  sizeof(cheat_manager_state.working_desc),
                  MENU_ENUM_LABEL_CHEAT_DESC,
                  MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_STRING(
                  list, list_info,
                  cheat_manager_state.working_code,
                  sizeof(cheat_manager_state.working_code),
                  MENU_ENUM_LABEL_CHEAT_CODE,
                  MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.handler, CHEAT_HANDLER,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
                  &setting_get_string_representation_uint_as_enum,
                  CHEAT_HANDLER_TYPE_EMU,CHEAT_HANDLER_TYPE_RETRO,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_STRING(
                  list, list_info,
                  cheat_manager_state.working_code,
                  sizeof(cheat_manager_state.working_code),
                  MENU_ENUM_LABEL_CHEAT_CODE,
                  MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.memory_search_size, CHEAT_MEMORY_SEARCH_SIZE,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
                  &setting_get_string_representation_uint_as_enum,
                  0,5,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.cheat_type, CHEAT_TYPE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
                  &setting_get_string_representation_uint_as_enum,
                  CHEAT_TYPE_DISABLED,CHEAT_TYPE_RUN_NEXT_IF_GT,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.value, CHEAT_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,
                  0,cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size),1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.address, CHEAT_ADDRESS,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  0,&setting_get_string_representation_hex_and_uint,
                  0,cheat_manager_state.total_memory_size==0?0:cheat_manager_state.total_memory_size-1,1);

            max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0;
            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.address_mask, CHEAT_ADDRESS_BIT_POSITION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,max_bit_position,1);

            CONFIG_BOOL(
                  list, list_info,
                  &cheat_manager_state.working_cheat.big_endian,
                  MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN,
                  MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
                  cheat_manager_state.working_cheat.big_endian,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_count, CHEAT_REPEAT_COUNT,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,1,2048,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_add_to_address, CHEAT_REPEAT_ADD_TO_ADDRESS,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,1,2048,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_add_to_value, CHEAT_REPEAT_ADD_TO_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,0xFFFF,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_type, CHEAT_RUMBLE_TYPE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
                  &setting_get_string_representation_uint_as_enum,
                  RUMBLE_TYPE_DISABLED,RUMBLE_TYPE_END_LIST-1,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_value, CHEAT_RUMBLE_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,
                  0,cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size),1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_port, CHEAT_RUMBLE_PORT,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_RUMBLE_PORT_0,
                  &setting_get_string_representation_uint_as_enum,
                  0,16,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_primary_strength, CHEAT_RUMBLE_PRIMARY_STRENGTH,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,65535,1);

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_primary_duration, CHEAT_RUMBLE_PRIMARY_DURATION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_uint,0,5000,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_secondary_strength, CHEAT_RUMBLE_SECONDARY_STRENGTH,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,65535,1);

            CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_secondary_duration, CHEAT_RUMBLE_SECONDARY_DURATION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_uint,0,5000,1);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
#endif
         break;
      case SETTINGS_LIST_CHEAT_SEARCH:
#ifdef HAVE_CHEATS
         if (!cheat_manager_state.cheats)
            break;

         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_UINT_CBS(cheat_manager_state.search_bit_size, CHEAT_START_OR_RESTART,
               setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
               MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
               &setting_get_string_representation_uint_as_enum,
               0,5,1);
         (*list)[list_info->index - 1].action_ok = &cheat_manager_initialize_memory;

         CONFIG_BOOL(
               list, list_info,
               &cheat_manager_state.big_endian,
               MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN,
               MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
               cheat_manager_state.big_endian,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.search_exact_value,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
               cheat_manager_state.search_exact_value,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0, cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size), 1, true, true);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_exact;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_exact;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_LT,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_lt;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_lt;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_LTE,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_lte;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_lte;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_GT,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_gt;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_gt;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_GTE,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_gte;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_gte;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQ,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_eq;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_eq;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.dummy,
               MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
               cheat_manager_state.dummy,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_neq;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_neq;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.search_eqplus_value,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
               cheat_manager_state.search_eqplus_value,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0, cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size), 1, true, true);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_eqplus;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_eqplus;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.search_eqminus_value,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS,
               MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
               cheat_manager_state.search_eqminus_value,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0, cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size), 1, true, true);
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_eqminus;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_search_eqminus;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.match_idx,
               MENU_ENUM_LABEL_CHEAT_DELETE_MATCH,
               MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
               cheat_manager_state.match_idx,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, cheat_manager_state.num_matches-1, 1, true, true);
         (*list)[list_info->index - 1].action_left = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_delete_match;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.match_idx,
               MENU_ENUM_LABEL_CHEAT_COPY_MATCH,
               MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
               cheat_manager_state.match_idx,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, cheat_manager_state.num_matches-1, 1, true, true);
         (*list)[list_info->index - 1].action_left = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].action_ok = &cheat_manager_copy_match;

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.browse_address,
               MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY,
               MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
               cheat_manager_state.browse_address,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, cheat_manager_state.total_memory_size>0?cheat_manager_state.total_memory_size-1:0, 1, true, true);
         (*list)[list_info->index - 1].action_left = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_browse_address;

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_VIDEO:
         {
            struct video_viewport *custom_vp   = video_viewport_get_custom();
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS), parent_group);
            MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_VIDEO_SETTINGS);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ui_suspend_screensaver_enable,
                  MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE,
                  MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Platform-specific", &group_info, &subgroup_info, parent_group);

            video_driver_menu_settings((void**)list, (void*)list_info, (void*)&group_info, (void*)&subgroup_info, parent_group);

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Monitor", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_monitor_index,
                  MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX,
                  MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
                  DEFAULT_MONITOR_INDEX,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            menu_settings_list_current_add_range(list, list_info, 0, 1, 1, true, false);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_video_monitor_index;

            /* prevent unused function warning on unsupported builds */
            (void)setting_get_string_representation_int_gpu_index;

#ifdef ANDROID
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_notch_write_over_enable,
                  MENU_ENUM_LABEL_VIDEO_NOTCH_WRITE_OVER,
                  MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
                  DEFAULT_NOTCH_WRITE_OVER_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

#ifdef HAVE_VULKAN
            if (string_is_equal(video_driver_get_ident(), "vulkan"))
            {
               CONFIG_INT(
                     list, list_info,
                     &settings->ints.vulkan_gpu_index,
                     MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_int_gpu_index;
            }
#endif

#ifdef HAVE_D3D10
            if (string_is_equal(video_driver_get_ident(), "d3d10"))
            {
               CONFIG_INT(
                     list, list_info,
                     &settings->ints.d3d10_gpu_index,
                     MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_int_gpu_index;
            }
#endif

#ifdef HAVE_D3D11
            if (string_is_equal(video_driver_get_ident(), "d3d11"))
            {
               CONFIG_INT(
                     list, list_info,
                     &settings->ints.d3d11_gpu_index,
                     MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_int_gpu_index;
            }
#endif

#ifdef HAVE_D3D12
            if (string_is_equal(video_driver_get_ident(), "d3d12"))
            {
               CONFIG_INT(
                     list, list_info,
                     &settings->ints.d3d12_gpu_index,
                     MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_int_gpu_index;
            }
#endif

#ifdef WIIU
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_wiiu_prefer_drc,
                  MENU_ENUM_LABEL_VIDEO_WIIU_PREFER_DRC,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
                  DEFAULT_WIIU_PREFER_DRC,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

            if (video_driver_has_windowed())
            {
               CONFIG_ACTION(
                     list, list_info,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
                     &group_info,
                     &subgroup_info,
                     parent_group);

               CONFIG_ACTION(
                     list, list_info,
                     MENU_ENUM_LABEL_VIDEO_WINDOWED_MODE_SETTINGS,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
                     &group_info,
                     &subgroup_info,
                     parent_group);
            }


            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_fullscreen,
                  MENU_ENUM_LABEL_VIDEO_FULLSCREEN,
                  MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
                  DEFAULT_FULLSCREEN,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT_FROM_TOGGLE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_windowed_fullscreen,
                     MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
                     DEFAULT_WINDOWED_FULLSCREEN,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_fullscreen_x,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
                     DEFAULT_FULLSCREEN_X,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 7680, 8, true, true);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_fullscreen_y,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
                     DEFAULT_FULLSCREEN_Y,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 4320, 8, true, true);
            }

#if defined(DINGUX) && defined(DINGUX_BETA)
            if (string_is_equal(settings->arrays.video_driver, "sdl_dingux") ||
                string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_dingux_refresh_rate,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_REFRESH_RATE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
                     DEFAULT_DINGUX_REFRESH_RATE,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_uint_video_dingux_refresh_rate;
               menu_settings_list_current_add_range(list, list_info, 0, DINGUX_REFRESH_RATE_LAST - 1, 1, true, true);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            }
            else
#endif
            {
               float actual_refresh_rate = video_driver_get_refresh_rate();

               CONFIG_FLOAT(
                     list, list_info,
                     &settings->floats.video_refresh_rate,
                     MENU_ENUM_LABEL_VIDEO_REFRESH_RATE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
                     DEFAULT_REFRESH_RATE,
                     "%.3f Hz",
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 0, 0.001, true, false);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_FLOAT(
                     list, list_info,
                     &settings->floats.video_refresh_rate,
                     MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO,
                     MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
                     DEFAULT_REFRESH_RATE,
                     "%.3f Hz",
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_start  = &setting_action_start_video_refresh_rate_auto;
               (*list)[list_info->index - 1].action_ok     = &setting_action_ok_video_refresh_rate_auto;
               (*list)[list_info->index - 1].action_select = &setting_action_ok_video_refresh_rate_auto;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_st_float_video_refresh_rate_auto;
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               if (actual_refresh_rate > 0.0)
               {
                  CONFIG_FLOAT(
                     list, list_info,
                     &settings->floats.video_refresh_rate,
                     MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED,
                     MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
                     actual_refresh_rate,
                     "%.3f Hz",
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
                  (*list)[list_info->index - 1].action_start  = &setting_action_start_video_refresh_rate_polled;
                  (*list)[list_info->index - 1].action_ok     = &setting_action_ok_video_refresh_rate_polled;
                  (*list)[list_info->index - 1].action_select = &setting_action_ok_video_refresh_rate_polled;
                  (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_st_float_video_refresh_rate_polled;
                  SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               }
            }

            if (string_is_equal(settings->arrays.video_driver, "gl"))
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_force_srgb_disable,
                     MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
                     false,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_ADVANCED
                     );
               MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            }

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Aspect", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_aspect_ratio_idx,
                  MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
                  MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
                  DEFAULT_ASPECT_RATIO_IDX,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_SET_ASPECT_RATIO);
            menu_settings_list_current_add_range(
                  list,
                  list_info,
                  0,
                  LAST_ASPECT_RATIO,
                  1,
                  true,
                  true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_aspect_ratio_index;
            (*list)[list_info->index - 1].action_left   = setting_uint_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_uint_action_right_with_refresh;

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.video_aspect_ratio,
                  MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO,
                  MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
                  DEFAULT_ASPECT_RATIO,
                  "%.2f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_SET_ASPECT_RATIO);
            menu_settings_list_current_add_range(list, list_info, 0.1, 16.0, 0.01, true, false);

            CONFIG_INT(
                  list, list_info,
                  &custom_vp->x,
                  MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -9999, 9999, 1, true, true);
            (*list)[list_info->index - 1].offset_by = -9999;
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_INT(
                  list, list_info,
                  &custom_vp->y,
                  MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -9999, 9999, 1, true, true);
            (*list)[list_info->index - 1].offset_by = -9999;
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#if defined(GEKKO) || defined(PS2) || !defined(__PSL1GHT__) && defined(__PS3__)
            if (true)
#else
            if (!string_is_equal(video_display_server_get_ident(), "null"))
#endif
            {
               CONFIG_ACTION(
                     list, list_info,
                     MENU_ENUM_LABEL_SCREEN_RESOLUTION,
                     MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
                     &group_info,
                     &subgroup_info,
                     parent_group);
            }

#if defined(HAVE_WINDOW_OFFSET)
            CONFIG_INT(
                  list, list_info,
                  &settings->ints.video_window_offset_x,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_X,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
                  DEFAULT_WINDOW_OFFSET_X,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -50, 50, 1, true, true);

            CONFIG_INT(
                  list, list_info,
                  &settings->ints.video_window_offset_y,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_Y,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
                  DEFAULT_WINDOW_OFFSET_Y,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -50, 50, 1, true, true);
#endif

            CONFIG_UINT(
                  list, list_info,
                  &custom_vp->width,
                  MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 9999, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_custom_viewport_width;
            (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_width;
            (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_custom_viewport_width;
            (*list)[list_info->index - 1].action_right = &setting_uint_action_right_custom_viewport_width;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &custom_vp->height,
                  MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 9999, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_custom_viewport_height;
            (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_height;
            (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_custom_viewport_height;
            (*list)[list_info->index - 1].action_right = &setting_uint_action_right_custom_viewport_height;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#if defined(DINGUX)
            if (string_is_equal(settings->arrays.video_driver, "sdl_dingux") ||
                string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_dingux_ipu_keep_aspect,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
                     DEFAULT_DINGUX_IPU_KEEP_ASPECT,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
               MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                     list,
                     list_info,
                     CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            }
#endif

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Scaling", &group_info, &subgroup_info, parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_VIDEO_SCALING_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);

            if (video_driver_has_windowed())
            {
               CONFIG_FLOAT(
                     list, list_info,
                     &settings->floats.video_scale,
                     MENU_ENUM_LABEL_VIDEO_SCALE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
                     DEFAULT_SCALE,
                     "%.1fx",
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 1.0, 10.0, 1.0, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.window_position_width,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
                     DEFAULT_WINDOW_WIDTH,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 7680, 8, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.window_position_height,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
                     DEFAULT_WINDOW_HEIGHT,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 4320, 8, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.window_auto_width_max,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
                     DEFAULT_WINDOW_AUTO_WIDTH_MAX,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 7680, 8, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.window_auto_height_max,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
                     DEFAULT_WINDOW_AUTO_HEIGHT_MAX,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 0, 4320, 8, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_window_opacity,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
                     DEFAULT_WINDOW_OPACITY,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].offset_by = 1;
               menu_settings_list_current_add_range(list, list_info, 1, 100, 1, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_window_show_decorations,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
                  DEFAULT_WINDOW_DECORATIONS,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_window_save_positions,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
                  DEFAULT_WINDOW_SAVE_POSITIONS,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
#else
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_window_custom_size_enable,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
                  DEFAULT_WINDOW_CUSTOM_SIZE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
#endif
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_scale_integer,
                  MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
                  DEFAULT_SCALE_INTEGER,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_scale_integer_overscale,
                  MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_OVERSCALE,
                  DEFAULT_SCALE_INTEGER_OVERSCALE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);

#ifdef GEKKO
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_viwidth,
                  MENU_ENUM_LABEL_VIDEO_VI_WIDTH,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
                  DEFAULT_VIDEO_VI_WIDTH,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 640, 720, 2, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_vfilter,
                  MENU_ENUM_LABEL_VIDEO_VFILTER,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
                  DEFAULT_VIDEO_VFILTER,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_overscan_correction_top,
                  MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
                  MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
                  DEFAULT_VIDEO_OVERSCAN_CORRECTION_TOP,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 24, 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_overscan_correction_bottom,
                  MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
                  MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
                  DEFAULT_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 24, 1, true, true);
#endif

#if defined(DINGUX)
            if (string_is_equal(settings->arrays.video_driver, "sdl_dingux"))
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_dingux_ipu_filter_type,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
                     DEFAULT_DINGUX_IPU_FILTER_TYPE,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_uint_video_dingux_ipu_filter_type;
               menu_settings_list_current_add_range(list, list_info, 0, DINGUX_IPU_FILTER_LAST - 1, 1, true, true);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            }
#if defined(RS90) || defined(MIYOO)
            else if (string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_dingux_rs90_softfilter_type,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
                     DEFAULT_DINGUX_RS90_SOFTFILTER_TYPE,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_uint_video_dingux_rs90_softfilter_type;
               menu_settings_list_current_add_range(list, list_info, 0, DINGUX_RS90_SOFTFILTER_LAST - 1, 1, true, true);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            }
#endif
            else
#endif
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_smooth,
                     MENU_ENUM_LABEL_VIDEO_SMOOTH,
                     MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
                     DEFAULT_VIDEO_SMOOTH,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE
                     );
               MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            }

#ifdef HAVE_ODROIDGO2
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_ctx_scaling,
                  MENU_ENUM_LABEL_VIDEO_CTX_SCALING,
                  MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
                  DEFAULT_VIDEO_CTX_SCALING,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_rotation,
                  MENU_ENUM_LABEL_VIDEO_ROTATION,
                  MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_video_rotation;
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.screen_orientation,
                  MENU_ENUM_LABEL_SCREEN_ORIENTATION,
                  MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_screen_orientation;
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);

            if(video_driver_supports_hdr())
            {
               START_SUB_GROUP(list, list_info, "HDR", &group_info, &subgroup_info, parent_group);

               CONFIG_ACTION(
                     list, list_info,
                     MENU_ENUM_LABEL_VIDEO_HDR_SETTINGS,
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SETTINGS,
                     &group_info,
                     &subgroup_info,
                     parent_group);

               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_hdr_enable,
                     MENU_ENUM_LABEL_VIDEO_HDR_ENABLE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
                     DEFAULT_VIDEO_HDR_ENABLE,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
               (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
               (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
               (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
               MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                     list,
                     list_info,
                     CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);

               /* if (settings->bools.video_hdr_enable) */
               {
                  CONFIG_FLOAT(
                        list, list_info,
                        &settings->floats.video_hdr_max_nits,
                        MENU_ENUM_LABEL_VIDEO_HDR_MAX_NITS,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
                        DEFAULT_VIDEO_HDR_MAX_NITS,
                        "%.1fx",
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler);
                  (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
                  menu_settings_list_current_add_range(list, list_info, 0.0, 10000.0, 10.0, true, true);

                  CONFIG_FLOAT(
                        list, list_info,
                        &settings->floats.video_hdr_paper_white_nits,
                        MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
                        DEFAULT_VIDEO_HDR_PAPER_WHITE_NITS,
                        "%.1fx",
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler);
                  (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
                  menu_settings_list_current_add_range(list, list_info, 0.0, 10000.0, 10.0, true, true);

                  CONFIG_FLOAT(
                        list, list_info,
                        &settings->floats.video_hdr_display_contrast,
                        MENU_ENUM_LABEL_VIDEO_HDR_CONTRAST,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
                        DEFAULT_VIDEO_HDR_CONTRAST,
                        "%.2fx",
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler);
                  (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
                  menu_settings_list_current_add_range(list, list_info, 0.0, VIDEO_HDR_MAX_CONTRAST, 0.1, true, true);

                  CONFIG_BOOL(
                        list, list_info,
                        &settings->bools.video_hdr_expand_gamut,
                        MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
                        DEFAULT_VIDEO_HDR_EXPAND_GAMUT,
                        MENU_ENUM_LABEL_VALUE_OFF,
                        MENU_ENUM_LABEL_VALUE_ON,
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler,
                        SD_FLAG_NONE);
                  (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
                  (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
                  (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
                  MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                        list,
                        list_info,
                        CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
               }

               END_SUB_GROUP(list, list_info, parent_group);
            }

            START_SUB_GROUP(
                  list,
                  list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC),
                  &group_info,
                  &subgroup_info,
                  parent_group);

            if (frontend_driver_can_set_screen_brightness())
            {
               CONFIG_UINT(
                      list, list_info,
                      &settings->uints.screen_brightness,
                      MENU_ENUM_LABEL_BRIGHTNESS_CONTROL,
                      MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
                      DEFAULT_SCREEN_BRIGHTNESS,
                      &group_info,
                      &subgroup_info,
                      parent_group,
                      general_write_handler,
                      general_read_handler);
                (*list)[list_info->index - 1].ui_type = ST_UI_TYPE_UINT_COMBOBOX;
                (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
                (*list)[list_info->index - 1].get_string_representation =
                   &setting_get_string_representation_percentage;
                menu_settings_list_current_add_range(list, list_info, 5, 100, 5, true, true);
            }

#if defined(HAVE_THREADS) && !defined(__PSL1GHT__) && !defined(__PS3__)
            CONFIG_BOOL(
                  list, list_info,
                  video_driver_get_threaded(),
                  MENU_ENUM_LABEL_VIDEO_THREADED,
                  MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
                  DEFAULT_VIDEO_THREADED,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO
                  );
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_vsync,
                  MENU_ENUM_LABEL_VIDEO_VSYNC,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
                  DEFAULT_VSYNC,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_swap_interval,
                  MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
                  DEFAULT_SWAP_INTERVAL,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_VIDEO_SET_BLOCKING_STATE);
            menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_max_swapchain_images,
                  MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
                  MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
                  DEFAULT_MAX_SWAPCHAIN_IMAGES,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 2;
            menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index - 1].offset_by, 4, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_hard_sync,
                  MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
                  MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
                  DEFAULT_HARD_SYNC,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_hard_sync_frames,
                  MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
                  MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
                  DEFAULT_HARD_SYNC_FRAMES,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC))
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_adaptive_vsync,
                     MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC,
                     MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
                     DEFAULT_ADAPTIVE_VSYNC,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE
                     );
            }

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_frame_delay,
                  MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
                  MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
                  DEFAULT_FRAME_DELAY,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, MAXIMUM_FRAME_DELAY, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_frame_delay_auto,
                  MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO,
                  MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
                  DEFAULT_FRAME_DELAY_AUTO,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            /* Unlike all other shader-related menu entries
             * (which appear in the shaders quick menu, and
             * are thus hidden automatically on platforms
             * without shader support), VIDEO_SHADER_DELAY
             * is shown in 'Settings > Video'. It therefore
             * requires an explicit guard to prevent display
             * on unsupported platforms */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            if (video_shader_any_supported())
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_shader_delay,
                     MENU_ENUM_LABEL_VIDEO_SHADER_DELAY,
                     MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
                     DEFAULT_SHADER_DELAY,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
            }
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_shader_watch_files,
                  MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES,
                  MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
                  DEFAULT_VIDEO_SHADER_WATCH_FILES,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_shader_remember_last_dir,
                  MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
                  DEFAULT_VIDEO_SHADER_REMEMBER_LAST_DIR,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

#if !defined(RARCH_MOBILE)
            if (video_driver_test_all_flags(GFX_CTX_FLAGS_BLACK_FRAME_INSERTION))
            {

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_black_frame_insertion,
                     MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION,
                     MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
                     DEFAULT_BLACK_FRAME_INSERTION,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 0, 5, 1, true, true);
            }  
#endif
            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(
                  list,
                  list_info,
                  "Miscellaneous",
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_gpu_screenshot,
                  MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT,
                  MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
                  DEFAULT_GPU_SCREENSHOT,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_crop_overscan,
                  MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN,
                  MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
                  DEFAULT_CROP_OVERSCAN,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_PATH(
                  list, list_info,
                  settings->paths.path_softfilter_plugin,
                  sizeof(settings->paths.path_softfilter_plugin),
                  MENU_ENUM_LABEL_VIDEO_FILTER,
                  MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
                  settings->paths.directory_video_filter,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_video_filter;
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "filt");
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CRT_SWITCHRES:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS), parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

			CONFIG_UINT(
               list, list_info,
               &settings->uints.crt_switch_resolution,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
               DEFAULT_CRT_SWITCH_RESOLUTION,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_crt_switch_resolutions;
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_range(list, list_info, CRT_SWITCH_NONE, CRT_SWITCH_INI, 1.0, true, true);

			CONFIG_UINT(
				  list, list_info,
				  &settings->uints.crt_switch_resolution_super,
				  MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER,
				  MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
				  DEFAULT_CRT_SWITCH_RESOLUTION_SUPER,
				  &group_info,
				  &subgroup_info,
				  parent_group,
				  general_write_handler,
				  general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
         (*list)[list_info->index - 1].action_left   = &setting_uint_action_left_crt_switch_resolution_super;
         (*list)[list_info->index - 1].action_right  = &setting_uint_action_right_crt_switch_resolution_super;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_crt_switch_resolution_super;

			CONFIG_INT(
				  list, list_info,
				  &settings->ints.crt_switch_center_adjust,
				  MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING,
				  MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
				  DEFAULT_CRT_SWITCH_CENTER_ADJUST,
				  &group_info,
				  &subgroup_info,
				  parent_group,
				  general_write_handler,
				  general_read_handler);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_UINT_SPINBOX;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = -3;
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_range(list, list_info, -3, 4, 1.0, true, true);

         CONFIG_INT(
				  list, list_info,
				  &settings->ints.crt_switch_porch_adjust,
				  MENU_ENUM_LABEL_CRT_SWITCH_PORCH_ADJUST,
				  MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
				  DEFAULT_CRT_SWITCH_PORCH_ADJUST,
				  &group_info,
				  &subgroup_info,
				  parent_group,
				  general_write_handler,
				  general_read_handler);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_UINT_SPINBOX;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = 0;
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_range(list, list_info, -20, 20, 1.0, true, true);
         
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.crt_switch_custom_refresh_enable,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.crt_switch_hires_menu,
               MENU_ENUM_LABEL_CRT_SWITCH_HIRES_MENU,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MENU_SOUNDS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SOUNDS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable_menu,
               MENU_ENUM_LABEL_AUDIO_ENABLE_MENU,
               MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
               audio_enable_menu,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable_menu_ok,
               MENU_ENUM_LABEL_MENU_SOUND_OK,
               MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
               audio_enable_menu_ok,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable_menu_cancel,
               MENU_ENUM_LABEL_MENU_SOUND_CANCEL,
               MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
               audio_enable_menu_cancel,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable_menu_notice,
               MENU_ENUM_LABEL_MENU_SOUND_NOTICE,
               MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
               audio_enable_menu_notice,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable_menu_bgm,
               MENU_ENUM_LABEL_MENU_SOUND_BGM,
               MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
               audio_enable_menu_bgm,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_AUDIO:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS), parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_AUDIO_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable,
               MENU_ENUM_LABEL_AUDIO_ENABLE,
               MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
               DEFAULT_AUDIO_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE),
               MENU_ENUM_LABEL_AUDIO_MUTE,
               MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

#ifdef HAVE_AUDIOMIXER
         CONFIG_BOOL(
               list, list_info,
               audio_get_bool_ptr(AUDIO_ACTION_MIXER_MUTE_ENABLE),
               MENU_ENUM_LABEL_AUDIO_MIXER_MUTE,
               MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_LAKKA_ADVANCED
               );
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_fastforward_mute,
               MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE,
               MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
               DEFAULT_AUDIO_FASTFORWARD_MUTE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_volume,
               MENU_ENUM_LABEL_AUDIO_VOLUME,
               MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
               DEFAULT_AUDIO_VOLUME,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);

#ifdef HAVE_AUDIOMIXER
         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_mixer_volume,
               MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME,
               MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
               DEFAULT_AUDIO_MIXER_VOLUME,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(
               list,
               list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC),
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_sync,
               MENU_ENUM_LABEL_AUDIO_SYNC,
               MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
               DEFAULT_AUDIO_SYNC,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_latency,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
               g_defaults.settings_out_latency ?
               g_defaults.settings_out_latency : DEFAULT_OUT_LATENCY,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 512, 1.0, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_resampler_quality,
               MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY,
               MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
               audio_resampler_quality_level,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_audio_resampler_quality;
         menu_settings_list_current_add_range(list, list_info, RESAMPLER_QUALITY_DONTCARE, RESAMPLER_QUALITY_HIGHEST, 1.0, true, true);

         CONFIG_FLOAT(
               list, list_info,
               audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA),
               MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
               MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
               DEFAULT_RATE_CONTROL_DELTA,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               write_handler_audio_rate_control_delta,
               read_handler_audio_rate_control_delta);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               0,
               0.001,
               true,
               false);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_max_timing_skew,
               MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW,
               MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
               DEFAULT_MAX_TIMING_SKEW,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(
               list,
               list_info,
               0.0,
               0.5,
               0.01,
               true,
               true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

#ifdef RARCH_MOBILE
         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_block_frames,
               MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES,
               MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(
               list,
               list_info,
               "Miscellaneous",
               &group_info,
               &subgroup_info,
               parent_group);

#if !defined(RARCH_CONSOLE)
         CONFIG_STRING(
               list, list_info,
               settings->arrays.audio_device,
               sizeof(settings->arrays.audio_device),
               MENU_ENUM_LABEL_AUDIO_DEVICE,
               MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;
         (*list)[list_info->index - 1].action_left   = &setting_string_action_left_audio_device;
         (*list)[list_info->index - 1].action_right  = &setting_string_action_right_audio_device;
#endif

         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_output_sample_rate,
               MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE,
               MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
               DEFAULT_OUTPUT_RATE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint_special;
         menu_settings_list_current_add_range(list, list_info, 1000, 192000, 100.0, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_PATH(
               list, list_info,
               settings->paths.path_audio_dsp_plugin,
               sizeof(settings->paths.path_audio_dsp_plugin),
               MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,
               MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
               settings->paths.directory_audio_filter,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "dsp");
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_DSP_FILTER_INIT);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_WASAPI
         if (string_is_equal(settings->arrays.audio_driver, "wasapi"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.audio_wasapi_exclusive_mode,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
                  MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
                  DEFAULT_WASAPI_EXCLUSIVE_MODE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.audio_wasapi_float_format,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT,
                  MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
                  DEFAULT_WASAPI_FLOAT_FORMAT,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_INT(
                  list, list_info,
                  &settings->ints.audio_wasapi_sh_buffer_length,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
                  MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
                  DEFAULT_WASAPI_SH_BUFFER_LENGTH,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -16.0f, 0.0f, 16.0f, true, false);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_int_audio_wasapi_sh_buffer_length;
            }
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_INPUT:
         {
            unsigned user;
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS_BEGIN),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_max_users,
                  MENU_ENUM_LABEL_INPUT_MAX_USERS,
                  MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
                  input_max_users,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok    = &setting_action_ok_uint;
            (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_max_users;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, MAX_USERS, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_unified_controls,
                  MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS,
                  MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.quit_press_twice,
                  MENU_ENUM_LABEL_QUIT_PRESS_TWICE,
                  MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
                  DEFAULT_QUIT_PRESS_TWICE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.vibrate_on_keypress,
                  MENU_ENUM_LABEL_VIBRATE_ON_KEYPRESS,
                  MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
                  vibrate_on_keypress,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.enable_device_vibration,
                  MENU_ENUM_LABEL_ENABLE_DEVICE_VIBRATION,
                  MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
                  enable_device_vibration,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_rumble_gain,
                  MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN,
                  MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
                  DEFAULT_RUMBLE_GAIN,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_percentage;
            menu_settings_list_current_add_range(list, list_info, 0, 100, 5, true, true);
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_poll_type_behavior,
                  MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
                  MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
                  input_poll_type_behavior,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_poll_type_behavior;
            menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef GEKKO
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_mouse_scale,
                  MENU_ENUM_LABEL_INPUT_MOUSE_SCALE,
                  MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
                  DEFAULT_MOUSE_SCALE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_touch_scale,
                  MENU_ENUM_LABEL_INPUT_TOUCH_SCALE,
                  MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
                  DEFAULT_TOUCH_SCALE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_input_touch_scale;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);

#ifdef VITA
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_backtouch_enable,
                  MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
                  input_backtouch_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_backtouch_toggle,
                  MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH,
                  MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
                  input_backtouch_toggle,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
#endif

#if TARGET_OS_IPHONE
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_keyboard_gamepad_enable,
                  MENU_ENUM_LABEL_INPUT_ICADE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_keyboard_gamepad_mapping_type,
                  MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
                  MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
                  1,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_keyboard_gamepad_mapping_type;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_small_keyboard_enable,
                  MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_menu_toggle_gamepad_combo,
                  MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
                  MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
                  DEFAULT_MENU_TOGGLE_GAMEPAD_COMBO,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_gamepad_combo;
            menu_settings_list_current_add_range(list, list_info, 0, (INPUT_COMBO_LAST-1), 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_quit_gamepad_combo,
                  MENU_ENUM_LABEL_INPUT_QUIT_GAMEPAD_COMBO,
                  MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
                  DEFAULT_QUIT_GAMEPAD_COMBO,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_gamepad_combo;
            menu_settings_list_current_add_range(list, list_info, 0, (INPUT_COMBO_LAST-1), 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_hotkey_block_delay,
                  MENU_ENUM_LABEL_INPUT_HOTKEY_BLOCK_DELAY,
                  MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
                  DEFAULT_INPUT_HOTKEY_BLOCK_DELAY,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 600, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_menu_swap_ok_cancel_buttons,
                  MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL,
                  MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
                  DEFAULT_MENU_SWAP_OK_CANCEL_BUTTONS,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_all_users_control_menu,
                  MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU,
                  MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
                  DEFAULT_ALL_USERS_CONTROL_MENU,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_remap_binds_enable,
                  MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_autodetect_enable,
                  MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
                  input_autodetect_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_nowinkey_enable,
                  MENU_ENUM_LABEL_INPUT_NOWINKEY_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_sensors_enable,
                  MENU_ENUM_LABEL_INPUT_SENSORS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
                  DEFAULT_INPUT_SENSORS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_auto_mouse_grab,
                  MENU_ENUM_LABEL_INPUT_AUTO_MOUSE_GRAB,
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_auto_game_focus,
                  MENU_ENUM_LABEL_INPUT_AUTO_GAME_FOCUS,
                  MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
                  DEFAULT_INPUT_AUTO_GAME_FOCUS,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_input_auto_game_focus;
            menu_settings_list_current_add_range(list, list_info, 0, AUTO_GAME_FOCUS_LAST-1, 1, true, true);
#if 0
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_descriptor_label_show,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW,
                  MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_descriptor_hide_unbound,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND,
                  MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
                  input_descriptor_hide_unbound,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );
#endif

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(
                  list,
                  list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST),
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.input_axis_threshold,
                  MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD,
                  MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
                  DEFAULT_AXIS_THRESHOLD,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 1.0, 0.01, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.input_analog_deadzone,
                  MENU_ENUM_LABEL_INPUT_ANALOG_DEADZONE,
                  MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
                  DEFAULT_ANALOG_DEADZONE,
                  "%.1f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 1.0, 0.1, true, true);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.input_analog_sensitivity,
                  MENU_ENUM_LABEL_INPUT_ANALOG_SENSITIVITY,
                  MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
                  DEFAULT_ANALOG_SENSITIVITY,
                  "%.1f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, -5.0, 5.0, 0.1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_bind_timeout,
                  MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT,
                  MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
                  input_bind_timeout,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 10, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_bind_hold,
                  MENU_ENUM_LABEL_INPUT_BIND_HOLD,
                  MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
                  input_bind_hold,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 10, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_MENU_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);


            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Binds", &group_info, &subgroup_info, parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,
                  MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
                  &group_info,
                  &subgroup_info,
                  parent_group);

            for (user = 0; user < MAX_USERS; user++)
            {
               static char binds_list[MAX_USERS][255];
               static char binds_label[MAX_USERS][255];
               unsigned user_value = user + 1;

               snprintf(binds_list[user],  sizeof(binds_list[user]), "%d_input_binds_list", user_value);
               snprintf(binds_label[user], sizeof(binds_label[user]), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS), user_value);

               CONFIG_ACTION_ALT(
                     list, list_info,
                     binds_list[user],
                     binds_label[user],
                     &group_info,
                     &subgroup_info,
                     parent_group);
               (*list)[list_info->index - 1].ui_type        = ST_UI_TYPE_BIND_BUTTON;
               (*list)[list_info->index - 1].index          = user_value;
               (*list)[list_info->index - 1].index_offset   = user;

               MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));
            }

            END_SUB_GROUP(list, list_info, parent_group);

            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_INPUT_TURBO_FIRE:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_turbo_period,
               MENU_ENUM_LABEL_INPUT_TURBO_PERIOD,
               MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
               turbo_period,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 100, 1, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_turbo_duty_cycle,
               MENU_ENUM_LABEL_INPUT_DUTY_CYCLE,
               MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
               turbo_duty_cycle,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 100, 1, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_turbo_mode,
               MENU_ENUM_LABEL_INPUT_TURBO_MODE,
               MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
               turbo_mode,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_turbo_mode;
         menu_settings_list_current_add_range(list, list_info, 0, (INPUT_TURBO_MODE_LAST-1), 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_turbo_default_button,
               MENU_ENUM_LABEL_INPUT_TURBO_DEFAULT_BUTTON,
               MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
               turbo_default_btn,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_turbo_default_button;
         menu_settings_list_current_add_range(list, list_info, 0, (INPUT_TURBO_DEFAULT_BUTTON_LAST-1), 1, true, true);

         END_SUB_GROUP(list, list_info, parent_group);

         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_RECORDING:
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
               list, list_info,
               &settings->uints.video_record_quality,
               MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY,
               MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
               RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_video_record_quality;
            menu_settings_list_current_add_range(list, list_info, RECORD_CONFIG_TYPE_RECORDING_CUSTOM, RECORD_CONFIG_TYPE_RECORDING_APNG, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_PATH(
               list, list_info,
               settings->paths.path_record_config,
               sizeof(settings->paths.path_record_config),
               MENU_ENUM_LABEL_RECORD_CONFIG,
               MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "cfg");

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.streaming_title,
                  sizeof(settings->paths.streaming_title),
                  MENU_ENUM_LABEL_STREAMING_TITLE,
                  MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_UINT(
               list, list_info,
               &settings->uints.streaming_mode,
               MENU_ENUM_LABEL_STREAMING_MODE,
               MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
               STREAMING_MODE_TWITCH,
               &group_info,
               &subgroup_info,
               parent_group,
               update_streaming_url_write_handler,
               general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_streaming_mode;
            menu_settings_list_current_add_range(list, list_info, 0, STREAMING_MODE_CUSTOM, 1, true, true);

            CONFIG_UINT(
               list, list_info,
               &settings->uints.video_stream_port,
               MENU_ENUM_LABEL_UDP_STREAM_PORT,
               MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
               RARCH_STREAM_DEFAULT_PORT,
               &group_info,
               &subgroup_info,
               parent_group,
               update_streaming_url_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 65536, 1, true, true);


            CONFIG_UINT(
               list, list_info,
               &settings->uints.video_stream_quality,
               MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY,
               MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
               RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_video_stream_quality;
               (*list)[list_info->index - 1].offset_by = RECORD_CONFIG_TYPE_STREAMING_CUSTOM;
            menu_settings_list_current_add_range(list, list_info, RECORD_CONFIG_TYPE_STREAMING_CUSTOM, RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY, 1, true, true);

            CONFIG_PATH(
               list, list_info,
               settings->paths.path_stream_config,
               sizeof(settings->paths.path_stream_config),
               MENU_ENUM_LABEL_STREAM_CONFIG,
               MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "cfg");
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_FILE_SELECTOR;

            CONFIG_STRING(
               list, list_info,
               settings->paths.path_stream_url,
               sizeof(settings->paths.path_stream_url),
               MENU_ENUM_LABEL_STREAMING_URL,
               MENU_ENUM_LABEL_VALUE_STREAMING_URL,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_UINT(
               list, list_info,
               &settings->uints.video_record_threads,
               MENU_ENUM_LABEL_VIDEO_RECORD_THREADS,
               MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
               DEFAULT_VIDEO_RECORD_THREADS,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
               menu_settings_list_current_add_range(list, list_info, 1, 8, 1, true, true);
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_DIR(
               list, list_info,
               recording_st->output_dir,
               sizeof(recording_st->output_dir),
               MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
               (*list)[list_info->index - 1].action_start = directory_action_start_generic;

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Miscellaneous", &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_post_filter_record,
                  MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,
                  MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
                  DEFAULT_POST_FILTER_RECORD,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_gpu_record,
                  MENU_ENUM_LABEL_VIDEO_GPU_RECORD,
                  MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
                  DEFAULT_GPU_RECORD,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_INPUT_HOTKEY:
         {
            unsigned i;
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS_BEGIN),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            for (i = 0; i < RARCH_BIND_LIST_END; i ++)
            {
               if (!input_config_bind_map_get_meta(i))
                  continue;
#ifndef HAVE_QT
               if (i == RARCH_UI_COMPANION_TOGGLE)
                  continue;
#endif
               CONFIG_BIND_ALT(
                     list, list_info,
                     &input_config_binds[0][i],
                     0, 0,
                     strdup(input_config_bind_map_get_base(i)),
                     strdup(input_config_bind_map_get_desc(i)),
                     &retro_keybinds_1[i],
                     &group_info, &subgroup_info, parent_group);
               (*list)[list_info->index - 1].ui_type        = ST_UI_TYPE_BIND_BUTTON;
               (*list)[list_info->index - 1].bind_type      = i + MENU_SETTINGS_BIND_BEGIN;
               MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
                     (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i));
            }

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_FRAME_THROTTLING:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.fastforward_ratio,
               MENU_ENUM_LABEL_FASTFORWARD_RATIO,
               MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
               DEFAULT_FASTFORWARD_RATIO,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_SET_FRAME_LIMIT);
         menu_settings_list_current_add_range(list, list_info, 0, MAXIMUM_FASTFORWARD_RATIO, 1.0, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.fastforward_frameskip,
               MENU_ENUM_LABEL_FASTFORWARD_FRAMESKIP,
               MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
               DEFAULT_FASTFORWARD_FRAMESKIP,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.vrr_runloop_enable,
               MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE,
               MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.slowmotion_ratio,
               MENU_ENUM_LABEL_SLOWMOTION_RATIO,
               MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
               DEFAULT_SLOWMOTION_RATIO,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 1, 10, 0.1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.run_ahead_enabled,
               MENU_ENUM_LABEL_RUN_AHEAD_ENABLED,
               MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_UINT(
            list, list_info,
            &settings->uints.run_ahead_frames,
            MENU_ENUM_LABEL_RUN_AHEAD_FRAMES,
            MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
            1,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 12, 1, true, true);

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.run_ahead_secondary_instance,
               MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE,
               MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
               DEFAULT_RUN_AHEAD_SECONDARY_INSTANCE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.run_ahead_hide_warnings,
               MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS,
               MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
               DEFAULT_RUN_AHEAD_HIDE_WARNINGS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

#ifdef ANDROID
         CONFIG_UINT(
            list, list_info,
            &settings->uints.input_block_timeout,
            MENU_ENUM_LABEL_INPUT_BLOCK_TIMEOUT,
            MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
            1,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 0, 4, 1, true, true);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_throttle_framerate,
               MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE,
               MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_ONSCREEN_NOTIFICATIONS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS);

         START_SUB_GROUP(list, list_info, "Notifications",
               &group_info,
               &subgroup_info,
               parent_group);

#ifdef HAVE_GFX_WIDGETS
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_enable_widgets,
               MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE,
               MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
               DEFAULT_MENU_ENABLE_WIDGETS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_widget_scale_auto,
               MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO,
               MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
               DEFAULT_MENU_WIDGET_SCALE_AUTO,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.menu_widget_scale_factor,
               MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR,
               MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
               DEFAULT_MENU_WIDGET_SCALE_FACTOR,
               "%.2fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
#else
         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.menu_widget_scale_factor,
               MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR,
               MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
               DEFAULT_MENU_WIDGET_SCALE_FACTOR,
               "%.2fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
#endif
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0.2, 5.0, 0.01, true, true);

#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.menu_widget_scale_factor_windowed,
               MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
               MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
               DEFAULT_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
               "%.2fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0.2, 5.0, 0.01, true, true);
#endif
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_font_enable,
               MENU_ENUM_LABEL_VIDEO_FONT_ENABLE,
               MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
               DEFAULT_FONT_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

         CONFIG_PATH(
               list, list_info,
               settings->paths.path_font,
               sizeof(settings->paths.path_font),
               MENU_ENUM_LABEL_VIDEO_FONT_PATH,
               MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "ttf");
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_FONT_SELECTOR;

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_font_size,
               MENU_ENUM_LABEL_VIDEO_FONT_SIZE,
               MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
               DEFAULT_FONT_SIZE,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 1.00, 100.00, 1.0, true, true);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_pos_x,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
               message_pos_offset_x,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_pos_y,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
               message_pos_offset_y,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_color_r,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
               ((message_color >> 16) & 0xff) / 255.0f,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
#if 0
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
#endif
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_float_video_msg_color;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 1.0f/255.0f, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_color_g,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
               ((message_color >> 8) & 0xff) / 255.0f,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_float_video_msg_color;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 1.0f/255.0f, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_color_b,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
               ((message_color >> 0) & 0xff) / 255.0f,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_float_video_msg_color;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 1.0f/255.0f, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_msg_bgcolor_enable,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
               message_bgcolor_enable,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.video_msg_bgcolor_red,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
               message_bgcolor_red,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.video_msg_bgcolor_green,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
               message_bgcolor_green,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.video_msg_bgcolor_blue,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
               message_bgcolor_blue,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_msg_bgcolor_opacity,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
               MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
               message_bgcolor_opacity,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Notification Views", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_fps_show,
               MENU_ENUM_LABEL_FPS_SHOW,
               MENU_ENUM_LABEL_VALUE_FPS_SHOW,
               DEFAULT_FPS_SHOW,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.fps_update_interval,
               MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL,
               MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
               DEFAULT_FPS_UPDATE_INTERVAL,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint_special;
         menu_settings_list_current_add_range(list, list_info, 1, 512, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_memory_show,
               MENU_ENUM_LABEL_MEMORY_SHOW,
               MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
               DEFAULT_MEMORY_SHOW,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.memory_update_interval,
               MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL,
               MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
               DEFAULT_MEMORY_UPDATE_INTERVAL,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint_special;
         menu_settings_list_current_add_range(list, list_info, 1, 512, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_statistics_show,
               MENU_ENUM_LABEL_STATISTICS_SHOW,
               MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
               DEFAULT_STATISTICS_SHOW,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_framecount_show,
               MENU_ENUM_LABEL_FRAMECOUNT_SHOW,
               MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
               DEFAULT_FRAMECOUNT_SHOW,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_GFX_WIDGETS
#ifdef HAVE_NETWORKING
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.netplay_ping_show,
               MENU_ENUM_LABEL_NETPLAY_PING_SHOW,
               MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
               DEFAULT_NETPLAY_PING_SHOW,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok    = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left  = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_bool_action_right_with_refresh;
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_load_content_animation,
               MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
               DEFAULT_MENU_SHOW_LOAD_CONTENT_ANIMATION,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_autoconfig,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
               DEFAULT_NOTIFICATION_SHOW_AUTOCONFIG,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_CHEATS
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_cheats_applied,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
               DEFAULT_NOTIFICATION_SHOW_CHEATS_APPLIED,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif
#ifdef HAVE_PATCH
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_patch_applied,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
               DEFAULT_NOTIFICATION_SHOW_PATCH_APPLIED,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_remap_load,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_REMAP_LOAD,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
               DEFAULT_NOTIFICATION_SHOW_REMAP_LOAD,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_config_override_load,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
               DEFAULT_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_set_initial_disk,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
               DEFAULT_NOTIFICATION_SHOW_SET_INITIAL_DISK,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_fast_forward,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_FAST_FORWARD,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
               DEFAULT_NOTIFICATION_SHOW_FAST_FORWARD,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_SCREENSHOTS
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_screenshot,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
               DEFAULT_NOTIFICATION_SHOW_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

#ifdef HAVE_GFX_WIDGETS
         CONFIG_UINT(
               list, list_info,
               &settings->uints.notification_show_screenshot_duration,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
               DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_notification_show_screenshot_duration;
         menu_settings_list_current_add_range(list, list_info, 0, NOTIFICATION_SHOW_SCREENSHOT_DURATION_LAST-1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.notification_show_screenshot_flash,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
               DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_notification_show_screenshot_flash;
         menu_settings_list_current_add_range(list, list_info, 0, NOTIFICATION_SHOW_SCREENSHOT_FLASH_LAST-1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
#endif
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_refresh_rate,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_REFRESH_RATE,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
               DEFAULT_NOTIFICATION_SHOW_REFRESH_RATE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_NETWORKING
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_netplay_extra,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
               DEFAULT_NOTIFICATION_SHOW_NETPLAY_EXTRA,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.notification_show_when_menu_is_alive,
               MENU_ENUM_LABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
               MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
               DEFAULT_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_OVERLAY:
#ifdef HAVE_OVERLAY
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_enable,
               MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
               config_overlay_enable_default(),
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_enable_autopreferred,
               MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED,
               MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
               DEFAULT_OVERLAY_ENABLE_AUTOPREFERRED,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         if (video_driver_test_all_flags(GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED))
         {
            CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_behind_menu,
               MENU_ENUM_LABEL_INPUT_OVERLAY_BEHIND_MENU,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
               DEFAULT_OVERLAY_BEHIND_MENU,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         }
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_hide_in_menu,
               MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
               DEFAULT_OVERLAY_HIDE_IN_MENU,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_hide_when_gamepad_connected,
               MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
               DEFAULT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_overlay_show_inputs,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
               DEFAULT_OVERLAY_SHOW_INPUTS,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler
               );
         (*list)[list_info->index - 1].ui_type                   = ST_UI_TYPE_UINT_COMBOBOX;
         (*list)[list_info->index - 1].action_ok                 = &setting_action_ok_uint;
         (*list)[list_info->index - 1].action_left               = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right              = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_input_overlay_show_inputs;
         menu_settings_list_current_add_range(list, list_info, 0, OVERLAY_SHOW_INPUT_LAST-1, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.input_overlay_show_inputs_port,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
               DEFAULT_OVERLAY_SHOW_INPUTS_PORT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler
               );
         (*list)[list_info->index - 1].action_ok                 = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_input_overlay_show_inputs_port;
         menu_settings_list_current_add_range(list, list_info, 0, MAX_USERS - 1, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_show_mouse_cursor,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
               DEFAULT_OVERLAY_SHOW_MOUSE_CURSOR,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_auto_rotate,
               MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
               DEFAULT_OVERLAY_AUTO_ROTATE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_auto_rotate_toggle_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_auto_scale,
               MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_SCALE,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
               DEFAULT_INPUT_OVERLAY_AUTO_SCALE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_PATH(
               list, list_info,
               settings->paths.path_overlay,
               sizeof(settings->paths.path_overlay),
               MENU_ENUM_LABEL_OVERLAY_PRESET,
               MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
               settings->paths.directory_overlay,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "cfg");
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_INIT);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_opacity,
               MENU_ENUM_LABEL_OVERLAY_OPACITY,
               MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
               DEFAULT_INPUT_OVERLAY_OPACITY,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_ALPHA_MOD);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_scale_landscape,
               MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_SCALE_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, 0.0f, 2.0f, 0.005, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_aspect_adjust_landscape,
               MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_x_separation_landscape,
               MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_X_SEPARATION_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_y_separation_landscape,
               MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_Y_SEPARATION_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_x_offset_landscape,
               MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_X_OFFSET_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -1.0f, 1.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_y_offset_landscape,
               MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
               DEFAULT_INPUT_OVERLAY_Y_OFFSET_LANDSCAPE,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -1.0f, 1.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_scale_portrait,
               MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_SCALE_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, 0.0f, 2.0f, 0.005, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_aspect_adjust_portrait,
               MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_x_separation_portrait,
               MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_X_SEPARATION_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_y_separation_portrait,
               MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_Y_SEPARATION_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -2.0f, 2.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_x_offset_portrait,
               MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_X_OFFSET_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -1.0f, 1.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_y_offset_portrait,
               MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT,
               MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
               DEFAULT_INPUT_OVERLAY_Y_OFFSET_PORTRAIT,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, -1.0f, 1.0f, 0.005f, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Onscreen Keyboard Overlay", &group_info, &subgroup_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
#ifdef HAVE_VIDEO_LAYOUT
      case SETTINGS_LIST_VIDEO_LAYOUT:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_layout_enable,
               MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE,
               MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               change_handler_video_layout_enable,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_PATH(
               list, list_info,
               settings->paths.path_video_layout,
               sizeof(settings->paths.path_video_layout),
               MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH,
               MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
               settings->paths.directory_video_layout,
               &group_info,
               &subgroup_info,
               parent_group,
               change_handler_video_layout_path,
               general_read_handler);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.video_layout_selected_view,
               MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW,
               MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               change_handler_video_layout_selected_view,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 1, false, false);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
#endif
      case SETTINGS_LIST_MENU:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS),
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_MENU_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         if (string_is_not_equal(settings->arrays.menu_driver, "rgui") &&
             string_is_not_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_PATH(
                  list, list_info,
                  settings->paths.path_menu_wallpaper,
                  sizeof(settings->paths.path_menu_wallpaper),
                  MENU_ENUM_LABEL_MENU_WALLPAPER,
                  MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "png");

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_wallpaper_opacity,
                  MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY,
                  MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
                  menu_wallpaper_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         if (string_is_not_equal(settings->arrays.menu_driver, "rgui"))
         {
            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_framebuffer_opacity,
                  MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY,
                  MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
                  menu_framebuffer_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_dynamic_wallpaper_enable,
                  MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,
                  MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
                  menu_dynamic_wallpaper_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_pause_libretro,
               MENU_ENUM_LABEL_PAUSE_LIBRETRO,
               MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO
               );
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_MENU_PAUSE_LIBRETRO);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_savestate_resume,
               MENU_ENUM_LABEL_MENU_SAVESTATE_RESUME,
               MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
               menu_savestate_resume,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_insert_disk_resume,
               MENU_ENUM_LABEL_MENU_INSERT_DISK_RESUME,
               MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
               DEFAULT_MENU_INSERT_DISK_RESUME,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_UINT(
               list, list_info,
               &settings->uints.quit_on_close_content,
               MENU_ENUM_LABEL_QUIT_ON_CLOSE_CONTENT,
               MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
               DEFAULT_QUIT_ON_CLOSE_CONTENT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_quit_on_close_content;
         menu_settings_list_current_add_range(list, list_info, 0, QUIT_ON_CLOSE_CONTENT_LAST-1, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_screensaver_timeout,
               MENU_ENUM_LABEL_MENU_SCREENSAVER_TIMEOUT,
               MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
               DEFAULT_MENU_SCREENSAVER_TIMEOUT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
         (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_screensaver_timeout;
         menu_settings_list_current_add_range(list, list_info, 0, 1800, 10, true, true);

#if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)
         if (string_is_equal(settings->arrays.menu_driver, "glui") ||
             string_is_equal(settings->arrays.menu_driver, "xmb")  ||
             string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_screensaver_animation,
                  MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION,
                  MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
                  DEFAULT_MENU_SCREENSAVER_ANIMATION,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok    = &setting_action_ok_uint;
            (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_menu_screensaver_animation;
            menu_settings_list_current_add_range(list, list_info, 0, MENU_SCREENSAVER_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type      = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_screensaver_animation_speed,
                  MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
                  MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
                  DEFAULT_MENU_SCREENSAVER_ANIMATION_SPEED,
                  "%.1fx",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.1, 10.0, 0.1, true, true);
         }
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_mouse_enable,
               MENU_ENUM_LABEL_MOUSE_ENABLE,
               MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
               DEFAULT_MOUSE_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_pointer_enable,
               MENU_ENUM_LABEL_POINTER_ENABLE,
               MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
               DEFAULT_POINTER_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_border_filler_enable,
                  MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_background_filler_thickness_enable,
                  MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_border_filler_thickness_enable,
                  MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_full_width_layout,
                  MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
                  rgui_full_width_layout,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_MENU_FRAME_FILTERING))
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.menu_linear_filter,
                     MENU_ENUM_LABEL_MENU_LINEAR_FILTER,
                     MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
                     true,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE
                     );

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.menu_rgui_internal_upscale_level,
                     MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
                     MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
                     rgui_internal_upscale_level,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
                  (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
                  (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_uint_rgui_internal_upscale_level;
               menu_settings_list_current_add_range(list, list_info, 0, RGUI_UPSCALE_LAST-1, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            }

#if !defined(DINGUX)
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_aspect_ratio,
                  MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
                  rgui_aspect,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_rgui_aspect_ratio;
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_ASPECT_RATIO_LAST-1, 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_aspect_ratio_lock,
                  MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
                  rgui_aspect_lock,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_rgui_aspect_ratio_lock;
#if defined(GEKKO)
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_ASPECT_RATIO_LOCK_LAST-3, 1, true, true);
#else
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_ASPECT_RATIO_LOCK_LAST-1, 1, true, true);
#endif
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_color_theme,
                  MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
                  DEFAULT_RGUI_COLOR_THEME,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_with_refresh;
               (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_rgui_menu_color_theme;
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_THEME_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_PATH(
                  list, list_info,
                  settings->paths.path_rgui_theme_preset,
                  sizeof(settings->paths.path_rgui_theme_preset),
                  MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET,
                  MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
                  settings->paths.directory_assets,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "cfg");

            /* ps2 and sdl_dingux/sdl_rs90 gfx drivers do
             * not support menu framebuffer transparency */
            if (!string_is_equal(settings->arrays.video_driver, "ps2") &&
                !string_is_equal(settings->arrays.video_driver, "sdl_dingux") &&
                !string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.menu_rgui_transparency,
                     MENU_ENUM_LABEL_MENU_RGUI_TRANSPARENCY,
                     MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
                     DEFAULT_RGUI_TRANSPARENCY,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_shadows,
                  MENU_ENUM_LABEL_MENU_RGUI_SHADOWS,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
                  rgui_shadows,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_particle_effect,
                  MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
                  rgui_particle_effect,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok    = &setting_action_ok_uint;
               (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_with_refresh;
               (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_rgui_particle_effect;
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_PARTICLE_EFFECT_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_rgui_particle_effect_speed,
                  MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
                  DEFAULT_RGUI_PARTICLE_EFFECT_SPEED,
                  "%.1fx",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.1, 10.0, 0.1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_particle_effect_screensaver,
                  MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
                  DEFAULT_RGUI_PARTICLE_EFFECT_SCREENSAVER,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_extended_ascii,
                  MENU_ENUM_LABEL_MENU_RGUI_EXTENDED_ASCII,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
                  rgui_extended_ascii,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_switch_icons,
                  MENU_ENUM_LABEL_MENU_RGUI_SWITCH_ICONS,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
                  DEFAULT_RGUI_SWITCH_ICONS,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }

#ifdef HAVE_XMB
         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_horizontal_animation,
                  MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,
                  MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
                  DEFAULT_MENU_HORIZONTAL_ANIMATION,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );
#ifdef RARCH_MOBILE
            /* We don't want mobile users being able to switch this off. */
            (*list)[list_info->index - 1].action_left   = NULL;
            (*list)[list_info->index - 1].action_right  = NULL;
            (*list)[list_info->index - 1].action_start  = NULL;
#else
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_animation_horizontal_highlight,
                  MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
                  DEFAULT_XMB_ANIMATION,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_xmb_animation_horizontal_highlight;
            menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_animation_move_up_down,
                  MENU_ENUM_LABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
                  DEFAULT_XMB_ANIMATION,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_xmb_animation_move_up_down;
            menu_settings_list_current_add_range(list, list_info, 0, 1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_animation_opening_main_menu,
                  MENU_ENUM_LABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
                  DEFAULT_XMB_ANIMATION,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_xmb_animation_opening_main_menu;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
         }
#endif

         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_ticker_type,
               MENU_ENUM_LABEL_MENU_TICKER_TYPE,
               MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
               DEFAULT_MENU_TICKER_TYPE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_ticker_type;
         menu_settings_list_current_add_range(list, list_info, 0, TICKER_TYPE_LAST-1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.menu_ticker_speed,
               MENU_ENUM_LABEL_MENU_TICKER_SPEED,
               MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
               menu_ticker_speed,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0.1, 10.0, 0.1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_ticker_smooth,
               MENU_ENUM_LABEL_MENU_TICKER_SMOOTH,
               MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
               DEFAULT_MENU_TICKER_SMOOTH,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Navigation", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_navigation_wraparound_enable,
               MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND,
               MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Settings View", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_advanced_settings,
               MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
               DEFAULT_SHOW_ADVANCED_SETTINGS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         if (string_is_equal(settings->arrays.menu_driver, "xmb") || string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.kiosk_mode_enable,
                  MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,
                  MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
                  DEFAULT_KIOSK_MODE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.kiosk_mode_password,
                  sizeof(settings->paths.kiosk_mode_password),
                  MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD,
                  MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_PASSWORD_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;
         }

#ifdef HAVE_THREADS
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.threaded_data_runloop_enable,
               MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE,
               MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
               DEFAULT_THREADED_DATA_RUNLOOP_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Display", &group_info, &subgroup_info, parent_group);

         /* > MaterialUI, XMB and Ozone all support menu scaling */
         if (string_is_equal(settings->arrays.menu_driver, "glui") ||
             string_is_equal(settings->arrays.menu_driver, "xmb") ||
             string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_scale_factor,
                  MENU_ENUM_LABEL_MENU_SCALE_FACTOR,
                  MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
                  DEFAULT_MENU_SCALE_FACTOR,
                  "%.2fx",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.2, 5.0, 0.01, true, true);
         }

#ifdef HAVE_XMB
         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            /* only XMB uses these values, don't show
             * them on other drivers. */
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_alpha_factor,
                  MENU_ENUM_LABEL_XMB_ALPHA_FACTOR,
                  MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
                  xmb_alpha_factor,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_vertical_fade_factor,
                  MENU_ENUM_LABEL_MENU_XMB_VERTICAL_FADE_FACTOR,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
                  DEFAULT_XMB_VERTICAL_FADE_FACTOR,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok    = &setting_action_ok_uint;
            (*list)[list_info->index - 1].action_left  = &setting_uint_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
            menu_settings_list_current_add_range(list, list_info, 0, 500, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_title_margin,
                  MENU_ENUM_LABEL_MENU_XMB_TITLE_MARGIN,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
                  DEFAULT_XMB_TITLE_MARGIN,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 12, 1, true, true);

            CONFIG_PATH(
                  list, list_info,
                  settings->paths.path_menu_xmb_font,
                  sizeof(settings->paths.path_menu_xmb_font),
                  MENU_ENUM_LABEL_XMB_FONT,
                  MENU_ENUM_LABEL_VALUE_XMB_FONT,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "ttf");
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_FONT_SELECTOR;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_font_color_red,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
                  MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
                  menu_font_color_red,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_font_color_green,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
                  MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
                  menu_font_color_green,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_font_color_blue,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE,
                  MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
                  menu_font_color_blue,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_layout,
                  MENU_ENUM_LABEL_XMB_LAYOUT,
                  MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
                  xmb_menu_layout,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_xmb_layout;
            menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_theme,
                  MENU_ENUM_LABEL_XMB_THEME,
                  MENU_ENUM_LABEL_VALUE_XMB_THEME,
                  xmb_icon_theme,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_xmb_icon_theme;
            menu_settings_list_current_add_range(list, list_info, 0, XMB_ICON_THEME_LAST - 1, 1, true, true);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_xmb_shadows_enable,
                  MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
                  DEFAULT_XMB_SHADOWS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_SHADERPIPELINE
            if (video_shader_any_supported())
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.menu_xmb_shader_pipeline,
                     MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,
                     MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
                     DEFAULT_MENU_SHADER_PIPELINE,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_xmb_shader_pipeline;
               menu_settings_list_current_add_range(list, list_info, 0, XMB_SHADER_PIPELINE_LAST-1, 1, true, true);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            }
#endif
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_color_theme,
                  MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
                  xmb_theme,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_xmb_menu_color_theme;
            menu_settings_list_current_add_range(list, list_info, 0, XMB_THEME_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
       }
#endif
         if (string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_use_preferred_system_color_theme,
                  MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
                  DEFAULT_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;
         }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_load_core,
                  MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
                  menu_show_load_core,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_load_content,
                  MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
                  menu_show_load_content,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#ifdef HAVE_CDROM
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_load_disc,
                  MENU_ENUM_LABEL_MENU_SHOW_LOAD_DISC,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
                  menu_show_load_disc,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_dump_disc,
                  MENU_ENUM_LABEL_MENU_SHOW_DUMP_DISC,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
                  menu_show_dump_disc,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#ifdef HAVE_LAKKA
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_eject_disc,
                  MENU_ENUM_LABEL_MENU_SHOW_EJECT_DISC,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
                  menu_show_eject_disc,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif /* HAVE_LAKKA */
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_information,
                  MENU_ENUM_LABEL_MENU_SHOW_INFORMATION,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
                  menu_show_information,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_configurations,
                  MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
                  menu_show_configurations,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_overlays,
                  MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_VIDEO_LAYOUT
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_video_layout,
                  MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO_LAYOUT,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_latency,
                  MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_rewind,
                  MENU_ENUM_LABEL_CONTENT_SHOW_REWIND,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_help,
                  MENU_ENUM_LABEL_MENU_SHOW_HELP,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
                  menu_show_help,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_LAKKA
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_quit_retroarch,
                  MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
                  menu_show_quit_retroarch,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#else
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_quit_retroarch,
                  MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
                  menu_show_quit_retroarch,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_reboot,
                  MENU_ENUM_LABEL_MENU_SHOW_REBOOT,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
                  menu_show_reboot,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_show_shutdown,
                  MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN,
                  MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
                  menu_show_shutdown,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#else
#if !defined(IOS)
            if (frontend_driver_has_fork())
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.menu_show_restart_retroarch,
                     MENU_ENUM_LABEL_MENU_SHOW_RESTART_RETROARCH,
                     MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
                     menu_show_restart_retroarch,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
#endif
#endif

#if defined(HAVE_XMB) || defined(HAVE_OZONE)
         if (string_is_equal(settings->arrays.menu_driver, "xmb") || string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_settings,
                  MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
                  content_show_settings,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_STRING(
               list, list_info,
               settings->paths.menu_content_show_settings_password,
               sizeof(settings->paths.menu_content_show_settings_password),
               MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
               MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_PASSWORD_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;
         }
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_favorites,
                  MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
                  content_show_favorites,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#ifdef HAVE_IMAGEVIEWER
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_images,
                  MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
                  content_show_images,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_music,
                  MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
                  content_show_music,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_video,
                  MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
                  content_show_video,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_history,
                  MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
                  content_show_history,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#ifdef HAVE_NETWORKING
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_netplay,
                  MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
                  content_show_netplay,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

#if defined(HAVE_XMB) || defined(HAVE_OZONE)
         if (string_is_equal(settings->arrays.menu_driver, "xmb") ||
             string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_add,
                  MENU_ENUM_LABEL_CONTENT_SHOW_ADD,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
                  DEFAULT_MENU_CONTENT_SHOW_ADD,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }
         else
#endif
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_content_show_add_entry,
                  MENU_ENUM_LABEL_CONTENT_SHOW_ADD_ENTRY,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
                  DEFAULT_MENU_CONTENT_SHOW_ADD_ENTRY,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_add_content_entry_display_type;
            menu_settings_list_current_add_range(list, list_info, 0, MENU_ADD_CONTENT_ENTRY_DISPLAY_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_playlists,
                  MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
                  content_show_playlists,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#if defined(HAVE_LIBRETRODB)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_explore,
                  MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
                  DEFAULT_MENU_CONTENT_SHOW_EXPLORE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif
         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_content_show_contentless_cores,
               MENU_ENUM_LABEL_CONTENT_SHOW_CONTENTLESS_CORES,
               MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
               DEFAULT_MENU_CONTENT_SHOW_CONTENTLESS_CORES,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_contentless_cores_display_type;
         menu_settings_list_current_add_range(list, list_info, 0, MENU_CONTENTLESS_CORES_DISPLAY_LAST-1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_MATERIALUI
         if (string_is_equal(settings->arrays.menu_driver, "glui"))
         {
            /* only MaterialUI uses these values, don't show
             * them on other drivers. */
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_icons_enable,
                  MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
                  DEFAULT_MATERIALUI_ICONS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_playlist_icons_enable,
                  MENU_ENUM_LABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
                  DEFAULT_MATERIALUI_PLAYLIST_ICONS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_materialui_color_theme,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
                  DEFAULT_MATERIALUI_THEME,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_menu_color_theme;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_THEME_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_materialui_transition_animation,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
                  DEFAULT_MATERIALUI_TRANSITION_ANIM,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_menu_transition_animation;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_TRANSITION_ANIM_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_materialui_landscape_layout_optimization,
                  MENU_ENUM_LABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
                  DEFAULT_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_landscape_layout_optimization;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_show_nav_bar,
                  MENU_ENUM_LABEL_MATERIALUI_SHOW_NAV_BAR,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
                  DEFAULT_MATERIALUI_SHOW_NAV_BAR,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_auto_rotate_nav_bar,
                  MENU_ENUM_LABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
                  DEFAULT_MATERIALUI_AUTO_ROTATE_NAV_BAR,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_materialui_thumbnail_view_portrait,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
                  DEFAULT_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_menu_thumbnail_view_portrait;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_materialui_thumbnail_view_landscape,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
                  DEFAULT_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_menu_thumbnail_view_landscape;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_dual_thumbnail_list_view_enable,
                  MENU_ENUM_LABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
                  DEFAULT_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_materialui_thumbnail_background_enable,
                  MENU_ENUM_LABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
                  DEFAULT_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            /* TODO: These should be removed entirely, but just
             * comment out for now in case users complain...
            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_header_opacity,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
                  menu_header_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.menu_footer_opacity,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
                  menu_footer_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            (*list)[list_info->index - 1].ui_type
                                  = ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX;
            */
         }
#endif

#ifdef HAVE_OZONE
         if (string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_ozone_color_theme,
                  MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
                  DEFAULT_OZONE_COLOR_THEME,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_ozone_menu_color_theme;
            menu_settings_list_current_add_range(list, list_info, 0, OZONE_COLOR_THEME_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ozone_collapse_sidebar,
                  MENU_ENUM_LABEL_OZONE_COLLAPSE_SIDEBAR,
                  MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
                  DEFAULT_OZONE_COLLAPSE_SIDEBAR,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ozone_scroll_content_metadata,
                  MENU_ENUM_LABEL_OZONE_SCROLL_CONTENT_METADATA,
                  MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
                  DEFAULT_OZONE_SCROLL_CONTENT_METADATA,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.ozone_thumbnail_scale_factor,
                  MENU_ENUM_LABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
                  MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
                  DEFAULT_OZONE_THUMBNAIL_SCALE_FACTOR,
                  "%.2fx",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 1.0, 2.0, 0.05, true, true);
         }
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_start_screen,
               MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN,
               MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
               DEFAULT_MENU_SHOW_START_SCREEN,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_inline_thumbnails,
                  MENU_ENUM_LABEL_MENU_RGUI_INLINE_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
                  rgui_inline_thumbnails,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_rgui_swap_thumbnails,
                  MENU_ENUM_LABEL_MENU_RGUI_SWAP_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
                  rgui_swap_thumbnails,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb") ||
             string_is_equal(settings->arrays.menu_driver, "ozone") ||
             string_is_equal(settings->arrays.menu_driver, "rgui") ||
             string_is_equal(settings->arrays.menu_driver, "glui"))
         {
            enum msg_hash_enums thumbnails_label_value;
            enum msg_hash_enums left_thumbnails_label_value;

            if (string_is_equal(settings->arrays.menu_driver, "rgui"))
            {
               thumbnails_label_value      = MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI;
               left_thumbnails_label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI;
            }
            else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
            {
               thumbnails_label_value      = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
               left_thumbnails_label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE;
            }
            else if (string_is_equal(settings->arrays.menu_driver, "glui"))
            {
               thumbnails_label_value      = MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI;
               left_thumbnails_label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI;
            }
            else
            {
               thumbnails_label_value      = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
               left_thumbnails_label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS;
            }

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.gfx_thumbnails,
                  MENU_ENUM_LABEL_THUMBNAILS,
                  thumbnails_label_value,
                  gfx_thumbnails_default,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_thumbnails;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_left_thumbnails,
                  MENU_ENUM_LABEL_LEFT_THUMBNAILS,
                  left_thumbnails_label_value,
                  menu_left_thumbnails_default,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_left_thumbnails;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_xmb_vertical_thumbnails,
                  MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
                  xmb_vertical_thumbnails,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_thumbnail_scale_factor,
                  MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
                  MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
                  xmb_thumbnail_scale_factor,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 30;
            menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index - 1].offset_by, 100, 1, true, true);
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb") ||
             string_is_equal(settings->arrays.menu_driver, "ozone") ||
             string_is_equal(settings->arrays.menu_driver, "glui"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.gfx_thumbnail_upscale_threshold,
                  MENU_ENUM_LABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
                  MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
                  gfx_thumbnail_upscale_threshold,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint_special;
            menu_settings_list_current_add_range(list, list_info, 0, 1024, 256, true, true);
         }

         if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_thumbnail_downscaler,
                  MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
                  rgui_thumbnail_downscaler,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_rgui_thumbnail_scaler;
            menu_settings_list_current_add_range(list, list_info, 0, RGUI_THUMB_SCALE_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_rgui_thumbnail_delay,
                  MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DELAY,
                  MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
                  rgui_thumbnail_delay,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0.0f, 1024.0f, 64.0f, true, true);
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_timedate_enable,
               MENU_ENUM_LABEL_TIMEDATE_ENABLE,
               MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
               DEFAULT_MENU_TIMEDATE_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         CONFIG_UINT(list, list_info,
            &settings->uints.menu_timedate_style,
            MENU_ENUM_LABEL_TIMEDATE_STYLE,
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
            DEFAULT_MENU_TIMEDATE_STYLE,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_menu_timedate_style;
         menu_settings_list_current_add_range(list, list_info, 0, MENU_TIMEDATE_STYLE_LAST - 1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

         CONFIG_UINT(list, list_info,
            &settings->uints.menu_timedate_date_separator,
            MENU_ENUM_LABEL_TIMEDATE_DATE_SEPARATOR,
            MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
            DEFAULT_MENU_TIMEDATE_DATE_SEPARATOR,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_menu_timedate_date_separator;
         menu_settings_list_current_add_range(list, list_info, 0, MENU_TIMEDATE_DATE_SEPARATOR_LAST - 1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_battery_level_enable,
               MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE,
               MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_core_enable,
               MENU_ENUM_LABEL_CORE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_sublabels,
               MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
               menu_show_sublabels,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.use_last_start_directory,
               MENU_ENUM_LABEL_USE_LAST_START_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MENU_FILE_BROWSER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_navigation_browser_filter_supported_extensions_enable,
               MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
               MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MULTIMEDIA:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.multimedia_builtin_mediaplayer_enable,
               MENU_ENUM_LABEL_USE_BUILTIN_PLAYER,
               MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
               DEFAULT_BUILTIN_MEDIAPLAYER_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_IMAGEVIEWER
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.multimedia_builtin_imageviewer_enable,
               MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER,
               MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
               DEFAULT_BUILTIN_IMAGEVIEWER_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.filter_by_current_core,
               MENU_ENUM_LABEL_FILTER_BY_CURRENT_CORE,
               MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
               DEFAULT_FILTER_BY_CURRENT_CORE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_POWER_MANAGEMENT:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#ifdef ANDROID
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.sustained_performance_mode,
               MENU_ENUM_LABEL_SUSTAINED_PERFORMANCE_MODE,
               MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
               sustained_performance_mode,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
#endif

#ifdef HAVE_LAKKA
#ifndef HAVE_LAKKA_SWITCH
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CPU_PERFPOWER,
               MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
               &group_info,
               &subgroup_info,
               parent_group);
#endif
#endif

         if (frontend_driver_has_gamemode())
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.gamemode_enable,
                  MENU_ENUM_LABEL_GAMEMODE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
                  DEFAULT_GAMEMODE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_WIFI_MANAGEMENT:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_WIFI_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.wifi_enabled,
               MENU_ENUM_LABEL_WIFI_ENABLED,
               MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
               DEFAULT_WIFI_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_WIFI_NETWORK_SCAN,
               MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_WIFI_DISCONNECT,
               MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
               &group_info,
               &subgroup_info,
               parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_ACCESSIBILITY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.accessibility_enable,
               MENU_ENUM_LABEL_ACCESSIBILITY_ENABLED,
               MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
               DEFAULT_ACCESSIBILITY_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.accessibility_narrator_speech_speed,
               MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
               MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
               DEFAULT_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 1, 10, 1, true, true);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_AI_SERVICE:
#ifdef HAVE_TRANSLATE
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_AI_SERVICE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.ai_service_mode,
               MENU_ENUM_LABEL_AI_SERVICE_MODE,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
               DEFAULT_AI_SERVICE_MODE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_ai_service_mode;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);

         CONFIG_STRING(
               list, list_info,
               settings->arrays.ai_service_url,
               sizeof(settings->arrays.ai_service_url),
               MENU_ENUM_LABEL_AI_SERVICE_URL,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
               DEFAULT_AI_SERVICE_URL,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ai_service_enable,
               MENU_ENUM_LABEL_AI_SERVICE_ENABLE,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
               DEFAULT_AI_SERVICE_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ai_service_pause,
               MENU_ENUM_LABEL_AI_SERVICE_PAUSE,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
               DEFAULT_AI_SERVICE_PAUSE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.ai_service_source_lang,
               MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
               DEFAULT_AI_SERVICE_SOURCE_LANG,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_ai_service_lang;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, TRANSLATION_LANG_DONT_CARE, (TRANSLATION_LANG_LAST-1), 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.ai_service_target_lang,
               MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG,
               MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
               DEFAULT_AI_SERVICE_TARGET_LANG,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_ai_service_lang;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, TRANSLATION_LANG_DONT_CARE, (TRANSLATION_LANG_LAST-1), 1, true, true);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_USER_INTERFACE:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.pause_nonactive,
               MENU_ENUM_LABEL_PAUSE_NONACTIVE,
               MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
               DEFAULT_PAUSE_NONACTIVE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#if !defined(RARCH_MOBILE)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_disable_composition,
               MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION,
               MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
               DEFAULT_DISABLE_COMPOSITION,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

#if defined(_3DS)
         {
            u8 device_model = 0xFF;

            /* Only O3DS and O3DSXL support running in 'dual-framebuffer'
             * mode with the parallax barrier disabled
             * (i.e. these are the only platforms that can use
             * CTR_VIDEO_MODE_2D_400X240 and CTR_VIDEO_MODE_2D_800X240) */
            CFGU_GetSystemModel(&device_model); /* (0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL) */

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_3ds_display_mode,
                  MENU_ENUM_LABEL_VIDEO_3DS_DISPLAY_MODE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
                  video_3ds_display_mode,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_video_3ds_display_mode;
            menu_settings_list_current_add_range(list, list_info, 0,
                  CTR_VIDEO_MODE_LAST - (((device_model == 0) || (device_model == 1)) ? 1 : 3),
                  1, true, true);
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.new3ds_speedup_enable,
               MENU_ENUM_LABEL_NEW3DS_SPEEDUP_ENABLE,
               MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
               new3ds_speedup_enable,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         (*list)[list_info->index - 1].change_handler = new3ds_speedup_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_3ds_lcd_bottom,
               MENU_ENUM_LABEL_VIDEO_3DS_LCD_BOTTOM,
               MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
               video_3ds_lcd_bottom,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
#ifdef CONSOLE_LOG
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT_FROM_TOGGLE);
#endif
#endif

#ifdef HAVE_NETWORKING
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_online_updater,
               MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
               menu_show_online_updater,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#if !defined(HAVE_LAKKA)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_core_updater,
               MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
               menu_show_online_updater,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_legacy_thumbnail_updater,
               MENU_ENUM_LABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
               menu_show_legacy_thumbnail_updater,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

#ifdef HAVE_MIST
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_core_manager_steam,
               MENU_ENUM_LABEL_MENU_SHOW_CORE_MANAGER_STEAM,
               MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
               menu_show_core_manager_steam,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_drivers,
               MENU_ENUM_LABEL_SETTINGS_SHOW_DRIVERS,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
               DEFAULT_SETTINGS_SHOW_DRIVERS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_video,
               MENU_ENUM_LABEL_SETTINGS_SHOW_VIDEO,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
               DEFAULT_SETTINGS_SHOW_VIDEO,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_audio,
               MENU_ENUM_LABEL_SETTINGS_SHOW_AUDIO,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
               DEFAULT_SETTINGS_SHOW_AUDIO,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_input,
               MENU_ENUM_LABEL_SETTINGS_SHOW_INPUT,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
               DEFAULT_SETTINGS_SHOW_INPUT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_latency,
               MENU_ENUM_LABEL_SETTINGS_SHOW_LATENCY,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
               DEFAULT_SETTINGS_SHOW_LATENCY,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_core,
               MENU_ENUM_LABEL_SETTINGS_SHOW_CORE,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
               DEFAULT_SETTINGS_SHOW_CORE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_configuration,
               MENU_ENUM_LABEL_SETTINGS_SHOW_CONFIGURATION,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
               DEFAULT_SETTINGS_SHOW_CONFIGURATION,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_saving,
               MENU_ENUM_LABEL_SETTINGS_SHOW_SAVING,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
               DEFAULT_SETTINGS_SHOW_SAVING,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_logging,
               MENU_ENUM_LABEL_SETTINGS_SHOW_LOGGING,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
               DEFAULT_SETTINGS_SHOW_LOGGING,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_file_browser,
               MENU_ENUM_LABEL_SETTINGS_SHOW_FILE_BROWSER,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
               DEFAULT_SETTINGS_SHOW_FILE_BROWSER,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_frame_throttle,
               MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
               DEFAULT_SETTINGS_SHOW_FRAME_THROTTLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_recording,
               MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
               DEFAULT_SETTINGS_SHOW_RECORDING,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_onscreen_display,
               MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
               DEFAULT_SETTINGS_SHOW_ONSCREEN_DISPLAY,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_user_interface,
               MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
               DEFAULT_SETTINGS_SHOW_USER_INTERFACE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_ai_service,
               MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
               DEFAULT_SETTINGS_SHOW_AI_SERVICE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_accessibility,
               MENU_ENUM_LABEL_SETTINGS_SHOW_ACCESSIBILITY,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
               DEFAULT_SETTINGS_SHOW_ACCESSIBILITY,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_power_management,
               MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
               DEFAULT_SETTINGS_SHOW_POWER_MANAGEMENT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_achievements,
               MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
               DEFAULT_SETTINGS_SHOW_ACHIEVEMENTS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_network,
               MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
               DEFAULT_SETTINGS_SHOW_NETWORK,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_playlists,
               MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
               DEFAULT_SETTINGS_SHOW_PLAYLISTS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_user,
               MENU_ENUM_LABEL_SETTINGS_SHOW_USER,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
               DEFAULT_SETTINGS_SHOW_USER,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.settings_show_directory,
               MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
               DEFAULT_SETTINGS_SHOW_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_take_screenshot,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               DEFAULT_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_save_load_state,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
               DEFAULT_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_undo_save_load_state,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
               DEFAULT_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_add_to_favorites,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
               quick_menu_show_add_to_favorites,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_start_recording,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
               quick_menu_show_start_recording,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_start_streaming,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
               quick_menu_show_start_streaming,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_set_core_association,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
               quick_menu_show_set_core_association,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_reset_core_association,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
               quick_menu_show_reset_core_association,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#if 0
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_recording,
               MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
               quick_menu_show_recording,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_streaming,
               MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
               quick_menu_show_streaming,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_resume_content,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
               DEFAULT_QUICK_MENU_SHOW_RESUME_CONTENT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_restart_content,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
               DEFAULT_QUICK_MENU_SHOW_RESTART_CONTENT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_close_content,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
               DEFAULT_QUICK_MENU_SHOW_CLOSE_CONTENT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_options,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
               quick_menu_show_options,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_core_options_flush,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
               DEFAULT_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_controls,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
               quick_menu_show_controls,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_cheats,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
               quick_menu_show_cheats,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         if (video_shader_any_supported())
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.quick_menu_show_shaders,
                  MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS,
                  MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
                  quick_menu_show_shaders,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_save_core_overrides,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
               quick_menu_show_save_core_overrides,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_save_game_overrides,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
               quick_menu_show_save_game_overrides,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_information,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
               quick_menu_show_information,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

#ifdef HAVE_NETWORKING
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_download_thumbnails,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
               quick_menu_show_download_thumbnails,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_scroll_fast,
               MENU_ENUM_LABEL_MENU_SCROLL_FAST,
               MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
               menu_scroll_fast,
               MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
               MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_scroll_delay,
               MENU_ENUM_LABEL_MENU_SCROLL_DELAY,
               MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
               DEFAULT_MENU_SCROLL_DELAY,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 999, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ui_companion_enable,
               MENU_ENUM_LABEL_UI_COMPANION_ENABLE,
               MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
               ui_companion_enable,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ui_companion_start_on_boot,
               MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT,
               MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
               ui_companion_start_on_boot,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ui_menubar_enable,
               MENU_ENUM_LABEL_UI_MENUBAR_ENABLE,
               MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
               DEFAULT_UI_MENUBAR_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

#ifdef HAVE_QT
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.desktop_menu_enable,
               MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE,
               MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
               DEFAULT_DESKTOP_MENU_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.ui_companion_toggle,
               MENU_ENUM_LABEL_UI_COMPANION_TOGGLE,
               MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
               ui_companion_toggle,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_PLAYLIST:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_SETTINGS_BEGIN),
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "History", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.history_list_enable,
               MENU_ENUM_LABEL_HISTORY_LIST_ENABLE,
               MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
               DEFAULT_HISTORY_LIST_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.content_history_size,
               MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE,
               MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
               default_content_history_size,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = 1;
         menu_settings_list_current_add_range(list, list_info, 1.0f, 9999.0f, 1.0f, true, true);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Playlist", &group_info, &subgroup_info, parent_group);

         /* Favourites size is traditionally associtated with
          * history size, but they are in fact unrelated. We
          * therefore place this entry outside the "History"
          * sub group. */
         CONFIG_INT(
               list, list_info,
               &settings->ints.content_favorites_size,
               MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE,
               MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
               default_content_favorites_size,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = -1;
         menu_settings_list_current_add_range(list, list_info, -1.0f, 9999.0f, 1.0f, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_entry_rename,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
               DEFAULT_PLAYLIST_ENTRY_RENAME,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.playlist_entry_remove_enable,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
               DEFAULT_PLAYLIST_ENTRY_REMOVE_ENABLE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_playlist_entry_remove_enable;
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_ENTRY_REMOVE_ENABLE_LAST-1, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_sort_alphabetical,
               MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
               DEFAULT_PLAYLIST_SORT_ALPHABETICAL,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_use_old_format,
               MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
               DEFAULT_PLAYLIST_USE_OLD_FORMAT,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

#if defined(HAVE_ZLIB)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_compression,
               MENU_ENUM_LABEL_PLAYLIST_COMPRESSION,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
               DEFAULT_PLAYLIST_COMPRESSION,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_show_sublabels,
               MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
               DEFAULT_PLAYLIST_SHOW_SUBLABELS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         /* Playlist entry index display and content specific history icon
          * are currently supported only by Ozone & XMB */
         if (string_is_equal(settings->arrays.menu_driver, "xmb") ||
             string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.playlist_show_history_icons,
                  MENU_ENUM_LABEL_PLAYLIST_SHOW_HISTORY_ICONS,
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
                  DEFAULT_PLAYLIST_SHOW_HISTORY_ICONS,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_playlist_show_history_icons;
            menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_SHOW_HISTORY_ICONS_LAST-1, 1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.playlist_show_entry_idx,
                  MENU_ENUM_LABEL_PLAYLIST_SHOW_ENTRY_IDX,
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
                  DEFAULT_PLAYLIST_SHOW_ENTRY_IDX,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
         }

         CONFIG_UINT(
               list, list_info,
               &settings->uints.playlist_sublabel_runtime_type,
               MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
               DEFAULT_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_playlist_sublabel_runtime_type;
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_RUNTIME_LAST-1, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.playlist_sublabel_last_played_style,
               MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
               DEFAULT_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_playlist_sublabel_last_played_style;
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_LAST_PLAYED_STYLE_LAST-1, 1, true, true);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.playlist_show_inline_core_name,
               MENU_ENUM_LABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
               DEFAULT_PLAYLIST_SHOW_INLINE_CORE_NAME,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_playlist_inline_core_display_type;
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_INLINE_CORE_DISPLAY_LAST-1, 1, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_fuzzy_archive_match,
               MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
               DEFAULT_PLAYLIST_FUZZY_ARCHIVE_MATCH,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_portable_paths,
               MENU_ENUM_LABEL_PLAYLIST_PORTABLE_PATHS,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
               DEFAULT_PLAYLIST_PORTABLE_PATHS,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
            );

#ifdef HAVE_OZONE
         if (string_is_equal(settings->arrays.menu_driver, "ozone"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ozone_truncate_playlist_name,
                  MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
                  MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
                  DEFAULT_OZONE_TRUNCATE_PLAYLIST_NAME,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ozone_sort_after_truncate_playlist_name,
                  MENU_ENUM_LABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
                  MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
                  DEFAULT_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_CHEEVOS:
#ifdef HAVE_CHEEVOS
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_enable,
               MENU_ENUM_LABEL_CHEEVOS_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
               DEFAULT_CHEEVOS_ENABLE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               achievement_hardcore_mode_write_handler, /* re-evaluate whether hardcore is enabled */
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].action_ok     = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left   = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right  = setting_bool_action_right_with_refresh;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_test_unofficial,
               MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->arrays.cheevos_leaderboards_enable,
               sizeof(settings->arrays.cheevos_leaderboards_enable),
               MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
               "true",
               "false|true",
               &group_info,
               &subgroup_info,
               parent_group,
               achievement_leaderboards_enabled_write_handler,
               general_read_handler);
#if defined(HAVE_GFX_WIDGETS)
         (*list)[list_info->index - 1].values = "false|true|trackers|notifications";
         (*list)[list_info->index - 1].action_ok = setting_action_ok_mapped_string;
#else
         (*list)[list_info->index - 1].action_ok = setting_string_action_left_string_options;
#endif
         (*list)[list_info->index - 1].action_left = setting_string_action_left_string_options;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_string_options;
         (*list)[list_info->index - 1].get_string_representation = achievement_leaderboards_get_string_representation;
         (*list)[list_info->index - 1].free_flags &= ~SD_FREE_FLAG_VALUES;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_challenge_indicators,
               MENU_ENUM_LABEL_CHEEVOS_CHALLENGE_INDICATORS,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_richpresence_enable,
               MENU_ENUM_LABEL_CHEEVOS_RICHPRESENCE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

#ifndef HAVE_GFX_WIDGETS
         if (string_is_equal(settings->arrays.menu_driver, "xmb") || string_is_equal(settings->arrays.menu_driver, "ozone"))
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.cheevos_badges_enable,
                  MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE,
                  MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );
#endif

#ifdef HAVE_AUDIOMIXER
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_unlock_sound_enable,
               MENU_ENUM_LABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
            );
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_verbose_enable,
               MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_auto_screenshot,
               MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_start_active,
               MENU_ENUM_LABEL_CHEEVOS_START_ACTIVE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_hardcore_mode_enable,
               MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               achievement_hardcore_mode_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_CORE_UPDATER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);
#ifdef HAVE_NETWORKING

#if defined(ANDROID)
         /* Play Store builds do not fetch cores
          * from the buildbot */
         if (!play_feature_delivery_enabled())
#endif
         {

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.network_buildbot_url,
                  sizeof(settings->paths.network_buildbot_url),
                  MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL,
                  MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
                  DEFAULT_BUILDBOT_SERVER_URL,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);

            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;
         }

         CONFIG_STRING(
               list, list_info,
               settings->paths.network_buildbot_assets_url,
               sizeof(settings->paths.network_buildbot_assets_url),
               MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL,
               MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
               DEFAULT_BUILDBOT_ASSETS_SERVER_URL,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.network_buildbot_auto_extract_archive,
               MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
               MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
               DEFAULT_NETWORK_BUILDBOT_AUTO_EXTRACT_ARCHIVE,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.network_buildbot_show_experimental_cores,
               MENU_ENUM_LABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
               MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
               DEFAULT_NETWORK_BUILDBOT_SHOW_EXPERIMENTAL_CORES,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

#if defined(ANDROID)
         /* Play Store builds do not support automatic
          * core backups */
         if (!play_feature_delivery_enabled())
#endif
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.core_updater_auto_backup,
                  MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP,
                  MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
                  DEFAULT_CORE_UPDATER_AUTO_BACKUP,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.core_updater_auto_backup_history_size,
                     MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
                     MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
                     DEFAULT_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].offset_by = 1;
               menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index - 1].offset_by, 500, 1, true, true);
         }
#endif
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_NETPLAY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS);

         START_SUB_GROUP(list, list_info, "Netplay", &group_info, &subgroup_info, parent_group);

         {
#if defined(HAVE_NETWORKING)
            unsigned user;
            char dev_req_label[64];
            char dev_req_value[64];

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_show_only_connectable,
                  MENU_ENUM_LABEL_NETPLAY_SHOW_ONLY_CONNECTABLE,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
                  DEFAULT_NETPLAY_SHOW_ONLY_CONNECTABLE,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_public_announce,
                  MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_use_mitm_server,
                  MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
                  netplay_use_mitm_server,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_STRING(
                  list, list_info,
                  settings->arrays.netplay_mitm_server,
                  sizeof(settings->arrays.netplay_mitm_server),
                  MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
                  DEFAULT_NETPLAY_MITM_SERVER,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
         (*list)[list_info->index - 1].action_start = setting_generic_action_start_default;
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_netplay_mitm_server;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_netplay_mitm_server;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_netplay_mitm_server;

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.netplay_custom_mitm_server,
                  sizeof(settings->paths.netplay_custom_mitm_server),
                  MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.netplay_server,
                  sizeof(settings->paths.netplay_server),
                  MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_port,
                  MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
                  RARCH_DEFAULT_PORT,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 65535, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_max_connections,
                  MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
                  netplay_max_connections,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 31, 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_max_ping,
                  MENU_ENUM_LABEL_NETPLAY_MAX_PING,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
                  netplay_max_ping,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type = ST_UI_TYPE_UINT_SPINBOX;
            menu_settings_list_current_add_range(list, list_info, 0, 500, 10, true, true);

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.netplay_password,
                  sizeof(settings->paths.netplay_password),
                  MENU_ENUM_LABEL_NETPLAY_PASSWORD,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_PASSWORD_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_STRING(
                  list, list_info,
                  settings->paths.netplay_spectate_password,
                  sizeof(settings->paths.netplay_spectate_password),
                  MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
            (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_PASSWORD_LINE_EDIT;
            (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_start_as_spectator,
                  MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_fade_chat,
                  MENU_ENUM_LABEL_NETPLAY_FADE_CHAT,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
                  netplay_fade_chat,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_allow_pausing,
                  MENU_ENUM_LABEL_NETPLAY_ALLOW_PAUSING,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
                  netplay_allow_pausing,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_allow_slaves,
                  MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_require_slaves,
                  MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_stateless_mode,
                  MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_INT(
                  list, list_info,
                  &settings->ints.netplay_check_frames,
                  MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
                  netplay_check_frames,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
            menu_settings_list_current_add_range(list, list_info, -600, 600, 1, false, false);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_INT(
                  list, list_info,
                  (int *) &settings->uints.netplay_input_latency_frames_min,
                  MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);

            CONFIG_INT(
                  list, list_info,
                  (int *) &settings->uints.netplay_input_latency_frames_range,
                  MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.netplay_nat_traversal,
                  MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_share_digital,
                  MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
                  netplay_share_digital,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_netplay_share_digital;
            menu_settings_list_current_add_range(list, list_info, 0, RARCH_NETPLAY_SHARE_DIGITAL_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_share_analog,
                  MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
                  netplay_share_analog,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_netplay_share_analog;
            menu_settings_list_current_add_range(list, list_info, 0, RARCH_NETPLAY_SHARE_ANALOG_LAST-1, 1, true, true);
            (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;

            for (user = 0; user < MAX_USERS; user++)
            {
               snprintf(dev_req_label, sizeof(dev_req_label),
                     msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_I), user + 1);
               snprintf(dev_req_value, sizeof(dev_req_value),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I), user + 1);
               CONFIG_BOOL_ALT(
                     list, list_info,
                     &settings->bools.netplay_request_devices[user],
                     strdup(dev_req_label),
                     strdup(dev_req_value),
                     false,
                     MENU_ENUM_LABEL_VALUE_NO,
                     MENU_ENUM_LABEL_VALUE_YES,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_ADVANCED);
               SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
               MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + user));
            }

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(
                  list,
                  list_info,
                  "Miscellaneous",
                  &group_info,
                  &subgroup_info,
                  parent_group);

#if defined(HAVE_NETWORK_CMD)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.network_cmd_enable,
                  MENU_ENUM_LABEL_NETWORK_CMD_ENABLE,
                  MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
                  network_cmd_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            (*list)[list_info->index - 1].action_ok     = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_left   = &setting_bool_action_left_with_refresh;
            (*list)[list_info->index - 1].action_right  = &setting_bool_action_right_with_refresh;

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.network_cmd_port,
                  MENU_ENUM_LABEL_NETWORK_CMD_PORT,
                  MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
                  network_cmd_port,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  NULL,
                  NULL);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 99999, 1, true, true);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.network_remote_enable,
                  MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.network_remote_base_port,
                  MENU_ENUM_LABEL_NETWORK_REMOTE_PORT,
                  MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
                  network_remote_base_port,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  NULL,
                  NULL);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, 99999, 1, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
            /* TODO/FIXME - add enum_idx */

            {
               unsigned max_users        = settings->uints.input_max_users;
               for (user = 0; user < max_users; user++)
               {
                  char s1[64], s2[64];

                  snprintf(s1, sizeof(s1), "%s_user_p%d", msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE), user + 1);
                  snprintf(s2, sizeof(s2), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE), user + 1);

                  CONFIG_BOOL_ALT(
                        list, list_info,
                        &settings->bools.network_remote_enable_user[user],
                        /* todo: figure out this value, it's working fine but I don't think this is correct */
                        strdup(s1),
                        strdup(s2),
                        false,
                        MENU_ENUM_LABEL_VALUE_OFF,
                        MENU_ENUM_LABEL_VALUE_ON,
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler,
                        SD_FLAG_ADVANCED);
                  SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
                  MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user));
               }
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.stdin_cmd_enable,
                  MENU_ENUM_LABEL_STDIN_CMD_ENABLE,
                  MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
                  stdin_cmd_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
#endif
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.network_on_demand_thumbnails,
                  MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
                  DEFAULT_NETWORK_ON_DEMAND_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif
         }
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_LAKKA_SERVICES:
         {
#if defined(HAVE_LAKKA)
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
                  &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.ssh_enable,
                  MENU_ENUM_LABEL_SSH_ENABLE,
                  MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = ssh_enable_toggle_change_handler;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.samba_enable,
                  MENU_ENUM_LABEL_SAMBA_ENABLE,
                  MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = samba_enable_toggle_change_handler;

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.bluetooth_enable,
                  MENU_ENUM_LABEL_BLUETOOTH_ENABLE,
                  MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = bluetooth_enable_toggle_change_handler;

#ifdef HAVE_WIFI
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.localap_enable,
                  MENU_ENUM_LABEL_LOCALAP_ENABLE,
                  MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = localap_enable_toggle_change_handler;
#endif

            CONFIG_STRING_OPTIONS(
                  list, list_info,
                  settings->arrays.timezone,
                  sizeof(settings->arrays.timezone),
                  MENU_ENUM_LABEL_TIMEZONE,
                  MENU_ENUM_LABEL_VALUE_TIMEZONE,
                  DEFAULT_TIMEZONE,
                  config_get_all_timezones(),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_read_handler,
                  general_write_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
            (*list)[list_info->index - 1].action_ok      = setting_action_ok_mapped_string;
            (*list)[list_info->index - 1].change_handler = timezone_change_handler;

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
#endif
         }
         break;
      case SETTINGS_LIST_USER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_USER_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCOUNTS_LIST,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_STRING(
               list, list_info,
               settings->paths.username,
               sizeof(settings->paths.username),
               MENU_ENUM_LABEL_NETPLAY_NICKNAME,
               MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_STRING(
               list, list_info,
               settings->paths.browse_url,
               sizeof(settings->paths.browse_url),
               MENU_ENUM_LABEL_BROWSE_URL,
               MENU_ENUM_LABEL_VALUE_BROWSE_URL,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

#ifdef HAVE_LANGEXTRA
         CONFIG_UINT(
               list, list_info,
               msg_hash_get_uint(MSG_HASH_USER_LANGUAGE),
               MENU_ENUM_LABEL_USER_LANGUAGE,
               MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
               DEFAULT_USER_LANGUAGE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               RETRO_LANGUAGE_LAST-1,
               1,
               true,
               true);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].action_left = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_user_language;
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_UINT_COMBOBOX;
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#ifdef HAVE_CHEEVOS
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
               &group_info,
               &subgroup_info,
               parent_group);
#endif

#ifdef HAVE_NETWORKING
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCOUNTS_TWITCH,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ACCOUNTS_FACEBOOK,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK,
               &group_info,
               &subgroup_info,
               parent_group);         
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_STRING(
               list, list_info,
               settings->arrays.youtube_stream_key,
               sizeof(settings->arrays.youtube_stream_key),
               MENU_ENUM_LABEL_YOUTUBE_STREAM_KEY,
               MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               update_streaming_url_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS_TWITCH:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_STRING(
               list, list_info,
               settings->arrays.twitch_stream_key,
               sizeof(settings->arrays.twitch_stream_key),
               MENU_ENUM_LABEL_TWITCH_STREAM_KEY,
               MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               update_streaming_url_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_STRING(
               list, list_info,
               settings->arrays.facebook_stream_key,
               sizeof(settings->arrays.facebook_stream_key),
               MENU_ENUM_LABEL_FACEBOOK_STREAM_KEY,
               MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               update_streaming_url_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;         
      case SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#ifdef HAVE_CHEEVOS
         CONFIG_STRING(
               list, list_info,
               settings->arrays.cheevos_username,
               sizeof(settings->arrays.cheevos_username),
               MENU_ENUM_LABEL_CHEEVOS_USERNAME,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_STRING(
               list, list_info,
               settings->arrays.cheevos_password,
               sizeof(settings->arrays.cheevos_password),
               MENU_ENUM_LABEL_CHEEVOS_PASSWORD,
               MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_cheevos_password;
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_PASSWORD_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DIRECTORY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS),
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_system,
               sizeof(settings->paths.directory_system),
               MENU_ENUM_LABEL_SYSTEM_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_SYSTEM],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_core_assets,
               sizeof(settings->paths.directory_core_assets),
               MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_assets,
               sizeof(settings->paths.directory_assets),
               MENU_ENUM_LABEL_ASSETS_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_ASSETS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_dynamic_wallpapers,
               sizeof(settings->paths.directory_dynamic_wallpapers),
               MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_WALLPAPERS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_thumbnails,
               sizeof(settings->paths.directory_thumbnails),
               MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_THUMBNAILS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_menu_content,
               sizeof(settings->paths.directory_menu_content),
               MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_menu_config,
               sizeof(settings->paths.directory_menu_config),
               MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_libretro,
               sizeof(settings->paths.directory_libretro),
               MENU_ENUM_LABEL_LIBRETRO_DIR_PATH,
               MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
               g_defaults.dirs[DEFAULT_DIR_CORE],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_CORE_INFO_INIT);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.path_libretro_info,
               sizeof(settings->paths.path_libretro_info),
               MENU_ENUM_LABEL_LIBRETRO_INFO_PATH,
               MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
               g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_CORE_INFO_INIT);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

#ifdef HAVE_LIBRETRODB
         CONFIG_DIR(
               list, list_info,
               settings->paths.path_content_database,
               sizeof(settings->paths.path_content_database),
               MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_DATABASE],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_cursor,
               sizeof(settings->paths.directory_cursor),
               MENU_ENUM_LABEL_CURSOR_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CURSOR],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;
#endif

         CONFIG_DIR(
               list, list_info,
               settings->paths.path_cheat_database,
               sizeof(settings->paths.path_cheat_database),
               MENU_ENUM_LABEL_CHEAT_DATABASE_PATH,
               MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
               g_defaults.dirs[DEFAULT_DIR_CHEATS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_video_filter,
               sizeof(settings->paths.directory_video_filter),
               MENU_ENUM_LABEL_VIDEO_FILTER_DIR,
               MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
               g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_audio_filter,
               sizeof(settings->paths.directory_audio_filter),
               MENU_ENUM_LABEL_AUDIO_FILTER_DIR,
               MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
               g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_video_shader,
               sizeof(settings->paths.directory_video_shader),
               MENU_ENUM_LABEL_VIDEO_SHADER_DIR,
               MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
               g_defaults.dirs[DEFAULT_DIR_SHADER],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;
#endif

         if (string_is_not_equal(settings->arrays.record_driver, "null"))
         {
            CONFIG_DIR(
                  list, list_info,
                  recording_st->output_dir,
                  sizeof(recording_st->output_dir),
                  MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,
                  MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
                  g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT],
                  MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_start = directory_action_start_generic;

            CONFIG_DIR(
                  list, list_info,
                  recording_st->config_dir,
                  sizeof(recording_st->config_dir),
                  MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY,
                  MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
                  g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG],
                  MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_start = directory_action_start_generic;
         }
#ifdef HAVE_OVERLAY
         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_overlay,
               sizeof(settings->paths.directory_overlay),
               MENU_ENUM_LABEL_OVERLAY_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_OVERLAY],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;
#endif

#ifdef HAVE_VIDEO_LAYOUT
         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_video_layout,
               sizeof(settings->paths.directory_video_layout),
               MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;
#endif

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_screenshot,
               sizeof(settings->paths.directory_screenshot),
               MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_autoconfig,
               sizeof(settings->paths.directory_autoconfig),
               MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR,
               MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
               g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_input_remapping,
               sizeof(settings->paths.directory_input_remapping),
               MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_REMAP],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
            (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_playlist,
               sizeof(settings->paths.directory_playlist),
               MENU_ENUM_LABEL_PLAYLIST_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_content_favorites,
               sizeof(settings->paths.directory_content_favorites),
               MENU_ENUM_LABEL_CONTENT_FAVORITES_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CONTENT_FAVORITES],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_content_history,
               sizeof(settings->paths.directory_content_history),
               MENU_ENUM_LABEL_CONTENT_HISTORY_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_content_image_history,
               sizeof(settings->paths.directory_content_image_history),
               MENU_ENUM_LABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CONTENT_IMAGE_HISTORY],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_content_music_history,
               sizeof(settings->paths.directory_content_music_history),
               MENU_ENUM_LABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CONTENT_MUSIC_HISTORY],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_content_video_history,
               sizeof(settings->paths.directory_content_video_history),
               MENU_ENUM_LABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CONTENT_VIDEO_HISTORY],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_runtime_log,
               sizeof(settings->paths.directory_runtime_log),
               MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
               "",
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               dir_get_ptr(RARCH_DIR_SAVEFILE),
               dir_get_size(RARCH_DIR_SAVEFILE),
               MENU_ENUM_LABEL_SAVEFILE_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_SRAM],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               dir_get_ptr(RARCH_DIR_SAVESTATE),
               dir_get_size(RARCH_DIR_SAVESTATE),
               MENU_ENUM_LABEL_SAVESTATE_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.directory_cache,
               sizeof(settings->paths.directory_cache),
               MENU_ENUM_LABEL_CACHE_DIRECTORY,
               MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
               g_defaults.dirs[DEFAULT_DIR_CACHE],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         CONFIG_DIR(
               list, list_info,
               settings->paths.log_dir,
               sizeof(settings->paths.log_dir),
               MENU_ENUM_LABEL_LOG_DIR,
               MENU_ENUM_LABEL_VALUE_LOG_DIR,
               g_defaults.dirs[DEFAULT_DIR_LOGS],
               MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start = directory_action_start_generic;

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_PRIVACY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State",
               &group_info, &subgroup_info, parent_group);

         if (string_is_not_equal(settings->arrays.camera_driver, "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.camera_allow,
                  MENU_ENUM_LABEL_CAMERA_ALLOW,
                  MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }

#ifdef HAVE_DISCORD
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.discord_enable,
               MENU_ENUM_LABEL_DISCORD_ALLOW,
               MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
#endif

         if (string_is_not_equal(settings->arrays.location_driver, "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.location_allow,
                  MENU_ENUM_LABEL_LOCATION_ALLOW,
                  MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
                  false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MIDI:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MIDI_SETTINGS);

         START_SUB_GROUP(list, list_info, "State",
               &group_info, &subgroup_info, parent_group);

         CONFIG_STRING(
               list, list_info,
               settings->arrays.midi_input,
               sizeof(settings->arrays.midi_input),
               MENU_ENUM_LABEL_MIDI_INPUT,
               MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
               DEFAULT_MIDI_INPUT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].action_start = setting_generic_action_start_default;
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_midi_input;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_midi_input;

         CONFIG_STRING(
               list, list_info,
               settings->arrays.midi_output,
               sizeof(settings->arrays.midi_output),
               MENU_ENUM_LABEL_MIDI_OUTPUT,
               MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
               DEFAULT_MIDI_OUTPUT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].action_start = setting_generic_action_start_default;
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_midi_output;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_midi_output;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.midi_volume,
               MENU_ENUM_LABEL_MIDI_VOLUME,
               MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
               midi_volume,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0.0f, 100.0f, 1.0f, true, true);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MANUAL_CONTENT_SCAN:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST);

         START_SUB_GROUP(list, list_info, "State",
               &group_info, &subgroup_info, parent_group);

         CONFIG_STRING(
               list, list_info,
               manual_content_scan_get_system_name_custom_ptr(),
               manual_content_scan_get_system_name_custom_size(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_STRING(
               list, list_info,
               manual_content_scan_get_file_exts_custom_ptr(),
               manual_content_scan_get_file_exts_custom_size(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
         (*list)[list_info->index - 1].action_start  = setting_generic_action_start_default;

         CONFIG_BOOL(
               list, list_info,
               manual_content_scan_get_search_recursively_ptr(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               manual_content_scan_get_search_archives_ptr(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_PATH(
               list, list_info,
               manual_content_scan_get_dat_file_path_ptr(),
               manual_content_scan_get_dat_file_path_size(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "dat|xml");

         CONFIG_BOOL(
               list, list_info,
               manual_content_scan_get_filter_dat_content_ptr(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         CONFIG_BOOL(
               list, list_info,
               manual_content_scan_get_overwrite_playlist_ptr(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         (*list)[list_info->index - 1].action_ok    = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_left  = setting_bool_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = setting_bool_action_right_with_refresh;

         CONFIG_BOOL(
               list, list_info,
               manual_content_scan_get_validate_entries_ptr(),
               MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_NONE:
      default:
         break;
   }

   return true;
}

void menu_setting_free(rarch_setting_t *setting)
{
   unsigned values, n;
   rarch_setting_t **list = NULL;

   if (!setting)
      return;

   list                   = (rarch_setting_t**)&setting;

   /* Free data which was previously tagged */
   for (; setting->type != ST_NONE; (*list = *list + 1))
      for (values = (unsigned)setting->free_flags, n = 0; values != 0; values >>= 1, n++)
         if (values & 1)
            switch (1 << n)
            {
               case SD_FREE_FLAG_VALUES:
                  if (setting->values)
                     free((void*)setting->values);
                  setting->values = NULL;
                  break;
               case SD_FREE_FLAG_NAME:
                  if (setting->name)
                     free((void*)setting->name);
                  setting->name = NULL;
                  break;
               case SD_FREE_FLAG_SHORT:
                  if (setting->short_description)
                     free((void*)setting->short_description);
                  setting->short_description = NULL;
                  break;
               default:
                  break;
            }
}

#define MENU_SETTING_INITIALIZE(list, _pos) \
{ \
   unsigned pos                                   = _pos; \
   (*&list)[pos].ui_type                          = ST_UI_TYPE_NONE; \
   (*&list)[pos].browser_selection_type           = ST_NONE; \
   (*&list)[pos].enum_idx                         = MSG_UNKNOWN; \
   (*&list)[pos].enum_value_idx                   = MSG_UNKNOWN; \
   (*&list)[pos].type                             = ST_NONE; \
   (*&list)[pos].dont_use_enum_idx_representation = false; \
   (*&list)[pos].enforce_minrange                 = false; \
   (*&list)[pos].enforce_maxrange                 = false; \
   (*&list)[pos].index                            = 0; \
   (*&list)[pos].index_offset                     = 0; \
   (*&list)[pos].offset_by                        = 0; \
   (*&list)[pos].bind_type                        = 0; \
   (*&list)[pos].size                             = 0; \
   (*&list)[pos].step                             = 0.0f; \
   (*&list)[pos].flags                            = 0; \
   (*&list)[pos].free_flags                       = 0; \
   (*&list)[pos].min                              = 0.0; \
   (*&list)[pos].max                              = 0.0; \
   (*&list)[pos].rounding_fraction                = NULL; \
   (*&list)[pos].name                             = NULL; \
   (*&list)[pos].short_description                = NULL; \
   (*&list)[pos].group                            = NULL; \
   (*&list)[pos].subgroup                         = NULL; \
   (*&list)[pos].parent_group                     = NULL; \
   (*&list)[pos].values                           = NULL; \
   (*&list)[pos].change_handler                   = NULL; \
   (*&list)[pos].read_handler                     = NULL; \
   (*&list)[pos].action_start                     = NULL; \
   (*&list)[pos].action_left                      = NULL; \
   (*&list)[pos].action_right                     = NULL; \
   (*&list)[pos].action_up                        = NULL; \
   (*&list)[pos].action_down                      = NULL; \
   (*&list)[pos].action_cancel                    = NULL; \
   (*&list)[pos].action_ok                        = NULL; \
   (*&list)[pos].action_select                    = NULL; \
   (*&list)[pos].get_string_representation        = NULL; \
   (*&list)[pos].default_value.fraction           = 0.0f; \
   (*&list)[pos].value.target.fraction            = NULL; \
   (*&list)[pos].original_value.fraction          = 0.0f; \
   (*&list)[pos].dir.empty_path                   = NULL; \
   (*&list)[pos].cmd_trigger_idx                  = CMD_EVENT_NONE; \
   (*&list)[pos].cmd_trigger_event_triggered      = false; \
   (*&list)[pos].boolean.off_label                = NULL; \
   (*&list)[pos].boolean.on_label                 = NULL; \
}

static rarch_setting_t *menu_setting_new_internal(rarch_setting_info_t *list_info)
{
   unsigned i;
   rarch_setting_t* resized_list        = NULL;
   enum settings_list_type list_types[] =
   {
      SETTINGS_LIST_MAIN_MENU,
      SETTINGS_LIST_DRIVERS,
      SETTINGS_LIST_CORE,
      SETTINGS_LIST_CONFIGURATION,
      SETTINGS_LIST_LOGGING,
      SETTINGS_LIST_SAVING,
      SETTINGS_LIST_REWIND,
      SETTINGS_LIST_CHEAT_DETAILS,
      SETTINGS_LIST_CHEAT_SEARCH,
      SETTINGS_LIST_CHEATS,
      SETTINGS_LIST_VIDEO,
      SETTINGS_LIST_CRT_SWITCHRES,
      SETTINGS_LIST_AUDIO,
      SETTINGS_LIST_INPUT,
      SETTINGS_LIST_INPUT_TURBO_FIRE,
      SETTINGS_LIST_INPUT_HOTKEY,
      SETTINGS_LIST_RECORDING,
      SETTINGS_LIST_FRAME_THROTTLING,
      SETTINGS_LIST_FRAME_TIME_COUNTER,
      SETTINGS_LIST_ONSCREEN_NOTIFICATIONS,
      SETTINGS_LIST_OVERLAY,
#ifdef HAVE_VIDEO_LAYOUT
      SETTINGS_LIST_VIDEO_LAYOUT,
#endif
      SETTINGS_LIST_MENU,
      SETTINGS_LIST_MENU_FILE_BROWSER,
      SETTINGS_LIST_MULTIMEDIA,
#ifdef HAVE_TRANSLATE
      SETTINGS_LIST_AI_SERVICE,
#endif
      SETTINGS_LIST_ACCESSIBILITY,
      SETTINGS_LIST_USER_INTERFACE,
      SETTINGS_LIST_POWER_MANAGEMENT,
      SETTINGS_LIST_WIFI_MANAGEMENT,
      SETTINGS_LIST_MENU_SOUNDS,
      SETTINGS_LIST_PLAYLIST,
      SETTINGS_LIST_CHEEVOS,
      SETTINGS_LIST_CORE_UPDATER,
      SETTINGS_LIST_NETPLAY,
      SETTINGS_LIST_LAKKA_SERVICES,
      SETTINGS_LIST_USER,
      SETTINGS_LIST_USER_ACCOUNTS,
      SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
      SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE,
      SETTINGS_LIST_USER_ACCOUNTS_TWITCH,
      SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK,
      SETTINGS_LIST_DIRECTORY,
      SETTINGS_LIST_PRIVACY,
      SETTINGS_LIST_MIDI,
      SETTINGS_LIST_MANUAL_CONTENT_SCAN
   };
   settings_t *settings                 = config_get_ptr();
   global_t   *global                   = global_get_ptr();
   const char *root                     = NULL;
   rarch_setting_t **list_ptr           = NULL;
   rarch_setting_t *list                = (rarch_setting_t*)
      malloc(list_info->size * sizeof(*list));

   if (!list)
      return NULL;

   root                                 = 
      msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);

   for (i = 0; i < (unsigned)list_info->size; i++)
   {
      MENU_SETTING_INITIALIZE(list, i);
   }

   for (i = 0; i < ARRAY_SIZE(list_types); i++)
   {
      if (!setting_append_list(
               settings, global,
               list_types[i], &list, list_info, root))
      {
         free(list);
         return NULL;
      }
   }

   list_ptr = &list;

   if (!SETTINGS_LIST_APPEND(list_ptr, list_info))
   {
      free(list);
      return NULL;
   }

   MENU_SETTING_INITIALIZE(list, list_info->index);
   list_info->index++;

   /* flatten this array to save ourselves some kilobytes. */
   resized_list = (rarch_setting_t*)realloc(list,
         list_info->index * sizeof(rarch_setting_t));
   if (!resized_list)
   {
      free(list);
      return NULL;
   }

   list = resized_list;

   return list;
}

/**
 * menu_setting_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
rarch_setting_t *menu_setting_new(void)
{
   rarch_setting_t* list           = NULL;
   rarch_setting_info_t *list_info = (rarch_setting_info_t*)
      malloc(sizeof(*list_info));

   if (!list_info)
      return NULL;

   list_info->index = 0;
   list_info->size  = 32;

   list             = menu_setting_new_internal(list_info);

   if (list_info)
      free(list_info);

   list_info        = NULL;

   return list;
}

void video_driver_menu_settings(void **list_data, void *list_info_data,
      void *group_data, void *subgroup_data, const char *parent_group)
{
#ifdef HAVE_MENU
   rarch_setting_t **list                    = (rarch_setting_t**)list_data;
   rarch_setting_info_t *list_info           = (rarch_setting_info_t*)list_info_data;
   rarch_setting_group_info_t *group_info    = (rarch_setting_group_info_t*)group_data;
   rarch_setting_group_info_t *subgroup_info = (rarch_setting_group_info_t*)subgroup_data;
   global_t                        *global   = global_get_ptr();

   (void)list;
   (void)list_info;
   (void)group_info;
   (void)subgroup_info;
   (void)global;

#if defined(GEKKO) || defined(_XBOX360)
   CONFIG_UINT(
         list, list_info,
         &global->console.screen.gamma_correction,
         MENU_ENUM_LABEL_VIDEO_GAMMA,
         MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
         0,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
         list,
         list_info,
         CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
   menu_settings_list_current_add_range(
         list,
         list_info,
         0,
         MAX_GAMMA_SETTING,
         1,
         true,
         true);
   SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info,
         SD_FLAG_CMD_APPLY_AUTO|SD_FLAG_ADVANCED);
#endif
#if defined(_XBOX1) || defined(HW_RVL)
   CONFIG_BOOL(
         list, list_info,
         &global->console.softfilter_enable,
         MENU_ENUM_LABEL_VIDEO_SOFT_FILTER,
         MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
         false,
         MENU_ENUM_LABEL_VALUE_OFF,
         MENU_ENUM_LABEL_VALUE_ON,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler,
         SD_FLAG_NONE);
   MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
         list,
         list_info,
         CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
#endif
#ifdef _XBOX1
   CONFIG_UINT(
         list, list_info,
         &global->console.screen.flicker_filter_index,
         MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER,
         MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
         0,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   menu_settings_list_current_add_range(list, list_info,
         0, 5, 1, true, true);
#endif
#endif
}
