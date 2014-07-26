/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "menu_common_backend.h"
#include "../menu_navigation.h"
#include "../menu_input_line_cb.h"

#include "../../../gfx/gfx_common.h"
#include "../../../driver.h"
#include "../../../file_ext.h"
#include "../../../input/input_common.h"
#include "../../../config.def.h"
#include "../../../input/keyboard_line.h"

#include "../../../settings_data.h"

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif
#endif

#ifdef HAVE_SHADER_MANAGER
static inline struct gfx_shader *shader_manager_get_current_shader(menu_handle_t *menu, unsigned type)
{
   struct gfx_shader *shader = NULL;

   if (type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
      shader = menu->shader;

   if (!shader && driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      shader = driver.video_poke->get_current_shader(driver.video_data);

   return shader;
}
#endif

static void menu_common_entries_init(void *data, unsigned menu_type)
{
   unsigned i, last;
   char tmp[256];
   menu_handle_t *menu = (menu_handle_t*)data;
   rarch_setting_t *setting_data, *current_setting;

   setting_data = (rarch_setting_t *)setting_data_get_list();

   if (!menu || !setting_data)
      return;

   switch (menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case MENU_SETTINGS_SHADER_PARAMETERS:
      case MENU_SETTINGS_SHADER_PRESET_PARAMETERS:
      {
         file_list_clear(menu->selection_buf);

         struct gfx_shader *shader = (struct gfx_shader*)shader_manager_get_current_shader(menu, menu_type);
         if (shader)
            for (i = 0; i < shader->num_parameters; i++)
               file_list_push(menu->selection_buf, shader->parameters[i].desc, MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
         menu->parameter_shader = shader;
         break;
      }
      case MENU_SETTINGS_SHADER_OPTIONS:
      {
         struct gfx_shader *shader = menu->shader;

         if (!shader)
            return;

         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Apply Shader Changes",
               MENU_SETTINGS_SHADER_APPLY, 0);
         file_list_push(menu->selection_buf, "Default Filter", MENU_SETTINGS_SHADER_FILTER, 0);
         file_list_push(menu->selection_buf, "Load Shader Preset",
               MENU_SETTINGS_SHADER_PRESET, 0);
         file_list_push(menu->selection_buf, "Save As Shader Preset",
               MENU_SETTINGS_SHADER_PRESET_SAVE, 0);
         file_list_push(menu->selection_buf, "Parameters (Current)",
               MENU_SETTINGS_SHADER_PARAMETERS, 0);
         file_list_push(menu->selection_buf, "Parameters (Menu)",
               MENU_SETTINGS_SHADER_PRESET_PARAMETERS, 0);
         file_list_push(menu->selection_buf, "Shader Passes",
               MENU_SETTINGS_SHADER_PASSES, 0);

         for (i = 0; i < shader->passes; i++)
         {
            char buf[64];

            snprintf(buf, sizeof(buf), "Shader #%u", i);
            file_list_push(menu->selection_buf, buf,
                  MENU_SETTINGS_SHADER_0 + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
            file_list_push(menu->selection_buf, buf,
                  MENU_SETTINGS_SHADER_0_FILTER + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
            file_list_push(menu->selection_buf, buf,
                  MENU_SETTINGS_SHADER_0_SCALE + 3 * i, 0);
         }
      }
      break;
#endif
      case MENU_SETTINGS_GENERAL_OPTIONS:
         file_list_clear(menu->selection_buf);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "libretro_log_level")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_LIBRETRO_LOG_LEVEL, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "log_verbosity")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_LOGGING_VERBOSITY, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "perfcnt_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PERFORMANCE_COUNTERS_ENABLE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "game_history_size")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_CONTENT_HISTORY_SIZE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "config_save_on_exit")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_CONFIG_SAVE_ON_EXIT, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "core_specific_config")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PER_CORE_CONFIG, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_gpu_screenshot")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_GPU_SCREENSHOT, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "dummy_on_core_shutdown")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "fps_show")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_DEBUG_TEXT, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "fastforward_ratio")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_FASTFORWARD_RATIO, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "slowmotion_ratio")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_SLOWMOTION_RATIO, 0);

         if ((current_setting = setting_data_find_setting(setting_data, "rewind_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_REWIND_ENABLE, 0);

         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "rewind_granularity")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_REWIND_GRANULARITY, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "block_sram_overwrite")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_BLOCK_SRAM_OVERWRITE, 0);
#ifdef HAVE_THREADS
         if ((current_setting = setting_data_find_setting(setting_data, "autosave_interval")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_SRAM_AUTOSAVE, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_disable_composition")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_WINDOW_COMPOSITING_ENABLE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "pause_nonactive")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "savestate_auto_save")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_SAVESTATE_AUTO_SAVE, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "savestate_auto_load")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_SAVESTATE_AUTO_LOAD, 0);
         break;
      case MENU_SETTINGS_VIDEO_OPTIONS:
         file_list_clear(menu->selection_buf);
         if ((current_setting = setting_data_find_setting(setting_data, "video_shared_context")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_HW_SHARED_CONTEXT, 0);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         file_list_push(menu->selection_buf, "Screen Resolution", MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         file_list_push(menu->selection_buf, "Software Filter", MENU_SETTINGS_VIDEO_SOFTFILTER, 0);
#if defined(__CELLOS_LV2__)
         file_list_push(menu->selection_buf, "PAL60 Mode", MENU_SETTINGS_VIDEO_PAL60, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_smooth")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_FILTER, 0);
#ifdef HW_RVL
         file_list_push(menu->selection_buf, "VI Trap filtering", MENU_SETTINGS_VIDEO_SOFT_FILTER, 0);
#endif
#if defined(HW_RVL) || defined(_XBOX360)
         file_list_push(menu->selection_buf, "Gamma", MENU_SETTINGS_VIDEO_GAMMA, 0);
#endif
#ifdef _XBOX1
         file_list_push(menu->selection_buf, "Soft filtering", MENU_SETTINGS_SOFT_DISPLAY_FILTER, 0);
         file_list_push(menu->selection_buf, "Flicker filtering", MENU_SETTINGS_FLICKER_FILTER, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_scale_integer")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_INTEGER_SCALE, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "aspect_ratio_index")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_ASPECT_RATIO, 0);
         file_list_push(menu->selection_buf, "Custom Ratio", MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         if ((current_setting = setting_data_find_setting(setting_data, "video_fullscreen")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_TOGGLE_FULLSCREEN, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_rotation")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_ROTATION, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "video_vsync")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_VSYNC, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "video_hard_sync")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_HARD_SYNC, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "video_hard_sync_frames")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_HARD_SYNC_FRAMES, 0);

#if !defined(RARCH_MOBILE)
         if ((current_setting = setting_data_find_setting(setting_data, "video_black_frame_insertion")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_BLACK_FRAME_INSERTION, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_swap_interval")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_SWAP_INTERVAL, 0);
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         if ((current_setting = setting_data_find_setting(setting_data, "video_threaded")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_THREADED, 0);
#endif
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         if ((current_setting = setting_data_find_setting(setting_data, "video_xscale")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_WINDOW_SCALE_X, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "video_yscale")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_WINDOW_SCALE_Y, 0);
#endif
         if ((current_setting = setting_data_find_setting(setting_data, "video_crop_overscan")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_CROP_OVERSCAN, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "video_monitor_index")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_VIDEO_MONITOR_INDEX, 0);
         file_list_push(menu->selection_buf, "Estimated Monitor FPS", MENU_SETTINGS_VIDEO_REFRESH_RATE_AUTO, 0);
         break;
      case MENU_SETTINGS_FONT_OPTIONS:
         file_list_clear(menu->selection_buf);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_font_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_FONT_ENABLE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_font_size")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_FONT_SIZE, 0);
         break;
      case MENU_SETTINGS_CORE_OPTIONS:
         file_list_clear(menu->selection_buf);

         if (g_extern.system.core_options)
         {
            size_t i, opts;

            opts = core_option_size(g_extern.system.core_options);
            for (i = 0; i < opts; i++)
               file_list_push(menu->selection_buf,
                     core_option_get_desc(g_extern.system.core_options, i), MENU_SETTINGS_CORE_OPTION_START + i, 0);
         }
         else
            file_list_push(menu->selection_buf, "No options available.", MENU_SETTINGS_CORE_OPTION_NONE, 0);
         break;
      case MENU_SETTINGS_CORE_INFO:
         {
            core_info_t *info = menu->core_info_current;
            file_list_clear(menu->selection_buf);

            if (info->data)
            {
               snprintf(tmp, sizeof(tmp), "Core name: %s",
                     info->display_name ? info->display_name : "");
               file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);

               if (info->authors_list)
               {
                  strlcpy(tmp, "Authors: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp), info->authors_list, ", ");
                  file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->permissions_list)
               {
                  strlcpy(tmp, "Permissions: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp), info->permissions_list, ", ");
                  file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->supported_extensions_list)
               {
                  strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp), info->supported_extensions_list, ", ");
                  file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->firmware_count > 0)
               {
                  core_info_list_update_missing_firmware(menu->core_info, info->path,
                        g_settings.system_directory);

                  file_list_push(menu->selection_buf, "Firmware: ", MENU_SETTINGS_CORE_INFO_NONE, 0);
                  for (i = 0; i < info->firmware_count; i++)
                  {
                     if (info->firmware[i].desc)
                     {
                        snprintf(tmp, sizeof(tmp), "	name: %s",
                              info->firmware[i].desc ? info->firmware[i].desc : "");
                        file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);

                        snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                              info->firmware[i].missing ? "missing" : "present",
                              info->firmware[i].optional ? "optional" : "required");
                        file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);
                     }
                  }
               }

               if (info->notes)
               {
                  snprintf(tmp, sizeof(tmp), "Core notes: ");
                  file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);

                  for (i = 0; i < info->note_list->size; i++)
                  {
                     snprintf(tmp, sizeof(tmp), " %s", info->note_list->elems[i].data);
                     file_list_push(menu->selection_buf, tmp, MENU_SETTINGS_CORE_INFO_NONE, 0);
                  }
               }
            }
            else
               file_list_push(menu->selection_buf, "No information available.", MENU_SETTINGS_CORE_OPTION_NONE, 0);
         }
         break;
      case MENU_SETTINGS_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Driver Options", MENU_SETTINGS_DRIVERS, 0);
         file_list_push(menu->selection_buf, "General Options", MENU_SETTINGS_GENERAL_OPTIONS, 0);
         file_list_push(menu->selection_buf, "Video Options", MENU_SETTINGS_VIDEO_OPTIONS, 0);
#ifdef HAVE_SHADER_MANAGER
         file_list_push(menu->selection_buf, "Shader Options", MENU_SETTINGS_SHADER_OPTIONS, 0);
#endif
         file_list_push(menu->selection_buf, "Font Options", MENU_SETTINGS_FONT_OPTIONS, 0);
         file_list_push(menu->selection_buf, "Audio Options", MENU_SETTINGS_AUDIO_OPTIONS, 0);
         file_list_push(menu->selection_buf, "Input Options", MENU_SETTINGS_INPUT_OPTIONS, 0);
#ifdef HAVE_OVERLAY
         file_list_push(menu->selection_buf, "Overlay Options", MENU_SETTINGS_OVERLAY_OPTIONS, 0);
#endif
#ifdef HAVE_NETPLAY
         file_list_push(menu->selection_buf, "Netplay Options", MENU_SETTINGS_NETPLAY_OPTIONS, 0);
#endif
         file_list_push(menu->selection_buf, "Path Options", MENU_SETTINGS_PATH_OPTIONS, 0);
         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            if (g_extern.system.disk_control.get_num_images)
               file_list_push(menu->selection_buf, "Disk Options", MENU_SETTINGS_DISK_OPTIONS, 0);
         }
         file_list_push(menu->selection_buf, "Privacy Options", MENU_SETTINGS_PRIVACY_OPTIONS, 0);
         break;
      case MENU_SETTINGS_PRIVACY_OPTIONS:
         file_list_clear(menu->selection_buf);
#ifdef HAVE_CAMERA
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "camera_allow")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PRIVACY_CAMERA_ALLOW, 0);
#endif
#ifdef HAVE_LOCATION
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "location_allow")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PRIVACY_LOCATION_ALLOW, 0);
#endif
         break;
      case MENU_SETTINGS_DISK_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Disk Index", MENU_SETTINGS_DISK_INDEX, 0);
         file_list_push(menu->selection_buf, "Disk Image Append", MENU_SETTINGS_DISK_APPEND, 0);
         break;
      case MENU_SETTINGS_OVERLAY_OPTIONS:
         file_list_clear(menu->selection_buf);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "input_overlay")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_OVERLAY_PRESET, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "input_overlay_opacity")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_OVERLAY_OPACITY, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "input_overlay_scale")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_OVERLAY_SCALE, 0);
         break;
#ifdef HAVE_NETPLAY
      case MENU_SETTINGS_NETPLAY_OPTIONS:
         file_list_clear(menu->selection_buf);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_NETPLAY_ENABLE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_mode")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_NETPLAY_MODE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_spectator_mode_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE, 0);
         file_list_push(menu->selection_buf, "Host IP Address", MENU_SETTINGS_NETPLAY_HOST_IP_ADDRESS, 0);
         file_list_push(menu->selection_buf, "TCP/UDP Port", MENU_SETTINGS_NETPLAY_TCP_UDP_PORT, 0);
         file_list_push(menu->selection_buf, "Delay Frames", MENU_SETTINGS_NETPLAY_DELAY_FRAMES, 0);
         file_list_push(menu->selection_buf, "Nickname", MENU_SETTINGS_NETPLAY_NICKNAME, 0);
         break;
#endif
      case MENU_SETTINGS_PATH_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Browser Directory", MENU_BROWSER_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Content Directory", MENU_CONTENT_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Assets Directory", MENU_ASSETS_DIR_PATH, 0);
#ifdef HAVE_DYNAMIC
         file_list_push(menu->selection_buf, "Config Directory", MENU_CONFIG_DIR_PATH, 0);
#endif
         file_list_push(menu->selection_buf, "Core Directory", MENU_LIBRETRO_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Core Info Directory", MENU_LIBRETRO_INFO_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Content History Path", MENU_SETTINGS_CONTENT_HISTORY_PATH, 0);
#ifdef HAVE_DYLIB
         file_list_push(menu->selection_buf, "Software Filter Directory", MENU_FILTER_DIR_PATH, 0);
#endif
         file_list_push(menu->selection_buf, "DSP Filter Directory", MENU_DSP_FILTER_DIR_PATH, 0);
#ifdef HAVE_SHADER_MANAGER
         file_list_push(menu->selection_buf, "Shader Directory", MENU_SHADER_DIR_PATH, 0);
#endif
         file_list_push(menu->selection_buf, "Savestate Directory", MENU_SAVESTATE_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Savefile Directory", MENU_SAVEFILE_DIR_PATH, 0);
#ifdef HAVE_OVERLAY
         file_list_push(menu->selection_buf, "Overlay Directory", MENU_OVERLAY_DIR_PATH, 0);
#endif
         file_list_push(menu->selection_buf, "System Directory", MENU_SYSTEM_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Screenshot Directory", MENU_SCREENSHOT_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Autoconfig Directory", MENU_AUTOCONFIG_DIR_PATH, 0);
         file_list_push(menu->selection_buf, "Extraction Directory", MENU_EXTRACTION_DIR_PATH, 0);
         break;
      case MENU_SETTINGS_INPUT_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Player", MENU_SETTINGS_BIND_PLAYER, 0);
         file_list_push(menu->selection_buf, "Device", MENU_SETTINGS_BIND_DEVICE, 0);
         file_list_push(menu->selection_buf, "Device Type", MENU_SETTINGS_BIND_DEVICE_TYPE, 0);
         file_list_push(menu->selection_buf, "Analog D-pad Mode", MENU_SETTINGS_BIND_ANALOG_MODE, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "input_axis_threshold")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_INPUT_AXIS_THRESHOLD, 0);
         if ((current_setting = setting_data_find_setting(setting_data, "input_autodetect_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_DEVICE_AUTODETECT_ENABLE, 0);

         file_list_push(menu->selection_buf, "Bind Mode", MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
         file_list_push(menu->selection_buf, "Configure All (RetroPad)", MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
         file_list_push(menu->selection_buf, "Default All (RetroPad)", MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
#ifdef HAVE_OSK
         if ((current_setting = setting_data_find_setting(setting_data, "osk_enable")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_ONSCREEN_KEYBOARD_ENABLE, 0);
#endif
         last = (driver.input && driver.input->set_keybinds && !driver.input->get_joypad_driver) ? (MENU_SETTINGS_BIND_BEGIN + RETRO_DEVICE_ID_JOYPAD_R3) : MENU_SETTINGS_BIND_ALL_LAST;
         for (i = MENU_SETTINGS_BIND_BEGIN; i <= last; i++)
            file_list_push(menu->selection_buf, input_config_bind_map[i - MENU_SETTINGS_BIND_BEGIN].desc, i, 0);
         break;
      case MENU_SETTINGS_AUDIO_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "DSP Filter", MENU_SETTINGS_AUDIO_DSP_FILTER, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_mute")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_AUDIO_MUTE, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_latency")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_AUDIO_LATENCY, 0);
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_sync")))
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_AUDIO_SYNC, 0);
         file_list_push(menu->selection_buf, "Rate Control Delta", MENU_SETTINGS_AUDIO_CONTROL_RATE_DELTA, 0);
#ifdef __CELLOS_LV2__
         file_list_push(menu->selection_buf, "System BGM Control", MENU_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE, 0);
#endif
         file_list_push(menu->selection_buf, "Volume Level", MENU_SETTINGS_AUDIO_VOLUME, 0);
         file_list_push(menu->selection_buf, "Audio Device", MENU_SETTINGS_DRIVER_AUDIO_DEVICE, 0);
         break;
      case MENU_SETTINGS_DRIVERS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Video Driver", MENU_SETTINGS_DRIVER_VIDEO, 0);
         file_list_push(menu->selection_buf, "Audio Driver", MENU_SETTINGS_DRIVER_AUDIO, 0);
         file_list_push(menu->selection_buf, "Audio Resampler", MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER, 0);
         file_list_push(menu->selection_buf, "Input Driver", MENU_SETTINGS_DRIVER_INPUT, 0);
#ifdef HAVE_CAMERA
         file_list_push(menu->selection_buf, "Camera Driver", MENU_SETTINGS_DRIVER_CAMERA, 0);
#endif
#ifdef HAVE_LOCATION
         file_list_push(menu->selection_buf, "Location Driver", MENU_SETTINGS_DRIVER_LOCATION, 0);
#endif
#ifdef HAVE_MENU
         file_list_push(menu->selection_buf, "Menu Driver", MENU_SETTINGS_DRIVER_MENU, 0);
#endif
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Frontend Counters", MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND, 0);
         file_list_push(menu->selection_buf, "Core Counters", MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO, 0);
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO:
         file_list_clear(menu->selection_buf);
         {
            const struct retro_perf_counter **counters = (const struct retro_perf_counter**)perf_counters_libretro;
            unsigned num = perf_ptr_libretro;

            if (!counters || num == 0)
               break;

            for (i = 0; i < num; i++)
               if (counters[i] && counters[i]->ident)
                  file_list_push(menu->selection_buf, counters[i]->ident, MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN + i, 0);
         }
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND:
         file_list_clear(menu->selection_buf);
         {
            const struct retro_perf_counter **counters = (const struct retro_perf_counter**)perf_counters_rarch;
            unsigned num = perf_ptr_rarch;

            if (!counters || num == 0)
               break;

            for (i = 0; i < num; i++)
               if (counters[i] && counters[i]->ident)
                  file_list_push(menu->selection_buf, counters[i]->ident, MENU_SETTINGS_PERF_COUNTERS_BEGIN + i, 0);
         }
         break;
      case MENU_SETTINGS:
         file_list_clear(menu->selection_buf);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
         file_list_push(menu->selection_buf, "Core", MENU_SETTINGS_CORE, 0);
#endif
         if (g_extern.history)
            file_list_push(menu->selection_buf, "Load Content (History)", MENU_SETTINGS_OPEN_HISTORY, 0);

         if (menu->core_info && core_info_list_num_info_files(menu->core_info))
            file_list_push(menu->selection_buf, "Load Content (Detect Core)", MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, 0);

         if (menu->info.library_name || g_extern.system.info.library_name)
         {
            char load_game_core_msg[64];
            snprintf(load_game_core_msg, sizeof(load_game_core_msg), "Load Content (%s)",
                  menu->info.library_name ? menu->info.library_name : g_extern.system.info.library_name);
            file_list_push(menu->selection_buf, load_game_core_msg, MENU_SETTINGS_OPEN_FILEBROWSER, 0);
         }

         file_list_push(menu->selection_buf, "Core Options", MENU_SETTINGS_CORE_OPTIONS, 0);
         file_list_push(menu->selection_buf, "Core Information", MENU_SETTINGS_CORE_INFO, 0);
         file_list_push(menu->selection_buf, "Settings", MENU_SETTINGS_OPTIONS, 0);

         if (g_extern.perfcnt_enable && (current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "perfcnt_enable")))
         {
            *current_setting->value.boolean = g_extern.perfcnt_enable;
            file_list_push(menu->selection_buf, current_setting->short_description, MENU_SETTINGS_PERFORMANCE_COUNTERS, 0);
         }

         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            file_list_push(menu->selection_buf, "Save State", MENU_SETTINGS_SAVESTATE_SAVE, 0);
            file_list_push(menu->selection_buf, "Load State", MENU_SETTINGS_SAVESTATE_LOAD, 0);
            file_list_push(menu->selection_buf, "Take Screenshot", MENU_SETTINGS_SCREENSHOT, 0);
            file_list_push(menu->selection_buf, "Resume Content", MENU_SETTINGS_RESUME_GAME, 0);
            file_list_push(menu->selection_buf, "Restart Content", MENU_SETTINGS_RESTART_GAME, 0);

         }
#ifndef HAVE_DYNAMIC
         file_list_push(menu->selection_buf, "Restart RetroArch", MENU_SETTINGS_RESTART_EMULATOR, 0);
#endif
         file_list_push(menu->selection_buf, "RetroArch Config", MENU_SETTINGS_CONFIG, 0);
         file_list_push(menu->selection_buf, "Save New Config", MENU_SETTINGS_SAVE_CONFIG, 0);
         file_list_push(menu->selection_buf, "Help", MENU_START_SCREEN, 0);
         file_list_push(menu->selection_buf, "Quit RetroArch", MENU_SETTINGS_QUIT_RARCH, 0);
         break;
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, menu_type);
}

