/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include "../config.def.h"
#include "../config.def.keybinds.h"

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif


#include "../frontend/frontend_driver.h"

#include "widgets/menu_input_bind_dialog.h"

#include "menu_setting.h"
#include "menu_cbs.h"
#include "menu_driver.h"
#include "menu_animation.h"
#include "menu_input.h"

#include "../core.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../defaults.h"
#include "../driver.h"
#include "../dirs.h"
#include "../paths.h"
#include "../dynamic.h"
#include "../list_special.h"
#include "../verbosity.h"
#include "../camera/camera_driver.h"
#include "../wifi/wifi_driver.h"
#include "../location/location_driver.h"
#include "../record/record_driver.h"
#include "../audio/audio_driver.h"
#include "../input/input_driver.h"
#include "../midi/midi_driver.h"
#include "../tasks/tasks_internal.h"
#include "../config.def.h"
#include "../ui/ui_companion_driver.h"
#include "../performance_counters.h"
#include "../setting_list.h"
#include "../lakka.h"
#include "../retroarch.h"
#include "../gfx/video_display_server.h"
#include "../managers/cheat_manager.h"

#include "../tasks/tasks_internal.h"

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

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
   SETTINGS_LIST_INPUT_HOTKEY,
   SETTINGS_LIST_RECORDING,
   SETTINGS_LIST_FRAME_THROTTLING,
   SETTINGS_LIST_FONT,
   SETTINGS_LIST_OVERLAY,
   SETTINGS_LIST_MENU,
   SETTINGS_LIST_MENU_FILE_BROWSER,
   SETTINGS_LIST_MULTIMEDIA,
   SETTINGS_LIST_USER_INTERFACE,
   SETTINGS_LIST_POWER_MANAGEMENT,
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
   SETTINGS_LIST_DIRECTORY,
   SETTINGS_LIST_PRIVACY,
   SETTINGS_LIST_MIDI
};

struct bool_entry
{
   bool default_value;
   bool *target;
   uint32_t flags;
   enum msg_hash_enums name_enum_idx;
   enum msg_hash_enums SHORT_enum_idx;
   enum msg_hash_enums off_enum_idx;
   enum msg_hash_enums on_enum_idx;
};

struct string_options_entry
{
   enum msg_hash_enums name_enum_idx;
   enum msg_hash_enums SHORT_enum_idx;
   const char *default_value;
   const char *values;
   char *target;
   size_t len;
};

static int setting_action_ok_bind_all(rarch_setting_t *setting, bool wraparound)
{
   (void)wraparound;
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_ALL, setting))
      return -1;
   return 0;
}

static int setting_action_ok_bind_all_save_autoconfig(rarch_setting_t *setting,
      bool wraparound)
{
   unsigned index_offset     = 0;
   const char *name          = NULL;

   (void)wraparound;

   if (!setting)
      return -1;

   index_offset = setting->index_offset;
   name         = input_config_get_device_name(index_offset);

   if(!string_is_empty(name) && config_save_autoconf_profile(name, index_offset))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY), 1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_ERROR_SAVING), 1, 100, true);


   return 0;
}

static int setting_action_ok_bind_defaults(rarch_setting_t *setting, bool wraparound)
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
      rarch_setting_t *setting, bool wraparound)
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

   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   return 0;
}

static int setting_action_ok_video_refresh_rate_polled(rarch_setting_t *setting,
      bool wraparound)
{
   float refresh_rate = 0.0;

   if (!setting)
     return -1;

   if ((refresh_rate = video_driver_get_refresh_rate()) == 0.0)
      return -1;

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
   /* Incase refresh rate update forced non-block video. */
   command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);

   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   return 0;
}


static int setting_action_ok_uint(rarch_setting_t *setting, bool wraparound)
{
   char enum_idx[16];
   if (!setting)
      return -1;

   snprintf(enum_idx, sizeof(enum_idx), "%d", setting->enum_idx);

   generic_action_ok_displaylist_push(
         enum_idx, /* we will pass the enumeration index of the string as a path */
         NULL, NULL, 0, 0, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST);
   return 0;
}

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
         strlcpy(s, "Twitch", len);
         break;
      case STREAMING_MODE_YOUTUBE:
         strlcpy(s, "YouTube", len);
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
      case 5:
         strlcpy(s, "Custom", len);
         break;
      case 6:
         strlcpy(s, "Low", len);
         break;
      case 7:
         strlcpy(s, "Medium", len);
         break;
      case 8:
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
      case 0:
         strlcpy(s, "Custom", len);
         break;
      case 1:
         strlcpy(s, "Low", len);
         break;
      case 2:
         strlcpy(s, "Medium", len);
         break;
      case 3:
         strlcpy(s, "High", len);
         break;
      case 4:
         strlcpy(s, "Lossless", len);
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
      strlcpy(s, "********", len);
   else
      *setting->value.target.string = '\0';
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
         strlcpy(s, "iPega PG-9017", len);
         break;
      case 2:
         strlcpy(s, "8-bitty", len);
         break;
      case 3:
         strlcpy(s, "SNES30 8bitdo", len);
         break;
   }
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

static void setting_get_string_representation_uint_menu_timedate_style(
   rarch_setting_t *setting,
   char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
   case 0:
      strlcpy(s, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS), len);
      break;
   case 1:
      strlcpy(s, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM), len);
      break;
   case 2:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY), len);
      break;
   case 3:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS), len);
      break;
   case 4:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM), len);
      break;
   case 5:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM), len);
      break;
   case 6:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM), len);
      break;
   case 7:
      strlcpy(s,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_AM_PM), len);
      break;
   }
}

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
         strlcpy(s, "Auto", len);
         break;
      case 1:
         strlcpy(s, "Console", len);
         break;
      case 2:
         strlcpy(s, "Handheld", len);
         break;
   }
}

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
      default:
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
   }
}

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
   if (!setting)
      return;

   av_info = video_viewport_get_system_av_info();
   geom    = (struct retro_game_geometry*)&av_info->geometry;

   if (*setting->value.target.unsigned_integer%geom->base_width == 0)
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_width);
   else
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint_custom_viewport_height(rarch_setting_t *setting,
      char *s, size_t len)
{
   struct retro_game_geometry  *geom    = NULL;
   struct retro_system_av_info *av_info = NULL;
   if (!setting)
      return;

   av_info = video_viewport_get_system_av_info();
   geom    = (struct retro_game_geometry*)&av_info->geometry;

   if (*setting->value.target.unsigned_integer%geom->base_height == 0)
      snprintf(s, len, "%u (%ux)",
            *setting->value.target.unsigned_integer,
            *setting->value.target.unsigned_integer / geom->base_height);
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
      strlcpy(s, msg_hash_to_str(MSG_NATIVE), len);
   else
      snprintf(s, len, "%d", *setting->value.target.unsigned_integer);
}

