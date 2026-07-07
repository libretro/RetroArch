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
#include <stdlib.h>
#include <string.h>

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

#include "menu_setting.h"
#include "menu_displaylist.h"
#include "menu_cbs.h"
#include "menu_driver.h"
#include "menu_input.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu_shader.h"
#endif

#if defined(HAVE_STEAM) && defined(HAVE_MIST)
#include <mist.h>
#endif

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#ifdef HAVE_WASAPI
#include "../audio/common/wasapi.h"
#endif

#include "../config.def.h"
#include "../config.def.keybinds.h"

#if !defined(__PSL1GHT__) && defined(__PS3__)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#include "../cheevos/cheevos_locals.h"
#endif

#ifdef HAVE_TRANSLATE
#include "../translation_defines.h"
#endif

#include "../frontend/frontend_driver.h"

#include "../camera/camera_driver.h"
#include "../gfx/gfx_animation.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#include "../core.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../defaults.h"
#include "../driver.h"
#include "../paths.h"
#include "../dynamic.h"
#include "../list_special.h"
#include "../msg_hash_lbl_str.h"
#include "../audio/audio_driver.h"
#ifdef HAVE_MICROPHONE
#include "../audio/microphone_driver.h"
#endif
#ifdef HAVE_BLUETOOTH
#include "../bluetooth/bluetooth_driver.h"
#endif
#include "../midi_driver.h"
#include "../location_driver.h"
#include "../network/cloud_sync_driver.h"
#include "../record/record_driver.h"
#include "../tasks/tasks_internal.h"
#include "../accessibility.h"
#include "../config.def.h"
#include "../ui/ui_companion_driver.h"
#include "../performance_counters.h"
#include "../setting_list.h"
#include "../lakka.h"
#ifdef HAVE_LAKKA_SWITCH
#include "../lakka-switch.h"
#endif
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

#if defined(HAVE_OVERLAY)
#include "../input/input_overlay.h"
#endif

/* Required for 3DS display mode setting */
#if defined(_3DS)
#include <3ds.h>
#include <3ds/services/cfgu.h>
#include "../gfx/common/ctr_defines.h"
#endif

#if defined(DINGUX)
#include "../dingux/dingux_utils.h"
#endif

#if defined(ANDROID)
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#ifdef HAVE_LANGEXTRA
#include "../intl/progress.h"
#endif

#ifdef HAVE_SMBCLIENT
#include "../libretro-common/vfs/vfs_implementation_smb.h"
#endif

/* Forward declaration for Win32 native menubar rebuild.
 * Defined in ui/drivers/ui_win32.c. Declared here rather than included
 * via ui/drivers/ui_win32.h to avoid dragging <windows.h> and friends
 * into menu_setting.c. */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && defined(HAVE_MENU)
void win32_menubar_rebuild(void);
#endif

#define _3_SECONDS  3000000
#define _6_SECONDS  6000000
#define _9_SECONDS  9000000
#define _12_SECONDS 12000000
#define _15_SECONDS 15000000
#define _18_SECONDS 18000000
#define _21_SECONDS 21000000

#define CONFIG_SIZE(a, b, c, d, e, f, g, h, i, j, k, l) \
   config_size(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_BOOL_ALT(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
   config_bool_alt(a, b, c, d, e, f, g, h, i, j, k, l, m, n)

#define CONFIG_BOOL(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
   config_bool(a, b, c, d, e, f, g, h, i, j, k, l, m, n)

#define CONFIG_INT(a, b, c, d, e, f, g, h, i, j, k) \
   config_int(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_UINT_ALT(a, b, c, d, e, f, g, h, i, j, k) \
   config_uint_alt(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_UINT(a, b, c, d, e, f, g, h, i, j, k) \
   config_uint(a, b, c, d, e, f, g, h, i, j, k)

#define CONFIG_STRING(a, b, c, d, e, f, g, h, i, j, k, l) \
   config_string(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_STRING_ALT(a, b, c, d, e, f, g, h, i, j, k, l) \
   config_string_alt(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_FLOAT(a, b, c, d, e, f, g, h, i, j, k, l) \
   config_float(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_DIR(a, b, c, d, e, f, g, h, i, j, k, l, m) \
   config_dir(a, b, c, d, e, f, g, h, i, j, k, l, m)

#define CONFIG_PATH(a, b, c, d, e, f, g, h, i, j, k, l) \
   config_path(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_ACTION_ALT(a, b, c, d, e, f, g) \
   config_action_alt(a, b, c, d, e, f, g)

#define CONFIG_ACTION(a, b, c, d, e, f, g) \
   config_action(a, b, c, d, e, f, g)

#define END_GROUP(a, b, c) \
   end_group(a, b, c)

#define START_SUB_GROUP(a, b, c, d, e, f) \
   start_sub_group(a, b, c, d, e, f)

#define END_SUB_GROUP(a, b, c) \
   end_sub_group(a, b, c)

/* Group scaffolding appears in one canonical shape across the
 * builders: open a named group, repoint parent_group to a second
 * enum, open the "State" sub-group; and at the end, close both.
 * Two macros carry the shape so the builders show intent, not
 * boilerplate.  The value enum names the group; the parent enum is
 * passed explicitly because it is not always the value enum's twin
 * (achievements, updater and the accounts groups reparent). */
#define GROUP_STATE(value_enum, parent_enum) \
   START_GROUP(list, list_info, &group_info, \
         msg_hash_to_str(value_enum), parent_group); \
   parent_group = msg_hash_to_str(parent_enum); \
   START_SUB_GROUP(list, list_info, "State", \
         &group_info, &subgroup_info, parent_group)
#define GROUP_END() \
   END_SUB_GROUP(list, list_info, parent_group); \
   END_GROUP(list, list_info, parent_group)

#define CONFIG_STRING_OPTIONS(a, b, c, d, e, f, g, h, i, j, k, l, m) \
   config_string_options(a, b, c, d, e, f, g, h, i, j, k, l, m)

#define CONFIG_HEX(a, b, c, d, e, f, g, h, i, j, k, l) \
   if (SETTINGS_LIST_APPEND(a, b)) \
      config_hex(a, b, c, d, e, f, g, h, i, j, k, l)

#define CONFIG_BIND_ALT(a, b, c, d, e, f, g, h, i, j, k) \
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

/* Action-tuple interning: entries share pointers into this pool
 * instead of carrying five handlers each. Content-addressed, so
 * repeated builds reuse blocks; sized over three times the 150
 * full combinations measured live, failing loud if a config ever
 * outgrows it. */
static const setting_actions_t settings_actions_none;
static setting_actions_t settings_actions_pool[512];
static unsigned settings_actions_pool_n;

static const setting_actions_t *settings_actions_intern(
      const setting_actions_t *t)
{
   unsigned i;
   for (i = 0; i < settings_actions_pool_n; i++)
      if (!memcmp(&settings_actions_pool[i], t, sizeof(*t)))
         return &settings_actions_pool[i];
   /* A configuration outgrowing the pool is a build-time defect;
    * fail loud rather than deref past the array. */
   if (settings_actions_pool_n >= ARRAY_SIZE(settings_actions_pool))
   {
      RARCH_ERR("[Settings] Action tuple pool exhausted.\n");
      return &settings_actions_pool[0];
   }
   settings_actions_pool[settings_actions_pool_n] = *t;
   return &settings_actions_pool[settings_actions_pool_n++];
}

#define SETTINGS_ACTION_SET(field, s, fn) \
{ \
   setting_actions_t _t = *((s)->actions); \
   _t.field             = (fn); \
   (s)->actions         = settings_actions_intern(&_t); \
}


#define MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, str) ((*(list))[MENU_SETTINGS_LIST_CURRENT_IDX((list_info))].values = (str))

#define MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, str) (*(list))[MENU_SETTINGS_LIST_CURRENT_IDX(list_info)].cmd_trigger_idx = (str)

#define CONFIG_UINT_CBS(var, label, a_left, a_right, msg_enum_base, string_rep, min, max, step) \
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
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], a_left) \
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], a_right) \
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], string_rep) \
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
   SETTINGS_LIST_CLOUD_SYNC,
   SETTINGS_LIST_REWIND,
   SETTINGS_LIST_CHEAT_DETAILS,
   SETTINGS_LIST_CHEAT_SEARCH,
   SETTINGS_LIST_CHEATS,
   SETTINGS_LIST_VIDEO,
   SETTINGS_LIST_CRT_SWITCHRES,
   SETTINGS_LIST_AUDIO,
#ifdef HAVE_MICROPHONE
   SETTINGS_LIST_MICROPHONE,
#endif
   SETTINGS_LIST_INPUT,
   SETTINGS_LIST_INPUT_TURBO_FIRE,
   SETTINGS_LIST_INPUT_HOTKEY,
   SETTINGS_LIST_INPUT_RETROPAD_BINDS,
   SETTINGS_LIST_RECORDING,
   SETTINGS_LIST_FRAME_THROTTLING,
   SETTINGS_LIST_FRAME_TIME_COUNTER,
   SETTINGS_LIST_ONSCREEN_NOTIFICATIONS,
   SETTINGS_LIST_OVERLAY,
   SETTINGS_LIST_OSK_OVERLAY,
   SETTINGS_LIST_OVERLAY_MOUSE,
   SETTINGS_LIST_OVERLAY_LIGHTGUN,
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
   SETTINGS_LIST_CHEEVOS_APPEARANCE,
   SETTINGS_LIST_CHEEVOS_VISIBILITY,
   SETTINGS_LIST_CORE_UPDATER,
   SETTINGS_LIST_NETPLAY,
   SETTINGS_LIST_LAKKA_SERVICES,
#ifdef HAVE_LAKKA_SWITCH
   SETTINGS_LIST_LAKKA_SWITCH_OPTIONS,
#endif
   SETTINGS_LIST_USER,
   SETTINGS_LIST_USER_ACCOUNTS,
   SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
   SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE,
   SETTINGS_LIST_USER_ACCOUNTS_TWITCH,
   SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK,
   SETTINGS_LIST_USER_ACCOUNTS_KICK,
   SETTINGS_LIST_DIRECTORY,
   SETTINGS_LIST_PRIVACY,
   SETTINGS_LIST_MIDI,
#ifdef HAVE_MIST
   SETTINGS_LIST_STEAM,
#endif
#ifdef HAVE_SMBCLIENT
   SETTINGS_LIST_SMBCLIENT,
#endif
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

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
static int setting_set_with_string_representation(rarch_setting_t* setting,
      const char* value)
{
   switch (setting->type)
   {
      case ST_INT:
         {
            char *ptr;
            uint32_t flags                 = setting->flags;
            *setting->value.target.integer = (int)strtol(value, &ptr, 10);
            if (flags & SD_FLAG_HAS_RANGE)
            {
               float min   = setting->min;
               float max   = setting->max;
               if (flags & SD_FLAG_ENFORCE_MINRANGE && *setting->value.target.integer < min)
                  *setting->value.target.integer = min;
               if (flags & SD_FLAG_ENFORCE_MAXRANGE && *setting->value.target.integer > max)
               {
                  settings_t *settings = config_get_ptr();
                  if (settings && settings->bools.menu_navigation_wraparound_enable)
                     *setting->value.target.integer = min;
                  else
                     *setting->value.target.integer = max;
               }
            }
         }
         break;
      case ST_UINT:
         {
            char *ptr;
            uint32_t flags = setting->flags;
            *setting->value.target.unsigned_integer = (unsigned int)strtoul(value, &ptr, 0);
            if (flags & SD_FLAG_HAS_RANGE)
            {
               float min   = setting->min;
               float max   = setting->max;
               if (flags & SD_FLAG_ENFORCE_MINRANGE && *setting->value.target.unsigned_integer < min)
                  *setting->value.target.unsigned_integer = min;
               if (flags & SD_FLAG_ENFORCE_MAXRANGE && *setting->value.target.unsigned_integer > max)
               {
                  settings_t *settings = config_get_ptr();
                  if (settings && settings->bools.menu_navigation_wraparound_enable)
                     *setting->value.target.unsigned_integer = min;
                  else
                     *setting->value.target.unsigned_integer = max;
               }
            }
         }
         break;
      case ST_SIZE:
         {
            uint32_t flags = setting->flags;
            char *end;
            unsigned long long parsed = strtoull(value, &end, 0);
            if (end != value && *end == '\0')
               *setting->value.target.sizet = (size_t)parsed;
            if (flags & SD_FLAG_HAS_RANGE)
            {
               float min = setting->min;
               float max = setting->max;
               if (flags & SD_FLAG_ENFORCE_MINRANGE && *setting->value.target.sizet < (size_t)min)
                  *setting->value.target.sizet = (size_t)min;
               if (flags & SD_FLAG_ENFORCE_MAXRANGE && *setting->value.target.sizet > (size_t)max)
               {
                  settings_t *settings = config_get_ptr();
                  if (settings && settings->bools.menu_navigation_wraparound_enable)
                     *setting->value.target.sizet = (size_t)min;
                  else
                     *setting->value.target.sizet = (size_t)max;
               }
            }
         }
         break;
      case ST_FLOAT:
         {
            char *ptr;
            uint32_t flags = setting->flags;
            /* strtof() is C99/POSIX. Just use the more portable kind. */
            *setting->value.target.fraction = (float)strtod(value, &ptr);
            if (flags & SD_FLAG_HAS_RANGE)
            {
               float min   = setting->min;
               float max   = setting->max;
               if (flags & SD_FLAG_ENFORCE_MINRANGE && *setting->value.target.fraction < min)
                  *setting->value.target.fraction = min;
               if (flags & SD_FLAG_ENFORCE_MAXRANGE && *setting->value.target.fraction > max)
               {
                  settings_t *settings = config_get_ptr();
                  if (settings && settings->bools.menu_navigation_wraparound_enable)
                     *setting->value.target.fraction = min;
                  else
                     *setting->value.target.fraction = max;
               }
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
         if (memcmp(value, "true", 5) == 0)
            *setting->value.target.boolean = true;
         else if (memcmp(value, "false", 6) == 0)
            *setting->value.target.boolean = false;
         break;
      default:
         break;
   }

   if (setting->actions->change)
      setting->actions->change(setting);

   return 0;
}

static void menu_input_st_uint_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char *ptr = str;
      /* Reject negative numbers */
      while (*ptr == ' ')
         ptr++;
      if (*ptr >= '0' && *ptr <= '9')
      {
         char *end            = NULL;
         unsigned long value  = strtoul(str, &end, 0);
         /* Ensure entire string was consumed and value fits in unsigned */
         if (end && *end == '\0' && value <= UINT_MAX)
         {
            struct menu_state *menu_st  = menu_state_get_ptr();
            const char *label           = menu_st->input_dialog_kb_label_setting;
            if (label && *label)
            {
               rarch_setting_t *setting = NULL;
               if ((setting = menu_setting_find(label)))
                  setting_set_with_string_representation(setting, str);
            }
         }
      }
   }
   menu_input_dialog_end();
}

static void menu_input_st_int_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char *ptr = str;

      if (*ptr >= '0' && *ptr <= '9')
      {
         while (*ptr >= '0' && *ptr <= '9')
            ptr++;

         /* Skip trailing whitespace */
         while (*ptr == ' ' || *ptr == '\t')
            ptr++;

         if (*ptr == '\0')
         {
            struct menu_state *menu_st  = menu_state_get_ptr();
            const char *label           = menu_st->input_dialog_kb_label_setting;
            if (label && *label)
            {
               rarch_setting_t *setting = NULL;
               if ((setting = menu_setting_find(label)))
                  setting_set_with_string_representation(setting, str);
            }
         }
      }
   }
   menu_input_dialog_end();
}

static void menu_input_st_float_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      char *end = NULL;
      (void)strtod(str, &end);
      if (end != str && *end == '\0')
      {
         struct menu_state *menu_st  = menu_state_get_ptr();
         const char *label           = menu_st->input_dialog_kb_label_setting;
         if (label && *label)
         {
            rarch_setting_t *setting = NULL;
            if ((setting = menu_setting_find(label)))
               setting_set_with_string_representation(setting, str);
         }
      }
   }
   menu_input_dialog_end();
}

static void menu_input_st_string_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      struct menu_state *menu_st  = menu_state_get_ptr();
      const char *label           = menu_st->input_dialog_kb_label_setting;

      if (label && *label)
      {
         rarch_setting_t *setting = NULL;
         if ((setting = menu_setting_find(label)))
         {
            if (setting->value.target.string)
               strlcpy(setting->value.target.string, str, setting->size);
            if (setting->actions->change)
               setting->actions->change(setting);
            menu_setting_generic(setting, 0, false);
         }
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

static int setting_string_action_start_generic(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   setting->value.target.string[0] = '\0';

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
      SETTINGS_ACTION_SET(ok, &(*list)[idx], setting_generic_action_ok_linefeed)
      SETTINGS_ACTION_SET(sel, &(*list)[idx], setting_generic_action_ok_linefeed)

      switch ((*list)[idx].type)
      {
         case ST_STRING:
            SETTINGS_ACTION_SET(start, &(*list)[idx], setting_string_action_start_generic)
            /* fall-through */
         case ST_SIZE:
         case ST_UINT:
         case ST_INT:
         case ST_FLOAT:
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

static int setting_bind_action_ok(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_SINGLE, setting))
      return -1;
   return 0;
}

static int setting_int_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   *setting->value.target.integer =
      *setting->value.target.integer + setting->step;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      float max = setting->max;
      if (*setting->value.target.integer > max)
      {
         settings_t *settings = config_get_ptr();
         float          min   = setting->min;

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

   if (!(keybind = (struct retro_keybind*)setting->value.target.keybind))
      return -1;

   keybind->joykey  = NO_BTN;
   keybind->joyaxis = AXIS_NONE;

   /* Clear old mapping bit */
   input_keyboard_mapping_bits(0, keybind->key);

   if (setting->index_offset)
      def_binds     = (struct retro_keybind*)retro_keybinds_rest;

   bind_type        = setting->bind_type;

   keybind->key     = def_binds[bind_type - MENU_SETTINGS_BIND_BEGIN].key;
   keybind->mbutton = def_binds[bind_type - MENU_SETTINGS_BIND_BEGIN].mbutton;

   /* Store new mapping bit */
   input_keyboard_mapping_bits(1, keybind->key);

   return 0;
}

static size_t setting_get_string_representation_hex_and_uint(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%u (%08X)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer);
   return 0;
}

static size_t setting_get_string_representation_uint(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
   return 0;
}

#if defined(HAVE_NETWORKING)
static size_t setting_get_string_representation_color_rgb(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "#%06X",
         *setting->value.target.unsigned_integer & 0xFFFFFF);
   return 0;
}
#endif

static size_t setting_get_string_representation_size_in_mb(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%" PRI_SIZET,
            (*setting->value.target.sizet) / (1024 * 1024));
   return 0;
}

#ifdef HAVE_CHEATS
static size_t setting_get_string_representation_uint_as_enum(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s,
            msg_hash_to_str((enum msg_hash_enums)(
               setting->index_offset+(
                  *setting->value.target.unsigned_integer))), len);
   return 0;
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

static int setting_uint_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool                 overflowed = false;
   float                step       = 0.0f;

   if (!setting)
      return -1;

   step = recalc_step_based_on_length_of_action(setting);

   if (step > *setting->value.target.unsigned_integer)
      overflowed = true;
   else
      *setting->value.target.unsigned_integer =
         *setting->value.target.unsigned_integer - step;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      float min = setting->min;
      if (overflowed || *setting->value.target.unsigned_integer < min)
      {
         settings_t *settings = config_get_ptr();

         if (settings &&
             settings->bools.menu_navigation_wraparound_enable)
         {
            float max = setting->max;
            *setting->value.target.unsigned_integer = max;
         }
         else
            *setting->value.target.unsigned_integer = min;
      }
   }

   return 0;
}

static int setting_uint_action_right_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   float                step = 0.0f;

   if (!setting)
      return -1;

   step                                    =
      recalc_step_based_on_length_of_action(setting);

   *setting->value.target.unsigned_integer =
      *setting->value.target.unsigned_integer + step;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      float max = setting->max;
      if (*setting->value.target.unsigned_integer > max)
      {
         settings_t *settings = config_get_ptr();
         float           min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.unsigned_integer = min;
         else
            *setting->value.target.unsigned_integer = max;
      }
   }

   return 0;
}

static int setting_bool_action_right_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   if (*setting->value.target.boolean)
      *setting->value.target.boolean = false;
   else
      *setting->value.target.boolean = true;
   if (setting->actions->change)
      setting->actions->change(setting);
   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int setting_uint_action_right_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int retval                 = setting_uint_action_right_default(setting, idx, wraparound);
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return retval;
}

int setting_bool_action_left_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   if (*setting->value.target.boolean)
      *setting->value.target.boolean = false;
   else
      *setting->value.target.boolean = true;
   if (setting->actions->change)
      setting->actions->change(setting);
   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

int setting_uint_action_left_with_refresh(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   int retval                 = setting_uint_action_left_default(
         setting, idx, wraparound);
   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return retval;
}

static int setting_size_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   bool                 overflowed = false;
   float                step       = 0.0f;

   if (!setting)
      return -1;

   step       = recalc_step_based_on_length_of_action(setting);

   if (!(overflowed = step > *setting->value.target.sizet))
      *setting->value.target.sizet = *setting->value.target.sizet - step;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      float min = setting->min;
      if (overflowed || *setting->value.target.sizet < min)
      {
         settings_t *settings = config_get_ptr();
         float           max  = setting->max;

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
   float                step = 0.0f;

   if (!setting)
      return -1;

   step = recalc_step_based_on_length_of_action(setting);

   *setting->value.target.sizet =
      *setting->value.target.sizet + step;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      float max = setting->max;
      if (*setting->value.target.sizet > max)
      {
         settings_t *settings = config_get_ptr();
         float           min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.sizet = min;
         else
            *setting->value.target.sizet = max;
      }
   }

   return 0;
}

static int setting_generic_action_ok_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   if (setting->cmd_trigger_idx != CMD_EVENT_NONE)
      setting->flags |= SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED;

   return 0;
}

void setting_generic_handle_change(rarch_setting_t *setting)
{
   settings_t *settings  = config_get_ptr();
   settings->flags      |= SETTINGS_FLG_MODIFIED;

   if (setting->actions->change)
      setting->actions->change(setting);

   if (       setting->cmd_trigger_idx
         && !(setting->flags & SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED))
      command_event(setting->cmd_trigger_idx, NULL);
}

static size_t setting_get_string_representation_int_gpu_index(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   size_t _len = 0;
   if (setting)
   {
      struct string_list *list = video_driver_get_gpu_api_devices(video_context_driver_get_api());
      _len = snprintf(s, len, "%d", *setting->value.target.integer);
      if (      list
            && (*setting->value.target.integer >= 0)
            && (*setting->value.target.integer < (int)list->size)
            && list->elems[*setting->value.target.integer].data
            && *list->elems[*setting->value.target.integer].data)
      {
         _len += strlcpy(s + _len, " - ", len - _len);
         _len += strlcpy(s + _len, list->elems[*setting->value.target.integer].data, len - _len);
      }
   }
   return _len;
}

static size_t setting_get_string_representation_int(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%d", *setting->value.target.integer);
   return 0;
}

static int setting_fraction_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   *setting->value.target.fraction =
      *setting->value.target.fraction - setting->step;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      float min       = setting->min;
      float half_step = setting->step * 0.5f;
      if (*setting->value.target.fraction < min - half_step)
      {
         settings_t *settings = config_get_ptr();
         float           max  = setting->max;

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
   if (!setting)
      return -1;

   *setting->value.target.fraction =
      *setting->value.target.fraction + setting->step;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      float max = setting->max;
      if (*setting->value.target.fraction > max)
      {
         settings_t *settings = config_get_ptr();
         float          min   = setting->min;

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
            {
               if (setting->value.target.string)
                  strlcpy(setting->value.target.string,
                        setting->default_value.string,
                        setting->size);
               if (setting->actions->change)
                  setting->actions->change(setting);
            }
            else
               fill_pathname_expand_special(setting->value.target.string,
                     setting->default_value.string, setting->size);
         }
         break;
      default:
         break;
   }

   if (setting->actions->change)
      setting->actions->change(setting);
}

static int setting_generic_action_start_default(rarch_setting_t *setting)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   if (!setting)
      return -1;
   setting_reset_setting(setting);
   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static size_t setting_get_string_representation_default(rarch_setting_t *setting,
      char *s, size_t len)
{
   s[0] = '\0';
   return 0;
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
static size_t setting_get_string_representation_st_bool(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return strlcpy(s, *setting->value.target.boolean
            ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)
            : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
            len);
   return 0;
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
static size_t setting_get_string_representation_st_float(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, setting->aux.rounding_fraction,
            *setting->value.target.fraction);
   return 0;
}

static size_t setting_get_string_representation_st_path(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
#if IOS
      return fill_pathname_abbreviate_special(s,
            path_basename(setting->value.target.string), len);
#else
      return fill_pathname(s, path_basename(setting->value.target.string),
            "", len);
#endif
   }
   return 0;
}

static size_t setting_get_string_representation_st_bind(rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned index_offset                 = 0;
   const struct retro_keybind* keybind   = NULL;
   const struct retro_keybind* auto_bind = NULL;
   settings_t *settings                  = config_get_ptr();

   if (!setting)
      return 0;
   index_offset = setting->index_offset;
   keybind      = (const struct retro_keybind*)setting->value.target.keybind;
   auto_bind    = (const struct retro_keybind*)
      input_config_get_bind_auto(index_offset, keybind->id);
   return input_config_get_bind_string(settings, s, keybind, auto_bind, len);
}

static int setting_action_action_ok(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_ACTION;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_action_action_ok;
      _t.start = NULL;
      _t.sel = setting_action_action_ok;
      _t.left = NULL;
      _t.right = NULL;
      _t.change = NULL;
      _t.read = NULL;
      _t.repr = &setting_get_string_representation_default;
      result.actions = settings_actions_intern(&_t);
   }
   return result;
}

/**
 * setting_group_setting:
 * @type               : type of setting.
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = name;
   if (     type == ST_GROUP
         && string_is_equal(parent_group, MENU_ENUM_LABEL_MAIN_MENU_STR))
      result.free_flags |= SD_FREE_FLAG_MAIN_MENU_GROUP;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = NULL;
      _t.start = NULL;
      _t.sel = NULL;
      _t.left = NULL;
      _t.right = NULL;
      _t.change = NULL;
      _t.read = NULL;
      _t.repr = &setting_get_string_representation_default;
      result.actions = settings_actions_intern(&_t);
   }
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_FLOAT;

   result.size                      = sizeof(float);

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = rounding;

   result.value.target.fraction     = target;
   result.default_value.fraction    = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_generic_action_ok_default;
      _t.start = setting_generic_action_start_default;
      _t.sel = setting_generic_action_ok_default;
      _t.left = setting_fraction_action_left_default;
      _t.right = setting_fraction_action_right_default;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = &setting_get_string_representation_st_float;
      result.actions = settings_actions_intern(&_t);
   }
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_UINT;

   result.size                      = sizeof(unsigned int);

   result.name                      = dont_use_enum_idx ? strdup(name) : name;
   result.short_description         = dont_use_enum_idx ? strdup(short_description) : short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                       = 0;
   result.browser_selection_type          = ST_NONE;
   result.step                            = 0.0f;
   result.aux.rounding_fraction               = NULL;

   result.value.target.unsigned_integer   = target;
   result.default_value.unsigned_integer  = default_value;

   result.cmd_trigger_idx                 = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_generic_action_ok_default;
      _t.start = setting_generic_action_start_default;
      _t.sel = setting_generic_action_ok_default;
      _t.left = setting_uint_action_left_default;
      _t.right = setting_uint_action_right_default;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = &setting_get_string_representation_uint;
      result.actions = settings_actions_intern(&_t);
   }
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_SIZE;

   result.size                      = sizeof(size_t);

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.value.target.sizet        = target;
   result.default_value.sizet       = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_generic_action_ok_default;
      _t.start = setting_generic_action_start_default;
      _t.sel = setting_generic_action_ok_default;
      _t.left = setting_size_action_left_default;
      _t.right = setting_size_action_right_default;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = string_representation_handler;
      result.actions = settings_actions_intern(&_t);
   }
   return result;
}

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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BIND;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = idx;
   result.index_offset              = idx_offset;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.value.target.keybind      = target;
   result.default_value.keybind     = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_bind_action_ok;
      _t.start = setting_bind_action_start;
      _t.sel = setting_bind_action_ok;
      _t.left = NULL;
      _t.right = NULL;
      _t.change = NULL;
      _t.read = NULL;
      _t.repr = &setting_get_string_representation_st_bind;
      result.actions = settings_actions_intern(&_t);
   }
   return result;
}

static int setting_int_action_left_default(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   if (!setting)
      return -1;

   *setting->value.target.integer = *setting->value.target.integer - setting->step;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      float min = setting->min;
      if (*setting->value.target.integer < min)
      {
         settings_t *settings = config_get_ptr();
         float           max  = setting->max;

         if (   settings
             && settings->bools.menu_navigation_wraparound_enable)
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

   if (*setting->value.target.boolean)
      *setting->value.target.boolean = false;
   else
      *setting->value.target.boolean = true;
   if (setting->actions->change)
      setting->actions->change(setting);

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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = size;

   result.name                      = dont_use_enum_idx ? strdup(name) : name;
   result.short_description         = dont_use_enum_idx ? strdup(short_description) : short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.aux.empty_path            = empty;
   result.value.target.string       = target;
   result.default_value.string      = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   switch (type)
   {
      case ST_DIR:
         result.browser_selection_type    = ST_DIR;
         break;
      case ST_PATH:
         result.browser_selection_type    = ST_PATH;
         break;
      default:
         break;
   }

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = NULL;
      _t.start = setting_string_action_start_generic;
      _t.sel = NULL;
      _t.left = NULL;
      _t.right = NULL;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = &setting_get_string_representation_st_path;
      result.actions = settings_actions_intern(&_t);
   }
   return result;
}

/**
 * setting_string_setting_options:
 * @type               : type of setting.
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


  result.values          = values;
   SETTINGS_ACTION_SET(start, &result, setting_generic_action_start_default)
  return result;
}

/**
 * setting_subgroup_setting:
 * @type               : type of setting.
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;

   result.size                      = 0;

   result.name                      = name;
   result.short_description         = name;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = NULL;
      _t.start = NULL;
      _t.sel = NULL;
      _t.left = NULL;
      _t.right = NULL;
      _t.change = NULL;
      _t.read = NULL;
      _t.repr = &setting_get_string_representation_default;
      result.actions = settings_actions_intern(&_t);
   }
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BOOL;

   result.size                      = sizeof(bool);

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.value.target.boolean      = target;
   result.default_value.boolean     = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_bool_action_ok_default;
      _t.start = setting_generic_action_start_default;
      _t.sel = setting_generic_action_ok_default;
      _t.left = setting_bool_action_ok_default;
      _t.right = setting_bool_action_ok_default;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = &setting_get_string_representation_st_bool;
      result.actions = settings_actions_intern(&_t);
   }
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
   rarch_setting_t result = {0};

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_INT;

   result.size                      = sizeof(int);

   result.name                      = name;
   result.short_description         = short_description;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;
   result.offset_by                 = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;


   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.aux.rounding_fraction         = NULL;

   result.value.target.integer      = target;
   result.default_value.integer     = default_value;

   result.cmd_trigger_idx           = CMD_EVENT_NONE;

   if (dont_use_enum_idx)
      result.flags |= SD_FLAG_DONT_USE_ENUM_IDX_REPRESENTATION;

   {
      setting_actions_t _t;
      memset(&_t, 0, sizeof(_t));
      _t.ok = setting_generic_action_ok_default;
      _t.start = setting_generic_action_start_default;
      _t.sel = setting_generic_action_ok_default;
      _t.left = setting_int_action_left_default;
      _t.right = setting_int_action_right_default;
      _t.change = change_handler;
      _t.read = read_handler;
      _t.repr = &setting_get_string_representation_int;
      result.actions = settings_actions_intern(&_t);
   }
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_bool_setting(
         strdup(name), strdup(SHORT),
         target, default_value,
         msg_hash_to_str(off_enum_idx),
         msg_hash_to_str(on_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, true);
   if (flags != SD_FLAG_NONE)
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, flags);
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_bool_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_uint_setting(
         strdup(name), strdup(SHORT),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         true);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_SPINBOX;
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_uint_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_size_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_float_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value, rounding,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_FLOAT_SPINBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_path(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *s, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_string_setting(ST_PATH,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         s, (unsigned)len,
         default_value, "",
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
      char *s, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      enum msg_hash_enums empty_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_string_setting(ST_DIR,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         s, (unsigned)len,
         default_value,
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
      char *s, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_string_setting(ST_STRING,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         s, (unsigned)len,
         default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void config_string_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *s, size_t len,
      char *label,
      const char* shortname,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_string_setting(ST_STRING,
         strdup(label), strdup(shortname),
         s, (unsigned)len,
         default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, true);
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
}

static void config_string_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *s, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value, const char *values,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_string_setting_options(
         ST_STRING_OPTIONS,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         s, (unsigned)len,
         default_value, "", values,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   (*list)[list_info->index - 1].ui_type      = ST_UI_TYPE_STRING_COMBOBOX;

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
   /* Request values to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_VALUES);
}

static void config_bind_alt(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *s,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_bind_setting(
         strdup(name), strdup(SHORT),
         s, player, player_offset,
         default_value,
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_action_setting(
         strdup(name), strdup(SHORT),
         group_info->name, subgroup_info->name, parent_group,
         true);
   /* Request name and short description to be freed later */
   SETTINGS_DATA_LIST_CURRENT_ADD_FREE_FLAGS(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
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
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_action_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         false);

   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, name_enum_idx);
   MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info, SHORT_enum_idx);
}

static void START_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      rarch_setting_group_info_t *group_info,
      const char *name, const char *parent_group)
{
   group_info->name = name;
   if (SETTINGS_LIST_APPEND(list, list_info))
      (*list)[list_info->index++] = setting_group_setting(ST_GROUP, name, parent_group);
}

static void end_group(rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_group_setting(ST_END_GROUP, 0, parent_group);
}

static bool start_sub_group(rarch_setting_t **list,
      rarch_setting_info_t *list_info, const char *name,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return false;

   subgroup_info->name = name;

   if (!SETTINGS_LIST_APPEND(list, list_info))
      return false;
   (*list)[list_info->index++] = setting_subgroup_setting(ST_SUB_GROUP,
         name, group_info->name, parent_group, false);
   return true;
}

static void end_sub_group(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   /* Capacity check lives here now instead of inlined at every
    * call site - the wrappers below are plain calls. */
   if (!SETTINGS_LIST_APPEND(list, list_info))
      return;

   (*list)[list_info->index++] = setting_group_setting(ST_END_SUB_GROUP, 0, parent_group);
}

/* MENU SETTINGS */

static int setting_action_ok_bind_all(rarch_setting_t *setting,
      size_t idx, bool wraparound)
{
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_ALL, setting))
      return -1;
   return 0;
}

#ifdef HAVE_CONFIGFILE
static int setting_action_ok_bind_all_save_autoconfig(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned index_offset = 0;
   unsigned map          = 0;
   const char *name      = NULL;
   settings_t *settings  = config_get_ptr();

   if (!setting || !settings)
      return -1;

   index_offset = setting->index_offset;
   map          = settings->uints.input_joypad_index[index_offset];
   name         = input_config_get_device_name(map);

   if (      name
         && *name
         && config_save_autoconf_profile(name, map))
   {
      int i;
      size_t _len;
      char buf[128];
      char msg[NAME_MAX_LENGTH];
      struct retro_keybind *target = &input_config_binds[map][0];

      config_get_autoconf_profile_filename(name, map, buf, sizeof(buf));
      _len = snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED), buf);
      runloop_msg_queue_push(msg, _len, 1, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_SUCCESS);

      /* Clear manual controller binds */
      for ( i  = MENU_SETTINGS_BIND_BEGIN;
            i <= MENU_SETTINGS_BIND_LAST; i++, target++)
      {
         target->joykey  = NO_BTN;
         target->joyaxis = AXIS_NONE;
      }

      /* Load and activate saved profile */
      {
         const input_device_driver_t *joypad = input_state_get_ptr()->primary_joypad;

         if (joypad)
            input_autoconfigure_connect(joypad->name(map),
                  NULL, NULL, joypad->ident,
                  map, 0, 0);
      }
   }
   else
   {
      const char *_msg = msg_hash_to_str(MSG_AUTOCONFIG_FILE_ERROR_SAVING);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   return 0;
}
#endif

static int setting_action_ok_bind_defaults(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned i;
   struct menu_state    *menu_st         = menu_state_get_ptr();
   struct menu_bind_state *binds         = &menu_st->input_binds;
   struct retro_keybind *target          = NULL;
   const struct retro_keybind *def_binds = NULL;

   if (!setting)
      return -1;

   target             =  &input_config_binds[setting->index_offset][0];
   def_binds          =  (setting->index_offset)
                        ? retro_keybinds_rest
                        : retro_keybinds_1;
   binds->begin       = MENU_SETTINGS_BIND_BEGIN;
   binds->last        = MENU_SETTINGS_BIND_LAST;

   for ( i  = MENU_SETTINGS_BIND_BEGIN;
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

static int setting_action_ok_retropad_bind(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_RETROPAD_BIND);
   return 1;
}

static int setting_action_left_retropad_bind(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int value       = 0;
   int step        = 1;
   int i           = 0;
   bool overflowed = false;

   if (!setting)
      return -1;

   value = *setting->value.target.integer;

   if (value < 0)
      overflowed = true;
   else if (input_config_bind_order[value] == 0)
      *setting->value.target.integer = -1;
   else
   {
      for (i = 0; i < setting->max + 1; i++)
      {
         if ((int)input_config_bind_order[i] == value)
         {
            *setting->value.target.integer = input_config_bind_order[i - step];
            break;
         }
      }
   }

   i -= step;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      if (overflowed || i < setting->min)
      {
         settings_t *settings = config_get_ptr();

         if (settings &&
             settings->bools.menu_navigation_wraparound_enable)
         {
            unsigned max = (unsigned)setting->max;
            *setting->value.target.integer = input_config_bind_order[max];
         }
      }
   }

   return 0;
}

static int setting_action_right_retropad_bind(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int value = 0;
   int step  = 1;
   int i     = 0;

   if (!setting)
      return -1;

   value = *setting->value.target.integer;

   if (value < 0)
      *setting->value.target.integer = input_config_bind_order[0];
   else
   {
      for (i = 0; i < setting->max + 1; i++)
      {
         if ((int)input_config_bind_order[i] == value)
         {
            *setting->value.target.integer = input_config_bind_order[i + step];
            break;
         }
      }
   }

   i += step;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      if (i > setting->max)
      {
         settings_t *settings = config_get_ptr();
         int min              = (int)setting->min;
         if (settings && settings->bools.menu_navigation_wraparound_enable)
         {
            if (min < 0)
               *setting->value.target.integer = min;
            else
               *setting->value.target.integer = input_config_bind_order[min];
         }
      }
   }

   return 0;
}

#if defined(HAVE_NETWORKING)
static void setting_action_ok_color_rgb_cb(void *userdata, const char *line)
{
   if (line && *line)
   {
      struct menu_state *menu_st  = menu_state_get_ptr();
      const char *label           = menu_st->input_dialog_kb_label_setting;
      rarch_setting_t *setting    = menu_setting_find(label);

      if (setting)
      {
         unsigned rgb;
         char    *rgb_end = NULL;

         if (*line == '#')
            line++;

         rgb = (unsigned)strtoul(line, &rgb_end, 16);

         if (!(*rgb_end) && (rgb_end - line) == STRLEN_CONST("RRGGBB"))
            *setting->value.target.unsigned_integer = rgb;
      }
   }

   menu_input_dialog_end();
}

static int setting_action_ok_color_rgb(rarch_setting_t *setting, size_t idx,
      bool wraparound)
{
   menu_input_ctx_line_t line;

   if (!setting)
      return -1;

   line.label         = setting->short_description;
   line.label_setting = setting->name;
   line.type          = 0;
   line.idx           = 0;
   line.cb            = setting_action_ok_color_rgb_cb;

   if (!menu_input_dialog_start(&line))
      return -1;

   return 0;
}
#endif

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

#ifdef ANDROID
static int setting_action_ok_select_physical_keyboard(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD);
   return 0;
}
#endif

static int setting_action_ok_select_reserved_device(
        rarch_setting_t *setting, size_t idx, bool wraparound)
{
    char enum_idx[16];
    if (!setting)
        return -1;

    snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

    generic_action_ok_displaylist_push(
            enum_idx, /* we will pass the enumeration index of the string as a path */
            NULL, NULL, 0, idx, 0,
            ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_SELECT_RESERVED_DEVICE);
    return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_ok_audio_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_AUDIO_DEVICE);
   return 0;
}

static int setting_string_action_start_audio_device(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   strlcpy(setting->value.target.string, "", setting->size);

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return 0;
}

static int setting_string_action_ok_midi_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_MIDI_DEVICE);
   return 0;
}

static int setting_string_action_start_midi_device(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   setting_reset_setting(setting);

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return 0;
}

#ifdef HAVE_MICROPHONE
static int setting_string_action_start_microphone_device(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   strlcpy(setting->value.target.string, "", setting->size);

   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   return 0;
}

static int setting_string_action_ok_microphone_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE);
   return 0;
}
#endif
#endif

static size_t setting_get_string_representation_streaming_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case STREAMING_MODE_TWITCH:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_TWITCH), len);
         case STREAMING_MODE_YOUTUBE:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_YOUTUBE), len);
         case STREAMING_MODE_FACEBOOK:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_FACEBOOK), len);
         case STREAMING_MODE_KICK:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_KICK), len);
         case STREAMING_MODE_LOCAL:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL), len);
         case STREAMING_MODE_CUSTOM:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_video_stream_quality(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RECORD_CONFIG_TYPE_STREAMING_CUSTOM:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM), len);
         case RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY), len);
         case RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY), len);
         case RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_video_record_quality(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RECORD_CONFIG_TYPE_RECORDING_CUSTOM:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM), len);
         case RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY), len);
         case RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY), len);
         case RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY), len);
         case RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY), len);
         case RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST), len);
         case RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY), len);
         case RECORD_CONFIG_TYPE_RECORDING_GIF:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_GIF), len);
         case RECORD_CONFIG_TYPE_RECORDING_APNG:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_APNG), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_video_filter(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return fill_pathname(s, path_basename(setting->value.target.string),
            "", len);
   return 0;
}

static size_t setting_get_string_representation_video_font_path(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (!setting)
      return 0;
   if (!setting->value.target.string || !*setting->value.target.string)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
   return fill_pathname(s, path_basename(setting->value.target.string),
         "", len);
}

static size_t setting_get_string_representation_video_hdr_expand_gamut(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE), len);
         case 1:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED), len);
         case 2:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE), len);
         case 3:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_SUPER), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_video_hdr_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF), len);
         case 1:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_HDR10), len);
         case 2:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_SCRGB), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_video_hdr_subpixel_layout(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT_RGB), len);
         case 1:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT_RBG), len);
         case 2:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT_BGR), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_state_slot(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (!setting)
      return 0;
   if (*setting->value.target.integer == -1)
      return strlcpy(s, "Auto", len);
   return snprintf(s, len, "%d", *setting->value.target.integer);
}

static size_t setting_get_string_representation_percentage(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%d%%", *setting->value.target.integer);
   return 0;
}

static size_t setting_get_string_representation_float_video_msg_color(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%d", (int)(*setting->value.target.fraction * 255.0f));
   return 0;
}

static size_t setting_get_string_representation_max_users(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
   return 0;
}

#if defined(HAVE_CHEEVOS) || defined(HAVE_CLOUDSYNC)
static size_t setting_get_string_representation_password(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      if (   setting->value.target.string
          && setting->value.target.string[0] != '\0')
         return strlcpy(s, "********", len);
      if (config_get_ptr()->arrays.cheevos_token[0])
         return strlcpy(s, "********", len);
      *setting->value.target.string = '\0';
   }
   return 0;
}
#endif

#if TARGET_OS_IPHONE
static size_t setting_get_string_representation_uint_keyboard_gamepad_mapping_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
         case 1:
            return strlcpy(s, "iPega PG-9017", len);
         case 2:
            return strlcpy(s, "8-bitty", len);
         case 3:
            return strlcpy(s, "SNES30 8bitdo", len);
      }
   }
   return 0;
}
#endif

#ifdef HAVE_TRANSLATE
static size_t setting_get_string_representation_uint_ai_service_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   if (!setting)
      return 0;
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
      return strlcpy(s, msg_hash_to_str(enum_idx), len);
   return 0;
}

static size_t setting_get_string_representation_uint_ai_service_lang(
      rarch_setting_t *setting, char *s, size_t len)
{
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   if (!setting)
      return 0;
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
      case TRANSLATION_LANG_BE:
         enum_idx = MENU_ENUM_LABEL_VALUE_LANG_BELARUSIAN;
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
      return strlcpy(s, msg_hash_to_str(enum_idx), len);
   return snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}
#endif

#if defined(__linux__) && !defined(ANDROID)
static size_t setting_get_string_representation_uint_accessibility_narrator_engine(
      rarch_setting_t *setting, char *s, size_t len)
{
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   if (!setting)
      return 0;
   switch (*setting->value.target.unsigned_integer)
   {
      case ACCESSIBILITY_NARRATOR_ENGINE_ESPEAK:
         enum_idx = MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_ENGINE_ESPEAK;
         break;
      case ACCESSIBILITY_NARRATOR_ENGINE_SPEECH_DISPATCHER:
         enum_idx = MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_ENGINE_SPEECH_DISPATCHER;
         break;
      default:
         break;
   }
   if (enum_idx != 0)
      return strlcpy(s, msg_hash_to_str(enum_idx), len);
   return 0;
}
#endif

static size_t setting_get_string_representation_uint_menu_thumbnails(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (!setting)
      return 0;

   switch (*setting->value.target.unsigned_integer)
   {
      default:
      case 0:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         break;
      case 1:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS), len);
         break;
      case 2:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS), len);
         break;
      case 3:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS), len);
         break;
      case 4:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS), len);
         break;
   }
}

static void setting_set_string_representation_timedate_date_separator(char *s)
{
   settings_t *settings                  = config_get_ptr();
   unsigned menu_timedate_date_separator = settings
         ? settings->uints.menu_timedate_date_separator
         : MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN;

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

static size_t setting_get_string_representation_uint_menu_timedate_style(
   rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len = 0;
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_TIMEDATE_STYLE_YMD_HMS:
            _len = strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS), len);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM:
            _len = strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_YMD:
            _len = strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD), len);
            break;
         case MENU_TIMEDATE_STYLE_YM:
            _len = strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YM), len);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS), len);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_MD_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY), len);
            break;
         case MENU_TIMEDATE_STYLE_MD:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM), len);
            break;
         case MENU_TIMEDATE_STYLE_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS), len);
            break;
         case MENU_TIMEDATE_STYLE_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_HM), len);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_MD_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMMYYYY_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_DDMM_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM), len);
            break;
         case MENU_TIMEDATE_STYLE_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM), len);
            break;
      }
   }

   /* Change date separator, if required */
   setting_set_string_representation_timedate_date_separator(s);
   return _len;
}

static size_t setting_get_string_representation_uint_menu_timedate_date_separator(
   rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN:
            return strlcpy(s, "'-'", len);
         case MENU_TIMEDATE_DATE_SEPARATOR_SLASH:
            return strlcpy(s, "'/'", len);
         case MENU_TIMEDATE_DATE_SEPARATOR_PERIOD:
            return strlcpy(s, "'.'", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_time_show(
   rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case TIME_SHOW_HM:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMEDATE_HM), len);
         case TIME_SHOW_HMS:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS), len);
         case TIME_SHOW_HM_AMPM:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM), len);
         case TIME_SHOW_HMS_AMPM:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM), len);
         case TIME_SHOW_OFF:
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
      }
   }

   return 0;
}

static size_t setting_get_string_representation_uint_menu_add_content_entry_display_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_ADD_CONTENT_ENTRY_DISPLAY_HIDDEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
         case MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB),
                  len);
         case MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_contentless_cores_display_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_CONTENTLESS_CORES_DISPLAY_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
         case MENU_CONTENTLESS_CORES_DISPLAY_ALL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL),
                  len);
         case MENU_CONTENTLESS_CORES_DISPLAY_SINGLE_PURPOSE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE),
                  len);
         case MENU_CONTENTLESS_CORES_DISPLAY_CUSTOM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_rgui_menu_color_theme(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_THEME_CUSTOM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM),
                  len);
         case RGUI_THEME_CLASSIC_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED),
                  len);
         case RGUI_THEME_CLASSIC_ORANGE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE),
                  len);
         case RGUI_THEME_CLASSIC_YELLOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW),
                  len);
         case RGUI_THEME_CLASSIC_GREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN),
                  len);
         case RGUI_THEME_CLASSIC_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE),
                  len);
         case RGUI_THEME_CLASSIC_VIOLET:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET),
                  len);
         case RGUI_THEME_CLASSIC_GREY:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY),
                  len);
         case RGUI_THEME_LEGACY_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED),
                  len);
         case RGUI_THEME_DARK_PURPLE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE),
                  len);
         case RGUI_THEME_MIDNIGHT_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE),
                  len);
         case RGUI_THEME_GOLDEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN),
                  len);
         case RGUI_THEME_ELECTRIC_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE),
                  len);
         case RGUI_THEME_APPLE_GREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN),
                  len);
         case RGUI_THEME_VOLCANIC_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED),
                  len);
         case RGUI_THEME_LAGOON:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON),
                  len);
         case RGUI_THEME_BROGRAMMER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER),
                  len);
         case RGUI_THEME_DRACULA:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA),
                  len);
         case RGUI_THEME_FAIRYFLOSS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS),
                  len);
         case RGUI_THEME_FLATUI:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI),
                  len);
         case RGUI_THEME_GRUVBOX_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK),
                  len);
         case RGUI_THEME_GRUVBOX_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT),
                  len);
         case RGUI_THEME_HACKING_THE_KERNEL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL),
                  len);
         case RGUI_THEME_NORD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD),
                  len);
         case RGUI_THEME_NOVA:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA),
                  len);
         case RGUI_THEME_ONE_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK),
                  len);
         case RGUI_THEME_PALENIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT),
                  len);
         case RGUI_THEME_SOLARIZED_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK),
                  len);
         case RGUI_THEME_SOLARIZED_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT),
                  len);
         case RGUI_THEME_TANGO_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK),
                  len);
         case RGUI_THEME_TANGO_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT),
                  len);
         case RGUI_THEME_ZENBURN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN),
                  len);
         case RGUI_THEME_ANTI_ZENBURN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN),
                  len);
         case RGUI_THEME_FLUX:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX),
                  len);
         case RGUI_THEME_DYNAMIC:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC),
                  len);
         case RGUI_THEME_GRAY_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK),
                  len);
         case RGUI_THEME_GRAY_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT),
                  len);
         case RGUI_THEME_EVERGARDEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_EVERGARDEN),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_rgui_thumbnail_scaler(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_THUMB_SCALE_POINT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT),
                  len);
         case RGUI_THUMB_SCALE_BILINEAR:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR),
                  len);
         case RGUI_THUMB_SCALE_SINC:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_rgui_internal_upscale_level(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_UPSCALE_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE),
                  len);
         case RGUI_UPSCALE_AUTO:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO),
                  len);
         case RGUI_UPSCALE_X2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X2),
                  len);
         case RGUI_UPSCALE_X3:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X3),
                  len);
         case RGUI_UPSCALE_X4:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X4),
                  len);
         case RGUI_UPSCALE_X5:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X5),
                  len);
         case RGUI_UPSCALE_X6:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X6),
                  len);
         case RGUI_UPSCALE_X7:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X7),
                  len);
         case RGUI_UPSCALE_X8:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X8),
                  len);
         case RGUI_UPSCALE_X9:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X9),
                  len);
      }
   }
   return 0;
}

#if !defined(DINGUX)
static size_t setting_get_string_representation_uint_rgui_aspect_ratio(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_ASPECT_RATIO_4_3:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_4_3),
                  len);
         case RGUI_ASPECT_RATIO_16_9:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9),
                  len);
         case RGUI_ASPECT_RATIO_16_9_CENTRE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE),
                  len);
         case RGUI_ASPECT_RATIO_16_10:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10),
                  len);
         case RGUI_ASPECT_RATIO_16_10_CENTRE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE),
                  len);
         case RGUI_ASPECT_RATIO_21_9:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9),
                  len);
         case RGUI_ASPECT_RATIO_21_9_CENTRE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE),
                  len);
         case RGUI_ASPECT_RATIO_3_2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2),
                  len);
         case RGUI_ASPECT_RATIO_3_2_CENTRE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE),
                  len);
         case RGUI_ASPECT_RATIO_5_3:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3),
                  len);
         case RGUI_ASPECT_RATIO_5_3_CENTRE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE),
                  len);
         case RGUI_ASPECT_RATIO_AUTO:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_rgui_aspect_ratio_lock(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_ASPECT_RATIO_LOCK_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE),
                  len);
         case RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN),
                  len);
         case RGUI_ASPECT_RATIO_LOCK_INTEGER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER),
                  len);
         case RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN),
                  len);
      }
   }
   return 0;
}
#endif

static size_t setting_get_string_representation_uint_rgui_particle_effect(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RGUI_PARTICLE_EFFECT_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE),
                  len);
         case RGUI_PARTICLE_EFFECT_SNOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW),
                  len);
         case RGUI_PARTICLE_EFFECT_SNOW_ALT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT),
                  len);
         case RGUI_PARTICLE_EFFECT_RAIN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN),
                  len);
         case RGUI_PARTICLE_EFFECT_VORTEX:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX),
                  len);
         case RGUI_PARTICLE_EFFECT_STARFIELD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_ticker_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case TICKER_TYPE_BOUNCE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE),
                  len);
         case TICKER_TYPE_LOOP:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP),
                  len);
      }
   }
   return 0;
}

/* The XMB animation settings are shared with the Ozone driver; their
 * handlers must build whenever either driver does. */
#if defined(HAVE_XMB) || defined(HAVE_OZONE)
static size_t setting_get_string_representation_uint_menu_xmb_animation_move_up_down(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, "Easing Out Quad", len);
         case 1:
            return strlcpy(s, "Easing Out Expo", len);
         case 2:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_xmb_animation_opening_main_menu(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, "Easing Out Quad", len);
         case 1:
            return strlcpy(s, "Easing Out Circ", len);
         case 2:
            return strlcpy(s, "Easing Out Expo", len);
         case 3:
            return strlcpy(s, "Easing Out Bounce", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_xmb_animation_horizontal_highlight(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, "Easing Out Quad", len);
         case 1:
            return strlcpy(s, "Easing In Sine", len);
         case 2:
            return strlcpy(s, "Easing Out Bounce", len);
      }
   }
   return 0;
}

#endif
#ifdef HAVE_XMB
static size_t setting_get_string_representation_uint_xmb_icon_theme(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case XMB_ICON_THEME_MONOCHROME:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME), len);
         case XMB_ICON_THEME_FLATUI:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI), len);
         case XMB_ICON_THEME_FLATUX:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUX), len);
         case XMB_ICON_THEME_RETROSYSTEM:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM), len);
         case XMB_ICON_THEME_PIXEL:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL), len);
         case XMB_ICON_THEME_SYSTEMATIC:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC), len);
         case XMB_ICON_THEME_DOTART:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART), len);
         case XMB_ICON_THEME_MONOCHROME_INVERTED:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED), len);
         case XMB_ICON_THEME_CUSTOM:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM), len);
         case XMB_ICON_THEME_AUTOMATIC:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC), len);
         case XMB_ICON_THEME_AUTOMATIC_INVERTED:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED), len);
         case XMB_ICON_THEME_DAITE:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DAITE), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_xmb_layout(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, "Auto", len);
         case 1:
            return strlcpy(s, "Console", len);
         case 2:
            return strlcpy(s, "Handheld", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_xmb_menu_color_theme(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case XMB_THEME_WALLPAPER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN),
                  len);
         case XMB_THEME_LEGACY_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED),
                  len);
         case XMB_THEME_DARK_PURPLE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE),
                  len);
         case XMB_THEME_MIDNIGHT_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE),
                  len);
         case XMB_THEME_GOLDEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN),
                  len);
         case XMB_THEME_ELECTRIC_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE),
                  len);
         case XMB_THEME_APPLE_GREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN),
                  len);
         case XMB_THEME_UNDERSEA:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA),
                  len);
         case XMB_THEME_VOLCANIC_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED),
                  len);
         case XMB_THEME_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK),
                  len);
         case XMB_THEME_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT),
                  len);
         case XMB_THEME_MORNING_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE),
                  len);
         case XMB_THEME_SUNBEAM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM),
                  len);
         case XMB_THEME_LIME:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME),
                  len);
         case XMB_THEME_MIDGAR:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR),
                  len);
         case XMB_THEME_PIKACHU_YELLOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW),
                  len);
         case XMB_THEME_GAMECUBE_PURPLE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE),
                  len);
         case XMB_THEME_FAMICOM_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED),
                  len);
         case XMB_THEME_FLAMING_HOT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT),
                  len);
         case XMB_THEME_ICE_COLD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD),
                  len);
         case XMB_THEME_GRAY_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK),
                  len);
         case XMB_THEME_GRAY_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_xmb_current_menu_icon(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE), len);
         case 2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE), len);
         case 1:
         default:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NORMAL), len);
      }
   }
   return 0;
}
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#if defined(HAVE_XMB) && defined(HAVE_SHADERPIPELINE)
static size_t setting_get_string_representation_uint_xmb_shader_pipeline(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case XMB_SHADER_PIPELINE_WALLPAPER:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         case XMB_SHADER_PIPELINE_SIMPLE_RIBBON:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED), len);
         case XMB_SHADER_PIPELINE_RIBBON:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON), len);
         case XMB_SHADER_PIPELINE_SIMPLE_SNOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW), len);
         case XMB_SHADER_PIPELINE_SNOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW), len);
         case XMB_SHADER_PIPELINE_BOKEH:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH), len);
         case XMB_SHADER_PIPELINE_SNOWFLAKE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE), len);
      }
   }
   return 0;
}
#endif
#endif

#ifdef HAVE_MATERIALUI
static size_t setting_get_string_representation_uint_materialui_menu_color_theme(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MATERIALUI_THEME_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE), len);
         case MATERIALUI_THEME_BLUE_GREY:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY), len);
         case MATERIALUI_THEME_GREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN), len);
         case MATERIALUI_THEME_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED), len);
         case MATERIALUI_THEME_YELLOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW), len);
         case MATERIALUI_THEME_DARK_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE), len);
         case MATERIALUI_THEME_NVIDIA_SHIELD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD), len);
         case MATERIALUI_THEME_MATERIALUI:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI), len);
         case MATERIALUI_THEME_MATERIALUI_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK), len);
         case MATERIALUI_THEME_OZONE_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK), len);
         case MATERIALUI_THEME_NORD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD), len);
         case MATERIALUI_THEME_GRUVBOX_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK), len);
         case MATERIALUI_THEME_SOLARIZED_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK), len);
         case MATERIALUI_THEME_CUTIE_BLUE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE), len);
         case MATERIALUI_THEME_CUTIE_CYAN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN), len);
         case MATERIALUI_THEME_CUTIE_GREEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN), len);
         case MATERIALUI_THEME_CUTIE_ORANGE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE), len);
         case MATERIALUI_THEME_CUTIE_PINK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK), len);
         case MATERIALUI_THEME_CUTIE_PURPLE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE), len);
         case MATERIALUI_THEME_CUTIE_RED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED), len);
         case MATERIALUI_THEME_VIRTUAL_BOY:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY), len);
         case MATERIALUI_THEME_HACKING_THE_KERNEL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL), len);
         case MATERIALUI_THEME_GRAY_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK), len);
         case MATERIALUI_THEME_GRAY_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT), len);
         case MATERIALUI_THEME_DRACULA:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DRACULA), len);
         default:
            break;
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_materialui_menu_transition_animation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MATERIALUI_TRANSITION_ANIM_AUTO:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO), len);
         case MATERIALUI_TRANSITION_ANIM_FADE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE), len);
         case MATERIALUI_TRANSITION_ANIM_SLIDE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE), len);
         case MATERIALUI_TRANSITION_ANIM_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE), len);
         default:
            break;
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_materialui_menu_thumbnail_view_portrait(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED), len);
         case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL), len);
         case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM), len);
         case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON), len);
         default:
            break;
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_materialui_menu_thumbnail_view_landscape(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED), len);
         case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL), len);
         case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM), len);
         case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE), len);
         case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP), len);
         default:
            break;
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_materialui_landscape_layout_optimization(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED), len);
         case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS), len);
         case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS), len);
         default:
            break;
      }
   }
   return 0;
}
#endif

#ifdef HAVE_OZONE
static size_t setting_get_string_representation_uint_ozone_menu_color_theme(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case OZONE_COLOR_THEME_BASIC_BLACK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK), len);
         case OZONE_COLOR_THEME_NORD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD), len);
         case OZONE_COLOR_THEME_GRUVBOX_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK), len);
         case OZONE_COLOR_THEME_BOYSENBERRY:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY), len);
         case OZONE_COLOR_THEME_HACKING_THE_KERNEL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL), len);
         case OZONE_COLOR_THEME_TWILIGHT_ZONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE), len);
         case OZONE_COLOR_THEME_DRACULA:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA), len);
         case OZONE_COLOR_THEME_SELENIUM:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM), len);
         case OZONE_COLOR_THEME_SOLARIZED_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK), len);
         case OZONE_COLOR_THEME_SOLARIZED_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT), len);
         case OZONE_COLOR_THEME_GRAY_DARK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK), len);
         case OZONE_COLOR_THEME_GRAY_LIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT), len);
         case OZONE_COLOR_THEME_PURPLE_RAIN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN), len);
         case OZONE_COLOR_THEME_BASIC_WHITE:
         default:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE), len);
         case OZONE_COLOR_THEME_EVERGARDEN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_EVERGARDEN), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_ozone_header_icon(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE), len);
         case 2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED), len);
         case 1:
         default:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_DYNAMIC), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_ozone_header_separator(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE), len);
         case 2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM), len);
         case 1:
         default:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NORMAL), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_ozone_font_scale(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case OZONE_FONT_SCALE_SEPARATE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE), len);
         case OZONE_FONT_SCALE_GLOBAL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL), len);
         case OZONE_FONT_SCALE_NONE:
         default:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF), len);
      }
   }
   return 0;
}
#endif


#ifdef HAVE_SCREENSHOTS
#ifdef HAVE_GFX_WIDGETS
static size_t setting_get_string_representation_uint_notification_show_screenshot_duration(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL), len);
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST), len);
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST), len);
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_notification_show_screenshot_flash(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL), len);
         case NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST), len);
         case NOTIFICATION_SHOW_SCREENSHOT_FLASH_OFF:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
      }
   }
   return 0;
}
#endif
#endif

static size_t setting_get_string_representation_uint_video_autoswitch_refresh_rate(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN), len);
         case AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN), len);
         case AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN), len);
         case AUTOSWITCH_REFRESH_RATE_OFF:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_video_monitor_index(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting && *setting->value.target.unsigned_integer)
      return snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
   return strlcpy(s, "0 (Auto)", len);
}

static size_t setting_get_string_representation_uint_custom_vp_width(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len;
   struct retro_game_geometry  *geom    = NULL;
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   unsigned int rotation                = retroarch_get_rotation();
   if (!setting || !av_info)
      return 0;
   geom    = (struct retro_game_geometry*)&av_info->geometry;
   _len    = snprintf(s, len, "%u", *setting->value.target.unsigned_integer);
   if (!geom->base_width || !geom->base_height)
      return _len;
   if (!(rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_width == 0))
      _len += snprintf(s + _len, len - _len, " (%ux)",
            *setting->value.target.unsigned_integer / geom->base_width);
   else if ((rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_height == 0))
      _len += snprintf(s + _len, len - _len, " (%ux)",
            *setting->value.target.unsigned_integer / geom->base_height);
   return _len;
}

static size_t setting_get_string_representation_uint_custom_vp_height(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len;
   struct retro_game_geometry  *geom    = NULL;
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   unsigned int rotation                = retroarch_get_rotation();
   if (!setting || !av_info)
      return 0;
   geom    = (struct retro_game_geometry*)&av_info->geometry;
   _len    = snprintf(s, len, "%u", *setting->value.target.unsigned_integer);
   if (!geom->base_width || !geom->base_height)
      return _len;
   if (!(rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_height == 0))
      _len += snprintf(s + _len, len - _len, " (%ux)",
            *setting->value.target.unsigned_integer / geom->base_height);
   else  if ((rotation % 2) && (*setting->value.target.unsigned_integer % geom->base_width == 0))
      _len += snprintf(s + _len, len - _len, " (%ux)",
            *setting->value.target.unsigned_integer / geom->base_width);
   return _len;
}

#ifdef HAVE_ASIO
static int setting_action_asio_control_panel(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   audio_asio_open_control_panel();
   return 0;
}
#endif

#ifdef HAVE_WASAPI
static size_t setting_get_string_representation_uint_audio_wasapi_sh_buffer_length(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len;
   settings_t *settings = config_get_ptr();
   if (!setting || !settings)
      return 0;
   _len = snprintf(s, len, "%u (", *setting->value.target.integer);
   switch (*setting->value.target.integer)
   {
      case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         /* TODO/FIXME - localize */
         _len += strlcpy(s + _len, "Audio Latency", len - _len);
         break;
      case WASAPI_SH_BUFFER_DEVICE_PERIOD:
         /* TODO/FIXME - localize */
         _len += strlcpy(s + _len, "Device Period", len - _len);
         break;
      case WASAPI_SH_BUFFER_CLIENT_BUFFER:
         /* TODO/FIXME - localize */
         _len += strlcpy(s + _len, "Client Buffer", len - _len);
         break;
      default:
         _len += snprintf(s + _len, len - _len, "%.1f ms",
               (float)*setting->value.target.integer * 1000 / settings->uints.audio_output_sample_rate);
         break;
   }
   _len += strlcpy(s + _len, ")", len - _len);
   return _len;
}

#ifdef HAVE_MICROPHONE
static size_t setting_get_string_representation_uint_microphone_wasapi_sh_buffer_length(
      rarch_setting_t *setting, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   if (!setting || !settings)
      return 0;
   if (*setting->value.target.integer > 0)
      return snprintf(s, len, "%u (%.1f ms)",
            *setting->value.target.integer,
            (float)*setting->value.target.integer * 1000 / settings->uints.audio_output_sample_rate);
   return strlcpy(s, "Auto", len);
}
#endif
#endif

#if !defined(RARCH_CONSOLE)
static size_t setting_get_string_representation_string_audio_device(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return 0;
   if (!setting->value.target.string || !*setting->value.target.string)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
   return strlcpy(s, setting->value.target.string, len);
}
#endif

static size_t setting_get_string_representation_crt_switch_resolution_super(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (!setting)
      return 0;
   if (*setting->value.target.unsigned_integer == 0)
      return strlcpy(s, "NATIVE", len);
   else if (*setting->value.target.unsigned_integer == 1)
      return strlcpy(s, "DYNAMIC", len);
   return snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}

static size_t setting_get_string_representation_uint_playlist_sublabel_runtime_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case PLAYLIST_RUNTIME_PER_CORE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE),
                  len);
         case PLAYLIST_RUNTIME_AGGREGATE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_playlist_sublabel_last_played_style(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len = 0;
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MD_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MD:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MD_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM_AMPM:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM),
                  len);
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_AGO:
            _len = strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO),
                  len);
            break;
      }
   }

   /* Change date separator, if required */
   setting_set_string_representation_timedate_date_separator(s);
   return _len;
}

static size_t setting_get_string_representation_uint_playlist_inline_core_display_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV),
                  len);
         case PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS),
                  len);
         case PLAYLIST_INLINE_CORE_DISPLAY_NEVER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_playlist_entry_remove_enable(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV),
                  len);
         case PLAYLIST_ENTRY_REMOVE_ENABLE_ALL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL),
                  len);
         case PLAYLIST_ENTRY_REMOVE_ENABLE_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE),
                  len);
      }
   }
   return 0;
}

#ifdef _3DS
static size_t setting_get_string_representation_uint_video_3ds_display_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case CTR_VIDEO_MODE_3D:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_3D),
                  len);
         case CTR_VIDEO_MODE_2D:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D),
                  len);
         case CTR_VIDEO_MODE_2D_400X240:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240),
                  len);
         case CTR_VIDEO_MODE_2D_800X240:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240),
                  len);
      }
   }
   return 0;
}
#endif

#if defined(DINGUX)
static size_t setting_get_string_representation_uint_video_dingux_ipu_filter_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case DINGUX_IPU_FILTER_BICUBIC:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC),
                  len);
         case DINGUX_IPU_FILTER_BILINEAR:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR),
                  len);
         case DINGUX_IPU_FILTER_NEAREST:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST),
                  len);
      }
   }
   return 0;
}

#if defined(DINGUX_BETA)
static size_t setting_get_string_representation_uint_video_dingux_refresh_rate(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case DINGUX_REFRESH_RATE_60HZ:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_60HZ),
                  len);
         case DINGUX_REFRESH_RATE_50HZ:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_50HZ),
                  len);
      }
   }
   return 0;
}
#endif

#if defined(RS90) || defined(MIYOO)
static size_t setting_get_string_representation_uint_video_dingux_rs90_softfilter_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case DINGUX_RS90_SOFTFILTER_POINT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT),
                  len);
         case DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ),
                  len);
      }
   }
   return 0;
}
#endif
#endif

static size_t setting_get_string_representation_uint_input_auto_game_focus(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case AUTO_GAME_FOCUS_OFF:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF),
                  len);
         case AUTO_GAME_FOCUS_ON:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON),
                  len);
         case AUTO_GAME_FOCUS_DETECT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT),
                  len);
      }
   }
   return 0;
}

#ifdef HAVE_CLOUDSYNC
static size_t setting_get_string_representation_uint_cloud_sync_sync_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case CLOUD_SYNC_MODE_AUTOMATIC:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC),
                  len);
         case CLOUD_SYNC_MODE_MANUAL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL),
                  len);
      }
   }
   return 0;
}
#endif

#if defined(HAVE_OVERLAY)
static size_t setting_get_string_representation_uint_input_overlay_show_inputs(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case OVERLAY_SHOW_INPUT_NONE:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
         case OVERLAY_SHOW_INPUT_TOUCHED:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED),
                  len);
         case OVERLAY_SHOW_INPUT_PHYSICAL:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_input_overlay_show_inputs_port(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer + 1);
   return 0;
}

static size_t setting_get_string_representation_overlay_lightgun_port(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      if (*setting->value.target.integer < 0)
         return strlcpy(s, msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY), len);
      return snprintf(s, len, "%d", *setting->value.target.integer + 1);
   }
   return 0;
}

static size_t setting_get_string_representation_overlay_lightgun_action(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case OVERLAY_LIGHTGUN_ACTION_NONE:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_NONE), len);
         case OVERLAY_LIGHTGUN_ACTION_TRIGGER:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER), len);
         case OVERLAY_LIGHTGUN_ACTION_RELOAD:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD), len);
         case OVERLAY_LIGHTGUN_ACTION_AUX_A:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A), len);
         case OVERLAY_LIGHTGUN_ACTION_AUX_B:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B), len);
         case OVERLAY_LIGHTGUN_ACTION_AUX_C:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C), len);
         case OVERLAY_LIGHTGUN_ACTION_SELECT:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT), len);
         case OVERLAY_LIGHTGUN_ACTION_START:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START), len);
         case OVERLAY_LIGHTGUN_ACTION_DPAD_UP:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP), len);
         case OVERLAY_LIGHTGUN_ACTION_DPAD_DOWN:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN), len);
         case OVERLAY_LIGHTGUN_ACTION_DPAD_LEFT:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT), len);
         case OVERLAY_LIGHTGUN_ACTION_DPAD_RIGHT:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_overlay_mouse_btn(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case OVERLAY_MOUSE_BTN_NONE:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_NONE), len);
         case OVERLAY_MOUSE_BTN_LMB:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT), len);
         case OVERLAY_MOUSE_BTN_RMB:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT), len);
         case OVERLAY_MOUSE_BTN_MMB:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE), len);
      }
   }

   return 0;
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
   settings_t *settings = config_get_ptr();
   unsigned port        = 0;

   if (!setting)
      return -1;

   port = setting->index_offset;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   settings->uints.input_analog_dpad_mode[port] =
      (settings->uints.input_analog_dpad_mode[port] + ANALOG_DPAD_LAST - 1)
      % ANALOG_DPAD_LAST;

   return 0;
}

unsigned libretro_device_get_size(unsigned *devices, size_t devices_size, unsigned port)
{
   unsigned types                           = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_system_info_t            *sys_info = &runloop_state_get_ptr()->system;

   devices[types++]                         = RETRO_DEVICE_NONE;
   devices[types++]                         = RETRO_DEVICE_JOYPAD;

   if (sys_info)
   {
      if (port < sys_info->ports.size)
         desc = &sys_info->ports.data[port];
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
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   struct menu_state *menu_st     = menu_state_get_ptr();

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

   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int setting_action_left_input_remap_port(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned port              = 0;
   settings_t *settings       = config_get_ptr();

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

   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
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

static bool setting_action_input_device_index_prevent(
      rarch_setting_t *setting, settings_t *settings, unsigned p, unsigned p_new)
{
   /* Prevent accidental port 1 device index removal */
   if (setting->index_offset == 0)
   {
      const char *name_cur = input_config_get_device_name(setting->index_offset);
      const char *name_new = input_config_get_device_name(p_new);
      if (     p == setting->index_offset
            && name_cur && *name_cur
            && (!name_new || !*name_new))
         return true;
   }
   return false;
}

static int setting_action_left_input_device_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;
   unsigned p_new            = 0;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_joypad_index[setting->index_offset];

   if (*p)
      p_new = *p - 1;
   else
      p_new = MAX_INPUT_DEVICES - 1;

   if (setting_action_input_device_index_prevent(setting, settings, *p, p_new))
      return 0;

   *p = p_new;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}

static int setting_action_left_input_device_reservation_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_device_reservation_type[setting->index_offset];

   if (*p)
      (*p)--;
   else
      *p = INPUT_DEVICE_RESERVATION_LAST - 1;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}


static int setting_action_left_input_mouse_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_mouse_index[setting->index_offset];

   if (*p)
      (*p)--;
   else
      *p = MAX_INPUT_DEVICES - 1;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}

static int setting_uint_action_left_custom_vp_width(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   settings_t                 *settings = config_get_ptr();
   video_viewport_t            *custom  = &settings->video_vp_custom;

   if (!settings || !av_info)
      return -1;

   if (custom->width <= setting->min)
      custom->width = setting->min;
   else if (settings->bools.video_scale_integer)
   {
      struct retro_game_geometry *geom = (struct retro_game_geometry*)
         &av_info->geometry;
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
      {
         if (custom->width > geom->base_height)
            custom->width -= geom->base_height;

         if (custom->width < geom->base_height)
            custom->width  = geom->base_height;
      }
      else
      {
         if (custom->width > geom->base_width)
            custom->width -= geom->base_width;

         if (custom->width < geom->base_width)
            custom->width  = geom->base_width;
      }
   }
   else
      custom->width -= 1;

   /* aspectratio_lut[ASPECT_RATIO_CUSTOM].value
    * is updated in general_write_handler() */

   return 0;
}

static int setting_uint_action_left_custom_vp_height(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   settings_t                 *settings = config_get_ptr();
   video_viewport_t            *custom  = &settings->video_vp_custom;

   if (!settings || !av_info)
      return -1;

   if (custom->height <= setting->min)
      custom->height = setting->min;
   else if (settings->bools.video_scale_integer)
   {
      struct retro_game_geometry *geom =
         (struct retro_game_geometry*)&av_info->geometry;
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
      {
         if (custom->height > geom->base_width)
            custom->height -= geom->base_width;

         if (custom->height < geom->base_width)
            custom->height  = geom->base_width;
      }
      else
      {
         if (custom->height > geom->base_height)
            custom->height -= geom->base_height;

         if (custom->height < geom->base_height)
            custom->height  = geom->base_height;
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
   if (audio_device_index < -1)
      audio_device_index = (int)(ptr->size - 1);

   if (audio_device_index < 0)
      strlcpy(setting->value.target.string,
            "", setting->size);
   else
      strlcpy(setting->value.target.string,
            ptr->elems[audio_device_index].data, setting->size);

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return 0;
}

#ifdef HAVE_MICROPHONE
static int setting_string_action_left_microphone_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int mic_device_index;
   struct string_list *ptr  = NULL;

   if (!microphone_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   mic_device_index = string_list_find_elem(
         ptr, setting->value.target.string) - 1;
   mic_device_index--;

   /* Reset index if needed */
   if (mic_device_index < -1)
      mic_device_index = (int)(ptr->size - 1);

   if (mic_device_index < 0)
      strlcpy(setting->value.target.string,
            "", setting->size);
   else
      strlcpy(setting->value.target.string,
            ptr->elems[mic_device_index].data, setting->size);

   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   return 0;
}
#endif
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
      while (    success
             &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
             && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
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
            while (    success
                   &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
                   && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
         }
      }
      else if (setting_is_protected_driver(setting))
      {
         /* If wraparound is disabled and if the driver is protected,
          * find the next driver in the array of drivers and keep finding more
          * next drivers while the driver is null or until there are no more next drivers. */
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv);
         while (    success
                &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
                && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
      }
   }

   if (setting->actions->change)
      setting->actions->change(setting);

   return 0;
}

#ifdef HAVE_NETWORKING
static int setting_string_action_ok_netplay_mitm_server(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   char enum_idx[16];

   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
      enum_idx,
      NULL, NULL, 0, idx, 0,
      ACTION_OK_DL_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER);

   return 0;
}

static int setting_string_action_left_netplay_mitm_server(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   size_t i;
   char *netplay_mitm_server;
   ssize_t offset = -1;

   if (!setting)
      return -1;

   netplay_mitm_server = setting->value.target.string;

   for (i = 0; i < ARRAY_SIZE(netplay_mitm_server_list); i++)
   {
      const mitm_server_t *server = &netplay_mitm_server_list[i];

      if (string_is_equal(server->name, netplay_mitm_server))
      {
         if (i > 0)
            offset = i - 1;
         else if (wraparound)
            offset = ARRAY_SIZE(netplay_mitm_server_list) - 1;
         else
            offset = i;

         break;
      }
   }

   if (offset < 0)
      offset = 0;

   strlcpy(netplay_mitm_server, netplay_mitm_server_list[offset].name,
      setting->size);

   return 0;
}

static int setting_string_action_right_netplay_mitm_server(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   size_t i;
   char *netplay_mitm_server;
   ssize_t offset = -1;

   if (!setting)
      return -1;

   netplay_mitm_server = setting->value.target.string;

   for (i = 0; i < ARRAY_SIZE(netplay_mitm_server_list); i++)
   {
      const mitm_server_t *server = &netplay_mitm_server_list[i];

      if (string_is_equal(server->name, netplay_mitm_server))
      {
         if (i < (ARRAY_SIZE(netplay_mitm_server_list) - 1))
            offset = i + 1;
         else if (wraparound)
            offset = 0;
         else
            offset = i;

         break;
      }
   }

   if (offset < 0)
      offset = ARRAY_SIZE(netplay_mitm_server_list) - 1;

   strlcpy(netplay_mitm_server, netplay_mitm_server_list[offset].name,
      setting->size);

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

static int setting_uint_action_right_custom_vp_width(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t                 *settings = config_get_ptr();
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   video_viewport_t            *custom  = &settings->video_vp_custom;

   if (!settings || !av_info)
      return -1;

   if (custom->width >= setting->max)
      custom->width = setting->max;
   else if (settings->bools.video_scale_integer)
   {
      struct retro_game_geometry *geom = (struct retro_game_geometry*)
         &av_info->geometry;
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

static int setting_uint_action_right_custom_vp_height(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   settings_t                 *settings = config_get_ptr();
   video_viewport_t            *custom  = &settings->video_vp_custom;

   if (!av_info)
      return -1;

   if (custom->height >= setting->max)
      custom->height = setting->max;
   else if (settings->bools.video_scale_integer)
   {
      struct retro_game_geometry *geom = (struct retro_game_geometry*)
         &av_info->geometry;
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
   audio_device_index = string_list_find_elem(ptr,setting->value.target.string) - 1;
   audio_device_index++;

   /* Reset index if needed */
   if (audio_device_index == (signed)ptr->size)
      audio_device_index = -1;

   if (audio_device_index < 0)
      strlcpy(setting->value.target.string,
            "", setting->size);
   else
      strlcpy(setting->value.target.string,
            ptr->elems[audio_device_index].data, setting->size);

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return 0;
}

#ifdef HAVE_MICROPHONE
static int setting_string_action_right_microphone_device(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int mic_device_index;
   struct string_list *ptr  = NULL;

   if (!microphone_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   mic_device_index = string_list_find_elem(ptr,setting->value.target.string) - 1;
   mic_device_index++;

   /* Reset index if needed */
   if (mic_device_index == (signed)ptr->size)
      mic_device_index = -1;

   if (mic_device_index < 0)
      strlcpy(setting->value.target.string,
            "", setting->size);
   else
      strlcpy(setting->value.target.string,
            ptr->elems[mic_device_index].data, setting->size);

   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   return 0;
}
#endif
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
      while (    success
             &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
             && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
   }

   if (!success)
   {
      settings_t                   *settings = config_get_ptr();
      bool menu_navigation_wraparound_enable = settings->bools.menu_navigation_wraparound_enable;

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
            while (    success
                   &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
                   && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv)));
         }
      }
      else if (setting_is_protected_driver(setting))
      {
         /* If wraparound is disabled and if the driver is protected,
          * find the previous driver in the array of drivers and keep finding more
          * previous drivers while the driver is null or until there are no more previous drivers. */
         success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv);
         while (    success
                &&  memcmp(drv.s, "null", 4) == 0 && drv.s[4] == '\0'
                && (success = driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv)));
      }
   }

   if (setting->actions->change)
      setting->actions->change(setting);

   return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_left_midi_input(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_inputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string) - 2;

      if (i == -1)
         i = (int)list->size - 1;
      if (i >= 0)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return -1;
}

static int setting_string_action_right_midi_input(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_inputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string);

      if (i == (int)list->size)
         i = 0;
      if (i >= 0 && i < (int)list->size)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return -1;
}

static int setting_string_action_left_midi_output(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_outputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string) - 2;

      if (i == -1)
         i = (int)list->size - 1;
      if (i >= 0)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return -1;
}

static int setting_string_action_right_midi_output(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   struct string_list *list = midi_driver_get_avail_outputs();

   if (list && list->size > 1)
   {
      int i = string_list_find_elem(list, setting->value.target.string);

      if (i == (int)list->size)
         i = 0;
      if (i >= 0 && i < (int)list->size)
      {
         strlcpy(setting->value.target.string,
               list->elems[i].data, setting->size);
         return 0;
      }
   }

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   return -1;
}
#endif

#ifdef HAVE_CHEATS
static size_t setting_get_string_representation_uint_cheat_exact(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_lt(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_gt(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_lte(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_gte(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_eq(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_neq(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL), len);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_eqplus(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_eqminus(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);
   return 0;
}

static size_t setting_get_string_representation_uint_cheat_browse_address(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len = 0;
   unsigned int address      = cheat_manager_state.browse_address;
   unsigned int address_mask = 0;
   unsigned int prev_val     = 0;
   unsigned int curr_val     = 0;

   if (setting)
      _len = snprintf(s, len, msg_hash_to_str(
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL),
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer);
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_BROWSE,
         cheat_manager_state.match_idx, &address, &address_mask,
         &prev_val, &curr_val);
   _len = snprintf(s, len, "Prev: %u Curr: %u", prev_val, curr_val);
   return _len;
}
#endif

static size_t setting_get_string_representation_video_swap_interval(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return 0;
   if (*setting->value.target.unsigned_integer == 0)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO), len);
   return snprintf(s, len, "%u", *setting->value.target.unsigned_integer);
}

static size_t setting_get_string_representation_black_frame_insertion(rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      unsigned value = *setting->value.target.unsigned_integer;

      if (value == 0)
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
      else
         return snprintf(s, len, "%u (%u/50, %u/60, %u/70 Hz)",
               value, ((value + 1) * 50), ((value + 1) * 60), ((value + 1) * 70));
   }
   return 0;
}

static size_t setting_get_string_representation_shader_subframes(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      unsigned value = *setting->value.target.unsigned_integer;

      if (value == 1)
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
      else
         return snprintf(s, len, "%u (%u/50, %u/60, %u/70 Hz)",
               value, (value * 50), (value * 60), (value * 70));
   }
   return 0;
}

static size_t setting_get_string_representation_video_frame_delay(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len                    = 0;
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   const char *label              = NULL;
   unsigned int value;
   file_list_t *menu_stack;

   if (!setting || !settings)
      return 0;

   value = *setting->value.target.unsigned_integer;

   menu_stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (menu_stack && menu_stack->size)
      label = menu_stack->list[menu_stack->size - 1].label;

   /* Non-automatic and dropdown list */
   if (     !settings->bools.video_frame_delay_auto
         || string_is_equal(label, MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_STR))
   {
      if (value == 0)
         _len = snprintf(s, len, "%s",
               (settings->bools.video_frame_delay_auto)
                  ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC)
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF));
      else if (value >= 20)
         _len = snprintf(s, len, "%u%% (%ums)",
               value,
               (unsigned)(1 / settings->floats.video_refresh_rate * 1000 * (value / 100.0f)));
      else
         _len = snprintf(s, len, "%ums", value);
      return _len;
   }

   /* Automatic delay also shows current effective delay */
   if (value == 0)
      _len = snprintf(s, len, "%s (%ums %s)",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC),
            video_st->frame_delay_effective,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE));
   else if (value >= 20)
      _len = snprintf(s, len, "%u%% (%ums, %ums %s)",
            value,
            (unsigned)(1 / settings->floats.video_refresh_rate * 1000 * (value / 100.0f)),
            video_st->frame_delay_effective,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE));
   else
      _len = snprintf(s, len, "%ums (%ums %s)",
            value,
            video_st->frame_delay_effective,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE));

   return _len;
}

static size_t setting_get_string_representation_uint_video_rotation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case VIDEO_ROTATION_NORMAL:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL),
                  len);
         case VIDEO_ROTATION_90_DEG:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG),
                  len);
         case VIDEO_ROTATION_180_DEG:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG),
                  len);
         case VIDEO_ROTATION_270_DEG:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_screen_orientation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case ORIENTATION_NORMAL:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL),
                  len);
         case ORIENTATION_VERTICAL:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL),
                  len);
         case ORIENTATION_FLIPPED:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED),
                  len);
         case ORIENTATION_FLIPPED_ROTATED:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_aspect_ratio_index(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return strlcpy(s,
            aspectratio_lut[*setting->value.target.unsigned_integer].name,
            len);
   return 0;
}

static size_t setting_get_string_representation_uint_crt_switch_resolutions(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case CRT_SWITCH_NONE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         case CRT_SWITCH_15KHZ:
            return strlcpy(s, "15 KHz", len);
         case CRT_SWITCH_31KHZ:
            return strlcpy(s, "31 KHz, Standard", len);
         case CRT_SWITCH_32_120:
            return strlcpy(s, "31 KHz, 120Hz", len);
         case CRT_SWITCH_INI:
            return strlcpy(s, "INI", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_audio_resampler_quality(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RESAMPLER_QUALITY_DONTCARE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE),
                  len);
         case RESAMPLER_QUALITY_LOWEST:
            return strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWEST),
                  len);
         case RESAMPLER_QUALITY_LOWER:
            return strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWER),
                  len);
         case RESAMPLER_QUALITY_HIGHER:
            return strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHER),
                  len);
         case RESAMPLER_QUALITY_HIGHEST:
            return strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHEST),
                  len);
         case RESAMPLER_QUALITY_NORMAL:
            return strlcpy(s, msg_hash_to_str(MSG_RESAMPLER_QUALITY_NORMAL),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_audio_format_negotiation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case AUDIO_FORMAT_NEGOTIATION_INT16:
            return strlcpy(s, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_AUDIO_FORMAT_NEGOTIATION_INT16), len);
         case AUDIO_FORMAT_NEGOTIATION_FLOAT:
            return strlcpy(s, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_AUDIO_FORMAT_NEGOTIATION_FLOAT), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_libretro_device(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned index_offset, device;
   const struct retro_controller_description *desc = NULL;
   const char *name              = NULL;
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
   if (!setting)
      return 0;
   index_offset                = setting->index_offset;
   device                      = input_config_get_device(index_offset);
   if (sys_info)
   {
      if (index_offset < sys_info->ports.size)
         desc = libretro_find_controller_description(
               &sys_info->ports.data[index_offset],
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
   if (name && *name)
      return strlcpy(s, name, len);
   return 0;
}

static size_t setting_get_string_representation_uint_analog_dpad_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   const char *name = NULL;

   if (!setting)
      return 0;

   switch (*setting->value.target.unsigned_integer)
   {
      default:
      case ANALOG_DPAD_NONE:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
         break;
      case ANALOG_DPAD_LSTICK:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG);
         break;
      case ANALOG_DPAD_LSTICK_FORCED:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED);
         break;
      case ANALOG_DPAD_RSTICK:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG);
         break;
      case ANALOG_DPAD_RSTICK_FORCED:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED);
         break;
      case ANALOG_DPAD_LRSTICK:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG);
         break;
      case ANALOG_DPAD_LRSTICK_FORCED:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG_FORCED);
         break;
      case ANALOG_DPAD_TWINSTICK:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG);
         break;
      case ANALOG_DPAD_TWINSTICK_FORCED:
         name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG_FORCED);
         break;
   }

   if (name && *name)
      return strlcpy(s, name, len);
   return 0;
}

static size_t setting_get_string_representation_uint_input_remap_port(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer + 1);
   return 0;
}

#ifdef HAVE_THREADS
static size_t setting_get_string_representation_uint_autosave_interval(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      if (*setting->value.target.unsigned_integer)
      {
         size_t _len = snprintf(s, len, "%u ", *setting->value.target.unsigned_integer);
         _len += strlcpy(s + _len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS), len - _len);
         return _len;
      }
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   }
   return 0;
}
#endif

#ifdef HAVE_BSV_MOVIE
static size_t setting_get_string_representation_uint_replay_checkpoint_interval(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return 0;
   if (*setting->value.target.unsigned_integer)
   {
      size_t _len = snprintf(s, len, "%u ", *setting->value.target.unsigned_integer);
      _len += strlcpy(s + _len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS), len - _len);
      return _len;
   }
   return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
}
#endif

#if defined(HAVE_NETWORKING)
static size_t setting_get_string_representation_netplay_mitm_server(
      rarch_setting_t *setting, char *s, size_t len)
{
   return 0;
}

static size_t setting_get_string_representation_netplay_share_digital(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RARCH_NETPLAY_SHARE_DIGITAL_NO_PREFERENCE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE), len);
         case RARCH_NETPLAY_SHARE_DIGITAL_OR:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR), len);
         case RARCH_NETPLAY_SHARE_DIGITAL_XOR:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR), len);
         case RARCH_NETPLAY_SHARE_DIGITAL_VOTE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE), len);
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_netplay_share_analog(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RARCH_NETPLAY_SHARE_ANALOG_NO_PREFERENCE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE), len);
         case RARCH_NETPLAY_SHARE_ANALOG_MAX:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX), len);
         case RARCH_NETPLAY_SHARE_ANALOG_AVERAGE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE), len);
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE), len);
      }
   }
   return 0;
}
#endif

static size_t setting_get_string_representation_gamepad_combo(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case INPUT_COMBO_NONE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
         case INPUT_COMBO_DOWN_Y_L_R:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R), len);
         case INPUT_COMBO_L3_R3:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L3_R3), len);
         case INPUT_COMBO_L1_R1_START_SELECT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L1_R1_START_SELECT), len);
         case INPUT_COMBO_START_SELECT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_SELECT), len);
         case INPUT_COMBO_L3_R:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L3_R), len);
         case INPUT_COMBO_L_R:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L_R), len);
         case INPUT_COMBO_HOLD_START:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HOLD_START), len);
         case INPUT_COMBO_HOLD_SELECT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HOLD_SELECT), len);
         case INPUT_COMBO_DOWN_SELECT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWN_SELECT), len);
         case INPUT_COMBO_L2_R2:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_L2_R2), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_turbo_mode(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case INPUT_TURBO_MODE_CLASSIC:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC), len);
         case INPUT_TURBO_MODE_CLASSIC_TOGGLE:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE), len);
         case INPUT_TURBO_MODE_SINGLEBUTTON:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON), len);
         case INPUT_TURBO_MODE_SINGLEBUTTON_HOLD:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_turbo_duty_cycle(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF), len);
         default:
            return snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_retropad_bind(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      int retro_id         = *setting->value.target.integer;

      if (retro_id < 0)
         return strlcpy(s, RARCH_NO_BIND, len);
      else
      {
         const struct retro_keybind *keyptr =
               &input_config_binds[0][retro_id];

         return strlcpy(s, msg_hash_to_str(keyptr->enum_idx), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_poll_type_behavior(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY), len);
         case 1:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL), len);
         case 2:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE), len);
      }
   }
   return 0;
}

#ifdef HAVE_RUNAHEAD
static size_t setting_get_string_representation_runahead_mode(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_RUNAHEAD_MODE_SINGLE_INSTANCE:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE), len);
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
         case MENU_RUNAHEAD_MODE_SECOND_INSTANCE:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE), len);
#endif
         case MENU_RUNAHEAD_MODE_PREEMPTIVE_FRAMES:
            return strlcpy(s, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES), len);
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
      }
   }
   return 0;
}
#endif

static size_t setting_get_string_representation_input_touch_scale(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
      return snprintf(s, len, "x%d", *setting->value.target.unsigned_integer);
   return 0;
}

#ifdef ANDROID
static size_t setting_get_string_representation_android_physical_keyboard(
        rarch_setting_t *setting, char *s, size_t len)
{
    if (setting)
    {
       const char *str = setting->value.target.string;
       if (   str[4] == ':'
           && str[9] == ' ')
          return strlcpy(s, &str[10], len);
       return strlcpy(s, str, len);
    }
    return 0;
}
#endif

#ifdef HAVE_LANGEXTRA
static size_t setting_get_string_representation_uint_user_language(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   size_t _len = 0;
   const char *modes[RETRO_LANGUAGE_LAST];
   uint32_t translated[RETRO_LANGUAGE_LAST];
#define LANG_DATA(STR) \
   modes[RETRO_LANGUAGE_##STR]      = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_##STR); \
   translated[RETRO_LANGUAGE_##STR] = LANGUAGE_PROGRESS_##STR##_TRANSLATED; \

   modes[RETRO_LANGUAGE_ENGLISH]                = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH);

   LANG_DATA(JAPANESE)
   LANG_DATA(FRENCH)
   LANG_DATA(SPANISH)
   LANG_DATA(GERMAN)
   LANG_DATA(ITALIAN)
   LANG_DATA(DUTCH)

   modes[RETRO_LANGUAGE_PORTUGUESE_BRAZIL]      = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL);
   translated[RETRO_LANGUAGE_PORTUGUESE_BRAZIL] = LANGUAGE_PROGRESS_PORTUGUESE_BRAZILIAN_TRANSLATED;
   modes[RETRO_LANGUAGE_PORTUGUESE_PORTUGAL]    = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL);
   translated[RETRO_LANGUAGE_PORTUGUESE_PORTUGAL] = LANGUAGE_PROGRESS_PORTUGUESE_TRANSLATED;

   LANG_DATA(RUSSIAN)
   LANG_DATA(KOREAN)
   LANG_DATA(CHINESE_TRADITIONAL)
   LANG_DATA(CHINESE_SIMPLIFIED)
   LANG_DATA(ESPERANTO)
   LANG_DATA(POLISH)
   LANG_DATA(VIETNAMESE)
   LANG_DATA(ARABIC)
   LANG_DATA(GREEK)
   LANG_DATA(TURKISH)
   LANG_DATA(SLOVAK)
   LANG_DATA(PERSIAN)
   LANG_DATA(HEBREW)
   LANG_DATA(ASTURIAN)
   LANG_DATA(FINNISH)
   LANG_DATA(INDONESIAN)
   LANG_DATA(SWEDISH)
   LANG_DATA(UKRAINIAN)
   LANG_DATA(CZECH)

   modes[RETRO_LANGUAGE_CATALAN_VALENCIA]      = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CATALAN_VALENCIA);
   translated[RETRO_LANGUAGE_CATALAN_VALENCIA] = LANGUAGE_PROGRESS_VALENCIAN_TRANSLATED;

   LANG_DATA(CATALAN)

   modes[RETRO_LANGUAGE_BRITISH_ENGLISH]       = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_BRITISH_ENGLISH);
   translated[RETRO_LANGUAGE_BRITISH_ENGLISH]  = LANGUAGE_PROGRESS_ENGLISH_UNITED_KINGDOM_TRANSLATED;

   LANG_DATA(HUNGARIAN)
   LANG_DATA(BELARUSIAN)
   LANG_DATA(GALICIAN)
   LANG_DATA(NORWEGIAN)
   LANG_DATA(IRISH)
   LANG_DATA(THAI)

   if (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE) == RETRO_LANGUAGE_ENGLISH)
      return strlcpy(s, modes[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)], len);
   {
      const char *rating = msg_hash_to_str(
            translated[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)] > 95 ? MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_95_PLUS :
            translated[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)] > 74 ? MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_75_PLUS :
            translated[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)] > 49 ? MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_50_PLUS :
            translated[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)] > 24 ? MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_PLUS :
            MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_MINUS);
      _len = snprintf(s, len, "%s [%s]",
            modes[*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)], rating);
   }
   return _len;
}
#endif

static size_t setting_get_string_representation_uint_libretro_log_level(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG), len);
         case 1:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO), len);
         case 2:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING), len);
         case 3:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_sensor_orientation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case 0:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_AUTO), len);
         case 1:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_0), len);
         case 2:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_90), len);
         case 3:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_180), len);
         case 4:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_270), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_quit_on_close_content(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case QUIT_ON_CLOSE_CONTENT_DISABLED:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
         case QUIT_ON_CLOSE_CONTENT_ENABLED:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
         case QUIT_ON_CLOSE_CONTENT_CLI:
            return strlcpy(s, "CLI", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_video_scale_integer_axis(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case VIDEO_SCALE_INTEGER_AXIS_Y_X:
            return strlcpy(s, "Y + X", len);
         case VIDEO_SCALE_INTEGER_AXIS_Y_XHALF:
            return strlcpy(s, "Y + X.5", len);
         case VIDEO_SCALE_INTEGER_AXIS_YHALF_XHALF:
            return strlcpy(s, "Y.5 + X.5", len);
         case VIDEO_SCALE_INTEGER_AXIS_X:
            return strlcpy(s, "X", len);
         case VIDEO_SCALE_INTEGER_AXIS_XHALF:
            return strlcpy(s, "X.5", len);
         case VIDEO_SCALE_INTEGER_AXIS_Y:
         default:
            return strlcpy(s, "Y", len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_video_scale_integer_scaling(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case VIDEO_SCALE_INTEGER_SCALING_OVERSCALE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE), len);
         case VIDEO_SCALE_INTEGER_SCALING_SMART:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART), len);
         case VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE:
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_playlist_show_history_icons(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case PLAYLIST_SHOW_HISTORY_ICONS_DEFAULT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
         case PLAYLIST_SHOW_HISTORY_ICONS_MAIN:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MAIN), len);
         case PLAYLIST_SHOW_HISTORY_ICONS_CONTENT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT), len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_screensaver_timeout(
      rarch_setting_t *setting, char *s, size_t len)
{
   size_t _len;
   if (!setting)
      return 0;
   if (*setting->value.target.unsigned_integer == 0)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   _len  = snprintf(s, len, "%u ", *setting->value.target.unsigned_integer);
   _len += strlcpy(s + _len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS), len - _len);
   return _len;
}

#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
static size_t setting_get_string_representation_uint_menu_screensaver_animation(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_SCREENSAVER_BLANK:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
         case MENU_SCREENSAVER_SNOW:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW),
                  len);
         case MENU_SCREENSAVER_STARFIELD:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD),
                  len);
         case MENU_SCREENSAVER_VORTEX:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX),
                  len);
      }
   }
   return 0;
}
#endif

static size_t setting_get_string_representation_uint_menu_remember_selection(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_REMEMBER_SELECTION_ALWAYS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS),
                  len);
         case MENU_REMEMBER_SELECTION_PLAYLISTS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS),
                  len);
         case MENU_REMEMBER_SELECTION_MAIN:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN),
                  len);
         case MENU_REMEMBER_SELECTION_OFF:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
      }
   }
   return 0;
}

static size_t setting_get_string_representation_uint_menu_startup_page(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case MENU_STARTUP_PAGE_MAIN_MENU:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MAIN_MENU),
                  len);
         case MENU_STARTUP_PAGE_HISTORY:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB),
                  len);
         case MENU_STARTUP_PAGE_FAVORITES:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB),
                  len);
         case MENU_STARTUP_PAGE_CONTENTLESS_CORES:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB),
                  len);
         case MENU_STARTUP_PAGE_EXPLORE:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_TAB),
                  len);
         case MENU_STARTUP_PAGE_PLAYLISTS:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                  len);
         case MENU_STARTUP_PAGE_LOAD_CONTENT:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                  len);
         case MENU_STARTUP_PAGE_START_DIRECTORY:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  len);
         case MENU_STARTUP_PAGE_DOWNLOADS:
            return strlcpy(s,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  len);
      }
   }
   return 0;
}

#ifdef HAVE_MIST
static size_t setting_get_string_representation_steam_rich_presence_format(
      rarch_setting_t *setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT), len);
         case STEAM_RICH_PRESENCE_FORMAT_CORE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE), len);
         case STEAM_RICH_PRESENCE_FORMAT_SYSTEM:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM), len);
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM), len);
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE), len);
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE), len);
         case STEAM_RICH_PRESENCE_FORMAT_NONE:
         default:
            return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
      }
   }
   return 0;
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
      (*list)[idx].ui_type        = ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX;

   (*list)[idx].min               = min;
   (*list)[idx].step              = step;
   (*list)[idx].max               = max;
   if (enforce_minrange_enable)
      (*list)[idx].flags         |= SD_FLAG_ENFORCE_MINRANGE;
   if (enforce_maxrange_enable)
      (*list)[idx].flags         |= SD_FLAG_ENFORCE_MAXRANGE;
   (*list)[idx].flags            |= SD_FLAG_HAS_RANGE;
}

int menu_setting_generic(rarch_setting_t *setting, size_t idx, bool wraparound)
{
   uint32_t flags = setting->flags;
   if (setting_generic_action_ok_default(setting, idx, wraparound) != 0)
      return -1;

   if (setting->actions->change)
      setting->actions->change(setting);

   if ((flags & SD_FLAG_EXIT) && (flags & SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED))
   {
      setting->flags &= ~SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED;
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
            struct menu_state *menu_st    = menu_state_get_ptr();
            menu_list_t *menu_list        = menu_st->entries.list;
            file_list_t *menu_stack       = MENU_LIST_GET(menu_list, 0);
            const char *name              = setting->name;
            size_t selection              = menu_st->selection_ptr;

            menu_displaylist_info_init(&info);

            info.path                     = strdup(setting->default_value.string);
            info.label                    = strdup(name);
            info.type                     = type;
            info.directory_ptr            = selection;
            info.list                     = menu_stack;

            /* Menu background image */
            if (  string_is_equal(info.label,
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_WALLPAPER))
               && settings->paths.path_menu_wallpaper[0] != '\0')
            {
               free(info.path);
               info.path = strdup(settings->paths.path_menu_wallpaper);
            }

            /* Browse basedir instead and set selection to file if available */
            if (info.path && info.path[0] != '\0' && !path_is_directory(info.path))
            {
               const char *selection_path = path_basename(info.path);
               if (selection_path && selection_path[0] != '\0')
                  menu_driver_set_pending_selection(selection_path);
               path_basedir(info.path);
            }

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info, settings))
               menu_displaylist_process(&info);

            menu_displaylist_info_free(&info);
         }
         /* fall-through. */
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_SIZE:
      case ST_FLOAT:
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_DIR:
      case ST_BIND:
      case ST_ACTION:
         {
            int ret                       = -1;
            struct menu_state *menu_st    = menu_state_get_ptr();
            size_t selection              = menu_st->selection_ptr;
            switch (action)
            {
               case MENU_ACTION_UP:
               case MENU_ACTION_DOWN:
                  /* No setting has ever populated an up or down
                   * handler; the fields are gone and the actions
                   * fall through unhandled as they always did. */
                  break;
               case MENU_ACTION_LEFT:
                  if (setting->actions->left)
                     ret = setting->actions->left(setting, selection, false);
                  break;
               case MENU_ACTION_RIGHT:
                  if (setting->actions->right)
                     ret = setting->actions->right(setting, selection, false);
                  break;
               case MENU_ACTION_SELECT:
                  if (setting->actions->sel)
                     ret = setting->actions->sel(setting, selection, true);
                  break;
               case MENU_ACTION_OK:
                  if (setting->actions->ok)
                     ret = setting->actions->ok(setting, selection, false);
                  break;
               case MENU_ACTION_CANCEL:
                  /* Never populated; falls through unhandled as it
                   * always did. */
                  break;
               case MENU_ACTION_START:
                  if (setting->actions->start)
                     ret = setting->actions->start(setting);
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

static const enum settings_list_type settings_list_build_order[] =
   {
      SETTINGS_LIST_MAIN_MENU,
      SETTINGS_LIST_DRIVERS,
      SETTINGS_LIST_CORE,
      SETTINGS_LIST_CONFIGURATION,
      SETTINGS_LIST_LOGGING,
      SETTINGS_LIST_SAVING,
      SETTINGS_LIST_CLOUD_SYNC,
      SETTINGS_LIST_REWIND,
      SETTINGS_LIST_CHEAT_DETAILS,
      SETTINGS_LIST_CHEAT_SEARCH,
      SETTINGS_LIST_CHEATS,
      SETTINGS_LIST_VIDEO,
      SETTINGS_LIST_CRT_SWITCHRES,
      SETTINGS_LIST_AUDIO,
#ifdef HAVE_MICROPHONE
      SETTINGS_LIST_MICROPHONE,
#endif
      SETTINGS_LIST_INPUT,
      SETTINGS_LIST_INPUT_TURBO_FIRE,
      SETTINGS_LIST_INPUT_HOTKEY,
      SETTINGS_LIST_RECORDING,
      SETTINGS_LIST_FRAME_THROTTLING,
      SETTINGS_LIST_FRAME_TIME_COUNTER,
      SETTINGS_LIST_ONSCREEN_NOTIFICATIONS,
      SETTINGS_LIST_OVERLAY,
      SETTINGS_LIST_OSK_OVERLAY,
      SETTINGS_LIST_OVERLAY_MOUSE,
      SETTINGS_LIST_OVERLAY_LIGHTGUN,
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
      SETTINGS_LIST_CHEEVOS_APPEARANCE,
      SETTINGS_LIST_CHEEVOS_VISIBILITY,
      SETTINGS_LIST_CORE_UPDATER,
      SETTINGS_LIST_NETPLAY,
      SETTINGS_LIST_LAKKA_SERVICES,
#ifdef HAVE_LAKKA_SWITCH
      SETTINGS_LIST_LAKKA_SWITCH_OPTIONS,
#endif
      SETTINGS_LIST_USER,
      SETTINGS_LIST_USER_ACCOUNTS,
      SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
      SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE,
      SETTINGS_LIST_USER_ACCOUNTS_TWITCH,
      SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK,
      SETTINGS_LIST_USER_ACCOUNTS_KICK,
      SETTINGS_LIST_DIRECTORY,
      SETTINGS_LIST_PRIVACY,
      SETTINGS_LIST_MIDI,
#ifdef HAVE_MIST
      SETTINGS_LIST_STEAM,
#endif
#ifdef HAVE_SMBCLIENT
      SETTINGS_LIST_SMBCLIENT,
#endif
      SETTINGS_LIST_MANUAL_CONTENT_SCAN
   };

static bool setting_append_list(settings_t *settings, global_t *global,
      enum settings_list_type type, rarch_setting_t **list,
      rarch_setting_info_t *list_info, const char *parent_group);

/* Stage three of the menu memory work: the settings list is no
 * longer kept resident.  menu_setting_new() builds the full list
 * once, learns which builder produces each enum and each name, then
 * frees it; builders are rebuilt on demand into a per-session cache
 * when a lookup first needs them.  Standalone builds were proven
 * byte-identical to in-sequence builds for every builder, so the
 * cache serves exactly the rows the flat list held.  Nothing is
 * evicted within a session: pointers handed out live until menu
 * deinit, the same lifetime the flat list gave them. */
static uint16_t         settings_lazy_bounds[64];
static unsigned         settings_lazy_nbuilders;
static rarch_setting_t *settings_lazy_lists[64];
static uint8_t         *settings_lazy_enum2b;    /* MSG_LAST slots: builder+1 */
/* name lookup: hash-sorted parallel arrays, five bytes per entry;
 * ties keep walk order via a stable insertion sort at learn time */
static uint32_t *settings_lazy_name_hash;
static uint8_t  *settings_lazy_name_builder;
static uint32_t  settings_lazy_names_n;
static rarch_setting_t *settings_lazy_token;

static uint32_t settings_lazy_hash(const char *s)
{
   uint32_t h = 5381;
   while (*s)
      h = ((h << 5) + h) ^ (uint8_t)*s++;
   return h;
}

static void settings_lazy_free(void)
{
   unsigned k;
   for (k = 0; k < 64; k++)
   {
      if (settings_lazy_lists[k])
      {
         menu_setting_free(settings_lazy_lists[k]);
         free(settings_lazy_lists[k]);
         settings_lazy_lists[k] = NULL;
      }
   }
   if (settings_lazy_enum2b)
      free(settings_lazy_enum2b);
   if (settings_lazy_name_hash)
      free(settings_lazy_name_hash);
   if (settings_lazy_name_builder)
      free(settings_lazy_name_builder);
   settings_lazy_enum2b       = NULL;
   settings_lazy_name_hash    = NULL;
   settings_lazy_name_builder = NULL;
   settings_lazy_names_n      = 0;
   settings_lazy_token   = NULL;
}

static void settings_lazy_learn(rarch_setting_t *list)
{
   uint32_t i = 0;
   unsigned k = 0;
   settings_lazy_free();
   settings_lazy_enum2b = (uint8_t*)calloc(MSG_LAST, sizeof(uint8_t));
   settings_lazy_name_hash    = (uint32_t*)malloc(0xFFFF * sizeof(uint32_t));
   settings_lazy_name_builder = (uint8_t*)malloc(0xFFFF);
   if (!settings_lazy_enum2b || !settings_lazy_name_hash
         || !settings_lazy_name_builder)
   {
      settings_lazy_free();
      return;
   }
   for (; list[i].type != ST_NONE; i++)
   {
      while (k + 1 < settings_lazy_nbuilders && i >= settings_lazy_bounds[k])
         k++;
      if (list[i].type > ST_GROUP)
         continue;
      if (     list[i].enum_idx != MSG_UNKNOWN
            && (uint32_t)list[i].enum_idx < (uint32_t)MSG_LAST
            && !settings_lazy_enum2b[list[i].enum_idx])
         settings_lazy_enum2b[list[i].enum_idx] = (uint8_t)(k + 1);
      if (list[i].name && settings_lazy_names_n < 0xFFFF)
      {
         /* stable insertion keeps walk order among equal hashes */
         uint32_t h = settings_lazy_hash(list[i].name);
         uint32_t p = settings_lazy_names_n;
         while (p > 0 && settings_lazy_name_hash[p - 1] > h)
         {
            settings_lazy_name_hash[p]    = settings_lazy_name_hash[p - 1];
            settings_lazy_name_builder[p] = settings_lazy_name_builder[p - 1];
            p--;
         }
         settings_lazy_name_hash[p]    = h;
         settings_lazy_name_builder[p] = (uint8_t)k;
         settings_lazy_names_n++;
      }
   }
   settings_lazy_name_hash    = (uint32_t*)realloc(settings_lazy_name_hash,
         settings_lazy_names_n * sizeof(uint32_t));
   settings_lazy_name_builder = (uint8_t*)realloc(settings_lazy_name_builder,
         settings_lazy_names_n ? settings_lazy_names_n : 1);
}

static rarch_setting_t *settings_lazy_get(unsigned k);

/**
 * menu_setting_find:
 * @label              : name of setting to search for
 *
 * Search for a setting with a specified name (@label).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t *menu_setting_find(const char *label)
{
   uint32_t h, lo, hi;

   if (!label || !settings_lazy_name_hash)
      return NULL;

   h  = settings_lazy_hash(label);
   lo = 0;
   hi = settings_lazy_names_n;
   while (lo < hi)
   {
      uint32_t mid = lo + ((hi - lo) >> 1);
      if (settings_lazy_name_hash[mid] < h)
         lo = mid + 1;
      else
         hi = mid;
   }
   for (; lo < settings_lazy_names_n && settings_lazy_name_hash[lo] == h; lo++)
   {
      rarch_setting_t *sl = settings_lazy_get(settings_lazy_name_builder[lo]);
      rarch_setting_t *setting;
      if (!sl)
         continue;
      for (setting = sl; setting->type != ST_NONE; setting++)
      {
         if (  setting->type <= ST_GROUP
            && setting->name
            && string_is_equal(label, setting->name))
         {
            if (!setting->short_description || !*setting->short_description)
               return NULL;

            if (setting->actions->read)
               setting->actions->read(setting);

            return setting;
         }
      }
   }

   return NULL;
}

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums enum_idx)
{
   rarch_setting_t *sl;
   rarch_setting_t *setting;
   uint8_t b;

   if (enum_idx == 0 || !settings_lazy_enum2b)
      return NULL;
   if ((uint32_t)enum_idx >= (uint32_t)MSG_LAST)
      return NULL;

   if (!(b = settings_lazy_enum2b[enum_idx]))
      return NULL;
   if (!(sl = settings_lazy_get((unsigned)b - 1)))
      return NULL;

   for (setting = sl; setting->type != ST_NONE; setting++)
   {
      if (     setting->type <= ST_GROUP
            && setting->enum_idx == enum_idx)
      {
         if (!setting->short_description || !*setting->short_description)
            return NULL;

         if (setting->actions->read)
            setting->actions->read(setting);

         return setting;
      }
   }

   return NULL;
}

int menu_setting_set(unsigned type, unsigned action, bool wraparound)
{
   int ret                    = 0;
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   file_list_t *selection_buf = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
   size_t selection           = menu_st->selection_ptr;
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
static int setting_action_start_input_device_index(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   configuration_set_uint(settings,
         settings->uints.input_joypad_index[setting->index_offset],
         setting->index_offset);
   return 0;
}

static int setting_action_start_input_device_reservation_type(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   configuration_set_uint(settings,
         settings->uints.input_device_reservation_type[setting->index_offset],
         INPUT_DEVICE_RESERVATION_NONE);
   return 0;
}

static int setting_action_start_input_device_reserved_device_name(
   rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   configuration_set_string(settings,
         settings->arrays.input_reserved_devices[setting->index_offset],
         "");

   command_event(CMD_EVENT_REINIT, NULL);
   return 0;
}

static int setting_action_start_custom_vp_width(rarch_setting_t *setting)
{
   video_viewport_t vp;
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   settings_t                 *settings = config_get_ptr();
   video_viewport_t            *custom  = &settings->video_vp_custom;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->bools.video_scale_integer)
   {
      struct retro_game_geometry *geom = (struct retro_game_geometry*)
         &av_info->geometry;
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

static int setting_action_start_custom_vp_height(rarch_setting_t *setting)
{
   video_viewport_t vp;
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   settings_t                 *settings = config_get_ptr();
   video_viewport_t            *custom  = &settings->video_vp_custom;
   bool video_scale_integer             = settings->bools.video_scale_integer;

   if (!av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (video_scale_integer)
   {
      struct retro_game_geometry *geom = (struct retro_game_geometry*)
         &av_info->geometry;
      unsigned int rotation = retroarch_get_rotation();
      if (rotation % 2)
         custom->height = ((custom->height + geom->base_width - 1) /
               geom->base_width) * geom->base_width;
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
   unsigned port;
   settings_t *settings                    = config_get_ptr();
   struct menu_state *menu_st              = menu_state_get_ptr();

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

   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int setting_action_start_video_refresh_rate_auto(
      rarch_setting_t *setting)
{
   video_driver_monitor_reset();
   return 0;
}

static int setting_action_start_video_refresh_rate_polled(
      rarch_setting_t *setting)
{
   /* Relay action to ok to prevent duplicate notifications */
   return setting_action_ok_video_refresh_rate_polled(setting, 0, false);
}

static int setting_action_start_input_mouse_index(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   configuration_set_uint(settings,
         settings->uints.input_mouse_index[setting->index_offset],
         setting->index_offset);
   return 0;
}

/**
 ******* ACTION TOGGLE CALLBACK FUNCTIONS *******
**/

static int setting_action_right_analog_dpad_mode(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t *settings = config_get_ptr();
   unsigned port        = 0;

   if (!setting)
      return -1;

   port = setting->index_offset;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   settings->uints.input_analog_dpad_mode[port] =
      (settings->uints.input_analog_dpad_mode[port] + 1)
      % ANALOG_DPAD_LAST;

   return 0;
}

static int setting_action_right_libretro_device_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   struct menu_state *menu_st     = menu_state_get_ptr();

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

   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int setting_action_right_input_remap_port(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   unsigned port              = 0;
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();

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

   menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                              |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int setting_action_right_input_device_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;
   unsigned p_new            = 0;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_joypad_index[setting->index_offset];

   if (*p < MAX_INPUT_DEVICES - 1)
      p_new = *p + 1;
   else
      p_new = 0;

   if (setting_action_input_device_index_prevent(setting, settings, *p, p_new))
      return 0;

   *p = p_new;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}

static int setting_action_right_input_device_reservation_type(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_device_reservation_type[setting->index_offset];

   if (*p < INPUT_DEVICE_RESERVATION_LAST - 1)
      (*p)++;
   else
      *p = 0;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}

static int setting_action_right_input_mouse_index(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   settings_t      *settings = config_get_ptr();
   unsigned *p               = NULL;

   if (!setting || !settings)
      return -1;

   p = &settings->uints.input_mouse_index[setting->index_offset];

   if (*p < MAX_INPUT_DEVICES - 1)
      (*p)++;
   else
      *p = 0;

   settings->flags |= SETTINGS_FLG_MODIFIED;
   return 0;
}

#ifdef HAVE_SMBCLIENT
static size_t setting_get_string_representation_smb_auth(
   rarch_setting_t *setting, char *s, size_t len)
{
   unsigned val;

   if (!setting || !setting->value.target.integer)
      return 0;

   val = *setting->value.target.integer;

   switch (val)
   {
      case RETRO_SMB2_SEC_NTLMSSP: /* SMB2_SEC_NTLMSSP */
         return strlcpy(s, "NTLMSSP", len);
      case RETRO_SMB2_SEC_KRB5: /* SMB2_SEC_KRB5 */
         return strlcpy(s, "Kerberos", len);
      default:
         return strlcpy(s, "KRB if available, NTLM if not", len);
   }
}

static size_t setting_get_string_representation_smb_password(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return 0;

   if (!setting->value.target.string || !*setting->value.target.string)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
   else
   {
      size_t i;
      size_t pass_len = strlen(setting->value.target.string);

      for (i = 0; i < pass_len && i < (len - 1); i++)
         s[i] = '*';
      s[i] = '\0';
   }

   return 0;
}
#endif

/**
 ******* ACTION OK CALLBACK FUNCTIONS *******
**/

static size_t
setting_get_string_representation_st_float_video_refresh_rate_polled(
      rarch_setting_t *setting, char *s, size_t len)
{
    return snprintf(s, len, "%.3f Hz", video_driver_get_refresh_rate());
}

static size_t
setting_get_string_representation_st_float_video_refresh_rate_auto(
      rarch_setting_t *setting, char *s, size_t len)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;
   if (!setting)
      return 0;
   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      gfx_animation_t *p_anim   = anim_get_ptr();
      size_t _len = snprintf(s, len, "%.3f Hz (%.1f%% dev, %u samples)",
            video_refresh_rate, 100.0 * deviation, sample_points);
      GFX_ANIMATION_SET_ACTIVE(p_anim);
      return _len;
   }
   return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

#ifdef HAVE_LIBNX
static size_t get_string_representation_split_joycon(
      rarch_setting_t *setting, char *s, size_t len)
{
   settings_t *settings  = config_get_ptr();
   unsigned index_offset = setting->index_offset;
   unsigned map          = settings->uints.input_split_joycon[index_offset];
   if (map == 0)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
}
#endif

static size_t get_string_representation_input_device_index(
      rarch_setting_t *setting, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   size_t _len          = 0;
   unsigned map         = 0;

   if (!setting || !settings)
      return 0;

   map = settings->uints.input_joypad_index[setting->index_offset];

   if (map < MAX_INPUT_DEVICES)
   {
      const char *device_name = input_config_get_device_display_name(map)
            ? input_config_get_device_display_name(map)
            : input_config_get_device_name(map);

      _len = snprintf(s, len,
            "#%u: %s",
            map + 1,
            (device_name && *device_name)
                  ? device_name
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

      if (device_name && *device_name)
      {
         unsigned idx = input_config_get_device_name_index(map);

         /* If idx is non-zero, it's part of a set */
         if (idx > 0)
            _len += snprintf(s + _len, len - _len, " (%u)", idx);
      }
   }

   if (!s || !*s)
      _len = strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
   return _len;
}

static size_t get_string_representation_input_device_reservation_type(
      rarch_setting_t *setting, char *s, size_t len)
{
   settings_t      *settings = config_get_ptr();
   unsigned map              = 0;
   if (!setting || !settings)
      return 0;
   map = settings->uints.input_device_reservation_type[setting->index_offset];
   if (map == INPUT_DEVICE_RESERVATION_NONE)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE), len);
   else if (map == INPUT_DEVICE_RESERVATION_PREFERRED)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED), len);
   else if (map == INPUT_DEVICE_RESERVATION_RESERVED)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED), len);
   return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
}

static size_t setting_get_string_representation_input_device_reserved_device_name(rarch_setting_t *setting, char *s, size_t len)
{
   const char *str;
   if (!setting)
      return 0;
   if (!setting->value.target.string || !*setting->value.target.string)
      return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
   str = setting->value.target.string;
   if (   ((str[0] >= '0' && str[0] <= '9') || (str[0] >= 'a' && str[0] <= 'f') || (str[0] >= 'A' && str[0] <= 'F'))
       && ((str[1] >= '0' && str[1] <= '9') || (str[1] >= 'a' && str[1] <= 'f') || (str[1] >= 'A' && str[1] <= 'F'))
       && ((str[2] >= '0' && str[2] <= '9') || (str[2] >= 'a' && str[2] <= 'f') || (str[2] >= 'A' && str[2] <= 'F'))
       && ((str[3] >= '0' && str[3] <= '9') || (str[3] >= 'a' && str[3] <= 'f') || (str[3] >= 'A' && str[3] <= 'F'))
       && str[4] == ':'
       && ((str[5] >= '0' && str[5] <= '9') || (str[5] >= 'a' && str[5] <= 'f') || (str[5] >= 'A' && str[5] <= 'F'))
       && ((str[6] >= '0' && str[6] <= '9') || (str[6] >= 'a' && str[6] <= 'f') || (str[6] >= 'A' && str[6] <= 'F'))
       && ((str[7] >= '0' && str[7] <= '9') || (str[7] >= 'a' && str[7] <= 'f') || (str[7] >= 'A' && str[7] <= 'F'))
       && ((str[8] >= '0' && str[8] <= '9') || (str[8] >= 'a' && str[8] <= 'f') || (str[8] >= 'A' && str[8] <= 'F'))
       && str[9] == ' ')
      return strlcpy(s, &str[10], len);
   return strlcpy(s, str, len);
}

static size_t get_string_representation_input_mouse_index(
      rarch_setting_t *setting, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   size_t _len          = 0;
   unsigned map         = 0;

   if (!setting || !settings)
      return 0;

   map = settings->uints.input_mouse_index[setting->index_offset];

   if (map < MAX_INPUT_DEVICES)
   {
      const char *device_name = input_config_get_mouse_display_name(map);

      _len = snprintf(s, len,
            "#%u: %s",
            map + 1,
            (device_name && *device_name)
                  ? device_name
                  : (map > 0)
                        ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
                        : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE));
   }

   if (!s || !*s)
      _len = strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
   return _len;
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
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
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
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         *setting->value.target.fraction = settings->floats.video_refresh_rate;
         break;
#ifdef ANDROID
      case MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD:
         setting->value.target.string = settings->arrays.input_android_physical_keyboard;
         break;
#endif
      default:
         break;
   }
}

static enum event_command write_handler_get_cmd(rarch_setting_t *setting)
{
   if (setting && setting->cmd_trigger_idx != CMD_EVENT_NONE)
   {
      uint32_t flags = setting->flags;
      if (flags & SD_FLAG_EXIT)
         if (*setting->value.target.boolean)
            *setting->value.target.boolean = false;

      if (     (flags & SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED)
            || (flags & SD_FLAG_CMD_APPLY_AUTO))
         return setting->cmd_trigger_idx;
   }
   return CMD_EVENT_NONE;
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

   if (rarch_cmd || (setting->flags & SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED))
      command_event(rarch_cmd, NULL);
}

static void general_write_handler(rarch_setting_t *setting)
{
   enum event_command rarch_cmd = CMD_EVENT_NONE;
   settings_t *settings         = config_get_ptr();

   if (!setting)
      return;

   rarch_cmd                    = write_handler_get_cmd(setting);

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         video_shader_toggle(settings, true);
#endif
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         if (*setting->value.target.boolean)
            task_queue_set_threaded();
         else
            task_queue_unset_threaded();
         break;
#ifndef HAVE_LAKKA
      case MENU_ENUM_LABEL_GAMEMODE_ENABLE:
         if (frontend_driver_has_gamemode())
         {
            bool on = *setting->value.target.boolean;

            if (!frontend_driver_set_gamemode(on) && on)
            {
#ifdef __linux__
               const char *_msg = msg_hash_to_str(MSG_FAILED_TO_ENTER_GAMEMODE_LINUX);
#else
               const char *_msg = msg_hash_to_str(MSG_FAILED_TO_ENTER_GAMEMODE);
#endif
               /* If we failed to enable game mode, display
                * a notification and force disable the feature */
               runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               configuration_set_bool(settings,
                     settings->bools.gamemode_enable, false);
            }
         }
         break;
#endif /*HAVE_LAKKA*/
     case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         core_set_poll_type(*setting->value.target.integer);
         break;
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && defined(HAVE_MENU)
      case MENU_ENUM_LABEL_USER_LANGUAGE:
         /* The native Win32 menubar bakes translated strings at
          * menu-creation time (popup headers in particular have no
          * resource ID and so are not walked by win32_localize_menu
          * afterwards). Rebuild the whole menubar so every string -
          * including popup headers - reflects the new language. */
         win32_menubar_rebuild();
         break;
#endif
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         {
            video_driver_state_t *video_st       = video_state_get_ptr();
            struct retro_system_av_info *av_info = &video_st->av_info;
            struct video_viewport *custom_vp     = &settings->video_vp_custom;

            if (*setting->value.target.boolean)
            {
               unsigned rotation                = retroarch_get_rotation();
               unsigned base_width              = 0;
               unsigned base_height             = 0;
               struct retro_game_geometry *geom = (struct retro_game_geometry*)&av_info->geometry;

               custom_vp->x         = 0;
               custom_vp->y         = 0;

               {
                  unsigned cache_w  = 0;
                  unsigned cache_h  = 0;
                  video_driver_cached_frame_info(&cache_w, &cache_h, NULL, NULL);
                  base_width        = (geom->base_width)  ? geom->base_width  : cache_w;
                  base_height       = (geom->base_height) ? geom->base_height : cache_h;
               }

               if (base_width <= 4 || base_height <= 4)
               {
                  base_width        = (rotation % 2) ? 240 : 320;
                  base_height       = (rotation % 2) ? 320 : 240;
               }

               if (rotation % 2)
               {
                  custom_vp->width  = ((custom_vp->width  + base_height - 1) / base_height)  * base_height;
                  custom_vp->height = ((custom_vp->height + base_width - 1)  / base_width)   * base_width;
               }
               else
               {
                  custom_vp->width  = ((custom_vp->width  + base_width - 1)  / base_width)  * base_width;
                  custom_vp->height = ((custom_vp->height + base_height - 1) / base_height) * base_height;
               }

               aspectratio_lut[ASPECT_RATIO_CUSTOM].value = (float)custom_vp->width / custom_vp->height;
            }
         }
         break;
      case MENU_ENUM_LABEL_HELP:
         if (*setting->value.target.boolean)
         {
            menu_displaylist_info_t info;
            struct menu_state *menu_st   = menu_state_get_ptr();
            menu_list_t *menu_list       = menu_st->entries.list;
            file_list_t *menu_stack      = MENU_LIST_GET(menu_list, 0);

            menu_displaylist_info_init(&info);

            info.enum_idx                = MENU_ENUM_LABEL_HELP;
            info.label                   = strdup(MENU_ENUM_LABEL_HELP_STR);
            info.list                    = menu_stack;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info, settings))
               menu_displaylist_process(&info);
            menu_displaylist_info_free(&info);
            *setting->value.target.boolean = false;
            if (setting->actions->change)
               setting->actions->change(setting);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         /* If enabling BFI, auto disable other sync settings
            that do not work together with BFI */
         if (*setting->value.target.unsigned_integer > 0)
         {
            configuration_set_uint(settings,
               settings->uints.video_swap_interval,
               0);
            configuration_set_uint(settings,
               settings->uints.video_frame_delay,
               0);
            configuration_set_bool(settings,
               settings->bools.video_frame_delay_auto,
               0);
            configuration_set_bool(settings,
               settings->bools.vrr_runloop_enable,
               0);
            configuration_set_uint(settings,
               settings->uints.video_shader_subframes,
               1);

             /* Set reasonable default for dark frames for current BFI value.
                Even results OR odd 60Hz multiples should be mostly immune
                to LCD voltage retention.
                Nothing to be done for 120hz except phase retention usage
                if needed. */
            if (*setting->value.target.unsigned_integer == 1)
            {
               configuration_set_uint(settings,
                  settings->uints.video_bfi_dark_frames,
                  1);
            }
            /* Odd 60hz multiples, safe to nudge result over 50% for more clarity by default */
            else if ((*setting->value.target.unsigned_integer + 1) % 2 != 0)
            {
               configuration_set_uint(settings,
                  settings->uints.video_bfi_dark_frames,
                  ((*setting->value.target.unsigned_integer + 1) / 2) + 1);
            }
            /* Odd result on even multiple, bump result up one */
            else if (((*setting->value.target.unsigned_integer + 1) / 2) % 2 != 0)
            {
               configuration_set_uint(settings,
                  settings->uints.video_bfi_dark_frames,
                  ((*setting->value.target.unsigned_integer + 1) / 2) + 1);
            }
            /* Even result on even multiple, leave alone */
            else
            {
               configuration_set_uint(settings,
                  settings->uints.video_bfi_dark_frames,
                  ((*setting->value.target.unsigned_integer + 1) / 2));
            }
         }
#ifdef HAVE_CHEEVOS
         rcheevos_validate_config_settings();
#endif
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_SUBFRAMES:
         /* If enabling BFI, auto disable other sync settings
            that do not work together with subframes */
         if (*setting->value.target.unsigned_integer > 1)
         {
            configuration_set_uint(settings,
               settings->uints.video_swap_interval,
               0);
            configuration_set_uint(settings,
               settings->uints.video_frame_delay,
               0);
            configuration_set_bool(settings,
               settings->bools.video_frame_delay_auto,
               0);
            configuration_set_uint(settings,
               settings->uints.video_black_frame_insertion,
               0);
         }
#ifdef HAVE_CHEEVOS
         rcheevos_validate_config_settings();
#endif
         break;
      case MENU_ENUM_LABEL_VIDEO_BFI_DARK_FRAMES:
         /* Limit choice to max possible for current BFI Hz Setting */
         if (*setting->value.target.unsigned_integer > settings->uints.video_black_frame_insertion)
            configuration_set_uint(settings,
               settings->uints.video_bfi_dark_frames,
               settings->uints.video_black_frame_insertion);
         break;
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         /* Recalibrate frame delay */
         video_state_get_ptr()->frame_delay_target = 0;
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO:
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
         /* BFI or shader subframes doesn't play nice with any of these */
         configuration_set_bool(settings,
            settings->uints.video_black_frame_insertion,
            0);
#ifdef HAVE_CHEEVOS
         rcheevos_validate_config_settings();
#endif
         /* Sync to Exact Content Framerate only changes the audio/video
          * system rates (audio locks to the exact content rate instead of the
          * display refresh) plus the per-frame swap interval, so re-adjust the
          * rates in place rather than reinitialising the drivers -- the runloop
          * reads vrr_runloop_enable every frame. */
         if (setting->enum_idx == MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE)
            driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE,
                  &settings->floats.video_refresh_rate);
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
            enum dingux_refresh_rate
               current_refresh_rate      = DINGUX_REFRESH_RATE_60HZ;
            enum dingux_refresh_rate
               target_refresh_rate       =
                  (enum dingux_refresh_rate)settings->uints.video_dingux_refresh_rate;
            bool refresh_rate_valid      = false;

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
         settings->flags                  |= SETTINGS_FLG_MODIFIED;
         settings->uints.video_scale       = *setting->value.target.unsigned_integer;

         if (!settings->bools.video_fullscreen)
            rarch_cmd = CMD_EVENT_REINIT;
         break;
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         {
            struct menu_state *menu_st = menu_state_get_ptr();
            /* Toggling rewind support shows or hides the dependent
             * rewind items (granularity, buffer size, ...), so force
             * the page to rebuild immediately. The setting value is
             * already written by the framework before this handler
             * runs, and the CMD_EVENT_REWIND_TOGGLE attached to the
             * setting still fires via rarch_cmd below. */
            menu_st->flags            |= MENU_ST_FLAG_PREVENT_POPULATE
                                       | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_ENABLE:
         {
            struct menu_state *menu_st     = menu_state_get_ptr();
            settings->flags               |= SETTINGS_FLG_MODIFIED;
            settings->uints.video_hdr_mode = *setting->value.target.unsigned_integer;

            rarch_cmd                      = CMD_EVENT_REINIT;

            /* Switching HDR on/off shows or hides the dependent HDR
             * items (paper white, expand gamut, scanlines, ...), so
             * force the page to rebuild immediately. This fires for
             * both left/right scroll and dropdown OK selection
             * because they both invoke setting->actions->change. */
            menu_st->flags                |= MENU_ST_FLAG_PREVENT_POPULATE
                                           | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
         }
         break;
      case MENU_ENUM_LABEL_MENU_HDR_BRIGHTNESS_NITS:
         {
            video_driver_state_t *video_st       = video_state_get_ptr();
            settings->flags                     |= SETTINGS_FLG_MODIFIED;
            settings->floats.video_hdr_menu_nits = roundf(*setting->value.target.fraction);

            if (video_st && video_st->poke && video_st->poke->set_hdr_menu_nits)
               video_st->poke->set_hdr_menu_nits(video_st->data,
                     settings->floats.video_hdr_menu_nits);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS:
         {
            video_driver_state_t *video_st                = video_state_get_ptr();
            settings->flags                              |= SETTINGS_FLG_MODIFIED;
            settings->floats.video_hdr_paper_white_nits   = roundf(*setting->value.target.fraction);

            if (video_st && video_st->poke && video_st->poke->set_hdr_paper_white_nits)
               video_st->poke->set_hdr_paper_white_nits(video_st->data,
                     settings->floats.video_hdr_paper_white_nits);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT:
         {
            video_driver_state_t *video_st                = video_state_get_ptr();
            settings->flags                              |= SETTINGS_FLG_MODIFIED;
            settings->uints.video_hdr_expand_gamut        = *setting->value.target.unsigned_integer;

            if (video_st && video_st->poke && video_st->poke->set_hdr_expand_gamut)
               video_st->poke->set_hdr_expand_gamut(video_st->data,
                     settings->uints.video_hdr_expand_gamut);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_SCANLINES:
         {
            video_driver_state_t *video_st               = video_state_get_ptr();
            struct menu_state *menu_st                   = menu_state_get_ptr();
            settings->flags                              |= SETTINGS_FLG_MODIFIED;
            settings->bools.video_hdr_scanlines          = *setting->value.target.boolean;

            if (video_st && video_st->poke && video_st->poke->set_hdr_scanlines)
               video_st->poke->set_hdr_scanlines(video_st->data,
                     settings->bools.video_hdr_scanlines);

            /* Scanlines on/off shows or hides the subpixel layout
             * item, so force the page to rebuild immediately. */
            menu_st->flags                               |= MENU_ST_FLAG_PREVENT_POPULATE
                                                          | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_HDR_SUBPIXEL_LAYOUT:
         {
            video_driver_state_t *video_st                  = video_state_get_ptr();
            settings->flags                                 |= SETTINGS_FLG_MODIFIED;
            settings->uints.video_hdr_subpixel_layout   = *setting->value.target.unsigned_integer;

            if (video_st && video_st->poke && video_st->poke->set_hdr_subpixel_layout)
               video_st->poke->set_hdr_subpixel_layout(video_st->data,
                     settings->uints.video_hdr_subpixel_layout);
         }
         break;
      case MENU_ENUM_LABEL_INPUT_MAX_USERS:
         command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
         break;
#ifdef ANDROID
      case MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD:
         settings->flags |= SETTINGS_FLG_MODIFIED;
         strlcpy(settings->arrays.input_android_physical_keyboard,
               setting->value.target.string,
               sizeof(settings->arrays.input_android_physical_keyboard));
         break;
#endif
      case MENU_ENUM_LABEL_LOG_TO_FILE:
         if (verbosity_is_enabled())
         {
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
         video_driver_set_filtering(1, settings->bools.video_smooth,
               settings->bools.video_ctx_scaling);
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         {
            rarch_system_info_t *sys_info        = &runloop_state_get_ptr()->system;
            video_driver_state_t *video_st       = video_state_get_ptr();
            struct retro_system_av_info *av_info = &video_st->av_info;
            video_viewport_t *custom_vp          = &settings->video_vp_custom;

            if (sys_info)
            {
               unsigned rotation                = retroarch_get_rotation();
               unsigned base_width              = 0;
               unsigned base_height             = 0;
               struct retro_game_geometry *geom = (struct retro_game_geometry*)&av_info->geometry;

               video_driver_set_rotation(
                     (*setting->value.target.unsigned_integer +
                      sys_info->rotation) % 4);

               /* Update Custom Aspect Ratio values */
               custom_vp->x         = 0;
               custom_vp->y         = 0;

               {
                  unsigned cache_w  = 0;
                  unsigned cache_h  = 0;
                  video_driver_cached_frame_info(&cache_w, &cache_h, NULL, NULL);
                  base_width        = (geom->base_width)  ? geom->base_width  : cache_w;
                  base_height       = (geom->base_height) ? geom->base_height : cache_h;
               }

               if (base_width <= 4 || base_height <= 4)
               {
                  base_width        = (rotation % 2) ? 240 : 320;
                  base_height       = (rotation % 2) ? 320 : 240;
               }

               /* Round down when rotation is "horizontal", round up when rotation is "vertical"
                  to avoid expanding viewport each time user rotates */
               if (rotation % 2)
               {
                  custom_vp->width  = MAX(1, (custom_vp->width  / base_height)) * base_height;
                  custom_vp->height = MAX(1, (custom_vp->height / base_width )) * base_width;
               }
               else
               {
                  custom_vp->width  = ((custom_vp->width  + base_width  - 1) / base_width)  * base_width;
                  custom_vp->height = ((custom_vp->height + base_height - 1) / base_height) * base_height;
               }

               aspectratio_lut[ASPECT_RATIO_CUSTOM].value = (float)custom_vp->width / custom_vp->height;

               /* Update Aspect Ratio (only useful for 1:1 PAR) */
               command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);
            }
         }
         break;
      case MENU_ENUM_LABEL_SCREEN_ORIENTATION:
#ifndef ANDROID
         /* FIXME: Changing at runtime on Android causes setting to somehow be incremented again, many times */
         video_display_server_set_screen_orientation(
               (enum rotation)(*setting->value.target.unsigned_integer));
#endif
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         audio_set_float(AUDIO_ACTION_VOLUME_GAIN, *setting->value.target.fraction);
         break;
      case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
#ifdef HAVE_AUDIOMIXER
         audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, *setting->value.target.fraction);
#endif
         break;
      case MENU_ENUM_LABEL_AUDIO_ENABLE:
      case MENU_ENUM_LABEL_AUDIO_SYNC:
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY:
      case MENU_ENUM_LABEL_AUDIO_FORMAT_NEGOTIATION:
#ifdef HAVE_WASAPI
      case MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE:
      case MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH:
#endif
         rarch_cmd = CMD_EVENT_AUDIO_REINIT;
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         configuration_set_float(settings,
               settings->floats.audio_max_timing_skew,
               *setting->value.target.fraction);
         rarch_cmd = CMD_EVENT_AUDIO_REINIT;
         break;
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
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
         rarch_cmd = CMD_EVENT_AUDIO_REINIT;
         break;
#ifdef HAVE_MICROPHONE
      case MENU_ENUM_LABEL_MICROPHONE_LATENCY:
      case MENU_ENUM_LABEL_MICROPHONE_INPUT_RATE:
      case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_DRIVER:
      case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_QUALITY:
         rarch_cmd = CMD_EVENT_MICROPHONE_REINIT;
         break;
#endif
      case MENU_ENUM_LABEL_PAL60_ENABLE:
         if (*setting->value.target.boolean && global_get_ptr()->console.screen.pal_enable)
            rarch_cmd = CMD_EVENT_REINIT;
         else
         {
            *setting->value.target.boolean = false;
            if (setting->actions->change)
               setting->actions->change(setting);
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
#ifdef HAVE_AUDIOMIXER
         if (settings->bools.audio_enable_menu)
         {
            command_event(CMD_EVENT_AUDIO_START, NULL);
            audio_driver_load_system_sounds();
         }
         else
         {
            audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
            command_event(CMD_EVENT_AUDIO_STOP, NULL);
         }
#endif
         break;
      case MENU_ENUM_LABEL_MENU_SOUND_BGM:
#ifdef HAVE_AUDIOMIXER
         if (settings->bools.audio_enable_menu)
         {
            audio_driver_load_system_sounds();
            if (settings->bools.audio_enable_menu_bgm)
               audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
            else
               audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
         }
#endif
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY:
         video_display_server_set_window_opacity(settings->uints.video_window_opacity);
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS:
         video_display_server_set_window_decorations(settings->bools.video_window_show_decorations);
         break;
      case MENU_ENUM_LABEL_MIDI_INPUT:
         midi_driver_set_input(settings->arrays.midi_input);
         break;
      case MENU_ENUM_LABEL_MIDI_OUTPUT:
         midi_driver_set_output(settings, settings->arrays.midi_output);
         break;
      case MENU_ENUM_LABEL_MIDI_VOLUME:
         midi_driver_set_volume(settings->uints.midi_volume);
         break;
      case MENU_ENUM_LABEL_SUSTAINED_PERFORMANCE_MODE:
         frontend_driver_set_sustained_performance_mode(settings->bools.sustained_performance_mode);
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
            if ((setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE)))
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size);
            }
            if ((setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION)))
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
            if ((setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS)))
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            }
            if ((setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS)))
            {
               *setting->value.target.unsigned_integer = 0;
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            }
         }
#endif
         break;
      case MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE:
         {
            unsigned new_capacity      = COLLECTION_SIZE;
            int content_favorites_size = settings->ints.content_favorites_size;

            /* Get new size */
            if (content_favorites_size >= 0)
               new_capacity            = (unsigned)content_favorites_size;

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
               retroarch_favorites_deinit();
               retroarch_favorites_init();
#if TARGET_OS_TV
               update_topshelf();
#endif
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
               {
                  const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID);
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
               break;
            case MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE:
               {
                  const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE);
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
               break;
            default:
               /* No action required */
               break;
         }
         break;
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         /* When changing the username, clear out the password and token */
         settings->arrays.cheevos_password[0] = '\0';
         settings->arrays.cheevos_token[0]    = '\0';
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         /* When changing the password, clear out the token */
         settings->arrays.cheevos_token[0] = '\0';
         break;
      case MENU_ENUM_LABEL_CHEEVOS_UNLOCK_SOUND_ENABLE:
#ifdef HAVE_AUDIOMIXER
         if (settings->bools.cheevos_unlock_sound_enable)
            audio_driver_load_system_sounds();
#endif
         break;
      case MENU_ENUM_LABEL_INPUT_SENSORS_ENABLE:
         if (!*setting->value.target.boolean)
         {
            /* Toggled OFF: disable all sensors at hardware level */
            unsigned i;

            for (i = 0; i < DEFAULT_MAX_PADS; i++)
            {
               input_set_sensor_state(i,
                     RETRO_SENSOR_ACCELEROMETER_DISABLE, 0);
               input_set_sensor_state(i,
                     RETRO_SENSOR_GYROSCOPE_DISABLE, 0);
               input_set_sensor_state(i,
                     RETRO_SENSOR_ILLUMINANCE_DISABLE, 0);
            }
         }
         else
         {
            /* Toggled ON: re-enable sensors at hardware level */
            unsigned i;
            unsigned event_rate = 60; /* Default rate */

            for (i = 0; i < DEFAULT_MAX_PADS; i++)
            {
               input_set_sensor_state(i,
                     RETRO_SENSOR_ACCELEROMETER_ENABLE, event_rate);
               input_set_sensor_state(i,
                     RETRO_SENSOR_GYROSCOPE_ENABLE, event_rate);
               input_set_sensor_state(i,
                     RETRO_SENSOR_ILLUMINANCE_ENABLE, event_rate);
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
            const char *dir_libretro       = settings->paths.directory_libretro;
            const char *path_libretro_info = settings->paths.path_libretro_info;

            /* When enabling the core info cache,
             * force a cache refresh on the next
             * core info initialisation */
            if (*setting->value.target.boolean)
               if (!core_info_cache_force_refresh(
                    (path_libretro_info && *path_libretro_info)
                   ? path_libretro_info : dir_libretro))
               {
                  const char *_msg = msg_hash_to_str(MSG_CORE_INFO_CACHE_UNSUPPORTED);
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
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH:
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         {
            /* Whenever custom viewport dimensions are
             * changed, ASPECT_RATIO_CUSTOM must be
             * recalculated */
            video_viewport_t *custom_vp = &settings->video_vp_custom;
            float default_aspect        = aspectratio_lut[ASPECT_RATIO_CORE].value;

            aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (custom_vp && custom_vp->width && custom_vp->height) ?
                     ((float)custom_vp->width / (float)custom_vp->height) :
                           default_aspect;
         }
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
      case MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS:
#ifdef HAVE_XMB
      case MENU_ENUM_LABEL_XMB_ENTRY_ICONS:
#endif
         {
            /* Reset wallpaper by menu context reset */
            struct menu_state *menu_st = menu_state_get_ptr();

            if (menu_st->driver_ctx && menu_st->driver_ctx->context_reset)
               menu_st->driver_ctx->context_reset(menu_st->userdata,
                     video_driver_is_threaded());
         }
         break;
#if HAVE_CLOUDSYNC
      case MENU_ENUM_LABEL_CLOUD_SYNC_DRIVER:
         {
            struct menu_state *menu_st = menu_state_get_ptr();
            menu_st->flags            |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
            task_push_cloud_sync_update_driver();
         }
         break;
#endif
      case MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS:
      case MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES:
      case MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES_FIRST:
      case MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY:
      case MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES:
      case MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC:
      case MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO:
      case MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY:
      case MENU_ENUM_LABEL_CONTENT_SHOW_ADD_ENTRY:
      case MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLIST_TABS:
      case MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE:
      case MENU_ENUM_LABEL_CONTENT_SHOW_CONTENTLESS_CORES:
         {
            struct menu_state *menu_st = menu_state_get_ptr();
            if (menu_st->driver_ctx->environ_cb)
               menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
                     NULL, menu_st->userdata);
         }
         break;
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         {
            struct menu_state *menu_st = menu_state_get_ptr();
            /* Sync history playlist init state to the new setting value
             * (HISTORY_INIT internally calls HISTORY_DEINIT first, then
             * early-returns if history_list_enable is now OFF). */
            command_event(CMD_EVENT_HISTORY_INIT, NULL);
            if (menu_st->driver_ctx->environ_cb)
               menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
                     NULL, menu_st->userdata);
            menu_st->flags            |=  MENU_ST_FLAG_PREVENT_POPULATE
                                       |  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
         }
         break;
      case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         {
            video_driver_state_t *video_st       = video_state_get_ptr();
            video_st->current_video->suppress_screensaver(video_st->data,
                  settings->bools.ui_suspend_screensaver_enable);
         }
         break;
      default:
         /* Special cases */

         /* > Mapped Port (virtual -> 'physical' port mapping)
          *   Occupies a range of enum indices, so cannot
          *   simply switch on the value */
         if (   (setting->enum_idx >= MENU_ENUM_LABEL_INPUT_REMAP_PORT)
             && (setting->enum_idx <= MENU_ENUM_LABEL_INPUT_REMAP_PORT_LAST))
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

   if (rarch_cmd || (setting->flags & SD_FLAG_CMD_TRIGGER_EVENT_TRIGGERED))
      command_event(rarch_cmd, NULL);
}

static void frontend_log_level_change_handler(rarch_setting_t *setting)
{
   if (!setting)
      return;

   verbosity_set_log_level(*setting->value.target.unsigned_integer);
}

#ifdef HAVE_RUNAHEAD
static void runahead_change_handler(rarch_setting_t *setting)
{
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   bool secondary_instance;
   bool run_ahead_enabled;
#endif
   bool preempt_enable;
   settings_t *settings        = config_get_ptr();
   struct menu_state *menu_st  = menu_state_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   preempt_t *preempt          = runloop_st->preempt_data;
   unsigned run_ahead_frames   = settings->uints.run_ahead_frames;

   if (!setting)
      return;

   if (setting->enum_idx == MENU_ENUM_LABEL_RUNAHEAD_MODE)
   {
      switch (menu_st->runahead_mode)
      {
         case MENU_RUNAHEAD_MODE_SINGLE_INSTANCE:
            settings->bools.run_ahead_enabled            = true;
            settings->bools.run_ahead_secondary_instance = false;
            settings->bools.preemptive_frames_enable     = false;
            break;
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
         case MENU_RUNAHEAD_MODE_SECOND_INSTANCE:
            settings->bools.run_ahead_enabled            = true;
            settings->bools.run_ahead_secondary_instance = true;
            settings->bools.preemptive_frames_enable     = false;
            break;
#endif
         case MENU_RUNAHEAD_MODE_PREEMPTIVE_FRAMES:
            settings->bools.run_ahead_enabled            = false;
            settings->bools.preemptive_frames_enable     = true;
            break;
         default:
            settings->bools.run_ahead_enabled            = false;
            settings->bools.preemptive_frames_enable     = false;
            break;
      }

      menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }

   preempt_enable     = settings->bools.preemptive_frames_enable;
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   secondary_instance = settings->bools.run_ahead_secondary_instance;
   run_ahead_enabled  = settings->bools.run_ahead_enabled;
#endif

   /* Toggle or update preemptive frames if needed */
   if (     preempt_enable != !!preempt
         || (preempt && preempt->frames != run_ahead_frames))
      command_event(CMD_EVENT_PREEMPT_UPDATE, NULL);

#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   /* If any changes here will cause second
    * instance runahead to be enabled, must
    * re-apply cheats to ensure that they
    * propagate to the newly-created secondary
    * core */
   if (     run_ahead_enabled
         && run_ahead_frames > 0
         && secondary_instance
         && !retroarch_ctl(RARCH_CTL_IS_SECOND_CORE_LOADED, NULL)
         && retroarch_ctl(RARCH_CTL_IS_SECOND_CORE_AVAILABLE, NULL)
         && command_event(CMD_EVENT_LOAD_SECOND_CORE, NULL))
      command_event(CMD_EVENT_CHEATS_APPLY, NULL);
#endif
}
#endif

#ifdef HAVE_OVERLAY
static void overlay_enable_toggle_change_handler(rarch_setting_t *setting)
{
   (void)setting;

   /* Force a re-init to apply updates */
   command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
   command_event(CMD_EVENT_OVERLAY_INIT, NULL);
}

static void overlay_show_mouse_cursor_change_handler(rarch_setting_t *setting)
{
   (void)setting;

   input_overlay_check_mouse_cursor();
}
#endif

#ifdef HAVE_CHEEVOS
static void achievement_hardcore_mode_write_handler(rarch_setting_t *setting)
{
   rcheevos_hardcore_enabled_changed();
}

static void achievement_leaderboard_trackers_enabled_write_handler(rarch_setting_t* setting)
{
   rcheevos_leaderboard_trackers_visibility_changed();
}

static size_t setting_get_string_representation_uint_cheevos_visibility_summary(
   rarch_setting_t* setting,
   char* s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case RCHEEVOS_SUMMARY_ALLGAMES:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES),
                  len);
         case RCHEEVOS_SUMMARY_HASCHEEVOS:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS),
                  len);
         case RCHEEVOS_SUMMARY_OFF:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_OFF),
                  len);
      }
   }
   return 0;
}

#ifdef HAVE_GFX_WIDGETS
static size_t setting_get_string_representation_uint_cheevos_appearance_anchor(
   rarch_setting_t* setting, char *s, size_t len)
{
   if (setting)
   {
      switch (*setting->value.target.unsigned_integer)
      {
         case CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT),
                  len);
         case CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER),
                  len);
         case CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT),
                  len);
         case CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT),
                  len);
         case CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER),
                  len);
         case CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT:
            return strlcpy(s,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT),
                  len);
      }
   }
   return 0;
}

static void cheevos_appearance_write_handler(rarch_setting_t* setting)
{
   gfx_widgets_update_cheevos_appearance();
}
#endif
#endif

static void update_streaming_url_write_handler(rarch_setting_t *setting)
{
   recording_driver_update_streaming_url();
}

static void record_driver_write_handler(rarch_setting_t *setting)
{
   /* Force the recording settings page to rebuild so that
    * driver-specific items are shown/hidden immediately. */
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->flags |= MENU_ST_FLAG_PREVENT_POPULATE
                    | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
}

static void audio_driver_write_handler(rarch_setting_t *setting)
{
   /* Delegate to the generic write handler, then force the audio
    * output settings page to rebuild so that driver-specific items
    * (e.g. WASAPI options) are shown/hidden immediately when the
    * audio driver changes. This fires for every write path
    * (left/right scroll and dropdown OK selection) because they
    * both invoke setting->actions->change. */
   struct menu_state *menu_st = menu_state_get_ptr();
   general_write_handler(setting);
   menu_st->flags |= MENU_ST_FLAG_PREVENT_POPULATE
                    | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
}

static int setting_record_driver_action_left(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int ret = setting_string_action_left_driver(setting, idx, wraparound);
   record_driver_write_handler(setting);
   return ret;
}

static int setting_record_driver_action_right(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int ret = setting_string_action_right_driver(setting, idx, wraparound);
   record_driver_write_handler(setting);
   return ret;
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

static void systemd_samba_service_toggle(const char *path, char *unit, bool enable)
{
   /* There is difference between samba and ssh/bluetooth.
    * - samba.service is disabled if "/storage/.cache/services/samba.disabled" file is exist.
    * - ssh.service is enabled if "/storage/.cache/services/sshd.conf" file is exist.
    * - bluetooth.service is enabled if "/storage/.cache/services/bluez.conf" file is exist.
    * So it separates the systemd_service_toggle for samba.service. */

   pid_t pid    = fork();
   char* args[] = {(char*)"systemctl",
                   enable ? (char*)"start" : (char*)"stop",
                   unit,
                   NULL};

   if (pid == 0)
   {
      if (enable)
         filestream_delete(path);
      else
         filestream_close(filestream_open(path,
                  RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE));

      execvp(args[0], args);
   }
}

#ifdef HAVE_LAKKA_SWITCH
static void switch_oc_enable_toggle_change_handler(rarch_setting_t *setting)
{
   FILE* f = fopen(SWITCH_OC_TOGGLE_PATH, "w");
   if (*setting->value.target.boolean)
      fprintf(f, "1\n");
   else
      fprintf(f, "0\n");
   fclose(f);
}

static void switch_cec_enable_toggle_change_handler(rarch_setting_t *setting)
{
   if (*setting->value.target.boolean)
   {
      FILE* f = fopen(SWITCH_CEC_TOGGLE_PATH, "w");
      fprintf(f, "\n");
      fclose(f);
   }
   else
      filestream_delete(SWITCH_CEC_TOGGLE_PATH);
}

static void bluetooth_ertm_disable_toggle_change_handler(rarch_setting_t *setting)
{
   if (*setting->value.target.boolean)
   {
      FILE* f = fopen(BLUETOOTH_ERTM_TOGGLE_PATH, "w");
      fprintf(f, "1\n");
      fclose(f);
   }
   else
   {
      FILE* f = fopen(BLUETOOTH_ERTM_TOGGLE_PATH, "w");
      fprintf(f, "0\n");
      fclose(f);
   }
}
#endif

static void ssh_enable_toggle_change_handler(rarch_setting_t *setting)
{
   systemd_service_toggle(LAKKA_SSH_PATH, (char*)"sshd.service",
         *setting->value.target.boolean);
}

static void samba_enable_toggle_change_handler(rarch_setting_t *setting)
{
   systemd_samba_service_toggle(LAKKA_SAMBA_DISABLED_FILE_PATH, (char*)"smbd.service",
         *setting->value.target.boolean);
}

#ifdef HAVE_RETROFLAG
static void safeshutdown_enable_toggle_change_handler(rarch_setting_t *setting)
{
   systemd_service_toggle(LAKKA_SAFESHUTDOWN_PATH,
#ifdef HAVE_RETROFLAG_RPI5
         (char*)"retroflag_picase_safeshutdown_pi5.service",
#else
         (char*)"retroflag_picase_safeshutdown.service",
#endif
         *setting->value.target.boolean);
#ifndef HAVE_RETROFLAG_RPI5
   if(*setting->value.target.boolean)
   {
      system("/usr/bin/retroflag_picase_install_gpio-poweroff_overlay.sh enable");
   }
   else
   {
      system("/usr/bin/retroflag_picase_install_gpio-poweroff_overlay.sh disable");
   }
#endif
}
#endif

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
   RFILE *tzfp;
   if (!setting)
      return;

   config_set_timezone(setting->value.target.string);

   if ((tzfp = filestream_open(LAKKA_TIMEZONE_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      filestream_printf(tzfp, "TIMEZONE=%s", setting->value.target.string);
      filestream_close(tzfp);
   }
}
#endif

static void appicon_change_handler(rarch_setting_t *setting)
{
   uico_driver_state_t *uico_st    = uico_state_get_ptr();
   if (!setting)
      return;
   if (!uico_st->drv || !uico_st->drv->set_app_icon)
      return;
   uico_st->drv->set_app_icon(setting->value.target.string);
}

#ifdef _3DS
static void new3ds_speedup_change_handler(rarch_setting_t *setting)
{
   if (setting)
      osSetSpeedupEnable(*setting->value.target.boolean);
}
#endif

/* Descriptor-driven setting registration.
 *
 * setting_append_list() registers most settings through long runs of
 * near-identical CONFIG_* macro invocations followed by direct pokes
 * into the entry just appended.  Every such block compiles to real
 * instructions (roughly 150-200 bytes per setting), which is why the
 * function body alone is ~145KB of machine code.
 *
 * A setting_desc_t row captures the same information as constant
 * data instead: one shared instantiation loop walks a rodata table
 * and calls the existing config_* helpers, so downstream behaviour
 * (allocation, flags, free semantics, special callbacks) is
 * unchanged.  Value targets are stored as byte offsets into
 * settings_t rather than pointers, keeping rows constant and
 * relocation-free.
 *
 * Rows are applied strictly in table order and tables may only
 * replace *contiguous* registration runs: list order is user-visible
 * through the PARSE_GROUP display path, so a table must never be
 * merged across an intervening registration of a different kind or a
 * runtime conditional.
 *
 * Settings that need anything not expressible here (runtime default
 * values, runtime target pointers, custom change handlers, values
 * non OFF/ON boolean labels) simply stay imperative.
 *
 * Holdout taxonomy (each site annotated in place): value targets
 * outside settings_t; non-general read/write handlers; runtime
 * default values; loop-parametric registrations; dynamic runtime
 * labels through the ALT macros; the BIND/HEX/SIZE classes; poke
 * tails outside the descriptor grammar, chiefly change_handler,
 * whose slot is deferred until rows are generated from a single
 * source and raw initializers make per-slot macro growth free. */

enum setting_desc_class
{
   SDESC_BOOL = 0,
   SDESC_INT,
   SDESC_UINT,
   SDESC_FLOAT,
   SDESC_STRING,
   SDESC_PATH,
   SDESC_DIR,
   SDESC_ACTION
};

/* setting_desc_t.desc_flags */
#define SDESC_FLG_HAS_RANGE    (1 << 0)
#define SDESC_FLG_ENFORCE_MIN  (1 << 1)
#define SDESC_FLG_ENFORCE_MAX  (1 << 2)
/* Boolean ok/left/right become the *_with_refresh handlers.  Note
 * that the imperative blocks assign the *left* handler to action_ok
 * as well; that quirk is reproduced faithfully. */
#define SDESC_FLG_REFRESH      (1 << 3)
#define SDESC_FLG_DEF_SETTINGS (1 << 4) /* default string is a settings_t
                                           field at def_offset, resolved at
                                           list-build time */

typedef struct setting_desc
{
   uint32_t                    value_offset;   /* offsetof into settings_t */
   uint32_t                    flags;          /* SD_FLAG_*               */
   enum msg_hash_enums         name_enum;
   enum msg_hash_enums         short_enum;
   float                       min;
   float                       max;
   float                       step;
   const char                 *rounding;      /* float rows: printf rounding
                                                 string rows: default value  */
   get_string_representation_t repr;           /* NULL = class default    */
   action_ok_handler_t         action_ok;      /* NULL = class default    */
   int32_t                     def_i;          /* bool/int/uint default   */
   float                       def_f;          /* float default           */
   action_start_handler_t      action_start;   /* NULL = class default    */
   action_select_handler_t     action_select;  /* NULL = class default    */
   action_left_handler_t       action_left;    /* NULL = class default    */
   action_right_handler_t      action_right;   /* NULL = class default    */
   uint16_t                    cmd_trigger;    /* enum event_command      */
   int16_t                     offset_by;
   uint8_t                     type;           /* enum setting_desc_class */
   uint8_t                     desc_flags;     /* SDESC_FLG_*             */
   uint8_t                     ui_type;        /* 0 = class default       */
   uint8_t                     pad;
   /* Fields below are absent from older row macros and zero-initialize
    * under C89 partial aggregate initialization. */
   const char                 *values;         /* browser extension filter */
   uint32_t                    def_offset;     /* with SDESC_FLG_DEF_SETTINGS:
                                                  offsetof of the settings_t
                                                  field holding the default */
} setting_desc_t;

/* Row builders.  Field order must match setting_desc_t. */
#define SDESC_BOOL_ROW(field, label, def, sd_flags, dflags, cmd) \
   { (uint32_t)offsetof(settings_t, bools.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, NULL, NULL, NULL, \
     (int32_t)(def), 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_BOOL, (dflags), 0, 0 }
#define SDESC_BOOL_ROW_EX(field, label, def, sd_flags, dflags, cmd, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, bools.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, (_start), (_select), (_left), (_right), \
     (uint16_t)(cmd), 0, SDESC_BOOL, (dflags), (uint8_t)(_uitype), 0 }

/* _LV variants take the label and value enums as separate tokens for
 * the few registrations whose pair is mismatched. */
#define SDESC_BOOL_ROW_LV(field, label, value, def, sd_flags, dflags, cmd) \
   { (uint32_t)offsetof(settings_t, bools.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##value, \
     0.0f, 0.0f, 0.0f, NULL, NULL, NULL, \
     (int32_t)(def), 0.0f, NULL, NULL, NULL, NULL, \
     (uint16_t)(cmd), 0, SDESC_BOOL, (dflags), 0, 0 }
#define SDESC_FLOAT_ROW_LV(field, label, value, def, _rounding, sd_flags, dflags, cmd) \
   { (uint32_t)offsetof(settings_t, floats.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##value, \
     0.0f, 0.0f, 0.0f, (_rounding), NULL, NULL, \
     0, (float)(def), NULL, NULL, NULL, NULL, \
     (uint16_t)(cmd), 0, SDESC_FLOAT, (dflags), 0, 0 }
#define SDESC_STRING_ROW_LV(field, label, value, def, sd_flags, cmd, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, arrays.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##value, \
     0.0f, 0.0f, 0.0f, (def), (_repr), (ok), \
     (int32_t)sizeof(((settings_t*)0)->arrays.field), 0.0f, \
     (_start), (_select), (_left), (_right), (uint16_t)(cmd), 0, SDESC_STRING, 0, (uint8_t)(_uitype), 0 }
#define SDESC_ACTION_ROW_LV(label, value, sd_flags, ok, _repr, cmd) \
   { 0, (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##value, \
     0.0f, 0.0f, 0.0f, NULL, (_repr), (ok), \
     0, 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_ACTION, 0, 0, 0 }

/* String rows whose value target lives in settings_t.paths rather
 * than settings_t.arrays (streaming title/url and friends). */
#define SDESC_STRING_ROW_P(field, label, def, sd_flags, cmd, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, paths.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, (def), (_repr), (ok), \
     (int32_t)sizeof(((settings_t*)0)->paths.field), 0.0f, \
     (_start), (_select), (_left), (_right), (uint16_t)(cmd), 0, SDESC_STRING, 0, (uint8_t)(_uitype), 0 }

#define SDESC_UINT_ROW(field, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr) \
   { (uint32_t)offsetof(settings_t, uints.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), (int16_t)(offby), SDESC_UINT, (dflags), 0, 0 }

#define SDESC_INT_ROW(field, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr) \
   { (uint32_t)offsetof(settings_t, ints.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), (int16_t)(offby), SDESC_INT, (dflags), 0, 0 }

#define SDESC_FLOAT_ROW(field, label, def, _rounding, sd_flags, dflags, cmd, _min, _max, _step, ok, _repr) \
   { (uint32_t)offsetof(settings_t, floats.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), (_rounding), (_repr), (ok), \
     0, (float)(def), NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_FLOAT, (dflags), 0, 0 }

#define SDESC_ACTION_ROW(label) \
   { 0, 0, \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, NULL, NULL, NULL, \
     0, 0.0f, NULL, NULL, NULL, NULL, 0, 0, SDESC_ACTION, 0, 0, 0 }
#define SDESC_ACTION_ROW_EX(label, sd_flags, ok, _repr, cmd) \
   { 0, (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, NULL, (_repr), (ok), \
     0, 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_ACTION, 0, 0, 0 }

/* _EX variants add action_start / action_select / ui_type overrides
 * (0 / NULL keeps the class default). */
#define SDESC_UINT_ROW_EX(field, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr, _start, _select, _left, _right, _uitype) \
   SDESC_UINT_ROW_AT_EX(offsetof(settings_t, uints.field), label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr, _start, _select, _left, _right, _uitype)
#define SDESC_INT_ROW_EX(field, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, ints.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, (_start), (_select), (_left), (_right), \
     (uint16_t)(cmd), (int16_t)(offby), SDESC_INT, (dflags), (uint8_t)(_uitype), 0 }

/* _AT variants take the settings_t offset expression directly, for
 * value targets that are inside settings_t but not under the plain
 * bools/ints/uints/floats structs (e.g. video_vp_custom members). */
#define SDESC_UINT_ROW_AT_EX(offs, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)(offs), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, (_start), (_select), (_left), (_right), (uint16_t)(cmd), (int16_t)(offby), SDESC_UINT, (dflags), (uint8_t)(_uitype), 0 }

#define SDESC_INT_ROW_AT(offs, label, def, sd_flags, dflags, cmd, _min, _max, _step, offby, ok, _repr) \
   { (uint32_t)(offs), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), NULL, (_repr), (ok), \
     (int32_t)(def), 0.0f, NULL, NULL, NULL, NULL, (uint16_t)(cmd), (int16_t)(offby), SDESC_INT, (dflags), 0, 0 }

#define SDESC_FLOAT_ROW_EX(field, label, def, _rounding, sd_flags, dflags, cmd, _min, _max, _step, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, floats.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     (float)(_min), (float)(_max), (float)(_step), (_rounding), (_repr), (ok), \
     0, (float)(def), (_start), (_select), (_left), (_right), (uint16_t)(cmd), 0, SDESC_FLOAT, (dflags), (uint8_t)(_uitype), 0 }

/* String rows: value target is a char array inside settings_t;
 * def_i carries the buffer size and the rounding slot carries the
 * default string (see the field comment). */
#define SDESC_STRING_ROW(field, label, def, sd_flags, cmd, ok, _repr, _start, _select, _left, _right, _uitype) \
   { (uint32_t)offsetof(settings_t, arrays.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, (def), (_repr), (ok), \
     (int32_t)sizeof(((settings_t*)0)->arrays.field), 0.0f, \
     (_start), (_select), (_left), (_right), (uint16_t)(cmd), 0, SDESC_STRING, 0, (uint8_t)(_uitype), 0 }

/* Path rows: value target lives in settings_t.paths; def is a static
 * default string.  The _DS variant takes another settings_t.paths field
 * as the runtime default (the dominant pattern: filter/asset dirs). */
#define SDESC_PATH_ROW(field, label, def, sd_flags, cmd, vals, _repr, _uitype) \
   { (uint32_t)offsetof(settings_t, paths.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, (def), (_repr), NULL, \
     (int32_t)sizeof(((settings_t*)0)->paths.field), 0.0f, \
     NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_PATH, 0, (uint8_t)(_uitype), 0, \
     (vals), 0 }
#define SDESC_PATH_ROW_DS(field, label, def_field, sd_flags, cmd, vals, _repr, _uitype) \
   { (uint32_t)offsetof(settings_t, paths.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, NULL, (_repr), NULL, \
     (int32_t)sizeof(((settings_t*)0)->paths.field), 0.0f, \
     NULL, NULL, NULL, NULL, (uint16_t)(cmd), 0, SDESC_PATH, \
     SDESC_FLG_DEF_SETTINGS, (uint8_t)(_uitype), 0, \
     (vals), (uint32_t)offsetof(settings_t, paths.def_field) }

/* Directory rows: like path rows, but with the ST_DIR empty-value
 * label carried in the otherwise unused offset_by slot (documented
 * dual use; msg_hash enums fit int16).  Defaults are address
 * constants such as g_defaults.dirs[x], legal in static
 * initializers, carried in the rounding slot. */
#define SDESC_DIR_ROW(field, label, def, empty_label, sd_flags, cmd, _start) \
   { (uint32_t)offsetof(settings_t, paths.field), (sd_flags), \
     MENU_ENUM_LABEL_##label, MENU_ENUM_LABEL_VALUE_##label, \
     0.0f, 0.0f, 0.0f, (def), NULL, NULL, \
     (int32_t)sizeof(((settings_t*)0)->paths.field), 0.0f, \
     (_start), NULL, NULL, NULL, (uint16_t)(cmd), \
     (int16_t)MENU_ENUM_LABEL_VALUE_##empty_label, SDESC_DIR, 0, 0, 0 }

#define SDESC_RANGE_MINMAX \
   (SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN | SDESC_FLG_ENFORCE_MAX)

/* Every builder adds its tables with the same seven arguments; one
 * macro keeps each addition to a line and the surface honest. */
#define ADD_DESC(tbl) \
   settings_list_add_desc(list, list_info, settings, \
         tbl, ARRAY_SIZE(tbl), &group_info, &subgroup_info, parent_group)

static void settings_list_add_desc(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      settings_t *settings,
      const setting_desc_t *desc,
      unsigned count,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   unsigned i;

   for (i = 0; i < count; i++)
   {
      const setting_desc_t *d = &desc[i];
      void *target            = (void*)((uint8_t*)settings + d->value_offset);

      switch (d->type)
      {
         case SDESC_BOOL:
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_BOOL(
                  list, list_info,
                  (bool*)target,
                  d->name_enum,
                  d->short_enum,
                  (d->def_i != 0),
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  d->flags);
            if (d->desc_flags & SDESC_FLG_REFRESH)
            {
               SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
               SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
               SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)
            }
            break;
         case SDESC_INT:
            CONFIG_INT(
                  list, list_info,
                  (int*)target,
                  d->name_enum,
                  d->short_enum,
                  d->def_i,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_UINT:
            CONFIG_UINT(
                  list, list_info,
                  (unsigned int*)target,
                  d->name_enum,
                  d->short_enum,
                  (unsigned int)d->def_i,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_FLOAT:
            CONFIG_FLOAT(
                  list, list_info,
                  (float*)target,
                  d->name_enum,
                  d->short_enum,
                  d->def_f,
                  d->rounding,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_STRING:
            CONFIG_STRING(
                  list, list_info,
                  (char*)settings + d->value_offset,
                  (size_t)d->def_i,
                  d->name_enum,
                  d->short_enum,
                  d->rounding,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_PATH:
            CONFIG_PATH(
                  list, list_info,
                  (char*)settings + d->value_offset,
                  (size_t)d->def_i,
                  d->name_enum,
                  d->short_enum,
                  (d->desc_flags & SDESC_FLG_DEF_SETTINGS)
                        ? (const char*)settings + d->def_offset
                        : d->rounding,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_DIR:
            CONFIG_DIR(
                  list, list_info,
                  (char*)settings + d->value_offset,
                  (size_t)d->def_i,
                  d->name_enum,
                  d->short_enum,
                  d->rounding,
                  (enum msg_hash_enums)(uint16_t)d->offset_by,
                  group_info,
                  subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         case SDESC_ACTION:
            CONFIG_ACTION(
                  list, list_info,
                  d->name_enum,
                  d->short_enum,
                  group_info,
                  subgroup_info,
                  parent_group);
            if (d->flags != SD_FLAG_NONE)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, d->flags);
            break;
         default:
            break;
      }

      if (d->action_ok)
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], d->action_ok)
      if (d->action_start)
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], d->action_start)
      if (d->action_select)
         SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], d->action_select)
      if (d->action_left)
         SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], d->action_left)
      if (d->action_right)
         SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], d->action_right)
      if (d->repr)
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], d->repr)
      if (d->offset_by != 0 && d->type != SDESC_DIR)
         (*list)[list_info->index - 1].offset_by = d->offset_by;
      if (d->desc_flags & SDESC_FLG_HAS_RANGE)
         menu_settings_list_current_add_range(list, list_info,
               d->min, d->max, d->step,
               (d->desc_flags & SDESC_FLG_ENFORCE_MIN) ? true : false,
               (d->desc_flags & SDESC_FLG_ENFORCE_MAX) ? true : false);
      if (d->cmd_trigger != CMD_EVENT_NONE)
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info,
               (enum event_command)d->cmd_trigger);
      if (d->ui_type)
         (*list)[list_info->index - 1].ui_type =
               (enum ui_setting_type)d->ui_type;
      if (d->values)
         MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, d->values);
   }
}

static bool setting_append_list_input_player_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group,
      unsigned user)
{
   unsigned i, j;
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *sys_info              = &runloop_state_get_ptr()->system;
   const struct retro_keybind* const defaults = (user == 0)
         ? retro_keybinds_1 : retro_keybinds_rest;
   const char *binds_group_label              = msg_hash_to_str
         ((enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));

   group_info.name                            = NULL;
   subgroup_info.name                         = NULL;

   START_GROUP(list, list_info, &group_info, binds_group_label, parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(
         list,
         list_info,
         "",
         &group_info,
         &subgroup_info,
         parent_group);

   {
      char analog_to_digital[64];
      char device_index[64];
      char device_reservation_type[64];
      char device_reserved_device[64];
      char mouse_index[64];
      char bind_all[64];
      char bind_all_save_autoconfig[64];
      char bind_defaults[64];

#ifdef HAVE_LIBNX
      char split_joycon[64];
      char label_split_joycon[64];
#endif

      snprintf(analog_to_digital, sizeof(analog_to_digital),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE),
            user + 1);
      snprintf(device_index, sizeof(device_index),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_JOYPAD_INDEX),
            user + 1);
      snprintf(device_reservation_type, sizeof(device_reservation_type),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DEVICE_RESERVATION_TYPE),
            user + 1);
      snprintf(device_reserved_device, sizeof(device_reserved_device),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME),
            user + 1);
      snprintf(mouse_index, sizeof(mouse_index),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_MOUSE_INDEX),
            user + 1);
      snprintf(bind_all, sizeof(bind_all),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_BIND_ALL_INDEX),
            user + 1);
      snprintf(bind_all_save_autoconfig, sizeof(bind_all_save_autoconfig),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SAVE_AUTOCONFIG_INDEX),
            user + 1);
      snprintf(bind_defaults, sizeof(bind_defaults),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_BIND_DEFAULTS_INDEX),
            user + 1);

      /* Descriptor holdout: dynamic runtime label through the ALT macro. */
      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_joypad_index[user],
            device_index,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX),
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_input_device_index)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_input_device_index)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_input_device_index)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_input_device_index)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &get_string_representation_input_device_index)
      menu_settings_list_current_add_range(list, list_info, 0, MAX_INPUT_DEVICES - 1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_DEVICE_INDEX + user));

#ifdef HAVE_LIBNX
      snprintf(split_joycon, sizeof(split_joycon),
            "%s_%u", msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SPLIT_JOYCON), user + 1);
      snprintf(label_split_joycon, sizeof(label_split_joycon),
            "%s %u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON), user + 1);

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_split_joycon[user],
            split_joycon,
            label_split_joycon,
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &get_string_representation_split_joycon)
      menu_settings_list_current_add_range(list, list_info, 0, 1, 1.0, true, true);
#endif

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_mouse_index[user],
            mouse_index,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX),
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_input_mouse_index)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_input_mouse_index)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_input_mouse_index)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_input_mouse_index)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &get_string_representation_input_mouse_index)
      menu_settings_list_current_add_range(list, list_info, 0, MAX_INPUT_DEVICES - 1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_MOUSE_INDEX + user));

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_analog_dpad_mode[user],
            analog_to_digital,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE),
            ANALOG_DPAD_LSTICK,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_analog_dpad_mode)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_analog_dpad_mode)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_analog_dpad_mode)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_analog_dpad_mode)
      menu_settings_list_current_add_range(list, list_info, 0, ANALOG_DPAD_LAST-1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE + user));

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_device_reservation_type[user],
            device_reservation_type,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE),
            INPUT_DEVICE_RESERVATION_NONE,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_input_device_reservation_type)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_input_device_reservation_type)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_input_device_reservation_type)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_input_device_reservation_type)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &get_string_representation_input_device_reservation_type)
      menu_settings_list_current_add_range(list, list_info, 0, INPUT_DEVICE_RESERVATION_LAST - 1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_DEVICE_RESERVATION_TYPE + user));

      CONFIG_STRING_ALT(
            list, list_info,
            settings->arrays.input_reserved_devices[user],
            sizeof(settings->arrays.input_reserved_devices[user]),
            device_reserved_device,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE),
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_select_reserved_device)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_input_device_reserved_device_name)
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_input_device_reserved_device_name)
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME + user));
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_VALUE_IDX(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME));

      CONFIG_ACTION_ALT(
            list, list_info,
            bind_all,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL),
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_bind_all)

      CONFIG_ACTION_ALT(
            list, list_info,
            bind_defaults,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL),
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_bind_defaults)

#ifdef HAVE_CONFIGFILE
      CONFIG_ACTION_ALT(
            list, list_info,
            bind_all_save_autoconfig,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG),
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_bind_all_save_autoconfig)
#endif
   }

   {
      const char *value_na               = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
      bool input_descriptor_label_show   = settings->bools.input_descriptor_label_show
            && core_has_set_input_descriptor();
      bool input_descriptor_hide_unbound = settings->bools.input_descriptor_hide_unbound;

      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         char label[NAME_MAX_LENGTH];
         char name[NAME_MAX_LENGTH];
         size_t _len = 0;
         i           = (j < RARCH_ANALOG_BIND_LIST_END)
               ? input_config_bind_order[j]
               : j;

         if (input_config_bind_map_get_meta(i))
            continue;

         name[0]          = '\0';
         label[0]         = '\0';

         if (     input_descriptor_label_show
               && i < RARCH_LIGHTGUN_BIND_LIST_END)
         {
            const char *input_desc_btn;

            input_desc_btn = sys_info->input_desc_btn[user][i];
            if (input_desc_btn && *input_desc_btn)
            {
               char input_description[NAME_MAX_LENGTH];
               /* > Up to RARCH_FIRST_CUSTOM_BIND, inputs
                *   are buttons - description can be used
                *   directly
                * > Above RARCH_FIRST_CUSTOM_BIND, inputs
                *   are analog axes - have to add +/-
                *   indicators */
               size_t _len = strlcpy(input_description, input_desc_btn,
                     sizeof(input_description));
               if (i >= RARCH_FIRST_CUSTOM_BIND)
               {
                  if ((i % 2) == 0)
                     input_description[  _len] = '+';
                  else
                     input_description[  _len] = '-';
                  input_description   [++_len] = '\0';
               }

               strlcpy(label, input_description, sizeof(label));
            }
            else
            {
               snprintf(label, sizeof(label), "%s (%s)",
                     input_config_bind_map_get_desc(i),
                     value_na);

               if (input_descriptor_hide_unbound)
                  continue;
            }
         }
         else
            strlcpy(label       + _len,
                  input_config_bind_map_get_desc(i),
                  sizeof(label) - _len);

         _len = snprintf(name, sizeof(name), "p%u_", user + 1);
         strlcpy(name + _len,
               input_config_bind_map_get_base(i),
               sizeof(name) - _len);

         CONFIG_BIND_ALT(
               list, list_info,
               &input_config_binds[user][i],
               user + 1,
               user,
               name,
               label,
               &defaults[i],
               &group_info,
               &subgroup_info,
               parent_group);
         (*list)[list_info->index - 1].bind_type = i + MENU_SETTINGS_BIND_BEGIN;
      }
   }

   GROUP_END();

   return true;
}

static bool setting_append_list_input_libretro_device_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   char key_device_type[64];
   char label_device_type[64];
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
      key_device_type[0]   = '\0';
      label_device_type[0] = '\0';

      snprintf(key_device_type, sizeof(key_device_type),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE),
            user + 1);

      strlcpy(label_device_type,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE),
            sizeof(label_device_type));

      /* Descriptor holdout: dynamic runtime label through the ALT macro. */
      CONFIG_UINT_ALT(
            list, list_info,
            input_config_get_device_ptr(user),
            key_device_type,
            label_device_type,
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_libretro_device_type)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_libretro_device_type)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_libretro_device_type)
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_libretro_device_type)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_libretro_device_type)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_libretro_device)
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE + user));
   }

   GROUP_END();

   return true;
}

static bool setting_append_list_input_remap_port_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   unsigned user;
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   char key_port[64];
   char label_port[64];
   settings_t *settings = config_get_ptr();

   group_info.name      = NULL;
   subgroup_info.name   = NULL;

   START_GROUP(list, list_info, &group_info,
         "Mapped Ports", parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(list, list_info, "State", &group_info,
         &subgroup_info, parent_group);

   for (user = 0; user < MAX_USERS; user++)
   {
      key_port[0]   = '\0';
      label_port[0] = '\0';

      snprintf(key_port, sizeof(key_port),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_REMAP_PORT),
            user + 1);

      strlcpy(label_port,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT),
            sizeof(label_port));

      /* Descriptor holdout: dynamic runtime label through the ALT macro. */
      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_remap_ports[user],
            key_port,
            label_port,
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index         = user + 1;
      (*list)[list_info->index - 1].index_offset  = user;
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_action_left_input_remap_port)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_action_right_input_remap_port)
      SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_right_input_remap_port)
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_input_remap_port)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_input_remap_port)
      menu_settings_list_current_add_range(list, list_info, 0, MAX_USERS-1, 1.0, true, true);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_REMAP_PORT + user));
   }

   GROUP_END();

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

   if (setting->value.target.string)
      strlcpy(setting->value.target.string,
            setting->default_value.string,
            setting->size);
   if (setting->actions->change)
      setting->actions->change(setting);

   return 0;
}

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
static const char *config_get_menu_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MENU_DRIVERS, NULL);
}


/* --- Generated settings descriptor tables -------------------------
 * One table per def file; rows are menu display order; each table
 * carries the platform guards of its original block. The emitter
 * set defines before the first table and undefines after the last.
 * setting_append_list consumes these by reference. */

static const setting_desc_t mm_desc_0[] = {
/* GENERATED: rows come from settings_def_menu_main_state.h in order. */
#include "../settings/settings_def_menu_rows_begin.h"
#include "../settings/settings_def_menu_main_state.h"
};

static const setting_desc_t mm_desc_1[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_1.h in order. */
#include "../settings/settings_def_menu_main_actions_1.h"
};

static const setting_desc_t mm_desc_2[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_2.h in order. */
#include "../settings/settings_def_menu_main_actions_2.h"
};

static const setting_desc_t mm_desc_3[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_4.h in order. */
#include "../settings/settings_def_menu_main_actions_4.h"
};

#ifdef HAVE_CDROM
static const setting_desc_t mm_desc_4[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_3.h in order. */
#include "../settings/settings_def_menu_main_actions_3.h"
};
#endif

static const setting_desc_t mm_desc_5[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_5.h in order. */
#include "../settings/settings_def_menu_main_actions_5.h"
};

static const setting_desc_t mm_desc_6[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_6.h in order. */
#include "../settings/settings_def_menu_main_actions_6.h"
};

#if !defined(IOS) && !defined(HAVE_LAKKA)
static const setting_desc_t mm_desc_7[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_10.h in order. */
#include "../settings/settings_def_menu_main_actions_10.h"
};
#endif

static const setting_desc_t mm_desc_8[] = {
/* GENERATED: rows come from settings_def_menu_main_lists_2.h in order. */
#include "../settings/settings_def_menu_main_lists_2.h"
};

#if !defined(IOS)
#ifdef HAVE_LAKKA
static const setting_desc_t quit_lakka_desc[] = {
/* GENERATED: rows come from settings_def_quit_restart.h in order. */
#include "../settings/settings_def_quit_restart.h"
};
#endif
#endif

#if !defined(IOS)
#if !defined(HAVE_LAKKA)
static const setting_desc_t mm_desc_9[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_7.h in order. */
#include "../settings/settings_def_menu_main_actions_7.h"
};
#endif
#endif

static const setting_desc_t mm_desc_10[] = {
/* GENERATED: rows come from settings_def_menu_main_lists_3.h in order. */
#include "../settings/settings_def_menu_main_lists_3.h"
};

static const setting_desc_t mm_desc_11[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_11.h in order. */
#include "../settings/settings_def_menu_main_actions_11.h"
};

static const setting_desc_t mm_desc_12[] = {
/* GENERATED: rows come from settings_def_menu_main_lists_4.h in order. */
#include "../settings/settings_def_menu_main_lists_4.h"
};

#ifdef HAVE_BLUETOOTH
static const setting_desc_t mm_desc_13[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_8.h in order. */
#include "../settings/settings_def_menu_main_actions_8.h"
};
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_WIFI)
static const setting_desc_t mm_desc_14[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_9.h in order. */
#include "../settings/settings_def_menu_main_actions_9.h"
};
#endif

static const setting_desc_t mm_desc_15[] = {
/* GENERATED: rows come from settings_def_services_actions.h in order. */
#include "../settings/settings_def_services_actions.h"
};

static const setting_desc_t mm_desc_16[] = {
/* GENERATED: rows come from settings_def_menu_main_actions_12.h in order. */
#include "../settings/settings_def_menu_main_actions_12.h"
};

static const setting_desc_t configuration_desc_0[] = {
/* GENERATED: rows come from settings_def_shader_preset.h in order. */
#include "../settings/settings_def_shader_preset.h"
};

static const setting_desc_t logging_desc_0[] = {
/* GENERATED: rows come from settings_def_logging.h in order. */
#include "../settings/settings_def_logging.h"
};

static const setting_desc_t sav_desc_0[] = {
/* GENERATED: rows come from settings_def_saving.h in order. */
#include "../settings/settings_def_saving.h"
};

static const setting_desc_t saving2_desc_0[] = {
/* GENERATED: rows come from settings_def_saving_actions.h in order. */
#include "../settings/settings_def_saving_actions.h"
};

#ifdef HAVE_CLOUDSYNC
static const setting_desc_t cs_desc_0[] = {
/* GENERATED: rows come from settings_def_cloud_sync_general.h in order. */
#include "../settings/settings_def_cloud_sync_general.h"
};
#endif

#ifdef HAVE_CLOUDSYNC
static const setting_desc_t cs_desc_1[] = {
/* GENERATED: rows come from settings_def_cloud_sync_webdav.h in order. */
#include "../settings/settings_def_cloud_sync_webdav.h"
};
#endif

#ifdef HAVE_CLOUDSYNC
#ifdef HAVE_S3
static const setting_desc_t cs_desc_2[] = {
/* GENERATED: rows come from settings_def_cloud_sync_s3.h in order. */
#include "../settings/settings_def_cloud_sync_s3.h"
};
#endif
#endif

static const setting_desc_t frame_time_cou_desc_0[] = {
/* GENERATED: two adjacent row files, concatenated so the whole
 * screen is one table and the builder becomes pure data. */
#include "../settings/settings_def_video_frame_time_sample.h"
#include "../settings/settings_def_frame_time_counter.h"
};

static const setting_desc_t rewind_desc_0[] = {
/* GENERATED: rows come from settings_def_rewind.h in order. */
#include "../settings/settings_def_rewind.h"
};

static const setting_desc_t rewind_desc_1[] = {
/* GENERATED: rows come from settings_def_rewind_step.h in order. */
#include "../settings/settings_def_rewind_step.h"
};

static const setting_desc_t cheats_desc_0[] = {
/* GENERATED: rows come from settings_def_cheats_apply.h in order. */
#include "../settings/settings_def_cheats_apply.h"
};

#if (!defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)) || (defined(IOS) && TARGET_OS_TV)
static const setting_desc_t vid_desc_0[] = {
/* GENERATED: rows come from settings_def_video_suspend_screensaver.h in order. */
#include "../settings/settings_def_video_suspend_screensaver.h"
};
#endif

static const setting_desc_t vid_desc_1[] = {
/* GENERATED: rows come from settings_def_video_monitor_index.h in order. */
#include "../settings/settings_def_video_monitor_index.h"
};

#if defined(ANDROID) || TARGET_OS_IOS
static const setting_desc_t vid_desc_2[] = {
/* GENERATED: rows come from settings_def_video_notch.h in order. */
#include "../settings/settings_def_video_notch.h"
};
#endif

#ifdef HAVE_VULKAN
static const setting_desc_t vid_desc_3[] = {
/* GENERATED: rows come from settings_def_vulkan_gpu_index.h in order. */
#include "../settings/settings_def_vulkan_gpu_index.h"
};
#endif

#ifdef HAVE_D3D10
static const setting_desc_t vid_desc_4[] = {
/* GENERATED: rows come from settings_def_gpu_index_d3d11.h in order. */
#include "../settings/settings_def_gpu_index_d3d11.h"
};
#endif

#ifdef HAVE_D3D11
static const setting_desc_t vid_desc_5[] = {
/* GENERATED: rows come from settings_def_gpu_index_d3d12.h in order. */
#include "../settings/settings_def_gpu_index_d3d12.h"
};
#endif

#ifdef HAVE_D3D12
static const setting_desc_t vid_desc_6[] = {
/* GENERATED: rows come from settings_def_gpu_index_gl.h in order. */
#include "../settings/settings_def_gpu_index_gl.h"
};
#endif

#ifdef HAVE_METAL
static const setting_desc_t vid_desc_7[] = {
/* GENERATED: rows come from settings_def_gpu_index_vulkan.h in order. */
#include "../settings/settings_def_gpu_index_vulkan.h"
};
#endif

#ifdef WIIU
static const setting_desc_t vid_desc_8[] = {
/* GENERATED: rows come from settings_def_video_wiiu_drc.h in order. */
#include "../settings/settings_def_video_wiiu_drc.h"
};
#endif

static const setting_desc_t vid_desc_9[] = {
/* GENERATED: rows come from settings_def_video_actions_1.h in order. */
#include "../settings/settings_def_video_actions_1.h"
};

static const setting_desc_t fs_desc[] = {
/* GENERATED: rows come from settings_def_video_fullscreen.h in order. */
#include "../settings/settings_def_video_fullscreen.h"
};

#if defined(DINGUX) && defined(DINGUX_BETA)
static const setting_desc_t dingux_rr_desc[] = {
/* GENERATED: rows come from settings_def_dingux_refresh_rate.h in order. */
#include "../settings/settings_def_dingux_refresh_rate.h"
};
#endif

static const setting_desc_t refresh_desc[] = {
/* GENERATED: rows come from settings_def_video_refresh_rate.h in order. */
#include "../settings/settings_def_video_refresh_rate.h"
};

static const setting_desc_t autoswitch_desc[] = {
/* GENERATED: rows come from settings_def_refresh_autoswitch.h in order. */
#include "../settings/settings_def_refresh_autoswitch.h"
};

static const setting_desc_t vid_desc_10[] = {
/* GENERATED: rows come from settings_def_video_srgb.h in order. */
#include "../settings/settings_def_video_srgb.h"
};

static const setting_desc_t bias_desc[] = {
/* GENERATED: rows come from settings_def_video_bias.h in order. */
#include "../settings/settings_def_video_bias.h"
};

static const setting_desc_t aspect_desc[] = {
/* GENERATED: rows come from settings_def_aspect_ratio.h in order. */
#include "../settings/settings_def_aspect_ratio.h"
};

static const setting_desc_t vid_desc_11[] = {
/* GENERATED: rows come from settings_def_video_actions_2.h in order. */
#include "../settings/settings_def_video_actions_2.h"
};

#if defined(HAVE_WINDOW_OFFSET)
static const setting_desc_t vid_desc_12[] = {
/* GENERATED: rows come from settings_def_video_window_offset.h in order. */
#include "../settings/settings_def_video_window_offset.h"
};
#endif

static const setting_desc_t vp_size_desc[] = {
/* GENERATED: rows come from settings_def_viewport_size.h in order. */
#include "../settings/settings_def_viewport_size.h"
};

#if defined(DINGUX)
static const setting_desc_t dingux_ka_desc[] = {
/* GENERATED: rows come from settings_def_video_dingux_ipu.h in order. */
#include "../settings/settings_def_video_dingux_ipu.h"
};
#endif

static const setting_desc_t vid_desc_13[] = {
/* GENERATED: rows come from settings_def_video_actions_3.h in order. */
#include "../settings/settings_def_video_actions_3.h"
};

static const setting_desc_t winscale_desc[] = {
/* GENERATED: rows come from settings_def_video_window.h in order. */
#include "../settings/settings_def_video_window.h"
};

static const setting_desc_t vid_desc_14[] = {
/* GENERATED: rows come from settings_def_video_window_decorations.h in order. */
#include "../settings/settings_def_video_window_decorations.h"
};

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
static const setting_desc_t vid_desc_15[] = {
/* GENERATED: rows come from settings_def_ui_menubar.h in order. */
#include "../settings/settings_def_ui_menubar.h"
};
#endif

#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH))
static const setting_desc_t vid_desc_16[] = {
/* GENERATED: rows come from settings_def_video_window_save_position.h in order. */
#include "../settings/settings_def_video_window_save_position.h"
};
#endif

#if !((defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH)))
static const setting_desc_t vid_desc_17[] = {
/* GENERATED: rows come from settings_def_video_window_custom_size.h in order. */
#include "../settings/settings_def_video_window_custom_size.h"
};
#endif

static const setting_desc_t video2_desc_0[] = {
/* GENERATED: rows come from settings_def_video_output_misc.h in order. */
#include "../settings/settings_def_video_output_misc.h"
};

#ifdef GEKKO
static const setting_desc_t gx_desc[] = {
/* GENERATED: rows come from settings_def_video_gamecube.h in order. */
#include "../settings/settings_def_video_gamecube.h"
};
#endif

#if defined(DINGUX)
static const setting_desc_t dingux_ipu_desc[] = {
/* GENERATED: rows come from settings_def_dingux_ipu.h in order. */
#include "../settings/settings_def_dingux_ipu.h"
};
#endif

#if defined(DINGUX)
#if defined(RS90) || defined(MIYOO)
static const setting_desc_t dingux_rs90_desc[] = {
/* GENERATED: rows come from settings_def_dingux_rs90.h in order. */
#include "../settings/settings_def_dingux_rs90.h"
};
#endif
#endif

static const setting_desc_t smooth_desc[] = {
/* GENERATED: rows come from settings_def_video_smooth.h in order. */
#include "../settings/settings_def_video_smooth.h"
};

#ifdef HAVE_ODROIDGO2
static const setting_desc_t vid_ctx_desc[] = {
/* GENERATED: rows come from settings_def_video_ctx_scaling.h in order. */
#include "../settings/settings_def_video_ctx_scaling.h"
};
#endif

static const setting_desc_t rot_desc[] = {
/* GENERATED: rows come from settings_def_video_rotation.h in order. */
#include "../settings/settings_def_video_rotation.h"
};

static const setting_desc_t vid_desc_18[] = {
/* GENERATED: rows come from settings_def_video_driver_actions.h in order. */
#include "../settings/settings_def_video_driver_actions.h"
};

static const setting_desc_t hdr_desc[] = {
/* GENERATED: rows come from settings_def_video_hdr.h in order. */
#include "../settings/settings_def_video_hdr.h"
};

static const setting_desc_t vid_desc_19[] = {
/* GENERATED: rows come from settings_def_video_actions_5.h in order. */
#include "../settings/settings_def_video_actions_5.h"
};

static const setting_desc_t hdr_desc2[] = {
/* GENERATED: rows come from settings_def_video_hdr_toggles.h in order. */
#include "../settings/settings_def_video_hdr_toggles.h"
};

static const setting_desc_t vid_desc_20[] = {
/* GENERATED: rows come from settings_def_screen_brightness.h in order. */
#include "../settings/settings_def_screen_brightness.h"
};

static const setting_desc_t sync_desc[] = {
/* GENERATED: rows come from settings_def_video_sync.h in order. */
#include "../settings/settings_def_video_sync.h"
#ifdef HAVE_D3DKMT
   SDESC_BOOL_ROW(video_scanline_sync, VIDEO_SCANLINE_SYNC,
         DEFAULT_SCANLINE_SYNC,
         SD_FLAG_NONE, 0, CMD_EVENT_NONE),
#endif
};

static const setting_desc_t avsync_desc[] = {
/* GENERATED: rows come from settings_def_video_adaptive_vsync.h in order. */
#include "../settings/settings_def_video_adaptive_vsync.h"
};

static const setting_desc_t fdelay_desc[] = {
/* GENERATED: rows come from settings_def_frame_delay.h in order. */
#include "../settings/settings_def_frame_delay.h"
};

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static const setting_desc_t sdelay_desc[] = {
/* GENERATED: rows come from settings_def_shader_delay.h in order. */
#include "../settings/settings_def_shader_delay.h"
};
#endif

static const setting_desc_t shader_desc[] = {
/* GENERATED: rows come from settings_def_shader_watch.h in order. */
#include "../settings/settings_def_shader_watch.h"
};

static const setting_desc_t vid_desc_21[] = {
/* GENERATED: rows come from settings_def_black_frame_insertion.h in order. */
#include "../settings/settings_def_black_frame_insertion.h"
};

static const setting_desc_t misc_desc[] = {
/* GENERATED: rows come from settings_def_video_filter_rotation.h in order. */
#include "../settings/settings_def_video_filter_rotation.h"
};

static const setting_desc_t video_filter_desc[] = {
/* GENERATED: rows come from settings_def_video_filter_path.h in order. */
#include "../settings/settings_def_video_filter_path.h"
};

static const setting_desc_t crt_switchres_desc_0[] = {
/* GENERATED: rows come from settings_def_crt_switchres.h in order. */
#include "../settings/settings_def_crt_switchres.h"
};

static const setting_desc_t menu_sounds_desc_0[] = {
/* GENERATED: rows come from settings_def_menu_sounds.h in order. */
#include "../settings/settings_def_menu_sounds.h"
};

static const setting_desc_t audio_en_desc[] = {
/* GENERATED: rows come from settings_def_audio_enable.h in order. */
#include "../settings/settings_def_audio_enable.h"
};

static const setting_desc_t audio_state_desc[] = {
/* GENERATED: rows come from settings_def_audio_state.h in order. */
#include "../settings/settings_def_audio_state.h"
};

static const setting_desc_t audio_sync_desc[] = {
/* GENERATED: rows come from settings_def_audio_sync.h in order. */
#include "../settings/settings_def_audio_sync.h"
};

static const setting_desc_t audio_rq_desc[] = {
/* GENERATED: rows come from settings_def_audio_resampler_quality.h in order. */
#include "../settings/settings_def_audio_resampler_quality.h"
};

static const setting_desc_t audio_fmt_desc[] = {
/* GENERATED: rows come from settings_def_audio_format.h in order. */
#include "../settings/settings_def_audio_format.h"
};

static const setting_desc_t audio_skew_desc[] = {
/* GENERATED: rows come from settings_def_audio_skew.h in order. */
#include "../settings/settings_def_audio_skew.h"
      #ifdef RARCH_MOBILE
      #ifdef HAVE_MICROPHONE
      SDESC_UINT_ROW(microphone_block_frames, MICROPHONE_BLOCK_FRAMES,
         0, SD_FLAG_ADVANCED, 0, 0,
         0, 0, 0, 0, NULL, NULL),
#endif
#endif
};

static const setting_desc_t audio_dev_desc[] = {
/* GENERATED: rows come from settings_def_audio_device.h in order. */
#include "../settings/settings_def_audio_device.h"
};

static const setting_desc_t audio_dsp_desc[] = {
/* GENERATED: rows come from settings_def_audio_dsp_path.h in order. */
#include "../settings/settings_def_audio_dsp_path.h"
};

#ifdef HAVE_WASAPI
static const setting_desc_t audio_wasapi_desc[] = {
/* GENERATED: rows come from settings_def_audio_wasapi.h in order. */
#include "../settings/settings_def_audio_wasapi.h"
};
#endif

#ifdef HAVE_ASIO
static const setting_desc_t audio_asio_desc[] = {
/* GENERATED: rows come from settings_def_audio_asio_action.h in order. */
#include "../settings/settings_def_audio_asio_action.h"
};
#endif

#ifdef HAVE_MICROPHONE
static const setting_desc_t mic_enable_desc[] = {
/* GENERATED: rows come from settings_def_microphone.h in order. */
#include "../settings/settings_def_microphone.h"
};
#endif

#ifdef HAVE_MICROPHONE
#ifdef RARCH_MOBILE
static const setting_desc_t mic_block_desc[] = {
/* GENERATED: rows come from settings_def_microphone_block.h in order. */
#include "../settings/settings_def_microphone_block.h"
};
#endif
#endif

#ifdef HAVE_MICROPHONE
static const setting_desc_t mic_misc_desc[] = {
/* GENERATED: rows come from settings_def_microphone_general.h in order. */
#include "../settings/settings_def_microphone_general.h"
};
#endif

#ifdef HAVE_MICROPHONE
#ifdef HAVE_WASAPI
static const setting_desc_t mic_wasapi_desc[] = {
/* GENERATED: rows come from settings_def_mic_wasapi.h in order. */
#include "../settings/settings_def_mic_wasapi.h"
};
#endif
#endif

static const setting_desc_t inp_desc_0[] = {
/* GENERATED: rows come from settings_def_input_general.h in order. */
#include "../settings/settings_def_input_general.h"
};

#ifdef GEKKO
static const setting_desc_t inp_desc_1[] = {
/* GENERATED: rows come from settings_def_input_mouse_scale.h in order. */
#include "../settings/settings_def_input_mouse_scale.h"
};
#endif

static const setting_desc_t inp_desc_2[] = {
/* GENERATED: rows come from settings_def_input_touch_scale.h in order. */
#include "../settings/settings_def_input_touch_scale.h"
};

#ifdef UDEV_TOUCH_SUPPORT
static const setting_desc_t inp_desc_3[] = {
/* GENERATED: rows come from settings_def_input_vmouse.h in order. */
#include "../settings/settings_def_input_vmouse.h"
};
#endif

#ifdef VITA
static const setting_desc_t inp_desc_4[] = {
/* GENERATED: rows come from settings_def_input_backtouch.h in order. */
#include "../settings/settings_def_input_backtouch.h"
};
#endif

#if TARGET_OS_IPHONE
static const setting_desc_t inp_desc_5[] = {
/* GENERATED: rows come from settings_def_input_bind_timeouts.h in order. */
#include "../settings/settings_def_input_bind_timeouts.h"
};
#endif

static const setting_desc_t inp_desc_6[] = {
/* GENERATED: rows come from settings_def_input_haptics.h in order. */
#include "../settings/settings_def_input_haptics.h"
};

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
static const setting_desc_t inp_desc_7[] = {
/* GENERATED: rows come from settings_def_input_nowinkey.h in order. */
#include "../settings/settings_def_input_nowinkey.h"
};
#endif

static const setting_desc_t inp_desc_8[] = {
/* GENERATED: rows come from settings_def_input_auto_mouse_grab.h in order. */
#include "../settings/settings_def_input_auto_mouse_grab.h"
};

#ifdef ANDROID
static const setting_desc_t inp_desc_9[] = {
/* GENERATED: rows come from settings_def_input_android_workaround.h in order. */
#include "../settings/settings_def_input_android_workaround.h"
};
#endif

static const setting_desc_t inp_desc_10[] = {
/* GENERATED: rows come from settings_def_input_turbo.h in order. */
#include "../settings/settings_def_input_turbo.h"
};

static const setting_desc_t inp_desc_11[] = {
/* GENERATED: rows come from settings_def_analog_deadzone.h in order. */
#include "../settings/settings_def_analog_deadzone.h"
};

static const setting_desc_t inp_desc_12[] = {
/* GENERATED: rows come from settings_def_input_sensors_extra.h in order. */
#include "../settings/settings_def_input_sensors_extra.h"
};

static const setting_desc_t inp_desc_13[] = {
/* GENERATED: rows come from settings_def_input_actions.h in order. */
#include "../settings/settings_def_input_actions.h"
};

static const setting_desc_t input_turbo_fi_desc_0[] = {
/* GENERATED: rows come from settings_def_input_turbo_fire.h in order. */
#include "../settings/settings_def_input_turbo_fire.h"
};

static const setting_desc_t recording_desc_0[] = {
/* GENERATED: rows come from settings_def_record_quality.h in order. */
#include "../settings/settings_def_record_quality.h"
};

static const setting_desc_t recording2_desc_0[] = {
/* GENERATED: rows come from settings_def_recording_paths.h in order. */
#include "../settings/settings_def_recording_paths.h"
};

static const setting_desc_t recording_desc_1[] = {
/* GENERATED: rows come from settings_def_stream_quality.h in order. */
#include "../settings/settings_def_stream_quality.h"
};

static const setting_desc_t recording2_desc_1[] = {
/* GENERATED: rows come from settings_def_streaming_paths.h in order. */
#include "../settings/settings_def_streaming_paths.h"
};

static const setting_desc_t recording_desc_2[] = {
/* GENERATED: rows come from settings_def_record_threads.h in order. */
#include "../settings/settings_def_record_threads.h"
};

static const setting_desc_t recording_desc_3[] = {
/* GENERATED: rows come from settings_def_recording_video.h in order. */
#include "../settings/settings_def_recording_video.h"
};

static const setting_desc_t frame_throttli_desc_0[] = {
/* GENERATED: rows come from settings_def_frame_throttle_general.h in order. */
#include "../settings/settings_def_frame_throttle_general.h"
};

static const setting_desc_t menu_thr_desc[] = {
/* GENERATED: rows come from settings_def_menu_throttle.h in order. */
#include "../settings/settings_def_menu_throttle.h"
};

static const setting_desc_t frame_throttli_desc_1[] = {
/* GENERATED: rows come from settings_def_frame_throttle_fastforward.h in order. */
#include "../settings/settings_def_frame_throttle_fastforward.h"
};

#ifdef HAVE_RUNAHEAD
static const setting_desc_t frame_throttli_desc_2[] = {
/* GENERATED: rows come from settings_def_runahead_warnings.h in order. */
#include "../settings/settings_def_runahead_warnings.h"
};
#endif

#ifdef ANDROID
static const setting_desc_t frame_throttli_desc_3[] = {
/* GENERATED: rows come from settings_def_frame_throttle_slowmotion.h in order. */
#include "../settings/settings_def_frame_throttle_slowmotion.h"
};
#endif

#ifdef HAVE_GFX_WIDGETS
static const setting_desc_t osn_desc_0[] = {
/* GENERATED: rows come from settings_def_notification_enable.h in order. */
#include "../settings/settings_def_notification_enable.h"
};
#endif

#ifdef HAVE_GFX_WIDGETS
#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
static const setting_desc_t osn_desc_1[] = {
/* GENERATED: rows come from settings_def_widget_scale.h in order. */
#include "../settings/settings_def_widget_scale.h"
};
#endif
#endif

#ifdef HAVE_GFX_WIDGETS
#if !((defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)))
static const setting_desc_t widget_fs_desc[] = {
/* GENERATED: rows come from settings_def_widget_scale_fullscreen.h in order. */
#include "../settings/settings_def_widget_scale_fullscreen.h"
};
#endif
#endif

#ifdef HAVE_GFX_WIDGETS
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
static const setting_desc_t osn_desc_2[] = {
/* GENERATED: rows come from settings_def_widget_scale_windowed.h in order. */
#include "../settings/settings_def_widget_scale_windowed.h"
};
#endif
#endif

static const setting_desc_t osn_desc_3[] = {
/* GENERATED: rows come from settings_def_notification_widgets.h in order. */
#include "../settings/settings_def_notification_widgets.h"
};

static const setting_desc_t onscreen_not2_desc_0[] = {
/* GENERATED: rows come from settings_def_notification_font_path.h in order. */
#include "../settings/settings_def_notification_font_path.h"
};

static const setting_desc_t osn_desc_4[] = {
/* GENERATED: rows come from settings_def_notification_positions.h in order. */
#include "../settings/settings_def_notification_positions.h"
};

static const setting_desc_t osn_desc_5[] = {
/* GENERATED: rows come from settings_def_notification_views.h in order. */
#include "../settings/settings_def_notification_views.h"
};

#ifdef HAVE_OVERLAY
static const setting_desc_t ovl_desc_0[] = {
/* GENERATED: rows come from settings_def_overlay_enable.h in order. */
#include "../settings/settings_def_overlay_enable.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t ovl_desc_1[] = {
/* GENERATED: rows come from settings_def_overlay_auto_scale.h in order. */
#include "../settings/settings_def_overlay_auto_scale.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t overlay2_desc_0[] = {
/* GENERATED: rows come from settings_def_overlay_preset_path.h in order. */
#include "../settings/settings_def_overlay_preset_path.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t ovl_desc_2[] = {
/* GENERATED: rows come from settings_def_overlay_opacity.h in order. */
#include "../settings/settings_def_overlay_opacity.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t ovl_desc_3[] = {
/* GENERATED: rows come from settings_def_overlay_appearance.h in order. */
#include "../settings/settings_def_overlay_appearance.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t overlay_lightg_desc_0[] = {
/* GENERATED: rows come from settings_def_overlay_lightgun.h in order. */
#include "../settings/settings_def_overlay_lightgun.h"
};
#endif

#ifdef HAVE_OVERLAY
static const setting_desc_t overlay_mouse_desc_0[] = {
/* GENERATED: rows come from settings_def_overlay_mouse.h in order. */
#include "../settings/settings_def_overlay_mouse.h"
};
#endif

static const setting_desc_t menu2_desc_0[] = {
/* GENERATED: rows come from settings_def_menu_wallpaper_path.h in order. */
#include "../settings/settings_def_menu_wallpaper_path.h"
};

static const setting_desc_t menu_desc_0[] = {
/* GENERATED: rows come from settings_def_wallpaper_opacity.h in order. */
#include "../settings/settings_def_wallpaper_opacity.h"
};

static const setting_desc_t menu_desc_1[] = {
/* GENERATED: rows come from settings_def_menu_framebuffer_opacity.h in order. */
#include "../settings/settings_def_menu_framebuffer_opacity.h"
};

static const setting_desc_t menu_desc_2[] = {
/* GENERATED: rows come from settings_def_menu_wallpaper.h in order. */
#include "../settings/settings_def_menu_wallpaper.h"
};

static const setting_desc_t menu_desc_3[] = {
/* GENERATED: rows come from settings_def_menu_appearance.h in order. */
#include "../settings/settings_def_menu_appearance.h"
};

#if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)
static const setting_desc_t menu_desc_4[] = {
/* GENERATED: rows come from settings_def_menu_header_footer.h in order. */
#include "../settings/settings_def_menu_header_footer.h"
};
#endif

static const setting_desc_t menu_desc_5[] = {
/* GENERATED: rows come from settings_def_menu_visibility.h in order. */
#include "../settings/settings_def_menu_visibility.h"
};

static const setting_desc_t menu_desc_6[] = {
/* GENERATED: rows come from settings_def_menu_rgui_layout.h in order. */
#include "../settings/settings_def_menu_rgui_layout.h"
};

static const setting_desc_t menu_desc_7[] = {
/* GENERATED: rows come from settings_def_menu_scroll.h in order. */
#include "../settings/settings_def_menu_scroll.h"
};

#if !defined(DINGUX)
static const setting_desc_t menu_desc_8[] = {
/* GENERATED: rows come from settings_def_rgui_aspect.h in order. */
#include "../settings/settings_def_rgui_aspect.h"
};
#endif

static const setting_desc_t menu_desc_9[] = {
/* GENERATED: rows come from settings_def_rgui_color_theme.h in order. */
#include "../settings/settings_def_rgui_color_theme.h"
};

static const setting_desc_t menu2_desc_1[] = {
/* GENERATED: rows come from settings_def_rgui_theme_path.h in order. */
#include "../settings/settings_def_rgui_theme_path.h"
};

static const setting_desc_t menu_desc_10[] = {
/* GENERATED: rows come from settings_def_menu_rgui_transparency.h in order. */
#include "../settings/settings_def_menu_rgui_transparency.h"
};

static const setting_desc_t menu_desc_11[] = {
/* GENERATED: rows come from settings_def_menu_landscape.h in order. */
#include "../settings/settings_def_menu_landscape.h"
};

#if defined(HAVE_XMB) || defined (HAVE_OZONE)
static const setting_desc_t menu_desc_12[] = {
/* GENERATED: rows come from settings_def_menu_horizontal_animation.h in order. */
#include "../settings/settings_def_menu_horizontal_animation.h"
};
#endif

#if defined(HAVE_XMB) || defined (HAVE_OZONE)
static const setting_desc_t menu_desc_13[] = {
/* GENERATED: rows come from settings_def_xmb_animations.h in order. */
#include "../settings/settings_def_xmb_animations.h"
};
#endif

static const setting_desc_t menu_desc_14[] = {
/* GENERATED: rows come from settings_def_menu_startup.h in order. */
#include "../settings/settings_def_menu_startup.h"
};

static const setting_desc_t menu_desc_15[] = {
/* GENERATED: rows come from settings_def_menu_wraparound.h in order. */
#include "../settings/settings_def_menu_wraparound.h"
};

static const setting_desc_t menu_desc_16[] = {
/* GENERATED: rows come from settings_def_menu_savestate_resume.h in order. */
#include "../settings/settings_def_menu_savestate_resume.h"
};

static const setting_desc_t menu2_desc_2[] = {
/* GENERATED: rows come from settings_def_kiosk_password.h in order. */
#include "../settings/settings_def_kiosk_password.h"
};

#ifdef HAVE_THREADS
static const setting_desc_t menu_desc_17[] = {
/* GENERATED: rows come from settings_def_menu_threaded_data.h in order. */
#include "../settings/settings_def_menu_threaded_data.h"
};
#endif

static const setting_desc_t menu_desc_18[] = {
/* GENERATED: rows come from settings_def_rgui_particle_effect.h in order. */
#include "../settings/settings_def_rgui_particle_effect.h"
};

#ifdef HAVE_XMB
static const setting_desc_t menu_desc_19[] = {
/* GENERATED: rows come from settings_def_menu_entry_display.h in order. */
#include "../settings/settings_def_menu_entry_display.h"
};
#endif

#ifdef HAVE_XMB
static const setting_desc_t menu2_desc_3[] = {
/* GENERATED: rows come from settings_def_xmb_font_path.h in order. */
#include "../settings/settings_def_xmb_font_path.h"
};
#endif

#ifdef HAVE_XMB
static const setting_desc_t menu_desc_20[] = {
/* GENERATED: rows come from settings_def_rgui_appearance.h in order. */
#include "../settings/settings_def_rgui_appearance.h"
};
#endif

#ifdef HAVE_XMB
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_SHADERPIPELINE
static const setting_desc_t menu_desc_21[] = {
/* GENERATED: rows come from settings_def_xmb_shader_pipeline.h in order. */
#include "../settings/settings_def_xmb_shader_pipeline.h"
};
#endif
#endif
#endif

#ifdef HAVE_XMB
static const setting_desc_t menu_desc_22[] = {
/* GENERATED: rows come from settings_def_xmb_color_theme.h in order. */
#include "../settings/settings_def_xmb_color_theme.h"
};
#endif

static const setting_desc_t menu_desc_23[] = {
/* GENERATED: rows come from settings_def_menu_ex_pilot.h in order. */
#include "../settings/settings_def_menu_ex_pilot.h"
};

static const setting_desc_t menu_desc_24[] = {
/* GENERATED: rows come from settings_def_menu_main_views.h in order. */
#include "../settings/settings_def_menu_main_views.h"
};

#ifdef HAVE_LAKKA
static const setting_desc_t menu_quit_lakka_desc[] = {
/* GENERATED: rows come from settings_def_menu_show_restart.h in order. */
#include "../settings/settings_def_menu_show_restart.h"
};
#endif

#if !defined(HAVE_LAKKA) && !defined(IOS)
static const setting_desc_t menu_desc_25[] = {
/* GENERATED: rows come from settings_def_quit_visibility.h in order. */
#include "../settings/settings_def_quit_visibility.h"
};
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2)
static const setting_desc_t menu_desc_26[] = {
/* GENERATED: rows come from settings_def_menu_power_views.h in order. */
#include "../settings/settings_def_menu_power_views.h"
};
#endif

#if !(defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2))
#if !defined(IOS)
static const setting_desc_t menu_desc_27[] = {
/* GENERATED: rows come from settings_def_menu_restart_view.h in order. */
#include "../settings/settings_def_menu_restart_view.h"
};
#endif
#endif

static const setting_desc_t menu_desc_28[] = {
/* GENERATED: rows come from settings_def_menu_content_settings_view.h in order. */
#include "../settings/settings_def_menu_content_settings_view.h"
};

static const setting_desc_t menu2_desc_4[] = {
/* GENERATED: rows come from settings_def_settings_password.h in order. */
#include "../settings/settings_def_settings_password.h"
};

static const setting_desc_t menu_desc_29[] = {
/* GENERATED: rows come from settings_def_ozone_appearance.h in order. */
#include "../settings/settings_def_ozone_appearance.h"
};

#ifdef HAVE_MATERIALUI
static const setting_desc_t menu_desc_30[] = {
/* GENERATED: rows come from settings_def_ozone_extras.h in order. */
#include "../settings/settings_def_ozone_extras.h"
};
#endif

#ifdef HAVE_OZONE
static const setting_desc_t menu_desc_31[] = {
/* GENERATED: rows come from settings_def_ozone_sidebar.h in order. */
#include "../settings/settings_def_ozone_sidebar.h"
};
#endif

#ifdef HAVE_OZONE
static const setting_desc_t menu2_desc_5[] = {
/* GENERATED: rows come from settings_def_ozone_font_path.h in order. */
#include "../settings/settings_def_ozone_font_path.h"
};
#endif

#ifdef HAVE_OZONE
static const setting_desc_t menu_desc_32[] = {
/* GENERATED: rows come from settings_def_ozone_typography.h in order. */
#include "../settings/settings_def_ozone_typography.h"
};
#endif

static const setting_desc_t menu_desc_33[] = {
/* GENERATED: rows come from settings_def_menu_start_screen.h in order. */
#include "../settings/settings_def_menu_start_screen.h"
};

static const setting_desc_t menu_desc_34[] = {
/* GENERATED: rows come from settings_def_menu_rgui_thumbnails.h in order. */
#include "../settings/settings_def_menu_rgui_thumbnails.h"
};

static const setting_desc_t menu_desc_35[] = {
/* GENERATED: rows come from settings_def_menu_thumbnail_background.h in order. */
#include "../settings/settings_def_menu_thumbnail_background.h"
};

static const setting_desc_t menu_desc_36[] = {
/* GENERATED: rows come from settings_def_menu_thumbnails.h in order. */
#include "../settings/settings_def_menu_thumbnails.h"
};

static const setting_desc_t menu_desc_37[] = {
/* GENERATED: rows come from settings_def_thumbnail_upscale.h in order. */
#include "../settings/settings_def_thumbnail_upscale.h"
};

static const setting_desc_t menu_desc_38[] = {
/* GENERATED: rows come from settings_def_rgui_thumbnail_downscale.h in order. */
#include "../settings/settings_def_rgui_thumbnail_downscale.h"
};

static const setting_desc_t menu_desc_39[] = {
/* GENERATED: rows come from settings_def_menu_privacy.h in order. */
#include "../settings/settings_def_menu_privacy.h"
};

static const setting_desc_t menu_file_brow_desc_0[] = {
/* GENERATED: rows come from settings_def_menu_filebrowser.h in order. */
#include "../settings/settings_def_menu_filebrowser.h"
};

static const setting_desc_t multimedia_desc_0[] = {
/* GENERATED: rows come from settings_def_multimedia.h in order. */
#include "../settings/settings_def_multimedia.h"
};

#ifdef ANDROID
static const setting_desc_t power_manageme_desc_0_s0[] = {
/* GENERATED: rows come from settings_def_sustained_performance.h in order. */
#include "../settings/settings_def_sustained_performance.h"
};
#endif

#ifdef HAVE_LAKKA
static const setting_desc_t power_manageme_desc_0_s1[] = {
/* GENERATED: rows come from settings_def_power_action.h in order. */
#include "../settings/settings_def_power_action.h"
};
#endif

#ifndef HAVE_LAKKA
static const setting_desc_t power_manageme_desc_1[] = {
/* GENERATED: rows come from settings_def_gamemode.h in order. */
#include "../settings/settings_def_gamemode.h"
};
#endif

static const setting_desc_t wifi_managemen_desc_0[] = {
/* GENERATED: rows come from settings_def_wifi.h in order. */
#include "../settings/settings_def_wifi.h"
};

static const setting_desc_t accessibility_desc_0[] = {
/* GENERATED: rows come from settings_def_accessibility.h in order. */
#include "../settings/settings_def_accessibility.h"
};

#ifdef HAVE_TRANSLATE
static const setting_desc_t ai_service_desc_0[] = {
/* GENERATED: rows come from settings_def_ai_service.h in order. */
#include "../settings/settings_def_ai_service.h"
};
#endif

#ifdef HAVE_TRANSLATE
static const setting_desc_t ai_service_desc_1[] = {
/* GENERATED: rows come from settings_def_ai_service_options.h in order. */
#include "../settings/settings_def_ai_service_options.h"
};
#endif

static const setting_desc_t ui_desc_0[] = {
/* GENERATED: rows come from settings_def_ui_focus.h in order. */
#include "../settings/settings_def_ui_focus.h"
};

#ifdef _3DS
static const setting_desc_t ui_desc_2[] = {
/* GENERATED: rows come from settings_def_3ds_bottom_lcd.h in order. */
#include "../settings/settings_def_3ds_bottom_lcd.h"
};
#endif

#ifdef _3DS
static const setting_desc_t ui_desc_3[] = {
/* GENERATED: rows come from settings_def_ui_appearance.h in order. */
#include "../settings/settings_def_ui_appearance.h"
};
#endif

#ifdef HAVE_NETWORKING
static const setting_desc_t ui_desc_4[] = {
/* GENERATED: rows come from settings_def_menu_online_updater_view.h in order. */
#include "../settings/settings_def_menu_online_updater_view.h"
};
#endif

#ifdef HAVE_NETWORKING
#if !defined(HAVE_LAKKA)
static const setting_desc_t ui_desc_5[] = {
/* GENERATED: rows come from settings_def_menu_core_updater_view.h in order. */
#include "../settings/settings_def_menu_core_updater_view.h"
};
#endif
#endif

#ifdef HAVE_MIST
static const setting_desc_t ui_desc_6[] = {
/* GENERATED: rows come from settings_def_menu_steam.h in order. */
#include "../settings/settings_def_menu_steam.h"
};
#endif

static const setting_desc_t ui_desc_7[] = {
/* GENERATED: rows come from settings_def_menu_settings_views.h in order. */
#include "../settings/settings_def_menu_settings_views.h"
};

#ifdef HAVE_MIST
static const setting_desc_t ui_desc_8[] = {
/* GENERATED: rows come from settings_def_settings_show_steam.h in order. */
#include "../settings/settings_def_settings_show_steam.h"
};
#endif

#ifdef HAVE_SMBCLIENT
static const setting_desc_t ui_desc_9[] = {
/* GENERATED: rows come from settings_def_settings_show_smb.h in order. */
#include "../settings/settings_def_settings_show_smb.h"
};
#endif

static const setting_desc_t ui_desc_10[] = {
/* GENERATED: rows come from settings_def_menu_quick_views.h in order. */
#include "../settings/settings_def_menu_quick_views.h"
};

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static const setting_desc_t ui_desc_11[] = {
/* GENERATED: rows come from settings_def_quick_menu_shaders_view.h in order. */
#include "../settings/settings_def_quick_menu_shaders_view.h"
};
#endif

static const setting_desc_t ui_desc_12[] = {
/* GENERATED: rows come from settings_def_menu_desktop.h in order. */
#include "../settings/settings_def_menu_desktop.h"
};

static const setting_desc_t ui_desc_13[] = {
/* GENERATED: rows come from settings_def_desktop_menu.h in order. */
#include "../settings/settings_def_desktop_menu.h"
};

static const setting_desc_t pl_desc_0[] = {
/* GENERATED: rows come from settings_def_playlist_sorting.h in order. */
#include "../settings/settings_def_playlist_sorting.h"
};

static const setting_desc_t pl_desc_1[] = {
/* GENERATED: rows come from settings_def_playlist_management.h in order. */
#include "../settings/settings_def_playlist_management.h"
};

static const setting_desc_t pl_desc_2[] = {
/* GENERATED: rows come from settings_def_playlist_history.h in order. */
#include "../settings/settings_def_playlist_history.h"
};

static const setting_desc_t pl_desc_3[] = {
/* GENERATED: rows come from settings_def_playlist_display.h in order. */
#include "../settings/settings_def_playlist_display.h"
};

#if defined(HAVE_OZONE) || defined(HAVE_XMB)
static const setting_desc_t pl_desc_4[] = {
/* GENERATED: rows come from settings_def_playlist_flags.h in order. */
#include "../settings/settings_def_playlist_flags.h"
};
#endif

#ifdef HAVE_CHEEVOS
static const setting_desc_t cheevos_desc_0[] = {
/* GENERATED: rows come from settings_def_cheevos.h in order. */
#include "../settings/settings_def_cheevos.h"
};
#endif

#ifdef HAVE_CHEEVOS
static const setting_desc_t chv_desc_0[] = {
/* GENERATED: rows come from settings_def_cheevos_general.h in order. */
#include "../settings/settings_def_cheevos_general.h"
};
#endif

#ifdef HAVE_CHEEVOS
static const setting_desc_t chv_desc_1[] = {
/* GENERATED: rows come from settings_def_cheevos_visibility.h in order. */
#include "../settings/settings_def_cheevos_visibility.h"
};
#endif

#ifdef HAVE_NETWORKING
static const setting_desc_t core_updater_desc_0[] = {
/* GENERATED: rows come from settings_def_updater_extract.h in order. */
#include "../settings/settings_def_updater_extract.h"
};
#endif

#ifdef HAVE_NETWORKING
#ifdef HAVE_UPDATE_CORES
static const setting_desc_t core_updater_desc_1[] = {
/* GENERATED: rows come from settings_def_updater_experimental.h in order. */
#include "../settings/settings_def_updater_experimental.h"
};
#endif
#endif

#ifdef HAVE_NETWORKING
#ifdef HAVE_UPDATE_CORES
static const setting_desc_t core_updater_desc_2[] = {
/* GENERATED: rows come from settings_def_updater_backup.h in order. */
#include "../settings/settings_def_updater_backup.h"
};
#endif
#endif

#ifdef HAVE_SMBCLIENT
static const setting_desc_t np_desc_0[] = {
/* GENERATED: rows come from settings_def_netplay_action.h in order. */
#include "../settings/settings_def_netplay_action.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t np_desc_1[] = {
/* GENERATED: rows come from settings_def_netplay_visibility.h in order. */
#include "../settings/settings_def_netplay_visibility.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t netplay2_desc_0[] = {
/* GENERATED: rows come from settings_def_netplay_server.h in order. */
#include "../settings/settings_def_netplay_server.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t np_desc_2[] = {
/* GENERATED: rows come from settings_def_netplay_ports.h in order. */
#include "../settings/settings_def_netplay_ports.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t netplay2_desc_1[] = {
/* GENERATED: rows come from settings_def_netplay_passwords.h in order. */
#include "../settings/settings_def_netplay_passwords.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t np_desc_3[] = {
/* GENERATED: rows come from settings_def_netplay_advanced.h in order. */
#include "../settings/settings_def_netplay_advanced.h"
};
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t np_desc_4[] = {
/* GENERATED: rows come from settings_def_netplay_sync.h in order. */
#include "../settings/settings_def_netplay_sync.h"
};
#endif

#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
static const setting_desc_t np_desc_5[] = {
/* GENERATED: rows come from settings_def_netplay_nat.h in order. */
#include "../settings/settings_def_netplay_nat.h"
};
#endif
#endif

#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
static const setting_desc_t np_desc_6[] = {
/* GENERATED: rows come from settings_def_netplay_stateless.h in order. */
#include "../settings/settings_def_netplay_stateless.h"
};
#endif
#endif

#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
static const setting_desc_t np_desc_7[] = {
/* GENERATED: rows come from settings_def_network_stdin_cmd.h in order. */
#include "../settings/settings_def_network_stdin_cmd.h"
};
#endif
#endif

#if defined(HAVE_NETWORKING)
static const setting_desc_t np_desc_8[] = {
/* GENERATED: rows come from settings_def_network_ondemand_thumbnails.h in order. */
#include "../settings/settings_def_network_ondemand_thumbnails.h"
};
#endif

static const setting_desc_t user_desc_0[] = {
/* GENERATED: rows come from settings_def_user_language_action.h in order. */
#include "../settings/settings_def_user_language_action.h"
};

static const setting_desc_t user2_desc_0[] = {
/* GENERATED: rows come from settings_def_user_identity.h in order. */
#include "../settings/settings_def_user_identity.h"
};

#ifdef HAVE_GAME_AI
static const setting_desc_t user_desc_1[] = {
/* GENERATED: rows come from settings_def_game_ai.h in order. */
#include "../settings/settings_def_game_ai.h"
};
#endif

#ifdef HAVE_CHEEVOS
static const setting_desc_t user_accounts_desc_0_s0[] = {
/* GENERATED: rows come from settings_def_accounts_cheevos.h in order. */
#include "../settings/settings_def_accounts_cheevos.h"
};
#endif

#ifdef HAVE_NETWORKING
#if !IOS
static const setting_desc_t user_accounts_desc_0_s1[] = {
/* GENERATED: rows come from settings_def_accounts_streaming.h in order. */
#include "../settings/settings_def_accounts_streaming.h"
};
#endif
#endif

#ifdef HAVE_CHEEVOS
static const setting_desc_t cheevos_acct_desc[] = {
/* GENERATED: rows come from settings_def_cheevos_account.h in order. */
#include "../settings/settings_def_cheevos_account.h"
};
#endif

static const setting_desc_t dir_desc_0[] = {
/* GENERATED: rows come from settings_def_dir_core.h in order. */
#include "../settings/settings_def_dir_core.h"
};

static const setting_desc_t dir_desc_1[] = {
/* GENERATED: rows come from settings_def_dir_user.h in order. */
#include "../settings/settings_def_dir_user.h"
};

static const setting_desc_t dir_desc_2[] = {
/* GENERATED: rows come from settings_def_dir_cache_log.h in order. */
#include "../settings/settings_def_dir_cache_log.h"
};

static const setting_desc_t privacy_desc_0[] = {
/* GENERATED: rows come from settings_def_privacy_camera.h in order. */
#include "../settings/settings_def_privacy_camera.h"
};

#ifdef HAVE_DISCORD
static const setting_desc_t privacy_desc_1[] = {
/* GENERATED: rows come from settings_def_privacy_discord.h in order. */
#include "../settings/settings_def_privacy_discord.h"
};
#endif

static const setting_desc_t privacy_desc_2[] = {
/* GENERATED: rows come from settings_def_privacy_location.h in order. */
#include "../settings/settings_def_privacy_location.h"
};

#if !defined(RARCH_CONSOLE)
static const setting_desc_t midi_desc_0[] = {
/* GENERATED: rows come from settings_def_midi_volume.h in order. */
#include "../settings/settings_def_midi_volume.h"
};
#endif

#ifdef HAVE_MIST
static const setting_desc_t steam_desc_0[] = {
/* GENERATED: rows come from settings_def_steam_presence.h in order. */
#include "../settings/settings_def_steam_presence.h"
};
#endif

#ifdef HAVE_SMBCLIENT
static const setting_desc_t smbclient_desc_0[] = {
/* GENERATED: rows come from settings_def_smb_client.h in order. */
#include "../settings/settings_def_smb_client.h"
};
#endif

#ifdef HAVE_SMBCLIENT
static const setting_desc_t smbclient_desc_1[] = {
/* GENERATED: rows come from settings_def_smb_client_auth.h in order. */
#include "../settings/settings_def_smb_client_auth.h"
#include "../settings/settings_def_rows_end.h"
};
#endif

/* --- Per-list build functions; the registry below replaces the old
 * monolithic switch. Adding a settings list means a build function
 * (or, for pure descriptor groups, only def-file rows) and one
 * registry entry. */

static void settings_build_main_menu(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   unsigned user;
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info, MENU_ENUM_LABEL_MAIN_MENU_STR, parent_group);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_MAIN_MENU);
      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            ADD_DESC(mm_desc_0);

#ifndef HAVE_DYNAMIC
      if (frontend_driver_has_fork())
#endif
      {
         /* Static: the list stores this pointer and reads it after this
             * frame is gone - a latent use-after-scope in the old switch. */
            static char ext_name[16];
         if (frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
         {
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_CORE_LIST,
                  MENU_ENUM_LABEL_VALUE_CORE_LIST,
                  &group_info,
                  &subgroup_info,
                  parent_group);
            (*list)[list_info->index - 1].size                = (uint32_t)path_get_realsize(RARCH_PATH_CORE);
            (*list)[list_info->index - 1].value.target.string = path_get_ptr(RARCH_PATH_CORE);
            (*list)[list_info->index - 1].values              = ext_name;
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_LOAD_CORE);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_BROWSER_ACTION);

            ADD_DESC(mm_desc_1);
         }
      }

            ADD_DESC(mm_desc_2);

      if (settings->bools.history_list_enable)
      {
            ADD_DESC(mm_desc_3);
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
            ADD_DESC(mm_desc_4);
            }

            string_list_free(drive_list);
         }
      }
#endif

#if defined(HAVE_XMB) || defined(HAVE_OZONE)
      if (     string_is_not_equal(settings->arrays.menu_driver, "xmb")
            && string_is_not_equal(settings->arrays.menu_driver, "ozone"))
#endif
      {
            ADD_DESC(mm_desc_5);
      }

            ADD_DESC(mm_desc_6);

#if !defined(IOS) && !defined(HAVE_LAKKA)
      if (frontend_driver_has_fork())
      {
            ADD_DESC(mm_desc_7);
      }
#endif

            ADD_DESC(mm_desc_8);
#if !defined(IOS)
      /* Apple rejects iOS apps that let you forcibly quit them. */
#ifdef HAVE_LAKKA
            ADD_DESC(quit_lakka_desc);
#else
            ADD_DESC(mm_desc_9);
#endif
#endif

            ADD_DESC(mm_desc_10);

      if (string_is_not_equal(settings->arrays.record_driver, "null"))
      {
            ADD_DESC(mm_desc_11);
      }

            ADD_DESC(mm_desc_12);
#ifdef HAVE_LAKKA
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);
#endif

#ifdef HAVE_BLUETOOTH
      if (string_is_not_equal(
               settings->arrays.bluetooth_driver, "null"))
      {
            ADD_DESC(mm_desc_13);
      }
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_WIFI)
      if (string_is_not_equal(settings->arrays.wifi_driver, "null"))
      {
            ADD_DESC(mm_desc_14);
      }
#endif

            ADD_DESC(mm_desc_15);
      if (string_is_not_equal(settings->arrays.midi_driver, "null"))
      {
            ADD_DESC(mm_desc_16);
      }

      for (user = 0; user < MAX_USERS; user++)
         setting_append_list_input_player_options(list, list_info, parent_group, user);

      setting_append_list_input_libretro_device_options(list, list_info, parent_group);
      setting_append_list_input_remap_port_options(list, list_info, parent_group);

      GROUP_END();
   }
}

static void settings_build_drivers(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         unsigned i, j = 0;
         struct string_options_entry string_options_entries[14] = {{0}};

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

#ifdef HAVE_MICROPHONE
         string_options_entries[j].target         = settings->arrays.microphone_driver;
         string_options_entries[j].len            = sizeof(settings->arrays.microphone_driver);
         string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_MICROPHONE_DRIVER;
         string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER;
         string_options_entries[j].default_value  = config_get_default_microphone();
         string_options_entries[j].values         = config_get_microphone_driver_options();

         j++;
#endif

         string_options_entries[j].target         = settings->arrays.audio_resampler;
         string_options_entries[j].len            = sizeof(settings->arrays.audio_resampler);
         string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER;
         string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER;
         string_options_entries[j].default_value  = config_get_default_audio_resampler();
         string_options_entries[j].values         = config_get_audio_resampler_driver_options();

         j++;

#ifdef HAVE_MICROPHONE
         string_options_entries[j].target         = settings->arrays.microphone_resampler;
         string_options_entries[j].len            = sizeof(settings->arrays.microphone_resampler);
         string_options_entries[j].name_enum_idx  = MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_DRIVER;
         string_options_entries[j].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER;
         string_options_entries[j].default_value  = config_get_default_audio_resampler();
         string_options_entries[j].values         = config_get_audio_resampler_driver_options();

         j++;
#endif

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
                  general_write_handler,
                  general_read_handler);
            SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
            SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_action_ok_uint)
            SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_driver)
            SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_driver)

            /* Record driver needs refresh-aware handlers so that the
             * recording settings page rebuilds when the driver changes. */
            if (string_options_entries[i].name_enum_idx
                  == MENU_ENUM_LABEL_RECORD_DRIVER)
            {
               SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_record_driver_action_left)
               SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_record_driver_action_right)
            }

            /* Audio driver needs a refresh-aware write handler so that
             * the audio output settings page rebuilds when the driver
             * changes, hiding/showing driver-specific items such as
             * the WASAPI options. Using change_handler (rather than
             * action_left/right wrappers) covers both the left/right
             * scroll and the dropdown OK selection paths, since each
             * invokes setting->actions->change after writing. */
            if (string_options_entries[i].name_enum_idx
                  == MENU_ENUM_LABEL_AUDIO_DRIVER)
               SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], audio_driver_write_handler)
         }

         GROUP_END();
       
   }
}

static void settings_build_core(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         unsigned i, listing = 0;
#ifndef HAVE_DYNAMIC
         struct bool_entry bool_entries[11];
#else
         struct bool_entry bool_entries[10];
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
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_VIDEO_SHARED_CONTEXT)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.driver_switch_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_DRIVER_SWITCH_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_DRIVER_SWITCH_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.load_dummy_on_core_shutdown;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_LOAD_DUMMY_ON_CORE_SHUTDOWN)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.set_supports_no_game_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         bool_entries[listing].flags         |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.systemfiles_in_content_dir;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SYSTEMFILES_IN_CONTENT_DIR)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.video_allow_rotate;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_ALLOW_ROTATE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.core_option_category_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_OPTION_CATEGORY_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_CORE_OPTION_CATEGORY_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.core_info_cache_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_CORE_INFO_CACHE_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.core_info_savestate_bypass;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CORE_INFO_SAVESTATE_BYPASS;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_CORE_INFO_SAVESTATE_BYPASS)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

#ifndef HAVE_DYNAMIC
         bool_entries[listing].target         = &settings->bools.always_reload_core_on_run_content;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;
#endif
         /* Iterate the filled count, not the array capacity:
          * the capacities above are upper bounds and iterating
          * ARRAY_SIZE registered one setting from uninitialized
          * stack memory (garbage label/flags/default and, worst,
          * a garbage value target pointer). */
         for (i = 0; i < listing; i++)
         {
#if !defined(HAVE_CORE_INFO_CACHE)
            if (bool_entries[i].name_enum_idx ==
                  MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE)
               continue;
#endif
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_BOOL(
                  list, list_info,
                  bool_entries[i].target,
                  bool_entries[i].name_enum_idx,
                  bool_entries[i].SHORT_enum_idx,
                  (bool_entries[i].flags & SD_FLAG_DEFAULT_VALUE) ? true : false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  bool_entries[i].flags);
         }

         GROUP_END();
       
   }
}

static void settings_build_configuration(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         uint8_t i, listing = 0;
         struct bool_entry bool_entries[10];
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
               parent_group);

         bool_entries[listing].target         = &settings->bools.config_save_on_exit;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_CONFIG_SAVE_ON_EXIT)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.config_save_minimal;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_CONFIG_SAVE_MINIMAL;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_MINIMAL;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_CONFIG_SAVE_MINIMAL)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.show_hidden_files;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SHOW_HIDDEN_FILES;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_SHOW_HIDDEN_FILES)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.game_specific_options;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_GAME_SPECIFIC_OPTIONS)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.auto_overrides_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_AUTO_OVERRIDES_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.auto_remaps_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_AUTO_REMAPS_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.initial_disk_change_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_INITIAL_DISK_CHANGE_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_INITIAL_DISK_CHANGE_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.auto_shaders_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_AUTO_SHADERS_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.global_core_options;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_GLOBAL_CORE_OPTIONS)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.remap_save_on_exit;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_REMAP_SAVE_ON_EXIT;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_REMAP_SAVE_ON_EXIT)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         for (i = 0; i < ARRAY_SIZE(bool_entries); i++)
         {
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_BOOL(
                  list, list_info,
                  bool_entries[i].target,
                  bool_entries[i].name_enum_idx,
                  bool_entries[i].SHORT_enum_idx,
                  (bool_entries[i].flags & SD_FLAG_DEFAULT_VALUE) ? true : false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  bool_entries[i].flags);
         }

            ADD_DESC(configuration_desc_0);

         GROUP_END();
       
   }
}

static void settings_build_logging(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
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
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_bool_action_right_with_refresh)

         /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
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
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], frontend_log_level_change_handler)
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         menu_settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_libretro_log_level)

            ADD_DESC(logging_desc_0);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Performance Counters", &group_info, &subgroup_info,
               parent_group);

         retroarch_ctl(RARCH_CTL_GET_PERFCNT, &tmp_b);

         /* Descriptor holdout: value target outside settings_t. */
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
       
      GROUP_END();
   }
}

static void settings_build_saving(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         uint8_t i, listing = 0;
         struct bool_entry bool_entries[12];

         GROUP_STATE(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS, MENU_ENUM_LABEL_SAVING_SETTINGS);

         bool_entries[listing].target         = &settings->bools.sort_savefiles_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SORT_SAVEFILES_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.sort_savestates_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SORT_SAVESTATES_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.sort_savefiles_by_content_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SORT_SAVEFILES_BY_CONTENT_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.sort_savestates_by_content_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SORT_SAVESTATES_BY_CONTENT_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.sort_screenshots_by_content_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SORT_SCREENSHOTS_BY_CONTENT_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.block_sram_overwrite;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_BLOCK_SRAM_OVERWRITE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;

         listing++;

         bool_entries[listing].target         = &settings->bools.savestate_auto_save;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_SAVESTATE_AUTO_SAVE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.savestate_auto_load;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD;
         bool_entries[listing].flags          = SD_FLAG_NONE;
         if (DEFAULT_SAVESTATE_AUTO_LOAD)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.savestate_thumbnail_enable;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SAVESTATE_THUMBNAIL_ENABLE)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.savefiles_in_content_dir;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SAVEFILES_IN_CONTENT_DIR)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.savestates_in_content_dir;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SAVESTATES_IN_CONTENT_DIR)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         bool_entries[listing].target         = &settings->bools.screenshots_in_content_dir;
         bool_entries[listing].name_enum_idx  = MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
         bool_entries[listing].flags          = SD_FLAG_ADVANCED;
         if (DEFAULT_SCREENSHOTS_IN_CONTENT_DIR)
            bool_entries[listing].flags      |= SD_FLAG_DEFAULT_VALUE;
         listing++;

         for (i = 0; i < ARRAY_SIZE(bool_entries); i++)
         {
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_BOOL(
                  list, list_info,
                  bool_entries[i].target,
                  bool_entries[i].name_enum_idx,
                  bool_entries[i].SHORT_enum_idx,
                  (bool_entries[i].flags & SD_FLAG_DEFAULT_VALUE) ? true : false,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  bool_entries[i].flags);
         }

            ADD_DESC(sav_desc_0);
            ADD_DESC(saving2_desc_0);

         GROUP_END();
       

   }
}

static void settings_build_cloud_sync(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CLOUDSYNC
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS, MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS);

            ADD_DESC(cs_desc_0);

      CONFIG_STRING_OPTIONS(
            list, list_info,
            settings->arrays.cloud_sync_driver,
            sizeof(settings->arrays.cloud_sync_driver),
            MENU_ENUM_LABEL_CLOUD_SYNC_DRIVER,
            MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
            "null",
            config_get_cloud_sync_driver_options(),
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_action_ok_uint)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_driver)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_driver)

            ADD_DESC(cs_desc_1);

#ifdef HAVE_S3
      /* AWS */
            ADD_DESC(cs_desc_2);
#endif
      GROUP_END();
#endif
   }
}


static void settings_build_rewind(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS), parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            ADD_DESC(rewind_desc_0);

         /* Descriptor holdout: setting class not modelled by descriptor rows. */
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
         menu_settings_list_current_add_range(list, list_info,
               1024 * 1024, 1024 * 1024 * 1024, settings->uints.rewind_buffer_size_step * 1024 * 1024, true, true);

            ADD_DESC(rewind_desc_1);

      GROUP_END();
   }
}


static void settings_build_cheat_details(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CHEATS
      {
         int max_bit_position;
         if (!cheat_manager_state.cheats)
            return;

         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DETAILS_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.idx, CHEAT_IDX,
               NULL,NULL,
               0,&setting_get_string_representation_uint,0,cheat_manager_get_size()-1,1);

         /* Descriptor holdout: value target outside settings_t. */
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
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

         /* The editor scratch is heap-allocated on demand; the rows
          * below bind its address, so it must exist before they build. */
         cheat_manager_working_code_ensure();
         CONFIG_STRING(
               list, list_info,
               cheat_manager_state.working_code,
               CHEAT_CODE_SCRATCH_SIZE,
               MENU_ENUM_LABEL_CHEAT_CODE,
               MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.handler, CHEAT_HANDLER,
               setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
               MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
               &setting_get_string_representation_uint_as_enum,
               CHEAT_HANDLER_TYPE_EMU,CHEAT_HANDLER_TYPE_RETRO,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_STRING(
               list, list_info,
               cheat_manager_state.working_code,
               CHEAT_CODE_SCRATCH_SIZE,
               MENU_ENUM_LABEL_CHEAT_CODE,
               MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.memory_search_size, CHEAT_MEMORY_SEARCH_SIZE,
               setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
               MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
               &setting_get_string_representation_uint_as_enum,
               0,5,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.cheat_type, CHEAT_TYPE,
               setting_uint_action_left_default,setting_uint_action_right_default,
               MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
               &setting_get_string_representation_uint_as_enum,
               CHEAT_TYPE_DISABLED,CHEAT_TYPE_RUN_NEXT_IF_GT,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)


         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.working_cheat.value,
               MENU_ENUM_LABEL_CHEAT_VALUE,
               MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0, cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size), 1, true, true);
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_hex_and_uint)
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);


         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.working_cheat.address,
               MENU_ENUM_LABEL_CHEAT_ADDRESS,
               MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0,  (cheat_manager_state.total_memory_size == 0) ? 0 : (cheat_manager_state.total_memory_size - 1), 1, true, true);
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_hex_and_uint)
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);


         max_bit_position = (cheat_manager_state.working_cheat.memory_search_size < 3) ? 255 : 0;
         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.address_mask,
               CHEAT_ADDRESS_BIT_POSITION,
               setting_uint_action_left_default,setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               0,
               max_bit_position,
               1);

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

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_count,
               CHEAT_REPEAT_COUNT,
               setting_uint_action_left_default,
               setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               1,
               2048,
               1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_add_to_address,
               CHEAT_REPEAT_ADD_TO_ADDRESS,
               setting_uint_action_left_default,setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               1,
               2048,
               1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.repeat_add_to_value,
               CHEAT_REPEAT_ADD_TO_VALUE,
               setting_uint_action_left_default,
               setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               0,
               0xFFFF,
               1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_type, CHEAT_RUMBLE_TYPE,
               setting_uint_action_left_default,setting_uint_action_right_default,
               MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
               &setting_get_string_representation_uint_as_enum,
               RUMBLE_TYPE_DISABLED,RUMBLE_TYPE_END_LIST-1,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT(
               list, list_info,
               &cheat_manager_state.working_cheat.rumble_value,
               MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE,
               MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info,
               0, cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size), 1, true, true);
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_hex_and_uint)
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_port, CHEAT_RUMBLE_PORT,
               setting_uint_action_left_default,setting_uint_action_right_default,
               MENU_ENUM_LABEL_RUMBLE_PORT_0,
               &setting_get_string_representation_uint_as_enum,
               0,16,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_primary_strength,
               CHEAT_RUMBLE_PRIMARY_STRENGTH,
               setting_uint_action_left_default,setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               0,
               65535,
               1);

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_primary_duration, CHEAT_RUMBLE_PRIMARY_DURATION,
               setting_uint_action_left_default,setting_uint_action_right_default,
               0,&setting_get_string_representation_uint,0,5000,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_secondary_strength,
               CHEAT_RUMBLE_SECONDARY_STRENGTH,
               setting_uint_action_left_default,
               setting_uint_action_right_default,
               0,
               &setting_get_string_representation_hex_and_uint,
               0,
               65535,
               1);

         CONFIG_UINT_CBS(cheat_manager_state.working_cheat.rumble_secondary_duration, CHEAT_RUMBLE_SECONDARY_DURATION,
               setting_uint_action_left_default,setting_uint_action_right_default,
               0,&setting_get_string_representation_uint,0,5000,1);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)

         GROUP_END();
      }
#endif
   }
}

static void settings_build_cheat_search(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CHEATS
      if (!cheat_manager_state.cheats)
         return;

      START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS), parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

      CONFIG_UINT_CBS(cheat_manager_state.search_bit_size,
            CHEAT_START_OR_RESTART,
            setting_uint_action_left_with_refresh,
            setting_uint_action_right_with_refresh,
            MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
            &setting_get_string_representation_uint_as_enum,
            0,5,1);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_initialize_memory)

      /* Descriptor holdout: value target outside settings_t. */
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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_exact)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_exact_input)
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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_lt)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_lt)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_lte)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_lte)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_gt)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_gt)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_gte)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_gte)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_eq)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_eq)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_neq)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_neq)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_eqplus)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_eqplus_input)

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
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_eqminus)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_search_eqminus_input)

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
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_delete_match)

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
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &cheat_manager_copy_match)

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
      menu_settings_list_current_add_range(list, list_info, 0,
            (cheat_manager_state.total_memory_size > 0)
            ? (cheat_manager_state.total_memory_size - 1)
            : 0,
            1,
            true,
            true);
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheat_browse_address)

      GROUP_END();
#endif
   }
}

static void settings_build_video(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
               parent_group);
         MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_VIDEO_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#if (!defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)) || (defined(IOS) && TARGET_OS_TV)
            ADD_DESC(vid_desc_0);
#endif
         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Platform-specific", &group_info,
               &subgroup_info, parent_group);

         video_driver_menu_settings((void**)list, (void*)list_info,
               (void*)&group_info, (void*)&subgroup_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Monitor", &group_info, &subgroup_info, parent_group);

            ADD_DESC(vid_desc_1);

         /* prevent unused function warning on unsupported builds */
         (void)setting_get_string_representation_int_gpu_index;

#if defined(ANDROID) || TARGET_OS_IOS
            ADD_DESC(vid_desc_2);
#endif
#ifdef HAVE_VULKAN
         if (string_is_equal(video_driver_get_ident(), "vulkan"))
         {
#ifdef __APPLE__
            /* Descriptor holdout: runtime default value. */
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_use_metal_arg_buffers,
                  MENU_ENUM_LABEL_VIDEO_USE_METAL_ARG_BUFFERS,
                  MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
                  config_metal_arg_buffers_default(),
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
#endif

            ADD_DESC(vid_desc_3);
         }
#endif

#ifdef HAVE_D3D10
         if (string_is_equal(video_driver_get_ident(), "d3d10"))
         {
            ADD_DESC(vid_desc_4);
         }
#endif

#ifdef HAVE_D3D11
         if (string_is_equal(video_driver_get_ident(), "d3d11"))
         {
            ADD_DESC(vid_desc_5);
         }
#endif

#ifdef HAVE_D3D12
         if (string_is_equal(video_driver_get_ident(), "d3d12"))
         {
            ADD_DESC(vid_desc_6);
         }
#endif

#ifdef HAVE_METAL
         if (string_is_equal(video_driver_get_ident(), "metal"))
         {
            ADD_DESC(vid_desc_7);
         }
#endif

#ifdef WIIU
            ADD_DESC(vid_desc_8);
#endif
         if (video_driver_has_windowed())
         {
            ADD_DESC(vid_desc_9);
         }


                  ADD_DESC(fs_desc);

#if defined(DINGUX) && defined(DINGUX_BETA)
         if (   string_is_equal(settings->arrays.video_driver, "sdl_dingux")
             || string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
                  ADD_DESC(dingux_rr_desc);
         else
#endif
         {
            float actual_refresh_rate = video_driver_get_refresh_rate();

            {
               /* video_refresh_rate is intentionally the value
                * target of the next two rows as well; the polled
                * variant below stays imperative because its
                * default is read from the video driver at
                * registration time. */
               ADD_DESC(refresh_desc);
            }

            {
               /* Descriptor holdout: runtime default value. */
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
               SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], &setting_action_start_video_refresh_rate_polled)
               SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_video_refresh_rate_polled)
               SETTINGS_ACTION_SET(sel, &(*list)[list_info->index - 1], &setting_action_ok_video_refresh_rate_polled)
               SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_st_float_video_refresh_rate_polled)
               SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            }
         }

                  ADD_DESC(autoswitch_desc);

         if (string_is_equal(settings->arrays.video_driver, "gl"))
         {
            ADD_DESC(vid_desc_10);
         }

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Aspect", &group_info, &subgroup_info, parent_group);

                  ADD_DESC(bias_desc);

                  ADD_DESC(aspect_desc);


#if defined(GEKKO) || defined(PS2) || defined(__PS3__)
         if (true)
#else
         if (!string_is_equal(video_display_server_get_ident(), "null"))
#endif
         {
            ADD_DESC(vid_desc_11);
         }

#if defined(HAVE_WINDOW_OFFSET)
            ADD_DESC(vid_desc_12);
#endif
                  ADD_DESC(vp_size_desc);

#if defined(DINGUX)
         if (   string_is_equal(settings->arrays.video_driver, "sdl_dingux")
             || string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
                  ADD_DESC(dingux_ka_desc);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Scaling", &group_info, &subgroup_info, parent_group);

            ADD_DESC(vid_desc_13);

         if (video_driver_has_windowed())
                  ADD_DESC(winscale_desc);

            ADD_DESC(vid_desc_14);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
            ADD_DESC(vid_desc_15);
#endif
#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) ||  \
    (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH))
            ADD_DESC(vid_desc_16);
#else
            ADD_DESC(vid_desc_17);
#endif
            ADD_DESC(video2_desc_0);

#ifdef GEKKO
                  ADD_DESC(gx_desc);
#endif

#if defined(DINGUX)
         if (string_is_equal(settings->arrays.video_driver, "sdl_dingux"))
                  ADD_DESC(dingux_ipu_desc);
#if defined(RS90) || defined(MIYOO)
         else if (string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
                  ADD_DESC(dingux_rs90_desc);
#endif
         else
#endif
                  ADD_DESC(smooth_desc);

#ifdef HAVE_ODROIDGO2
                              ADD_DESC(vid_ctx_desc);
#endif

                  ADD_DESC(rot_desc);

         END_SUB_GROUP(list, list_info, parent_group);

         if ((video_driver_get_disp_flags() & VIDEO_FLAG_HDR_SUPPORT))
         {
            START_SUB_GROUP(list, list_info, "HDR", &group_info, &subgroup_info, parent_group);

            ADD_DESC(vid_desc_18);

            /* Descriptor holdout: poke tail outside the descriptor grammar. */
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_hdr_mode,
                  MENU_ENUM_LABEL_VIDEO_HDR_ENABLE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
                  DEFAULT_VIDEO_HDR_MODE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
            SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_video_hdr_mode)
            menu_settings_list_current_add_range(list, list_info, 0, video_driver_hdr_max_mode(), 1, true, true);
            MENU_SETTINGS_LIST_CURRENT_ADD_CMD(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);

            /* if (settings->uints.video_hdr_mode > 0) */
            {
                              ADD_DESC(hdr_desc);

               START_SUB_GROUP(list, list_info, "HDR", &group_info, &subgroup_info, parent_group);

            ADD_DESC(vid_desc_19);

                              ADD_DESC(hdr_desc2);

               END_SUB_GROUP(list, list_info, parent_group);
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
            ADD_DESC(vid_desc_20);
         }

#if defined(HAVE_THREADS) && !defined(__PSL1GHT__) && !defined(__PS3__) && !defined(__APPLE__)
         /* Descriptor holdout: value target outside settings_t. */
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
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)
         MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);
#endif

         {
            /* Synchronization block: contiguous descriptor run.
             * Row order is list order and must be preserved. */
            ADD_DESC(sync_desc);
         }

         if (video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC))
                  ADD_DESC(avsync_desc);

                  ADD_DESC(fdelay_desc);

         /* Unlike all other shader-related menu entries
          * (which appear in the shaders quick menu, and
          * are thus hidden automatically on platforms
          * without shader support), VIDEO_SHADER_DELAY
          * is shown in 'Settings > Video'. It therefore
          * requires an explicit guard to prevent display
          * on unsupported platforms */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            gfx_ctx_flags_t flags;
            flags.flags     = 0;
            video_context_driver_get_flags(&flags);

            if (
                     BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_HLSL))
                        ADD_DESC(sdelay_desc);
         }
#endif

                  ADD_DESC(shader_desc);

         {
#if defined(HAVE_STEAM) && defined(HAVE_MIST)
            bool on_deck = false;
            mist_steam_utils_is_steam_running_on_steam_deck(&on_deck);
            /* We want to not expose Black Frame Insertion on Steam Deck
             * for safety reasons */
            if (!on_deck && video_driver_test_all_flags(GFX_CTX_FLAGS_BLACK_FRAME_INSERTION))
#else
            if (video_driver_test_all_flags(GFX_CTX_FLAGS_BLACK_FRAME_INSERTION))
#endif
            {

            ADD_DESC(vid_desc_21);
            }
         }

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(
               list,
               list_info,
               "Miscellaneous",
               &group_info,
               &subgroup_info,
               parent_group);

                  ADD_DESC(misc_desc);
                  ADD_DESC(video_filter_desc);

         GROUP_END();
       
   }
}



static void settings_build_audio(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS), parent_group);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_AUDIO_SETTINGS);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            ADD_DESC(audio_en_desc);

      /* The two mute settings stay imperative: their value
       * targets come from audio_get_bool_ptr() at registration
       * time and live outside settings_t, which offset-based
       * descriptor rows cannot express. */
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
      /* Descriptor holdout: value target outside settings_t. */
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

            ADD_DESC(audio_state_desc);

      END_SUB_GROUP(list, list_info, parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      START_SUB_GROUP(
            list,
            list_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC),
            &group_info,
            &subgroup_info,
            parent_group);

            ADD_DESC(audio_sync_desc);

      /* The latency pair stays imperative: defaults come from
       * g_defaults at registration time. */
      /* Descriptor holdout: runtime default value. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      menu_settings_list_current_add_range(list, list_info, 0, 512, 1.0, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_MICROPHONE
      CONFIG_UINT(
            list, list_info,
            &settings->uints.microphone_latency,
            MENU_ENUM_LABEL_MICROPHONE_LATENCY,
            MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
            g_defaults.settings_in_latency ?
            g_defaults.settings_in_latency : DEFAULT_IN_LATENCY,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      menu_settings_list_current_add_range(list, list_info, 0, 512, 1.0, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

            ADD_DESC(audio_rq_desc);

            ADD_DESC(audio_fmt_desc);

      CONFIG_FLOAT(
            list, list_info,
            &settings->floats.audio_rate_control_delta,
            MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
            MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
            DEFAULT_RATE_CONTROL_DELTA,
            "%.3f",
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      menu_settings_list_current_add_range(list, list_info, 0.0, 0.020, 0.001, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            ADD_DESC(audio_skew_desc);

      END_SUB_GROUP(list, list_info, parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      START_SUB_GROUP(
            list,
            list_info,
            "Miscellaneous",
            &group_info,
            &subgroup_info,
            parent_group);

            ADD_DESC(audio_dev_desc);

            ADD_DESC(audio_dsp_desc);

#ifdef HAVE_WASAPI
      if (string_is_equal(audio_driver_get_ident(), "wasapi"))
            ADD_DESC(audio_wasapi_desc);
#endif

#ifdef HAVE_ASIO
      if (string_is_equal(audio_driver_get_ident(), "asio"))
            ADD_DESC(audio_asio_desc);
#endif

      GROUP_END();
   }
}

#ifdef HAVE_MICROPHONE
static void settings_build_microphone(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS), parent_group);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_MICROPHONE_SETTINGS);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            ADD_DESC(mic_enable_desc);

      END_SUB_GROUP(list, list_info, parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      /* Descriptor holdout: runtime default value. */
      CONFIG_UINT(
            list, list_info,
            &settings->uints.microphone_latency,
            MENU_ENUM_LABEL_MICROPHONE_LATENCY,
            MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
            g_defaults.settings_in_latency ?
            g_defaults.settings_in_latency : DEFAULT_IN_LATENCY,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      menu_settings_list_current_add_range(list, list_info, 0, 512, 1.0, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef RARCH_MOBILE
            ADD_DESC(mic_block_desc);
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

            ADD_DESC(mic_misc_desc);

#ifdef HAVE_WASAPI
      if (string_is_equal(settings->arrays.microphone_driver, "wasapi"))
            ADD_DESC(mic_wasapi_desc);
#endif

      GROUP_END();
   }
}
#endif

static void settings_build_input(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       

         START_GROUP(list, list_info, &group_info,
               MENU_ENUM_LABEL_INPUT_SETTINGS_BEGIN_STR,
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            ADD_DESC(inp_desc_0);

#ifdef GEKKO
            ADD_DESC(inp_desc_1);
#endif

            ADD_DESC(inp_desc_2);

#ifdef UDEV_TOUCH_SUPPORT
            ADD_DESC(inp_desc_3);
#endif

#ifdef VITA
            ADD_DESC(inp_desc_4);
#endif

#if TARGET_OS_IPHONE
            ADD_DESC(inp_desc_5);
#endif

            ADD_DESC(inp_desc_6);

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
            ADD_DESC(inp_desc_7);
#endif

            ADD_DESC(inp_desc_8);

#ifdef ANDROID
            ADD_DESC(inp_desc_9);

      {
         input_driver_state_t *st = input_state_get_ptr();
         input_driver_t *current_input = st->current_driver;
         if (string_is_equal(current_input->ident, "android"))
         {
            /* Descriptor holdout: value target outside settings_t. */
            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
                  MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
                  &group_info,
                  &subgroup_info,
                  parent_group);
            SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_select_physical_keyboard)
            SETTINGS_ACTION_SET(read, &(*list)[list_info->index - 1], &general_read_handler)
            SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], &general_write_handler)
            SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_android_physical_keyboard)
            (*list)[list_info->index - 1].default_value.string      = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
         }
      }
#endif

            ADD_DESC(inp_desc_10);

         END_SUB_GROUP(list, list_info, parent_group);


            ADD_DESC(inp_desc_11);

         {
            /* Auto (0) uses gravity detection, only available on Android.
             * Non-Android defaults to 1 (0° neutral). */
#ifdef ANDROID
            unsigned orientation_default = 0;
#else
            unsigned orientation_default = 1;
#endif
            /* Descriptor holdout: runtime default value. */
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.input_sensor_orientation,
                  MENU_ENUM_LABEL_INPUT_SENSOR_ORIENTATION,
                  MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ORIENTATION,
                  orientation_default,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
         }
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
#ifdef ANDROID
         menu_settings_list_current_add_range(list, list_info, 0, 4, 1.0, true, true);
#else
         menu_settings_list_current_add_range(list, list_info, 1, 4, 1.0, true, true);
#endif
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_sensor_orientation)

            ADD_DESC(inp_desc_12);


         START_SUB_GROUP(list, list_info, "Binds", &group_info, &subgroup_info, parent_group);

            ADD_DESC(inp_desc_13);

         {
            unsigned user;
            char binds_label[NAME_MAX_LENGTH];
            const char *val_input_user_binds =
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS);

            for (user = 0; user < MAX_USERS; user++)
            {
               snprintf(binds_label, sizeof(binds_label),
                     val_input_user_binds, user + 1);

               /* Descriptor holdout: dynamic runtime label through the ALT macro. */
               CONFIG_ACTION_ALT(
                     list, list_info,
                     msg_hash_to_str((enum msg_hash_enums)
                           (MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user)),
                     binds_label,
                     &group_info,
                     &subgroup_info,
                     parent_group);
               (*list)[list_info->index - 1].ui_type        = ST_UI_TYPE_BIND_BUTTON;
               (*list)[list_info->index - 1].index          = user + 1;
               (*list)[list_info->index - 1].index_offset   = user;

               MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
                     (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));
            }
         }

         GROUP_END();
       
   }
}


static void settings_build_recording(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   recording_state_t *recording_st = recording_state_get_ptr();
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
         GROUP_STATE(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS, MENU_ENUM_LABEL_RECORDING_SETTINGS);

            ADD_DESC(recording_desc_0);

            ADD_DESC(recording2_desc_0);

         /* Descriptor holdout: non-general read/write handler. */
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
            SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
            SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_streaming_mode)
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
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 65536, 1, true, true);


            ADD_DESC(recording_desc_1);

            ADD_DESC(recording2_desc_1);

            ADD_DESC(recording_desc_2);

         /* Descriptor holdout: value target lives in recording_state,
          * outside settings_t. */
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
            SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Miscellaneous", &group_info, &subgroup_info, parent_group);

            ADD_DESC(recording_desc_3);

         GROUP_END();
   }
}

static void settings_build_input_hotkey(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         unsigned i;
         START_GROUP(list, list_info, &group_info,
               MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS_BEGIN_STR,
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
                  input_config_bind_map_get_base(i),
                  input_config_bind_map_get_desc(i),
                  &retro_keybinds_1[i],
                  &group_info, &subgroup_info, parent_group);
            (*list)[list_info->index - 1].ui_type        = ST_UI_TYPE_BIND_BUTTON;
            (*list)[list_info->index - 1].bind_type      = i + MENU_SETTINGS_BIND_BEGIN;
            MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info,
                  (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i));
         }

         GROUP_END();
       
   }
}

static void settings_build_frame_throttling(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS, MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS);

            ADD_DESC(frame_throttli_desc_0);

            ADD_DESC(menu_thr_desc);
            ADD_DESC(frame_throttli_desc_1);

#ifdef HAVE_RUNAHEAD
      /* Descriptor holdout: value target outside settings_t. */
      CONFIG_UINT(
            list, list_info,
            &menu_state_get_ptr()->runahead_mode,
            MENU_ENUM_LABEL_RUNAHEAD_MODE,
            MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
            MENU_RUNAHEAD_MODE_OFF,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_runahead_mode)
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], runahead_change_handler)
      menu_settings_list_current_add_range(list, list_info,
            MENU_RUNAHEAD_MODE_OFF, MENU_RUNAHEAD_MODE_LAST - 1, 1, true, true);

      /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      (*list)[list_info->index - 1].offset_by = 1;
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], runahead_change_handler)
      menu_settings_list_current_add_range(list, list_info, 1, MAX_RUNAHEAD_FRAMES, 1, true, true);

            ADD_DESC(frame_throttli_desc_2);

      CONFIG_UINT(
            list, list_info,
            &settings->uints.run_ahead_frames,
            MENU_ENUM_LABEL_PREEMPT_FRAMES,
            MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
            1,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_COMBOBOX;
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      (*list)[list_info->index - 1].offset_by = 1;
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], runahead_change_handler)
      menu_settings_list_current_add_range(list, list_info, 1, MAX_RUNAHEAD_FRAMES, 1, true, true);
#endif

#ifdef ANDROID
            ADD_DESC(frame_throttli_desc_3);
#endif
      GROUP_END();
   }
}

static void settings_build_onscreen_notifications(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),
            parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS);

      START_SUB_GROUP(list, list_info, "Notifications",
            &group_info,
            &subgroup_info,
            parent_group);

#ifdef HAVE_GFX_WIDGETS
                     ADD_DESC(osn_desc_0);

#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
                     ADD_DESC(osn_desc_1);
#else
            ADD_DESC(widget_fs_desc);
      /* The fullscreen variant is an LV row and the LV float grammar
       * has no range or handler slots yet; until it grows them, the
       * customization stays here. The windowed row carries its own. */
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      menu_settings_list_current_add_range(list, list_info, 0.2, 5.0, 0.01, true, true);
#endif

#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
                     ADD_DESC(osn_desc_2);
#endif
#endif
                     ADD_DESC(osn_desc_3);

            ADD_DESC(onscreen_not2_desc_0);

                     ADD_DESC(osn_desc_4);

      END_SUB_GROUP(list, list_info, parent_group);
      START_SUB_GROUP(list, list_info, "Notification Views", &group_info, &subgroup_info, parent_group);

                     ADD_DESC(osn_desc_5);

      GROUP_END();
   }
}

static void settings_build_overlay(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_OVERLAY
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
            parent_group);

      parent_group = MENU_ENUM_LABEL_OVERLAY_SETTINGS_STR;

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

      /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_bool_action_right_with_refresh)
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_enable_toggle_change_handler)

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
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_enable_toggle_change_handler)

      if (video_driver_test_all_flags(GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED))
      {
            ADD_DESC(ovl_desc_0);
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
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_enable_toggle_change_handler)

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
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_enable_toggle_change_handler)

      /* Descriptor holdout: poke tail outside the descriptor grammar. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_input_overlay_show_inputs)
      menu_settings_list_current_add_range(list, list_info, 0, OVERLAY_SHOW_INPUT_LAST-1, 1, true, true);

      /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_input_overlay_show_inputs_port)
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
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_show_mouse_cursor_change_handler)

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
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], overlay_enable_toggle_change_handler)

            ADD_DESC(ovl_desc_1);

            ADD_DESC(overlay2_desc_0);

            ADD_DESC(ovl_desc_2);

      /* Descriptor holdout: poke tail outside the descriptor grammar. */
      CONFIG_UINT(
            list, list_info,
            &settings->uints.input_overlay_dpad_diagonal_sensitivity,
            MENU_ENUM_LABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
            MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
            DEFAULT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler
            );
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_percentage)
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY);
      menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

      CONFIG_UINT(
            list, list_info,
            &settings->uints.input_overlay_abxy_diagonal_sensitivity,
            MENU_ENUM_LABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
            MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
            DEFAULT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler
            );
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_percentage)
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY);
      menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

      CONFIG_UINT(
            list, list_info,
            &settings->uints.input_overlay_analog_recenter_zone,
            MENU_ENUM_LABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
            MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
            DEFAULT_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler
            );
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_percentage)
      menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);

            ADD_DESC(ovl_desc_3);

      GROUP_END();
#endif
   }
}

static void settings_build_osk_overlay(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_OVERLAY
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS),
            parent_group);

      parent_group = MENU_ENUM_LABEL_OVERLAY_SETTINGS_STR;

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

      /* Descriptor holdout: runtime default value. */
      CONFIG_PATH(
            list, list_info,
            settings->paths.path_osk_overlay,
            sizeof(settings->paths.path_osk_overlay),
            MENU_ENUM_LABEL_OSK_OVERLAY_PRESET,
            MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
            settings->paths.directory_osk_overlay,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      MENU_SETTINGS_LIST_CURRENT_ADD_VALUES(list, list_info, "cfg");
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_INIT);

      /* Descriptor holdout: poke tail outside the descriptor grammar. */
      CONFIG_BOOL(
            list, list_info,
            &settings->bools.input_osk_overlay_auto_scale,
            MENU_ENUM_LABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
            MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_bool_action_right_with_refresh)
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

      CONFIG_FLOAT(
            list, list_info,
            &settings->floats.input_osk_overlay_opacity,
            MENU_ENUM_LABEL_OSK_OVERLAY_OPACITY,
            MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
            DEFAULT_INPUT_OVERLAY_OPACITY,
            "%.2f",
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_OVERLAY_SET_ALPHA_MOD);
      menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

      GROUP_END();
#endif
   }
}



static void settings_build_menu(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS),
            parent_group);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_MENU_SETTINGS);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

      if (   string_is_not_equal(settings->arrays.menu_driver, "rgui")
          && string_is_not_equal(settings->arrays.menu_driver, "ozone"))
      {
            ADD_DESC(menu2_desc_0);

            ADD_DESC(menu_desc_0);
      }

      if (   string_is_not_equal(settings->arrays.menu_driver, "rgui")
          && string_is_not_equal(settings->arrays.menu_driver, "xmb"))
      {
            ADD_DESC(menu_desc_1);
      }

      if (string_is_equal(settings->arrays.menu_driver, "xmb"))
      {
            ADD_DESC(menu_desc_2);
      }

            ADD_DESC(menu_desc_3);

#if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)
      if (     memcmp(settings->arrays.menu_driver, "glui", 5) == 0
            || memcmp(settings->arrays.menu_driver, "xmb", 4) == 0
            || memcmp(settings->arrays.menu_driver, "ozone", 6) == 0)
      {
            ADD_DESC(menu_desc_4);
      }
#endif

            ADD_DESC(menu_desc_5);

      if (string_is_equal(settings->arrays.menu_driver, "rgui"))
      {
            ADD_DESC(menu_desc_6);

         if (video_driver_test_all_flags(GFX_CTX_FLAGS_MENU_FRAME_FILTERING))
         {
            ADD_DESC(menu_desc_7);
         }

#if !defined(DINGUX)
            ADD_DESC(menu_desc_8);
#endif

            ADD_DESC(menu_desc_9);

            ADD_DESC(menu2_desc_1);

         /* ps2 and sdl_dingux/sdl_rs90 gfx drivers do
          * not support menu framebuffer transparency */
         if (   !string_is_equal(settings->arrays.video_driver, "ps2")
             && !string_is_equal(settings->arrays.video_driver, "sdl_dingux")
             && !string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
         {
            ADD_DESC(menu_desc_10);
         }

            ADD_DESC(menu_desc_11);
      }

#if defined(HAVE_XMB) || defined (HAVE_OZONE)
      if (     string_is_equal(settings->arrays.menu_driver, "xmb")
            || string_is_equal(settings->arrays.menu_driver, "ozone"))
      {
            ADD_DESC(menu_desc_12);
#ifdef RARCH_MOBILE
         /* We don't want mobile users being able to switch this off. */
         SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], NULL)
         SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], NULL)
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], NULL)
#else
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
         SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)
#endif
      }

      if (string_is_equal(settings->arrays.menu_driver, "xmb"))
      {
            ADD_DESC(menu_desc_13);
      }
#endif

            ADD_DESC(menu_desc_14);

      END_SUB_GROUP(list, list_info, parent_group);

      START_SUB_GROUP(list, list_info, "Navigation", &group_info, &subgroup_info, parent_group);

            ADD_DESC(menu_desc_15);

      END_SUB_GROUP(list, list_info, parent_group);
      START_SUB_GROUP(list, list_info, "Settings View", &group_info, &subgroup_info, parent_group);

            ADD_DESC(menu_desc_16);

            ADD_DESC(menu2_desc_2);

#ifdef HAVE_THREADS
            ADD_DESC(menu_desc_17);
#endif
      END_SUB_GROUP(list, list_info, parent_group);

      START_SUB_GROUP(list, list_info, "Display", &group_info, &subgroup_info, parent_group);

      /* > MaterialUI, XMB and Ozone all support menu scaling */
      if (     memcmp(settings->arrays.menu_driver, "glui", 5) == 0
            || memcmp(settings->arrays.menu_driver, "xmb", 4) == 0
            || memcmp(settings->arrays.menu_driver, "ozone", 6) == 0)
      {
            ADD_DESC(menu_desc_18);
      }

#ifdef HAVE_XMB
      if (string_is_equal(settings->arrays.menu_driver, "xmb"))
      {
         /* only XMB uses these values, don't show
          * them on other drivers. */
            ADD_DESC(menu_desc_19);

            ADD_DESC(menu2_desc_3);

            ADD_DESC(menu_desc_20);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_SHADERPIPELINE
         {
            gfx_ctx_flags_t flags;
            flags.flags     = 0;
            video_context_driver_get_flags(&flags);

            if (
                     BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG)
                  || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_HLSL))
            {
            ADD_DESC(menu_desc_21);
            }
         }
#endif
#endif

            ADD_DESC(menu_desc_22);
    }
#endif
      if (string_is_equal(settings->arrays.menu_driver, "ozone"))
      {
            ADD_DESC(menu_desc_23);
      }

            ADD_DESC(menu_desc_24);

#ifdef HAVE_LAKKA
            ADD_DESC(menu_quit_lakka_desc);
#elif !defined(IOS)
            ADD_DESC(menu_desc_25);
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2)
            ADD_DESC(menu_desc_26);
#else
#if !defined(IOS)
         if (frontend_driver_has_fork())
            ADD_DESC(menu_desc_27);
#endif
#endif

            ADD_DESC(menu_desc_28);

            ADD_DESC(menu2_desc_4);

            ADD_DESC(menu_desc_29);

#ifdef HAVE_MATERIALUI
      if (string_is_equal(settings->arrays.menu_driver, "glui"))
      {
         /* only MaterialUI uses these values, don't show
          * them on other drivers. */
            ADD_DESC(menu_desc_30);
      }
#endif

#ifdef HAVE_OZONE
      if (string_is_equal(settings->arrays.menu_driver, "ozone"))
      {
            ADD_DESC(menu_desc_31);

            ADD_DESC(menu2_desc_5);

            ADD_DESC(menu_desc_32);
      }
#endif

            ADD_DESC(menu_desc_33);

      if (string_is_equal(settings->arrays.menu_driver, "rgui"))
      {
            ADD_DESC(menu_desc_34);
      }

      if (     memcmp(settings->arrays.menu_driver, "rgui", 5) == 0
            || memcmp(settings->arrays.menu_driver, "xmb", 4) == 0
            || memcmp(settings->arrays.menu_driver, "ozone", 6) == 0)
      {
            ADD_DESC(menu_desc_35);
      }

      if (   string_is_equal(settings->arrays.menu_driver, "xmb")
          || string_is_equal(settings->arrays.menu_driver, "ozone")
          || string_is_equal(settings->arrays.menu_driver, "rgui")
          || string_is_equal(settings->arrays.menu_driver, "glui"))
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

         /* Descriptor holdout: poke tail outside the descriptor grammar. */
         CONFIG_UINT(
               list, list_info,
               &settings->uints.gfx_thumbnails,
               MENU_ENUM_LABEL_THUMBNAILS,
               thumbnails_label_value,
               DEFAULT_GFX_THUMBNAILS_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_menu_thumbnails)
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_THUMBNAIL_MODE_LAST - PLAYLIST_THUMBNAIL_MODE_OFF - 1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;

         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_left_thumbnails,
               MENU_ENUM_LABEL_LEFT_THUMBNAILS,
               left_thumbnails_label_value,
               DEFAULT_MENU_LEFT_THUMBNAILS_DEFAULT,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_menu_thumbnails)
         menu_settings_list_current_add_range(list, list_info, 0, PLAYLIST_THUMBNAIL_MODE_LAST - PLAYLIST_THUMBNAIL_MODE_OFF - 1, 1, true, true);
         (*list)[list_info->index - 1].ui_type   = ST_UI_TYPE_UINT_RADIO_BUTTONS;
      }

      if (string_is_equal(settings->arrays.menu_driver, "xmb"))
      {
            ADD_DESC(menu_desc_36);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.menu_xmb_thumbnail_scale_factor,
               MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
               MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
               DEFAULT_XMB_THUMBNAIL_SCALE_FACTOR,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         (*list)[list_info->index - 1].offset_by = 30;
         menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index - 1].offset_by, 100, 1, true, true);
      }

      if (     memcmp(settings->arrays.menu_driver, "glui", 5) == 0
            || memcmp(settings->arrays.menu_driver, "xmb", 4) == 0
            || memcmp(settings->arrays.menu_driver, "ozone", 6) == 0)
      {
            ADD_DESC(menu_desc_37);
      }

      if (string_is_equal(settings->arrays.menu_driver, "rgui"))
      {
            ADD_DESC(menu_desc_38);
      }

            ADD_DESC(menu_desc_39);


      GROUP_END();
   }
}



static void settings_build_power_management(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS, MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS);

#ifdef ANDROID
            ADD_DESC(power_manageme_desc_0_s0);
#endif
#ifdef HAVE_LAKKA
            ADD_DESC(power_manageme_desc_0_s1);
#endif
#ifndef HAVE_LAKKA
      if (frontend_driver_has_gamemode())
            ADD_DESC(power_manageme_desc_1);

      GROUP_END();
#endif

   }
}



static void settings_build_ai_service(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_TRANSLATE
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS, MENU_ENUM_LABEL_AI_SERVICE_SETTINGS);

            ADD_DESC(ai_service_desc_0);

      CONFIG_STRING_OPTIONS(
            list, list_info,
            settings->arrays.ai_service_backend,
            sizeof(settings->arrays.ai_service_backend),
            MENU_ENUM_LABEL_AI_SERVICE_BACKEND,
            MENU_ENUM_LABEL_VALUE_AI_SERVICE_BACKEND,
            config_get_default_ai_service_backend(),
            strdup(config_get_ai_service_backend_options()),
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_action_ok_uint)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_driver)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_driver)

      /* Descriptor holdout: poke tail outside the descriptor grammar. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

            ADD_DESC(ai_service_desc_1);


      GROUP_END();
#endif
   }
}

static void settings_build_user_interface(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS, MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS);

      {
         CONFIG_STRING_OPTIONS(
            list, list_info,
            settings->paths.app_icon,
            sizeof(settings->paths.app_icon),
            MENU_ENUM_LABEL_APPICON_SETTINGS,
            MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
            "",
            (char*)calloc(1, sizeof(char)),
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_action_ok_uint)
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], appicon_change_handler)
      }

                     ADD_DESC(ui_desc_0);
#ifdef _3DS
      {
         u8 device_model = 0xFF;

         /* Only O3DS and O3DSXL support running in 'dual-framebuffer'
          * mode with the parallax barrier disabled
          * (i.e. these are the only platforms that can use
          * CTR_VIDEO_MODE_2D_400X240 and CTR_VIDEO_MODE_2D_800X240) */
         CFGU_GetSystemModel(&device_model); /* (0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL) */

                  {
         /* NB: max depends on the runtime system model, so this table
          * cannot be 'static const' (the initializer is non-constant). */
         const setting_desc_t ui_desc_1[] = {
            SDESC_UINT_ROW_EX(video_3ds_display_mode, VIDEO_3DS_DISPLAY_MODE,
                  DEFAULT_VIDEO_3DS_DISPLAY_MODE,
                  SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0,
                  0, CTR_VIDEO_MODE_LAST - ((device_model != 3) ? 1 : 3), 1, 0,
                  setting_action_ok_uint, setting_get_string_representation_uint_video_3ds_display_mode,
                  NULL, NULL, NULL, NULL, 0),
         };
         ADD_DESC(ui_desc_1);
      }
      }

      /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
      CONFIG_BOOL(
            list, list_info,
            &settings->bools.new3ds_speedup_enable,
            MENU_ENUM_LABEL_NEW3DS_SPEEDUP_ENABLE,
            MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
            DEFAULT_NEW_3DS_SPEEDUP_ENABLE,
            MENU_ENUM_LABEL_VALUE_OFF,
            MENU_ENUM_LABEL_VALUE_ON,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler,
            SD_FLAG_CMD_APPLY_AUTO);
      SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], new3ds_speedup_change_handler)

                     ADD_DESC(ui_desc_2);
#ifdef CONSOLE_LOG
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT_FROM_TOGGLE);
#endif

      /* Descriptor holdout: runtime default value. */
      CONFIG_DIR(
            list, list_info,
            settings->paths.directory_bottom_assets,
            sizeof(settings->paths.directory_bottom_assets),
            MENU_ENUM_LABEL_BOTTOM_ASSETS_DIRECTORY,
            MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
            g_defaults.dirs[DEFAULT_DIR_BOTTOM_ASSETS],
            MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)
      MENU_SETTINGS_LIST_CURRENT_ADD_CMD(list, list_info, CMD_EVENT_REINIT);

                     ADD_DESC(ui_desc_3);
#endif

#ifdef HAVE_NETWORKING
                     ADD_DESC(ui_desc_4);

#if !defined(HAVE_LAKKA)
                     ADD_DESC(ui_desc_5);
#endif
#endif

#ifdef HAVE_MIST
                     ADD_DESC(ui_desc_6);
#endif

                     ADD_DESC(ui_desc_7);

#ifdef HAVE_MIST
                     ADD_DESC(ui_desc_8);
#endif

#ifdef HAVE_SMBCLIENT
                     ADD_DESC(ui_desc_9);
#endif

                     ADD_DESC(ui_desc_10);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
      {
         gfx_ctx_flags_t flags;
         flags.flags     = 0;
         video_context_driver_get_flags(&flags);

         if (
                  BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG)
               || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL)
               || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG)
               || BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_HLSL))
         {
                           ADD_DESC(ui_desc_11);
         }
      }
#endif

                     ADD_DESC(ui_desc_12);
      /* Descriptor holdout: poke tail outside the descriptor grammar. */
      CONFIG_BOOL(
            list, list_info,
            &settings->bools.menu_scroll_fast,
            MENU_ENUM_LABEL_MENU_SCROLL_FAST,
            MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
            DEFAULT_MENU_SCROLL_FAST,
            MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
            MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler,
            SD_FLAG_NONE);

                     ADD_DESC(ui_desc_13);
      GROUP_END();
   }
}

static void settings_build_playlist(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            MENU_ENUM_LABEL_PLAYLIST_SETTINGS_BEGIN_STR,
            parent_group);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

      START_SUB_GROUP(list, list_info, "History", &group_info, &subgroup_info, parent_group);

                     ADD_DESC(pl_desc_0);

      END_SUB_GROUP(list, list_info, parent_group);

      START_SUB_GROUP(list, list_info, "Playlist", &group_info, &subgroup_info, parent_group);

      /* Favourites size is traditionally associated with
       * history size, but they are in fact unrelated. We
       * therefore place this entry outside the "History"
       * sub group. */
                     ADD_DESC(pl_desc_1);

      /* Playlist entry index display and content specific history icon
       * are currently supported only by Ozone & XMB */
      if (   string_is_equal(settings->arrays.menu_driver, "xmb")
          || string_is_equal(settings->arrays.menu_driver, "ozone"))
      {
                        ADD_DESC(pl_desc_2);
      }

                     ADD_DESC(pl_desc_3);

#if defined(HAVE_OZONE) || defined(HAVE_XMB)
      if (   string_is_equal(settings->arrays.menu_driver, "ozone")
          || string_is_equal(settings->arrays.menu_driver, "xmb"))
      {
                        ADD_DESC(pl_desc_4);
      }
#endif

      GROUP_END();
   }
}

static void settings_build_cheevos(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CHEEVOS
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS, MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS);

      /* Descriptor holdout: non-general read/write handler. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)

            ADD_DESC(cheevos_desc_0);

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

      GROUP_END();
#endif
   }
}

static void settings_build_cheevos_appearance(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CHEEVOS
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS, MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_SETTINGS);

#ifdef HAVE_GFX_WIDGETS
      CONFIG_UINT(
         list, list_info,
         &settings->uints.cheevos_appearance_anchor,
         MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_ANCHOR,
         MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
         DEFAULT_CHEEVOS_APPEARANCE_ANCHOR,
         &group_info,
         &subgroup_info,
         parent_group,
         cheevos_appearance_write_handler,
         general_read_handler);
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_cheevos_appearance_anchor)
      menu_settings_list_current_add_range(list, list_info, 0, CHEEVOS_APPEARANCE_ANCHOR_LAST - 1, 1, true, true);
      (*list)[list_info->index - 1].ui_type = ST_UI_TYPE_UINT_COMBOBOX;

      CONFIG_BOOL(
         list, list_info,
         &settings->bools.cheevos_appearance_padding_auto,
         MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
         MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
         true,
         MENU_ENUM_LABEL_VALUE_OFF,
         MENU_ENUM_LABEL_VALUE_ON,
         &group_info,
         &subgroup_info,
         parent_group,
         cheevos_appearance_write_handler,
         general_read_handler,
         SD_FLAG_NONE
      );
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)

      CONFIG_FLOAT(
         list, list_info,
         &settings->floats.cheevos_appearance_padding_h,
         MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_H,
         MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
         DEFAULT_CHEEVOS_APPEARANCE_PADDING_H,
         "%.2f",
         &group_info,
         &subgroup_info,
         parent_group,
         cheevos_appearance_write_handler,
         general_read_handler
      );
      menu_settings_list_current_add_range(list, list_info, 0.0, 0.5, 0.01, true, true);

      CONFIG_FLOAT(
         list, list_info,
         &settings->floats.cheevos_appearance_padding_v,
         MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_V,
         MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
         DEFAULT_CHEEVOS_APPEARANCE_PADDING_V,
         "%.2f",
         &group_info,
         &subgroup_info,
         parent_group,
         cheevos_appearance_write_handler,
         general_read_handler
      );
      menu_settings_list_current_add_range(list, list_info, 0.0, 0.5, 0.01, true, true);
#endif

      GROUP_END();
#endif
   }
}

static void settings_build_cheevos_visibility(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
#ifdef HAVE_CHEEVOS
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS, MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_SETTINGS);

                     ADD_DESC(chv_desc_0);

      CONFIG_BOOL(
         list, list_info,
         &settings->bools.cheevos_visibility_lboard_trackers,
         MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
         MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
         DEFAULT_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
         MENU_ENUM_LABEL_VALUE_OFF,
         MENU_ENUM_LABEL_VALUE_ON,
         &group_info,
         &subgroup_info,
         parent_group,
         achievement_leaderboard_trackers_enabled_write_handler,
         general_read_handler,
         SD_FLAG_NONE
      );

                     ADD_DESC(chv_desc_1);


      GROUP_END();
#endif
   }
}

static void settings_build_core_updater(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS, MENU_ENUM_LABEL_UPDATER_SETTINGS);
#ifdef HAVE_NETWORKING

#ifdef HAVE_UPDATE_CORES
#if defined(ANDROID)
      /* Play Store builds do not fetch cores
       * from the buildbot */
      if (!play_feature_delivery_enabled())
#endif
      {

         /* Descriptor holdout: poke tail outside the descriptor grammar. */
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
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)
      }
#endif

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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

            ADD_DESC(core_updater_desc_0);

#ifdef HAVE_UPDATE_CORES
            ADD_DESC(core_updater_desc_1);

#if defined(ANDROID)
      /* Play Store builds do not support automatic
       * core backups */
      if (!play_feature_delivery_enabled())
#endif
      {
            ADD_DESC(core_updater_desc_2);

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
            SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index - 1].offset_by, 500, 1, true, true);
      }
#endif
#endif
      GROUP_END();
   }
}

static void settings_build_netplay(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS),
            parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS);

#ifdef HAVE_SMBCLIENT
      if (settings->bools.settings_show_smb_client)
      {
            ADD_DESC(np_desc_0);
      }
#endif

      START_SUB_GROUP(list, list_info, "Netplay", &group_info, &subgroup_info, parent_group);

      {
#if defined(HAVE_NETWORKING)
         unsigned user;
         char dev_req_label[64];
         char dev_req_value[64];

            ADD_DESC(np_desc_1);

         /* Descriptor holdout: poke tail outside the descriptor grammar. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_string_action_ok_netplay_mitm_server)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_netplay_mitm_server)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_netplay_mitm_server)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_netplay_mitm_server)

            ADD_DESC(netplay2_desc_0);

            ADD_DESC(np_desc_2);

            ADD_DESC(netplay2_desc_1);

            ADD_DESC(np_desc_3);

         /* Descriptor holdout: value target outside settings_t. */
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
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
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
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);

            ADD_DESC(np_desc_4);

         for (user = 0; user < MAX_USERS; user++)
         {
            snprintf(dev_req_label, sizeof(dev_req_label),
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_I), user + 1);
            snprintf(dev_req_value, sizeof(dev_req_value),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I), user + 1);
            /* Descriptor holdout: dynamic runtime label through the ALT macro. */
            CONFIG_BOOL_ALT(
                  list, list_info,
                  &settings->bools.netplay_request_devices[user],
                  dev_req_label,
                  dev_req_value,
                  false,
                  MENU_ENUM_LABEL_VALUE_NO,
                  MENU_ENUM_LABEL_VALUE_YES,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
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
            ADD_DESC(np_desc_5);

         /* Descriptor holdout: non-general read/write handler. */
         CONFIG_UINT(
               list, list_info,
               &settings->uints.network_cmd_port,
               MENU_ENUM_LABEL_NETWORK_CMD_PORT,
               MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
               DEFAULT_NETWORK_CMD_PORT,
               &group_info,
               &subgroup_info,
               parent_group,
               NULL,
               NULL);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 0, 65535, 1, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

            ADD_DESC(np_desc_6);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.network_remote_base_port,
               MENU_ENUM_LABEL_NETWORK_REMOTE_PORT,
               MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
               DEFAULT_NETWORK_REMOTE_BASE_PORT,
               &group_info,
               &subgroup_info,
               parent_group,
               NULL,
               NULL);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 0, 65535, 1, true, true);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ADVANCED);

         /* TODO/FIXME - add enum_idx */
         {
            unsigned max_users                    = settings->uints.input_max_users;
            const char *lbl_network_remote_enable =
               msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE);
            const char *val_network_remote_enable =
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE);
            for (user = 0; user < max_users; user++)
            {
               char s1[32], s2[64];
               size_t _len = strlcpy(s1, lbl_network_remote_enable, sizeof(s1));
               snprintf(s1 + _len, sizeof(s1) - _len, "_user_p%d", user + 1);
               snprintf(s2, sizeof(s2), val_network_remote_enable, user + 1);

               /* Descriptor holdout: dynamic runtime label through the ALT macro. */
               CONFIG_BOOL_ALT(
                     list, list_info,
                     &settings->bools.network_remote_enable_user[user],
                     /* todo: figure out this value, it's working fine but I don't think this is correct */
                     s1,
                     s2,
                     false,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_ADVANCED);
               MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user));
            }
         }

            ADD_DESC(np_desc_7);
#endif
            ADD_DESC(np_desc_8);
#endif
      }
      GROUP_END();
   }
}

static void settings_build_lakka_services(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
#if defined(HAVE_LAKKA)
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
               &group_info, &subgroup_info, parent_group);
         /* Descriptor holdout: change_handler poke; descriptor slot deferred to the single-source phase. */
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
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], ssh_enable_toggle_change_handler)

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
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], samba_enable_toggle_change_handler)

#ifdef HAVE_BLUETOOTH
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
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], bluetooth_enable_toggle_change_handler)
#endif
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
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], localap_enable_toggle_change_handler)
#endif

#ifdef HAVE_RETROFLAG
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.safeshutdown_enable,
               MENU_ENUM_LABEL_SAFESHUTDOWN_ENABLE,
               MENU_ENUM_LABEL_VALUE_SAFESHUTDOWN_ENABLE,
               true,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], safeshutdown_enable_toggle_change_handler)
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
               general_write_handler,
               general_read_handler);
         SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_IS_DRIVER);
         SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_action_ok_uint)
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], timezone_change_handler)

         GROUP_END();
#endif
       
   }
}

#ifdef HAVE_LAKKA_SWITCH
static void settings_build_lakka_switch_options(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
       
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS),
               &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.switch_oc,
               MENU_ENUM_LABEL_SWITCH_OC_ENABLE,
               MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
               DEFAULT_SWITCH_OC,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], switch_oc_enable_toggle_change_handler)

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.switch_cec,
               MENU_ENUM_LABEL_SWITCH_CEC_ENABLE,
               MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
               DEFAULT_SWITCH_CEC,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], switch_cec_enable_toggle_change_handler)

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.bluetooth_ertm_disable,
               MENU_ENUM_LABEL_BLUETOOTH_ERTM_DISABLE,
               MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
               DEFAULT_BLUETOOTH_ERTM,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         SETTINGS_ACTION_SET(change, &(*list)[list_info->index - 1], bluetooth_ertm_disable_toggle_change_handler)
         GROUP_END();
       
   }
}
#endif

static void settings_build_user(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_USER_SETTINGS, MENU_ENUM_LABEL_USER_SETTINGS);

            ADD_DESC(user_desc_0);

            ADD_DESC(user2_desc_0);

#ifdef HAVE_LANGEXTRA
      /* Descriptor holdout: value target outside settings_t. */
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], &setting_action_ok_uint)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], &setting_uint_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], &setting_uint_action_right_with_refresh)
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], &setting_get_string_representation_uint_user_language)
      (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_UINT_COMBOBOX;
#endif

#ifdef HAVE_GAME_AI
            ADD_DESC(user_desc_1);
#endif

      GROUP_END();
   }
}

static void settings_build_user_accounts(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END, MENU_ENUM_LABEL_SETTINGS);

#ifdef HAVE_CHEEVOS
            ADD_DESC(user_accounts_desc_0_s0);
#endif
#ifdef HAVE_NETWORKING
#if !IOS
            ADD_DESC(user_accounts_desc_0_s1);
#endif
#endif
      GROUP_END();
   }
}

static void settings_build_user_accounts_youtube(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE, MENU_ENUM_LABEL_SETTINGS);

      /* Descriptor holdout: non-general read/write handler. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

      GROUP_END();
   }
}

static void settings_build_user_accounts_twitch(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH, MENU_ENUM_LABEL_SETTINGS);

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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

      GROUP_END();
   }
}

static void settings_build_user_accounts_facebook(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK, MENU_ENUM_LABEL_SETTINGS);

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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

      GROUP_END();
   }
}

static void settings_build_user_accounts_kick(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_ACCOUNTS_KICK, MENU_ENUM_LABEL_SETTINGS);

      CONFIG_STRING(
            list, list_info,
            settings->arrays.kick_stream_key,
            sizeof(settings->arrays.kick_stream_key),
            MENU_ENUM_LABEL_KICK_STREAM_KEY,
            MENU_ENUM_LABEL_VALUE_KICK_STREAM_KEY,
            "",
            &group_info,
            &subgroup_info,
            parent_group,
            update_streaming_url_write_handler,
            general_read_handler);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
      (*list)[list_info->index - 1].ui_type       = ST_UI_TYPE_STRING_LINE_EDIT;
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

      GROUP_END();
   }
}


static void settings_build_directory(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   recording_state_t *recording_st = recording_state_get_ptr();
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS),
            parent_group);
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

      START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

                     ADD_DESC(dir_desc_0);
      if (string_is_not_equal(settings->arrays.record_driver, "null"))
      {
         /* Descriptor holdout: value target outside settings_t. */
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
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)

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
         SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)
      }
            ADD_DESC(dir_desc_1);

      /* Descriptor holdouts: value targets resolved through
       * dir_get_ptr() at runtime, outside settings_t. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)

      /* Descriptor holdout: value target outside settings_t. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], directory_action_start_generic)

                     ADD_DESC(dir_desc_2);

      GROUP_END();
   }
}

static void settings_build_privacy(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS), parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS);

      START_SUB_GROUP(list, list_info, "State",
            &group_info, &subgroup_info, parent_group);

      if (string_is_not_equal(settings->arrays.camera_driver, "null"))
      {
            ADD_DESC(privacy_desc_0);
      }

#ifdef HAVE_DISCORD
            ADD_DESC(privacy_desc_1);
#endif
      if (string_is_not_equal(settings->arrays.location_driver, "null"))
      {
            ADD_DESC(privacy_desc_2);
      }

      GROUP_END();
   }
}

static void settings_build_midi(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS), parent_group);

      parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MIDI_SETTINGS);

      START_SUB_GROUP(list, list_info, "State",
            &group_info, &subgroup_info, parent_group);

#if !defined(RARCH_CONSOLE)
      /* Descriptor holdout: poke tail outside the descriptor grammar. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_string_action_start_midi_device)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_midi_input)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_midi_input)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_string_action_ok_midi_device)

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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_string_action_start_midi_device)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_string_action_left_midi_output)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_string_action_right_midi_output)
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_string_action_ok_midi_device)

            ADD_DESC(midi_desc_0);
#endif

      GROUP_END();
   }
}

static void settings_build_manual_content_scan(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      START_GROUP(list, list_info, &group_info,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST), parent_group);

      parent_group = MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST_STR;

      START_SUB_GROUP(list, list_info, "State",
            &group_info, &subgroup_info, parent_group);

      /* Descriptor holdout: value target outside settings_t. */
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

      CONFIG_BOOL(
            list, list_info,
            manual_content_scan_get_omit_db_ref_ptr(),
            MENU_ENUM_LABEL_SCAN_OMIT_DB_REF,
            MENU_ENUM_LABEL_VALUE_SCAN_OMIT_DB_REF,
            false,
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
      SETTINGS_ACTION_SET(start, &(*list)[list_info->index - 1], setting_generic_action_start_default)

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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)

      CONFIG_BOOL(
            list, list_info,
            manual_content_scan_get_search_archives_ptr(),
            MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
            MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
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
            manual_content_scan_get_scan_single_file_ptr(),
            MENU_ENUM_LABEL_SCAN_SINGLE_FILE,
            MENU_ENUM_LABEL_VALUE_SCAN_SINGLE_FILE,
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
      SETTINGS_ACTION_SET(ok, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(left, &(*list)[list_info->index - 1], setting_bool_action_left_with_refresh)
      SETTINGS_ACTION_SET(right, &(*list)[list_info->index - 1], setting_bool_action_right_with_refresh)

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

      GROUP_END();
   }
}

#ifdef HAVE_MIST
#endif

#ifdef HAVE_SMBCLIENT
static void settings_build_smbclient(
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)settings; (void)global; (void)group_info; (void)subgroup_info;
   {
      GROUP_STATE(MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SETTINGS, MENU_ENUM_LABEL_SMB_CLIENT_SETTINGS);

            ADD_DESC(smbclient_desc_0);

      /* Descriptor holdout: non-general read/write handler. */
      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_server_address,
         sizeof(settings->arrays.smb_client_server_address),
         MENU_ENUM_LABEL_SMB_CLIENT_SERVER,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SERVER,
         "",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_share,
         sizeof(settings->arrays.smb_client_share),
         MENU_ENUM_LABEL_SMB_CLIENT_SHARE,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SHARE,
         "",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_subdir,
         sizeof(settings->arrays.smb_client_subdir),
         MENU_ENUM_LABEL_SMB_CLIENT_SUBDIR,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SUBDIR,
         "",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_username,
         sizeof(settings->arrays.smb_client_username),
         MENU_ENUM_LABEL_SMB_CLIENT_USERNAME,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_USERNAME,
         "",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_password,
         sizeof(settings->arrays.smb_client_password),
         MENU_ENUM_LABEL_SMB_CLIENT_PASSWORD,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_PASSWORD,
         "",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);
      SETTINGS_ACTION_SET(repr, &(*list)[list_info->index - 1], setting_get_string_representation_smb_password)

      CONFIG_STRING(
         list, list_info,
         settings->arrays.smb_client_workgroup,
         sizeof(settings->arrays.smb_client_workgroup),
         MENU_ENUM_LABEL_SMB_CLIENT_WORKGROUP,
         MENU_ENUM_LABEL_VALUE_SMB_CLIENT_WORKGROUP,
         "WORKGROUP",
         &group_info,
         &subgroup_info,
         parent_group,
         NULL,
         NULL);
      SETTINGS_DATA_LIST_CURRENT_ADD_FLAGS(list, list_info, SD_FLAG_ALLOW_INPUT);

            ADD_DESC(smbclient_desc_1);

      GROUP_END();
   }
}
#endif

typedef struct settings_build_entry
{
   enum settings_list_type type;
   void (*build)(settings_t *settings, global_t *global,
         rarch_setting_t **list, rarch_setting_info_t *list_info,
         const char *parent_group);
   /* Data-driven groups: when build is NULL the generic builder runs
    * from these fields and the list needs no code at all. */
   const setting_desc_t *rows;
   unsigned count;
   enum msg_hash_enums value_label;   /* group title             */
   enum msg_hash_enums idx_label;     /* enum idx, 0 for none    */
   enum msg_hash_enums parent_label;  /* parent group, 0 keeps   */
} settings_build_entry_t;

static void settings_build_desc_group(
      const settings_build_entry_t *e,
      settings_t *settings, global_t *global,
      rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   group_info.name    = NULL;
   subgroup_info.name = NULL;
   (void)global;
   START_GROUP(list, list_info, &group_info,
         msg_hash_to_str(e->value_label), parent_group);
   if (e->idx_label)
      MENU_SETTINGS_LIST_CURRENT_ADD_ENUM_IDX_PTR(list, list_info, e->idx_label);
   if (e->parent_label)
      parent_group = msg_hash_to_str(e->parent_label);
   START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);
   if (e->rows)
      settings_list_add_desc(list, list_info, settings,
            e->rows, e->count, &group_info, &subgroup_info, parent_group);
   GROUP_END();
}

static const settings_build_entry_t settings_build_registry[] = {
   { SETTINGS_LIST_MAIN_MENU, settings_build_main_menu, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_DRIVERS, settings_build_drivers, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CORE, settings_build_core, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CONFIGURATION, settings_build_configuration, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_LOGGING, settings_build_logging, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_SAVING, settings_build_saving, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CLOUD_SYNC, settings_build_cloud_sync, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_FRAME_TIME_COUNTER, NULL,
     frame_time_cou_desc_0, (unsigned)ARRAY_SIZE(frame_time_cou_desc_0),
     MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS },
   { SETTINGS_LIST_REWIND, settings_build_rewind, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CHEATS, NULL,
     cheats_desc_0, (unsigned)ARRAY_SIZE(cheats_desc_0),
     MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_CHEAT_SETTINGS },
   { SETTINGS_LIST_CHEAT_DETAILS, settings_build_cheat_details, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CHEAT_SEARCH, settings_build_cheat_search, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_VIDEO, settings_build_video, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CRT_SWITCHRES, NULL,
     crt_switchres_desc_0, (unsigned)ARRAY_SIZE(crt_switchres_desc_0),
     MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
     MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,
     MENU_ENUM_LABEL_SETTINGS },
   { SETTINGS_LIST_MENU_SOUNDS, NULL,
     menu_sounds_desc_0, (unsigned)ARRAY_SIZE(menu_sounds_desc_0),
     MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_AUDIO_SETTINGS },
   { SETTINGS_LIST_AUDIO, settings_build_audio, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#ifdef HAVE_MICROPHONE
   { SETTINGS_LIST_MICROPHONE, settings_build_microphone, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#endif
   { SETTINGS_LIST_INPUT, settings_build_input, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_INPUT_TURBO_FIRE, NULL,
     input_turbo_fi_desc_0, (unsigned)ARRAY_SIZE(input_turbo_fi_desc_0),
     MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS },
   { SETTINGS_LIST_RECORDING, settings_build_recording, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_INPUT_HOTKEY, settings_build_input_hotkey, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_FRAME_THROTTLING, settings_build_frame_throttling, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_ONSCREEN_NOTIFICATIONS, settings_build_onscreen_notifications, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_OVERLAY, settings_build_overlay, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_OSK_OVERLAY, settings_build_osk_overlay, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   #ifdef HAVE_OVERLAY
   { SETTINGS_LIST_OVERLAY_LIGHTGUN, NULL,
     overlay_lightg_desc_0, (unsigned)ARRAY_SIZE(overlay_lightg_desc_0),
     MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_OVERLAY_SETTINGS },
#endif
   #ifdef HAVE_OVERLAY
   { SETTINGS_LIST_OVERLAY_MOUSE, NULL,
     overlay_mouse_desc_0, (unsigned)ARRAY_SIZE(overlay_mouse_desc_0),
     MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_OVERLAY_SETTINGS },
#endif
   { SETTINGS_LIST_MENU, settings_build_menu, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_MENU_FILE_BROWSER, NULL,
     menu_file_brow_desc_0, (unsigned)ARRAY_SIZE(menu_file_brow_desc_0),
     MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS },
   { SETTINGS_LIST_MULTIMEDIA, NULL,
     multimedia_desc_0, (unsigned)ARRAY_SIZE(multimedia_desc_0),
     MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_SETTINGS },
   { SETTINGS_LIST_POWER_MANAGEMENT, settings_build_power_management, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_WIFI_MANAGEMENT, NULL,
     wifi_managemen_desc_0, (unsigned)ARRAY_SIZE(wifi_managemen_desc_0),
     MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_WIFI_SETTINGS },
   { SETTINGS_LIST_ACCESSIBILITY, NULL,
     accessibility_desc_0, (unsigned)ARRAY_SIZE(accessibility_desc_0),
     MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS },
   { SETTINGS_LIST_AI_SERVICE, settings_build_ai_service, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_INTERFACE, settings_build_user_interface, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_PLAYLIST, settings_build_playlist, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CHEEVOS, settings_build_cheevos, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CHEEVOS_APPEARANCE, settings_build_cheevos_appearance, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CHEEVOS_VISIBILITY, settings_build_cheevos_visibility, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_CORE_UPDATER, settings_build_core_updater, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_NETPLAY, settings_build_netplay, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_LAKKA_SERVICES, settings_build_lakka_services, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#ifdef HAVE_LAKKA_SWITCH
   { SETTINGS_LIST_LAKKA_SWITCH_OPTIONS, settings_build_lakka_switch_options, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#endif
   { SETTINGS_LIST_USER, settings_build_user, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS, settings_build_user_accounts, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS_YOUTUBE, settings_build_user_accounts_youtube, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS_TWITCH, settings_build_user_accounts_twitch, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS_FACEBOOK, settings_build_user_accounts_facebook, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS_KICK, settings_build_user_accounts_kick, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS, NULL,
#ifdef HAVE_CHEEVOS
     cheevos_acct_desc, (unsigned)ARRAY_SIZE(cheevos_acct_desc),
#else
     /* group markers still emit without the feature, matching the
      * old function whose rows alone were guarded */
     NULL, 0,
#endif
     MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_SETTINGS },
   { SETTINGS_LIST_DIRECTORY, settings_build_directory, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_PRIVACY, settings_build_privacy, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_MIDI, settings_build_midi, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
   { SETTINGS_LIST_MANUAL_CONTENT_SCAN, settings_build_manual_content_scan, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#ifdef HAVE_MIST
   { SETTINGS_LIST_STEAM, NULL,
     steam_desc_0, (unsigned)ARRAY_SIZE(steam_desc_0),
     MENU_ENUM_LABEL_VALUE_STEAM_SETTINGS,
     MSG_UNKNOWN,
     MENU_ENUM_LABEL_VALUE_STEAM_SETTINGS },
#endif
#ifdef HAVE_SMBCLIENT
   { SETTINGS_LIST_SMBCLIENT, settings_build_smbclient, NULL, 0, MSG_UNKNOWN, MSG_UNKNOWN, MSG_UNKNOWN },
#endif
};

static bool setting_append_list(
      settings_t *settings,
      global_t *global,
      enum settings_list_type type,
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   size_t i;
   for (i = 0; i < ARRAY_SIZE(settings_build_registry); i++)
   {
      if (settings_build_registry[i].type != type)
         continue;
      if (settings_build_registry[i].build)
         settings_build_registry[i].build(settings, global,
               list, list_info, parent_group);
      else
         settings_build_desc_group(&settings_build_registry[i],
               settings, global, list, list_info, parent_group);
      return true;
   }
   return true;
}

void menu_setting_free(rarch_setting_t *setting)
{
   unsigned values, n;
   rarch_setting_t **list = NULL;

   if (!setting)
      return;

   if (setting && setting == settings_lazy_token)
      settings_lazy_free();

   list                   = (rarch_setting_t**)&setting;

   /* Free data which was previously tagged */
   for (; setting->type != ST_NONE; (*list = *list + 1))
      for (values = setting->free_flags, n = 0; values != 0; values >>= 1, n++)
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
   (*&list)[pos].aux.rounding_fraction                = NULL; \
   (*&list)[pos].name                             = NULL; \
   (*&list)[pos].short_description                = NULL; \
   (*&list)[pos].values                           = NULL; \
   (*&list)[pos].actions                          = &settings_actions_none; \
   (*&list)[pos].default_value.fraction           = 0.0f; \
   (*&list)[pos].value.target.fraction            = NULL; \
   (*&list)[pos].cmd_trigger_idx                  = CMD_EVENT_NONE; \
}

static rarch_setting_t *settings_lazy_get(unsigned k)
{
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   rarch_setting_t *sl       = NULL;
   rarch_setting_t **lp      = NULL;
   rarch_setting_info_t li;
   rarch_setting_info_t *lip = NULL;
   unsigned j;
   if (k >= settings_lazy_nbuilders)
      return NULL;
   if (settings_lazy_lists[k])
      return settings_lazy_lists[k];
   li.index = 0;
   li.size  = 32;
   if (!(sl = (rarch_setting_t*)malloc(li.size * sizeof(*sl))))
      return NULL;
   for (j = 0; j < (unsigned)li.size; j++)
   {
      MENU_SETTING_INITIALIZE((&sl)[0], j);
   }
   if (!setting_append_list(settings, global,
         settings_list_build_order[k], &sl, &li,
         MENU_ENUM_LABEL_MAIN_MENU_STR))
   {
      free(sl);
      return NULL;
   }
   lp  = &sl;
   lip = &li;
   if (!SETTINGS_LIST_APPEND(lp, lip))
   {
      free(sl);
      return NULL;
   }
   MENU_SETTING_INITIALIZE(sl, li.index);
   li.index++;
   settings_lazy_lists[k] = (rarch_setting_t*)realloc(sl,
         li.index * sizeof(*sl));
   if (!settings_lazy_lists[k])
      settings_lazy_lists[k] = sl;
   return settings_lazy_lists[k];
}


/* --- Stage A of the layout consolidation -------------------------
 * Every descriptor table, registered once with the guards of its
 * add_desc call site, so a setting is constructible by enum without
 * knowing which build function owns it.  The referee below proves,
 * entry by entry against the hand-sequenced build, that
 * index-driven construction reproduces the same setting - the same
 * interned action tuple pointer, so every callback identical - and
 * counts what the descriptor files do not yet cover. */
typedef struct settings_desc_table
{
   const setting_desc_t *rows;
   uint16_t              count;
} settings_desc_table_t;

static const settings_desc_table_t settings_desc_registry[] = {
   { mm_desc_0, (uint16_t)ARRAY_SIZE(mm_desc_0) },
   { mm_desc_1, (uint16_t)ARRAY_SIZE(mm_desc_1) },
   { mm_desc_2, (uint16_t)ARRAY_SIZE(mm_desc_2) },
   { mm_desc_3, (uint16_t)ARRAY_SIZE(mm_desc_3) },
#ifdef HAVE_CDROM
   { mm_desc_4, (uint16_t)ARRAY_SIZE(mm_desc_4) },
#endif
   { mm_desc_5, (uint16_t)ARRAY_SIZE(mm_desc_5) },
   { mm_desc_6, (uint16_t)ARRAY_SIZE(mm_desc_6) },
#if !defined(IOS) && !defined(HAVE_LAKKA)
   { mm_desc_7, (uint16_t)ARRAY_SIZE(mm_desc_7) },
#endif
   { mm_desc_8, (uint16_t)ARRAY_SIZE(mm_desc_8) },
#if !defined(IOS)
#ifdef HAVE_LAKKA
   { quit_lakka_desc, (uint16_t)ARRAY_SIZE(quit_lakka_desc) },
#endif
#endif
#if !defined(IOS)
#ifndef HAVE_LAKKA
   { mm_desc_9, (uint16_t)ARRAY_SIZE(mm_desc_9) },
#endif
#endif
   { mm_desc_10, (uint16_t)ARRAY_SIZE(mm_desc_10) },
   { mm_desc_11, (uint16_t)ARRAY_SIZE(mm_desc_11) },
   { mm_desc_12, (uint16_t)ARRAY_SIZE(mm_desc_12) },
#ifdef HAVE_BLUETOOTH
   { mm_desc_13, (uint16_t)ARRAY_SIZE(mm_desc_13) },
#endif
#if defined(HAVE_LAKKA) || defined(HAVE_WIFI)
   { mm_desc_14, (uint16_t)ARRAY_SIZE(mm_desc_14) },
#endif
   { mm_desc_15, (uint16_t)ARRAY_SIZE(mm_desc_15) },
   { mm_desc_16, (uint16_t)ARRAY_SIZE(mm_desc_16) },
   { configuration_desc_0, (uint16_t)ARRAY_SIZE(configuration_desc_0) },
   { logging_desc_0, (uint16_t)ARRAY_SIZE(logging_desc_0) },
   { sav_desc_0, (uint16_t)ARRAY_SIZE(sav_desc_0) },
   { saving2_desc_0, (uint16_t)ARRAY_SIZE(saving2_desc_0) },
#ifdef HAVE_CLOUDSYNC
   { cs_desc_0, (uint16_t)ARRAY_SIZE(cs_desc_0) },
#endif
#ifdef HAVE_CLOUDSYNC
   { cs_desc_1, (uint16_t)ARRAY_SIZE(cs_desc_1) },
#endif
#ifdef HAVE_CLOUDSYNC
#ifdef HAVE_S3
   { cs_desc_2, (uint16_t)ARRAY_SIZE(cs_desc_2) },
#endif
#endif
   { frame_time_cou_desc_0, (uint16_t)ARRAY_SIZE(frame_time_cou_desc_0) },
   { rewind_desc_0, (uint16_t)ARRAY_SIZE(rewind_desc_0) },
   { rewind_desc_1, (uint16_t)ARRAY_SIZE(rewind_desc_1) },
#if (!defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)) || (defined(IOS) && TARGET_OS_TV)
   { vid_desc_0, (uint16_t)ARRAY_SIZE(vid_desc_0) },
#endif
   { vid_desc_1, (uint16_t)ARRAY_SIZE(vid_desc_1) },
#if defined(ANDROID) || TARGET_OS_IOS
   { vid_desc_2, (uint16_t)ARRAY_SIZE(vid_desc_2) },
#endif
#ifdef HAVE_VULKAN
   { vid_desc_3, (uint16_t)ARRAY_SIZE(vid_desc_3) },
#endif
#ifdef HAVE_D3D10
   { vid_desc_4, (uint16_t)ARRAY_SIZE(vid_desc_4) },
#endif
#ifdef HAVE_D3D11
   { vid_desc_5, (uint16_t)ARRAY_SIZE(vid_desc_5) },
#endif
#ifdef HAVE_D3D12
   { vid_desc_6, (uint16_t)ARRAY_SIZE(vid_desc_6) },
#endif
#ifdef HAVE_METAL
   { vid_desc_7, (uint16_t)ARRAY_SIZE(vid_desc_7) },
#endif
#ifdef WIIU
   { vid_desc_8, (uint16_t)ARRAY_SIZE(vid_desc_8) },
#endif
   { vid_desc_9, (uint16_t)ARRAY_SIZE(vid_desc_9) },
   { fs_desc, (uint16_t)ARRAY_SIZE(fs_desc) },
#if defined(DINGUX) && defined(DINGUX_BETA)
   { dingux_rr_desc, (uint16_t)ARRAY_SIZE(dingux_rr_desc) },
#endif
   { refresh_desc, (uint16_t)ARRAY_SIZE(refresh_desc) },
   { autoswitch_desc, (uint16_t)ARRAY_SIZE(autoswitch_desc) },
   { vid_desc_10, (uint16_t)ARRAY_SIZE(vid_desc_10) },
   { bias_desc, (uint16_t)ARRAY_SIZE(bias_desc) },
   { aspect_desc, (uint16_t)ARRAY_SIZE(aspect_desc) },
   { vid_desc_11, (uint16_t)ARRAY_SIZE(vid_desc_11) },
#if defined(HAVE_WINDOW_OFFSET)
   { vid_desc_12, (uint16_t)ARRAY_SIZE(vid_desc_12) },
#endif
   { vp_size_desc, (uint16_t)ARRAY_SIZE(vp_size_desc) },
#if defined(DINGUX)
   { dingux_ka_desc, (uint16_t)ARRAY_SIZE(dingux_ka_desc) },
#endif
   { vid_desc_13, (uint16_t)ARRAY_SIZE(vid_desc_13) },
   { winscale_desc, (uint16_t)ARRAY_SIZE(winscale_desc) },
   { vid_desc_14, (uint16_t)ARRAY_SIZE(vid_desc_14) },
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   { vid_desc_15, (uint16_t)ARRAY_SIZE(vid_desc_15) },
#endif
#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) ||   (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH))
   { vid_desc_16, (uint16_t)ARRAY_SIZE(vid_desc_16) },
#endif
#if !((defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) ||   (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH)))
   { vid_desc_17, (uint16_t)ARRAY_SIZE(vid_desc_17) },
#endif
   { video2_desc_0, (uint16_t)ARRAY_SIZE(video2_desc_0) },
#ifdef GEKKO
   { gx_desc, (uint16_t)ARRAY_SIZE(gx_desc) },
#endif
#if defined(DINGUX)
   { dingux_ipu_desc, (uint16_t)ARRAY_SIZE(dingux_ipu_desc) },
#endif
#if defined(DINGUX)
#if defined(RS90) || defined(MIYOO)
   { dingux_rs90_desc, (uint16_t)ARRAY_SIZE(dingux_rs90_desc) },
#endif
#endif
   { smooth_desc, (uint16_t)ARRAY_SIZE(smooth_desc) },
#ifdef HAVE_ODROIDGO2
   { vid_ctx_desc, (uint16_t)ARRAY_SIZE(vid_ctx_desc) },
#endif
   { rot_desc, (uint16_t)ARRAY_SIZE(rot_desc) },
   { vid_desc_18, (uint16_t)ARRAY_SIZE(vid_desc_18) },
   { hdr_desc, (uint16_t)ARRAY_SIZE(hdr_desc) },
   { vid_desc_19, (uint16_t)ARRAY_SIZE(vid_desc_19) },
   { hdr_desc2, (uint16_t)ARRAY_SIZE(hdr_desc2) },
   { vid_desc_20, (uint16_t)ARRAY_SIZE(vid_desc_20) },
   { sync_desc, (uint16_t)ARRAY_SIZE(sync_desc) },
   { avsync_desc, (uint16_t)ARRAY_SIZE(avsync_desc) },
   { fdelay_desc, (uint16_t)ARRAY_SIZE(fdelay_desc) },
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { sdelay_desc, (uint16_t)ARRAY_SIZE(sdelay_desc) },
#endif
   { shader_desc, (uint16_t)ARRAY_SIZE(shader_desc) },
   { vid_desc_21, (uint16_t)ARRAY_SIZE(vid_desc_21) },
   { misc_desc, (uint16_t)ARRAY_SIZE(misc_desc) },
   { video_filter_desc, (uint16_t)ARRAY_SIZE(video_filter_desc) },
   { audio_en_desc, (uint16_t)ARRAY_SIZE(audio_en_desc) },
   { audio_state_desc, (uint16_t)ARRAY_SIZE(audio_state_desc) },
   { audio_sync_desc, (uint16_t)ARRAY_SIZE(audio_sync_desc) },
   { audio_rq_desc, (uint16_t)ARRAY_SIZE(audio_rq_desc) },
   { audio_fmt_desc, (uint16_t)ARRAY_SIZE(audio_fmt_desc) },
   { audio_skew_desc, (uint16_t)ARRAY_SIZE(audio_skew_desc) },
   { audio_dev_desc, (uint16_t)ARRAY_SIZE(audio_dev_desc) },
   { audio_dsp_desc, (uint16_t)ARRAY_SIZE(audio_dsp_desc) },
#ifdef HAVE_WASAPI
   { audio_wasapi_desc, (uint16_t)ARRAY_SIZE(audio_wasapi_desc) },
#endif
#ifdef HAVE_ASIO
   { audio_asio_desc, (uint16_t)ARRAY_SIZE(audio_asio_desc) },
#endif
#ifdef HAVE_MICROPHONE
   { mic_enable_desc, (uint16_t)ARRAY_SIZE(mic_enable_desc) },
#endif
#ifdef HAVE_MICROPHONE
#ifdef RARCH_MOBILE
   { mic_block_desc, (uint16_t)ARRAY_SIZE(mic_block_desc) },
#endif
#endif
#ifdef HAVE_MICROPHONE
   { mic_misc_desc, (uint16_t)ARRAY_SIZE(mic_misc_desc) },
#endif
#ifdef HAVE_MICROPHONE
#ifdef HAVE_WASAPI
   { mic_wasapi_desc, (uint16_t)ARRAY_SIZE(mic_wasapi_desc) },
#endif
#endif
   { inp_desc_0, (uint16_t)ARRAY_SIZE(inp_desc_0) },
#ifdef GEKKO
   { inp_desc_1, (uint16_t)ARRAY_SIZE(inp_desc_1) },
#endif
   { inp_desc_2, (uint16_t)ARRAY_SIZE(inp_desc_2) },
#ifdef UDEV_TOUCH_SUPPORT
   { inp_desc_3, (uint16_t)ARRAY_SIZE(inp_desc_3) },
#endif
#ifdef VITA
   { inp_desc_4, (uint16_t)ARRAY_SIZE(inp_desc_4) },
#endif
#if TARGET_OS_IPHONE
   { inp_desc_5, (uint16_t)ARRAY_SIZE(inp_desc_5) },
#endif
   { inp_desc_6, (uint16_t)ARRAY_SIZE(inp_desc_6) },
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
   { inp_desc_7, (uint16_t)ARRAY_SIZE(inp_desc_7) },
#endif
   { inp_desc_8, (uint16_t)ARRAY_SIZE(inp_desc_8) },
#ifdef ANDROID
   { inp_desc_9, (uint16_t)ARRAY_SIZE(inp_desc_9) },
#endif
   { inp_desc_10, (uint16_t)ARRAY_SIZE(inp_desc_10) },
   { inp_desc_11, (uint16_t)ARRAY_SIZE(inp_desc_11) },
   { inp_desc_12, (uint16_t)ARRAY_SIZE(inp_desc_12) },
   { inp_desc_13, (uint16_t)ARRAY_SIZE(inp_desc_13) },
   { recording_desc_0, (uint16_t)ARRAY_SIZE(recording_desc_0) },
   { recording2_desc_0, (uint16_t)ARRAY_SIZE(recording2_desc_0) },
   { recording_desc_1, (uint16_t)ARRAY_SIZE(recording_desc_1) },
   { recording2_desc_1, (uint16_t)ARRAY_SIZE(recording2_desc_1) },
   { recording_desc_2, (uint16_t)ARRAY_SIZE(recording_desc_2) },
   { recording_desc_3, (uint16_t)ARRAY_SIZE(recording_desc_3) },
   { frame_throttli_desc_0, (uint16_t)ARRAY_SIZE(frame_throttli_desc_0) },
   { menu_thr_desc, (uint16_t)ARRAY_SIZE(menu_thr_desc) },
   { frame_throttli_desc_1, (uint16_t)ARRAY_SIZE(frame_throttli_desc_1) },
#ifdef HAVE_RUNAHEAD
   { frame_throttli_desc_2, (uint16_t)ARRAY_SIZE(frame_throttli_desc_2) },
#endif
#ifdef ANDROID
   { frame_throttli_desc_3, (uint16_t)ARRAY_SIZE(frame_throttli_desc_3) },
#endif
#ifdef HAVE_GFX_WIDGETS
   { osn_desc_0, (uint16_t)ARRAY_SIZE(osn_desc_0) },
#endif
#ifdef HAVE_GFX_WIDGETS
#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   { osn_desc_1, (uint16_t)ARRAY_SIZE(osn_desc_1) },
#endif
#endif
#ifdef HAVE_GFX_WIDGETS
#if !((defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)))
   { widget_fs_desc, (uint16_t)ARRAY_SIZE(widget_fs_desc) },
#endif
#endif
#ifdef HAVE_GFX_WIDGETS
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   { osn_desc_2, (uint16_t)ARRAY_SIZE(osn_desc_2) },
#endif
#endif
   { osn_desc_3, (uint16_t)ARRAY_SIZE(osn_desc_3) },
   { onscreen_not2_desc_0, (uint16_t)ARRAY_SIZE(onscreen_not2_desc_0) },
   { osn_desc_4, (uint16_t)ARRAY_SIZE(osn_desc_4) },
   { osn_desc_5, (uint16_t)ARRAY_SIZE(osn_desc_5) },
#ifdef HAVE_OVERLAY
   { ovl_desc_0, (uint16_t)ARRAY_SIZE(ovl_desc_0) },
#endif
#ifdef HAVE_OVERLAY
   { ovl_desc_1, (uint16_t)ARRAY_SIZE(ovl_desc_1) },
#endif
#ifdef HAVE_OVERLAY
   { overlay2_desc_0, (uint16_t)ARRAY_SIZE(overlay2_desc_0) },
#endif
#ifdef HAVE_OVERLAY
   { ovl_desc_2, (uint16_t)ARRAY_SIZE(ovl_desc_2) },
#endif
#ifdef HAVE_OVERLAY
   { ovl_desc_3, (uint16_t)ARRAY_SIZE(ovl_desc_3) },
#endif
   { menu2_desc_0, (uint16_t)ARRAY_SIZE(menu2_desc_0) },
   { menu_desc_0, (uint16_t)ARRAY_SIZE(menu_desc_0) },
   { menu_desc_1, (uint16_t)ARRAY_SIZE(menu_desc_1) },
   { menu_desc_2, (uint16_t)ARRAY_SIZE(menu_desc_2) },
   { menu_desc_3, (uint16_t)ARRAY_SIZE(menu_desc_3) },
#if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)
   { menu_desc_4, (uint16_t)ARRAY_SIZE(menu_desc_4) },
#endif
   { menu_desc_5, (uint16_t)ARRAY_SIZE(menu_desc_5) },
   { menu_desc_6, (uint16_t)ARRAY_SIZE(menu_desc_6) },
   { menu_desc_7, (uint16_t)ARRAY_SIZE(menu_desc_7) },
#if !defined(DINGUX)
   { menu_desc_8, (uint16_t)ARRAY_SIZE(menu_desc_8) },
#endif
   { menu_desc_9, (uint16_t)ARRAY_SIZE(menu_desc_9) },
   { menu2_desc_1, (uint16_t)ARRAY_SIZE(menu2_desc_1) },
   { menu_desc_10, (uint16_t)ARRAY_SIZE(menu_desc_10) },
   { menu_desc_11, (uint16_t)ARRAY_SIZE(menu_desc_11) },
#if defined(HAVE_XMB) || defined (HAVE_OZONE)
   { menu_desc_12, (uint16_t)ARRAY_SIZE(menu_desc_12) },
#endif
#if defined(HAVE_XMB) || defined (HAVE_OZONE)
   { menu_desc_13, (uint16_t)ARRAY_SIZE(menu_desc_13) },
#endif
   { menu_desc_14, (uint16_t)ARRAY_SIZE(menu_desc_14) },
   { menu_desc_15, (uint16_t)ARRAY_SIZE(menu_desc_15) },
   { menu_desc_16, (uint16_t)ARRAY_SIZE(menu_desc_16) },
   { menu2_desc_2, (uint16_t)ARRAY_SIZE(menu2_desc_2) },
#ifdef HAVE_THREADS
   { menu_desc_17, (uint16_t)ARRAY_SIZE(menu_desc_17) },
#endif
   { menu_desc_18, (uint16_t)ARRAY_SIZE(menu_desc_18) },
#ifdef HAVE_XMB
   { menu_desc_19, (uint16_t)ARRAY_SIZE(menu_desc_19) },
#endif
#ifdef HAVE_XMB
   { menu2_desc_3, (uint16_t)ARRAY_SIZE(menu2_desc_3) },
#endif
#ifdef HAVE_XMB
   { menu_desc_20, (uint16_t)ARRAY_SIZE(menu_desc_20) },
#endif
#ifdef HAVE_XMB
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_SHADERPIPELINE
   { menu_desc_21, (uint16_t)ARRAY_SIZE(menu_desc_21) },
#endif
#endif
#endif
#ifdef HAVE_XMB
   { menu_desc_22, (uint16_t)ARRAY_SIZE(menu_desc_22) },
#endif
   { menu_desc_23, (uint16_t)ARRAY_SIZE(menu_desc_23) },
   { menu_desc_24, (uint16_t)ARRAY_SIZE(menu_desc_24) },
#ifdef HAVE_LAKKA
   { menu_quit_lakka_desc, (uint16_t)ARRAY_SIZE(menu_quit_lakka_desc) },
#endif
#if !defined(HAVE_LAKKA) && !defined(IOS)
   { menu_desc_25, (uint16_t)ARRAY_SIZE(menu_desc_25) },
#endif
#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2)
   { menu_desc_26, (uint16_t)ARRAY_SIZE(menu_desc_26) },
#endif
#if !(defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2))
#if !defined(IOS)
   { menu_desc_27, (uint16_t)ARRAY_SIZE(menu_desc_27) },
#endif
#endif
   { menu_desc_28, (uint16_t)ARRAY_SIZE(menu_desc_28) },
   { menu2_desc_4, (uint16_t)ARRAY_SIZE(menu2_desc_4) },
   { menu_desc_29, (uint16_t)ARRAY_SIZE(menu_desc_29) },
#ifdef HAVE_MATERIALUI
   { menu_desc_30, (uint16_t)ARRAY_SIZE(menu_desc_30) },
#endif
#ifdef HAVE_OZONE
   { menu_desc_31, (uint16_t)ARRAY_SIZE(menu_desc_31) },
#endif
#ifdef HAVE_OZONE
   { menu2_desc_5, (uint16_t)ARRAY_SIZE(menu2_desc_5) },
#endif
#ifdef HAVE_OZONE
   { menu_desc_32, (uint16_t)ARRAY_SIZE(menu_desc_32) },
#endif
   { menu_desc_33, (uint16_t)ARRAY_SIZE(menu_desc_33) },
   { menu_desc_34, (uint16_t)ARRAY_SIZE(menu_desc_34) },
   { menu_desc_35, (uint16_t)ARRAY_SIZE(menu_desc_35) },
   { menu_desc_36, (uint16_t)ARRAY_SIZE(menu_desc_36) },
   { menu_desc_37, (uint16_t)ARRAY_SIZE(menu_desc_37) },
   { menu_desc_38, (uint16_t)ARRAY_SIZE(menu_desc_38) },
   { menu_desc_39, (uint16_t)ARRAY_SIZE(menu_desc_39) },
#ifdef ANDROID
   { power_manageme_desc_0_s0, (uint16_t)ARRAY_SIZE(power_manageme_desc_0_s0) },
#endif
#ifdef HAVE_LAKKA
   { power_manageme_desc_0_s1, (uint16_t)ARRAY_SIZE(power_manageme_desc_0_s1) },
#endif
#ifndef HAVE_LAKKA
   { power_manageme_desc_1, (uint16_t)ARRAY_SIZE(power_manageme_desc_1) },
#endif
#ifdef HAVE_TRANSLATE
   { ai_service_desc_0, (uint16_t)ARRAY_SIZE(ai_service_desc_0) },
#endif
#ifdef HAVE_TRANSLATE
   { ai_service_desc_1, (uint16_t)ARRAY_SIZE(ai_service_desc_1) },
#endif
   { ui_desc_0, (uint16_t)ARRAY_SIZE(ui_desc_0) },
   /* ui_desc_1 (3DS display mode) is deliberately absent: its range
    * is computed from the console model at runtime, so the table is
    * function-local and cannot be registered. It stays a documented
    * runtime-descriptor exception until the row grammar can express
    * computed ranges. */
#ifdef _3DS
   { ui_desc_2, (uint16_t)ARRAY_SIZE(ui_desc_2) },
#endif
#ifdef _3DS
   { ui_desc_3, (uint16_t)ARRAY_SIZE(ui_desc_3) },
#endif
#ifdef HAVE_NETWORKING
   { ui_desc_4, (uint16_t)ARRAY_SIZE(ui_desc_4) },
#endif
#ifdef HAVE_NETWORKING
#if !defined(HAVE_LAKKA)
   { ui_desc_5, (uint16_t)ARRAY_SIZE(ui_desc_5) },
#endif
#endif
#ifdef HAVE_MIST
   { ui_desc_6, (uint16_t)ARRAY_SIZE(ui_desc_6) },
#endif
   { ui_desc_7, (uint16_t)ARRAY_SIZE(ui_desc_7) },
#ifdef HAVE_MIST
   { ui_desc_8, (uint16_t)ARRAY_SIZE(ui_desc_8) },
#endif
#ifdef HAVE_SMBCLIENT
   { ui_desc_9, (uint16_t)ARRAY_SIZE(ui_desc_9) },
#endif
   { ui_desc_10, (uint16_t)ARRAY_SIZE(ui_desc_10) },
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { ui_desc_11, (uint16_t)ARRAY_SIZE(ui_desc_11) },
#endif
   { ui_desc_12, (uint16_t)ARRAY_SIZE(ui_desc_12) },
   { ui_desc_13, (uint16_t)ARRAY_SIZE(ui_desc_13) },
   { pl_desc_0, (uint16_t)ARRAY_SIZE(pl_desc_0) },
   { pl_desc_1, (uint16_t)ARRAY_SIZE(pl_desc_1) },
   { pl_desc_2, (uint16_t)ARRAY_SIZE(pl_desc_2) },
   { pl_desc_3, (uint16_t)ARRAY_SIZE(pl_desc_3) },
#if defined(HAVE_OZONE) || defined(HAVE_XMB)
   { pl_desc_4, (uint16_t)ARRAY_SIZE(pl_desc_4) },
#endif
#ifdef HAVE_CHEEVOS
   { cheevos_desc_0, (uint16_t)ARRAY_SIZE(cheevos_desc_0) },
#endif
#ifdef HAVE_CHEEVOS
   { chv_desc_0, (uint16_t)ARRAY_SIZE(chv_desc_0) },
#endif
#ifdef HAVE_CHEEVOS
   { chv_desc_1, (uint16_t)ARRAY_SIZE(chv_desc_1) },
#endif
#ifdef HAVE_NETWORKING
   { core_updater_desc_0, (uint16_t)ARRAY_SIZE(core_updater_desc_0) },
#endif
#ifdef HAVE_NETWORKING
#ifdef HAVE_UPDATE_CORES
   { core_updater_desc_1, (uint16_t)ARRAY_SIZE(core_updater_desc_1) },
#endif
#endif
#ifdef HAVE_NETWORKING
#ifdef HAVE_UPDATE_CORES
   { core_updater_desc_2, (uint16_t)ARRAY_SIZE(core_updater_desc_2) },
#endif
#endif
#ifdef HAVE_SMBCLIENT
   { np_desc_0, (uint16_t)ARRAY_SIZE(np_desc_0) },
#endif
#if defined(HAVE_NETWORKING)
   { np_desc_1, (uint16_t)ARRAY_SIZE(np_desc_1) },
#endif
#if defined(HAVE_NETWORKING)
   { netplay2_desc_0, (uint16_t)ARRAY_SIZE(netplay2_desc_0) },
#endif
#if defined(HAVE_NETWORKING)
   { np_desc_2, (uint16_t)ARRAY_SIZE(np_desc_2) },
#endif
#if defined(HAVE_NETWORKING)
   { netplay2_desc_1, (uint16_t)ARRAY_SIZE(netplay2_desc_1) },
#endif
#if defined(HAVE_NETWORKING)
   { np_desc_3, (uint16_t)ARRAY_SIZE(np_desc_3) },
#endif
#if defined(HAVE_NETWORKING)
   { np_desc_4, (uint16_t)ARRAY_SIZE(np_desc_4) },
#endif
#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
   { np_desc_5, (uint16_t)ARRAY_SIZE(np_desc_5) },
#endif
#endif
#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
   { np_desc_6, (uint16_t)ARRAY_SIZE(np_desc_6) },
#endif
#endif
#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
   { np_desc_7, (uint16_t)ARRAY_SIZE(np_desc_7) },
#endif
#endif
#if defined(HAVE_NETWORKING)
   { np_desc_8, (uint16_t)ARRAY_SIZE(np_desc_8) },
#endif
   { user_desc_0, (uint16_t)ARRAY_SIZE(user_desc_0) },
   { user2_desc_0, (uint16_t)ARRAY_SIZE(user2_desc_0) },
#ifdef HAVE_GAME_AI
   { user_desc_1, (uint16_t)ARRAY_SIZE(user_desc_1) },
#endif
#ifdef HAVE_CHEEVOS
   { user_accounts_desc_0_s0, (uint16_t)ARRAY_SIZE(user_accounts_desc_0_s0) },
#endif
#ifdef HAVE_NETWORKING
#if !IOS
   { user_accounts_desc_0_s1, (uint16_t)ARRAY_SIZE(user_accounts_desc_0_s1) },
#endif
#endif
   { dir_desc_0, (uint16_t)ARRAY_SIZE(dir_desc_0) },
   { dir_desc_1, (uint16_t)ARRAY_SIZE(dir_desc_1) },
   { dir_desc_2, (uint16_t)ARRAY_SIZE(dir_desc_2) },
   { privacy_desc_0, (uint16_t)ARRAY_SIZE(privacy_desc_0) },
#ifdef HAVE_DISCORD
   { privacy_desc_1, (uint16_t)ARRAY_SIZE(privacy_desc_1) },
#endif
   { privacy_desc_2, (uint16_t)ARRAY_SIZE(privacy_desc_2) },
#if !defined(RARCH_CONSOLE)
   { midi_desc_0, (uint16_t)ARRAY_SIZE(midi_desc_0) },
#endif
#ifdef HAVE_MIST
   { steam_desc_0, (uint16_t)ARRAY_SIZE(steam_desc_0) },
#endif
#ifdef HAVE_SMBCLIENT
   { smbclient_desc_0, (uint16_t)ARRAY_SIZE(smbclient_desc_0) },
#endif
#ifdef HAVE_SMBCLIENT
   { smbclient_desc_1, (uint16_t)ARRAY_SIZE(smbclient_desc_1) },
#endif
};

static const setting_desc_t *settings_master_find(enum msg_hash_enums e)
{
   unsigned t, r;
   for (t = 0; t < ARRAY_SIZE(settings_desc_registry); t++)
   {
      const settings_desc_table_t *tab = &settings_desc_registry[t];
      for (r = 0; r < tab->count; r++)
         if (tab->rows[r].name_enum == e)
            return &tab->rows[r];
   }
   return NULL;
}

static rarch_setting_t *menu_setting_new_internal(rarch_setting_info_t *list_info)
{
   unsigned i;
   rarch_setting_t* resized_list        = NULL;
      settings_t *settings                 = config_get_ptr();
   global_t   *global                   = global_get_ptr();
   const char *root                     = NULL;
   rarch_setting_t **list_ptr           = NULL;
   rarch_setting_t *list                = (rarch_setting_t*)
      malloc(list_info->size * sizeof(*list));

   if (!list)
      return NULL;

   root                                 = MENU_ENUM_LABEL_MAIN_MENU_STR;

   for (i = 0; i < (unsigned)list_info->size; i++)
   {
      MENU_SETTING_INITIALIZE(list, i);
   }

   for (i = 0; i < ARRAY_SIZE(settings_list_build_order); i++)
   {
      if (!setting_append_list(
               settings, global,
               settings_list_build_order[i], &list, list_info, root))
      {
         free(list);
         return NULL;
      }
      settings_lazy_bounds[i] = (uint16_t)list_info->index;
   }
   settings_lazy_nbuilders = (unsigned)ARRAY_SIZE(settings_list_build_order);

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
#if defined(RETROARCH_VALIDATION_DUMPS)
/* Validation harness for settings-list refactoring.
 *
 * When the RETROARCH_SETTINGS_DUMP environment variable is set to a
 * file path, the fully constructed settings list is serialised there
 * in a canonical text form at menu init.  Dumps taken before and
 * after a registration refactor (e.g. converting imperative
 * CONFIG_* runs to setting_desc_t tables) must be byte-identical.
 *
 * Handler pointers cannot be compared across binaries, so the
 * handlers most affected by conversions are reported as symbolic
 * tags, and get_string_representation is exercised directly for
 * value types so that a wrongly wired representation callback shows
 * up as a differing string rather than an invisible pointer.
 *
 * Zero cost unless the environment variable is set. */
static void menu_setting_validation_dump(rarch_setting_t *list)
{
   unsigned i;
   RFILE *f;
   const char *path = getenv("RETROARCH_SETTINGS_DUMP");

   if (!path || !list)
      return;

   f = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
      return;

   for (i = 0; list[i].type != ST_NONE; i++)
   {
      char repr[512];
      rarch_setting_t *s   = &list[i];
      const char *ok_tag   = "-";
      const char *left_tag = "-";
      const char *rght_tag = "-";
      const char *strt_tag = "0";
      const char *sel_tag  = "0";
      char dflt[64];

      if (s->actions->ok == setting_bool_action_left_with_refresh)
         ok_tag   = "boolrefL";
      else if (s->actions->ok == setting_action_ok_uint)
         ok_tag   = "okuint";
      if (s->actions->left == setting_bool_action_left_with_refresh)
         left_tag = "boolrefL";
      else if (s->actions->left == setting_uint_action_left_with_refresh)
         left_tag = "uintrefL";
      else if (s->actions->left == setting_uint_action_left_default)
         left_tag = "Du";
      else if (s->actions->left == setting_int_action_left_default)
         left_tag = "Di";
      else if (s->actions->left == setting_bool_action_ok_default)
         left_tag = "Db";
      else if (s->actions->left == setting_fraction_action_left_default)
         left_tag = "Df";
      else if (s->actions->left)
         left_tag = "C";
      else
         left_tag = "0";
      if (s->actions->right == setting_bool_action_right_with_refresh)
         rght_tag = "boolrefR";
      else if (s->actions->right == setting_uint_action_right_with_refresh)
         rght_tag = "uintrefR";
      else if (s->actions->right == setting_uint_action_right_default)
         rght_tag = "Du";
      else if (s->actions->right == setting_int_action_right_default)
         rght_tag = "Di";
      else if (s->actions->right == setting_bool_action_ok_default)
         rght_tag = "Db";
      else if (s->actions->right == setting_fraction_action_right_default)
         rght_tag = "Df";
      else if (s->actions->right)
         rght_tag = "C";
      else
         rght_tag = "0";
      if (s->actions->start == setting_generic_action_start_default)
         strt_tag = "D";
      else if (s->actions->start)
         strt_tag = "C";
      if (s->actions->sel == setting_generic_action_ok_default)
         sel_tag  = "D";
      else if (s->actions->sel)
         sel_tag  = "C";

      dflt[0] = '\0';
      repr[0] = '\0';

      switch (s->type)
      {
         case ST_BOOL:
            snprintf(dflt, sizeof(dflt), "%d",
                  s->default_value.boolean ? 1 : 0);
            break;
         case ST_INT:
            snprintf(dflt, sizeof(dflt), "%d",
                  s->default_value.integer);
            break;
         case ST_UINT:
            snprintf(dflt, sizeof(dflt), "%u",
                  s->default_value.unsigned_integer);
            break;
         case ST_SIZE:
            snprintf(dflt, sizeof(dflt), "%u",
                  (unsigned)s->default_value.sizet);
            break;
         case ST_FLOAT:
            snprintf(dflt, sizeof(dflt), "%.6g",
                  s->default_value.fraction);
            break;
         case ST_PATH:
         case ST_DIR:
         case ST_STRING:
         case ST_STRING_OPTIONS:
            snprintf(dflt, sizeof(dflt), "%s",
                  s->default_value.string ? s->default_value.string : "");
            break;
         default:
            break;
      }

      switch (s->type)
      {
         case ST_BOOL:
         case ST_INT:
         case ST_UINT:
         case ST_SIZE:
         case ST_FLOAT:
            if (s->actions->repr)
               s->actions->repr(s, repr, sizeof(repr));
            break;
         default:
            break;
      }

      filestream_printf(f,
            "%u|%s|%d|%d|%d|%08x|%02x|%.6g|%.6g|%.6g|%d|%d|%d|%d|%u|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
            i,
            s->name ? s->name : "",
            (int)s->type,
            (int)s->enum_idx,
            (int)s->enum_value_idx,
            (unsigned)s->flags,
            (unsigned)s->free_flags,
            (double)s->min,
            (double)s->max,
            (double)s->step,
            (int)s->offset_by,
            (int)s->ui_type,
            (int)s->browser_selection_type,
            (int)s->cmd_trigger_idx,
            (unsigned)s->size,
            dflt,
            (s->type == ST_FLOAT && s->aux.rounding_fraction)
                  ? s->aux.rounding_fraction : "",
            ok_tag, left_tag, rght_tag, strt_tag, sel_tag,
            s->short_description ? s->short_description : "",
            s->values ? s->values : "",
            repr);
   }

   filestream_close(f);
}
#endif

rarch_setting_t *menu_setting_new(void)
{
   rarch_setting_t *list           = NULL;
   rarch_setting_t *token          = NULL;
   rarch_setting_info_t *list_info = (rarch_setting_info_t*)
      malloc(sizeof(*list_info));

   if (!list_info)
      return NULL;

   list_info->index = 0;
   list_info->size  = 32;

   list             = menu_setting_new_internal(list_info);

   free(list_info);

   if (!list)
      return NULL;

   /* Learn which builder owns each enum and name, then let the full
    * list go; builders rebuild on demand into the session cache.
    * The dumps run while the full list is still alive - the
    * displaylist dump resolves its settings through the cache and so
    * validates the lazy path against the full build every run. */
   settings_lazy_learn(list);

#if defined(RETROARCH_VALIDATION_DUMPS)
   /* Descriptor-index referee: for every enum-bearing entry the
    * hand-sequenced build produced, construct the same setting from
    * the master index alone and compare. Pointer equality on the
    * interned actions tuple covers all eight callbacks at once. */
   if (getenv("RETROARCH_DESC_REFEREE"))
   {
      settings_t *settings = config_get_ptr();
      unsigned i2, n_checked = 0, n_nodesc = 0, n_ident = 0;
      unsigned n_nodesc_nonbind = 0;
      /* LV twins share one enum; only the first entry per enum is
       * reachable through find_enum, so only it is compared. */
      uint8_t *seen = (uint8_t*)calloc(MSG_LAST, 1);
      unsigned mm_act = 0, mm_name = 0, mm_tgt = 0, mm_def = 0;
      unsigned mm_rng = 0, mm_flag = 0, mm_other = 0;
      for (i2 = 0; list[i2].type != ST_NONE; i2++)
      {
         rarch_setting_t *e = &list[i2];
         const setting_desc_t *d;
         rarch_setting_t *sc = NULL;
         rarch_setting_info_t li;
         rarch_setting_group_info_t gi, sgi;
         rarch_setting_t *one;
         bool diff = false;
         if (e->type > ST_GROUP || e->type == ST_GROUP)
            continue;
         if (e->enum_idx == MSG_UNKNOWN)
            continue;
         if (seen && seen[e->enum_idx])
            continue;
         if (seen)
            seen[e->enum_idx] = 1;
         n_checked++;
         if (!(d = settings_master_find(e->enum_idx)))
         {
            n_nodesc++;
            if (e->type != ST_BIND && n_nodesc_nonbind++ < 200)
               fprintf(stderr, "REFEREE nodesc(%u): %s\n",
                     (unsigned)e->type, e->name);
            continue;
         }
         li.index = 0;
         li.size  = 8;
         if (!(sc = (rarch_setting_t*)calloc(li.size, sizeof(*sc))))
            continue;
         {
            unsigned j2;
            for (j2 = 0; j2 < 8; j2++)
            {
               MENU_SETTING_INITIALIZE((&sc)[0], j2);
            }
         }
         gi.name  = "Referee";
         sgi.name = "Referee";
         settings_list_add_desc(&sc, &li, settings, d, 1,
               &gi, &sgi, MENU_ENUM_LABEL_MAIN_MENU_STR);
         if (li.index < 1)
         {
            free(sc);
            n_nodesc++;
            continue;
         }
         one = &sc[0];
         if (one->actions != e->actions)
         {
            mm_act++; diff = true;
            if (mm_act <= 20)
               fprintf(stderr, "REFEREE actions: %s\n", e->name);
         }
         if (!string_is_equal(one->name, e->name))
         {  mm_name++; diff = true; }
         if (one->value.target.string != e->value.target.string)
         {
            mm_tgt++; diff = true;
            if (mm_tgt <= 3)
               fprintf(stderr, "REFEREE target: %s\n", e->name);
         }
         if (memcmp(&one->default_value, &e->default_value,
               sizeof(one->default_value)))
         {
            mm_def++; diff = true;
            if (mm_def <= 3)
               fprintf(stderr, "REFEREE default: %s\n", e->name);
         }
         if (one->min != e->min || one->max != e->max
               || one->step != e->step)
         {
            mm_rng++; diff = true;
            if (mm_rng <= 20)
               fprintf(stderr, "REFEREE range: %s\n", e->name);
         }
         if ((one->flags | SD_FLAG_ADVANCED)
               != (e->flags | SD_FLAG_ADVANCED))
         {
            mm_flag++; diff = true;
            if (mm_flag <= 20)
               fprintf(stderr, "REFEREE flags %08x vs %08x: %s\n",
                     one->flags, e->flags, e->name);
         }
         if (one->cmd_trigger_idx != e->cmd_trigger_idx
               || one->browser_selection_type != e->browser_selection_type
               || one->type != e->type)
         {
            mm_other++; diff = true;
            if (mm_other <= 20)
               fprintf(stderr, "REFEREE other: %s\n", e->name);
         }
         if (!diff)
            n_ident++;
         menu_setting_free(sc);
         free(sc);
      }
      if (seen)
         free(seen);
      fprintf(stderr,
            "DESC_REFEREE checked=%u identical=%u no-desc=%u nonbind-nodesc=%u "
            "act=%u name=%u tgt=%u def=%u rng=%u flag=%u other=%u\n",
            n_checked, n_ident, n_nodesc, n_nodesc_nonbind,
            mm_act, mm_name, mm_tgt, mm_def, mm_rng, mm_flag, mm_other);
      /* Per-builder verdict: a build function may only collapse into
       * the registry walker when every entry it produces is
       * index-identical; print the green list. */
      {
         unsigned k2, b2 = 0;
         for (k2 = 0; k2 < settings_lazy_nbuilders; k2++)
         {
            unsigned lo2 = b2;
            unsigned hi2 = settings_lazy_bounds[k2];
            unsigned bad2 = 0, tot2 = 0, nd2 = 0;
            for (i2 = lo2; i2 < hi2 && list[i2].type != ST_NONE; i2++)
            {
               rarch_setting_t *e2 = &list[i2];
               const setting_desc_t *d2;
               if (e2->type >= ST_GROUP || e2->enum_idx == MSG_UNKNOWN)
                  continue;
               tot2++;
               if (!(d2 = settings_master_find(e2->enum_idx)))
               {
                  nd2++;
                  continue;
               }
            }
            fprintf(stderr, "BUILDER %u span=%u..%u settings=%u nodesc=%u\n",
                  k2, lo2, hi2, tot2, nd2);
            b2 = hi2;
         }
      }
   }
   menu_setting_validation_dump(list);
   menu_displaylist_validation_dump(list);
#endif

   menu_setting_free(list);
   free(list);

   /* The caller stores and later frees one list pointer; hand it a
    * terminator-only token whose free tears the cache down. */
   if (!(token = (rarch_setting_t*)calloc(1, sizeof(*token))))
   {
      settings_lazy_free();
      return NULL;
   }
   token->type          = ST_NONE;
   token->actions       = &settings_actions_none;
   settings_lazy_token  = token;

   return token;
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
   /* Descriptor holdout: value target outside settings_t. */
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