static int menu_info_screen_iterate(unsigned action)
{
   char msg[1024];

   if (!driver.menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   switch (driver.menu->info_selection)
   {
      case MENU_SETTINGS_WINDOW_COMPOSITING_ENABLE:
         snprintf(msg, sizeof(msg),
               "-- Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_SETTINGS_LIBRETRO_LOG_LEVEL:
         snprintf(msg, sizeof(msg),
               "-- Sets log level for libretro cores \n"
               "(GET_LOG_INTERFACE). \n"
               " \n"
               " If a log level issued by a libretro \n"
               " core is below libretro_log level, it \n"
               " is ignored.\n"
               " \n"
               " DEBUG logs are always ignored unless \n"
               " verbose mode is activated (--verbose).\n"
               " \n"
               " DEBUG = 0\n"
               " INFO  = 1\n"
               " WARN  = 2\n"
               " ERROR = 3"
               );
         break;
      case MENU_SETTINGS_LOGGING_VERBOSITY:
         snprintf(msg, sizeof(msg),
               "-- Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS_ENABLE:
         snprintf(msg, sizeof(msg),
               "-- Enable or disable frontend \n"
               "performance counters.");
         break;
      case MENU_SYSTEM_DIR_PATH:
         snprintf(msg, sizeof(msg),
               "-- System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Implementations can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_START_SCREEN:
         snprintf(msg, sizeof(msg),
               " -- Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Config Save On Exit' is set to true.\n");
         break;
      case MENU_SETTINGS_CONFIG_SAVE_ON_EXIT:
         snprintf(msg, sizeof(msg),
               " -- Flushes config to disk on exit.\n"
               "Useful for menu as settings can be\n"
               "modified. Overwrites the config.\n"
               " \n"
               "#include's and comments are not \n"
               "preserved. \n"
               " \n"
               "By design, the config file is \n"
               "considered immutable as it is \n"
               "likely maintained by the user, \n"
               "and should not be overwritten \n"
               "behind the user's back."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nThis is not not the case on \n"
               "consoles however, where \n"
               "looking at the config file \n"
               "manually isn't really an option."
#endif
               );
         break;
      case MENU_SETTINGS_OPEN_FILEBROWSER:
         snprintf(msg, sizeof(msg),
               " -- Load Content. \n"
               "Browse for content. \n"
               " \n"
               "To load content, you need a \n"
               "libretro core to use, and a \n"
               "content file. \n"
               " \n"
               "To control where the menu starts \n"
               " to browse for content, set  \n"
               "Browser Directory. If not set,  \n"
               "it will start in root. \n"
               " \n"
               "The browser will filter out \n"
               "extensions for the last core set \n"
               "in 'Core', and use that core when \n"
               "content is loaded."
               );
         break;
      case MENU_SETTINGS_PER_CORE_CONFIG:
         snprintf(msg, sizeof(msg),
               " -- Load up a specific config file \n"
               "based on the core being used.\n");
         break;
      case MENU_SETTINGS_VIDEO_WINDOW_SCALE_X:
      case MENU_SETTINGS_VIDEO_WINDOW_SCALE_Y:
         snprintf(msg, sizeof(msg),
               " -- Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_SETTINGS_VIDEO_VSYNC:
         snprintf(msg, sizeof(msg),
               " -- Video V-Sync.\n");
         break;
      case MENU_SETTINGS_VIDEO_HARD_SYNC:
         snprintf(msg, sizeof(msg),
               " -- Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
         break;
      case MENU_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         snprintf(msg, sizeof(msg),
               " -- Sets how many frames CPU can \n"
               "run ahead of GPU when using 'GPU \n"
               "Hard Sync'.\n"
               " \n"
               "Maximum is 3.\n"
               " \n"
               " 0: Syncs to GPU immediately.\n"
               " 1: Syncs to previous frame.\n"
               " 2: Etc ...");
         break;
      case MENU_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(msg, sizeof(msg),
               " -- Inserts a black frame inbetween \n"
               "frames.\n"
               " \n"
               "Useful for 120 Hz monitors who want to \n"
               "play 60 Hz material with eliminated \n"
               "ghosting.\n"
               " \n"
               "Video refresh rate should still be \n"
               "configured as if it is a 60 Hz monitor \n"
               "(divide refresh rate by 2).");
         break;
      case MENU_SETTINGS_VIDEO_THREADED:
         snprintf(msg, sizeof(msg),
               " -- Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case MENU_SETTINGS_VIDEO_INTEGER_SCALE:
         snprintf(msg, sizeof(msg),
               " -- Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
         break;
      case MENU_SETTINGS_VIDEO_CROP_OVERSCAN:
         snprintf(msg, sizeof(msg),
               " -- Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
         break;
      case MENU_SETTINGS_VIDEO_MONITOR_INDEX:
         snprintf(msg, sizeof(msg),
               " -- Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case MENU_SETTINGS_VIDEO_ROTATION:
         snprintf(msg, sizeof(msg),
               " -- Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case MENU_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         snprintf(msg, sizeof(msg),
               " -- Audio rate control.\n"
               " \n"
               "Setting this to 0 disables rate control.\n"
               "Any other value controls audio rate control \n"
               "delta.\n"
               " \n"
               "Defines how much input rate can be adjusted \n"
               "dynamically.\n"
               " \n"
               " Input rate is defined as: \n"
               " input rate * (1.0 +/- (rate control delta))");
         break;
      case MENU_SETTINGS_AUDIO_VOLUME:
         snprintf(msg, sizeof(msg),
               " -- Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
         break;
      case MENU_SETTINGS_VIDEO_SOFTFILTER:
#ifdef HAVE_FILTERS_BUILTIN
         snprintf(msg, sizeof(msg),
               " -- CPU-based video filter.");
#else
         snprintf(msg, sizeof(msg),
               " -- CPU-based video filter.\n"
               " \n"
               "Path to a dynamic library.");
#endif
         break;
      case MENU_SETTINGS_BLOCK_SRAM_OVERWRITE:
         snprintf(msg, sizeof(msg),
               " -- Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case MENU_SETTINGS_PRIVACY_CAMERA_ALLOW:
         snprintf(msg, sizeof(msg),
               " -- Allow or disallow camera access by \n"
               "cores.");
         break;
      case MENU_SETTINGS_PRIVACY_LOCATION_ALLOW:
         snprintf(msg, sizeof(msg),
               " -- Allow or disallow location services \n"
               "access by cores.");
         break;
      case MENU_SETTINGS_REWIND_ENABLE:
         snprintf(msg, sizeof(msg),
               " -- Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case MENU_SETTINGS_REWIND_GRANULARITY:
         snprintf(msg, sizeof(msg),
               " -- Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case MENU_SETTINGS_DEVICE_AUTODETECT_ENABLE:
         snprintf(msg, sizeof(msg),
               " -- Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case MENU_SETTINGS_INPUT_AXIS_THRESHOLD:
         snprintf(msg, sizeof(msg),
               " -- Defines axis threshold.\n"
               " \n"
               " Possible values are [0.0, 1.0].");
         break;
      case MENU_SETTINGS_CORE:
         snprintf(msg, sizeof(msg),
               " -- Core Selection. \n"
               " \n"
               "Browse for a libretro core \n"
               "implementation. Where the browser \n"
               "starts depends on your Core Directory \n"
               "path. If blank, it will start in root. \n"
               " \n"
               "If Core Directory is a directory, the menu \n"
               "will use that as top folder. If Core \n"
               "Directory is a full path, it will start \n"
               "in the folder where the file is.");
         break;
      case MENU_SETTINGS_OPEN_HISTORY:
         snprintf(msg, sizeof(msg),
               " -- Loading content from history. \n"
               " \n"
               "As content is loaded, content and libretro \n"
               "core combinations are saved to history. \n"
               " \n"
               "The history is saved to a file in the same \n"
               "directory as the RetroArch config file. If \n"
               "no config file was loaded in startup, history \n"
               "will not be saved or loaded, and will not exist \n"
               "in the main menu."
               );
         break;
      case MENU_SETTINGS_SHADER_PRESET:
         snprintf(msg, sizeof(msg),
               " -- Load Shader Preset. \n"
               " \n"
               " Load a "
#ifdef HAVE_CG
               "Cg"
#endif
#ifdef HAVE_GLSL
#ifdef HAVE_CG
               "/"
#endif
               "GLSL"
#endif
#ifdef HAVE_HLSL
#if defined(HAVE_CG) || defined(HAVE_HLSL)
               "/"
#endif
               "HLSL"
#endif
               " preset directly. \n"
               "The menu shader menu is updated accordingly. \n"
               " \n"
               "If the CGP uses scaling methods which are not \n"
               "simple, (i.e. source scaling, same scaling \n"
               "factor for X/Y), the scaling factor displayed \n"
               "in the menu might not be correct."
               );
         break;
      case MENU_SETTINGS_SHADER_APPLY:
         snprintf(msg, sizeof(msg),
               " -- Apply Shader Changes. \n"
               " \n"
               "After changing shader settings, use this to \n"
               "apply changes. \n"
               " \n"
               "Changing shader settings is a somewhat \n"
               "expensive operation so it has to be \n"
               "done explicitly. \n"
               " \n"
               "When you apply shaders, the menu shader \n"
               "settings are saved to a temporary file (either \n"
               "menu.cgp or menu.glslp) and loaded. The file \n"
               "persists after RetroArch exits. The file is \n"
               "saved to Shader Directory."
               );
         break;
      case MENU_SETTINGS_SHADER_PASSES:
         snprintf(msg, sizeof(msg),
               " -- Shader Passes. \n"
               " \n"
               "RetroArch allows you to mix and match various \n"
               "shaders with arbitrary shader passes, with \n"
               "custom hardware filters and scale factors. \n"
               " \n"
               "This option specifies the number of shader \n"
               "passes to use. If you set this to 0, and use \n"
               "Apply Shader Changes, you use a 'blank' shader. \n"
               " \n"
               "The Default Filter option will affect the \n"
               "stretching filter.");
         break;
      case MENU_SETTINGS_BIND_DEVICE:
         snprintf(msg, sizeof(msg),
               " -- Input Device. \n"
               " \n"
               "Picks which gamepad to use for player N. \n"
               "The name of the pad is available."
               );
         break;
      case MENU_SETTINGS_BIND_DEVICE_TYPE:
         snprintf(msg, sizeof(msg),
               " -- Input Device Type. \n"
               " \n"
               "Picks which device type to use. This is \n"
               "relevant for the libretro core itself."
               );
         break;
      case MENU_SETTINGS_DRIVER_INPUT:
         if (!strcmp(g_settings.input.driver, "udev"))
            snprintf(msg, sizeof(msg),
                  " -- udev Input driver. \n"
                  " \n"
                  "This driver can run without X. \n"
                  " \n"
                  "It uses the recent evdev joypad API \n"
                  "for joystick support. It supports \n"
                  "hotplugging and force feedback (if \n"
                  "supported by device). \n"
                  " \n"
                  "The driver reads evdev events for keyboard \n"
                  "support. It also supports keyboard callback, \n"
                  "mice and touchpads. \n"
                  " \n"
                  "By default in most distros, /dev/input nodes \n"
                  "are root-only (mode 600). You can set up a udev \n"
                  "rule which makes these accessible to non-root."
                  );
         else if (!strcmp(g_settings.input.driver, "linuxraw"))
            snprintf(msg, sizeof(msg),
                  " -- linuxraw Input driver. \n"
                  " \n"
                  "This driver requires an active TTY. Keyboard \n"
                  "events are read directly from the TTY which \n"
                  "makes it simpler, but not as flexible as udev. \n"
                  "Mice, etc, are not supported at all. \n"
                  " \n"
                  "This driver uses the older joystick API \n"
                  "(/dev/input/js*).");
         else
            snprintf(msg, sizeof(msg),
                  " -- Input driver.\n"
                  " \n"
                  "Depending on video driver, it might \n"
                  "force a different input driver.");
         break;
      case MENU_SETTINGS_AUDIO_DSP_FILTER:
         snprintf(msg, sizeof(msg),
               " -- Audio DSP plugin.\n"
               " Processes audio before it's sent to \n"
               "the driver."
#ifndef HAVE_FILTERS_BUILTIN
               " \n"
               "Path to a dynamic library."
#endif
               );
         break;
      case MENU_SETTINGS_TOGGLE_FULLSCREEN:
         snprintf(msg, sizeof(msg),
               " -- Toggles fullscreen.");
         break;
      case MENU_SETTINGS_SLOWMOTION_RATIO:
         snprintf(msg, sizeof(msg),
               " -- Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case MENU_SETTINGS_FASTFORWARD_RATIO:
         snprintf(msg, sizeof(msg),
               " -- Fastforward ratio."
               " \n"
               "The maximum rate at which content will\n"
               "be run when using fast forward.\n"
               " \n"
               " (E.g. 5.0 for 60 fps content => 300 fps \n"
               "cap).\n"
               " \n"
               "RetroArch will go to sleep to ensure that \n"
               "the maximum rate will not be exceeded.\n"
               "Do not rely on this cap to be perfectly \n"
               "accurate.");
         break;
      case MENU_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
         snprintf(msg, sizeof(msg),
               " -- Pause gameplay when window focus \n"
               "is lost.");
         break;
      case MENU_SETTINGS_GPU_SCREENSHOT:
         snprintf(msg, sizeof(msg),
               " -- Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case MENU_SETTINGS_SRAM_AUTOSAVE:
         snprintf(msg, sizeof(msg),
               " -- Autosaves the non-volatile SRAM \n"
               "at a regular interval.\n"
               " \n"
               "This is disabled by default unless set \n"
               "otherwise. The interval is measured in \n"
               "seconds. \n"
               " \n"
               "A value of 0 disables autosave.");
         break;
      case MENU_SCREENSHOT_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
         break;
      case MENU_SETTINGS_DRIVER_AUDIO_DEVICE:
         snprintf(msg, sizeof(msg),
               " -- Override the default audio device \n"
               "the audio driver uses.\n"
               "This is driver dependent. E.g.\n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA wants a PCM device."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS wants a path (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK wants portnames (e.g. system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound wants an IP address to an RSound \n"
               "server."
#endif
               );
         break;
      case MENU_ASSETS_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Assets Directory. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_SETTINGS_SAVESTATE_AUTO_SAVE:
         snprintf(msg, sizeof(msg),
               " -- Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Savestate Auto\n"
               "Load' is set.");
         break;
      case MENU_SETTINGS_VIDEO_SWAP_INTERVAL:
         snprintf(msg, sizeof(msg),
               " -- VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case MENU_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
         snprintf(msg, sizeof(msg),
               " -- Refresh Rate Auto.\n"
               " \n"
               "The accurate refresh rate of our monitor (Hz).\n"
               "This is used to calculate audio input rate with \n"
               "the formula: \n"
               " \n"
               "audio_input_rate = game input rate * display \n"
               "refresh rate / game refresh rate\n"
               " \n"
               "If the implementation does not report any \n"
               "values, NTSC defaults will be assumed for \n"
               "compatibility.\n"
               " \n"
               "This value should stay close to 60Hz to avoid \n"
               "large pitch changes. If your monitor does \n"
               "not run at 60Hz, or something close to it, \n"
               "disable VSync, and leave this at its default.");
         break;
      case MENU_LIBRETRO_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Core Directory. \n"
               " \n"
               "A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case MENU_SAVEFILE_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Savefile Directory. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_SAVESTATE_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_LEFT_X_PLUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_LEFT_X_MINUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_LEFT_Y_PLUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_LEFT_Y_MINUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_X_PLUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_X_MINUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_Y_PLUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_Y_MINUS:
         snprintf(msg, sizeof(msg),
               " -- Axis for analog stick (DualShock-esque).\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_SHADER_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Applies next shader in directory.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_SHADER_PREV:
         snprintf(msg, sizeof(msg),
               " -- Applies previous shader in directory.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_LOAD_STATE_KEY:
         snprintf(msg, sizeof(msg),
               " -- Loads state.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_SAVE_STATE_KEY:
         snprintf(msg, sizeof(msg),
               " -- Saves state.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_STATE_SLOT_PLUS:
      case MENU_SETTINGS_BIND_BEGIN + RARCH_STATE_SLOT_MINUS:
         snprintf(msg, sizeof(msg),
               " -- State slots.\n"
               " \n"
               " With slot set to 0, save state name is *.state \n"
               " (or whatever defined on commandline).\n"
               "When slot is != 0, path will be (path)(d), \n"
               "where (d) is slot number.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_TURBO_ENABLE:
         snprintf(msg, sizeof(msg),
               " -- Turbo enable.\n"
               " \n"
               "Holding the turbo while pressing another \n"
               "button will let the button enter a turbo \n"
               "mode where the button state is modulated \n"
               "with a periodic signal. \n"
               " \n"
               "The modulation stops when the button \n"
               "itself (not turbo button) is released.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_FAST_FORWARD_HOLD_KEY:
         snprintf(msg, sizeof(msg),
               " -- Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_QUIT_KEY:
         snprintf(msg, sizeof(msg),
               " -- Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nKilling it in any hard way (SIGKILL, \n"
               "etc) will terminate without saving\n"
               "RAM, etc. On Unix-likes,\n"
               "SIGINT/SIGTERM allows\n"
               "a clean deinitialization."
#endif
               );
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_REWIND:
         snprintf(msg, sizeof(msg),
               " -- Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_MOVIE_RECORD_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle between recording and not.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_PAUSE_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle between paused and non-paused state.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_FRAMEADVANCE:
         snprintf(msg, sizeof(msg),
               " -- Frame advance when content is paused.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_RESET:
         snprintf(msg, sizeof(msg),
               " -- Reset the content.\n");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_CHEAT_INDEX_PLUS:
         snprintf(msg, sizeof(msg),
               " -- Increment cheat index.\n");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_CHEAT_INDEX_MINUS:
         snprintf(msg, sizeof(msg),
               " -- Decrement cheat index.\n");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_CHEAT_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle cheat index.\n");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_SCREENSHOT:
         snprintf(msg, sizeof(msg),
               " -- Take screenshot.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_MUTE:
         snprintf(msg, sizeof(msg),
               " -- Mute/unmute audio.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_NETPLAY_FLIP:
         snprintf(msg, sizeof(msg),
               " -- Netplay flip players.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_SLOWMOTION:
         snprintf(msg, sizeof(msg),
               " -- Hold for slowmotion.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_ENABLE_HOTKEY:
         snprintf(msg, sizeof(msg),
               " -- Enable other hotkeys.\n"
               " \n"
               " If this hotkey is bound to either keyboard, \n"
               "joybutton or joyaxis, all other hotkeys will \n"
               "be disabled unless this hotkey is also held \n"
               "at the same time. \n"
               " \n"
               "This is useful for RETRO_KEYBOARD centric \n"
               "implementations which query a large area of \n"
               "the keyboard, where it is not desirable that \n"
               "hotkeys get in the way.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_VOLUME_UP:
         snprintf(msg, sizeof(msg),
               " -- Increases audio volume.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_VOLUME_DOWN:
         snprintf(msg, sizeof(msg),
               " -- Decreases audio volume.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_OVERLAY_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Toggles to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_DISK_EJECT_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_DISK_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Cycles through disk images. Use after \n"
               "ejecting. \n"
               " \n"
               " Complete by toggling eject again.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_GRAB_MOUSE_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles mouse grab.\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
         break;
      case MENU_SETTINGS_BIND_BEGIN + RARCH_MENU_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles menu.");
         break;
      case MENU_SETTINGS_DRIVER_VIDEO:
         if (!strcmp(g_settings.video.driver, "gl"))
            snprintf(msg, sizeof(msg),
                  " -- OpenGL Video driver. \n"
                  " \n"
                  "This driver allows libretro GL cores to  \n"
                  "be used in addition to software-rendered \n"
                  "core implementations.\n"
                  " \n"
                  "Performance for software-rendered and \n"
                  "libretro GL core implementations is \n"
                  "dependent on your graphics card's \n"
                  "underlying GL driver).");
         else if (!strcmp(g_settings.video.driver, "sdl"))
            snprintf(msg, sizeof(msg),
                  " -- SDL Video driver.\n"
                  " \n"
                  "This is an SDL 1.2 software-rendered video \n"
                  "driver.\n"
                  " \n"
                  "Performance is considered to be suboptimal. \n"
                  "Consider using it only as a last resort.\n"
                  " \n"
                  "WARNING: Menu support is currently not \n"
                  "supported by this driver.");
         else if (!strcmp(g_settings.video.driver, "d3d"))
            snprintf(msg, sizeof(msg),
                  " -- Direct3D Video driver. \n"
                  " \n"
                  "Performance for software-rendered cores \n"
                  "is dependent on your graphic card's \n"
                  "underlying D3D driver).");
         else if (!strcmp(g_settings.video.driver, "lima"))
            snprintf(msg, sizeof(msg),
                  " -- Lima Video Driver. \n"
                  " \n"
                  "This is a low-level Limare video driver. \n"
                  " \n"
                  "Performance for software rendered cores \n"
                  "should be optimal.");
         else
            snprintf(msg, sizeof(msg),
                  " -- Current Video driver.");
         break;
      case MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER:
         if (!strcmp(g_settings.audio.resampler, "sinc"))
            snprintf(msg, sizeof(msg),
                  " -- Windowed SINC implementation.");
         else if (!strcmp(g_settings.audio.resampler, "CC"))
            snprintf(msg, sizeof(msg),
                  " -- Convoluted Cosine implementation.");
         break;
      case MENU_SETTINGS_SHADER_0_FILTER + (0 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (1 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (2 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (3 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (4 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (5 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (6 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (7 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (8 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (9 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (10 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (11 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (12 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (13 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (14 * 3):
      case MENU_SETTINGS_SHADER_0_FILTER + (15 * 3):
         snprintf(msg, sizeof(msg),
               " -- Hardware filter for this pass. \n"
               " \n"
               "If 'Don't Care' is set, 'Default \n"
               "Filter' will be used."
               );
         break;
      case MENU_SETTINGS_SHADER_0 + (0 * 3):
      case MENU_SETTINGS_SHADER_0 + (1 * 3):
      case MENU_SETTINGS_SHADER_0 + (2 * 3):
      case MENU_SETTINGS_SHADER_0 + (3 * 3):
      case MENU_SETTINGS_SHADER_0 + (4 * 3):
      case MENU_SETTINGS_SHADER_0 + (5 * 3):
      case MENU_SETTINGS_SHADER_0 + (6 * 3):
      case MENU_SETTINGS_SHADER_0 + (7 * 3):
      case MENU_SETTINGS_SHADER_0 + (8 * 3):
      case MENU_SETTINGS_SHADER_0 + (9 * 3):
      case MENU_SETTINGS_SHADER_0 + (10 * 3):
      case MENU_SETTINGS_SHADER_0 + (11 * 3):
      case MENU_SETTINGS_SHADER_0 + (12 * 3):
      case MENU_SETTINGS_SHADER_0 + (13 * 3):
      case MENU_SETTINGS_SHADER_0 + (14 * 3):
      case MENU_SETTINGS_SHADER_0 + (15 * 3):
         snprintf(msg, sizeof(msg),
               " -- Path to shader. \n"
               " \n"
               "All shaders must be of the same \n"
               "type (i.e. CG, GLSL or HLSL). \n"
               " \n"
               "Set Shader Directory to set where \n"
               "the browser starts to look for \n"
               "shaders."
               );
         break;
      case MENU_SETTINGS_SHADER_0_SCALE + (0 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (1 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (2 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (3 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (4 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (5 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (6 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (7 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (8 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (9 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (10 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (11 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (12 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (13 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (14 * 3):
      case MENU_SETTINGS_SHADER_0_SCALE + (15 * 3):
         snprintf(msg, sizeof(msg),
               " -- Scale for this pass. \n"
               " \n"
               "The scale factor accumulates, i.e. 2x \n"
               "for first pass and 2x for second pass \n"
               "will give you a 4x total scale. \n"
               " \n"
               "If there is a scale factor for last \n"
               "pass, the result is stretched to \n"
               "screen with the filter specified in \n"
               "'Default Filter'. \n"
               " \n"
               "If 'Don't Care' is set, either 1x \n"
               "scale or stretch to fullscreen will \n"
               "be used depending if it's not the last \n"
               "pass or not."
               );
         break;
      default:
         snprintf(msg, sizeof(msg),
               "-- No info on this item available. --\n");
   }

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
   return 0;
}

static int menu_start_screen_iterate(unsigned action)
{
   unsigned i;
   char msg[1024];

   if (!driver.menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RARCH_MENU_TOGGLE,
      RARCH_QUIT_KEY,
   };
   char desc[ARRAY_SIZE(binds)][64];

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      if (driver.input && driver.input->set_keybinds)
      {
         struct platform_bind key_label;
         strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
         key_label.joykey = g_settings.input.binds[0][binds[i]].joykey;
         driver.input->set_keybinds(&key_label, 0, 0, 0, 1ULL << KEYBINDS_ACTION_GET_BIND_LABEL);
         strlcpy(desc[i], key_label.desc, sizeof(desc[i]));
      }
      else
      {
         const struct retro_keybind *bind = &g_settings.input.binds[0][binds[i]];
         const struct retro_keybind *auto_bind = input_get_auto_bind(0, binds[i]);
         input_get_bind_string(desc[i], bind, auto_bind, sizeof(desc[i]));
      }
   }

   snprintf(msg, sizeof(msg),
         "-- Welcome to RetroArch --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic Menu controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "           Info: %-20s\n"
         "Enter/Exit Menu: %-20s\n"
         " Exit RetroArch: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Core).\n"
         "Load a content file (Load Content).     \n"
         " \n"

         "See Path Options to set directories\n"
         "for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
         desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6]);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
   return 0;
}

static unsigned menu_common_type_is(unsigned type)
{
   unsigned ret = 0;
   bool type_found;

   type_found =
      type == MENU_SETTINGS ||
      type == MENU_SETTINGS_GENERAL_OPTIONS ||
      type == MENU_SETTINGS_CORE_OPTIONS ||
      type == MENU_SETTINGS_CORE_INFO ||
      type == MENU_SETTINGS_VIDEO_OPTIONS ||
      type == MENU_SETTINGS_FONT_OPTIONS ||
      type == MENU_SETTINGS_SHADER_OPTIONS ||
      type == MENU_SETTINGS_SHADER_PARAMETERS ||
      type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS ||
      type == MENU_SETTINGS_AUDIO_OPTIONS ||
      type == MENU_SETTINGS_DISK_OPTIONS ||
      type == MENU_SETTINGS_PATH_OPTIONS ||
      type == MENU_SETTINGS_PRIVACY_OPTIONS ||
      type == MENU_SETTINGS_OVERLAY_OPTIONS ||
      type == MENU_SETTINGS_NETPLAY_OPTIONS ||
      type == MENU_SETTINGS_OPTIONS ||
      type == MENU_SETTINGS_DRIVERS ||
      type == MENU_SETTINGS_PERFORMANCE_COUNTERS ||
      type == MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO ||
      type == MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND ||
      (type == MENU_SETTINGS_INPUT_OPTIONS);

   if (type_found)
   {
      ret = MENU_SETTINGS;
      return ret;
   }

   type_found = (type >= MENU_SETTINGS_SHADER_0 &&
         type <= MENU_SETTINGS_SHADER_LAST &&
         ((type - MENU_SETTINGS_SHADER_0) % 3) == 0) ||
      type == MENU_SETTINGS_SHADER_PRESET;

   if (type_found)
   {
      ret = MENU_SETTINGS_SHADER_OPTIONS;
      return ret;
   }

   type_found =
      type == MENU_BROWSER_DIR_PATH ||
      type == MENU_CONTENT_DIR_PATH ||
      type == MENU_ASSETS_DIR_PATH ||
      type == MENU_SHADER_DIR_PATH ||
      type == MENU_FILTER_DIR_PATH ||
      type == MENU_DSP_FILTER_DIR_PATH ||
      type == MENU_SAVESTATE_DIR_PATH ||
      type == MENU_LIBRETRO_DIR_PATH ||
      type == MENU_LIBRETRO_INFO_DIR_PATH ||
      type == MENU_CONFIG_DIR_PATH ||
      type == MENU_SAVEFILE_DIR_PATH ||
      type == MENU_OVERLAY_DIR_PATH ||
      type == MENU_SCREENSHOT_DIR_PATH ||
      type == MENU_AUTOCONFIG_DIR_PATH ||
      type == MENU_EXTRACTION_DIR_PATH ||
      type == MENU_SYSTEM_DIR_PATH;

   if (type_found)
   {
      ret = MENU_FILE_DIRECTORY;
      return ret;
   }

   return ret;
}

static int menu_settings_iterate(unsigned action)
{
   if (!driver.menu)
      return 0;

   driver.menu->frame_buf_pitch = driver.menu->width * 2;
   unsigned type = 0;
   const char *label = NULL;
   if (action != MENU_ACTION_REFRESH)
      file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &label, &type);

   if (type == MENU_SETTINGS_CORE)
      label = g_settings.libretro_directory;
   else if (type == MENU_SETTINGS_CONFIG)
      label = g_settings.menu_config_directory;
   else if (type == MENU_SETTINGS_DISK_APPEND)
      label = g_settings.menu_content_directory;

   const char *dir = NULL;
   unsigned menu_type = 0;
   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.menu->need_refresh)
      action = MENU_ACTION_NOOP;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (driver.menu->selection_ptr > 0)
            menu_decrement_navigation(driver.menu);
         else
            menu_set_navigation(driver.menu, file_list_get_size(driver.menu->selection_buf) - 1);
         break;

      case MENU_ACTION_DOWN:
         if (driver.menu->selection_ptr + 1 < file_list_get_size(driver.menu->selection_buf))
            menu_increment_navigation(driver.menu);
         else
            menu_clear_navigation(driver.menu);
         break;

      case MENU_ACTION_CANCEL:
         if (file_list_get_size(driver.menu->menu_stack) > 1)
         {
            file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
            driver.menu->need_refresh = true;
         }
         break;
      case MENU_ACTION_SELECT:
         {
            const char *path = NULL;
            file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &path, &driver.menu->info_selection);
            file_list_push(driver.menu->menu_stack, "", MENU_INFO_SCREEN, driver.menu->selection_ptr);
         }
         break;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
      case MENU_ACTION_OK:
      case MENU_ACTION_START:
         if ((type == MENU_SETTINGS_OPEN_FILEBROWSER || type == MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE)
               && action == MENU_ACTION_OK)
         {
            driver.menu->defer_core = type == MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE;
            file_list_push(driver.menu->menu_stack, g_settings.menu_content_directory, MENU_FILE_DIRECTORY, driver.menu->selection_ptr);
            menu_clear_navigation(driver.menu);
            driver.menu->need_refresh = true;
         }
         else if ((type == MENU_SETTINGS_OPEN_HISTORY || menu_common_type_is(type) == MENU_FILE_DIRECTORY) && action == MENU_ACTION_OK)
         {
            file_list_push(driver.menu->menu_stack, "", type, driver.menu->selection_ptr);
            menu_clear_navigation(driver.menu);
            driver.menu->need_refresh = true;
         }
         else if ((menu_common_type_is(type) == MENU_SETTINGS || type == MENU_SETTINGS_CORE || type == MENU_SETTINGS_CONFIG || type == MENU_SETTINGS_DISK_APPEND) && action == MENU_ACTION_OK)
         {
            file_list_push(driver.menu->menu_stack, label, type, driver.menu->selection_ptr);
            menu_clear_navigation(driver.menu);
            driver.menu->need_refresh = true;
         }
         else if (type == MENU_SETTINGS_CUSTOM_VIEWPORT && action == MENU_ACTION_OK)
         {
            file_list_push(driver.menu->menu_stack, "", type, driver.menu->selection_ptr);

            // Start with something sane.
            rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;

            if (driver.video_data && driver.video && driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, custom);
            aspectratio_lut[ASPECT_RATIO_CUSTOM].value = (float)custom->width / custom->height;

            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
            if (driver.video_data && driver.video_poke && driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data,
                     g_settings.video.aspect_ratio_idx);
         }
         else
         {
            int ret = 0;

            if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_toggle)
               ret = driver.menu_ctx->backend->setting_toggle(type, action, menu_type);
            if (ret)
               return ret;
         }
         break;

      case MENU_ACTION_REFRESH:
         menu_clear_navigation(driver.menu);
         driver.menu->need_refresh = true;
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
         break;

      default:
         break;
   }

   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.menu->need_refresh && !(menu_type == MENU_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY ||
            menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER ||
            menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER ||
            menu_type == MENU_SETTINGS_OVERLAY_PRESET ||
            menu_type == MENU_SETTINGS_CONTENT_HISTORY_PATH ||
            menu_type == MENU_SETTINGS_CORE ||
            menu_type == MENU_SETTINGS_CONFIG ||
            menu_type == MENU_SETTINGS_DISK_APPEND ||
            menu_type == MENU_SETTINGS_OPEN_HISTORY))
   {
      driver.menu->need_refresh = false;
      if (
               menu_type == MENU_SETTINGS_INPUT_OPTIONS
            || menu_type == MENU_SETTINGS_PATH_OPTIONS
            || menu_type == MENU_SETTINGS_OVERLAY_OPTIONS
            || menu_type == MENU_SETTINGS_NETPLAY_OPTIONS
            || menu_type == MENU_SETTINGS_OPTIONS
            || menu_type == MENU_SETTINGS_DRIVERS
            || menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS
            || menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND
            || menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO
            || menu_type == MENU_SETTINGS_CORE_INFO
            || menu_type == MENU_SETTINGS_CORE_OPTIONS
            || menu_type == MENU_SETTINGS_AUDIO_OPTIONS
            || menu_type == MENU_SETTINGS_DISK_OPTIONS
            || menu_type == MENU_SETTINGS_PRIVACY_OPTIONS
            || menu_type == MENU_SETTINGS_GENERAL_OPTIONS
            || menu_type == MENU_SETTINGS_VIDEO_OPTIONS
            || menu_type == MENU_SETTINGS_FONT_OPTIONS
            || menu_type == MENU_SETTINGS_SHADER_OPTIONS
            || menu_type == MENU_SETTINGS_SHADER_PARAMETERS
            || menu_type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS
            )
         menu_common_entries_init(driver.menu, menu_type);
      else
         menu_common_entries_init(driver.menu, MENU_SETTINGS);
   }

   if (driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   // Have to defer it so we let settings refresh.
   if (driver.menu->push_start_screen)
   {
      driver.menu->push_start_screen = false;
      file_list_push(driver.menu->menu_stack, "", MENU_START_SCREEN, 0);
   }

   return 0;
}

static int menu_viewport_iterate(unsigned action)
{
   rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;

   if (!driver.menu)
      return 0;

   unsigned menu_type = 0;
   file_list_get_last(driver.menu->menu_stack, NULL, &menu_type);

   struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   int stride_x = g_settings.video.scale_integer ?
      geom->base_width : 1;
   int stride_y = g_settings.video.scale_integer ?
      geom->base_height : 1;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case MENU_ACTION_DOWN:
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y += stride_y;
            if (custom->height >= (unsigned)stride_y)
               custom->height -= stride_y;
         }
         else
            custom->height += stride_y;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case MENU_ACTION_LEFT:
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case MENU_ACTION_RIGHT:
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x += stride_x;
            if (custom->width >= (unsigned)stride_x)
               custom->width -= stride_x;
         }
         else
            custom->width += stride_x;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case MENU_ACTION_CANCEL:
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT_2)
         {
            file_list_push(driver.menu->menu_stack, "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  driver.menu->selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            file_list_push(driver.menu->menu_stack, "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT_2,
                  driver.menu->selection_ptr);
         }
         break;

      case MENU_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            rarch_viewport_t vp;

            if (driver.video_data && driver.video && driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, &vp);

            if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
            {
               custom->width += custom->x;
               custom->height += custom->y;
               custom->x = 0;
               custom->y = 0;
            }
            else
            {
               custom->width = vp.full_width - custom->x;
               custom->height = vp.full_height - custom->y;
            }

            if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
         break;

      default:
         break;
   }

   file_list_get_last(driver.menu->menu_stack, NULL, &menu_type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   const char *base_msg = NULL;
   char msg[64];

   if (g_settings.video.scale_integer)
   {
      custom->x = 0;
      custom->y = 0;
      custom->width = ((custom->width + geom->base_width - 1) / geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;

      base_msg = "Set scale";
      snprintf(msg, sizeof(msg), "%s (%4ux%4u, %u x %u scale)",
            base_msg,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height);
   }
   else
   {
      if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT_2)
         base_msg = "Set Bottom-Right Corner";

      snprintf(msg, sizeof(msg), "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height);
   }

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
      driver.video_poke->apply_state_changes(driver.video_data);

   return 0;
}

static void menu_parse_and_resolve(unsigned menu_type)
{
   const core_info_t *info = NULL;
   const char *dir;
   size_t i, list_size;
   file_list_t *list;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot parse and resolve menu, menu handle is not initialized.\n");
      return;
   }

   dir = NULL;

   file_list_clear(driver.menu->selection_buf);

   // parsing switch
   switch (menu_type)
   {
      case MENU_SETTINGS_OPEN_HISTORY:
         /* History parse */
         list_size = content_history_size(g_extern.history);

         for (i = 0; i < list_size; i++)
         {
            const char *path, *core_path, *core_name;
            char fill_buf[PATH_MAX];

            path = NULL;
            core_path = NULL;
            core_name = NULL;

            content_history_get_index(g_extern.history, i,
                  &path, &core_path, &core_name);

            if (path)
            {
               char path_short[PATH_MAX];
               fill_pathname(path_short, path_basename(path), "", sizeof(path_short));

               snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
                     path_short, core_name);
            }
            else
               strlcpy(fill_buf, core_name, sizeof(fill_buf));

            file_list_push(driver.menu->selection_buf, fill_buf, MENU_FILE_PLAIN, 0);
         }
         break;
      case MENU_SETTINGS_DEFERRED_CORE:
         break;
      default:
         {
            /* Directory parse */
            file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

            if (!*dir)
            {
#if defined(GEKKO)
#ifdef HW_RVL
               file_list_push(driver.menu->selection_buf, "sd:/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "usb:/", menu_type, 0);
#endif
               file_list_push(driver.menu->selection_buf, "carda:/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "cardb:/", menu_type, 0);
#elif defined(_XBOX1)
               file_list_push(driver.menu->selection_buf, "C:", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "D:", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "E:", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "F:", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "G:", menu_type, 0);
#elif defined(_XBOX360)
               file_list_push(driver.menu->selection_buf, "game:", menu_type, 0);
#elif defined(_WIN32)
               unsigned drives = GetLogicalDrives();
               char drive[] = " :\\";
               for (i = 0; i < 32; i++)
               {
                  drive[0] = 'A' + i;
                  if (drives & (1 << i))
                     file_list_push(driver.menu->selection_buf, drive, menu_type, 0);
               }
#elif defined(__CELLOS_LV2__)
               file_list_push(driver.menu->selection_buf, "/app_home/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_hdd0/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_hdd1/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/host_root/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb000/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb001/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb002/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb003/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb004/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb005/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/dev_usb006/", menu_type, 0);
#elif defined(PSP)
               file_list_push(driver.menu->selection_buf, "ms0:/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "ef0:/", menu_type, 0);
               file_list_push(driver.menu->selection_buf, "host0:/", menu_type, 0);
#else
               file_list_push(driver.menu->selection_buf, "/", menu_type, 0);
#endif
               return;
            }
#if defined(GEKKO) && defined(HW_RVL)
            LWP_MutexLock(gx_device_mutex);
            int dev = gx_get_device_from_path(dir);

            if (dev != -1 && !gx_devices[dev].mounted && gx_devices[dev].interface->isInserted())
               fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

            LWP_MutexUnlock(gx_device_mutex);
#endif

            const char *exts;
            char ext_buf[1024];
            if (menu_type == MENU_SETTINGS_CORE)
               exts = EXT_EXECUTABLES;
            else if (menu_type == MENU_SETTINGS_CONFIG)
               exts = "cfg";
            else if (menu_type == MENU_SETTINGS_SHADER_PRESET)
               exts = "cgp|glslp";
            else if (menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS)
               exts = "cg|glsl";
            else if (menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER)
               exts = EXT_EXECUTABLES;
            else if (menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER)
               exts = "dsp";
            else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
               exts = "cfg";
            else if (menu_type == MENU_SETTINGS_CONTENT_HISTORY_PATH)
               exts = "cfg";
            else if (menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY)
               exts = ""; // we ignore files anyway
            else if (driver.menu->defer_core)
               exts = driver.menu->core_info ? core_info_list_get_all_extensions(driver.menu->core_info) : "";
            else if (driver.menu->info.valid_extensions)
            {
               exts = ext_buf;
               if (*driver.menu->info.valid_extensions)
                  snprintf(ext_buf, sizeof(ext_buf), "%s|zip", driver.menu->info.valid_extensions);
               else
                  *ext_buf = '\0';
            }
            else
               exts = g_extern.system.valid_extensions;

            struct string_list *list = dir_list_new(dir, exts, true);
            if (!list)
               return;

            dir_list_sort(list, true);

            if (menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY)
               file_list_push(driver.menu->selection_buf, "<Use this directory>", MENU_FILE_USE_DIRECTORY, 0);

            list_size = list->size;
            for (i = 0; i < list_size; i++)
            {
               bool is_dir = list->elems[i].attr.b;

               if ((menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY) && !is_dir)
                  continue;

               // Need to preserve slash first time.
               const char *path = list->elems[i].data;
               if (*dir)
                  path = path_basename(path);

#ifdef HAVE_LIBRETRO_MANAGEMENT
               if (menu_type == MENU_SETTINGS_CORE && (is_dir || strcasecmp(path, SALAMANDER_FILE) == 0))
                  continue;
#endif

               // Push menu_type further down in the chain.
               // Needed for shader manager currently.
               file_list_push(driver.menu->selection_buf, path,
                     is_dir ? menu_type : MENU_FILE_PLAIN, 0);
            }

            if (driver.menu_ctx && driver.menu_ctx->backend->entries_init)
               driver.menu_ctx->backend->entries_init(driver.menu, menu_type);

            string_list_free(list);
         }
   }

   // resolving switch
   switch (menu_type)
   {
      case MENU_SETTINGS_CORE:
         dir = NULL;
         list = (file_list_t*)driver.menu->selection_buf;
         file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);
         list_size = file_list_get_size(list);
         for (i = 0; i < list_size; i++)
         {
            const char *path;
            char core_path[PATH_MAX], display_name[256];
            unsigned type = 0;

            path = NULL;
            file_list_get_at_offset(list, i, &path, &type);
            if (type != MENU_FILE_PLAIN)
               continue;

            fill_pathname_join(core_path, dir, path, sizeof(core_path));

            if (driver.menu->core_info &&
                  core_info_list_get_display_name(driver.menu->core_info,
                     core_path, display_name, sizeof(display_name)))
               file_list_set_alt_at_offset(list, i, display_name);
         }
         file_list_sort_on_alt(driver.menu->selection_buf);
         break;
      case MENU_SETTINGS_DEFERRED_CORE:
         core_info_list_get_supported_cores(driver.menu->core_info, driver.menu->deferred_path, &info, &list_size);
         for (i = 0; i < list_size; i++)
         {
            file_list_push(driver.menu->selection_buf, info[i].path, MENU_FILE_PLAIN, 0);
            file_list_set_alt_at_offset(driver.menu->selection_buf, i, info[i].display_name);
         }
         file_list_sort_on_alt(driver.menu->selection_buf);
         break;
      default:
         (void)0;
   }

   driver.menu->scroll_indices_size = 0;
   if (menu_type != MENU_SETTINGS_OPEN_HISTORY)
      menu_build_scroll_indices(driver.menu->selection_buf);

   // Before a refresh, we could have deleted a file on disk, causing
   // selection_ptr to suddendly be out of range. Ensure it doesn't overflow.
   if (driver.menu->selection_ptr >= file_list_get_size(driver.menu->selection_buf) && file_list_get_size(driver.menu->selection_buf))
      menu_set_navigation(driver.menu, file_list_get_size(driver.menu->selection_buf) - 1);
   else if (!file_list_get_size(driver.menu->selection_buf))
      menu_clear_navigation(driver.menu);
}

// This only makes sense for PC so far.
// Consoles use set_keybind callbacks instead.
static int menu_custom_bind_iterate(void *data, unsigned action)
{
   char msg[256];
   menu_handle_t *menu = (menu_handle_t*)data;

   (void)action; // Have to ignore action here. Only bind that should work here is Quit RetroArch or something like that.
   (void)msg;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)", input_config_bind_map[menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   struct menu_bind_state binds = menu->binds;
   menu_poll_bind_state(&binds);

   if ((binds.skip && !menu->binds.skip) || menu_poll_find_trigger(&menu->binds, &binds))
   {
      binds.begin++;
      if (binds.begin <= binds.last)
         binds.target++;
      else
         file_list_pop(menu->menu_stack, &menu->selection_ptr);

      // Avoid new binds triggering things right away.
      menu->trigger_state = 0;
      menu->old_input_state = -1ULL;
   }
   menu->binds = binds;
   return 0;
}

static int menu_custom_bind_iterate_keyboard(void *data, unsigned action)
{
   char msg[256];
   menu_handle_t *menu = (menu_handle_t*)data;

   (void)action; // Have to ignore action here.
   (void)msg;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   int64_t current = rarch_get_time_usec();
   int timeout = (menu->binds.timeout_end - current) / 1000000;

   snprintf(msg, sizeof(msg), "[%s]\npress keyboard\n(timeout %d seconds)",
         input_config_bind_map[menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc, timeout);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   bool timed_out = false;
   if (timeout <= 0)
   {
      menu->binds.begin++;
      menu->binds.target->key = RETROK_UNKNOWN; // Could be unsafe, but whatever.
      menu->binds.target++;
      menu->binds.timeout_end = rarch_get_time_usec() + MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      timed_out = true;
   }

   // binds.begin is updated in keyboard_press callback.
   if (menu->binds.begin > menu->binds.last)
   {
      file_list_pop(menu->menu_stack, &menu->selection_ptr);

      // Avoid new binds triggering things right away.
      menu->trigger_state = 0;
      menu->old_input_state = -1ULL;

      // We won't be getting any key events, so just cancel early.
      if (timed_out)
         input_keyboard_wait_keys_cancel();
   }
   return 0;
}

static void menu_common_defer_decision_automatic(void)
{
   if (!driver.menu)
      return;

   menu_flush_stack_type(MENU_SETTINGS);
   driver.menu->msg_force = true;
}

static void menu_common_defer_decision_manual(void)
{
   if (!driver.menu)
      return;

   file_list_push(driver.menu->menu_stack, g_settings.libretro_directory, MENU_SETTINGS_DEFERRED_CORE, driver.menu->selection_ptr);
   menu_clear_navigation(driver.menu);
   driver.menu->need_refresh = true;
}

static int menu_common_setting_set_perf(unsigned setting, unsigned action,
      struct retro_perf_counter **counters, unsigned offset)
{
   if (counters[offset] && action == MENU_ACTION_START)
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }
   return 0;
}

static void menu_common_setting_set_current_boolean(rarch_setting_t *setting, unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         *setting->value.boolean = !(*setting->value.boolean);
         break;
      case MENU_ACTION_START:
         *setting->value.boolean = setting->default_value.boolean;
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_fraction(rarch_setting_t *setting, float step, unsigned action,
      bool enforce_min_check, bool enforce_max_check)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         *setting->value.fraction = *setting->value.fraction - step;

         if (enforce_min_check)
         {
            if (*setting->value.fraction < setting->min)
               *setting->value.fraction = setting->min;
         }
         break;

      case MENU_ACTION_RIGHT:
      case MENU_ACTION_OK:
         *setting->value.fraction = *setting->value.fraction + step;

         if (enforce_max_check)
         {
            if (*setting->value.fraction > setting->max)
               *setting->value.fraction = setting->max;
         }
         break;

      case MENU_ACTION_START:
         *setting->value.fraction = setting->default_value.fraction;
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_unsigned_integer(rarch_setting_t *setting, unsigned step, unsigned action,
      bool enforce_min_check, bool enforce_max_check)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (*setting->value.unsigned_integer != setting->min)
            *setting->value.unsigned_integer = *setting->value.unsigned_integer - step;

         if (enforce_min_check)
         {
            if (*setting->value.unsigned_integer < setting->min)
               *setting->value.unsigned_integer = setting->min;
         }
         break;

      case MENU_ACTION_RIGHT:
      case MENU_ACTION_OK:
         *setting->value.unsigned_integer = *setting->value.unsigned_integer + step;

         if (enforce_max_check)
         {
            if (*setting->value.unsigned_integer > setting->max)
               *setting->value.unsigned_integer = setting->max;
         }
         break;

      case MENU_ACTION_START:
         *setting->value.unsigned_integer = setting->default_value.unsigned_integer;
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_string_path(rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);
}

static int menu_common_iterate(unsigned action)
{
   rarch_setting_t *setting_data, *current_setting;
   int ret = 0;
   const char *dir = 0;
   unsigned menu_type = 0;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot iterate menu, menu handle is not initialized.\n");
      return 0;
   }

   setting_data = (rarch_setting_t *)setting_data_get_list();

   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

#ifdef HAVE_OSK
   // process pending osk init callback
   if (g_extern.osk.cb_init)
   {
      if (g_extern.osk.cb_init(driver.osk_data))
         g_extern.osk.cb_init = NULL;
   }

   // process pending osk callback
   if (g_extern.osk.cb_callback)
   {
      if (g_extern.osk.cb_callback(driver.osk_data))
         g_extern.osk.cb_callback = NULL;
   }
#endif

   if (menu_type == MENU_START_SCREEN)
      return menu_start_screen_iterate(action);
   else if (menu_type == MENU_INFO_SCREEN)
      return menu_info_screen_iterate(action);
   else if (menu_common_type_is(menu_type) == MENU_SETTINGS)
      return menu_settings_iterate(action);
   else if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT || menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT_2)
      return menu_viewport_iterate(action);
   else if (menu_type == MENU_SETTINGS_CUSTOM_BIND)
      return menu_custom_bind_iterate(driver.menu, action);
   else if (menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
      return menu_custom_bind_iterate_keyboard(driver.menu, action);

   if (driver.menu->need_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_NOOP;

   unsigned scroll_speed = (max(driver.menu->scroll_accel, 2) - 2) / 4 + 1;
   unsigned fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (driver.menu->selection_ptr >= scroll_speed)
            menu_set_navigation(driver.menu, driver.menu->selection_ptr - scroll_speed);
         else
            menu_set_navigation(driver.menu, file_list_get_size(driver.menu->selection_buf) - 1);
         break;

      case MENU_ACTION_DOWN:
         if (driver.menu->selection_ptr + scroll_speed < file_list_get_size(driver.menu->selection_buf))
            menu_set_navigation(driver.menu, driver.menu->selection_ptr + scroll_speed);
         else
            menu_clear_navigation(driver.menu);
         break;

      case MENU_ACTION_LEFT:
         if (driver.menu->selection_ptr > fast_scroll_speed)
            menu_set_navigation(driver.menu, driver.menu->selection_ptr - fast_scroll_speed);
         else
            menu_clear_navigation(driver.menu);
         break;

      case MENU_ACTION_RIGHT:
         if (driver.menu->selection_ptr + fast_scroll_speed < file_list_get_size(driver.menu->selection_buf))
            menu_set_navigation(driver.menu, driver.menu->selection_ptr + fast_scroll_speed);
         else
            menu_set_navigation_last(driver.menu);
         break;

      case MENU_ACTION_SCROLL_UP:
         menu_descend_alphabet(driver.menu, &driver.menu->selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_ascend_alphabet(driver.menu, &driver.menu->selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (file_list_get_size(driver.menu->menu_stack) > 1)
         {
            file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
            driver.menu->need_refresh = true;
         }
         break;

      case MENU_ACTION_OK:
      {
         if (file_list_get_size(driver.menu->selection_buf) == 0)
            return 0;

         const char *path = 0;
         unsigned type = 0;
         file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &path, &type);

         if (
               menu_common_type_is(type) == MENU_SETTINGS_SHADER_OPTIONS ||
               menu_common_type_is(type) == MENU_FILE_DIRECTORY ||
               type == MENU_SETTINGS_OVERLAY_PRESET ||
               type == MENU_SETTINGS_VIDEO_SOFTFILTER ||
               type == MENU_SETTINGS_AUDIO_DSP_FILTER ||
               type == MENU_SETTINGS_CONTENT_HISTORY_PATH ||
               type == MENU_SETTINGS_CORE ||
               type == MENU_SETTINGS_CONFIG ||
               type == MENU_SETTINGS_DISK_APPEND ||
               type == MENU_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            fill_pathname_join(cat_path, dir, path, sizeof(cat_path));

            file_list_push(driver.menu->menu_stack, cat_path, type, driver.menu->selection_ptr);
            menu_clear_navigation(driver.menu);
            driver.menu->need_refresh = true;
         }
         else
         {
#ifdef HAVE_SHADER_MANAGER
            if (menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS)
            {
               if (menu_type == MENU_SETTINGS_SHADER_PRESET)
               {
                  char shader_path[PATH_MAX];
                  fill_pathname_join(shader_path, dir, path, sizeof(shader_path));
                  if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_set_preset)
                     driver.menu_ctx->backend->shader_manager_set_preset(driver.menu->shader, gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
                        shader_path);
               }
               else
               {
                  struct gfx_shader *shader = driver.menu->shader;
                  unsigned pass = (menu_type - MENU_SETTINGS_SHADER_0) / 3;

                  fill_pathname_join(shader->pass[pass].source.path,
                        dir, path, sizeof(shader->pass[pass].source.path));

                  // This will reset any changed parameters.
                  gfx_shader_resolve_parameters(NULL, driver.menu->shader);
               }

               // Pop stack until we hit shader manager again.
               menu_flush_stack_type(MENU_SETTINGS_SHADER_OPTIONS);
            }
            else
#endif
            if (menu_type == MENU_SETTINGS_DEFERRED_CORE)
            {
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               strlcpy(g_extern.fullpath, driver.menu->deferred_path, sizeof(g_extern.fullpath));
               rarch_main_command(RARCH_CMD_LOAD_CONTENT);
               driver.menu->msg_force = true;
               ret = -1;
               menu_flush_stack_type(MENU_SETTINGS);
            }
            else if (menu_type == MENU_SETTINGS_CORE)
            {
               fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
               rarch_main_command(RARCH_CMD_LOAD_CORE);
#if defined(HAVE_DYNAMIC)
               // No ROM needed for this core, load game immediately.
               if (driver.menu->load_no_rom)
               {
                  g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
                  *g_extern.fullpath = '\0';
                  driver.menu->msg_force = true;
                  ret = -1;
               }

               // Core selection on non-console just updates directory listing.
               // Will take effect on new ROM load.
#elif defined(RARCH_CONSOLE)
#if defined(GEKKO) && defined(HW_RVL)
               fill_pathname_join(g_extern.fullpath, g_defaults.core_dir,
                     SALAMANDER_FILE, sizeof(g_extern.fullpath));
#endif
               g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
               g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
               ret = -1;
#endif

               menu_flush_stack_type(MENU_SETTINGS);
            }
            else if (menu_type == MENU_SETTINGS_CONFIG)
            {
               char config[PATH_MAX];
               fill_pathname_join(config, dir, path, sizeof(config));
               menu_flush_stack_type(MENU_SETTINGS);
               driver.menu->msg_force = true;
               if (menu_replace_config(config))
               {
                  menu_clear_navigation(driver.menu);
                  ret = -1;
               }
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
            {
               if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "input_overlay")))
                  menu_common_setting_set_current_string_path(current_setting, dir, path);

               menu_flush_stack_type(MENU_SETTINGS_OPTIONS);
            }
#endif
            else if (menu_type == MENU_SETTINGS_DISK_APPEND)
            {
               char image[PATH_MAX];
               fill_pathname_join(image, dir, path, sizeof(image));
               rarch_disk_control_append_image(image);

               g_extern.lifecycle_state |= 1ULL << MODE_GAME;

               menu_flush_stack_type(MENU_SETTINGS);
               ret = -1;
            }
            else if (menu_type == MENU_SETTINGS_OPEN_HISTORY)
            {
               load_menu_game_history(driver.menu->selection_ptr);
               menu_flush_stack_type(MENU_SETTINGS);
               ret = -1;
            }
            else if (menu_type == MENU_SETTINGS_CONTENT_HISTORY_PATH)
            {
               fill_pathname_join(g_settings.game_history_path, dir, path, sizeof(g_settings.input.overlay));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_BROWSER_DIR_PATH)
            {
               strlcpy(g_settings.menu_content_directory, dir, sizeof(g_settings.menu_content_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_CONTENT_DIR_PATH)
            {
               strlcpy(g_settings.content_directory, dir, sizeof(g_settings.content_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_ASSETS_DIR_PATH)
            {
               strlcpy(g_settings.assets_directory, dir, sizeof(g_settings.assets_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_SCREENSHOT_DIR_PATH)
            {
               strlcpy(g_settings.screenshot_directory, dir, sizeof(g_settings.screenshot_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_SAVEFILE_DIR_PATH)
            {
               strlcpy(g_extern.savefile_dir, dir, sizeof(g_extern.savefile_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == MENU_OVERLAY_DIR_PATH)
            {
               strlcpy(g_extern.overlay_dir, dir, sizeof(g_extern.overlay_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER)
            {
               fill_pathname_join(g_settings.video.filter_path, dir, path, sizeof(g_settings.video.filter_path));
               rarch_reinit_drivers();
               menu_flush_stack_type(MENU_SETTINGS_VIDEO_OPTIONS);
            }
            else if (menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER)
            {
#ifdef HAVE_DYLIB
               fill_pathname_join(g_settings.audio.dsp_plugin, dir, path, sizeof(g_settings.audio.dsp_plugin));
#endif
               rarch_deinit_dsp_filter();
               rarch_init_dsp_filter();
               menu_flush_stack_type(MENU_SETTINGS_AUDIO_OPTIONS);
            }
            else if (menu_type == MENU_SAVESTATE_DIR_PATH)
            {
               strlcpy(g_extern.savestate_dir, dir, sizeof(g_extern.savestate_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_LIBRETRO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_directory, dir, sizeof(g_settings.libretro_directory));

               if (driver.menu_ctx && driver.menu_ctx->init_core_info)
                  driver.menu_ctx->init_core_info(driver.menu);
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_DYNAMIC
            else if (menu_type == MENU_CONFIG_DIR_PATH)
            {
               strlcpy(g_settings.menu_config_directory, dir, sizeof(g_settings.menu_config_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == MENU_LIBRETRO_INFO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_info_path, dir, sizeof(g_settings.libretro_info_path));
               if (driver.menu_ctx && driver.menu_ctx->init_core_info)
                  driver.menu_ctx->init_core_info(driver.menu);
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_SHADER_DIR_PATH)
            {
               strlcpy(g_settings.video.shader_dir, dir, sizeof(g_settings.video.shader_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_FILTER_DIR_PATH)
            {
               strlcpy(g_settings.video.filter_dir, dir, sizeof(g_settings.video.filter_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_DSP_FILTER_DIR_PATH)
            {
               strlcpy(g_settings.audio.filter_dir, dir, sizeof(g_settings.audio.filter_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_SYSTEM_DIR_PATH)
            {
               strlcpy(g_settings.system_directory, dir, sizeof(g_settings.system_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_AUTOCONFIG_DIR_PATH)
            {
               strlcpy(g_settings.input.autoconfig_dir, dir, sizeof(g_settings.input.autoconfig_dir));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == MENU_EXTRACTION_DIR_PATH)
            {
               strlcpy(g_settings.extraction_directory, dir, sizeof(g_settings.extraction_directory));
               menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
            }
            else
            {
               if (driver.menu->defer_core)
               {
                  ret = menu_defer_core(driver.menu->core_info, dir, path, driver.menu->deferred_path, sizeof(driver.menu->deferred_path));

                  if (ret == -1)
                  {
                     rarch_main_command(RARCH_CMD_LOAD_CORE);
                     if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->defer_decision_automatic) 
                        driver.menu_ctx->backend->defer_decision_automatic();
                  }
                  else if (ret == 0)
                  {
                     if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->defer_decision_manual) 
                        driver.menu_ctx->backend->defer_decision_manual();
                  }
               }
               else
               {
                  fill_pathname_join(g_extern.fullpath, dir, path, sizeof(g_extern.fullpath));
                  g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);

                  menu_flush_stack_type(MENU_SETTINGS);
                  driver.menu->msg_force = true;
                  ret = -1;
               }
            }
         }
         break;
      }

      case MENU_ACTION_REFRESH:
         menu_clear_navigation(driver.menu);
         driver.menu->need_refresh = true;
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
         break;

      default:
         break;
   }


   // refresh values in case the stack changed
   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.menu->need_refresh && (menu_type == MENU_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY ||
            menu_type == MENU_SETTINGS_OVERLAY_PRESET ||
            menu_type == MENU_SETTINGS_CONTENT_HISTORY_PATH ||
            menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER ||
            menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER ||
            menu_type == MENU_SETTINGS_DEFERRED_CORE ||
            menu_type == MENU_SETTINGS_CORE ||
            menu_type == MENU_SETTINGS_CONFIG ||
            menu_type == MENU_SETTINGS_OPEN_HISTORY ||
            menu_type == MENU_SETTINGS_DISK_APPEND))
   {
      driver.menu->need_refresh = false;
      menu_parse_and_resolve(menu_type);
   }

   if (driver.menu_ctx && driver.menu_ctx->iterate)
      driver.menu_ctx->iterate(driver.menu, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   return ret;
}

static void menu_common_shader_manager_init(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;

#ifdef HAVE_SHADER_MANAGER
   config_file_t *conf = NULL;

   const char *config_path = NULL;
   if (*g_extern.core_specific_config_path && g_settings.core_specific_config)
      config_path = g_extern.core_specific_config_path;
   else if (*g_extern.config_path)
      config_path = g_extern.config_path;

   // In a multi-config setting, we can't have conflicts on menu.cgp/menu.glslp.
   if (config_path)
   {
      fill_pathname_base(menu->default_glslp, config_path, sizeof(menu->default_glslp));
      path_remove_extension(menu->default_glslp);
      strlcat(menu->default_glslp, ".glslp", sizeof(menu->default_glslp));
      fill_pathname_base(menu->default_cgp, config_path, sizeof(menu->default_cgp));
      path_remove_extension(menu->default_cgp);
      strlcat(menu->default_cgp, ".cgp", sizeof(menu->default_cgp));
   }
   else
   {
      strlcpy(menu->default_glslp, "menu.glslp", sizeof(menu->default_glslp));
      strlcpy(menu->default_cgp, "menu.cgp", sizeof(menu->default_cgp));
   }

   char cgp_path[PATH_MAX];

   const char *ext = path_get_extension(g_settings.video.shader_path);
   if (strcmp(ext, "glslp") == 0 || strcmp(ext, "cgp") == 0)
   {
      conf = config_file_new(g_settings.video.shader_path);
      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, menu->shader))
         {
            gfx_shader_resolve_relative(menu->shader, g_settings.video.shader_path);
            gfx_shader_resolve_parameters(conf, menu->shader);
         }
         config_file_free(conf);
      }
   }
   else if (strcmp(ext, "glsl") == 0 || strcmp(ext, "cg") == 0)
   {
      struct gfx_shader *shader = menu->shader;

      strlcpy(shader->pass[0].source.path, g_settings.video.shader_path,
            sizeof(shader->pass[0].source.path));
      shader->passes = 1;
   }
   else
   {
      const char *shader_dir = *g_settings.video.shader_dir ?
         g_settings.video.shader_dir : g_settings.system_directory;

      fill_pathname_join(cgp_path, shader_dir, "menu.glslp", sizeof(cgp_path));
      conf = config_file_new(cgp_path);

      if (!conf)
      {
         fill_pathname_join(cgp_path, shader_dir, "menu.cgp", sizeof(cgp_path));
         conf = config_file_new(cgp_path);
      }

      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, menu->shader))
         {
            gfx_shader_resolve_relative(menu->shader, cgp_path);
            gfx_shader_resolve_parameters(conf, menu->shader);
         }
         config_file_free(conf);
      }
   }
#endif
}

static void menu_common_shader_manager_set_preset(struct gfx_shader *shader, unsigned type, const char *path)
{
#ifdef HAVE_SHADER_MANAGER
   RARCH_LOG("Setting Menu shader: %s.\n", path ? path : "N/A (stock)");

   if (video_set_shader_func((enum rarch_shader_type)type, path))
   {
      // Makes sure that we use Menu CGP shader on driver reinit.
      // Only do this when the cgp actually works to avoid potential errors.
      strlcpy(g_settings.video.shader_path, path ? path : "",
            sizeof(g_settings.video.shader_path));
      g_settings.video.shader_enable = true;

      if (path && shader)
      {
         // Load stored CGP into menu on success.
         // Used when a preset is directly loaded.
         // No point in updating when the CGP was created from the menu itself.
         config_file_t *conf = config_file_new(path);
         if (conf)
         {
            gfx_shader_read_conf_cgp(conf, shader);
            gfx_shader_resolve_relative(shader, path);
            gfx_shader_resolve_parameters(conf, shader);
            config_file_free(conf);
         }

         driver.menu->need_refresh = true;
      }
   }
   else
   {
      RARCH_ERR("Setting Menu CGP failed.\n");
      g_settings.video.shader_enable = false;
   }
#endif
}

static void menu_common_shader_manager_get_str(struct gfx_shader *shader, char *type_str, size_t type_str_size, unsigned type)
{
   (void)shader;
   (void)type_str;
   (void)type_str_size;
   (void)type;

#ifdef HAVE_SHADER_MANAGER
   if (type == MENU_SETTINGS_SHADER_APPLY)
      *type_str = '\0';
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0 && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      // menu->parameter_shader here.
      if (shader)
      {
         const struct gfx_shader_parameter *param = (const struct gfx_shader_parameter*)&shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
         snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]", param->current, param->minimum, param->maximum);
      }
      else
         *type_str = '\0';
   }
   else if (type == MENU_SETTINGS_SHADER_PASSES)
      snprintf(type_str, type_str_size, "%u", shader->passes);
   else if (type >= MENU_SETTINGS_SHADER_0 && type <= MENU_SETTINGS_SHADER_LAST)
   {
      unsigned pass = (type - MENU_SETTINGS_SHADER_0) / 3;
      switch ((type - MENU_SETTINGS_SHADER_0) % 3)
      {
         case 0:
            if (*shader->pass[pass].source.path)
               fill_pathname_base(type_str,
                     shader->pass[pass].source.path, type_str_size);
            else
               strlcpy(type_str, "N/A", type_str_size);
            break;

         case 1:
            switch (shader->pass[pass].filter)
            {
               case RARCH_FILTER_LINEAR:
                  strlcpy(type_str, "Linear", type_str_size);
                  break;

               case RARCH_FILTER_NEAREST:
                  strlcpy(type_str, "Nearest", type_str_size);
                  break;

               case RARCH_FILTER_UNSPEC:
                  strlcpy(type_str, "Don't care", type_str_size);
                  break;
            }
            break;

         case 2:
         {
            unsigned scale = shader->pass[pass].fbo.scale_x;
            if (!scale)
               strlcpy(type_str, "Don't care", type_str_size);
            else
               snprintf(type_str, type_str_size, "%ux", scale);
            break;
         }
      }
   }
   else
      *type_str = '\0';
#endif
}

static void menu_common_shader_manager_save_preset(const char *basename, bool apply)
{
#ifdef HAVE_SHADER_MANAGER
   char buffer[PATH_MAX];
   unsigned d, type;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot save shader preset, menu handle is not initialized.\n");
      return;
   }

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_type)
      type = driver.menu_ctx->backend->shader_manager_get_type(driver.menu->shader);
   else
      type = RARCH_SHADER_NONE;

   if (type == RARCH_SHADER_NONE)
      return;

   const char *conf_path = NULL;

   if (basename)
   {
      strlcpy(buffer, basename, sizeof(buffer));
      // Append extension automatically as appropriate.
      if (!strstr(basename, ".cgp") && !strstr(basename, ".glslp"))
      {
         if (type == RARCH_SHADER_GLSL)
            strlcat(buffer, ".glslp", sizeof(buffer));
         else if (type == RARCH_SHADER_CG)
            strlcat(buffer, ".cgp", sizeof(buffer));
      }
      conf_path = buffer;
   }
   else
      conf_path = type == RARCH_SHADER_GLSL ? driver.menu->default_glslp : driver.menu->default_cgp;

   char config_directory[PATH_MAX];
   if (*g_extern.config_path)
      fill_pathname_basedir(config_directory, g_extern.config_path, sizeof(config_directory));
   else
      *config_directory = '\0';

   char cgp_path[PATH_MAX];
   const char *dirs[] = {
      g_settings.video.shader_dir,
      g_settings.menu_config_directory,
      config_directory,
   };

   config_file_t *conf = config_file_new(NULL);
   if (!conf)
      return;
   gfx_shader_write_conf_cgp(conf, driver.menu->shader);

   bool ret = false;

   for (d = 0; d < ARRAY_SIZE(dirs); d++)
   {
      if (!*dirs[d])
         continue;

      fill_pathname_join(cgp_path, dirs[d], conf_path, sizeof(cgp_path));
      if (config_file_write(conf, cgp_path))
      {
         RARCH_LOG("Saved shader preset to %s.\n", cgp_path);
         if (apply)
         {
            if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_set_preset)
               driver.menu_ctx->backend->shader_manager_set_preset(NULL, type, cgp_path);
         }
         ret = true;
         break;
      }
      else
         RARCH_LOG("Failed writing shader preset to %s.\n", cgp_path);
   }

   config_file_free(conf);
   if (!ret)
      RARCH_ERR("Failed to save shader preset. Make sure config directory and/or shader dir are writable.\n");
#endif
}

static unsigned menu_common_shader_manager_get_type(const struct gfx_shader *shader)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned i, type;

   // All shader types must be the same, or we cannot use it.
   type = RARCH_SHADER_NONE;

   if (!shader)
   {
      RARCH_ERR("Cannot get shader type, shader handle is not initialized.\n");
      return type;
   }

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type = gfx_shader_parse_type(shader->pass[i].source.path,
            RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;

         default:
            return RARCH_SHADER_NONE;
      }
   }

   return type;
#else
   return 0;
#endif

}

static int menu_common_shader_manager_setting_toggle(unsigned setting, unsigned action)
{
   if (!driver.menu)
   {
      RARCH_ERR("Cannot toggle shader setting, menu handle is not initialized.\n");
      return 0;
   }

   (void)setting;
   (void)action;

#ifdef HAVE_SHADER_MANAGER
   unsigned dist_shader, dist_filter, dist_scale;

   dist_shader = setting - MENU_SETTINGS_SHADER_0;
   dist_filter = setting - MENU_SETTINGS_SHADER_0_FILTER;
   dist_scale  = setting - MENU_SETTINGS_SHADER_0_SCALE;

   if (setting == MENU_SETTINGS_SHADER_FILTER)
   {
      switch (action)
      {
         case MENU_ACTION_START:
            g_settings.video.smooth = true;
            break;

         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
            g_settings.video.smooth = !g_settings.video.smooth;
            break;

         default:
            break;
      }
   }
   else if ((setting == MENU_SETTINGS_SHADER_PARAMETERS || setting == MENU_SETTINGS_SHADER_PRESET_PARAMETERS) && action == MENU_ACTION_OK)
   {
      file_list_push(driver.menu->menu_stack, "", setting, driver.menu->selection_ptr);
      menu_clear_navigation(driver.menu);
      driver.menu->need_refresh = true;
   }
   else if (setting >= MENU_SETTINGS_SHADER_PARAMETER_0 && setting <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      struct gfx_shader *shader = driver.menu->parameter_shader;
      if (!shader)
         return 0;

      struct gfx_shader_parameter *param = &shader->parameters[setting - MENU_SETTINGS_SHADER_PARAMETER_0];
      if (!param)
         return 0;

      switch (action)
      {
         case MENU_ACTION_START:
            param->current = param->initial;
            break;

         case MENU_ACTION_LEFT:
            param->current -= param->step;
            break;

         case MENU_ACTION_RIGHT:
            param->current += param->step;
            break;

         default:
            break;
      }

      param->current = min(max(param->minimum, param->current), param->maximum);
   }
   else if ((setting == MENU_SETTINGS_SHADER_APPLY || setting == MENU_SETTINGS_SHADER_PASSES) &&
         (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_set))
      driver.menu_ctx->backend->setting_set(setting, action);
   else if (((dist_shader % 3) == 0 || setting == MENU_SETTINGS_SHADER_PRESET))
   {
      dist_shader /= 3;
      struct gfx_shader *shader = driver.menu->shader;
      struct gfx_shader_pass *pass = NULL;
      if (shader && setting == MENU_SETTINGS_SHADER_PRESET)
         pass = &shader->pass[dist_shader];

      switch (action)
      {
         case MENU_ACTION_OK:
            file_list_push(driver.menu->menu_stack, g_settings.video.shader_dir, setting, driver.menu->selection_ptr);
            menu_clear_navigation(driver.menu);
            driver.menu->need_refresh = true;
            break;

         case MENU_ACTION_START:
            if (pass)
               *pass->source.path = '\0';
            break;

         default:
            break;
      }
   }
   else if ((dist_filter % 3) == 0)
   {
      dist_filter /= 3;
      struct gfx_shader *shader = driver.menu->shader;
      struct gfx_shader_pass *pass = &shader->pass[dist_filter];
      switch (action)
      {
         case MENU_ACTION_START:
            shader->pass[dist_filter].filter = RARCH_FILTER_UNSPEC;
            break;

         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
         {
            unsigned delta = action == MENU_ACTION_LEFT ? 2 : 1;
            pass->filter = (enum gfx_filter_type)((pass->filter + delta) % 3);
            break;
         }

         default:
         break;
      }
   }
   else if ((dist_scale % 3) == 0)
   {
      dist_scale /= 3;
      struct gfx_shader *shader = driver.menu->shader;
      struct gfx_shader_pass *pass = &shader->pass[dist_scale];
      switch (action)
      {
         case MENU_ACTION_START:
            pass->fbo.scale_x = pass->fbo.scale_y = 0;
            pass->fbo.valid = false;
            break;

         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
         {
            unsigned current_scale = pass->fbo.scale_x;
            unsigned delta = action == MENU_ACTION_LEFT ? 5 : 1;
            current_scale = (current_scale + delta) % 6;
            pass->fbo.valid = current_scale;
            pass->fbo.scale_x = pass->fbo.scale_y = current_scale;
            break;
         }

         default:
         break;
      }
   }
#endif

   return 0;
}

static int menu_common_setting_toggle(unsigned setting, unsigned action, unsigned menu_type)
{
   (void)menu_type;

#ifdef HAVE_SHADER_MANAGER
   if ((setting >= MENU_SETTINGS_SHADER_FILTER) && (setting <= MENU_SETTINGS_SHADER_LAST))
   {
      if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_setting_toggle)
         return driver.menu_ctx->backend->shader_manager_setting_toggle(setting, action);
      else
         return 0;
   }
#endif
   if ((setting >= MENU_SETTINGS_CORE_OPTION_START) &&
         (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->core_setting_toggle)
      )
      return driver.menu_ctx->backend->core_setting_toggle(setting, action);

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_set)
      return driver.menu_ctx->backend->setting_set(setting, action);

   return 0;
}

static int menu_common_core_setting_toggle(unsigned setting, unsigned action)
{
   unsigned index;
   index = setting - MENU_SETTINGS_CORE_OPTION_START;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, index);
         break;

      case MENU_ACTION_RIGHT:
      case MENU_ACTION_OK:
         core_option_next(g_extern.system.core_options, index);
         break;

      case MENU_ACTION_START:
         core_option_set_default(g_extern.system.core_options, index);
         break;

      default:
         break;
   }

   return 0;
}

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2

static unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

static unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;
#else
#define MAX_GAMMA_SETTING 1
#endif


#ifdef HAVE_OSK
static bool osk_callback_enter_audio_device(void *data)
{
   if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_SUCCESS)
         && driver.osk && driver.osk->get_text_buf)
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      wchar_t *text_buf = (wchar_t*)driver.osk->get_text_buf(driver.osk_data);
      int num = wcstombs(tmp_str, text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;
      strlcpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device));
      goto do_exit;
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;

do_exit:
   g_extern.lifecycle_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_audio_device_init(void *data)
{
   if (!driver.osk)
      return false;

   if (driver.osk->write_initial_msg)
      driver.osk->write_initial_msg(driver.osk_data, L"192.168.1.1");
   if (driver.osk->write_msg)
      driver.osk->write_msg(driver.osk_data, L"Enter Audio Device / IP address for audio driver.");
   if (driver.osk->start)
      driver.osk->start(driver.osk_data);

   return true;
}

static bool osk_callback_enter_filename(void *data)
{
   if (!driver.osk)
   {
      RARCH_ERR("OSK driver is not initialized, exiting OSK callback ...\n");
      return false;
   }

   if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      char filepath[PATH_MAX];
      int num = wcstombs(tmp_str, driver.osk->get_text_buf(driver.osk_data), sizeof(tmp_str));
      tmp_str[num] = 0;

      fill_pathname_join(filepath, g_settings.video.shader_dir, tmp_str, sizeof(filepath));
      strlcat(filepath, ".cgp", sizeof(filepath));
      RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
      config_file_t *conf = config_file_new(NULL);
      if (!conf)
         return false;
      gfx_shader_write_conf_cgp(conf, driver.menu->shader);
      config_file_write(conf, filepath);
      config_file_free(conf);
      goto do_exit;
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;
do_exit:
   g_extern.lifecycle_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_filename_init(void *data)
{
   if (!driver.osk)
      return false;

   if (driver.osk->write_initial_msg)
      driver.osk->write_initial_msg(driver.osk_data, L"Save Preset");
   if (driver.osk->write_msg)
      driver.osk->write_msg(driver.osk_data, L"Enter filename for preset.");
   if (driver.osk->start)
      driver.osk->start(driver.osk_data);

   return true;
}

#endif

#ifndef RARCH_DEFAULT_PORT
#define RARCH_DEFAULT_PORT 55435
#endif


static int menu_common_setting_set(unsigned setting, unsigned action)
{
   rarch_setting_t *setting_data, *current_setting;
   struct retro_perf_counter **counters;
   unsigned port = driver.menu->current_pad;

   setting_data = (rarch_setting_t *)setting_data_get_list();

   if (setting >= MENU_SETTINGS_PERF_COUNTERS_BEGIN && setting <= MENU_SETTINGS_PERF_COUNTERS_END)
   {
      counters = (struct retro_perf_counter**)perf_counters_rarch;
      return menu_common_setting_set_perf(setting, action, counters, setting - MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   }
   else if (setting >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN && setting <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
   {
      counters = (struct retro_perf_counter**)perf_counters_libretro;
      return menu_common_setting_set_perf(setting, action, counters, setting - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   }
   else if (setting >= MENU_SETTINGS_BIND_BEGIN && setting <= MENU_SETTINGS_BIND_ALL_LAST)
   {
      if (driver.input->set_keybinds && !driver.input->get_joypad_driver)
      {
         unsigned keybind_action = KEYBINDS_ACTION_NONE;

         if (action == MENU_ACTION_START)
            keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);

         // FIXME: The array indices here look totally wrong ... Fixed it so it looks kind of sane for now.
         if (keybind_action != KEYBINDS_ACTION_NONE)
            driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port,
                  setting - MENU_SETTINGS_BIND_BEGIN, keybind_action);
      }
      else
      {
         struct retro_keybind *bind = &g_settings.input.binds[port][setting - MENU_SETTINGS_BIND_BEGIN];
         if (action == MENU_ACTION_OK)
         {
            driver.menu->binds.begin = setting;
            driver.menu->binds.last = setting;
            driver.menu->binds.target = bind;
            driver.menu->binds.player = port;
            file_list_push(driver.menu->menu_stack, "",
                  driver.menu->bind_mode_keyboard ? MENU_SETTINGS_CUSTOM_BIND_KEYBOARD : MENU_SETTINGS_CUSTOM_BIND, driver.menu->selection_ptr);

            if (driver.menu->bind_mode_keyboard)
            {
               driver.menu->binds.timeout_end = rarch_get_time_usec() + MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
               input_keyboard_wait_keys(driver.menu, menu_custom_bind_keyboard_cb);
            }
            else
            {
               menu_poll_bind_get_rested_axes(&driver.menu->binds);
               menu_poll_bind_state(&driver.menu->binds);
            }
         }
         else if (action == MENU_ACTION_START)
         {
            if (driver.menu->bind_mode_keyboard)
            {
               const struct retro_keybind *def_binds = port ? retro_keybinds_rest : retro_keybinds_1;
               bind->key = def_binds[setting - MENU_SETTINGS_BIND_BEGIN].key;
            }
            else
            {
               bind->joykey = NO_BTN;
               bind->joyaxis = AXIS_NONE;
            }
         }
      }
      return 0;
   }

   switch (setting)
   {
      case MENU_START_SCREEN:
         if (action == MENU_ACTION_OK)
            file_list_push(driver.menu->menu_stack, "", MENU_START_SCREEN, 0);
         break;
      case MENU_SETTINGS_REWIND_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "rewind_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_GPU_SCREENSHOT:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_gpu_screenshot")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_REWIND_GRANULARITY:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "rewind_granularity")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, false);
         break;
      case MENU_SETTINGS_LIBRETRO_LOG_LEVEL:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "libretro_log_level")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, true);
         break;
      case MENU_SETTINGS_LOGGING_VERBOSITY:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "log_verbosity")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "perfcnt_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_CONFIG_SAVE_ON_EXIT:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "config_save_on_exit")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_SAVESTATE_AUTO_SAVE:
         if ((current_setting = setting_data_find_setting(setting_data, "savestate_auto_save")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_SAVESTATE_AUTO_LOAD:
         if ((current_setting = setting_data_find_setting(setting_data, "savestate_auto_load")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_BLOCK_SRAM_OVERWRITE:
         if ((current_setting = setting_data_find_setting(setting_data, "block_sram_overwrite")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_PER_CORE_CONFIG:
         if ((current_setting = setting_data_find_setting(setting_data, "core_specific_config")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
#if defined(HAVE_THREADS)
      case MENU_SETTINGS_SRAM_AUTOSAVE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "autosave_interval")))
         {
            if (action == MENU_ACTION_OK || action == MENU_ACTION_RIGHT)
               *current_setting->value.unsigned_integer += 10;
            else if (action == MENU_ACTION_LEFT)
            {
               if (*current_setting->value.unsigned_integer)
                  *current_setting->value.unsigned_integer -= min(10, *current_setting->value.unsigned_integer);
            }
            else if (action == MENU_ACTION_START)
               *current_setting->value.unsigned_integer = 0;

            if (current_setting->change_handler)
               current_setting->change_handler(current_setting);
         }
         break;
#endif
      case MENU_SETTINGS_SAVESTATE_SAVE:
      case MENU_SETTINGS_SAVESTATE_LOAD:
         if (action == MENU_ACTION_OK)
         {
            if (setting == MENU_SETTINGS_SAVESTATE_SAVE)
               rarch_main_command(RARCH_CMD_SAVE_STATE);
            else
               rarch_main_command(RARCH_CMD_LOAD_STATE);
            return -1;
         }
         else if (action == MENU_ACTION_START)
            g_extern.state_slot = 0;
         else if (action == MENU_ACTION_LEFT)
         {
            // Slot -1 is (auto) slot.
            if (g_extern.state_slot >= 0)
               g_extern.state_slot--;
         }
         else if (action == MENU_ACTION_RIGHT)
            g_extern.state_slot++;
         break;
      case MENU_SETTINGS_SCREENSHOT:
         if (action == MENU_ACTION_OK)
            rarch_main_command(RARCH_CMD_TAKE_SCREENSHOT);
         break;
      case MENU_SETTINGS_RESTART_GAME:
         if (action == MENU_ACTION_OK)
         {
            rarch_main_command(RARCH_CMD_RESET);
            return -1;
         }
         break;
      case MENU_SETTINGS_AUDIO_MUTE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_mute")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_AUDIO_LATENCY:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_latency")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, false);
         break;
      case MENU_SETTINGS_AUDIO_SYNC:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_sync")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         if (action == MENU_ACTION_START)
         {
            g_settings.audio.rate_control_delta = rate_control_delta;
            g_settings.audio.rate_control = rate_control;
         }
         else if (action == MENU_ACTION_LEFT)
         {
            if (g_settings.audio.rate_control_delta > 0.0)
               g_settings.audio.rate_control_delta -= 0.001;

            if (g_settings.audio.rate_control_delta < 0.0005)
            {
               g_settings.audio.rate_control = false;
               g_settings.audio.rate_control_delta = 0.0;
            }
            else
               g_settings.audio.rate_control = true;
         }
         else if (action == MENU_ACTION_RIGHT)
         {
            if (g_settings.audio.rate_control_delta < 0.2)
               g_settings.audio.rate_control_delta += 0.001;
            g_settings.audio.rate_control = true;
         }
         break;
      case MENU_SETTINGS_AUDIO_VOLUME:
         {
            float db_delta = 0.0f;
            if (action == MENU_ACTION_START)
            {
               g_extern.audio_data.volume_db = 0.0f;
               g_extern.audio_data.volume_gain = 1.0f;
            }
            else if (action == MENU_ACTION_LEFT)
               db_delta -= 1.0f;
            else if (action == MENU_ACTION_RIGHT)
               db_delta += 1.0f;

            if (db_delta != 0.0f)
            {
               g_extern.audio_data.volume_db += db_delta;
               g_extern.audio_data.volume_db = max(g_extern.audio_data.volume_db, -80.0f);
               g_extern.audio_data.volume_db = min(g_extern.audio_data.volume_db, 12.0f);
               g_extern.audio_data.volume_gain = db_to_gain(g_extern.audio_data.volume_db);
            }
         }
         break;
      case MENU_SETTINGS_FASTFORWARD_RATIO:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "fastforward_ratio")))
            menu_common_setting_set_current_fraction(current_setting, 0.1f, action, true, true);
         break;
      case MENU_SETTINGS_SLOWMOTION_RATIO:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "slowmotion_ratio")))
            menu_common_setting_set_current_fraction(current_setting, 0.1f, action, false, false);
         break;
      case MENU_SETTINGS_DEBUG_TEXT:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "fps_show")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_DISK_INDEX:
         {
            const struct retro_disk_control_callback *control = &g_extern.system.disk_control;

            unsigned num_disks = control->get_num_images();
            unsigned current   = control->get_image_index();

            int step = 0;
            if (action == MENU_ACTION_RIGHT || action == MENU_ACTION_OK)
               step = 1;
            else if (action == MENU_ACTION_LEFT)
               step = -1;

            if (step)
            {
               unsigned next_index = (current + num_disks + 1 + step) % (num_disks + 1);
               rarch_disk_control_set_eject(true, false);
               rarch_disk_control_set_index(next_index);
               rarch_disk_control_set_eject(false, false);
            }

            break;
         }
      case MENU_SETTINGS_RESTART_EMULATOR:
         if (action == MENU_ACTION_OK)
         {
#if defined(GEKKO) && defined(HW_RVL)
            fill_pathname_join(g_extern.fullpath, g_defaults.core_dir, SALAMANDER_FILE,
                  sizeof(g_extern.fullpath));
#endif
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }
         break;
      case MENU_SETTINGS_RESUME_GAME:
         if (action == MENU_ACTION_OK)
         {
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case MENU_SETTINGS_QUIT_RARCH:
         if (action == MENU_ACTION_OK)
         {
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            return -1;
         }
         break;
      case MENU_SETTINGS_SAVE_CONFIG:
         if (action == MENU_ACTION_OK)
            menu_save_new_config();
         break;
      case MENU_CONTENT_HISTORY_SIZE:
         if ((current_setting = setting_data_find_setting(setting_data, "game_history_size")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, false);
         break;
#ifdef HAVE_OVERLAY
      case MENU_SETTINGS_OVERLAY_PRESET:
         switch (action)
         {
            case MENU_ACTION_OK:
               file_list_push(driver.menu->menu_stack, g_extern.overlay_dir, setting, driver.menu->selection_ptr);
               menu_clear_navigation(driver.menu);
               driver.menu->need_refresh = true;
               break;
            case MENU_ACTION_START:
               if (driver.overlay)
                  input_overlay_free(driver.overlay);
               driver.overlay = NULL;
               *g_settings.input.overlay = '\0';
               break;
            default:
               break;
         }
         break;
#endif
      case MENU_SETTINGS_CONTENT_HISTORY_PATH:
         switch (action)
         {
            case MENU_ACTION_OK:
               file_list_push(driver.menu->menu_stack, "", setting, driver.menu->selection_ptr);
               menu_clear_navigation(driver.menu);
               driver.menu->need_refresh = true;
               break;
            case MENU_ACTION_START:
               *g_settings.game_history_path = '\0';
               break;
            default:
               break;
         }
         break;
      case MENU_SETTINGS_VIDEO_SOFTFILTER:
         switch (action)
         {
#ifdef HAVE_FILTERS_BUILTIN
            case MENU_ACTION_LEFT:
               if (g_settings.video.filter_idx > 0)
                  g_settings.video.filter_idx--;
               break;
            case MENU_ACTION_RIGHT:
               if ((g_settings.video.filter_idx + 1) != softfilter_get_last_idx())
                  g_settings.video.filter_idx++;
               break;
#endif
            case MENU_ACTION_OK:
#ifdef HAVE_FILTERS_BUILTIN
               driver.menu_data_own = true;
               rarch_reinit_drivers();
#elif defined(HAVE_DYLIB)
               file_list_push(driver.menu->menu_stack, g_settings.video.filter_dir, setting, driver.menu->selection_ptr);
               menu_clear_navigation(driver.menu);
#endif
               driver.menu->need_refresh = true;
               break;
            case MENU_ACTION_START:
#if defined(HAVE_FILTERS_BUILTIN)
               g_settings.video.filter_idx = 0;
#else
               strlcpy(g_settings.video.filter_path, "", sizeof(g_settings.video.filter_path));
#endif
               driver.menu_data_own = true;
               rarch_reinit_drivers();
               break;
         }
         break;
      case MENU_SETTINGS_AUDIO_DSP_FILTER:
         switch (action)
         {
            case MENU_ACTION_OK:
               file_list_push(driver.menu->menu_stack, g_settings.audio.filter_dir, setting, driver.menu->selection_ptr);
               menu_clear_navigation(driver.menu);
               driver.menu->need_refresh = true;
               break;
            case MENU_ACTION_START:
               *g_settings.audio.dsp_plugin = '\0';
               rarch_deinit_dsp_filter();
               break;
         }
         break;
#ifdef HAVE_OVERLAY
      case MENU_SETTINGS_OVERLAY_OPACITY:
         if ((current_setting = setting_data_find_setting(setting_data, "input_overlay_opacity")))
            menu_common_setting_set_current_fraction(current_setting, 0.01f, action, true, true);
         break;
      case MENU_SETTINGS_OVERLAY_SCALE:
         if ((current_setting = setting_data_find_setting(setting_data, "input_overlay_scale")))
            menu_common_setting_set_current_fraction(current_setting, 0.01f, action, true, true);
         break;
#endif
         // controllers
      case MENU_SETTINGS_BIND_PLAYER:
         if (action == MENU_ACTION_START)
            driver.menu->current_pad = 0;
         else if (action == MENU_ACTION_LEFT)
         {
            if (driver.menu->current_pad != 0)
               driver.menu->current_pad--;
         }
         else if (action == MENU_ACTION_RIGHT)
         {
            if (driver.menu->current_pad < MAX_PLAYERS - 1)
               driver.menu->current_pad++;
         }
         if (port != driver.menu->current_pad)
            driver.menu->need_refresh = true;
         port = driver.menu->current_pad;
         break;
      case MENU_SETTINGS_BIND_DEVICE:
         // If set_keybinds is supported, we do it more fancy, and scroll through
         // a list of supported devices directly.
         if (driver.input->set_keybinds && driver.input->devices_size)
         {
            unsigned device_last = driver.input->devices_size(driver.input_data);
            g_settings.input.device[port] += device_last;
            if (action == MENU_ACTION_START)
               g_settings.input.device[port] = 0;
            else if (action == MENU_ACTION_LEFT)
               g_settings.input.device[port]--;
            else if (action == MENU_ACTION_RIGHT)
               g_settings.input.device[port]++;

            // device_last can be 0, avoid modulo.
            if (g_settings.input.device[port] >= device_last)
               g_settings.input.device[port] -= device_last;
            // needs to be checked twice, in case we go right past the end of the list
            if (g_settings.input.device[port] >= device_last)
               g_settings.input.device[port] -= device_last;

            unsigned keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS);

            driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port, 0,
                  keybind_action);
         }
         else
         {
            // When only straight g_settings.input.joypad_map[] style
            // mapping is supported.
            int *p = &g_settings.input.joypad_map[port];
            if (action == MENU_ACTION_START)
               *p = port;
            else if (action == MENU_ACTION_LEFT)
               (*p)--;
            else if (action == MENU_ACTION_RIGHT)
               (*p)++;

            if (*p < -1)
               *p = -1;
            else if (*p >= MAX_PLAYERS)
               *p = MAX_PLAYERS - 1;
         }
         break;
      case MENU_SETTINGS_BIND_ANALOG_MODE:
         switch (action)
         {
            case MENU_ACTION_START:
               g_settings.input.analog_dpad_mode[port] = 0;
               break;

            case MENU_ACTION_OK:
            case MENU_ACTION_RIGHT:
               g_settings.input.analog_dpad_mode[port] = (g_settings.input.analog_dpad_mode[port] + 1) % ANALOG_DPAD_LAST;
               break;

            case MENU_ACTION_LEFT:
               g_settings.input.analog_dpad_mode[port] = (g_settings.input.analog_dpad_mode[port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST;
               break;

            default:
               break;
         }
         break;
      case MENU_SETTINGS_INPUT_AXIS_THRESHOLD:
         if ((current_setting = setting_data_find_setting(setting_data, "input_axis_threshold")))
            menu_common_setting_set_current_fraction(current_setting, 0.01, action, false, false);
         break;
      case MENU_SETTINGS_BIND_DEVICE_TYPE:
         {
            unsigned current_device, current_index, i;
            unsigned types = 0;
            unsigned devices[128];

            devices[types++] = RETRO_DEVICE_NONE;
            devices[types++] = RETRO_DEVICE_JOYPAD;
            // Only push RETRO_DEVICE_ANALOG as default if we use an older core which doesn't use SET_CONTROLLER_INFO.
            if (!g_extern.system.num_ports)
               devices[types++] = RETRO_DEVICE_ANALOG;

            const struct retro_controller_info *desc = port < g_extern.system.num_ports ? &g_extern.system.ports[port] : NULL;
            if (desc)
            {
               for (i = 0; i < desc->num_types; i++)
               {
                  unsigned id = desc->types[i].id;
                  if (types < ARRAY_SIZE(devices) && id != RETRO_DEVICE_NONE && id != RETRO_DEVICE_JOYPAD)
                     devices[types++] = id;
               }
            }

            current_device = g_settings.input.libretro_device[port];
            current_index = 0;
            for (i = 0; i < types; i++)
            {
               if (current_device == devices[i])
               {
                  current_index = i;
                  break;
               }
            }

            bool updated = true;
            switch (action)
            {
               case MENU_ACTION_START:
                  current_device = RETRO_DEVICE_JOYPAD;
                  break;

               case MENU_ACTION_LEFT:
                  current_device = devices[(current_index + types - 1) % types];
                  break;

               case MENU_ACTION_RIGHT:
               case MENU_ACTION_OK:
                  current_device = devices[(current_index + 1) % types];
                  break;

               default:
                  updated = false;
            }

            if (updated)
            {
               g_settings.input.libretro_device[port] = current_device;
               pretro_set_controller_port_device(port, current_device);
            }

            break;
         }
      case MENU_SETTINGS_DEVICE_AUTODETECT_ENABLE:
         if ((current_setting = setting_data_find_setting(setting_data, "input_autodetect_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_CUSTOM_BIND_MODE:
         if (action == MENU_ACTION_OK || action == MENU_ACTION_LEFT || action == MENU_ACTION_RIGHT)
            driver.menu->bind_mode_keyboard = !driver.menu->bind_mode_keyboard;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_ALL:
         if (action == MENU_ACTION_OK)
         {
            if (driver.menu->bind_mode_keyboard)
            {
               driver.menu->binds.target = &g_settings.input.binds[port][0];
               driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
               driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;
               file_list_push(driver.menu->menu_stack, "", MENU_SETTINGS_CUSTOM_BIND_KEYBOARD, driver.menu->selection_ptr);
               driver.menu->binds.timeout_end = rarch_get_time_usec() + MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
               input_keyboard_wait_keys(driver.menu, menu_custom_bind_keyboard_cb);
            }
            else
            {
               driver.menu->binds.target = &g_settings.input.binds[port][0];
               driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
               driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;
               file_list_push(driver.menu->menu_stack, "", MENU_SETTINGS_CUSTOM_BIND, driver.menu->selection_ptr);
               menu_poll_bind_get_rested_axes(&driver.menu->binds);
               menu_poll_bind_state(&driver.menu->binds);
            }
         }
         break;
      case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         if (action == MENU_ACTION_OK)
         {
            unsigned i;
            struct retro_keybind *target = (struct retro_keybind*)&g_settings.input.binds[port][0];
            const struct retro_keybind *def_binds = port ? retro_keybinds_rest : retro_keybinds_1;
            driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
            driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;
            for (i = MENU_SETTINGS_BIND_BEGIN; i <= MENU_SETTINGS_BIND_LAST; i++, target++)
            {
               if (driver.menu->bind_mode_keyboard)
                  target->key = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
               else
               {
                  target->joykey = NO_BTN;
                  target->joyaxis = AXIS_NONE;
               }
            }
         }
         break;
      case MENU_BROWSER_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.menu_content_directory = '\0';
         break;
      case MENU_CONTENT_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.content_directory = '\0';
         break;
      case MENU_ASSETS_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.assets_directory = '\0';
         break;
      case MENU_SCREENSHOT_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.screenshot_directory = '\0';
         break;
      case MENU_SAVEFILE_DIR_PATH:
         if (action == MENU_ACTION_START)
            strlcpy(g_extern.savefile_dir, g_defaults.sram_dir, sizeof(g_extern.savefile_dir));
         break;
#ifdef HAVE_OVERLAY
      case MENU_OVERLAY_DIR_PATH:
         if (action == MENU_ACTION_START)
            strlcpy(g_extern.overlay_dir, g_defaults.overlay_dir, sizeof(g_extern.overlay_dir));
         break;
#endif
      case MENU_SAVESTATE_DIR_PATH:
         if (action == MENU_ACTION_START)
            strlcpy(g_extern.savestate_dir, g_defaults.savestate_dir, sizeof(g_extern.savestate_dir));
         break;
      case MENU_LIBRETRO_DIR_PATH:
         if (action == MENU_ACTION_START)
         {
            *g_settings.libretro_directory = '\0';
            if (driver.menu_ctx && driver.menu_ctx->init_core_info)
               driver.menu_ctx->init_core_info(driver.menu);
         }
         break;
      case MENU_LIBRETRO_INFO_DIR_PATH:
         if (action == MENU_ACTION_START)
         {
            *g_settings.libretro_info_path = '\0';
            if (driver.menu_ctx && driver.menu_ctx->init_core_info)
               driver.menu_ctx->init_core_info(driver.menu);
         }
         break;
      case MENU_CONFIG_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.menu_config_directory = '\0';
         break;
      case MENU_FILTER_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.video.filter_dir = '\0';
         break;
      case MENU_DSP_FILTER_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.audio.filter_dir = '\0';
         break;
      case MENU_SHADER_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.video.shader_dir = '\0';
         break;
      case MENU_SYSTEM_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.system_directory = '\0';
         break;
      case MENU_AUTOCONFIG_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.input.autoconfig_dir = '\0';
         break;
      case MENU_EXTRACTION_DIR_PATH:
         if (action == MENU_ACTION_START)
            *g_settings.extraction_directory = '\0';
         break;
      case MENU_SETTINGS_VIDEO_ROTATION:
         if ((current_setting = setting_data_find_setting(setting_data, "video_rotation")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, true);
         break;
      case MENU_SETTINGS_VIDEO_FILTER:
         if ((current_setting = setting_data_find_setting(setting_data, "video_smooth")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_DRIVER_VIDEO:
         if (action == MENU_ACTION_LEFT)
            find_prev_video_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_video_driver();
         break;
      case MENU_SETTINGS_DRIVER_AUDIO:
         if (action == MENU_ACTION_LEFT)
            find_prev_audio_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_audio_driver();
         break;
      case MENU_SETTINGS_DRIVER_AUDIO_DEVICE:
         if (action == MENU_ACTION_OK)
         {
#ifdef HAVE_OSK
            if (g_settings.osk.enable)
            {
               g_extern.osk.cb_init     = osk_callback_enter_audio_device_init;
               g_extern.osk.cb_callback = osk_callback_enter_audio_device;
            }
            else
#endif
               menu_key_start_line(driver.menu, "Audio Device Name / IP: ", audio_device_callback);
         }
         else if (action == MENU_ACTION_START)
            *g_settings.audio.device = '\0';
         break;
      case MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER:
         if (action == MENU_ACTION_LEFT)
            find_prev_resampler_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_resampler_driver();
         break;
      case MENU_SETTINGS_DRIVER_INPUT:
         if (action == MENU_ACTION_LEFT)
            find_prev_input_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_input_driver();
         break;
#ifdef HAVE_CAMERA
      case MENU_SETTINGS_DRIVER_CAMERA:
         if (action == MENU_ACTION_LEFT)
            find_prev_camera_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_camera_driver();
         break;
#endif
#ifdef HAVE_LOCATION
      case MENU_SETTINGS_DRIVER_LOCATION:
         if (action == MENU_ACTION_LEFT)
            find_prev_location_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_location_driver();
         break;
#endif
#ifdef HAVE_MENU
      case MENU_SETTINGS_DRIVER_MENU:
         if (action == MENU_ACTION_LEFT)
            find_prev_menu_driver();
         else if (action == MENU_ACTION_RIGHT)
            find_next_menu_driver();
         break;
#endif
      case MENU_SETTINGS_VIDEO_GAMMA:
         if (action == MENU_ACTION_START)
         {
            g_extern.console.screen.gamma_correction = 0;
            if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         else if (action == MENU_ACTION_LEFT)
         {
            if (g_extern.console.screen.gamma_correction > 0)
            {
               g_extern.console.screen.gamma_correction--;
               if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         else if (action == MENU_ACTION_RIGHT)
         {
            if (g_extern.console.screen.gamma_correction < MAX_GAMMA_SETTING)
            {
               g_extern.console.screen.gamma_correction++;
               if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         break;

      case MENU_SETTINGS_VIDEO_INTEGER_SCALE:
         if ((current_setting = setting_data_find_setting(setting_data, "video_scale_integer")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;

      case MENU_SETTINGS_VIDEO_ASPECT_RATIO:
         if ((current_setting = setting_data_find_setting(setting_data, "aspect_ratio_index")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, true);
         break;
      case MENU_SETTINGS_TOGGLE_FULLSCREEN:
         if ((current_setting = setting_data_find_setting(setting_data, "video_fullscreen")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;

#if defined(GEKKO)
      case MENU_SETTINGS_VIDEO_RESOLUTION:
         if (action == MENU_ACTION_LEFT)
         {
            if (menu_current_gx_resolution > 0)
               menu_current_gx_resolution--;
         }
         else if (action == MENU_ACTION_RIGHT)
         {
            if (menu_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
            {
#ifdef HW_RVL
               if ((menu_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
                  if (CONF_GetVideo() != CONF_VIDEO_PAL)
                     return 0;
#endif

               menu_current_gx_resolution++;
            }
         }
         else if (action == MENU_ACTION_OK)
         {
            if (driver.video_data)
               gx_set_video_mode(driver.video_data, menu_gx_resolutions[menu_current_gx_resolution][0],
                     menu_gx_resolutions[menu_current_gx_resolution][1]);
         }
         break;
#elif defined(__CELLOS_LV2__)
      case MENU_SETTINGS_VIDEO_RESOLUTION:
         if (action == MENU_ACTION_LEFT)
         {
            if (g_extern.console.screen.resolutions.current.idx)
            {
               g_extern.console.screen.resolutions.current.idx--;
               g_extern.console.screen.resolutions.current.id =
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == MENU_ACTION_RIGHT)
         {
            if (g_extern.console.screen.resolutions.current.idx + 1 <
                  g_extern.console.screen.resolutions.count)
            {
               g_extern.console.screen.resolutions.current.idx++;
               g_extern.console.screen.resolutions.current.id =
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == MENU_ACTION_OK)
         {
            if (g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx] == CELL_VIDEO_OUT_RESOLUTION_576)
            {
               if (g_extern.console.screen.pal_enable)
                  g_extern.lifecycle_state |= (1ULL<< MODE_VIDEO_PAL_ENABLE);
            }
            else
            {
               g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_ENABLE);
               g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
            }

            rarch_reinit_drivers();
         }
         break;
      case MENU_SETTINGS_VIDEO_PAL60:
         switch (action)
         {
            case MENU_ACTION_LEFT:
            case MENU_ACTION_RIGHT:
            case MENU_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
                     g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  else
                     g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  rarch_reinit_drivers();
               }
               break;
            case MENU_ACTION_START:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  rarch_reinit_drivers();
               }
               break;
         }
         break;
#endif
#ifdef HW_RVL
      case MENU_SETTINGS_VIDEO_SOFT_FILTER:
         if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
            g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         else
            g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
#endif

      case MENU_SETTINGS_VIDEO_HW_SHARED_CONTEXT:
         if ((current_setting = setting_data_find_setting(setting_data, "video_shared_context")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_VSYNC:
         if ((current_setting = setting_data_find_setting(setting_data, "video_vsync")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_HARD_SYNC:
         if ((current_setting = setting_data_find_setting(setting_data, "video_hard_sync")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         if ((current_setting = setting_data_find_setting(setting_data, "video_black_frame_insertion")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_CROP_OVERSCAN:
         if ((current_setting = setting_data_find_setting(setting_data, "video_crop_overscan")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_WINDOW_SCALE_X:
         if ((current_setting = setting_data_find_setting(setting_data, "video_xscale")))
            menu_common_setting_set_current_fraction(current_setting, 1.0f, action, false, false);
         break;
      case MENU_SETTINGS_VIDEO_WINDOW_SCALE_Y:
         if ((current_setting = setting_data_find_setting(setting_data, "video_yscale")))
            menu_common_setting_set_current_fraction(current_setting, 1.0f, action, false, false);
         break;
      case MENU_SETTINGS_VIDEO_THREADED:
         if ((current_setting = setting_data_find_setting(setting_data, "video_threaded")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_VIDEO_SWAP_INTERVAL:
         if ((current_setting = setting_data_find_setting(setting_data, "video_swap_interval")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, true);
         break;
      case MENU_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         if ((current_setting = setting_data_find_setting(setting_data, "video_hard_sync_frames")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, true, true);
         break;

      case MENU_SETTINGS_VIDEO_MONITOR_INDEX:
         if ((current_setting = setting_data_find_setting(setting_data, "video_monitor_index")))
            menu_common_setting_set_current_unsigned_integer(current_setting, 1, action, false, false);
         break;
      case MENU_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
         switch (action)
         {
            case MENU_ACTION_START:
               g_extern.measure_data.frame_time_samples_count = 0;
               break;

            case MENU_ACTION_OK:
            {
               double refresh_rate = 0.0;
               double deviation = 0.0;
               unsigned sample_points = 0;
               if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
               {
                  driver_set_monitor_refresh_rate(refresh_rate);
                  // Incase refresh rate update forced non-block video.
                  video_set_nonblock_state_func(false);
               }
               break;
            }

            default:
               break;
         }
         break;
#ifdef HAVE_SHADER_MANAGER
      case MENU_SETTINGS_SHADER_PASSES:
         {
            struct gfx_shader *shader = driver.menu->shader;

            switch (action)
            {
               case MENU_ACTION_START:
                  if (shader && shader->passes)
                     shader->passes = 0;
                  driver.menu->need_refresh = true;
                  break;

               case MENU_ACTION_LEFT:
                  if (shader && shader->passes)
                  {
                     shader->passes--;
                     driver.menu->need_refresh = true;
                  }
                  break;

               case MENU_ACTION_RIGHT:
               case MENU_ACTION_OK:
                  if (shader && (shader->passes < GFX_MAX_SHADERS))
                  {
                     shader->passes++;
                     driver.menu->need_refresh = true;
                  }
                  break;

               default:
                  break;
            }

            if (driver.menu->need_refresh)
               gfx_shader_resolve_parameters(NULL, driver.menu->shader);
         }
         break;
      case MENU_SETTINGS_SHADER_APPLY:
      {
         struct gfx_shader *shader = driver.menu->shader;
         unsigned type = RARCH_SHADER_NONE;

         if (!driver.video || !driver.video->set_shader || action != MENU_ACTION_OK)
            return 0;

         RARCH_LOG("Applying shader ...\n");

         if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_type)
            type = driver.menu_ctx->backend->shader_manager_get_type(driver.menu->shader);

         if (shader->passes && type != RARCH_SHADER_NONE
               && driver.menu_ctx && driver.menu_ctx->backend &&
               driver.menu_ctx->backend->shader_manager_save_preset)
            driver.menu_ctx->backend->shader_manager_save_preset(NULL, true);
         else
         {
            type = gfx_shader_parse_type("", DEFAULT_SHADER_TYPE);
            if (type == RARCH_SHADER_NONE)
            {
#if defined(HAVE_GLSL)
               type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
               type = RARCH_SHADER_CG;
#endif
            }
            if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_set_preset)
               driver.menu_ctx->backend->shader_manager_set_preset(NULL, type, NULL);
         }
         break;
      }
      case MENU_SETTINGS_SHADER_PRESET_SAVE:
         if (action == MENU_ACTION_OK)
         {
#ifdef HAVE_OSK
            if (g_settings.osk.enable)
            {
               g_extern.osk.cb_init = osk_callback_enter_filename_init;
               g_extern.osk.cb_callback = osk_callback_enter_filename;
            }
            else
#endif
               menu_key_start_line(driver.menu, "Preset Filename: ", preset_filename_callback);
         }
         break;
#endif
#ifdef _XBOX1
      case MENU_SETTINGS_FLICKER_FILTER:
         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (g_extern.console.screen.flicker_filter_index > 0)
                  g_extern.console.screen.flicker_filter_index--;
               break;
            case MENU_ACTION_RIGHT:
               if (g_extern.console.screen.flicker_filter_index < 5)
                  g_extern.console.screen.flicker_filter_index++;
               break;
            case MENU_ACTION_START:
               g_extern.console.screen.flicker_filter_index = 0;
               break;
         }
         break;
      case MENU_SETTINGS_SOFT_DISPLAY_FILTER:
         switch (action)
         {
            case MENU_ACTION_LEFT:
            case MENU_ACTION_RIGHT:
            case MENU_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               else
                  g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
            case MENU_ACTION_START:
               g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
         }
         break;
#endif
      case MENU_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE:
         switch (action)
         {
            case MENU_ACTION_OK:
#if (CELL_SDK_VERSION > 0x340000)
               if (g_extern.lifecycle_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
                  g_extern.lifecycle_state &= ~(1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
               else
                  g_extern.lifecycle_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
               if (g_extern.lifecycle_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
                  cellSysutilEnableBgmPlayback();
               else
                  cellSysutilDisableBgmPlayback();

#endif
               break;
            case MENU_ACTION_START:
#if (CELL_SDK_VERSION > 0x340000)
               g_extern.lifecycle_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
#endif
               break;
         }
         break;
      case MENU_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "pause_nonactive")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_WINDOW_COMPOSITING_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_disable_composition")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_NETPLAY_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_NETPLAY_MODE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_mode")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "netplay_spectator_mode_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
#ifdef HAVE_NETPLAY
      case MENU_SETTINGS_NETPLAY_HOST_IP_ADDRESS:
         if (action == MENU_ACTION_OK)
            menu_key_start_line(driver.menu, "IP Address: ", netplay_ipaddress_callback);
         else if (action == MENU_ACTION_START)
            *g_extern.netplay_server = '\0';
         break;
      case MENU_SETTINGS_NETPLAY_DELAY_FRAMES:
         if (action == MENU_ACTION_LEFT)
         {
            if (g_extern.netplay_sync_frames != 0)
               g_extern.netplay_sync_frames--;
         }
         else if (action == MENU_ACTION_RIGHT)
            g_extern.netplay_sync_frames++;
         else if (action == MENU_ACTION_START)
            g_extern.netplay_sync_frames = 0;
         break;
      case MENU_SETTINGS_NETPLAY_TCP_UDP_PORT:
         if (action == MENU_ACTION_OK)
            menu_key_start_line(driver.menu, "TCP/UDP Port: ", netplay_port_callback);
         else if (action == MENU_ACTION_START)
            g_extern.netplay_port = RARCH_DEFAULT_PORT;
         break;
      case MENU_SETTINGS_NETPLAY_NICKNAME:
         if (action == MENU_ACTION_OK)
            menu_key_start_line(driver.menu, "Nickname: ", netplay_nickname_callback);
         else if (action == MENU_ACTION_START)
            *g_extern.netplay_nick = '\0';
         break;
#endif
      case MENU_SETTINGS_ONSCREEN_KEYBOARD_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "osk_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_PRIVACY_CAMERA_ALLOW:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "camera_allow")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_PRIVACY_LOCATION_ALLOW:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "location_allow")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_FONT_ENABLE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_font_enable")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      case MENU_SETTINGS_FONT_SIZE:
         if ((current_setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_font_size")))
         {
            if (action == MENU_ACTION_LEFT)
               *current_setting->value.fraction = *current_setting->value.fraction - 1.0f;
            else if (action == MENU_ACTION_RIGHT)
               *current_setting->value.fraction = *current_setting->value.fraction + 1.0f;
            else if (action == MENU_ACTION_START)
               *current_setting->value.fraction = font_size;

            *current_setting->value.fraction = roundf(max(*current_setting->value.fraction, 1.0f));

            if (current_setting->change_handler)
               current_setting->change_handler(current_setting);
         }
         break;
      case MENU_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN:
         if ((current_setting = setting_data_find_setting(setting_data, "dummy_on_core_shutdown")))
            menu_common_setting_set_current_boolean(current_setting, action);
         break;
      default:
         break;
   }

   return 0;
}

static void menu_common_setting_set_label_perf(char *type_str, size_t type_str_size, unsigned *w, unsigned type,
      const struct retro_perf_counter **counters, unsigned offset)
{
   if (counters[offset] && counters[offset]->call_cnt)
   {
      snprintf(type_str, type_str_size,
#ifdef _WIN32
            "%I64u ticks, %I64u runs.",
#else
            "%llu ticks, %llu runs.",
#endif
            ((unsigned long long)counters[offset]->total / (unsigned long long)counters[offset]->call_cnt),
            (unsigned long long)counters[offset]->call_cnt);
   }
   else
   {
      *type_str = '\0';
      *w = 0;
   }
}

static void menu_common_setting_set_label(char *type_str, size_t type_str_size, unsigned *w, unsigned type)
{
   if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type, perf_counters_rarch,
            type - MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type, perf_counters_libretro,
            type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   else if (type >= MENU_SETTINGS_BIND_BEGIN && type <= MENU_SETTINGS_BIND_ALL_LAST)
   {
      const struct retro_keybind *auto_bind = input_get_auto_bind(driver.menu->current_pad, type - MENU_SETTINGS_BIND_BEGIN);
      input_get_bind_string(type_str, &g_settings.input.binds[driver.menu->current_pad][type - MENU_SETTINGS_BIND_BEGIN], auto_bind, type_str_size);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_VIDEO_ROTATION:
            strlcpy(type_str, rotation_lut[g_settings.video.rotation],
                  type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_SOFT_FILTER:
            snprintf(type_str, type_str_size,
                  (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
            break;
         case MENU_SETTINGS_VIDEO_FILTER:
            if (g_settings.video.smooth)
               strlcpy(type_str, "Bilinear filtering", type_str_size);
            else
               strlcpy(type_str, "Point filtering", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_GAMMA:
            snprintf(type_str, type_str_size, "%d", g_extern.console.screen.gamma_correction);
            break;
         case MENU_SETTINGS_VIDEO_VSYNC:
            strlcpy(type_str, g_settings.video.vsync ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_HW_SHARED_CONTEXT:
            strlcpy(type_str, g_settings.video.shared_context ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_HARD_SYNC:
            strlcpy(type_str, g_settings.video.hard_sync ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
            strlcpy(type_str, g_settings.video.black_frame_insertion ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_SWAP_INTERVAL:
            snprintf(type_str, type_str_size, "%u", g_settings.video.swap_interval);
            break;
         case MENU_SETTINGS_VIDEO_THREADED:
            strlcpy(type_str, g_settings.video.threaded ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_WINDOW_SCALE_X:
            snprintf(type_str, type_str_size, "%.1fx", g_settings.video.xscale);
            break;
         case MENU_SETTINGS_VIDEO_WINDOW_SCALE_Y:
            snprintf(type_str, type_str_size, "%.1fx", g_settings.video.yscale);
            break;
         case MENU_SETTINGS_VIDEO_CROP_OVERSCAN:
            strlcpy(type_str, g_settings.video.crop_overscan ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
            snprintf(type_str, type_str_size, "%u", g_settings.video.hard_sync_frames);
            break;
         case MENU_SETTINGS_DRIVER_VIDEO:
            strlcpy(type_str, g_settings.video.driver, type_str_size);
            break;
         case MENU_SETTINGS_DRIVER_AUDIO:
            strlcpy(type_str, g_settings.audio.driver, type_str_size);
            break;
         case MENU_SETTINGS_DRIVER_AUDIO_DEVICE:
            strlcpy(type_str, g_settings.audio.device, type_str_size);
            break;
         case MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER:
            strlcpy(type_str, g_settings.audio.resampler, type_str_size);
            break;
         case MENU_SETTINGS_DRIVER_INPUT:
            strlcpy(type_str, g_settings.input.driver, type_str_size);
            break;
#ifdef HAVE_CAMERA
         case MENU_SETTINGS_DRIVER_CAMERA:
            strlcpy(type_str, g_settings.camera.driver, type_str_size);
            break;
#endif
#ifdef HAVE_LOCATION
         case MENU_SETTINGS_DRIVER_LOCATION:
            strlcpy(type_str, g_settings.location.driver, type_str_size);
            break;
#endif
#ifdef HAVE_MENU
         case MENU_SETTINGS_DRIVER_MENU:
            strlcpy(type_str, g_settings.menu.driver, type_str_size);
            break;
#endif
         case MENU_SETTINGS_VIDEO_MONITOR_INDEX:
            if (g_settings.video.monitor_index)
               snprintf(type_str, type_str_size, "%u", g_settings.video.monitor_index);
            else
               strlcpy(type_str, "0 (Auto)", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
            {
               double refresh_rate = 0.0;
               double deviation = 0.0;
               unsigned sample_points = 0;
               if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
                  snprintf(type_str, type_str_size, "%.3f Hz (%.1f%% dev, %u samples)", refresh_rate, 100.0 * deviation, sample_points);
               else
                  strlcpy(type_str, "N/A", type_str_size);
               break;
            }
         case MENU_SETTINGS_VIDEO_INTEGER_SCALE:
            strlcpy(type_str, g_settings.video.scale_integer ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_ASPECT_RATIO:
            strlcpy(type_str, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, type_str_size);
            break;
#if defined(GEKKO)
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            strlcpy(type_str, gx_get_video_mode(), type_str_size);
            break;
#elif defined(__CELLOS_LV2__)
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            {
               unsigned width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               unsigned height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               snprintf(type_str, type_str_size, "%dx%d", width, height);
            }
            break;
         case MENU_SETTINGS_VIDEO_PAL60:
            if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
               strlcpy(type_str, "ON", type_str_size);
            else
               strlcpy(type_str, "OFF", type_str_size);
            break;
#endif
         case MENU_FILE_PLAIN:
            strlcpy(type_str, "(FILE)", type_str_size);
            *w = 6;
            break;
         case MENU_FILE_DIRECTORY:
            strlcpy(type_str, "(DIR)", type_str_size);
            *w = 5;
            break;
         case MENU_SETTINGS_REWIND_ENABLE:
            strlcpy(type_str, g_settings.rewind_enable ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_GPU_SCREENSHOT:
            strlcpy(type_str, g_settings.video.gpu_screenshot ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_REWIND_GRANULARITY:
            snprintf(type_str, type_str_size, "%u", g_settings.rewind_granularity);
            break;
         case MENU_SETTINGS_LIBRETRO_LOG_LEVEL:
            switch(g_settings.libretro_log_level)
            {
               case 0:
                  snprintf(type_str, type_str_size, "0 (Debug)");
                  break;
               case 1:
                  snprintf(type_str, type_str_size, "1 (Info)");
                  break;
               case 2:
                  snprintf(type_str, type_str_size, "2 (Warning)");
                  break;
               case 3:
                  snprintf(type_str, type_str_size, "3 (Error)");
                  break;
            }
            break;
         case MENU_SETTINGS_LOGGING_VERBOSITY:
            strlcpy(type_str, g_extern.verbosity ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_PERFORMANCE_COUNTERS_ENABLE:
            strlcpy(type_str, g_extern.perfcnt_enable ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_CONFIG_SAVE_ON_EXIT:
            strlcpy(type_str, g_extern.config_save_on_exit ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_SAVESTATE_AUTO_SAVE:
            strlcpy(type_str, g_settings.savestate_auto_save ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_SAVESTATE_AUTO_LOAD:
            strlcpy(type_str, g_settings.savestate_auto_load ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_BLOCK_SRAM_OVERWRITE:
            strlcpy(type_str, g_settings.block_sram_overwrite ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_PER_CORE_CONFIG:
            strlcpy(type_str, g_settings.core_specific_config ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_SRAM_AUTOSAVE:
            if (g_settings.autosave_interval)
               snprintf(type_str, type_str_size, "%u seconds", g_settings.autosave_interval);
            else
               strlcpy(type_str, "OFF", type_str_size);
            break;
         case MENU_SETTINGS_SAVESTATE_SAVE:
         case MENU_SETTINGS_SAVESTATE_LOAD:
            if (g_extern.state_slot < 0)
               strlcpy(type_str, "-1 (auto)", type_str_size);
            else
               snprintf(type_str, type_str_size, "%d", g_extern.state_slot);
            break;
         case MENU_SETTINGS_AUDIO_LATENCY:
            snprintf(type_str, type_str_size, "%d ms", g_settings.audio.latency);
            break;
         case MENU_SETTINGS_AUDIO_SYNC:
            strlcpy(type_str, g_settings.audio.sync ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_AUDIO_MUTE:
            strlcpy(type_str, g_extern.audio_data.mute ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
            snprintf(type_str, type_str_size, "%.3f", g_settings.audio.rate_control_delta);
            break;
         case MENU_SETTINGS_FASTFORWARD_RATIO:
            if (g_settings.fastforward_ratio > 0.0f)
               snprintf(type_str, type_str_size, "%.1fx", g_settings.fastforward_ratio);
            else
               snprintf(type_str, type_str_size, "%.1fx (No Limit)", g_settings.fastforward_ratio);
            break;
         case MENU_SETTINGS_SLOWMOTION_RATIO:
            snprintf(type_str, type_str_size, "%.1fx", g_settings.slowmotion_ratio);
            break;
         case MENU_SETTINGS_DEBUG_TEXT:
            snprintf(type_str, type_str_size, (g_settings.fps_show) ? "ON" : "OFF");
            break;
         case MENU_BROWSER_DIR_PATH:
            strlcpy(type_str, *g_settings.menu_content_directory ? g_settings.menu_content_directory : "<default>", type_str_size);
            break;
         case MENU_CONTENT_HISTORY_SIZE:
            snprintf(type_str, type_str_size, "%d", g_settings.game_history_size);
            break;
         case MENU_CONTENT_DIR_PATH:
            strlcpy(type_str, *g_settings.content_directory ? g_settings.content_directory : "<default>", type_str_size);
            break;
         case MENU_ASSETS_DIR_PATH:
            strlcpy(type_str, *g_settings.assets_directory ? g_settings.assets_directory : "<default>", type_str_size);
            break;
         case MENU_SCREENSHOT_DIR_PATH:
            strlcpy(type_str, *g_settings.screenshot_directory ? g_settings.screenshot_directory : "<ROM dir>", type_str_size);
            break;
         case MENU_SAVEFILE_DIR_PATH:
            strlcpy(type_str, *g_extern.savefile_dir ? g_extern.savefile_dir : "<ROM dir>", type_str_size);
            break;
#ifdef HAVE_OVERLAY
         case MENU_OVERLAY_DIR_PATH:
            strlcpy(type_str, *g_extern.overlay_dir ? g_extern.overlay_dir : "<default>", type_str_size);
            break;
#endif
         case MENU_SAVESTATE_DIR_PATH:
            strlcpy(type_str, *g_extern.savestate_dir ? g_extern.savestate_dir : "<ROM dir>", type_str_size);
            break;
         case MENU_LIBRETRO_DIR_PATH:
            strlcpy(type_str, *g_settings.libretro_directory ? g_settings.libretro_directory : "<None>", type_str_size);
            break;
         case MENU_LIBRETRO_INFO_DIR_PATH:
            strlcpy(type_str, *g_settings.libretro_info_path ? g_settings.libretro_info_path : "<Core dir>", type_str_size);
            break;
         case MENU_CONFIG_DIR_PATH:
            strlcpy(type_str, *g_settings.menu_config_directory ? g_settings.menu_config_directory : "<default>", type_str_size);
            break;
         case MENU_FILTER_DIR_PATH:
            strlcpy(type_str, *g_settings.video.filter_dir ? g_settings.video.filter_dir : "<default>", type_str_size);
            break;
         case MENU_DSP_FILTER_DIR_PATH:
            strlcpy(type_str, *g_settings.audio.filter_dir ? g_settings.audio.filter_dir : "<default>", type_str_size);
            break;
         case MENU_SHADER_DIR_PATH:
            strlcpy(type_str, *g_settings.video.shader_dir ? g_settings.video.shader_dir : "<default>", type_str_size);
            break;
         case MENU_SYSTEM_DIR_PATH:
            strlcpy(type_str, *g_settings.system_directory ? g_settings.system_directory : "<ROM dir>", type_str_size);
            break;
         case MENU_AUTOCONFIG_DIR_PATH:
            strlcpy(type_str, *g_settings.input.autoconfig_dir ? g_settings.input.autoconfig_dir : "<default>", type_str_size);
            break;
         case MENU_EXTRACTION_DIR_PATH:
            strlcpy(type_str, *g_settings.extraction_directory ? g_settings.extraction_directory : "<None>", type_str_size);
            break;
         case MENU_SETTINGS_DISK_INDEX:
            {
               const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
               unsigned images = control->get_num_images();
               unsigned current = control->get_image_index();
               if (current >= images)
                  strlcpy(type_str, "No Disk", type_str_size);
               else
                  snprintf(type_str, type_str_size, "%u", current + 1);
               break;
            }
         case MENU_SETTINGS_CONFIG:
            if (*g_extern.config_path)
               fill_pathname_base(type_str, g_extern.config_path, type_str_size);
            else
               strlcpy(type_str, "<default>", type_str_size);
            break;
         case MENU_SETTINGS_OPEN_FILEBROWSER:
         case MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE:
         case MENU_SETTINGS_OPEN_HISTORY:
         case MENU_SETTINGS_CORE_OPTIONS:
         case MENU_SETTINGS_CORE_INFO:
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
         case MENU_SETTINGS_TOGGLE_FULLSCREEN:
         case MENU_SETTINGS_VIDEO_OPTIONS:
         case MENU_SETTINGS_FONT_OPTIONS:
         case MENU_SETTINGS_AUDIO_OPTIONS:
         case MENU_SETTINGS_DISK_OPTIONS:
#ifdef HAVE_SHADER_MANAGER
         case MENU_SETTINGS_SHADER_OPTIONS:
         case MENU_SETTINGS_SHADER_PRESET:
#endif
         case MENU_SETTINGS_GENERAL_OPTIONS:
         case MENU_SETTINGS_SHADER_PRESET_SAVE:
         case MENU_SETTINGS_CORE:
         case MENU_SETTINGS_DISK_APPEND:
         case MENU_SETTINGS_INPUT_OPTIONS:
         case MENU_SETTINGS_PATH_OPTIONS:
         case MENU_SETTINGS_OVERLAY_OPTIONS:
         case MENU_SETTINGS_NETPLAY_OPTIONS:
         case MENU_SETTINGS_PRIVACY_OPTIONS:
         case MENU_SETTINGS_OPTIONS:
         case MENU_SETTINGS_PERFORMANCE_COUNTERS:
         case MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND:
         case MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO:
         case MENU_SETTINGS_DRIVERS:
         case MENU_SETTINGS_CUSTOM_BIND_ALL:
         case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
            strlcpy(type_str, "...", type_str_size);
            break;
         case MENU_SETTINGS_VIDEO_SOFTFILTER:
            {
               const char *filter_name = rarch_softfilter_get_name(g_extern.filter.filter);
               strlcpy(type_str, filter_name ? filter_name : "N/A", type_str_size);
            }
            break;
         case MENU_SETTINGS_AUDIO_DSP_FILTER:
            strlcpy(type_str, path_basename(g_settings.audio.dsp_plugin), type_str_size);
            break;
#ifdef HAVE_OVERLAY
         case MENU_SETTINGS_OVERLAY_PRESET:
            strlcpy(type_str, path_basename(g_settings.input.overlay), type_str_size);
            break;
         case MENU_SETTINGS_OVERLAY_OPACITY:
            snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_opacity);
            break;
         case MENU_SETTINGS_OVERLAY_SCALE:
            snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_scale);
            break;
#endif
         case MENU_SETTINGS_CONTENT_HISTORY_PATH:
            strlcpy(type_str, g_settings.game_history_path ? g_settings.game_history_path : "<None>", type_str_size);
            break;
         case MENU_SETTINGS_BIND_PLAYER:
            snprintf(type_str, type_str_size, "#%d", driver.menu->current_pad + 1);
            break;
         case MENU_SETTINGS_BIND_DEVICE:
            {
               int map = g_settings.input.joypad_map[driver.menu->current_pad];
               if (map >= 0 && map < MAX_PLAYERS)
               {
                  const char *device_name = g_settings.input.device_names[map];
                  if (*device_name)
                     strlcpy(type_str, device_name, type_str_size);
                  else
                     snprintf(type_str, type_str_size, "N/A (port #%u)", map);
               }
               else
                  strlcpy(type_str, "Disabled", type_str_size);
            }
            break;
         case MENU_SETTINGS_BIND_ANALOG_MODE:
            {
               static const char *modes[] = {
                  "None",
                  "Left Analog",
                  "Right Analog",
                  "Dual Analog",
               };

               strlcpy(type_str, modes[g_settings.input.analog_dpad_mode[driver.menu->current_pad] % ANALOG_DPAD_LAST], type_str_size);
            }
            break;
         case MENU_SETTINGS_INPUT_AXIS_THRESHOLD:
            snprintf(type_str, type_str_size, "%.3f", g_settings.input.axis_threshold);
            break;
         case MENU_SETTINGS_BIND_DEVICE_TYPE:
            {
               const struct retro_controller_description *desc = NULL;
               if (driver.menu->current_pad < g_extern.system.num_ports)
               {
                  desc = libretro_find_controller_description(&g_extern.system.ports[driver.menu->current_pad],
                        g_settings.input.libretro_device[driver.menu->current_pad]);
               }

               const char *name = desc ? desc->desc : NULL;
               if (!name) // Find generic name.
               {
                  switch (g_settings.input.libretro_device[driver.menu->current_pad])
                  {
                     case RETRO_DEVICE_NONE:
                        name = "None";
                        break;
                     case RETRO_DEVICE_JOYPAD:
                        name = "Joypad";
                        break;
                     case RETRO_DEVICE_ANALOG:
                        name = "Joypad w/ Analog";
                        break;
                     default:
                        name = "Unknown";
                        break;
                  }
               }

               strlcpy(type_str, name, type_str_size);
            }
            break;
         case MENU_SETTINGS_DEVICE_AUTODETECT_ENABLE:
            strlcpy(type_str, g_settings.input.autodetect_enable ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_CUSTOM_BIND_MODE:
            strlcpy(type_str, driver.menu->bind_mode_keyboard ? "Keyboard" : "Joypad", type_str_size);
            break;
         case MENU_SETTINGS_AUDIO_VOLUME:
            snprintf(type_str, type_str_size, "%.1f dB", g_extern.audio_data.volume_db);
            break;
#ifdef _XBOX1
         case MENU_SETTINGS_FLICKER_FILTER:
            snprintf(type_str, type_str_size, "%d", g_extern.console.screen.flicker_filter_index);
            break;
         case MENU_SETTINGS_SOFT_DISPLAY_FILTER:
            snprintf(type_str, type_str_size,
                  (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
            break;
#endif
         case MENU_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE:
            strlcpy(type_str, (g_extern.lifecycle_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
            strlcpy(type_str, g_settings.pause_nonactive ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_WINDOW_COMPOSITING_ENABLE:
            strlcpy(type_str, g_settings.video.disable_composition ? "OFF" : "ON", type_str_size);
            break;
#ifdef HAVE_NETPLAY
         case MENU_SETTINGS_NETPLAY_ENABLE:
            strlcpy(type_str, g_extern.netplay_enable ? "ON" : "OFF", type_str_size);
            break;
         case MENU_SETTINGS_NETPLAY_HOST_IP_ADDRESS:
            strlcpy(type_str, g_extern.netplay_server, type_str_size);
            break;
         case MENU_SETTINGS_NETPLAY_DELAY_FRAMES:
            snprintf(type_str, type_str_size, "%d", g_extern.netplay_sync_frames);
            break;
         case MENU_SETTINGS_NETPLAY_TCP_UDP_PORT:
            snprintf(type_str, type_str_size, "%d", g_extern.netplay_port ? g_extern.netplay_port : RARCH_DEFAULT_PORT);
            break;
         case MENU_SETTINGS_NETPLAY_NICKNAME:
            snprintf(type_str, type_str_size, "%s", g_extern.netplay_nick);
            break;
         case MENU_SETTINGS_NETPLAY_MODE:
            snprintf(type_str, type_str_size, g_extern.netplay_is_client ? "ON" : "OFF");
            break;
         case MENU_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(type_str, type_str_size, g_extern.netplay_is_spectate ? "ON" : "OFF");
            break;
#endif
#ifdef HAVE_CAMERA
         case MENU_SETTINGS_PRIVACY_CAMERA_ALLOW:
            snprintf(type_str, type_str_size, g_settings.camera.allow ? "ON" : "OFF");
            break;
#endif
#ifdef HAVE_LOCATION
         case MENU_SETTINGS_PRIVACY_LOCATION_ALLOW:
            snprintf(type_str, type_str_size, g_settings.location.allow ? "ON" : "OFF");
            break;
#endif
#ifdef HAVE_OSK
         case MENU_SETTINGS_ONSCREEN_KEYBOARD_ENABLE:
            snprintf(type_str, type_str_size, g_settings.osk.enable ? "ON" : "OFF");
            break;
#endif
         case MENU_SETTINGS_FONT_ENABLE:
            snprintf(type_str, type_str_size, g_settings.video.font_enable ? "ON" : "OFF");
            break;
         case MENU_SETTINGS_FONT_SIZE:
            snprintf(type_str, type_str_size, "%.1f", g_settings.video.font_size);
            break;
         case MENU_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(type_str, type_str_size, g_settings.load_dummy_on_core_shutdown ? "ON" : "OFF");
            break;
         default:
            *type_str = '\0';
            *w = 0;
            break;
      }
   }
}

const menu_ctx_driver_backend_t menu_ctx_backend_common = {
   menu_common_entries_init,
   menu_common_iterate,
   menu_common_shader_manager_init,
   menu_common_shader_manager_get_str,
   menu_common_shader_manager_set_preset,
   menu_common_shader_manager_save_preset,
   menu_common_shader_manager_get_type,
   menu_common_shader_manager_setting_toggle,
   menu_common_type_is,
   menu_common_core_setting_toggle,
   menu_common_setting_toggle,
   menu_common_setting_set,
   menu_common_setting_set_label,
   menu_common_defer_decision_automatic,
   menu_common_defer_decision_manual,
   "menu_common",
};