static int setting_action_left_analog_dpad_mode(rarch_setting_t *setting, bool wraparound)
{
   unsigned port = 0;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   configuration_set_uint(settings, settings->uints.input_analog_dpad_mode[port],
      (settings->uints.input_analog_dpad_mode
       [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST);

   return 0;
}

static unsigned libretro_device_get_size(unsigned *devices, size_t devices_size, unsigned port)
{
   unsigned types                           = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_system_info_t              *system = runloop_get_system_info();

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
      rarch_setting_t *setting, bool wraparound)
{
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

   return 0;
}


static int setting_uint_action_left_crt_switch_resolution_super(
      rarch_setting_t *setting, bool wraparound)
{
   if (!setting)
      return -1;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
         *setting->value.target.unsigned_integer = 3840;
         break;
      case 1920:
         *setting->value.target.unsigned_integer = 0;
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

static int setting_action_left_bind_device(rarch_setting_t *setting, bool wraparound)
{
   unsigned               *p = NULL;
   unsigned index_offset     = 0;
   unsigned max_devices      = input_config_get_device_count();
   settings_t      *settings = config_get_ptr();

   if (!setting || max_devices == 0)
      return -1;

   index_offset = setting->index_offset;

   p = &settings->uints.input_joypad_map[index_offset];

   if ((*p) >= max_devices)
      *p = max_devices - 1;
   else if ((*p) > 0)
      (*p)--;

   return 0;
}


static int setting_action_left_mouse_index(rarch_setting_t *setting, bool wraparound)
{
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   if (settings->uints.input_mouse_index[setting->index_offset])
   {
      --settings->uints.input_mouse_index[setting->index_offset];
      settings->modified = true;
   }

   return 0;
}

static int setting_uint_action_left_custom_viewport_width(
      rarch_setting_t *setting, bool wraparound)
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

   if (custom->width <= 1)
      custom->width = 1;
   else if (settings->bools.video_scale_integer)
   {
      if (custom->width > geom->base_width)
         custom->width -= geom->base_width;
   }
   else
      custom->width -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_uint_action_left_custom_viewport_height(
      rarch_setting_t *setting, bool wraparound)
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

   if (custom->height <= 1)
      custom->height = 1;
   else if (settings->bools.video_scale_integer)
   {
      if (custom->height > geom->base_height)
         custom->height -= geom->base_height;
   }
   else
      custom->height -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_string_action_left_audio_device(
      rarch_setting_t *setting, bool wraparound)
{
#if !defined(RARCH_CONSOLE)
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
#endif

   return 0;
}

static int setting_string_action_left_driver(rarch_setting_t *setting,
      bool wraparound)
{
   driver_ctx_info_t drv;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   if (!driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv))
   {
      settings_t *settings = config_get_ptr();

      if (settings && settings->bools.menu_navigation_wraparound_enable)
      {
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         driver_ctl(RARCH_DRIVER_CTL_FIND_LAST, &drv);
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}


#ifdef HAVE_NETWORKING
static int setting_string_action_left_netplay_mitm_server(
      rarch_setting_t *setting, bool wraparound)
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
         if (i - 1 >= 0)
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

   strlcpy(setting->value.target.string,
         netplay_mitm_server_list[offset].name, setting->size);

   return 0;
}

static int setting_string_action_right_netplay_mitm_server(
      rarch_setting_t *setting, bool wraparound)
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
      rarch_setting_t *setting, bool wraparound)
{
   if (!setting)
      return -1;

   switch (*setting->value.target.unsigned_integer)
   {
      case 0:
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
      rarch_setting_t *setting, bool wraparound)
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
      custom->width += geom->base_width;
   else
      custom->width += 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_uint_action_right_custom_viewport_height(
      rarch_setting_t *setting, bool wraparound)
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
      custom->height += geom->base_height;
   else
      custom->height += 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_right_audio_device(
      rarch_setting_t *setting, bool wraparound)
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

static int setting_string_action_right_driver(rarch_setting_t *setting,
      bool wraparound)
{
   driver_ctx_info_t drv;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   if (!driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv))
   {
      settings_t *settings = config_get_ptr();

      if (settings && settings->bools.menu_navigation_wraparound_enable)
      {
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         driver_ctl(RARCH_DRIVER_CTL_FIND_FIRST, &drv);
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int setting_string_action_left_midi_input(rarch_setting_t *setting, bool wraparound)
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

static int setting_string_action_right_midi_input(rarch_setting_t *setting, bool wraparound)
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

static int setting_string_action_left_midi_output(rarch_setting_t *setting, bool wraparound)
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

static int setting_string_action_right_midi_output(rarch_setting_t *setting, bool wraparound)
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

static void setting_get_string_representation_uint_cheat_browse_address(rarch_setting_t *setting,
      char *s, size_t len)
{
   unsigned int address      = cheat_manager_state.browse_address;
   unsigned int address_mask = 0;
   unsigned int prev_val     = 0;
   unsigned int curr_val     = 0 ;

   if (setting)
      snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL),
            *setting->value.target.unsigned_integer, *setting->value.target.unsigned_integer);

   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_BROWSE, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val) ;

   snprintf(s, len, "Prev: %u Curr: %u", prev_val, curr_val) ;

}

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
         strlcpy(s, "31 KHz", len);
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
         strlcpy(s, "Lowest",
               len);
         break;
      case RESAMPLER_QUALITY_LOWER:
         strlcpy(s, "Lower",
               len);
         break;
      case RESAMPLER_QUALITY_HIGHER:
         strlcpy(s, "Higher",
               len);
         break;
      case RESAMPLER_QUALITY_HIGHEST:
         strlcpy(s, "Highest",
               len);
         break;
      case RESAMPLER_QUALITY_NORMAL:
         strlcpy(s, "Normal",
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
   rarch_system_info_t *system = runloop_get_system_info();

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
   const char *modes[3];

   modes[0] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
   modes[1] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG);
   modes[2] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG);

   strlcpy(s, modes[*setting->value.target.unsigned_integer % ANALOG_DPAD_LAST], len);
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
#endif

static void setting_get_string_representation_toggle_gamepad_combo(
      rarch_setting_t *setting,
      char *s, size_t len)
{
   if (!setting)
      return;

   switch (*setting->value.target.unsigned_integer)
   {
      case INPUT_TOGGLE_NONE:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), len);
         break;
      case INPUT_TOGGLE_DOWN_Y_L_R:
         strlcpy(s, "Down + L1 + R1 + Y", len);
         break;
      case INPUT_TOGGLE_L3_R3:
         strlcpy(s, "L3 + R3", len);
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         strlcpy(s, "L1 + R1 + Start + Select", len);
         break;
      case INPUT_TOGGLE_START_SELECT:
         strlcpy(s, "Start + Select", len);
         break;
      case INPUT_TOGGLE_L3_R:
         strlcpy(s, "L3 + R", len);
         break;
      case INPUT_TOGGLE_L_R:
         strlcpy(s, "L + R", len);
         break;
   }
}

#ifdef HAVE_NETWORKING
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

enum setting_type menu_setting_get_browser_selection_type(rarch_setting_t *setting)
{
   if (!setting)
      return ST_NONE;
   return setting->browser_selection_type;
}

static void menu_settings_info_list_free(rarch_setting_info_t *list_info)
{
   if (list_info)
      free(list_info);
}

void menu_settings_list_current_add_range(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float min, float max, float step,
      bool enforce_minrange_enable, bool enforce_maxrange_enable)
{
   unsigned idx                   = list_info->index - 1;

   (*list)[idx].min               = min;
   (*list)[idx].step              = step;
   (*list)[idx].max               = max;
   (*list)[idx].enforce_minrange  = enforce_minrange_enable;
   (*list)[idx].enforce_maxrange  = enforce_maxrange_enable;

   (*list)[list_info->index - 1].flags |= SD_FLAG_HAS_RANGE;
}

static void menu_settings_list_current_add_values(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *values)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].values = values;
}

void menu_settings_list_current_add_cmd(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum event_command values)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].cmd_trigger.idx = values;
}

void menu_settings_list_current_add_enum_idx(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums enum_idx)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].enum_idx = enum_idx;
}

void menu_settings_list_current_add_enum_value_idx(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums enum_idx)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].enum_value_idx = enum_idx;
}


int menu_setting_generic(rarch_setting_t *setting, bool wraparound)
{
   uint64_t flags = setting->flags;
   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   if (setting->change_handler)
      setting->change_handler(setting);

   if ((flags & SD_FLAG_EXIT) && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

static int setting_handler(rarch_setting_t *setting, unsigned action)
{
   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (setting->action_up)
            return setting->action_up(setting);
         break;
      case MENU_ACTION_DOWN:
         if (setting->action_down)
            return setting->action_down(setting);
         break;
      case MENU_ACTION_LEFT:
         if (setting->action_left)
         {
            int ret = setting->action_left(setting, false);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH, NULL);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE, NULL);
            return ret;
         }
         break;
      case MENU_ACTION_RIGHT:
         if (setting->action_right)
         {
            int ret = setting->action_right(setting, false);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH, NULL);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE, NULL);
            return ret;
         }
         break;
      case MENU_ACTION_SELECT:
         if (setting->action_select)
            return setting->action_select(setting, true);
         break;
      case MENU_ACTION_OK:
         if (setting->action_ok)
            return setting->action_ok(setting, false);
         break;
      case MENU_ACTION_CANCEL:
         if (setting->action_cancel)
            return setting->action_cancel(setting);
         break;
      case MENU_ACTION_START:
         if (setting->action_start)
            return setting->action_start(setting);
         break;
   }

   return -1;
}

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound)
{
   if (!setting)
      return -1;

   switch (setting_get_type(setting))
   {
      case ST_PATH:
         if (action == MENU_ACTION_OK)
         {
            menu_displaylist_info_t  info;
            file_list_t       *menu_stack = menu_entries_get_menu_stack_ptr(0);
            const char      *name         = setting->name;
            size_t selection              = menu_navigation_get_selection();

            menu_displaylist_info_init(&info);

            info.path                     = strdup(setting->default_value.string);
            info.label                    = strdup(name);
            info.type                     = type;
            info.directory_ptr            = selection;
            info.list                     = menu_stack;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info))
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
         if (setting_handler(setting, action) == 0)
            return menu_setting_generic(setting, wraparound);
         break;
      default:
         break;
   }

   return -1;
}

static rarch_setting_t *menu_setting_find_internal(rarch_setting_t *setting,
      const char *label)
{
   rarch_setting_t **list = &setting;

   for (; setting_get_type(setting) != ST_NONE; (*list = *list + 1))
   {
      const char *name              = setting->name;
      const char *short_description = setting->short_description;

      if (
            string_is_equal(label, name) &&
            (setting_get_type(setting) <= ST_GROUP))
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

static rarch_setting_t *menu_setting_find_internal_enum(rarch_setting_t *setting,
     enum msg_hash_enums enum_idx)
{
   rarch_setting_t **list = &setting;
   for (; setting_get_type(setting) != ST_NONE; (*list = *list + 1))
   {
      if (setting->enum_idx == enum_idx && setting_get_type(setting) <= ST_GROUP)
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

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting || !label)
      return NULL;

   return menu_setting_find_internal(setting, label);
}

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums enum_idx)
{
   rarch_setting_t *setting = NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting || enum_idx == 0)
      return NULL;

   return menu_setting_find_internal_enum(setting, enum_idx);
}

int menu_setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   switch (setting_get_type(setting))
   {
      case ST_STRING_OPTIONS:
         return MENU_SETTING_STRING_OPTIONS;
      case ST_ACTION:
         return MENU_SETTING_ACTION;
      case ST_PATH:
         return FILE_TYPE_PATH;
      case ST_GROUP:
         return MENU_SETTING_GROUP;
      case ST_SUB_GROUP:
         return MENU_SETTING_SUBGROUP;
      default:
         break;
   }

   return 0;
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

void *setting_get_ptr(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;

   switch (setting_get_type(setting))
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

/**
 * setting_get_string_representation:
 * @setting            : pointer to setting
 * @s                  : buffer to write contents of string representation to.
 * @len                : size of the buffer (@s)
 *
 * Get a setting value's string representation.
 **/
void setting_get_string_representation(rarch_setting_t *setting, char *s, size_t len)
{
   if (!setting || !s)
      return;

   if (setting->get_string_representation)
      setting->get_string_representation(setting, s, len);
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
         settings->uints.input_joypad_map[setting->index_offset], setting->index_offset);
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
      custom->width = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
   else
      custom->width = vp.full_width - custom->x;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

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
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;
   else
      custom->height = vp.full_height - custom->y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

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

static int setting_action_start_video_refresh_rate_auto(
      rarch_setting_t *setting)
{
   (void)setting;

   video_driver_monitor_reset();
   return 0;
}

/**
 ******* ACTION TOGGLE CALLBACK FUNCTIONS *******
**/

static int setting_action_right_analog_dpad_mode(rarch_setting_t *setting, bool wraparound)
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
      rarch_setting_t *setting, bool wraparound)
{
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

   return 0;
}

static int setting_action_right_bind_device(rarch_setting_t *setting, bool wraparound)
{
   unsigned index_offset;
   unsigned               *p = NULL;
   unsigned max_devices      = input_config_get_device_count();
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   index_offset = setting->index_offset;

   p = &settings->uints.input_joypad_map[index_offset];

   if (*p < max_devices)
      (*p)++;

   return 0;
}

static int setting_action_right_mouse_index(rarch_setting_t *setting, bool wraparound)
{
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   ++settings->uints.input_mouse_index[setting->index_offset];
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
      snprintf(s, len, "%.3f Hz (%.1f%% dev, %u samples)",
            video_refresh_rate, 100.0 * deviation, sample_points);
      menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);
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
   map          = settings->uints.input_joypad_map[index_offset];

   if (map < max_devices)
   {
      const char *device_name = input_config_get_device_display_name(map) ?
         input_config_get_device_display_name(map) : input_config_get_device_name(map);

      if (!string_is_empty(device_name))
      {
         unsigned idx = input_autoconfigure_get_device_name_index(map);

         /*if idx is non-zero, it's part of a set*/
         if ( idx > 0)
            snprintf(s, len,
                  "%s (#%u)",
                  device_name,
                  idx);
         else
            snprintf(s, len,
                  "%s",
                  device_name);
      }
      else
         snprintf(s, len,
               "%s (%s #%u)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
               map);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
}


/**
 * menu_setting_get_label:
 * @list               : File list on which to perform the search
 * @s                  : String for the type to be represented on-screen as
 *                       a label.
 * @len                : Size of @s
 * @w                  : Width of the string (for text label representation
 *                       purposes in the menu display driver).
 * @type               : Identifier of setting.
 * @menu_label         : Menu Label identifier of setting.
 * @label              : Label identifier of setting.
 * @idx                : Index identifier of setting.
 *
 * Get associated label of a setting.
 **/
void menu_setting_get_label(file_list_t *list, char *s,
      size_t len, unsigned *w, unsigned type,
      const char *menu_label, const char *label, unsigned idx)
{
   rarch_setting_t *setting = NULL;
   if (!list || !label)
      return;

   setting = menu_setting_find(list->list[idx].label);

   if (setting && setting->get_string_representation)
      setting->get_string_representation(setting, s, len);
}

void general_read_handler(rarch_setting_t *setting)
{
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return;

   if (setting->enum_idx == MSG_UNKNOWN)
      return;

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
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
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         *setting->value.target.fraction = settings->floats.audio_max_timing_skew;
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         *setting->value.target.fraction = settings->floats.video_refresh_rate;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_map[0];
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_map[1];
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_map[2];
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_map[3];
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
         *setting->value.target.integer = settings->uints.input_joypad_map[4];
         break;
      default:
         break;
   }
}

void general_write_handler(rarch_setting_t *setting)
{
   enum event_command rarch_cmd = CMD_EVENT_NONE;
   settings_t *settings         = config_get_ptr();

   if (!setting)
      return;

   if (setting->cmd_trigger.idx != CMD_EVENT_NONE)
   {
      uint64_t flags = setting->flags;

      if (flags & SD_FLAG_EXIT)
      {
         if (*setting->value.target.boolean)
            *setting->value.target.boolean = false;
      }
      if (setting->cmd_trigger.triggered ||
            (flags & SD_FLAG_CMD_APPLY_AUTO))
         rarch_cmd = setting->cmd_trigger.idx;
   }

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         {
            if (*setting->value.target.boolean)
               task_queue_set_threaded();
            else
               task_queue_unset_threaded();
         }
         break;
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         core_set_poll_type((unsigned int*)setting->value.target.integer);
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
               custom->x      = 0;
               custom->y      = 0;
               custom->width  = ((custom->width + geom->base_width   - 1) / geom->base_width)  * geom->base_width;
               custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;
               aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (float)custom->width / custom->height;
            }
         }
         break;
      case MENU_ENUM_LABEL_HELP:
         if (*setting->value.target.boolean)
         {
            menu_displaylist_info_t info;
            file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);

            menu_displaylist_info_init(&info);

            info.enum_idx                = MENU_ENUM_LABEL_HELP;
            info.label                   = strdup(
                  msg_hash_to_str(MENU_ENUM_LABEL_HELP));
            info.list                    = menu_stack;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info))
               menu_displaylist_process(&info);
            menu_displaylist_info_free(&info);
            setting_set_with_string_representation(setting, "false");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         configuration_set_float(settings, settings->floats.audio_max_timing_skew,
               *setting->value.target.fraction);
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
         driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, setting->value.target.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = CMD_EVENT_VIDEO_SET_BLOCKING_STATE;
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
         driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, setting->value.target.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = CMD_EVENT_VIDEO_SET_BLOCKING_STATE;
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         settings->modified           = true;
         settings->floats.video_scale = roundf(*setting->value.target.fraction);

         if (!settings->bools.video_fullscreen)
            rarch_cmd = CMD_EVENT_REINIT;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
         settings->modified            = true;
         settings->uints.input_joypad_map[0] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
         settings->modified            = true;
         settings->uints.input_joypad_map[1] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
         settings->modified            = true;
         settings->uints.input_joypad_map[2] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
         settings->modified            = true;
         settings->uints.input_joypad_map[3] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
         settings->modified            = true;
         settings->uints.input_joypad_map[4] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         if (!verbosity_is_enabled())
            verbosity_enable();
         else
            verbosity_disable();
         retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
         break;
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
         video_driver_set_filtering(1, settings->bools.video_smooth);
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         {
            rarch_system_info_t *system = runloop_get_system_info();

            if (system)
               video_driver_set_rotation(
                     (*setting->value.target.unsigned_integer +
                      system->rotation) % 4);
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         audio_set_float(AUDIO_ACTION_VOLUME_GAIN, *setting->value.target.fraction);
         break;
      case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
         audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, *setting->value.target.fraction);
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
#if defined(__CELLOS_LV2__) && (CELL_SDK_VERSION > 0x340000)
            cellSysutilEnableBgmPlayback();
#endif
         }
         else
         {
#if defined(__CELLOS_LV2__) && (CELL_SDK_VERSION > 0x340000)
            cellSysutilDisableBgmPlayback();
#endif
         }
         break;
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
#ifdef HAVE_NETWORKING
         {
            bool val = (!string_is_empty(setting->value.target.string));
            if (val)
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
            else
               retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
         }
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_MODE:
#ifdef HAVE_NETWORKING
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
#ifdef HAVE_NETWORKING
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
#ifdef HAVE_NETWORKING
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);
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
         midi_driver_set_output(settings->arrays.midi_output);
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
               buffer_size_setting->step = (*setting->value.target.unsigned_integer)*1024*1024 ;
         }
         break;
      case MENU_ENUM_LABEL_CHEAT_MEMORY_SEARCH_SIZE:
         {
            rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_VALUE);
            if (setting)
            {
               *(setting->value.target.unsigned_integer) = 0 ;
               setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1;
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0 ;
               setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1;
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION);
            if (setting)
            {
               int max_bit_position;
               *setting->value.target.unsigned_integer = 0 ;
               max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0 ;
               setting->max     = max_bit_position ;
            }

         }
         break;
      case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
         {
            rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0 ;
               setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0 ;
               setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS);
            if (setting)
            {
               *setting->value.target.unsigned_integer = 0 ;
               setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
            }

         }
         break;
      default:
         break;
   }

   if (rarch_cmd || setting->cmd_trigger.triggered)
      command_event(rarch_cmd, NULL);
}

#ifdef HAVE_OVERLAY
static void overlay_enable_toggle_change_handler(rarch_setting_t *setting)
{
   settings_t *settings  = config_get_ptr();

   if (!setting)
      return;

   if (settings && settings->bools.input_overlay_hide_in_menu)
   {
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
      return;
   }

   if (setting->value.target.boolean)
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   else
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
}
#endif

#ifdef HAVE_CHEEVOS
static void achievement_hardcore_mode_write_handler(rarch_setting_t *setting)
{
   settings_t *settings  = config_get_ptr();

   if (!setting)
      return;

   if (settings && settings->bools.cheevos_hardcore_mode_enable
         && cheevos_state_loaded_flag
         )
   {
      cheevos_hardcore_paused = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true);
      return;
   }
}
#endif

static void update_streaming_url_write_handler(rarch_setting_t *setting)
{
   recording_driver_update_streaming_url();
}

#ifdef HAVE_LAKKA
static void systemd_service_toggle(const char *path, char *unit, bool enable)
{
   int      pid = fork();
   char* args[] = {(char*)"systemctl", NULL, NULL, NULL};

   if (enable)
      args[1] = (char*)"start";
   else
      args[1] = (char*)"stop";
   args[2] = unit;

   if (enable)
      filestream_close(filestream_open(path,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(path);

   if (pid == 0)
      execvp(args[0], args);

   return;
}

static void ssh_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();

   if (settings && settings->bools.ssh_enable)
      enable = true;

   systemd_service_toggle(LAKKA_SSH_PATH, (char*)"sshd.service",
         enable);
}

static void samba_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();

   if (settings && settings->bools.samba_enable)
      enable = true;

   systemd_service_toggle(LAKKA_SAMBA_PATH, (char*)"smbd.service",
         enable);
}

static void bluetooth_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();

   if (settings && settings->bools.bluetooth_enable)
      enable = true;

   systemd_service_toggle(LAKKA_BLUETOOTH_PATH, (char*)"bluetooth.service",
         enable);
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
   unsigned i;
   rarch_setting_group_info_t group_info      = {0};
   rarch_setting_group_info_t subgroup_info   = {0};
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *system                = runloop_get_system_info();
   const struct retro_keybind* const defaults =
      (user == 0) ? retro_keybinds_1 : retro_keybinds_rest;
   const char *temp_value                     = msg_hash_to_str
      ((enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));

   snprintf(buffer[user],    sizeof(buffer[user]),
         "%s %u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);

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
      static char key_type[MAX_USERS][64];
      static char key_analog[MAX_USERS][64];
      static char key_bind_all[MAX_USERS][64];
      static char key_bind_all_save_autoconfig[MAX_USERS][64];
      static char split_joycon[MAX_USERS][64];
      static char split_joycon_lbl[MAX_USERS][64];
      static char key_bind_defaults[MAX_USERS][64];
      static char mouse_index[MAX_USERS][64];

      static char label[MAX_USERS][64];
      static char label_type[MAX_USERS][64];
      static char label_analog[MAX_USERS][64];
      static char label_bind_all[MAX_USERS][64];
      static char label_bind_all_save_autoconfig[MAX_USERS][64];
      static char label_bind_defaults[MAX_USERS][64];
      static char label_mouse_index[MAX_USERS][64];

      tmp_string[0] = '\0';

      snprintf(tmp_string, sizeof(tmp_string), "input_player%u", user + 1);

      fill_pathname_join_delim(key[user], tmp_string, "joypad_index", '_',
            sizeof(key[user]));
      snprintf(key_type[user], sizeof(key_type[user]),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE),
               user + 1);
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
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX));
      snprintf(label_type[user], sizeof(label_type[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE));
      snprintf(label_analog[user], sizeof(label_analog[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE));
      snprintf(label_bind_all[user], sizeof(label_bind_all[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL));
      snprintf(label_bind_defaults[user], sizeof(label_bind_defaults[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL));
      snprintf(label_bind_all_save_autoconfig[user], sizeof(label_bind_all_save_autoconfig[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG));
      snprintf(label_mouse_index[user], sizeof(label_mouse_index[user]),
               "%s %u %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX));

      CONFIG_UINT_ALT(
            list, list_info,
            input_config_get_device_ptr(user),
            key_type[user],
            label_type[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_libretro_device_type;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_select = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_libretro_device_type;
      (*list)[list_info->index - 1].get_string_representation =
         &setting_get_string_representation_uint_libretro_device;
      menu_settings_list_current_add_enum_idx(list, list_info,
            (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE + user));

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
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_analog_dpad_mode;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_select = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_analog_dpad_mode;
      (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
      (*list)[list_info->index - 1].get_string_representation =
         &setting_get_string_representation_uint_analog_dpad_mode;
      menu_settings_list_current_add_range(list, list_info, 0, 2, 1.0, true, true);
      menu_settings_list_current_add_enum_idx(list, list_info,
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
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_bind_device;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_bind_device;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].action_select = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_bind_device;

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

      CONFIG_UINT_ALT(
            list, list_info,
            &settings->uints.input_mouse_index[user],
            mouse_index[user],
            label_mouse_index[user],
            0,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index        = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_left  = &setting_action_left_mouse_index;
      (*list)[list_info->index - 1].action_right = &setting_action_right_mouse_index;
   }

   for (i = 0; i < RARCH_BIND_LIST_END; i ++)
   {
      char label[255];
      char name[255];

      if (input_config_bind_map_get_meta(i))
         continue;

      label[0] = name[0]          = '\0';

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

#define config_uint_cbs(var, label, left, right, msg_enum_base, string_rep, min, max, step) \
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

static bool setting_append_list(
      enum settings_list_type type,
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   unsigned user;
   rarch_setting_group_info_t group_info    = {0};
   rarch_setting_group_info_t subgroup_info = {0};
   settings_t *settings                     = config_get_ptr();
   global_t   *global                       = global_get_ptr();

   (void)settings;
   (void)global;

   switch (type)
   {
      case SETTINGS_LIST_MAIN_MENU:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MAIN_MENU);
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
         menu_settings_list_current_add_range(list, list_info, -1, 0, 1, true, false);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_START_CORE,
               MENU_ENUM_LABEL_VALUE_START_CORE,
               &group_info,
               &subgroup_info,
               parent_group);

#if defined(HAVE_VIDEO_PROCESSOR)
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
               menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_LOAD_CORE);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_BROWSER_ACTION);
            }
         }

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
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

         if (string_is_not_equal(settings->arrays.menu_driver, "xmb"))
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

#if !defined(__CELLOS_LV2__) && !defined(HAVE_DYNAMIC)
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_RESTART_RETROARCH,
               MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_RESTART_RETROARCH);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
               MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATIONS,
               MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG,
               MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_NEW_CONFIG,
               MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CONFIG);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
               MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_HELP_LIST,
               MENU_ENUM_LABEL_VALUE_HELP_LIST,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#ifdef HAVE_QT
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SHOW_WIMP,
               MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_UI_COMPANION_TOGGLE);
#endif
#if !defined(IOS)
         /* Apple rejects iOS apps that let you forcibly quit them. */
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_QUIT_RETROARCH,
               MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_QUIT);
#endif

#if defined(HAVE_LAKKA)
#ifdef HAVE_LAKKA_SWITCH
        CONFIG_ACTION(
              list, list_info,
              MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
              MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
              &group_info,
              &subgroup_info,
              parent_group);

        CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
               MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
               &group_info,
               &subgroup_info,
               parent_group);

        CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL,
               MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
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
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REBOOT);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SHUTDOWN,
               MENU_ENUM_LABEL_VALUE_SHUTDOWN,
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_SHUTDOWN);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_DRIVER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
               MENU_ENUM_LABEL_AUDIO_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CORE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_CONFIGURATION_SETTINGS,
               MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_SAVING_SETTINGS,
               MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_LOGGING_SETTINGS,
               MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_REWIND_SETTINGS,
               MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_RECORDING_SETTINGS,
               MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_OVERLAY
         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
               MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
               MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,
               MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,
               MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
               MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

#ifdef HAVE_LAKKA
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_PRIVACY_SETTINGS,
               MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_ACTION(
               list, list_info,
               MENU_ENUM_LABEL_MIDI_SETTINGS,
               MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
               &group_info,
               &subgroup_info,
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         for (user = 0; user < MAX_USERS; user++)
            setting_append_list_input_player_options(list, list_info, parent_group, user);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DRIVERS:
         {
            unsigned i;
            struct string_options_entry string_options_entries[11];

            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS), parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DRIVER_SETTINGS);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info,
                  &subgroup_info, parent_group);

            string_options_entries[0].target         = settings->arrays.input_driver;
            string_options_entries[0].len            = sizeof(settings->arrays.input_driver);
            string_options_entries[0].name_enum_idx  = MENU_ENUM_LABEL_INPUT_DRIVER;
            string_options_entries[0].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_DRIVER;
            string_options_entries[0].default_value  = config_get_default_input();
            string_options_entries[0].values         = config_get_input_driver_options();

            string_options_entries[1].target         = settings->arrays.input_joypad_driver;
            string_options_entries[1].len            = sizeof(settings->arrays.input_joypad_driver);
            string_options_entries[1].name_enum_idx  = MENU_ENUM_LABEL_JOYPAD_DRIVER;
            string_options_entries[1].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER;
            string_options_entries[1].default_value  = config_get_default_joypad();
            string_options_entries[1].values         = config_get_joypad_driver_options();

            string_options_entries[2].target         = settings->arrays.video_driver;
            string_options_entries[2].len            = sizeof(settings->arrays.video_driver);
            string_options_entries[2].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_DRIVER;
            string_options_entries[2].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER;
            string_options_entries[2].default_value  = config_get_default_video();
            string_options_entries[2].values         = config_get_video_driver_options();

            string_options_entries[3].target         = settings->arrays.audio_driver;
            string_options_entries[3].len            = sizeof(settings->arrays.audio_driver);
            string_options_entries[3].name_enum_idx  = MENU_ENUM_LABEL_AUDIO_DRIVER;
            string_options_entries[3].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER;
            string_options_entries[3].default_value  = config_get_default_audio();
            string_options_entries[3].values         = config_get_audio_driver_options();

            string_options_entries[4].target         = settings->arrays.audio_resampler;
            string_options_entries[4].len            = sizeof(settings->arrays.audio_resampler);
            string_options_entries[4].name_enum_idx  = MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER;
            string_options_entries[4].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER;
            string_options_entries[4].default_value  = config_get_default_audio_resampler();
            string_options_entries[4].values         = config_get_audio_resampler_driver_options();

            string_options_entries[5].target         = settings->arrays.camera_driver;
            string_options_entries[5].len            = sizeof(settings->arrays.camera_driver);
            string_options_entries[5].name_enum_idx  = MENU_ENUM_LABEL_CAMERA_DRIVER;
            string_options_entries[5].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER;
            string_options_entries[5].default_value  = config_get_default_camera();
            string_options_entries[5].values         = config_get_camera_driver_options();

            string_options_entries[6].target         = settings->arrays.wifi_driver;
            string_options_entries[6].len            = sizeof(settings->arrays.wifi_driver);
            string_options_entries[6].name_enum_idx  = MENU_ENUM_LABEL_WIFI_DRIVER;
            string_options_entries[6].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_WIFI_DRIVER;
            string_options_entries[6].default_value  = config_get_default_wifi();
            string_options_entries[6].values         = config_get_wifi_driver_options();

            string_options_entries[7].target         = settings->arrays.location_driver;
            string_options_entries[7].len            = sizeof(settings->arrays.location_driver);
            string_options_entries[7].name_enum_idx  = MENU_ENUM_LABEL_LOCATION_DRIVER;
            string_options_entries[7].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER;
            string_options_entries[7].default_value  = config_get_default_location();
            string_options_entries[7].values         = config_get_location_driver_options();

            string_options_entries[8].target         = settings->arrays.menu_driver;
            string_options_entries[8].len            = sizeof(settings->arrays.menu_driver);
            string_options_entries[8].name_enum_idx  = MENU_ENUM_LABEL_MENU_DRIVER;
            string_options_entries[8].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_MENU_DRIVER;
            string_options_entries[8].default_value  = config_get_default_menu();
            string_options_entries[8].values         = config_get_menu_driver_options();

            string_options_entries[9].target         = settings->arrays.record_driver;
            string_options_entries[9].len            = sizeof(settings->arrays.record_driver);
            string_options_entries[9].name_enum_idx  = MENU_ENUM_LABEL_RECORD_DRIVER;
            string_options_entries[9].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_RECORD_DRIVER;
            string_options_entries[9].default_value  = config_get_default_record();
            string_options_entries[9].values         = config_get_record_driver_options();

            string_options_entries[10].target         = settings->arrays.midi_driver;
            string_options_entries[10].len            = sizeof(settings->arrays.midi_driver);
            string_options_entries[10].name_enum_idx  = MENU_ENUM_LABEL_MIDI_DRIVER;
            string_options_entries[10].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_MIDI_DRIVER;
            string_options_entries[10].default_value  = config_get_default_midi();
            string_options_entries[10].values         = config_get_midi_driver_options();

            for (i = 0; i < ARRAY_SIZE(string_options_entries); i++)
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
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
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
            unsigned i;
            struct bool_entry bool_entries[5];

            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS), parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_SETTINGS);

            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            bool_entries[0].target         = &settings->bools.video_shared_context;
            bool_entries[0].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT;
            bool_entries[0].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT;
            bool_entries[0].default_value  = video_shared_context;
            bool_entries[0].flags          = SD_FLAG_ADVANCED;

            bool_entries[1].target         = &settings->bools.load_dummy_on_core_shutdown;
            bool_entries[1].name_enum_idx  = MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN;
            bool_entries[1].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN;
            bool_entries[1].default_value  = load_dummy_on_core_shutdown;
            bool_entries[1].flags          = SD_FLAG_ADVANCED;

            bool_entries[2].target         = &settings->bools.set_supports_no_game_enable;
            bool_entries[2].name_enum_idx  = MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
            bool_entries[2].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE;
            bool_entries[2].default_value  = true;
            bool_entries[2].flags          = SD_FLAG_ADVANCED;

            bool_entries[3].target         = &settings->bools.check_firmware_before_loading;
            bool_entries[3].name_enum_idx  = MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE;
            bool_entries[3].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE;
            bool_entries[3].default_value  = true;
            bool_entries[3].flags          = SD_FLAG_ADVANCED;

            bool_entries[4].target         = &settings->bools.video_allow_rotate;
            bool_entries[4].name_enum_idx  = MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE;
            bool_entries[4].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE;
            bool_entries[4].default_value  = allow_rotate;
            bool_entries[4].flags          = SD_FLAG_ADVANCED;

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


            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CONFIGURATION:
         {
            uint8_t i;
            struct bool_entry bool_entries[6];
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS), parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            bool_entries[0].target         = &settings->bools.config_save_on_exit;
            bool_entries[0].name_enum_idx  = MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT;
            bool_entries[0].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT;
            bool_entries[0].default_value  = config_save_on_exit;
            bool_entries[0].flags          = SD_FLAG_NONE;

            bool_entries[1].target         = &settings->bools.show_hidden_files;
            bool_entries[1].name_enum_idx  = MENU_ENUM_LABEL_SHOW_HIDDEN_FILES;
            bool_entries[1].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES;
            bool_entries[1].default_value  = show_hidden_files;
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

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_LOGGING:
         {
            bool *tmp_b = NULL;
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS), parent_group);
            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS);

            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.libretro_log_level,
                  MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL,
                  MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
                  libretro_log_level,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_libretro_log_level;
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Performance Counters", &group_info, &subgroup_info,
                  parent_group);

            rarch_ctl(RARCH_CTL_GET_PERFCNT, &tmp_b);

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
            struct bool_entry bool_entries[11];

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

            bool_entries[2].target         = &settings->bools.block_sram_overwrite;
            bool_entries[2].name_enum_idx  = MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE;
            bool_entries[2].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE;
            bool_entries[2].default_value  = block_sram_overwrite;
            bool_entries[2].flags          = SD_FLAG_NONE;

            bool_entries[3].target         = &settings->bools.savestate_auto_index;
            bool_entries[3].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX;
            bool_entries[3].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX;
            bool_entries[3].default_value  = savestate_auto_index;
            bool_entries[3].flags          = SD_FLAG_NONE;

            bool_entries[4].target         = &settings->bools.savestate_auto_save;
            bool_entries[4].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE;
            bool_entries[4].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE;
            bool_entries[4].default_value  = savestate_auto_save;
            bool_entries[4].flags          = SD_FLAG_NONE;

            bool_entries[5].target         = &settings->bools.savestate_auto_load;
            bool_entries[5].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD;
            bool_entries[5].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD;
            bool_entries[5].default_value  = savestate_auto_load;
            bool_entries[5].flags          = SD_FLAG_NONE;

            bool_entries[6].target         = &settings->bools.savestate_thumbnail_enable;
            bool_entries[6].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE;
            bool_entries[6].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE;
            bool_entries[6].default_value  = savestate_thumbnail_enable;
            bool_entries[6].flags          = SD_FLAG_ADVANCED;

            bool_entries[7].target         = &settings->bools.savefiles_in_content_dir;
            bool_entries[7].name_enum_idx  = MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[7].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[7].default_value  = default_savefiles_in_content_dir;
            bool_entries[7].flags          = SD_FLAG_ADVANCED;

            bool_entries[8].target         = &settings->bools.savestates_in_content_dir;
            bool_entries[8].name_enum_idx  = MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE;
            bool_entries[8].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE;
            bool_entries[8].default_value  = default_savestates_in_content_dir;
            bool_entries[8].flags          = SD_FLAG_ADVANCED;

            bool_entries[9].target         = &settings->bools.systemfiles_in_content_dir;
            bool_entries[9].name_enum_idx  = MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[9].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE;
            bool_entries[9].default_value  = default_systemfiles_in_content_dir;
            bool_entries[9].flags          = SD_FLAG_ADVANCED;

            bool_entries[10].target         = &settings->bools.screenshots_in_content_dir;
            bool_entries[10].name_enum_idx  = MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
            bool_entries[10].SHORT_enum_idx = MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE;
            bool_entries[10].default_value  = default_screenshots_in_content_dir;
            bool_entries[10].flags          = SD_FLAG_ADVANCED;

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
                  autosave_interval,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_AUTOSAVE_INIT);
            menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_autosave_interval;
#endif


            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
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
               rewind_enable,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REWIND_TOGGLE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.rewind_granularity,
                  MENU_ENUM_LABEL_REWIND_GRANULARITY,
                  MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
                  rewind_granularity,
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
                  rewind_buffer_size,
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
                  rewind_buffer_size_step,
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
                  apply_cheats_after_load,
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
                  apply_cheats_after_toggle,
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
         {
            int max_bit_position;
            if (!cheat_manager_state.cheats)
               break ;

            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS), parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DETAILS_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);



            config_uint_cbs(cheat_manager_state.working_cheat.idx, CHEAT_IDX,
                  NULL,NULL,
                  0,&setting_get_string_representation_uint,0,cheat_manager_get_size()-1,1) ;

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

            config_uint_cbs(cheat_manager_state.working_cheat.handler, CHEAT_HANDLER,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,&setting_get_string_representation_uint_as_enum,
                  CHEAT_HANDLER_TYPE_EMU,CHEAT_HANDLER_TYPE_RETRO,1) ;
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

            config_uint_cbs(cheat_manager_state.working_cheat.memory_search_size, CHEAT_MEMORY_SEARCH_SIZE,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,&setting_get_string_representation_uint_as_enum,
                  0,5,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.cheat_type, CHEAT_TYPE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,&setting_get_string_representation_uint_as_enum,
                  CHEAT_TYPE_DISABLED,CHEAT_TYPE_RUN_NEXT_IF_GT,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.value, CHEAT_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,(int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.address, CHEAT_ADDRESS,
                  setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
                  0,&setting_get_string_representation_hex_and_uint,0,cheat_manager_state.total_memory_size==0?0:cheat_manager_state.total_memory_size-1,1) ;

            max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0 ;
            config_uint_cbs(cheat_manager_state.working_cheat.address_mask, CHEAT_ADDRESS_BIT_POSITION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,max_bit_position,1) ;

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

            config_uint_cbs(cheat_manager_state.working_cheat.repeat_count, CHEAT_REPEAT_COUNT,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,1,2048,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.repeat_add_to_address, CHEAT_REPEAT_ADD_TO_ADDRESS,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,1,2048,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.repeat_add_to_value, CHEAT_REPEAT_ADD_TO_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,0xFFFF,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_type, CHEAT_RUMBLE_TYPE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,&setting_get_string_representation_uint_as_enum,
                  RUMBLE_TYPE_DISABLED,RUMBLE_TYPE_GT_VALUE,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_value, CHEAT_RUMBLE_VALUE,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,(int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_port, CHEAT_RUMBLE_PORT,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  MENU_ENUM_LABEL_RUMBLE_PORT_0,&setting_get_string_representation_uint_as_enum,
                  0,16,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_primary_strength, CHEAT_RUMBLE_PRIMARY_STRENGTH,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,65535,1) ;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_primary_duration, CHEAT_RUMBLE_PRIMARY_DURATION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_uint,0,5000,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_secondary_strength, CHEAT_RUMBLE_SECONDARY_STRENGTH,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_hex_and_uint,0,65535,1) ;

            config_uint_cbs(cheat_manager_state.working_cheat.rumble_secondary_duration, CHEAT_RUMBLE_SECONDARY_DURATION,
                  setting_uint_action_left_default,setting_uint_action_right_default,
                  0,&setting_get_string_representation_uint,0,5000,1) ;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CHEAT_SEARCH:
         if (!cheat_manager_state.cheats)
            break ;

         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         config_uint_cbs(cheat_manager_state.search_bit_size, CHEAT_START_OR_RESTART,
               setting_uint_action_left_with_refresh,setting_uint_action_right_with_refresh,
               MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,&setting_get_string_representation_uint_as_enum,
               0,5,1) ;
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
         menu_settings_list_current_add_range(list, list_info, 0, (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1, 1, true, true);
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
         menu_settings_list_current_add_range(list, list_info, 0, (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1, 1, true, true);
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
         menu_settings_list_current_add_range(list, list_info, 0, (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1, 1, true, true);
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
         menu_settings_list_current_add_range(list, list_info, 0, cheat_manager_state.actual_memory_size>0?cheat_manager_state.actual_memory_size-1:0, 1, true, true);
         (*list)[list_info->index - 1].action_left = &setting_uint_action_left_with_refresh;
         (*list)[list_info->index - 1].action_right = &setting_uint_action_right_with_refresh;
         (*list)[list_info->index - 1].get_string_representation = &setting_get_string_representation_uint_cheat_browse_address;


         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_VIDEO:
         {
            struct video_viewport *custom_vp   = video_viewport_get_custom();
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS), parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SETTINGS);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_fps_show,
                  MENU_ENUM_LABEL_FPS_SHOW,
                  MENU_ENUM_LABEL_VALUE_FPS_SHOW,
                  fps_show,
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
                  &settings->bools.video_statistics_show,
                  MENU_ENUM_LABEL_STATISTICS_SHOW,
                  MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
                  fps_show,
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
               fps_show,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Platform-specific", &group_info, &subgroup_info, parent_group);

            video_driver_menu_settings((void**)list, (void*)list_info, (void*)&group_info, (void*)&subgroup_info, parent_group);

            END_SUB_GROUP(list, list_info, parent_group);

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Monitor", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_monitor_index,
                  MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX,
                  MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
                  monitor_index,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
            menu_settings_list_current_add_range(list, list_info, 0, 1, 1, true, false);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_video_monitor_index;

            if (video_driver_has_windowed())
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_fullscreen,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
                     fullscreen,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_CMD_APPLY_AUTO);
               menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT_FROM_TOGGLE);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            }
            if (video_driver_has_windowed())
            {
               CONFIG_BOOL(
                     list, list_info,
                     &settings->bools.video_windowed_fullscreen,
                     MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
                     windowed_fullscreen,
                     MENU_ENUM_LABEL_VALUE_OFF,
                     MENU_ENUM_LABEL_VALUE_ON,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_NONE);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_fullscreen_x,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
                     fullscreen_x,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 0, 7680, 8, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_fullscreen_y,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y,
                     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
                     fullscreen_y,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 0, 4320, 8, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            }
            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.video_refresh_rate,
                  MENU_ENUM_LABEL_VIDEO_REFRESH_RATE,
                  MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
                  refresh_rate,
                  "%.3f Hz",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 0, 0.001, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.video_refresh_rate,
                  MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO,
                  MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
                  refresh_rate,
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            {
               float actual_refresh_rate = video_driver_get_refresh_rate();
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
                  (*list)[list_info->index - 1].action_ok     = &setting_action_ok_video_refresh_rate_polled;
                  (*list)[list_info->index - 1].action_select = &setting_action_ok_video_refresh_rate_polled;
                  (*list)[list_info->index - 1].get_string_representation =
                     &setting_get_string_representation_st_float_video_refresh_rate_polled;
                  settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
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
               menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
            }

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Aspect", &group_info, &subgroup_info, parent_group);
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_aspect_ratio_idx,
                  MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
                  MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
                  aspect_ratio_idx,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_cmd(
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_aspect_ratio_index;

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->floats.video_aspect_ratio,
                  MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO,
                  MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
                  1.33,
                  "%.2f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_SET_ASPECT_RATIO);
            menu_settings_list_current_add_range(list, list_info, 0.1, 16.0, 0.01, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, -99999, 0, 1, false, false);
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, -99999, 0, 1, false, false);
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_custom_viewport_width;
            (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_width;
            (*list)[list_info->index - 1].action_left  = setting_uint_action_left_custom_viewport_width;
            (*list)[list_info->index - 1].action_right = setting_uint_action_right_custom_viewport_width;
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_custom_viewport_height;
            (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_height;
            (*list)[list_info->index - 1].action_left  = setting_uint_action_left_custom_viewport_height;
            (*list)[list_info->index - 1].action_right = setting_uint_action_right_custom_viewport_height;
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(list, list_info, "Scaling", &group_info, &subgroup_info, parent_group);

            if (video_driver_has_windowed())
            {
               CONFIG_FLOAT(
                     list, list_info,
                     &settings->floats.video_scale,
                     MENU_ENUM_LABEL_VIDEO_SCALE,
                     MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
                     scale,
                     "%.1fx",
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               menu_settings_list_current_add_range(list, list_info, 1.0, 10.0, 1.0, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_window_x,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 7680, 8, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_window_y,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
                     0,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               menu_settings_list_current_add_range(list, list_info, 0, 4320, 8, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.video_window_opacity,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY,
                     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
                     window_opacity,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].offset_by = 1;
               menu_settings_list_current_add_range(list, list_info, 1, 100, 1, true, true);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_window_show_decorations,
                  MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
                  MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
                  window_decorations,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_scale_integer,
                  MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
                  scale_integer,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_cmd(
                  list,
                  list_info,
                  CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);

#ifdef GEKKO
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_viwidth,
                  MENU_ENUM_LABEL_VIDEO_VI_WIDTH,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
                  video_viwidth,
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
                  video_vfilter,
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
                  &settings->bools.video_smooth,
                  MENU_ENUM_LABEL_VIDEO_SMOOTH,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
                  video_smooth,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);

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
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_video_rotation;
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);
            START_SUB_GROUP(
                  list,
                  list_info,
                  "Synchronization",
                  &group_info,
                  &subgroup_info,
                  parent_group);

#if defined(HAVE_THREADS)
            CONFIG_BOOL(
                  list, list_info,
                  video_driver_get_threaded(),
                  MENU_ENUM_LABEL_VIDEO_THREADED,
                  MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
                  video_threaded,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO
                  );
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_vsync,
                  MENU_ENUM_LABEL_VIDEO_VSYNC,
                  MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
                  vsync,
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
                  &settings->uints.video_swap_interval,
                  MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL,
                  MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
                  swap_interval,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_VIDEO_SET_BLOCKING_STATE);
            menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            {
               gfx_ctx_flags_t flags;

               if (video_driver_get_all_flags(&flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES))
               {
                  CONFIG_UINT(
                        list, list_info,
                        &settings->uints.video_max_swapchain_images,
                        MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
                        MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
                        max_swapchain_images,
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler);
                  menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
                  settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
               }
            }

            {
               gfx_ctx_flags_t flags;

               if (video_driver_get_all_flags(&flags, GFX_CTX_FLAGS_HARD_SYNC))
               {
                  CONFIG_BOOL(
                        list, list_info,
                        &settings->bools.video_hard_sync,
                        MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
                        hard_sync,
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
                        &settings->uints.video_hard_sync_frames,
                        MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
                        MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
                        hard_sync_frames,
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler);
                  (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
                  menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
               }
            }

            {
               gfx_ctx_flags_t flags;

               if (video_driver_get_all_flags(&flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC))
               {
                  CONFIG_BOOL(
                        list, list_info,
                        &settings->bools.video_adaptive_vsync,
                        MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC,
                        MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
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
               }
            }

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.video_frame_delay,
                  MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
                  MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
                  frame_delay,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#if !defined(RARCH_MOBILE)
            {
               gfx_ctx_flags_t flags;

               if (video_driver_get_all_flags(&flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION))
               {
                  CONFIG_BOOL(
                        list, list_info,
                        &settings->bools.video_black_frame_insertion,
                        MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION,
                        MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
                        black_frame_insertion,
                        MENU_ENUM_LABEL_VALUE_OFF,
                        MENU_ENUM_LABEL_VALUE_ON,
                        &group_info,
                        &subgroup_info,
                        parent_group,
                        general_write_handler,
                        general_read_handler,
                        SD_FLAG_NONE
                        );
                  settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
               }
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
                  gpu_screenshot,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_crop_overscan,
                  MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN,
                  MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
                  crop_overscan,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            menu_settings_list_current_add_values(list, list_info, "filt");
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_CRT_SWITCHRES:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

			CONFIG_UINT(
               list, list_info,
               &settings->uints.crt_switch_resolution,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
               crt_switch_resolution,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_crt_switch_resolutions;
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_range(list, list_info, CRT_SWITCH_NONE, CRT_SWITCH_31KHZ, 1.0, true, true);

			CONFIG_UINT(
				  list, list_info,
				  &settings->uints.crt_switch_resolution_super,
				  MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER,
				  MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
				  crt_switch_resolution_super,
				  &group_info,
				  &subgroup_info,
				  parent_group,
				  general_write_handler,
				  general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         (*list)[list_info->index - 1].action_left   = &setting_uint_action_left_crt_switch_resolution_super;
         (*list)[list_info->index - 1].action_right  = &setting_uint_action_right_crt_switch_resolution_super;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_crt_switch_resolution_super;

			CONFIG_INT(
				  list, list_info,
				  &settings->ints.crt_switch_center_adjust,
				  MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING,
				  MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
				  crt_switch_center_adjust,
				  &group_info,
				  &subgroup_info,
				  parent_group,
				  general_write_handler,
				  general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by     = -3;
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_range(list, list_info, -3, 4, 1.0, true, true);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.crt_switch_custom_refresh_enable,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
               MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
               audio_enable,
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
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_enable,
               MENU_ENUM_LABEL_AUDIO_ENABLE,
               MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
               audio_enable,
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
               &settings->bools.audio_enable_menu,
               MENU_ENUM_LABEL_AUDIO_ENABLE_MENU,
               MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
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

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_volume,
               MENU_ENUM_LABEL_AUDIO_VOLUME,
               MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
               audio_volume,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_mixer_volume,
               MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME,
               MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
               audio_mixer_volume,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         END_SUB_GROUP(list, list_info, parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(
               list,
               list_info,
               "Synchronization",
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.audio_sync,
               MENU_ENUM_LABEL_AUDIO_SYNC,
               MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
               audio_sync,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_latency,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
               g_defaults.settings.out_latency ?
               g_defaults.settings.out_latency : out_latency,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, 512, 1.0, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_audio_resampler_quality;
         menu_settings_list_current_add_range(list, list_info, RESAMPLER_QUALITY_DONTCARE, RESAMPLER_QUALITY_HIGHEST, 1.0, true, true);

         CONFIG_FLOAT(
               list, list_info,
               audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA),
               MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
               MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
               rate_control_delta,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               0,
               0.001,
               true,
               false);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.audio_max_timing_skew,
               MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW,
               MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
               max_timing_skew,
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].action_left   = &setting_string_action_left_audio_device;
         (*list)[list_info->index - 1].action_right  = &setting_string_action_right_audio_device;
#endif

         CONFIG_UINT(
               list, list_info,
               &settings->uints.audio_out_rate,
               MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE,
               MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
               out_rate,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 1000, 192000, 100.0, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
         menu_settings_list_current_add_values(list, list_info, "dsp");
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_DSP_FILTER_INIT);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#ifdef HAVE_WASAPI
         if (string_is_equal(settings->arrays.audio_driver, "wasapi"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.audio_wasapi_exclusive_mode,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
                  MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
                  wasapi_exclusive_mode,
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
                  wasapi_float_format,
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
                  wasapi_sh_buffer_length,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, -16.0f, 0.0f, 16.0f, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
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
                  input_driver_get_uint(INPUT_ACTION_MAX_USERS),
                  MENU_ENUM_LABEL_INPUT_MAX_USERS,
                  MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
                  input_max_users,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_max_users;
            (*list)[list_info->index - 1].offset_by = 1;
            menu_settings_list_current_add_range(list, list_info, 1, MAX_USERS, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_poll_type_behavior;
            menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
                  menu_toggle_gamepad_combo,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_toggle_gamepad_combo;
            menu_settings_list_current_add_range(list, list_info, 0, (INPUT_TOGGLE_LAST-1), 1, true, true);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.input_menu_swap_ok_cancel_buttons,
                  MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL,
                  MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
                  menu_swap_ok_cancel_buttons,
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
                  all_users_control_menu,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
                  input_driver_get_float(INPUT_ACTION_AXIS_THRESHOLD),
                  MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD,
                  MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD,
                  axis_threshold,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 1.00, 0.001, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Binds", &group_info, &subgroup_info, parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,
                  MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
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
               (*list)[list_info->index - 1].index          = user_value;
               (*list)[list_info->index - 1].index_offset   = user;

               menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));
            }

            END_SUB_GROUP(list, list_info, parent_group);

            END_GROUP(list, list_info, parent_group);
         }
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
            menu_settings_list_current_add_range(list, list_info, RECORD_CONFIG_TYPE_RECORDING_CUSTOM, RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY, 1, true, true);

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
            menu_settings_list_current_add_values(list, list_info, "cfg");

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
               1,
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
               (*list)[list_info->index - 1].offset_by = 5;
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
            menu_settings_list_current_add_values(list, list_info, "cfg");

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

            CONFIG_DIR(
               list, list_info,
               global->record.output_dir,
               sizeof(global->record.output_dir),
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

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Miscellaneous", &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.video_post_filter_record,
                  MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,
                  MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
                  post_filter_record,
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
                  gpu_record,
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
               (*list)[list_info->index - 1].bind_type = i + MENU_SETTINGS_BIND_BEGIN;
               menu_settings_list_current_add_enum_idx(list, list_info,
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
               fastforward_ratio,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_SET_FRAME_LIMIT);
         menu_settings_list_current_add_range(list, list_info, 0, 10, 1.0, true, true);

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
               slowmotion_ratio,
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
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].offset_by = 1;
         menu_settings_list_current_add_range(list, list_info, 1, 6, 1, true, true);

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.run_ahead_secondary_instance,
               MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE,
               MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
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
               &settings->bools.run_ahead_hide_warnings,
               MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS,
               MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
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
      case SETTINGS_LIST_FONT:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS);

         START_SUB_GROUP(list, list_info, "Messages",
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_font_enable,
               MENU_ENUM_LABEL_VIDEO_FONT_ENABLE,
               MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
               font_enable,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );

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

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.video_font_size,
               MENU_ENUM_LABEL_VIDEO_FONT_SIZE,
               MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
               font_size,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 1.00, 100.00, 1.0, true, true);

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
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_enable_autopreferred,
               MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED,
               MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
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
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.input_overlay_hide_in_menu,
               MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
               overlay_hide_in_menu,
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
               &settings->bools.input_overlay_show_physical_inputs,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
               MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
               show_physical_inputs,
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
                  &settings->uints.input_overlay_show_physical_inputs_port,
                  MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
                  MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler
                  );
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_range(list, list_info, 0, MAX_USERS - 1, 1, true, true);

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
         menu_settings_list_current_add_values(list, list_info, "cfg");
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_INIT);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_opacity,
               MENU_ENUM_LABEL_OVERLAY_OPACITY,
               MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
               0.7f,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_SET_ALPHA_MOD);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->floats.input_overlay_scale,
               MENU_ENUM_LABEL_OVERLAY_SCALE,
               MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
               1.0f,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, 0, 2, 0.01, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Onscreen Keyboard Overlay", &group_info, &subgroup_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_MENU:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS),
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         if (string_is_not_equal(settings->arrays.menu_driver, "rgui"))
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
            menu_settings_list_current_add_values(list, list_info, "png");

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_dynamic_wallpaper_enable,
                  MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,
                  MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
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
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_PAUSE_LIBRETRO);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_mouse_enable,
               MENU_ENUM_LABEL_MOUSE_ENABLE,
               MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
               def_mouse_enable,
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
               pointer_enable,
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
            gfx_ctx_flags_t flags;

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

            if (video_driver_get_all_flags(&flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING))
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

            /* These colors are hints. The menu driver is not required to use them. */
            CONFIG_HEX(
                  list, list_info,
                  &settings->uints.menu_entry_normal_color,
                  MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR,
                  MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR,
                  menu_entry_normal_color,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_HEX(
                  list, list_info,
                  &settings->uints.menu_entry_hover_color,
                  MENU_ENUM_LABEL_ENTRY_HOVER_COLOR,
                  MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR,
                  menu_entry_hover_color,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_HEX(
                  list, list_info,
                  &settings->uints.menu_title_color,
                  MENU_ENUM_LABEL_TITLE_COLOR,
                  MENU_ENUM_LABEL_VALUE_TITLE_COLOR,
                  menu_title_color,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         }

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_horizontal_animation,
                  MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,
                  MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
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
#ifdef RARCH_MOBILE
            /* We don't want mobile users being able to switch this off. */
            (*list)[list_info->index - 1].action_left   = NULL;
            (*list)[list_info->index - 1].action_right  = NULL;
            (*list)[list_info->index - 1].action_start  = NULL;
#endif
         }


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
               show_advanced_settings,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.kiosk_mode_enable,
                  MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,
                  MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
                  kiosk_mode_enable,
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         }

#ifdef HAVE_THREADS
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.threaded_data_runloop_enable,
               MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE,
               MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
               threaded_data_runloop_enable,
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

         if (string_is_equal(settings->arrays.menu_driver, "glui"))
         {
            /* only GLUI uses these values, don't show
             * them on other drivers */
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_dpi_override_enable,
                  MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE,
                  MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
                  menu_dpi_override_enable,
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
                  &settings->uints.menu_dpi_override_value,
                  MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE,
                  MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
                  menu_dpi_override_value,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 72;
            menu_settings_list_current_add_range(list, list_info, 72, 999, 1, true, true);
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_xmb_scale_factor,
                  MENU_ENUM_LABEL_XMB_SCALE_FACTOR,
                  MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR,
                  xmb_scale_factor,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].offset_by = 20;
            menu_settings_list_current_add_range(list, list_info, (*list)[list_info->index -1].offset_by, 200, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_float_video_msg_color;
            menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_float_video_msg_color;
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            menu_settings_list_current_add_range(list, list_info, 0, 255, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);

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
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_xmb_shadows_enable,
                  MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,
                  MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
                  xmb_shadows_enable,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);

#ifdef HAVE_SHADERPIPELINE
            if (video_shader_any_supported())
            {
               CONFIG_UINT(
                     list, list_info,
                     &settings->uints.menu_xmb_shader_pipeline,
                     MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,
                     MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
                     menu_shader_pipeline,
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler);
               (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
               (*list)[list_info->index - 1].get_string_representation =
                  &setting_get_string_representation_uint_xmb_shader_pipeline;
               menu_settings_list_current_add_range(list, list_info, 0, XMB_SHADER_PIPELINE_LAST-1, 1, true, true);
            }
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
       }
#endif
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
#endif

#ifdef HAVE_XMB
         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED);
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

#ifdef HAVE_LIBRETRODB
            CONFIG_BOOL(
                  list, list_info,
                  &settings->bools.menu_content_show_add,
                  MENU_ENUM_LABEL_CONTENT_SHOW_ADD,
                  MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
                  content_show_add,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
#endif

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
                  materialui_icons_enable,
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
                  &settings->uints.menu_materialui_color_theme,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,
                  MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
                  MATERIALUI_THEME_BLUE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_materialui_menu_color_theme;
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_THEME_LAST-1, 1, true, true);

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
         }
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_show_start_screen,
               MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN,
               MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
               default_menu_show_start_screen,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_thumbnails,
                  MENU_ENUM_LABEL_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_THUMBNAILS,
                  menu_thumbnails_default,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_uint_menu_thumbnails;
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.menu_left_thumbnails,
                  MENU_ENUM_LABEL_LEFT_THUMBNAILS,
                  MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
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
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.menu_timedate_enable,
               MENU_ENUM_LABEL_TIMEDATE_ENABLE,
               MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
               true,
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
            menu_timedate_style,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
         (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_menu_timedate_style;
         menu_settings_list_current_add_range(list, list_info, 0, 7, 1, true, true);

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

#ifdef HAVE_LIBRETRODB
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.automatically_add_content_to_playlist,
               MENU_ENUM_LABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
               MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
               true,
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
               &settings->bools.multimedia_builtin_mediaplayer_enable,
               MENU_ENUM_LABEL_USE_BUILTIN_PLAYER,
               MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
               true,
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
               true,
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

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
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
               pause_nonactive,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

#if !defined(RARCH_MOBILE)
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.video_disable_composition,
               MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION,
               MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
               disable_composition,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);
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
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.quick_menu_show_take_screenshot,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               quick_menu_show_take_screenshot,
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
               quick_menu_show_save_load_state,
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
               quick_menu_show_undo_save_load_state,
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

         if (string_is_not_equal(ui_companion_driver_get_ident(), "null"))
         {
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
                  true,
                  MENU_ENUM_LABEL_VALUE_OFF,
                  MENU_ENUM_LABEL_VALUE_ON,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
         }
#ifdef HAVE_QT
         CONFIG_BOOL(
               list, list_info,
               &settings->bools.desktop_menu_enable,
               MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE,
               MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
               desktop_menu_enable,
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "History", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.history_list_enable,
               MENU_ENUM_LABEL_HISTORY_LIST_ENABLE,
               MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
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
         menu_settings_list_current_add_range(list, list_info, 0, 0, 1.0, true, false);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Playlist", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.playlist_entry_rename,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
               def_playlist_entry_rename,
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
               &settings->bools.playlist_entry_remove,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE,
               MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
               def_playlist_entry_remove,
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
               cheevos_enable,
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
               &settings->bools.cheevos_test_unofficial,
               MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
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
               &settings->bools.cheevos_leaderboards_enable,
               MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
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

         if (string_is_equal(settings->arrays.menu_driver, "xmb"))
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
                  SD_FLAG_NONE
                  );

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.cheevos_verbose_enable,
               MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
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
               &settings->bools.cheevos_hardcore_mode_enable,
               MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
               false,
               MENU_ENUM_LABEL_VALUE_OFF,
               MENU_ENUM_LABEL_VALUE_ON,
               &group_info,
               &subgroup_info,
               parent_group,
               achievement_hardcore_mode_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE);

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
         CONFIG_STRING(
               list, list_info,
               settings->paths.network_buildbot_url,
               sizeof(settings->paths.network_buildbot_url),
               MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL,
               MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
               buildbot_server_url,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

         CONFIG_STRING(
               list, list_info,
               settings->paths.network_buildbot_assets_url,
               sizeof(settings->paths.network_buildbot_assets_url),
               MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL,
               MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
               buildbot_assets_server_url,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

         CONFIG_BOOL(
               list, list_info,
               &settings->bools.network_buildbot_auto_extract_archive,
               MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
               MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
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

            CONFIG_STRING(
                  list, list_info,
                  settings->arrays.netplay_mitm_server,
                  sizeof(settings->arrays.netplay_mitm_server),
                  MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
                  netplay_mitm_server,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_netplay_mitm_server;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_netplay_mitm_server;
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_netplay_mitm_server;

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            menu_settings_list_current_add_range(list, list_info, -600, 600, 1, false, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_share_digital,
                  MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_netplay_share_digital;
            menu_settings_list_current_add_range(list, list_info, 0, RARCH_NETPLAY_SHARE_DIGITAL_LAST-1, 1, true, true);

            CONFIG_UINT(
                  list, list_info,
                  &settings->uints.netplay_share_analog,
                  MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG,
                  MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            (*list)[list_info->index - 1].action_ok = &setting_action_ok_uint;
            (*list)[list_info->index - 1].get_string_representation =
               &setting_get_string_representation_netplay_share_analog;
            menu_settings_list_current_add_range(list, list_info, 0, RARCH_NETPLAY_SHARE_ANALOG_LAST-1, 1, true, true);

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
               settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
               menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + user));
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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

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
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            /* TODO/FIXME - add enum_idx */

            {
               unsigned max_users        = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
               for(user = 0; user < max_users; user++)
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
                  settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
                  menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user));
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_LAKKA_ADVANCED);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

#ifdef HAVE_LANGEXTRA
         CONFIG_UINT(
               list, list_info,
               msg_hash_get_uint(MSG_HASH_USER_LANGUAGE),
               MENU_ENUM_LABEL_USER_LANGUAGE,
               MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
               def_user_language,
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
         (*list)[list_info->index - 1].get_string_representation =
            &setting_get_string_representation_uint_user_language;
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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

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
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DIRECTORY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS),
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

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
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CORE_INFO_INIT);
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
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CORE_INFO_INIT);
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

         if (string_is_not_equal(settings->arrays.record_driver, "null"))
         {
            CONFIG_DIR(
                  list, list_info,
                  global->record.output_dir,
                  sizeof(global->record.output_dir),
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
                  global->record.config_dir,
                  sizeof(global->record.config_dir),
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
               midi_input,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_midi_input;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_midi_input;

         CONFIG_STRING(
               list, list_info,
               settings->arrays.midi_output,
               sizeof(settings->arrays.midi_output),
               MENU_ENUM_LABEL_MIDI_OUTPUT,
               MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
               midi_output,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
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
   for (; setting_get_type(setting) != ST_NONE; (*list = *list + 1))
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

static void menu_setting_terminate_last(rarch_setting_t *list, unsigned pos)
{
   (*&list)[pos].enum_idx           = MSG_UNKNOWN;
   (*&list)[pos].type               = ST_NONE;
   (*&list)[pos].size               = 0;
   (*&list)[pos].name               = NULL;
   (*&list)[pos].short_description  = NULL;
   (*&list)[pos].group              = NULL;
   (*&list)[pos].subgroup           = NULL;
   (*&list)[pos].parent_group       = NULL;
   (*&list)[pos].values             = NULL;
   (*&list)[pos].index              = 0;
   (*&list)[pos].index_offset       = 0;
   (*&list)[pos].min                = 0.0;
   (*&list)[pos].max                = 0.0;
   (*&list)[pos].flags              = 0;
   (*&list)[pos].free_flags         = 0;
   (*&list)[pos].change_handler     = NULL;
   (*&list)[pos].read_handler       = NULL;
   (*&list)[pos].action_start       = NULL;
   (*&list)[pos].action_left        = NULL;
   (*&list)[pos].action_right       = NULL;
   (*&list)[pos].action_up          = NULL;
   (*&list)[pos].action_down        = NULL;
   (*&list)[pos].action_cancel      = NULL;
   (*&list)[pos].action_ok          = NULL;
   (*&list)[pos].action_select      = NULL;
   (*&list)[pos].get_string_representation = NULL;
   (*&list)[pos].bind_type          = 0;
   (*&list)[pos].browser_selection_type = ST_NONE;
   (*&list)[pos].step               = 0.0f;
   (*&list)[pos].rounding_fraction  = NULL;
   (*&list)[pos].enforce_minrange   = false;
   (*&list)[pos].enforce_maxrange   = false;
   (*&list)[pos].cmd_trigger.idx    = CMD_EVENT_NONE;
   (*&list)[pos].cmd_trigger.triggered = false;
   (*&list)[pos].dont_use_enum_idx_representation = false;
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
      SETTINGS_LIST_INPUT_HOTKEY,
      SETTINGS_LIST_RECORDING,
      SETTINGS_LIST_FRAME_THROTTLING,
      SETTINGS_LIST_FONT,
      SETTINGS_LIST_OVERLAY,
      SETTINGS_LIST_MENU,
      SETTINGS_LIST_MENU_FILE_BROWSER,
      SETTINGS_LIST_MULTIMEDIA,
      SETTINGS_LIST_USER_INTERFACE,
      SETTINGS_LIST_POWER_MANAGEMENT,
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
      SETTINGS_LIST_DIRECTORY,
      SETTINGS_LIST_PRIVACY,
      SETTINGS_LIST_MIDI
   };
   const char *root                     = msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);
   rarch_setting_t *list                = (rarch_setting_t*)calloc(
         list_info->size, sizeof(*list));

   if (!list)
      goto error;

   for (i = 0; i < ARRAY_SIZE(list_types); i++)
   {
      if (!setting_append_list(list_types[i], &list, list_info, root))
         goto error;
   }

   if (!(settings_list_append(&list, list_info)))
      goto error;
   menu_setting_terminate_last(list, list_info->index);
   list_info->index++;

   /* flatten this array to save ourselves some kilobytes. */
   resized_list = (rarch_setting_t*)realloc(list,
         list_info->index * sizeof(rarch_setting_t));
   if (!resized_list)
      goto error;

   list = resized_list;

   return list;

error:
   if (list)
      free(list);
   return NULL;
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
static rarch_setting_t *menu_setting_new(void)
{
   rarch_setting_t* list           = NULL;
   rarch_setting_info_t *list_info = (rarch_setting_info_t*)
      malloc(sizeof(*list_info));

   if (!list_info)
      return NULL;

   list_info->index = 0;
   list_info->size  = 32;

   list             = menu_setting_new_internal(list_info);

   menu_settings_info_list_free(list_info);

   list_info        = NULL;

   return list;
}

bool menu_setting_ctl(enum menu_setting_ctl_state state, void *data)
{
   uint64_t flags;

   switch (state)
   {
      case MENU_SETTING_CTL_IS_OF_PATH_TYPE:
         {
            bool cbs_bound           = false;
            rarch_setting_t *setting = (rarch_setting_t*)data;

            if (!setting)
               return false;

            flags                    = setting->flags;

            if (setting_get_type(setting) != ST_ACTION)
               return false;

            if (!setting->change_handler)
               return false;

            cbs_bound = (setting->action_right != NULL);
            cbs_bound = cbs_bound || setting->action_left;
            cbs_bound = cbs_bound || setting->action_select;

            if (!cbs_bound)
               return false;

            if (!(flags & SD_FLAG_BROWSER_ACTION))
               return false;
         }
         break;
      case MENU_SETTING_CTL_NEW:
         {
            rarch_setting_t **setting = (rarch_setting_t**)data;
            if (!setting)
               return false;
            *setting = menu_setting_new();
         }
         break;
      case MENU_SETTING_CTL_ACTION_RIGHT:
         {
            rarch_setting_t *setting = (rarch_setting_t*)data;
            if (!setting)
               return false;

            if (setting_handler(setting, MENU_ACTION_RIGHT) == -1)
               return false;
         }
         break;
      case MENU_SETTING_CTL_NONE:
      default:
         break;
   }

   return true;
}
