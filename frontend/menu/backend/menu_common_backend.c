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

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif
#endif

static unsigned info_selection_ptr = 0;

#ifdef HAVE_SHADER_MANAGER
static inline struct gfx_shader *shader_manager_get_current_shader(void *data, unsigned type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
   {
      RARCH_ERR("Cannot get current shader, menu handle is not initialized.\n");
      return NULL;
   }

   struct gfx_shader *shader = type == RGUI_SETTINGS_SHADER_PRESET_PARAMETERS ? &rgui->shader : NULL;
   if (!shader && driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      shader = driver.video_poke->get_current_shader(driver.video_data);

   return shader;
}
#endif

static void menu_common_entries_init(void *data, unsigned menu_type)
{
   unsigned i, last;
   char tmp[256];
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   switch (menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case RGUI_SETTINGS_SHADER_PARAMETERS:
      case RGUI_SETTINGS_SHADER_PRESET_PARAMETERS:
      {
         file_list_clear(rgui->selection_buf);

         struct gfx_shader *shader = shader_manager_get_current_shader(rgui, menu_type);
         if (shader)
            for (i = 0; i < shader->num_parameters; i++)
               file_list_push(rgui->selection_buf, shader->parameters[i].desc, RGUI_SETTINGS_SHADER_PARAMETER_0 + i, 0);
         rgui->parameter_shader = shader;
         break;
      }
      case RGUI_SETTINGS_SHADER_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Apply Shader Changes",
               RGUI_SETTINGS_SHADER_APPLY, 0);
         file_list_push(rgui->selection_buf, "Default Filter", RGUI_SETTINGS_SHADER_FILTER, 0);
         file_list_push(rgui->selection_buf, "Load Shader Preset",
               RGUI_SETTINGS_SHADER_PRESET, 0);
         file_list_push(rgui->selection_buf, "Save As Shader Preset",
               RGUI_SETTINGS_SHADER_PRESET_SAVE, 0);
         file_list_push(rgui->selection_buf, "Parameters (Current)",
               RGUI_SETTINGS_SHADER_PARAMETERS, 0);
         file_list_push(rgui->selection_buf, "Parameters (RGUI)",
               RGUI_SETTINGS_SHADER_PRESET_PARAMETERS, 0);
         file_list_push(rgui->selection_buf, "Shader Passes",
               RGUI_SETTINGS_SHADER_PASSES, 0);

         for (i = 0; i < rgui->shader.passes; i++)
         {
            char buf[64];

            snprintf(buf, sizeof(buf), "Shader #%u", i);
            file_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0 + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
            file_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0_FILTER + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
            file_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0_SCALE + 3 * i, 0);
         }
         break;
#endif
      case RGUI_SETTINGS_GENERAL_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Libretro Logging Level", RGUI_SETTINGS_LIBRETRO_LOG_LEVEL, 0);
         file_list_push(rgui->selection_buf, "Logging Verbosity", RGUI_SETTINGS_LOGGING_VERBOSITY, 0);
         file_list_push(rgui->selection_buf, "Configuration Save On Exit", RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT, 0);
         file_list_push(rgui->selection_buf, "Configuration Per-Core", RGUI_SETTINGS_PER_CORE_CONFIG, 0);
#ifdef HAVE_SCREENSHOTS
         file_list_push(rgui->selection_buf, "GPU Screenshots", RGUI_SETTINGS_GPU_SCREENSHOT, 0);
#endif
         file_list_push(rgui->selection_buf, "Load Dummy On Core Shutdown", RGUI_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN, 0);
         file_list_push(rgui->selection_buf, "Show Framerate", RGUI_SETTINGS_DEBUG_TEXT, 0);
         file_list_push(rgui->selection_buf, "Maximum Run Speed", RGUI_SETTINGS_FASTFORWARD_RATIO, 0);
         file_list_push(rgui->selection_buf, "Slow-Motion Ratio", RGUI_SETTINGS_SLOWMOTION_RATIO, 0);
         file_list_push(rgui->selection_buf, "Rewind", RGUI_SETTINGS_REWIND_ENABLE, 0);
         file_list_push(rgui->selection_buf, "Rewind Granularity", RGUI_SETTINGS_REWIND_GRANULARITY, 0);
         file_list_push(rgui->selection_buf, "SRAM Block Overwrite", RGUI_SETTINGS_BLOCK_SRAM_OVERWRITE, 0);
#if defined(HAVE_THREADS)
         file_list_push(rgui->selection_buf, "SRAM Autosave", RGUI_SETTINGS_SRAM_AUTOSAVE, 0);
#endif
         file_list_push(rgui->selection_buf, "Window Compositing", RGUI_SETTINGS_WINDOW_COMPOSITING_ENABLE, 0);
         file_list_push(rgui->selection_buf, "Window Unfocus Pause", RGUI_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST, 0);
         file_list_push(rgui->selection_buf, "Savestate Autosave On Exit", RGUI_SETTINGS_SAVESTATE_AUTO_SAVE, 0);
         file_list_push(rgui->selection_buf, "Savestate Autoload", RGUI_SETTINGS_SAVESTATE_AUTO_LOAD, 0);
         break;
      case RGUI_SETTINGS_VIDEO_OPTIONS:
         file_list_clear(rgui->selection_buf);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         file_list_push(rgui->selection_buf, "Screen Resolution", RGUI_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         file_list_push(rgui->selection_buf, "Soft Filter", RGUI_SETTINGS_VIDEO_SOFTFILTER, 0);
#if defined(__CELLOS_LV2__)
         file_list_push(rgui->selection_buf, "PAL60 Mode", RGUI_SETTINGS_VIDEO_PAL60, 0);
#endif
#ifndef HAVE_SHADER_MANAGER
         file_list_push(rgui->selection_buf, "Default Filter", RGUI_SETTINGS_VIDEO_FILTER, 0);
#endif
#ifdef HW_RVL
         file_list_push(rgui->selection_buf, "VI Trap filtering", RGUI_SETTINGS_VIDEO_SOFT_FILTER, 0);
#endif
#if defined(HW_RVL) || defined(_XBOX360)
         file_list_push(rgui->selection_buf, "Gamma", RGUI_SETTINGS_VIDEO_GAMMA, 0);
#endif
#ifdef _XBOX1
         file_list_push(rgui->selection_buf, "Soft filtering", RGUI_SETTINGS_SOFT_DISPLAY_FILTER, 0);
         file_list_push(rgui->selection_buf, "Flicker filtering", RGUI_SETTINGS_FLICKER_FILTER, 0);
#endif
         file_list_push(rgui->selection_buf, "Integer Scale", RGUI_SETTINGS_VIDEO_INTEGER_SCALE, 0);
         file_list_push(rgui->selection_buf, "Aspect Ratio", RGUI_SETTINGS_VIDEO_ASPECT_RATIO, 0);
         file_list_push(rgui->selection_buf, "Custom Ratio", RGUI_SETTINGS_CUSTOM_VIEWPORT, 0);
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         file_list_push(rgui->selection_buf, "Toggle Fullscreen", RGUI_SETTINGS_TOGGLE_FULLSCREEN, 0);
#endif
         file_list_push(rgui->selection_buf, "Rotation", RGUI_SETTINGS_VIDEO_ROTATION, 0);
         file_list_push(rgui->selection_buf, "VSync", RGUI_SETTINGS_VIDEO_VSYNC, 0);
         file_list_push(rgui->selection_buf, "Hard GPU Sync", RGUI_SETTINGS_VIDEO_HARD_SYNC, 0);
         file_list_push(rgui->selection_buf, "Hard GPU Sync Frames", RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES, 0);
#if !defined(RARCH_MOBILE)
         file_list_push(rgui->selection_buf, "Black Frame Insertion", RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION, 0);
#endif
         file_list_push(rgui->selection_buf, "VSync Swap Interval", RGUI_SETTINGS_VIDEO_SWAP_INTERVAL, 0);
#if defined(HAVE_THREADS) && !defined(GEKKO)
         file_list_push(rgui->selection_buf, "Threaded Driver", RGUI_SETTINGS_VIDEO_THREADED, 0);
#endif
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         file_list_push(rgui->selection_buf, "Windowed Scale (X)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X, 0);
         file_list_push(rgui->selection_buf, "Windowed Scale (Y)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y, 0);
#endif
         file_list_push(rgui->selection_buf, "Crop Overscan (reload)", RGUI_SETTINGS_VIDEO_CROP_OVERSCAN, 0);
         file_list_push(rgui->selection_buf, "Monitor Index", RGUI_SETTINGS_VIDEO_MONITOR_INDEX, 0);
         file_list_push(rgui->selection_buf, "Estimated Monitor FPS", RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO, 0);
         break;
      case RGUI_SETTINGS_FONT_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "OSD Font Enable", RGUI_SETTINGS_FONT_ENABLE, 0);
         file_list_push(rgui->selection_buf, "OSD Font Scale to Window", RGUI_SETTINGS_FONT_SCALE, 0);
         file_list_push(rgui->selection_buf, "OSD Font Size", RGUI_SETTINGS_FONT_SIZE, 0);
         break;
      case RGUI_SETTINGS_CORE_OPTIONS:
         file_list_clear(rgui->selection_buf);

         if (g_extern.system.core_options)
         {
            size_t i, opts;

            opts = core_option_size(g_extern.system.core_options);
            for (i = 0; i < opts; i++)
               file_list_push(rgui->selection_buf,
                     core_option_get_desc(g_extern.system.core_options, i), RGUI_SETTINGS_CORE_OPTION_START + i, 0);
         }
         else
            file_list_push(rgui->selection_buf, "No options available.", RGUI_SETTINGS_CORE_OPTION_NONE, 0);
         break;
      case RGUI_SETTINGS_CORE_INFO:
         file_list_clear(rgui->selection_buf);
         if (rgui->core_info_current.data)
         {
            snprintf(tmp, sizeof(tmp), "Core name: %s",
                  rgui->core_info_current.display_name ? rgui->core_info_current.display_name : "");
            file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);

            if (rgui->core_info_current.authors_list)
            {
               strlcpy(tmp, "Authors: ", sizeof(tmp));
               string_list_join_concat(tmp, sizeof(tmp), rgui->core_info_current.authors_list, ", ");
               file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);
            }

            if (rgui->core_info_current.permissions_list)
            {
               strlcpy(tmp, "Permissions: ", sizeof(tmp));
               string_list_join_concat(tmp, sizeof(tmp), rgui->core_info_current.permissions_list, ", ");
               file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);
            }

            if (rgui->core_info_current.supported_extensions_list)
            {
               strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
               string_list_join_concat(tmp, sizeof(tmp), rgui->core_info_current.supported_extensions_list, ", ");
               file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);
            }

            if (rgui->core_info_current.firmware_count > 0)
            {
               core_info_list_update_missing_firmware(rgui->core_info, rgui->core_info_current.path,
                     g_settings.system_directory);

               file_list_push(rgui->selection_buf, "Firmware: ", RGUI_SETTINGS_CORE_INFO_NONE, 0);
               for (i = 0; i < rgui->core_info_current.firmware_count; i++)
               {
                  if (rgui->core_info_current.firmware[i].desc)
                  {
                     snprintf(tmp, sizeof(tmp), "	name: %s",
                           rgui->core_info_current.firmware[i].desc ? rgui->core_info_current.firmware[i].desc : "");
                     file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);

                     snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                           rgui->core_info_current.firmware[i].missing ? "missing" : "present",
                           rgui->core_info_current.firmware[i].optional ? "optional" : "required");
                     file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);
                  }
               }
            }

            if (rgui->core_info_current.notes)
            {
               snprintf(tmp, sizeof(tmp), "Core notes: ");
               file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);

               for (i = 0; i < rgui->core_info_current.note_list->size; i++)
               {
                  snprintf(tmp, sizeof(tmp), " %s", rgui->core_info_current.note_list->elems[i].data);
                  file_list_push(rgui->selection_buf, tmp, RGUI_SETTINGS_CORE_INFO_NONE, 0);
               }
            }
         }
         else
            file_list_push(rgui->selection_buf, "No information available.", RGUI_SETTINGS_CORE_OPTION_NONE, 0);
         break;
      case RGUI_SETTINGS_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "General Options", RGUI_SETTINGS_GENERAL_OPTIONS, 0);
         file_list_push(rgui->selection_buf, "Video Options", RGUI_SETTINGS_VIDEO_OPTIONS, 0);
#ifdef HAVE_SHADER_MANAGER
         file_list_push(rgui->selection_buf, "Shader Options", RGUI_SETTINGS_SHADER_OPTIONS, 0);
#endif
         file_list_push(rgui->selection_buf, "Font Options", RGUI_SETTINGS_FONT_OPTIONS, 0);
         file_list_push(rgui->selection_buf, "Audio Options", RGUI_SETTINGS_AUDIO_OPTIONS, 0);
         file_list_push(rgui->selection_buf, "Input Options", RGUI_SETTINGS_INPUT_OPTIONS, 0);
#ifdef HAVE_OVERLAY
         file_list_push(rgui->selection_buf, "Overlay Options", RGUI_SETTINGS_OVERLAY_OPTIONS, 0);
#endif
#ifdef HAVE_NETPLAY
         file_list_push(rgui->selection_buf, "Netplay Options", RGUI_SETTINGS_NETPLAY_OPTIONS, 0);
#endif
         file_list_push(rgui->selection_buf, "Path Options", RGUI_SETTINGS_PATH_OPTIONS, 0);
         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            if (g_extern.system.disk_control.get_num_images)
               file_list_push(rgui->selection_buf, "Disk Options", RGUI_SETTINGS_DISK_OPTIONS, 0);
         }
         file_list_push(rgui->selection_buf, "Privacy Options", RGUI_SETTINGS_PRIVACY_OPTIONS, 0);
         break;
      case RGUI_SETTINGS_PRIVACY_OPTIONS:
         file_list_clear(rgui->selection_buf);
#ifdef HAVE_CAMERA
         file_list_push(rgui->selection_buf, "Allow Camera", RGUI_SETTINGS_PRIVACY_CAMERA_ALLOW, 0);
#endif
#ifdef HAVE_LOCATION
         file_list_push(rgui->selection_buf, "Allow Location Services", RGUI_SETTINGS_PRIVACY_LOCATION_ALLOW, 0);
#endif
         break;
      case RGUI_SETTINGS_DISK_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Disk Index", RGUI_SETTINGS_DISK_INDEX, 0);
         file_list_push(rgui->selection_buf, "Disk Image Append", RGUI_SETTINGS_DISK_APPEND, 0);
         break;
      case RGUI_SETTINGS_OVERLAY_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Overlay Preset", RGUI_SETTINGS_OVERLAY_PRESET, 0);
         file_list_push(rgui->selection_buf, "Overlay Opacity", RGUI_SETTINGS_OVERLAY_OPACITY, 0);
         file_list_push(rgui->selection_buf, "Overlay Scale", RGUI_SETTINGS_OVERLAY_SCALE, 0);
         break;
#ifdef HAVE_NETPLAY
      case RGUI_SETTINGS_NETPLAY_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Netplay Enable", RGUI_SETTINGS_NETPLAY_ENABLE, 0);
         file_list_push(rgui->selection_buf, "Netplay Mode", RGUI_SETTINGS_NETPLAY_MODE, 0);
         file_list_push(rgui->selection_buf, "Spectator Mode Enable", RGUI_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE, 0);
         file_list_push(rgui->selection_buf, "Host IP Address", RGUI_SETTINGS_NETPLAY_HOST_IP_ADDRESS, 0);
         file_list_push(rgui->selection_buf, "TCP/UDP Port", RGUI_SETTINGS_NETPLAY_TCP_UDP_PORT, 0);
         file_list_push(rgui->selection_buf, "Delay Frames", RGUI_SETTINGS_NETPLAY_DELAY_FRAMES, 0);
         file_list_push(rgui->selection_buf, "Nickname", RGUI_SETTINGS_NETPLAY_NICKNAME, 0);
         break;
#endif
      case RGUI_SETTINGS_PATH_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Browser Directory", RGUI_BROWSER_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Content Directory", RGUI_CONTENT_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Assets Directory", RGUI_ASSETS_DIR_PATH, 0);
#ifdef HAVE_DYNAMIC
         file_list_push(rgui->selection_buf, "Config Directory", RGUI_CONFIG_DIR_PATH, 0);
#endif
         file_list_push(rgui->selection_buf, "Core Directory", RGUI_LIBRETRO_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Core Info Directory", RGUI_LIBRETRO_INFO_DIR_PATH, 0);
#ifdef HAVE_DYLIB
         file_list_push(rgui->selection_buf, "Soft Filter Directory", RGUI_FILTER_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "DSP Filter Directory", RGUI_DSP_FILTER_DIR_PATH, 0);
#endif
#ifdef HAVE_SHADER_MANAGER
         file_list_push(rgui->selection_buf, "Shader Directory", RGUI_SHADER_DIR_PATH, 0);
#endif
         file_list_push(rgui->selection_buf, "Savestate Directory", RGUI_SAVESTATE_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Savefile Directory", RGUI_SAVEFILE_DIR_PATH, 0);
#ifdef HAVE_OVERLAY
         file_list_push(rgui->selection_buf, "Overlay Directory", RGUI_OVERLAY_DIR_PATH, 0);
#endif
         file_list_push(rgui->selection_buf, "System Directory", RGUI_SYSTEM_DIR_PATH, 0);
#ifdef HAVE_SCREENSHOTS
         file_list_push(rgui->selection_buf, "Screenshot Directory", RGUI_SCREENSHOT_DIR_PATH, 0);
#endif
         file_list_push(rgui->selection_buf, "Autoconfig Directory", RGUI_AUTOCONFIG_DIR_PATH, 0);
         break;
      case RGUI_SETTINGS_INPUT_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Player", RGUI_SETTINGS_BIND_PLAYER, 0);
         file_list_push(rgui->selection_buf, "Device", RGUI_SETTINGS_BIND_DEVICE, 0);
         file_list_push(rgui->selection_buf, "Device Type", RGUI_SETTINGS_BIND_DEVICE_TYPE, 0);
         file_list_push(rgui->selection_buf, "Analog D-pad Mode", RGUI_SETTINGS_BIND_ANALOG_MODE, 0);
         file_list_push(rgui->selection_buf, "Input Axis Threshold", RGUI_SETTINGS_INPUT_AXIS_THRESHOLD, 0);
         file_list_push(rgui->selection_buf, "Autodetect Enable", RGUI_SETTINGS_DEVICE_AUTODETECT_ENABLE, 0);

         file_list_push(rgui->selection_buf, "Bind Mode", RGUI_SETTINGS_CUSTOM_BIND_MODE, 0);
         file_list_push(rgui->selection_buf, "Configure All (RetroPad)", RGUI_SETTINGS_CUSTOM_BIND_ALL, 0);
         file_list_push(rgui->selection_buf, "Default All (RetroPad)", RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
#ifdef HAVE_OSK
         file_list_push(rgui->selection_buf, "Onscreen Keyboard Enable", RGUI_SETTINGS_ONSCREEN_KEYBOARD_ENABLE, 0);
#endif
         last = (driver.input && driver.input->set_keybinds && !driver.input->get_joypad_driver) ? RGUI_SETTINGS_BIND_R3 : RGUI_SETTINGS_BIND_MENU_TOGGLE;
         for (i = RGUI_SETTINGS_BIND_BEGIN; i <= last; i++)
            file_list_push(rgui->selection_buf, input_config_bind_map[i - RGUI_SETTINGS_BIND_BEGIN].desc, i, 0);
         break;
      case RGUI_SETTINGS_AUDIO_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "DSP Filter", RGUI_SETTINGS_AUDIO_DSP_FILTER, 0);
         file_list_push(rgui->selection_buf, "Audio Mute", RGUI_SETTINGS_AUDIO_MUTE, 0);
         file_list_push(rgui->selection_buf, "Rate Control Delta", RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA, 0);
#ifdef __CELLOS_LV2__
         file_list_push(rgui->selection_buf, "System BGM Control", RGUI_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE, 0);
#endif
#ifdef _XBOX1
         file_list_push(rgui->selection_buf, "Volume Effect", RGUI_SETTINGS_AUDIO_DSP_EFFECT, 0);
#endif
         file_list_push(rgui->selection_buf, "Volume Level", RGUI_SETTINGS_AUDIO_VOLUME, 0);
         break;
      case RGUI_SETTINGS_DRIVERS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Video Driver", RGUI_SETTINGS_DRIVER_VIDEO, 0);
         file_list_push(rgui->selection_buf, "Audio Driver", RGUI_SETTINGS_DRIVER_AUDIO, 0);
         file_list_push(rgui->selection_buf, "Audio Device", RGUI_SETTINGS_DRIVER_AUDIO_DEVICE, 0);
         file_list_push(rgui->selection_buf, "Audio Resampler", RGUI_SETTINGS_DRIVER_AUDIO_RESAMPLER, 0);
         file_list_push(rgui->selection_buf, "Input Driver", RGUI_SETTINGS_DRIVER_INPUT, 0);
#ifdef HAVE_CAMERA
         file_list_push(rgui->selection_buf, "Camera Driver", RGUI_SETTINGS_DRIVER_CAMERA, 0);
#endif
#ifdef HAVE_LOCATION
         file_list_push(rgui->selection_buf, "Location Driver", RGUI_SETTINGS_DRIVER_LOCATION, 0);
#endif
#ifdef HAVE_MENU
         file_list_push(rgui->selection_buf, "Menu Driver", RGUI_SETTINGS_DRIVER_MENU, 0);
#endif
         break;
      case RGUI_SETTINGS:
         file_list_clear(rgui->selection_buf);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
         file_list_push(rgui->selection_buf, "Core", RGUI_SETTINGS_CORE, 0);
#endif
         if (rgui->history)
            file_list_push(rgui->selection_buf, "Load Content (History)", RGUI_SETTINGS_OPEN_HISTORY, 0);

         if (rgui->core_info && core_info_list_num_info_files(rgui->core_info))
            file_list_push(rgui->selection_buf, "Load Content (Detect Core)", RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, 0);

         if (rgui->info.library_name || g_extern.system.info.library_name)
         {
            char load_game_core_msg[64];
            snprintf(load_game_core_msg, sizeof(load_game_core_msg), "Load Content (%s)",
                  rgui->info.library_name ? rgui->info.library_name : g_extern.system.info.library_name);
            file_list_push(rgui->selection_buf, load_game_core_msg, RGUI_SETTINGS_OPEN_FILEBROWSER, 0);
         }

         file_list_push(rgui->selection_buf, "Core Options", RGUI_SETTINGS_CORE_OPTIONS, 0);
         file_list_push(rgui->selection_buf, "Core Information", RGUI_SETTINGS_CORE_INFO, 0);
         file_list_push(rgui->selection_buf, "Settings", RGUI_SETTINGS_OPTIONS, 0);
         file_list_push(rgui->selection_buf, "Drivers", RGUI_SETTINGS_DRIVERS, 0);

         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            file_list_push(rgui->selection_buf, "Save State", RGUI_SETTINGS_SAVESTATE_SAVE, 0);
            file_list_push(rgui->selection_buf, "Load State", RGUI_SETTINGS_SAVESTATE_LOAD, 0);
#ifdef HAVE_SCREENSHOTS
            file_list_push(rgui->selection_buf, "Take Screenshot", RGUI_SETTINGS_SCREENSHOT, 0);
#endif
            file_list_push(rgui->selection_buf, "Resume Content", RGUI_SETTINGS_RESUME_GAME, 0);
            file_list_push(rgui->selection_buf, "Restart Content", RGUI_SETTINGS_RESTART_GAME, 0);

         }
#ifndef HAVE_DYNAMIC
         file_list_push(rgui->selection_buf, "Restart RetroArch", RGUI_SETTINGS_RESTART_EMULATOR, 0);
#endif
         file_list_push(rgui->selection_buf, "RetroArch Config", RGUI_SETTINGS_CONFIG, 0);
         file_list_push(rgui->selection_buf, "Save New Config", RGUI_SETTINGS_SAVE_CONFIG, 0);
         file_list_push(rgui->selection_buf, "Help", RGUI_START_SCREEN, 0);
         file_list_push(rgui->selection_buf, "Quit RetroArch", RGUI_SETTINGS_QUIT_RARCH, 0);
         break;
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(rgui, menu_type);
}

static int menu_info_screen_iterate(unsigned action)
{
   unsigned i;
   char msg[1024];
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
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
         input_get_bind_string(desc[i], bind, sizeof(desc[i]));
      }
   }

   switch(info_selection_ptr)
   {
      case RGUI_SETTINGS_WINDOW_COMPOSITING_ENABLE:
         snprintf(msg, sizeof(msg),
               "-- Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case RGUI_SETTINGS_LIBRETRO_LOG_LEVEL:
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
      case RGUI_SETTINGS_LOGGING_VERBOSITY:
         snprintf(msg, sizeof(msg),
               "-- Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case RGUI_SYSTEM_DIR_PATH:
         snprintf(msg, sizeof(msg),
               "-- Sets the 'system' directory.\n"
               "Implementations can query for this\n"
               "directory to load BIOSes, system-\n"
               "specific configs, etc.");
         break;
      case RGUI_START_SCREEN:
         snprintf(msg, sizeof(msg),
               " -- Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Config Save On Exit' is set to true.\n");
         break;
      case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
         snprintf(msg, sizeof(msg),
               " -- Flushes config to disk on exit.\n"
               "Useful for menu as settings can be\n"
               "modified. Overwrites the config.\n"
               " \n"
               "#include's and comments are not pre-\n"
               "served.\n");
         break;
      case RGUI_SETTINGS_PER_CORE_CONFIG:
         snprintf(msg, sizeof(msg),
               " -- Load up a specific config file \n"
               "based on the core being used.\n");
         break;
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
         snprintf(msg, sizeof(msg),
               " -- Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case RGUI_SETTINGS_VIDEO_VSYNC:
         snprintf(msg, sizeof(msg),
               " -- Video V-Sync.\n");
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC:
         snprintf(msg, sizeof(msg),
               " -- Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
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
      case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(msg, sizeof(msg),
               " -- Inserts a black frame inbetween \n"
               "frames.\n"
               " \n"
               "Useful for 120 Hz monitors who want to \n"
               "play 60 Hz material with eliminated \n"
               "ghosting.\n"
               " \n"
               "Video refresh rate should still be con-\n"
               "figured as if it is a 60 Hz monitor \n"
               "(divide refresh rate by 2).");
         break;
      case RGUI_SETTINGS_VIDEO_THREADED:
         snprintf(msg, sizeof(msg),
               " -- Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
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
      case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
         snprintf(msg, sizeof(msg),
               " -- Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is core-\n"
               "implementation specific.");
         break;
      case RGUI_SETTINGS_VIDEO_MONITOR_INDEX:
         snprintf(msg, sizeof(msg),
               " -- Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case RGUI_SETTINGS_VIDEO_ROTATION:
         snprintf(msg, sizeof(msg),
               " -- Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
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
      case RGUI_SETTINGS_AUDIO_VOLUME:
         snprintf(msg, sizeof(msg),
               " -- Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
         break;
      case RGUI_SETTINGS_VIDEO_SOFTFILTER:
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
      case RGUI_SETTINGS_BLOCK_SRAM_OVERWRITE:
         snprintf(msg, sizeof(msg),
               " -- Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case RGUI_SETTINGS_PRIVACY_CAMERA_ALLOW:
         snprintf(msg, sizeof(msg),
               " -- Allow or disallow camera access by \n"
               "cores.");
         break;
      case RGUI_SETTINGS_PRIVACY_LOCATION_ALLOW:
         snprintf(msg, sizeof(msg),
               " -- Allow or disallow location services \n"
               "access by cores.");
         break;
      case RGUI_SETTINGS_REWIND_ENABLE:
         snprintf(msg, sizeof(msg),
               " -- Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case RGUI_SETTINGS_REWIND_GRANULARITY:
         snprintf(msg, sizeof(msg),
               " -- Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case RGUI_SETTINGS_DEVICE_AUTODETECT_ENABLE:
         snprintf(msg, sizeof(msg),
               " -- Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case RGUI_SETTINGS_INPUT_AXIS_THRESHOLD:
         snprintf(msg, sizeof(msg),
               " -- Defines axis threshold.\n"
               " \n"
               " Possible values are [0.0, 1.0].");
         break;
      case RGUI_SETTINGS_DRIVER_INPUT:
         snprintf(msg, sizeof(msg),
               " -- Input driver.\n"
               " \n"
               "Depending on video driver, it might \n"
               "force a different input driver.");
         break;
      case RGUI_SETTINGS_AUDIO_DSP_FILTER:
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
      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
         snprintf(msg, sizeof(msg),
               " -- Toggles fullscreen.");
         break;
      case RGUI_SETTINGS_SLOWMOTION_RATIO:
         snprintf(msg, sizeof(msg),
               " -- Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case RGUI_SETTINGS_FASTFORWARD_RATIO:
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
      case RGUI_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
         snprintf(msg, sizeof(msg),
               " -- Pause gameplay when window focus \n"
               "is lost.");
         break;
      case RGUI_SETTINGS_GPU_SCREENSHOT:
         snprintf(msg, sizeof(msg),
               " -- Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
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
      case RGUI_SCREENSHOT_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Directory to dump screenshots to.");
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO_DEVICE:
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
      case RGUI_ASSETS_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Assets directory. \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case RGUI_SETTINGS_SAVESTATE_AUTO_SAVE:
         snprintf(msg, sizeof(msg),
               " -- Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Savestate Auto\n"
               "Load' is set.");
         break;
      case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
         snprintf(msg, sizeof(msg),
               " -- VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
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
      case RGUI_LIBRETRO_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case RGUI_SAVEFILE_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case RGUI_SAVESTATE_DIR_PATH:
         snprintf(msg, sizeof(msg),
               " -- Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
         snprintf(msg, sizeof(msg),
               " -- Axis for analog stick (DualShock-esque.\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case RGUI_SETTINGS_BIND_SHADER_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Applies next shader in directory.");
         break;
      case RGUI_SETTINGS_BIND_SHADER_PREV:
         snprintf(msg, sizeof(msg),
               " -- Applies previous shader in directory.");
         break;
      case RGUI_SETTINGS_BIND_LOAD_STATE_KEY:
         snprintf(msg, sizeof(msg),
               " -- Loads state.");
         break;
      case RGUI_SETTINGS_BIND_SAVE_STATE_KEY:
         snprintf(msg, sizeof(msg),
               " -- Saves state.");
         break;
      case RGUI_SETTINGS_BIND_STATE_SLOT_PLUS:
      case RGUI_SETTINGS_BIND_STATE_SLOT_MINUS:
         snprintf(msg, sizeof(msg),
               " -- State slots.\n"
               " \n"
               " With slot set to 0, save state name is *.state \n"
               " (or whatever defined on commandline).\n"
               "When slot is != 0, path will be (path)(d), \n"
               "where (d) is slot number.");
         break;
      case RGUI_SETTINGS_BIND_TURBO_ENABLE:
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
      case RGUI_SETTINGS_BIND_FAST_FORWARD_HOLD_KEY:
         snprintf(msg, sizeof(msg),
               " -- Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case RGUI_SETTINGS_BIND_QUIT_KEY:
         snprintf(msg, sizeof(msg),
               " -- Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nKilling it in any hard way (SIGTERM, SIGKILL, \n"
               "etc) will terminate without saving RAM, etc."
#endif
               );
         break;
      case RGUI_SETTINGS_BIND_REWIND:
         snprintf(msg, sizeof(msg),
               " -- Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
         break;
      case RGUI_SETTINGS_BIND_MOVIE_RECORD_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle between recording and not.");
         break;
      case RGUI_SETTINGS_BIND_PAUSE_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle between paused and non-paused state.");
         break;
      case RGUI_SETTINGS_BIND_FRAMEADVANCE:
         snprintf(msg, sizeof(msg),
               " -- Frame advance when content is paused.");
         break;
      case RGUI_SETTINGS_BIND_RESET:
         snprintf(msg, sizeof(msg),
               " -- Reset the content.\n");
         break;
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_PLUS:
         snprintf(msg, sizeof(msg),
               " -- Increment cheat index.\n");
         break;
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_MINUS:
         snprintf(msg, sizeof(msg),
               " -- Decrement cheat index.\n");
         break;
      case RGUI_SETTINGS_BIND_CHEAT_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggle cheat index.\n");
         break;
      case RGUI_SETTINGS_BIND_SCREENSHOT:
         snprintf(msg, sizeof(msg),
               " -- Take screenshot.");
         break;
      case RGUI_SETTINGS_BIND_MUTE:
         snprintf(msg, sizeof(msg),
               " -- Mute/unmute audio.");
         break;
      case RGUI_SETTINGS_BIND_NETPLAY_FLIP:
         snprintf(msg, sizeof(msg),
               " -- Netplay flip players.");
         break;
      case RGUI_SETTINGS_BIND_SLOWMOTION:
         snprintf(msg, sizeof(msg),
               " -- Hold for slowmotion.");
         break;
      case RGUI_SETTINGS_BIND_ENABLE_HOTKEY:
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
      case RGUI_SETTINGS_BIND_VOLUME_UP:
         snprintf(msg, sizeof(msg),
               " -- Increases audio volume.");
         break;
      case RGUI_SETTINGS_BIND_VOLUME_DOWN:
         snprintf(msg, sizeof(msg),
               " -- Decreases audio volume.");
         break;
      case RGUI_SETTINGS_BIND_OVERLAY_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Toggles to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case RGUI_SETTINGS_BIND_DISK_EJECT_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
         break;
      case RGUI_SETTINGS_BIND_DISK_NEXT:
         snprintf(msg, sizeof(msg),
               " -- Cycles through disk images. Use after \n"
               "ejecting. \n"
               " \n"
               " Complete by toggling eject again.");
         break;
      case RGUI_SETTINGS_BIND_GRAB_MOUSE_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles mouse grab.\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
         break;
      case RGUI_SETTINGS_BIND_MENU_TOGGLE:
         snprintf(msg, sizeof(msg),
               " -- Toggles menu.");
         break;
      default:
         snprintf(msg, sizeof(msg),
               "-- No info on this item available. --\n");
   }

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == RGUI_ACTION_OK)
      file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
   return 0;
}

static int menu_start_screen_iterate(unsigned action)
{
   unsigned i;
   char msg[1024];
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
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
         input_get_bind_string(desc[i], bind, sizeof(desc[i]));
      }
   }

   snprintf(msg, sizeof(msg),
         "-- Welcome to RetroArch / RGUI --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic RGUI controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "           Info: %-20s\n"
         "Enter/Exit RGUI: %-20s\n"
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

   if (action == RGUI_ACTION_OK)
      file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
   return 0;
}

static unsigned menu_common_type_is(unsigned type)
{
   unsigned ret = 0;
   bool type_found;

   type_found =
      type == RGUI_SETTINGS ||
      type == RGUI_SETTINGS_GENERAL_OPTIONS ||
      type == RGUI_SETTINGS_CORE_OPTIONS ||
      type == RGUI_SETTINGS_CORE_INFO ||
      type == RGUI_SETTINGS_VIDEO_OPTIONS ||
      type == RGUI_SETTINGS_FONT_OPTIONS ||
      type == RGUI_SETTINGS_SHADER_OPTIONS ||
      type == RGUI_SETTINGS_SHADER_PARAMETERS ||
      type == RGUI_SETTINGS_SHADER_PRESET_PARAMETERS ||
      type == RGUI_SETTINGS_AUDIO_OPTIONS ||
      type == RGUI_SETTINGS_DISK_OPTIONS ||
      type == RGUI_SETTINGS_PATH_OPTIONS ||
      type == RGUI_SETTINGS_PRIVACY_OPTIONS ||
      type == RGUI_SETTINGS_OVERLAY_OPTIONS ||
      type == RGUI_SETTINGS_NETPLAY_OPTIONS ||
      type == RGUI_SETTINGS_OPTIONS ||
      type == RGUI_SETTINGS_DRIVERS ||
      (type == RGUI_SETTINGS_INPUT_OPTIONS);

   if (type_found)
   {
      ret = RGUI_SETTINGS;
      return ret;
   }

   type_found = (type >= RGUI_SETTINGS_SHADER_0 &&
         type <= RGUI_SETTINGS_SHADER_LAST &&
         ((type - RGUI_SETTINGS_SHADER_0) % 3) == 0) ||
      type == RGUI_SETTINGS_SHADER_PRESET;

   if (type_found)
   {
      ret = RGUI_SETTINGS_SHADER_OPTIONS;
      return ret;
   }

   type_found =
      type == RGUI_BROWSER_DIR_PATH ||
      type == RGUI_CONTENT_DIR_PATH ||
      type == RGUI_ASSETS_DIR_PATH ||
      type == RGUI_SHADER_DIR_PATH ||
      type == RGUI_FILTER_DIR_PATH ||
      type == RGUI_DSP_FILTER_DIR_PATH ||
      type == RGUI_SAVESTATE_DIR_PATH ||
      type == RGUI_LIBRETRO_DIR_PATH ||
      type == RGUI_LIBRETRO_INFO_DIR_PATH ||
      type == RGUI_CONFIG_DIR_PATH ||
      type == RGUI_SAVEFILE_DIR_PATH ||
      type == RGUI_OVERLAY_DIR_PATH ||
      type == RGUI_SCREENSHOT_DIR_PATH ||
      type == RGUI_AUTOCONFIG_DIR_PATH ||
      type == RGUI_SYSTEM_DIR_PATH;

   if (type_found)
   {
      ret = RGUI_FILE_DIRECTORY;
      return ret;
   }

   return ret;
}

static int menu_settings_iterate(unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;
   
   if (!rgui)
      return 0;

   rgui->frame_buf_pitch = rgui->width * 2;
   unsigned type = 0;
   const char *label = NULL;
   if (action != RGUI_ACTION_REFRESH)
      file_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &label, &type);

   if (type == RGUI_SETTINGS_CORE)
      label = g_settings.libretro_directory;
   else if (type == RGUI_SETTINGS_CONFIG)
      label = g_settings.rgui_config_directory;
   else if (type == RGUI_SETTINGS_DISK_APPEND)
      label = g_settings.rgui_content_directory;

   const char *dir = NULL;
   unsigned menu_type = 0;
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   rgui = (rgui_handle_t*)driver.menu;

   if (rgui->need_refresh)
      action = RGUI_ACTION_NOOP;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr > 0)
            menu_decrement_navigation(rgui);
         else
            menu_set_navigation(rgui, rgui->selection_buf->size - 1);
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + 1 < rgui->selection_buf->size)
            menu_increment_navigation(rgui);
         else
            menu_clear_navigation(rgui);
         break;

      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
            rgui->need_refresh = true;
         }
         break;
      case RGUI_ACTION_SELECT:
         {
            const char *path = NULL;
            file_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &path, &info_selection_ptr);
            file_list_push(rgui->menu_stack, "", RGUI_INFO_SCREEN, 0);
         }
         break;
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if ((type == RGUI_SETTINGS_OPEN_FILEBROWSER || type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE)
               && action == RGUI_ACTION_OK)
         {
            rgui->defer_core = type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE;
            file_list_push(rgui->menu_stack, g_settings.rgui_content_directory, RGUI_FILE_DIRECTORY, rgui->selection_ptr);
            menu_clear_navigation(rgui);
            rgui->need_refresh = true;
         }
         else if ((type == RGUI_SETTINGS_OPEN_HISTORY || menu_common_type_is(type) == RGUI_FILE_DIRECTORY) && action == RGUI_ACTION_OK)
         {
            file_list_push(rgui->menu_stack, "", type, rgui->selection_ptr);
            menu_clear_navigation(rgui);
            rgui->need_refresh = true;
         }
         else if ((menu_common_type_is(type) == RGUI_SETTINGS || type == RGUI_SETTINGS_CORE || type == RGUI_SETTINGS_CONFIG || type == RGUI_SETTINGS_DISK_APPEND) && action == RGUI_ACTION_OK)
         {
            file_list_push(rgui->menu_stack, label, type, rgui->selection_ptr);
            menu_clear_navigation(rgui);
            rgui->need_refresh = true;
         }
         else if (type == RGUI_SETTINGS_CUSTOM_VIEWPORT && action == RGUI_ACTION_OK)
         {
            file_list_push(rgui->menu_stack, "", type, rgui->selection_ptr);

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

      case RGUI_ACTION_REFRESH:
         menu_clear_navigation(rgui);
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   rgui = (rgui_handle_t*)driver.menu;

   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui && rgui->need_refresh && !(menu_type == RGUI_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == RGUI_FILE_DIRECTORY ||
            menu_type == RGUI_SETTINGS_VIDEO_SOFTFILTER ||
            menu_type == RGUI_SETTINGS_AUDIO_DSP_FILTER ||
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY))
   {
      rgui->need_refresh = false;
      if (
               menu_type == RGUI_SETTINGS_INPUT_OPTIONS
            || menu_type == RGUI_SETTINGS_PATH_OPTIONS
            || menu_type == RGUI_SETTINGS_OVERLAY_OPTIONS
            || menu_type == RGUI_SETTINGS_NETPLAY_OPTIONS
            || menu_type == RGUI_SETTINGS_OPTIONS
            || menu_type == RGUI_SETTINGS_DRIVERS
            || menu_type == RGUI_SETTINGS_CORE_INFO
            || menu_type == RGUI_SETTINGS_CORE_OPTIONS
            || menu_type == RGUI_SETTINGS_AUDIO_OPTIONS
            || menu_type == RGUI_SETTINGS_DISK_OPTIONS
            || menu_type == RGUI_SETTINGS_PRIVACY_OPTIONS
            || menu_type == RGUI_SETTINGS_GENERAL_OPTIONS
            || menu_type == RGUI_SETTINGS_VIDEO_OPTIONS
            || menu_type == RGUI_SETTINGS_FONT_OPTIONS
            || menu_type == RGUI_SETTINGS_SHADER_OPTIONS
            || menu_type == RGUI_SETTINGS_SHADER_PARAMETERS
            || menu_type == RGUI_SETTINGS_SHADER_PRESET_PARAMETERS
            )
         menu_common_entries_init(driver.menu, menu_type);
      else
         menu_common_entries_init(driver.menu, RGUI_SETTINGS);
   }

   if (driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   // Have to defer it so we let settings refresh.
   if (rgui->push_start_screen)
   {
      rgui->push_start_screen = false;
      file_list_push(rgui->menu_stack, "", RGUI_START_SCREEN, 0);
   }

   return 0;
}

static int menu_viewport_iterate(unsigned action)
{
   rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
      return 0;

   unsigned menu_type = 0;
   file_list_get_last(rgui->menu_stack, NULL, &menu_type);

   struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   int stride_x = g_settings.video.scale_integer ?
      geom->base_width : 1;
   int stride_y = g_settings.video.scale_integer ?
      geom->base_height : 1;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_DOWN:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
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

      case RGUI_ACTION_LEFT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_RIGHT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
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

      case RGUI_ACTION_CANCEL:
         file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
         {
            file_list_push(rgui->menu_stack, "",
                  RGUI_SETTINGS_CUSTOM_VIEWPORT,
                  rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_OK:
         file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            file_list_push(rgui->menu_stack, "",
                  RGUI_SETTINGS_CUSTOM_VIEWPORT_2,
                  rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            rarch_viewport_t vp;

            if (driver.video_data && driver.video && driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, &vp);

            if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
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

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   file_list_get_last(rgui->menu_stack, NULL, &menu_type);

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
      if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
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
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
   {
      RARCH_ERR("Cannot parse and resolve menu, menu handle is not initialized.\n");
      return;
   }

   dir = NULL;

   file_list_clear(rgui->selection_buf);

   // parsing switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_OPEN_HISTORY:
         /* History parse */
         list_size = rom_history_size(rgui->history);

         for (i = 0; i < list_size; i++)
         {
            const char *path, *core_path, *core_name;
            char fill_buf[PATH_MAX];

            path = NULL;
            core_path = NULL;
            core_name = NULL;

            rom_history_get_index(rgui->history, i,
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

            file_list_push(rgui->selection_buf, fill_buf, RGUI_FILE_PLAIN, 0);
         }
         break;
      case RGUI_SETTINGS_DEFERRED_CORE:
         break;
      default:
         {
            /* Directory parse */
            file_list_get_last(rgui->menu_stack, &dir, &menu_type);

            if (!*dir)
            {
#if defined(GEKKO)
#ifdef HW_RVL
               file_list_push(rgui->selection_buf, "sd:/", menu_type, 0);
               file_list_push(rgui->selection_buf, "usb:/", menu_type, 0);
#endif
               file_list_push(rgui->selection_buf, "carda:/", menu_type, 0);
               file_list_push(rgui->selection_buf, "cardb:/", menu_type, 0);
#elif defined(_XBOX1)
               file_list_push(rgui->selection_buf, "C:", menu_type, 0);
               file_list_push(rgui->selection_buf, "D:", menu_type, 0);
               file_list_push(rgui->selection_buf, "E:", menu_type, 0);
               file_list_push(rgui->selection_buf, "F:", menu_type, 0);
               file_list_push(rgui->selection_buf, "G:", menu_type, 0);
#elif defined(_XBOX360)
               file_list_push(rgui->selection_buf, "game:", menu_type, 0);
#elif defined(_WIN32)
               unsigned drives = GetLogicalDrives();
               char drive[] = " :\\";
               for (i = 0; i < 32; i++)
               {
                  drive[0] = 'A' + i;
                  if (drives & (1 << i))
                     file_list_push(rgui->selection_buf, drive, menu_type, 0);
               }
#elif defined(__CELLOS_LV2__)
               file_list_push(rgui->selection_buf, "/app_home/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_hdd0/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_hdd1/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/host_root/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb000/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb001/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb002/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb003/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb004/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb005/", menu_type, 0);
               file_list_push(rgui->selection_buf, "/dev_usb006/", menu_type, 0);
#elif defined(PSP)
               file_list_push(rgui->selection_buf, "ms0:/", menu_type, 0);
               file_list_push(rgui->selection_buf, "ef0:/", menu_type, 0);
               file_list_push(rgui->selection_buf, "host0:/", menu_type, 0);
#else
               file_list_push(rgui->selection_buf, "/", menu_type, 0);
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
            if (menu_type == RGUI_SETTINGS_CORE)
               exts = EXT_EXECUTABLES;
            else if (menu_type == RGUI_SETTINGS_CONFIG)
               exts = "cfg";
            else if (menu_type == RGUI_SETTINGS_SHADER_PRESET)
               exts = "cgp|glslp";
            else if (menu_common_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
               exts = "cg|glsl";
            else if (menu_type == RGUI_SETTINGS_VIDEO_SOFTFILTER)
               exts = EXT_EXECUTABLES;
            else if (menu_type == RGUI_SETTINGS_AUDIO_DSP_FILTER)
               exts = "dsp";
            else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
               exts = "cfg";
            else if (menu_common_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               exts = ""; // we ignore files anyway
            else if (rgui->defer_core)
               exts = rgui->core_info ? core_info_list_get_all_extensions(rgui->core_info) : "";
            else if (rgui->info.valid_extensions)
            {
               exts = ext_buf;
               if (*rgui->info.valid_extensions)
                  snprintf(ext_buf, sizeof(ext_buf), "%s|zip", rgui->info.valid_extensions);
               else
                  *ext_buf = '\0';
            }
            else
               exts = g_extern.system.valid_extensions;

            struct string_list *list = dir_list_new(dir, exts, true);
            if (!list)
               return;

            dir_list_sort(list, true);

            if (menu_common_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               file_list_push(rgui->selection_buf, "<Use this directory>", RGUI_FILE_USE_DIRECTORY, 0);

            for (i = 0; i < list->size; i++)
            {
               bool is_dir = list->elems[i].attr.b;

               if ((menu_common_type_is(menu_type) == RGUI_FILE_DIRECTORY) && !is_dir)
                  continue;

               // Need to preserve slash first time.
               const char *path = list->elems[i].data;
               if (*dir)
                  path = path_basename(path);

#ifdef HAVE_LIBRETRO_MANAGEMENT
               if (menu_type == RGUI_SETTINGS_CORE && (is_dir || strcasecmp(path, SALAMANDER_FILE) == 0))
                  continue;
#endif

               // Push menu_type further down in the chain.
               // Needed for shader manager currently.
               file_list_push(rgui->selection_buf, path,
                     is_dir ? menu_type : RGUI_FILE_PLAIN, 0);
            }

            if (driver.menu_ctx && driver.menu_ctx->backend->entries_init)
               driver.menu_ctx->backend->entries_init(rgui, menu_type);

            string_list_free(list);
         }
   }

   // resolving switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_CORE:
         dir = NULL;
         list = (file_list_t*)rgui->selection_buf;
         file_list_get_last(rgui->menu_stack, &dir, &menu_type);
         list_size = list->size;
         for (i = 0; i < list_size; i++)
         {
            const char *path;
            unsigned type = 0;
            file_list_get_at_offset(list, i, &path, &type);
            if (type != RGUI_FILE_PLAIN)
               continue;

            char core_path[PATH_MAX];
            fill_pathname_join(core_path, dir, path, sizeof(core_path));

            char display_name[256];
            if (rgui->core_info &&
                  core_info_list_get_display_name(rgui->core_info,
                     core_path, display_name, sizeof(display_name)))
               file_list_set_alt_at_offset(list, i, display_name);
         }
         file_list_sort_on_alt(rgui->selection_buf);
         break;
      case RGUI_SETTINGS_DEFERRED_CORE:
         core_info_list_get_supported_cores(rgui->core_info, rgui->deferred_path, &info, &list_size);
         for (i = 0; i < list_size; i++)
         {
            file_list_push(rgui->selection_buf, info[i].path, RGUI_FILE_PLAIN, 0);
            file_list_set_alt_at_offset(rgui->selection_buf, i, info[i].display_name);
         }
         file_list_sort_on_alt(rgui->selection_buf);
         break;
      default:
         (void)0;
   }

   rgui->scroll_indices_size = 0;
   if (menu_type != RGUI_SETTINGS_OPEN_HISTORY)
      menu_build_scroll_indices(rgui->selection_buf);

   // Before a refresh, we could have deleted a file on disk, causing
   // selection_ptr to suddendly be out of range. Ensure it doesn't overflow.
   if (rgui->selection_ptr >= rgui->selection_buf->size && rgui->selection_buf->size)
      menu_set_navigation(rgui, rgui->selection_buf->size - 1);
   else if (!rgui->selection_buf->size)
      menu_clear_navigation(rgui);
}

// This only makes sense for PC so far.
// Consoles use set_keybind callbacks instead.
static int menu_custom_bind_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   (void)action; // Have to ignore action here. Only bind that should work here is Quit RetroArch or something like that.

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   char msg[256];
   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)", input_config_bind_map[rgui->binds.begin - RGUI_SETTINGS_BIND_BEGIN].desc);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   struct rgui_bind_state binds = rgui->binds;
   menu_poll_bind_state(&binds);

   if ((binds.skip && !rgui->binds.skip) || menu_poll_find_trigger(&rgui->binds, &binds))
   {
      binds.begin++;
      if (binds.begin <= binds.last)
         binds.target++;
      else
         file_list_pop(rgui->menu_stack, &rgui->selection_ptr);

      // Avoid new binds triggering things right away.
      rgui->trigger_state = 0;
      rgui->old_input_state = -1ULL;
   }
   rgui->binds = binds;
   return 0;
}

static int menu_custom_bind_iterate_keyboard(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   (void)action; // Have to ignore action here.

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   int64_t current = rarch_get_time_usec();
   int timeout = (rgui->binds.timeout_end - current) / 1000000;

   char msg[256];
   snprintf(msg, sizeof(msg), "[%s]\npress keyboard\n(timeout %d seconds)",
         input_config_bind_map[rgui->binds.begin - RGUI_SETTINGS_BIND_BEGIN].desc, timeout);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   bool timed_out = false;
   if (timeout <= 0)
   {
      rgui->binds.begin++;
      rgui->binds.target->key = RETROK_UNKNOWN; // Could be unsafe, but whatever.
      rgui->binds.target++;
      rgui->binds.timeout_end = rarch_get_time_usec() + RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      timed_out = true;
   }

   // binds.begin is updated in keyboard_press callback.
   if (rgui->binds.begin > rgui->binds.last)
   {
      file_list_pop(rgui->menu_stack, &rgui->selection_ptr);

      // Avoid new binds triggering things right away.
      rgui->trigger_state = 0;
      rgui->old_input_state = -1ULL;

      // We won't be getting any key events, so just cancel early.
      if (timed_out)
         input_keyboard_wait_keys_cancel();
   }
   return 0;
}

static void menu_common_defer_decision_automatic(void)
{
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
      return;

   menu_flush_stack_type(RGUI_SETTINGS);
   rgui->msg_force = true;
}

static void menu_common_defer_decision_manual(void)
{
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
      return;

   file_list_push(rgui->menu_stack, g_settings.libretro_directory, RGUI_SETTINGS_DEFERRED_CORE, rgui->selection_ptr);
   menu_clear_navigation(rgui);
   rgui->need_refresh = true;
}

static int menu_common_iterate(unsigned action)
{
   int ret = 0;
   const char *dir = 0;
   unsigned menu_type = 0;
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
   {
      RARCH_ERR("Cannot iterate menu, menu handle is not initialized.\n");
      return 0;
   }

   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(rgui, false);

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

   if (menu_type == RGUI_START_SCREEN)
      return menu_start_screen_iterate(action);
   else if (menu_type == RGUI_INFO_SCREEN)
      return menu_info_screen_iterate(action);
   else if (menu_common_type_is(menu_type) == RGUI_SETTINGS)
      return menu_settings_iterate(action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
      return menu_viewport_iterate(action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_BIND)
      return menu_custom_bind_iterate(driver.menu, action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_BIND_KEYBOARD)
      return menu_custom_bind_iterate_keyboard(driver.menu, action);

   if (rgui->need_refresh && action != RGUI_ACTION_MESSAGE)
      action = RGUI_ACTION_NOOP;

   unsigned scroll_speed = (max(rgui->scroll_accel, 2) - 2) / 4 + 1;
   unsigned fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr >= scroll_speed)
            menu_set_navigation(rgui, rgui->selection_ptr - scroll_speed);
         else
            menu_set_navigation(rgui, rgui->selection_buf->size - 1);
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + scroll_speed < rgui->selection_buf->size)
            menu_set_navigation(rgui, rgui->selection_ptr + scroll_speed);
         else
            menu_clear_navigation(rgui);
         break;

      case RGUI_ACTION_LEFT:
         if (rgui->selection_ptr > fast_scroll_speed)
            menu_set_navigation(rgui, rgui->selection_ptr - fast_scroll_speed);
         else
            menu_clear_navigation(rgui);
         break;

      case RGUI_ACTION_RIGHT:
         if (rgui->selection_ptr + fast_scroll_speed < rgui->selection_buf->size)
            menu_set_navigation(rgui, rgui->selection_ptr + fast_scroll_speed);
         else
            menu_set_navigation_last(rgui);
         break;

      case RGUI_ACTION_SCROLL_UP:
         menu_descend_alphabet(rgui, &rgui->selection_ptr);
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         menu_ascend_alphabet(rgui, &rgui->selection_ptr);
         break;

      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_OK:
      {
         if (rgui->selection_buf->size == 0)
            return 0;

         const char *path = 0;
         unsigned type = 0;
         file_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &path, &type);

         if (
               menu_common_type_is(type) == RGUI_SETTINGS_SHADER_OPTIONS ||
               menu_common_type_is(type) == RGUI_FILE_DIRECTORY ||
               type == RGUI_SETTINGS_OVERLAY_PRESET ||
               type == RGUI_SETTINGS_VIDEO_SOFTFILTER ||
               type == RGUI_SETTINGS_AUDIO_DSP_FILTER ||
               type == RGUI_SETTINGS_CORE ||
               type == RGUI_SETTINGS_CONFIG ||
               type == RGUI_SETTINGS_DISK_APPEND ||
               type == RGUI_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            fill_pathname_join(cat_path, dir, path, sizeof(cat_path));

            file_list_push(rgui->menu_stack, cat_path, type, rgui->selection_ptr);
            menu_clear_navigation(rgui);
            rgui->need_refresh = true;
         }
         else
         {
#ifdef HAVE_SHADER_MANAGER
            if (menu_common_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
            {
               if (menu_type == RGUI_SETTINGS_SHADER_PRESET)
               {
                  char shader_path[PATH_MAX];
                  fill_pathname_join(shader_path, dir, path, sizeof(shader_path));
                  if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_set_preset)
                     driver.menu_ctx->backend->shader_manager_set_preset(&rgui->shader, gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
                        shader_path);
               }
               else
               {
                  unsigned pass = (menu_type - RGUI_SETTINGS_SHADER_0) / 3;
                  fill_pathname_join(rgui->shader.pass[pass].source.path,
                        dir, path, sizeof(rgui->shader.pass[pass].source.path));

                  // This will reset any changed parameters.
                  gfx_shader_resolve_parameters(NULL, &rgui->shader);
               }

               // Pop stack until we hit shader manager again.
               menu_flush_stack_type(RGUI_SETTINGS_SHADER_OPTIONS);
            }
            else
#endif
            if (menu_type == RGUI_SETTINGS_DEFERRED_CORE)
            {
               // FIXME: Add for consoles.
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               strlcpy(g_extern.fullpath, rgui->deferred_path, sizeof(g_extern.fullpath));
               load_menu_game_new_core();
               rgui->msg_force = true;
               ret = -1;
               menu_flush_stack_type(RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_CORE)
            {
#if defined(HAVE_DYNAMIC)
               fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
               menu_update_system_info(driver.menu, &rgui->load_no_rom);

               // No ROM needed for this core, load game immediately.
               if (rgui->load_no_rom)
               {
                  g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
                  *g_extern.fullpath = '\0';
                  rgui->msg_force = true;
                  ret = -1;
               }

               // Core selection on non-console just updates directory listing.
               // Will take affect on new ROM load.
#elif defined(RARCH_CONSOLE)
               rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)path);

#if defined(GEKKO) && defined(HW_RVL)
               fill_pathname_join(g_extern.fullpath, default_paths.core_dir,
                     SALAMANDER_FILE, sizeof(g_extern.fullpath));
#else
               fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
#endif
               g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
               g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
               ret = -1;
#endif

               menu_flush_stack_type(RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_CONFIG)
            {
               char config[PATH_MAX];
               fill_pathname_join(config, dir, path, sizeof(config));
               menu_flush_stack_type(RGUI_SETTINGS);
               rgui->msg_force = true;
               if (menu_replace_config(config))
               {
                  menu_clear_navigation(rgui);
                  ret = -1;
               }
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
            {
               fill_pathname_join(g_settings.input.overlay, dir, path, sizeof(g_settings.input.overlay));

               if (driver.overlay)
                  input_overlay_free(driver.overlay);
               driver.overlay = input_overlay_new(g_settings.input.overlay);
               if (!driver.overlay)
                  RARCH_ERR("Failed to load overlay.\n");

               menu_flush_stack_type(RGUI_SETTINGS_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
            {
               char image[PATH_MAX];
               fill_pathname_join(image, dir, path, sizeof(image));
               rarch_disk_control_append_image(image);

               g_extern.lifecycle_state |= 1ULL << MODE_GAME;

               menu_flush_stack_type(RGUI_SETTINGS);
               ret = -1;
            }
            else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
            {
               load_menu_game_history(rgui->selection_ptr);
               menu_flush_stack_type(RGUI_SETTINGS);
               ret = -1;
            }
            else if (menu_type == RGUI_BROWSER_DIR_PATH)
            {
               strlcpy(g_settings.rgui_content_directory, dir, sizeof(g_settings.rgui_content_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_CONTENT_DIR_PATH)
            {
               strlcpy(g_settings.content_directory, dir, sizeof(g_settings.content_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_ASSETS_DIR_PATH)
            {
               strlcpy(g_settings.assets_directory, dir, sizeof(g_settings.assets_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_SCREENSHOTS
            else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
            {
               strlcpy(g_settings.screenshot_directory, dir, sizeof(g_settings.screenshot_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
            {
               strlcpy(g_extern.savefile_dir, dir, sizeof(g_extern.savefile_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == RGUI_OVERLAY_DIR_PATH)
            {
               strlcpy(g_extern.overlay_dir, dir, sizeof(g_extern.overlay_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SETTINGS_VIDEO_SOFTFILTER)
            {
               fill_pathname_join(g_settings.video.filter_path, dir, path, sizeof(g_settings.video.filter_path));
               rarch_deinit_filter();
               rarch_init_filter(g_extern.system.pix_fmt);
               menu_flush_stack_type(RGUI_SETTINGS_VIDEO_OPTIONS);
            }
            else if (menu_type == RGUI_SETTINGS_AUDIO_DSP_FILTER)
            {
#ifdef HAVE_DYLIB
               fill_pathname_join(g_settings.audio.dsp_plugin, dir, path, sizeof(g_settings.audio.dsp_plugin));
#endif
               rarch_deinit_dsp_filter();
               rarch_init_dsp_filter();
               menu_flush_stack_type(RGUI_SETTINGS_AUDIO_OPTIONS);
            }
            else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
            {
               strlcpy(g_extern.savestate_dir, dir, sizeof(g_extern.savestate_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_directory, dir, sizeof(g_settings.libretro_directory));

               if (driver.menu_ctx && driver.menu_ctx->init_core_info)
                  driver.menu_ctx->init_core_info(rgui);
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_DYNAMIC
            else if (menu_type == RGUI_CONFIG_DIR_PATH)
            {
               strlcpy(g_settings.rgui_config_directory, dir, sizeof(g_settings.rgui_config_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_LIBRETRO_INFO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_info_path, dir, sizeof(g_settings.libretro_info_path));
               if (driver.menu_ctx && driver.menu_ctx->init_core_info)
                  driver.menu_ctx->init_core_info(rgui);
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SHADER_DIR_PATH)
            {
               strlcpy(g_settings.video.shader_dir, dir, sizeof(g_settings.video.shader_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_FILTER_DIR_PATH)
            {
               strlcpy(g_settings.video.filter_dir, dir, sizeof(g_settings.video.filter_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_DSP_FILTER_DIR_PATH)
            {
               strlcpy(g_settings.audio.filter_dir, dir, sizeof(g_settings.audio.filter_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SYSTEM_DIR_PATH)
            {
               strlcpy(g_settings.system_directory, dir, sizeof(g_settings.system_directory));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_AUTOCONFIG_DIR_PATH)
            {
               strlcpy(g_settings.input.autoconfig_dir, dir, sizeof(g_settings.input.autoconfig_dir));
               menu_flush_stack_type(RGUI_SETTINGS_PATH_OPTIONS);
            }
            else
            {
               if (rgui->defer_core)
               {
                  ret = menu_defer_core(rgui->core_info, dir, path, rgui->deferred_path, sizeof(rgui->deferred_path));

                  if (ret == -1)
                  {
                     menu_update_system_info(driver.menu, &rgui->load_no_rom);
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

                  menu_flush_stack_type(RGUI_SETTINGS);
                  rgui->msg_force = true;
                  ret = -1;
               }
            }
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         menu_clear_navigation(rgui);
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }


   // refresh values in case the stack changed
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh && (menu_type == RGUI_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == RGUI_FILE_DIRECTORY ||
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
            menu_type == RGUI_SETTINGS_VIDEO_SOFTFILTER ||
            menu_type == RGUI_SETTINGS_AUDIO_DSP_FILTER ||
            menu_type == RGUI_SETTINGS_DEFERRED_CORE ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY ||
            menu_type == RGUI_SETTINGS_DISK_APPEND))
   {
      rgui->need_refresh = false;
      menu_parse_and_resolve(menu_type);
   }

   if (driver.menu_ctx && driver.menu_ctx->iterate)
      driver.menu_ctx->iterate(rgui, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   return ret;
}

static void menu_common_shader_manager_init(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (!rgui)
      return;

#ifdef HAVE_SHADER_MANAGER
   memset(&rgui->shader, 0, sizeof(rgui->shader));
   config_file_t *conf = NULL;

   const char *config_path = NULL;
   if (*g_extern.core_specific_config_path && g_settings.core_specific_config)
      config_path = g_extern.core_specific_config_path;
   else if (*g_extern.config_path)
      config_path = g_extern.config_path;

   // In a multi-config setting, we can't have conflicts on rgui.cgp/rgui.glslp.
   if (config_path)
   {
      fill_pathname_base(rgui->default_glslp, config_path, sizeof(rgui->default_glslp));
      path_remove_extension(rgui->default_glslp);
      strlcat(rgui->default_glslp, ".glslp", sizeof(rgui->default_glslp));
      fill_pathname_base(rgui->default_cgp, config_path, sizeof(rgui->default_cgp));
      path_remove_extension(rgui->default_cgp);
      strlcat(rgui->default_cgp, ".cgp", sizeof(rgui->default_cgp));
   }
   else
   {
      strlcpy(rgui->default_glslp, "rgui.glslp", sizeof(rgui->default_glslp));
      strlcpy(rgui->default_cgp, "rgui.cgp", sizeof(rgui->default_cgp));
   }

   char cgp_path[PATH_MAX];

   const char *ext = path_get_extension(g_settings.video.shader_path);
   if (strcmp(ext, "glslp") == 0 || strcmp(ext, "cgp") == 0)
   {
      conf = config_file_new(g_settings.video.shader_path);
      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, &rgui->shader))
         {
            gfx_shader_resolve_relative(&rgui->shader, g_settings.video.shader_path);
            gfx_shader_resolve_parameters(conf, &rgui->shader);
         }
         config_file_free(conf);
      }
   }
   else if (strcmp(ext, "glsl") == 0 || strcmp(ext, "cg") == 0)
   {
      strlcpy(rgui->shader.pass[0].source.path, g_settings.video.shader_path,
            sizeof(rgui->shader.pass[0].source.path));
      rgui->shader.passes = 1;
   }
   else
   {
      const char *shader_dir = *g_settings.video.shader_dir ?
         g_settings.video.shader_dir : g_settings.system_directory;

      fill_pathname_join(cgp_path, shader_dir, "rgui.glslp", sizeof(cgp_path));
      conf = config_file_new(cgp_path);

      if (!conf)
      {
         fill_pathname_join(cgp_path, shader_dir, "rgui.cgp", sizeof(cgp_path));
         conf = config_file_new(cgp_path);
      }

      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, &rgui->shader))
         {
            gfx_shader_resolve_relative(&rgui->shader, cgp_path);
            gfx_shader_resolve_parameters(conf, &rgui->shader);
         }
         config_file_free(conf);
      }
   }
#endif
}

static void menu_common_shader_manager_set_preset(void *data, unsigned type, const char *path)
{
#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader *shader = (struct gfx_shader*)data;

   RARCH_LOG("Setting RGUI shader: %s.\n", path ? path : "N/A (stock)");

   if (video_set_shader_func((enum rarch_shader_type)type, path))
   {
      // Makes sure that we use RGUI CGP shader on driver reinit.
      // Only do this when the cgp actually works to avoid potential errors.
      strlcpy(g_settings.video.shader_path, path ? path : "",
            sizeof(g_settings.video.shader_path));
      g_settings.video.shader_enable = true;

      if (path && shader)
      {
         rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;
         // Load stored CGP into RGUI menu on success.
         // Used when a preset is directly loaded.
         // No point in updating when the CGP was created from RGUI itself.
         config_file_t *conf = config_file_new(path);
         if (conf)
         {
            gfx_shader_read_conf_cgp(conf, shader);
            gfx_shader_resolve_relative(shader, path);
            gfx_shader_resolve_parameters(conf, shader);
            config_file_free(conf);
         }

         rgui->need_refresh = true;
      }
   }
   else
   {
      RARCH_ERR("Setting RGUI CGP failed.\n");
      g_settings.video.shader_enable = false;
   }
#endif
}

static void menu_common_shader_manager_get_str(void *data, char *type_str, size_t type_str_size, unsigned type)
{
   (void)data;
   (void)type_str;
   (void)type_str_size;
   (void)type;

#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader *shader = (struct gfx_shader*)data;
   if (type == RGUI_SETTINGS_SHADER_APPLY)
      *type_str = '\0';
   else if (type >= RGUI_SETTINGS_SHADER_PARAMETER_0 && type <= RGUI_SETTINGS_SHADER_PARAMETER_LAST)
   {
      // rgui->parameter_shader here.
      if (shader)
      {
         const struct gfx_shader_parameter *param = &shader->parameters[type - RGUI_SETTINGS_SHADER_PARAMETER_0];
         snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]", param->current, param->minimum, param->maximum);
      }
      else
         *type_str = '\0';
   }
   else if (type == RGUI_SETTINGS_SHADER_PASSES)
      snprintf(type_str, type_str_size, "%u", shader->passes);
   else if (type >= RGUI_SETTINGS_SHADER_0 && type <= RGUI_SETTINGS_SHADER_LAST)
   {
      unsigned pass = (type - RGUI_SETTINGS_SHADER_0) / 3;
      switch ((type - RGUI_SETTINGS_SHADER_0) % 3)
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
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;
   char buffer[PATH_MAX];
   unsigned d, type;

   if (!rgui)
   {
      RARCH_ERR("Cannot save shader preset, menu handle is not initialized.\n");
      return;
   }

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_type)
      type = driver.menu_ctx->backend->shader_manager_get_type(&rgui->shader);
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
      conf_path = type == RARCH_SHADER_GLSL ? rgui->default_glslp : rgui->default_cgp;

   char config_directory[PATH_MAX];
   if (*g_extern.config_path)
      fill_pathname_basedir(config_directory, g_extern.config_path, sizeof(config_directory));
   else
      *config_directory = '\0';

   char cgp_path[PATH_MAX];
   const char *dirs[] = {
      g_settings.video.shader_dir,
      g_settings.rgui_config_directory,
      config_directory,
   };

   config_file_t *conf = config_file_new(NULL);
   if (!conf)
      return;
   gfx_shader_write_conf_cgp(conf, &rgui->shader);

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

static unsigned menu_common_shader_manager_get_type(void *data)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned i, type;
   const struct gfx_shader *shader = (const struct gfx_shader*)data;

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
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
   {
      RARCH_ERR("Cannot toggle shader setting, menu handle is not initialized.\n");
      return 0;
   }

   (void)setting;
   (void)action;

#ifdef HAVE_SHADER_MANAGER
   unsigned dist_shader, dist_filter, dist_scale;

   dist_shader = setting - RGUI_SETTINGS_SHADER_0;
   dist_filter = setting - RGUI_SETTINGS_SHADER_0_FILTER;
   dist_scale  = setting - RGUI_SETTINGS_SHADER_0_SCALE;

   if (setting == RGUI_SETTINGS_SHADER_FILTER)
   {
      switch (action)
      {
         case RGUI_ACTION_START:
            g_settings.video.smooth = true;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
            g_settings.video.smooth = !g_settings.video.smooth;
            break;

         default:
            break;
      }
   }
   else if ((setting == RGUI_SETTINGS_SHADER_PARAMETERS || setting == RGUI_SETTINGS_SHADER_PRESET_PARAMETERS) && action == RGUI_ACTION_OK)
   {
      file_list_push(rgui->menu_stack, "", setting, rgui->selection_ptr);
      menu_clear_navigation(rgui);
      rgui->need_refresh = true;
   }
   else if (setting >= RGUI_SETTINGS_SHADER_PARAMETER_0 && setting <= RGUI_SETTINGS_SHADER_PARAMETER_LAST)
   {
      if (!rgui->parameter_shader)
         return 0;

      struct gfx_shader_parameter *param = &rgui->parameter_shader->parameters[setting - RGUI_SETTINGS_SHADER_PARAMETER_0];
      switch (action)
      {
         case RGUI_ACTION_START:
            param->current = param->initial;
            break;

         case RGUI_ACTION_LEFT:
            param->current -= param->step;
            break;

         case RGUI_ACTION_RIGHT:
            param->current += param->step;
            break;

         default:
            break;
      }

      param->current = min(max(param->minimum, param->current), param->maximum);
   }
   else if ((setting == RGUI_SETTINGS_SHADER_APPLY || setting == RGUI_SETTINGS_SHADER_PASSES) &&
         (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_set))
      driver.menu_ctx->backend->setting_set(setting, action);
   else if (((dist_shader % 3) == 0 || setting == RGUI_SETTINGS_SHADER_PRESET))
   {
      dist_shader /= 3;
      struct gfx_shader_pass *pass = setting == RGUI_SETTINGS_SHADER_PRESET ?
         &rgui->shader.pass[dist_shader] : NULL;
      switch (action)
      {
         case RGUI_ACTION_OK:
            file_list_push(rgui->menu_stack, g_settings.video.shader_dir, setting, rgui->selection_ptr);
            menu_clear_navigation(rgui);
            rgui->need_refresh = true;
            break;

         case RGUI_ACTION_START:
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
      struct gfx_shader_pass *pass = &rgui->shader.pass[dist_filter];
      switch (action)
      {
         case RGUI_ACTION_START:
            rgui->shader.pass[dist_filter].filter = RARCH_FILTER_UNSPEC;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
         {
            unsigned delta = action == RGUI_ACTION_LEFT ? 2 : 1;
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
      struct gfx_shader_pass *pass = &rgui->shader.pass[dist_scale];
      switch (action)
      {
         case RGUI_ACTION_START:
            pass->fbo.scale_x = pass->fbo.scale_y = 0;
            pass->fbo.valid = false;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
         {
            unsigned current_scale = pass->fbo.scale_x;
            unsigned delta = action == RGUI_ACTION_LEFT ? 5 : 1;
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
   if ((setting >= RGUI_SETTINGS_SHADER_FILTER) && (setting <= RGUI_SETTINGS_SHADER_LAST))
   {
      if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_setting_toggle)
         return driver.menu_ctx->backend->shader_manager_setting_toggle(setting, action);
      else
         return 0;
   }
#endif
   if ((setting >= RGUI_SETTINGS_CORE_OPTION_START) &&
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
   index = setting - RGUI_SETTINGS_CORE_OPTION_START;

   switch (action)
   {
      case RGUI_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, index);
         break;

      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
         core_option_next(g_extern.system.core_options, index);
         break;

      case RGUI_ACTION_START:
         core_option_set_default(g_extern.system.core_options, index);
         break;

      default:
         break;
   }

   return 0;
}

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2

static unsigned rgui_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
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
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 640, 480 },
};

static unsigned rgui_current_gx_resolution = GX_RESOLUTIONS_640_480;
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
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!driver.osk || !rgui)
      return false;

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
      gfx_shader_write_conf_cgp(conf, &rgui->shader);
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
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;
   unsigned port = rgui->current_pad;

   switch (setting)
   {
      case RGUI_START_SCREEN:
         if (action == RGUI_ACTION_OK)
            file_list_push(rgui->menu_stack, "", RGUI_START_SCREEN, 0);
         break;
      case RGUI_SETTINGS_REWIND_ENABLE:
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
         {
            g_settings.rewind_enable = !g_settings.rewind_enable;
            if (g_settings.rewind_enable)
               rarch_init_rewind();
            else
               rarch_deinit_rewind();
         }
         else if (action == RGUI_ACTION_START)
         {
            g_settings.rewind_enable = false;
            rarch_deinit_rewind();
         }
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_GPU_SCREENSHOT:
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
            g_settings.video.gpu_screenshot = !g_settings.video.gpu_screenshot;
         else if (action == RGUI_ACTION_START)
            g_settings.video.gpu_screenshot = true;
         break;
#endif
      case RGUI_SETTINGS_REWIND_GRANULARITY:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT)
            g_settings.rewind_granularity++;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.rewind_granularity > 1)
               g_settings.rewind_granularity--;
         }
         else if (action == RGUI_ACTION_START)
            g_settings.rewind_granularity = 1;
         break;
      case RGUI_SETTINGS_LIBRETRO_LOG_LEVEL:
         if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.libretro_log_level > 0)
               g_settings.libretro_log_level--;
         }
         else if (action == RGUI_ACTION_RIGHT)
            if (g_settings.libretro_log_level < 3)
               g_settings.libretro_log_level++;
         break;
      case RGUI_SETTINGS_LOGGING_VERBOSITY:
         if (action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_extern.verbose = !g_extern.verbose;
         break;
      case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT
               || action == RGUI_ACTION_LEFT)
            g_extern.config_save_on_exit = !g_extern.config_save_on_exit;
         else if (action == RGUI_ACTION_START)
            g_extern.config_save_on_exit = true;
         break;
      case RGUI_SETTINGS_SAVESTATE_AUTO_SAVE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT
               || action == RGUI_ACTION_LEFT)
            g_settings.savestate_auto_save = !g_settings.savestate_auto_save;
         else if (action == RGUI_ACTION_START)
            g_settings.savestate_auto_save = false;
         break;
      case RGUI_SETTINGS_SAVESTATE_AUTO_LOAD:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT
               || action == RGUI_ACTION_LEFT)
            g_settings.savestate_auto_load = !g_settings.savestate_auto_load;
         else if (action == RGUI_ACTION_START)
            g_settings.savestate_auto_load = true;
         break;
      case RGUI_SETTINGS_BLOCK_SRAM_OVERWRITE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT
               || action == RGUI_ACTION_LEFT)
            g_settings.block_sram_overwrite = !g_settings.block_sram_overwrite;
         else if (action == RGUI_ACTION_START)
            g_settings.block_sram_overwrite = false;
         break;
      case RGUI_SETTINGS_PER_CORE_CONFIG:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT
               || action == RGUI_ACTION_LEFT)
            g_settings.core_specific_config = !g_settings.core_specific_config;
         else if (action == RGUI_ACTION_START)
            g_settings.core_specific_config = default_core_specific_config;
         break;
#if defined(HAVE_THREADS)
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT)
         {
            rarch_deinit_autosave();
            g_settings.autosave_interval += 10;
            if (g_settings.autosave_interval)
               rarch_init_autosave();
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.autosave_interval)
            {
               rarch_deinit_autosave();
               g_settings.autosave_interval -= min(10, g_settings.autosave_interval);
               if (g_settings.autosave_interval)
                  rarch_init_autosave();
            }
         }
         else if (action == RGUI_ACTION_START)
         {
            rarch_deinit_autosave();
            g_settings.autosave_interval = 0;
         }
         break;
#endif
      case RGUI_SETTINGS_SAVESTATE_SAVE:
      case RGUI_SETTINGS_SAVESTATE_LOAD:
         if (action == RGUI_ACTION_OK)
         {
            if (setting == RGUI_SETTINGS_SAVESTATE_SAVE)
               rarch_save_state();
            else
            {
               // Disallow savestate load when we absoluetely cannot change game state.
#ifdef HAVE_BSV_MOVIE
               if (g_extern.bsv.movie)
                  break;
#endif
#ifdef HAVE_NETPLAY
               if (g_extern.netplay)
                  break;
#endif
               rarch_load_state();
            }
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         else if (action == RGUI_ACTION_START)
            g_extern.state_slot = 0;
         else if (action == RGUI_ACTION_LEFT)
         {
            // Slot -1 is (auto) slot.
            if (g_extern.state_slot >= 0)
               g_extern.state_slot--;
         }
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.state_slot++;
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_SCREENSHOT:
         if (action == RGUI_ACTION_OK)
            rarch_take_screenshot();
         break;
#endif
      case RGUI_SETTINGS_RESTART_GAME:
         if (action == RGUI_ACTION_OK)
         {
            rarch_game_reset();
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         if (action == RGUI_ACTION_START)
            g_extern.audio_data.mute = false;
         else
            g_extern.audio_data.mute = !g_extern.audio_data.mute;
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         if (action == RGUI_ACTION_START)
         {
            g_settings.audio.rate_control_delta = rate_control_delta;
            g_settings.audio.rate_control = rate_control;
         }
         else if (action == RGUI_ACTION_LEFT)
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
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.audio.rate_control_delta < 0.2)
               g_settings.audio.rate_control_delta += 0.001;
            g_settings.audio.rate_control = true;
         }
         break;
      case RGUI_SETTINGS_AUDIO_VOLUME:
         {
            float db_delta = 0.0f;
            if (action == RGUI_ACTION_START)
            {
               g_extern.audio_data.volume_db = 0.0f;
               g_extern.audio_data.volume_gain = 1.0f;
            }
            else if (action == RGUI_ACTION_LEFT)
               db_delta -= 1.0f;
            else if (action == RGUI_ACTION_RIGHT)
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
      case RGUI_SETTINGS_FASTFORWARD_RATIO:
         {
            bool clamp_value = false;
            if (action == RGUI_ACTION_START)
               g_settings.fastforward_ratio = fastforward_ratio;
            else if (action == RGUI_ACTION_LEFT)
            {
               g_settings.fastforward_ratio -= 0.1f;
               if (g_settings.fastforward_ratio < 0.95f) // Avoid potential rounding errors when going from 1.1 to 1.0.
                  g_settings.fastforward_ratio = fastforward_ratio;
               else
                  clamp_value = true;
            }
            else if (action == RGUI_ACTION_RIGHT)
            {
               g_settings.fastforward_ratio += 0.1f;
               clamp_value = true;
            }

            if (clamp_value)
               g_settings.fastforward_ratio = max(min(g_settings.fastforward_ratio, 10.0f), 1.0f);
         }
         break;
      case RGUI_SETTINGS_SLOWMOTION_RATIO:
         {
            if (action == RGUI_ACTION_START)
               g_settings.slowmotion_ratio = slowmotion_ratio;
            else if (action == RGUI_ACTION_LEFT)
               g_settings.slowmotion_ratio -= 0.1f;
            else if (action == RGUI_ACTION_RIGHT)
               g_settings.slowmotion_ratio += 0.1f;

            g_settings.slowmotion_ratio = max(min(g_settings.slowmotion_ratio, 10.0f), 1.0f);
         }
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         if (action == RGUI_ACTION_START)
            g_settings.fps_show = false;
         else if (action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.fps_show = !g_settings.fps_show;
         break;
      case RGUI_SETTINGS_DISK_INDEX:
         {
            const struct retro_disk_control_callback *control = &g_extern.system.disk_control;

            unsigned num_disks = control->get_num_images();
            unsigned current   = control->get_image_index();

            int step = 0;
            if (action == RGUI_ACTION_RIGHT || action == RGUI_ACTION_OK)
               step = 1;
            else if (action == RGUI_ACTION_LEFT)
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
      case RGUI_SETTINGS_RESTART_EMULATOR:
         if (action == RGUI_ACTION_OK)
         {
#if defined(GEKKO) && defined(HW_RVL)
            fill_pathname_join(g_extern.fullpath, default_paths.core_dir, SALAMANDER_FILE,
                  sizeof(g_extern.fullpath));
#endif
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }
         break;
      case RGUI_SETTINGS_RESUME_GAME:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_QUIT_RARCH:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_SAVE_CONFIG:
         if (action == RGUI_ACTION_OK)
            menu_save_new_config();
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_PRESET:
         switch (action)
         {
            case RGUI_ACTION_OK:
               file_list_push(rgui->menu_stack, g_extern.overlay_dir, setting, rgui->selection_ptr);
               menu_clear_navigation(rgui);
               rgui->need_refresh = true;
               break;

#ifndef __QNX__ // FIXME: Why ifndef QNX?
            case RGUI_ACTION_START:
               if (driver.overlay)
                  input_overlay_free(driver.overlay);
               driver.overlay = NULL;
               *g_settings.input.overlay = '\0';
               break;
#endif

            default:
               break;
         }
         break;
#endif
      case RGUI_SETTINGS_VIDEO_SOFTFILTER:
         switch (action)
         {
#ifdef HAVE_FILTERS_BUILTIN
            case RGUI_ACTION_LEFT:
               if (g_settings.video.filter_idx > 0)
                  g_settings.video.filter_idx--;
               break;
            case RGUI_ACTION_RIGHT:
               if ((g_settings.video.filter_idx + 1) != softfilter_get_last_idx())
                  g_settings.video.filter_idx++;
               break;
#endif
            case RGUI_ACTION_OK:
#if defined(HAVE_DYLIB)
               file_list_push(rgui->menu_stack, g_settings.video.filter_dir, setting, rgui->selection_ptr);
               menu_clear_navigation(rgui);
#else
               rarch_deinit_filter();
               rarch_init_filter(g_extern.system.pix_fmt);
#endif
               rgui->need_refresh = true;
               break;
            case RGUI_ACTION_START:
#if defined(HAVE_FILTERS_BUILTIN)
               g_settings.video.filter_idx = 0;
#else
               strlcpy(g_settings.video.filter_path, "", sizeof(g_settings.video.filter_path));
#endif
               rarch_deinit_filter();
               rarch_init_filter(g_extern.system.pix_fmt);
               break;
         }
         break;
      case RGUI_SETTINGS_AUDIO_DSP_FILTER:
         switch (action)
         {
            case RGUI_ACTION_OK:
               file_list_push(rgui->menu_stack, g_settings.audio.filter_dir, setting, rgui->selection_ptr);
               menu_clear_navigation(rgui);
               rgui->need_refresh = true;
               break;
            case RGUI_ACTION_START:
               *g_settings.audio.dsp_plugin = '\0';
               rarch_deinit_dsp_filter();
               break;
         }
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_OPACITY:
         {
            bool changed = true;
            switch (action)
            {
               case RGUI_ACTION_LEFT:
                  g_settings.input.overlay_opacity -= 0.01f;

                  if (g_settings.input.overlay_opacity < 0.0f)
                     g_settings.input.overlay_opacity = 0.0f;
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  g_settings.input.overlay_opacity += 0.01f;

                  if (g_settings.input.overlay_opacity > 1.0f)
                     g_settings.input.overlay_opacity = 1.0f;
                  break;

               case RGUI_ACTION_START:
                  g_settings.input.overlay_opacity = 0.7f;
                  break;

               default:
                  changed = false;
                  break;
            }

            if (changed && driver.overlay)
               input_overlay_set_alpha_mod(driver.overlay,
                     g_settings.input.overlay_opacity);
            break;
         }

      case RGUI_SETTINGS_OVERLAY_SCALE:
         {
            bool changed = true;
            switch (action)
            {
               case RGUI_ACTION_LEFT:
                  g_settings.input.overlay_scale -= 0.01f;

                  if (g_settings.input.overlay_scale < 0.01f) // Avoid potential divide by zero.
                     g_settings.input.overlay_scale = 0.01f;
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  g_settings.input.overlay_scale += 0.01f;

                  if (g_settings.input.overlay_scale > 2.0f)
                     g_settings.input.overlay_scale = 2.0f;
                  break;

               case RGUI_ACTION_START:
                  g_settings.input.overlay_scale = 1.0f;
                  break;

               default:
                  changed = false;
                  break;
            }

            if (changed && driver.overlay)
               input_overlay_set_scale_factor(driver.overlay,
                     g_settings.input.overlay_scale);
            break;
         }
#endif
         // controllers
      case RGUI_SETTINGS_BIND_PLAYER:
         if (action == RGUI_ACTION_START)
            rgui->current_pad = 0;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (rgui->current_pad != 0)
               rgui->current_pad--;
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (rgui->current_pad < MAX_PLAYERS - 1)
               rgui->current_pad++;
         }
#ifdef HAVE_RGUI
         if (port != rgui->current_pad)
            rgui->need_refresh = true;
#endif
         port = rgui->current_pad;
         break;
      case RGUI_SETTINGS_BIND_DEVICE:
         // If set_keybinds is supported, we do it more fancy, and scroll through
         // a list of supported devices directly.
         if (driver.input->set_keybinds && driver.input->devices_size)
         {
            unsigned device_last = driver.input->devices_size(driver.input_data);
            g_settings.input.device[port] += device_last;
            if (action == RGUI_ACTION_START)
               g_settings.input.device[port] = 0;
            else if (action == RGUI_ACTION_LEFT)
               g_settings.input.device[port]--;
            else if (action == RGUI_ACTION_RIGHT)
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
            if (action == RGUI_ACTION_START)
               *p = port;
            else if (action == RGUI_ACTION_LEFT)
               (*p)--;
            else if (action == RGUI_ACTION_RIGHT)
               (*p)++;

            if (*p < -1)
               *p = -1;
            else if (*p >= MAX_PLAYERS)
               *p = MAX_PLAYERS - 1;
         }
         break;
      case RGUI_SETTINGS_BIND_ANALOG_MODE:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.input.analog_dpad_mode[port] = 0;
               break;

            case RGUI_ACTION_OK:
            case RGUI_ACTION_RIGHT:
               g_settings.input.analog_dpad_mode[port] = (g_settings.input.analog_dpad_mode[port] + 1) % ANALOG_DPAD_LAST;
               break;

            case RGUI_ACTION_LEFT:
               g_settings.input.analog_dpad_mode[port] = (g_settings.input.analog_dpad_mode[port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST;
               break;

            default:
               break;
         }
         break;
      case RGUI_SETTINGS_INPUT_AXIS_THRESHOLD:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.input.axis_threshold = 0.5;
               break;

            case RGUI_ACTION_OK:
            case RGUI_ACTION_RIGHT:
               g_settings.input.axis_threshold += 0.01;
               break;
            case RGUI_ACTION_LEFT:
               g_settings.input.axis_threshold -= 0.01;
               break;

            default:
               break;
         }
         g_settings.input.axis_threshold = max(min(g_settings.input.axis_threshold, 0.95f), 0.05f);
         break;
      case RGUI_SETTINGS_BIND_DEVICE_TYPE:
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
               case RGUI_ACTION_START:
                  current_device = RETRO_DEVICE_JOYPAD;
                  break;

               case RGUI_ACTION_LEFT:
                  current_device = devices[(current_index + types - 1) % types];
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
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
      case RGUI_SETTINGS_DEVICE_AUTODETECT_ENABLE:
         if (action == RGUI_ACTION_OK)
            g_settings.input.autodetect_enable = !g_settings.input.autodetect_enable;
         break;
      case RGUI_SETTINGS_CUSTOM_BIND_MODE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            rgui->bind_mode_keyboard = !rgui->bind_mode_keyboard;
         break;
      case RGUI_SETTINGS_CUSTOM_BIND_ALL:
         if (action == RGUI_ACTION_OK)
         {
            if (rgui->bind_mode_keyboard)
            {
               rgui->binds.target = &g_settings.input.binds[port][0];
               rgui->binds.begin = RGUI_SETTINGS_BIND_BEGIN;
               rgui->binds.last = RGUI_SETTINGS_BIND_LAST;
               file_list_push(rgui->menu_stack, "", RGUI_SETTINGS_CUSTOM_BIND_KEYBOARD, rgui->selection_ptr);
               rgui->binds.timeout_end = rarch_get_time_usec() + RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
               input_keyboard_wait_keys(driver.menu, menu_custom_bind_keyboard_cb);
            }
            else
            {
               rgui->binds.target = &g_settings.input.binds[port][0];
               rgui->binds.begin = RGUI_SETTINGS_BIND_BEGIN;
               rgui->binds.last = RGUI_SETTINGS_BIND_LAST;
               file_list_push(rgui->menu_stack, "", RGUI_SETTINGS_CUSTOM_BIND, rgui->selection_ptr);
               menu_poll_bind_get_rested_axes(&rgui->binds);
               menu_poll_bind_state(&rgui->binds);
            }
         }
         break;
      case RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         if (action == RGUI_ACTION_OK)
         {
            unsigned i;
            struct retro_keybind *target = &g_settings.input.binds[port][0];
            const struct retro_keybind *def_binds = port ? retro_keybinds_rest : retro_keybinds_1;
            rgui->binds.begin = RGUI_SETTINGS_BIND_BEGIN;
            rgui->binds.last = RGUI_SETTINGS_BIND_LAST;
            for (i = RGUI_SETTINGS_BIND_BEGIN; i <= RGUI_SETTINGS_BIND_LAST; i++, target++)
            {
               if (rgui->bind_mode_keyboard)
                  target->key = def_binds[i - RGUI_SETTINGS_BIND_BEGIN].key;
               else
               {
                  target->joykey = NO_BTN;
                  target->joyaxis = AXIS_NONE;
               }
            }
         }
         break;
      case RGUI_SETTINGS_BIND_UP:
      case RGUI_SETTINGS_BIND_DOWN:
      case RGUI_SETTINGS_BIND_LEFT:
      case RGUI_SETTINGS_BIND_RIGHT:
      case RGUI_SETTINGS_BIND_A:
      case RGUI_SETTINGS_BIND_B:
      case RGUI_SETTINGS_BIND_X:
      case RGUI_SETTINGS_BIND_Y:
      case RGUI_SETTINGS_BIND_START:
      case RGUI_SETTINGS_BIND_SELECT:
      case RGUI_SETTINGS_BIND_L:
      case RGUI_SETTINGS_BIND_R:
      case RGUI_SETTINGS_BIND_L2:
      case RGUI_SETTINGS_BIND_R2:
      case RGUI_SETTINGS_BIND_L3:
      case RGUI_SETTINGS_BIND_R3:
      case RGUI_SETTINGS_BIND_TURBO_ENABLE:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
      case RGUI_SETTINGS_BIND_FAST_FORWARD_KEY:
      case RGUI_SETTINGS_BIND_FAST_FORWARD_HOLD_KEY:
      case RGUI_SETTINGS_BIND_LOAD_STATE_KEY:
      case RGUI_SETTINGS_BIND_SAVE_STATE_KEY:
      case RGUI_SETTINGS_BIND_FULLSCREEN_TOGGLE_KEY:
      case RGUI_SETTINGS_BIND_QUIT_KEY:
      case RGUI_SETTINGS_BIND_STATE_SLOT_PLUS:
      case RGUI_SETTINGS_BIND_STATE_SLOT_MINUS:
      case RGUI_SETTINGS_BIND_REWIND:
      case RGUI_SETTINGS_BIND_MOVIE_RECORD_TOGGLE:
      case RGUI_SETTINGS_BIND_PAUSE_TOGGLE:
      case RGUI_SETTINGS_BIND_FRAMEADVANCE:
      case RGUI_SETTINGS_BIND_RESET:
      case RGUI_SETTINGS_BIND_SHADER_NEXT:
      case RGUI_SETTINGS_BIND_SHADER_PREV:
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_PLUS:
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_MINUS:
      case RGUI_SETTINGS_BIND_CHEAT_TOGGLE:
      case RGUI_SETTINGS_BIND_SCREENSHOT:
      case RGUI_SETTINGS_BIND_MUTE:
      case RGUI_SETTINGS_BIND_NETPLAY_FLIP:
      case RGUI_SETTINGS_BIND_SLOWMOTION:
      case RGUI_SETTINGS_BIND_ENABLE_HOTKEY:
      case RGUI_SETTINGS_BIND_VOLUME_UP:
      case RGUI_SETTINGS_BIND_VOLUME_DOWN:
      case RGUI_SETTINGS_BIND_OVERLAY_NEXT:
      case RGUI_SETTINGS_BIND_DISK_EJECT_TOGGLE:
      case RGUI_SETTINGS_BIND_DISK_NEXT:
      case RGUI_SETTINGS_BIND_GRAB_MOUSE_TOGGLE:
      case RGUI_SETTINGS_BIND_MENU_TOGGLE:
         if (driver.input->set_keybinds && !driver.input->get_joypad_driver)
         {
            unsigned keybind_action = KEYBINDS_ACTION_NONE;

            if (action == RGUI_ACTION_START)
               keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);

            // FIXME: The array indices here look totally wrong ... Fixed it so it looks kind of sane for now.
            if (keybind_action != KEYBINDS_ACTION_NONE)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port,
                     setting - RGUI_SETTINGS_BIND_BEGIN, keybind_action);
         }
         else
         {
            struct retro_keybind *bind = &g_settings.input.binds[port][setting - RGUI_SETTINGS_BIND_BEGIN];
            if (action == RGUI_ACTION_OK)
            {
               rgui->binds.begin = setting;
               rgui->binds.last = setting;
               rgui->binds.target = bind;
               rgui->binds.player = port;
               file_list_push(rgui->menu_stack, "",
                     rgui->bind_mode_keyboard ? RGUI_SETTINGS_CUSTOM_BIND_KEYBOARD : RGUI_SETTINGS_CUSTOM_BIND, rgui->selection_ptr);

               if (rgui->bind_mode_keyboard)
               {
                  rgui->binds.timeout_end = rarch_get_time_usec() + RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
                  input_keyboard_wait_keys(driver.menu, menu_custom_bind_keyboard_cb);
               }
               else
               {
                  menu_poll_bind_get_rested_axes(&rgui->binds);
                  menu_poll_bind_state(&rgui->binds);
               }
            }
            else if (action == RGUI_ACTION_START)
            {
               if (rgui->bind_mode_keyboard)
               {
                  const struct retro_keybind *def_binds = port ? retro_keybinds_rest : retro_keybinds_1;
                  bind->key = def_binds[setting - RGUI_SETTINGS_BIND_BEGIN].key;
               }
               else
               {
                  bind->joykey = NO_BTN;
                  bind->joyaxis = AXIS_NONE;
               }
            }
         }
         break;
      case RGUI_BROWSER_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.rgui_content_directory = '\0';
         break;
      case RGUI_CONTENT_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.content_directory = '\0';
         break;
      case RGUI_ASSETS_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.assets_directory = '\0';
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SCREENSHOT_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.screenshot_directory = '\0';
         break;
#endif
      case RGUI_SAVEFILE_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.savefile_dir = '\0';
         break;
#ifdef HAVE_OVERLAY
      case RGUI_OVERLAY_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.overlay_dir = '\0';
         break;
#endif
      case RGUI_SAVESTATE_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.savestate_dir = '\0';
         break;
      case RGUI_LIBRETRO_DIR_PATH:
         if (action == RGUI_ACTION_START)
         {
            *g_settings.libretro_directory = '\0';
            if (driver.menu_ctx && driver.menu_ctx->init_core_info)
               driver.menu_ctx->init_core_info(rgui);
         }
         break;
      case RGUI_LIBRETRO_INFO_DIR_PATH:
         if (action == RGUI_ACTION_START)
         {
            *g_settings.libretro_info_path = '\0';
            if (driver.menu_ctx && driver.menu_ctx->init_core_info)
               driver.menu_ctx->init_core_info(rgui);
         }
         break;
      case RGUI_CONFIG_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.rgui_config_directory = '\0';
         break;
      case RGUI_FILTER_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.video.filter_dir = '\0';
         break;
      case RGUI_DSP_FILTER_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.audio.filter_dir = '\0';
         break;
      case RGUI_SHADER_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.video.shader_dir = '\0';
         break;
      case RGUI_SYSTEM_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.system_directory = '\0';
         break;
      case RGUI_AUTOCONFIG_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.input.autoconfig_dir = '\0';
         break;
      case RGUI_SETTINGS_VIDEO_ROTATION:
         if (action == RGUI_ACTION_START)
         {
            g_settings.video.rotation = ORIENTATION_NORMAL;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.video.rotation > 0)
               g_settings.video.rotation--;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.video.rotation < LAST_ORIENTATION)
               g_settings.video.rotation++;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         break;

      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            g_settings.video.smooth = video_smooth;
         else
            g_settings.video.smooth = !g_settings.video.smooth;

         if (driver.video_data && driver.video_poke && driver.video_poke->set_filtering)
            driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         break;

      case RGUI_SETTINGS_DRIVER_VIDEO:
         if (action == RGUI_ACTION_LEFT)
            find_prev_video_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_video_driver();
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO:
         if (action == RGUI_ACTION_LEFT)
            find_prev_audio_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_audio_driver();
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO_DEVICE:
         if (action == RGUI_ACTION_OK)
         {
#ifdef HAVE_OSK
            if (g_settings.osk.enable)
            {
               g_extern.osk.cb_init     = osk_callback_enter_audio_device_init;
               g_extern.osk.cb_callback = osk_callback_enter_audio_device;
            }
            else
#endif
               menu_key_start_line(rgui, "Audio Device Name / IP: ", audio_device_callback);
         }
         else if (action == RGUI_ACTION_START)
            *g_settings.audio.device = '\0';
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO_RESAMPLER:
         if (action == RGUI_ACTION_LEFT)
            find_prev_resampler_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_resampler_driver();
         break;
      case RGUI_SETTINGS_DRIVER_INPUT:
         if (action == RGUI_ACTION_LEFT)
            find_prev_input_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_input_driver();
         break;
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_DRIVER_CAMERA:
         if (action == RGUI_ACTION_LEFT)
            find_prev_camera_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_camera_driver();
         break;
#endif
#ifdef HAVE_LOCATION
      case RGUI_SETTINGS_DRIVER_LOCATION:
         if (action == RGUI_ACTION_LEFT)
            find_prev_location_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_location_driver();
         break;
#endif
#ifdef HAVE_MENU
      case RGUI_SETTINGS_DRIVER_MENU:
         if (action == RGUI_ACTION_LEFT)
            find_prev_menu_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_menu_driver();
         break;
#endif
      case RGUI_SETTINGS_VIDEO_GAMMA:
         if (action == RGUI_ACTION_START)
         {
            g_extern.console.screen.gamma_correction = 0;
            if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_extern.console.screen.gamma_correction > 0)
            {
               g_extern.console.screen.gamma_correction--;
               if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_extern.console.screen.gamma_correction < MAX_GAMMA_SETTING)
            {
               g_extern.console.screen.gamma_correction++;
               if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         break;

      case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
         if (action == RGUI_ACTION_START)
            g_settings.video.scale_integer = scale_integer;
         else if (action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT ||
               action == RGUI_ACTION_OK)
            g_settings.video.scale_integer = !g_settings.video.scale_integer;

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         if (action == RGUI_ACTION_START)
            g_settings.video.aspect_ratio_idx = aspect_ratio_idx;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.video.aspect_ratio_idx > 0)
               g_settings.video.aspect_ratio_idx--;
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.video.aspect_ratio_idx < LAST_ASPECT_RATIO)
               g_settings.video.aspect_ratio_idx++;
         }

         if (driver.video_data && driver.video_poke && driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         break;

      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
         if (action == RGUI_ACTION_OK)
            rarch_set_fullscreen(g_settings.video.fullscreen);
         break;

#if defined(GEKKO)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         if (action == RGUI_ACTION_LEFT)
         {
            if (rgui_current_gx_resolution > 0)
            {
               rgui_current_gx_resolution--;
               if (driver.video_data)
                  gx_set_video_mode(driver.video_data, rgui_gx_resolutions[rgui_current_gx_resolution][0], rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (rgui_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
            {
#ifdef HW_RVL
               if ((rgui_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
                  if (CONF_GetVideo() != CONF_VIDEO_PAL)
                     return 0;
#endif

               rgui_current_gx_resolution++;
               if (driver.video_data)
                  gx_set_video_mode(driver.video_data, rgui_gx_resolutions[rgui_current_gx_resolution][0],
                        rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
         break;
#elif defined(__CELLOS_LV2__)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         if (action == RGUI_ACTION_LEFT)
         {
            if (g_extern.console.screen.resolutions.current.idx)
            {
               g_extern.console.screen.resolutions.current.idx--;
               g_extern.console.screen.resolutions.current.id =
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_extern.console.screen.resolutions.current.idx + 1 <
                  g_extern.console.screen.resolutions.count)
            {
               g_extern.console.screen.resolutions.current.idx++;
               g_extern.console.screen.resolutions.current.id =
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == RGUI_ACTION_OK)
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

            rarch_set_fullscreen(g_settings.video.fullscreen);
         }
         break;
      case RGUI_SETTINGS_VIDEO_PAL60:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
                     g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  else
                     g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  rarch_set_fullscreen(g_settings.video.fullscreen);
               }
               break;
            case RGUI_ACTION_START:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  rarch_set_fullscreen(g_settings.video.fullscreen);
               }
               break;
         }
         break;
#endif
#ifdef HW_RVL
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
            g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         else
            g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);

         if (driver.video_data && driver.video_poke && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
#endif

      case RGUI_SETTINGS_VIDEO_VSYNC:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.vsync = true;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.vsync = !g_settings.video.vsync;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_HARD_SYNC:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.hard_sync = false;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.hard_sync = !g_settings.video.hard_sync;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.black_frame_insertion = false;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.black_frame_insertion = !g_settings.video.black_frame_insertion;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.crop_overscan = true;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.crop_overscan = !g_settings.video.crop_overscan;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
      {
         float *scale = setting == RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X ? &g_settings.video.xscale : &g_settings.video.yscale;
         float old_scale = *scale;

         switch (action)
         {
            case RGUI_ACTION_START:
               *scale = 3.0f;
               break;

            case RGUI_ACTION_LEFT:
               *scale -= 1.0f;
               break;

            case RGUI_ACTION_RIGHT:
               *scale += 1.0f;
               break;

            default:
               break;
         }

         *scale = roundf(*scale);
         *scale = max(*scale, 1.0f);

         if (old_scale != *scale && !g_settings.video.fullscreen)
            rarch_set_fullscreen(g_settings.video.fullscreen);

         break;
      }

#ifdef HAVE_THREADS
      case RGUI_SETTINGS_VIDEO_THREADED:
      {
         bool old = g_settings.video.threaded;
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
            g_settings.video.threaded = !g_settings.video.threaded;
         else if (action == RGUI_ACTION_START)
            g_settings.video.threaded = false;

         if (g_settings.video.threaded != old)
            rarch_set_fullscreen(g_settings.video.fullscreen);
         break;
      }
#endif

      case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
      {
         unsigned old = g_settings.video.swap_interval;
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.swap_interval = 1;
               break;

            case RGUI_ACTION_LEFT:
               g_settings.video.swap_interval--;
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.swap_interval++;
               break;

            default:
               break;
         }

         g_settings.video.swap_interval = min(g_settings.video.swap_interval, 4);
         g_settings.video.swap_interval = max(g_settings.video.swap_interval, 1);
         if (old != g_settings.video.swap_interval && driver.video && driver.video_data)
            video_set_nonblock_state_func(false); // This will update the current swap interval. Since we're in RGUI now, always apply VSync.

         break;
      }

      case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.hard_sync_frames = 0;
               break;

            case RGUI_ACTION_LEFT:
               if (g_settings.video.hard_sync_frames > 0)
                  g_settings.video.hard_sync_frames--;
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_settings.video.hard_sync_frames < 3)
                  g_settings.video.hard_sync_frames++;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_MONITOR_INDEX:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.monitor_index = 0;
               rarch_set_fullscreen(g_settings.video.fullscreen);
               break;

            case RGUI_ACTION_OK:
            case RGUI_ACTION_RIGHT:
               g_settings.video.monitor_index++;
               rarch_set_fullscreen(g_settings.video.fullscreen);
               break;

            case RGUI_ACTION_LEFT:
               if (g_settings.video.monitor_index)
               {
                  g_settings.video.monitor_index--;
                  rarch_set_fullscreen(g_settings.video.fullscreen);
               }
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_extern.measure_data.frame_time_samples_count = 0;
               break;

            case RGUI_ACTION_OK:
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
      case RGUI_SETTINGS_SHADER_PASSES:
         switch (action)
         {
            case RGUI_ACTION_START:
               rgui->shader.passes = 0;
               rgui->need_refresh = true;
               break;

            case RGUI_ACTION_LEFT:
               if (rgui->shader.passes)
               {
                  rgui->shader.passes--;
                  rgui->need_refresh = true;
               }
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (rgui->shader.passes < GFX_MAX_SHADERS)
               {
                  rgui->shader.passes++;
                  rgui->need_refresh = true;
               }
               break;

            default:
               break;
         }

         if (rgui->need_refresh)
            gfx_shader_resolve_parameters(NULL, &rgui->shader);
         break;
      case RGUI_SETTINGS_SHADER_APPLY:
      {
         unsigned type = RARCH_SHADER_NONE;

         if (!driver.video || !driver.video->set_shader || action != RGUI_ACTION_OK)
            return 0;

         RARCH_LOG("Applying shader ...\n");

         if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_type)
            type = driver.menu_ctx->backend->shader_manager_get_type(&rgui->shader);

         if (rgui->shader.passes && type != RARCH_SHADER_NONE
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
      case RGUI_SETTINGS_SHADER_PRESET_SAVE:
         if (action == RGUI_ACTION_OK)
         {
#ifdef HAVE_OSK
            if (g_settings.osk.enable)
            {
               g_extern.osk.cb_init = osk_callback_enter_filename_init;
               g_extern.osk.cb_callback = osk_callback_enter_filename;
            }
            else
#endif
               menu_key_start_line(rgui, "Preset Filename: ", preset_filename_callback);
         }
         break;
#endif
#ifdef _XBOX1
      case RGUI_SETTINGS_FLICKER_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
               if (g_extern.console.screen.flicker_filter_index > 0)
                  g_extern.console.screen.flicker_filter_index--;
               break;
            case RGUI_ACTION_RIGHT:
               if (g_extern.console.screen.flicker_filter_index < 5)
                  g_extern.console.screen.flicker_filter_index++;
               break;
            case RGUI_ACTION_START:
               g_extern.console.screen.flicker_filter_index = 0;
               break;
         }
         break;
      case RGUI_SETTINGS_SOFT_DISPLAY_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               else
                  g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
            case RGUI_ACTION_START:
               g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
         }
         break;
#endif
      case RGUI_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE:
         switch (action)
         {
            case RGUI_ACTION_OK:
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
            case RGUI_ACTION_START:
#if (CELL_SDK_VERSION > 0x340000)
               g_extern.lifecycle_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
#endif
               break;
         }
         break;
      case RGUI_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.pause_nonactive = !g_settings.pause_nonactive;
         else if (action == RGUI_ACTION_START)
            g_settings.pause_nonactive = false;
         break;
      case RGUI_SETTINGS_WINDOW_COMPOSITING_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
         {
            g_settings.video.disable_composition = !g_settings.video.disable_composition;
            rarch_set_fullscreen(g_settings.video.fullscreen);
         }
         else if (action == RGUI_ACTION_START)
         {
            g_settings.video.disable_composition = false;
            rarch_set_fullscreen(g_settings.video.fullscreen);
         }
         break;
#ifdef HAVE_NETPLAY
      case RGUI_SETTINGS_NETPLAY_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
         {
            g_extern.netplay_enable = !g_extern.netplay_enable;
            /* TODO/FIXME - toggle netplay on/off */
         }
         else if (action == RGUI_ACTION_START)
         {
            g_extern.netplay_enable = false;
            /* TODO/FIXME - toggle netplay on/off */
         }
         break;
      case RGUI_SETTINGS_NETPLAY_HOST_IP_ADDRESS:
         if (action == RGUI_ACTION_OK)
            menu_key_start_line(rgui, "IP Address: ", netplay_ipaddress_callback);
         else if (action == RGUI_ACTION_START)
            *g_extern.netplay_server = '\0';
         break;
      case RGUI_SETTINGS_NETPLAY_DELAY_FRAMES:
         if (action == RGUI_ACTION_LEFT)
         {
            if (g_extern.netplay_sync_frames != 0)
               g_extern.netplay_sync_frames--;
         }
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.netplay_sync_frames++;
         else if (action == RGUI_ACTION_START)
            g_extern.netplay_sync_frames = 0;
         break;
      case RGUI_SETTINGS_NETPLAY_TCP_UDP_PORT:
         if (action == RGUI_ACTION_OK)
            menu_key_start_line(rgui, "TCP/UDP Port: ", netplay_port_callback);
         else if (action == RGUI_ACTION_START)
            g_extern.netplay_port = RARCH_DEFAULT_PORT;
         break;
      case RGUI_SETTINGS_NETPLAY_NICKNAME:
         if (action == RGUI_ACTION_OK)
            menu_key_start_line(rgui, "Nickname: ", netplay_nickname_callback);
         else if (action == RGUI_ACTION_START)
            *g_extern.netplay_nick = '\0';
         break;
      case RGUI_SETTINGS_NETPLAY_MODE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_extern.netplay_is_client = !g_extern.netplay_is_client;
         else if (action == RGUI_ACTION_START)
            g_extern.netplay_is_client = false;
         break;
      case RGUI_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_extern.netplay_is_spectate = !g_extern.netplay_is_spectate;
         else if (action == RGUI_ACTION_START)
            g_extern.netplay_is_spectate = false;
         break;
#endif
#ifdef HAVE_OSK
      case RGUI_SETTINGS_ONSCREEN_KEYBOARD_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.osk.enable = !g_settings.osk.enable;
         else if (action == RGUI_ACTION_START)
            g_settings.osk.enable = false;
         break;
#endif
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_PRIVACY_CAMERA_ALLOW:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.camera.allow = !g_settings.camera.allow;
         else if (action == RGUI_ACTION_START)
            g_settings.camera.allow = false;
         break;
#endif
#ifdef HAVE_LOCATION
      case RGUI_SETTINGS_PRIVACY_LOCATION_ALLOW:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.location.allow = !g_settings.location.allow;
         else if (action == RGUI_ACTION_START)
            g_settings.location.allow = false;
         break;
#endif
      case RGUI_SETTINGS_FONT_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.video.font_enable = !g_settings.video.font_enable;
         else if (action == RGUI_ACTION_START)
            g_settings.video.font_enable = true;
         break;
      case RGUI_SETTINGS_FONT_SCALE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.video.font_scale = !g_settings.video.font_scale;
         else if (action == RGUI_ACTION_START)
            g_settings.video.font_scale = true;
         break;
      case RGUI_SETTINGS_FONT_SIZE:
         if (action == RGUI_ACTION_LEFT)
            g_settings.video.font_size -= 1.0f;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.video.font_size += 1.0f;
         else if (action == RGUI_ACTION_START)
            g_settings.video.font_size = font_size;
         g_settings.video.font_size = roundf(max(g_settings.video.font_size, 1.0f));
         break;
      case RGUI_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.load_dummy_on_core_shutdown = !g_settings.load_dummy_on_core_shutdown;
         else if (action == RGUI_ACTION_START)
            g_settings.load_dummy_on_core_shutdown = load_dummy_on_core_shutdown;
         break;
      default:
         break;
   }

   return 0;
}

static void menu_common_setting_set_label(char *type_str, size_t type_str_size, unsigned *w, unsigned type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   switch (type)
   {
      case RGUI_SETTINGS_VIDEO_ROTATION:
         strlcpy(type_str, rotation_lut[g_settings.video.rotation],
               type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         snprintf(type_str, type_str_size,
               (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
         break;
      case RGUI_SETTINGS_VIDEO_FILTER:
         if (g_settings.video.smooth)
            strlcpy(type_str, "Bilinear filtering", type_str_size);
         else
            strlcpy(type_str, "Point filtering", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_GAMMA:
         snprintf(type_str, type_str_size, "%d", g_extern.console.screen.gamma_correction);
         break;
      case RGUI_SETTINGS_VIDEO_VSYNC:
         strlcpy(type_str, g_settings.video.vsync ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC:
         strlcpy(type_str, g_settings.video.hard_sync ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         strlcpy(type_str, g_settings.video.black_frame_insertion ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
         snprintf(type_str, type_str_size, "%u", g_settings.video.swap_interval);
         break;
      case RGUI_SETTINGS_VIDEO_THREADED:
         strlcpy(type_str, g_settings.video.threaded ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
         snprintf(type_str, type_str_size, "%.1fx", g_settings.video.xscale);
         break;
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
         snprintf(type_str, type_str_size, "%.1fx", g_settings.video.yscale);
         break;
      case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
         strlcpy(type_str, g_settings.video.crop_overscan ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         snprintf(type_str, type_str_size, "%u", g_settings.video.hard_sync_frames);
         break;
      case RGUI_SETTINGS_DRIVER_VIDEO:
         strlcpy(type_str, g_settings.video.driver, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO:
         strlcpy(type_str, g_settings.audio.driver, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO_DEVICE:
         strlcpy(type_str, g_settings.audio.device, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO_RESAMPLER:
         strlcpy(type_str, g_settings.audio.resampler, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_INPUT:
         strlcpy(type_str, g_settings.input.driver, type_str_size);
         break;
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_DRIVER_CAMERA:
         strlcpy(type_str, g_settings.camera.driver, type_str_size);
         break;
#endif
#ifdef HAVE_LOCATION
      case RGUI_SETTINGS_DRIVER_LOCATION:
         strlcpy(type_str, g_settings.location.driver, type_str_size);
         break;
#endif
#ifdef HAVE_MENU
      case RGUI_SETTINGS_DRIVER_MENU:
         strlcpy(type_str, g_settings.menu.driver, type_str_size);
         break;
#endif
      case RGUI_SETTINGS_VIDEO_MONITOR_INDEX:
         if (g_settings.video.monitor_index)
            snprintf(type_str, type_str_size, "%u", g_settings.video.monitor_index);
         else
            strlcpy(type_str, "0 (Auto)", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
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
      case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
         strlcpy(type_str, g_settings.video.scale_integer ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         strlcpy(type_str, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, type_str_size);
         break;
#if defined(GEKKO)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         strlcpy(type_str, gx_get_video_mode(), type_str_size);
         break;
#elif defined(__CELLOS_LV2__)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         {
               unsigned width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               unsigned height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               snprintf(type_str, type_str_size, "%dx%d", width, height);
         }
         break;
      case RGUI_SETTINGS_VIDEO_PAL60:
         if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
            strlcpy(type_str, "ON", type_str_size);
         else
            strlcpy(type_str, "OFF", type_str_size);
         break;
#endif
      case RGUI_FILE_PLAIN:
         strlcpy(type_str, "(FILE)", type_str_size);
         *w = 6;
         break;
      case RGUI_FILE_DIRECTORY:
         strlcpy(type_str, "(DIR)", type_str_size);
         *w = 5;
         break;
      case RGUI_SETTINGS_REWIND_ENABLE:
         strlcpy(type_str, g_settings.rewind_enable ? "ON" : "OFF", type_str_size);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_GPU_SCREENSHOT:
         strlcpy(type_str, g_settings.video.gpu_screenshot ? "ON" : "OFF", type_str_size);
         break;
#endif
      case RGUI_SETTINGS_REWIND_GRANULARITY:
         snprintf(type_str, type_str_size, "%u", g_settings.rewind_granularity);
         break;
      case RGUI_SETTINGS_LIBRETRO_LOG_LEVEL:
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
      case RGUI_SETTINGS_LOGGING_VERBOSITY:
         strlcpy(type_str, g_extern.verbose ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
         strlcpy(type_str, g_extern.config_save_on_exit ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SAVESTATE_AUTO_SAVE:
         strlcpy(type_str, g_settings.savestate_auto_save ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SAVESTATE_AUTO_LOAD:
         strlcpy(type_str, g_settings.savestate_auto_load ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_BLOCK_SRAM_OVERWRITE:
         strlcpy(type_str, g_settings.block_sram_overwrite ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_PER_CORE_CONFIG:
         strlcpy(type_str, g_settings.core_specific_config ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
         if (g_settings.autosave_interval)
            snprintf(type_str, type_str_size, "%u seconds", g_settings.autosave_interval);
         else
            strlcpy(type_str, "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SAVESTATE_SAVE:
      case RGUI_SETTINGS_SAVESTATE_LOAD:
         if (g_extern.state_slot < 0)
            strlcpy(type_str, "-1 (auto)", type_str_size);
         else
            snprintf(type_str, type_str_size, "%d", g_extern.state_slot);
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         strlcpy(type_str, g_extern.audio_data.mute ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         snprintf(type_str, type_str_size, "%.3f", g_settings.audio.rate_control_delta);
         break;
      case RGUI_SETTINGS_FASTFORWARD_RATIO:
         if (g_settings.fastforward_ratio > 0.0f)
            snprintf(type_str, type_str_size, "%.1fx", g_settings.fastforward_ratio);
         else
            strlcpy(type_str, "No Limit", type_str_size);
         break;
      case RGUI_SETTINGS_SLOWMOTION_RATIO:
         snprintf(type_str, type_str_size, "%.1fx", g_settings.slowmotion_ratio);
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         snprintf(type_str, type_str_size, (g_settings.fps_show) ? "ON" : "OFF");
         break;
      case RGUI_BROWSER_DIR_PATH:
         strlcpy(type_str, *g_settings.rgui_content_directory ? g_settings.rgui_content_directory : "<default>", type_str_size);
         break;
      case RGUI_CONTENT_DIR_PATH:
         strlcpy(type_str, *g_settings.content_directory ? g_settings.content_directory : "<default>", type_str_size);
         break;
      case RGUI_ASSETS_DIR_PATH:
         strlcpy(type_str, *g_settings.assets_directory ? g_settings.assets_directory : "<default>", type_str_size);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SCREENSHOT_DIR_PATH:
         strlcpy(type_str, *g_settings.screenshot_directory ? g_settings.screenshot_directory : "<ROM dir>", type_str_size);
         break;
#endif
      case RGUI_SAVEFILE_DIR_PATH:
         strlcpy(type_str, *g_extern.savefile_dir ? g_extern.savefile_dir : "<ROM dir>", type_str_size);
         break;
#ifdef HAVE_OVERLAY
      case RGUI_OVERLAY_DIR_PATH:
         strlcpy(type_str, *g_extern.overlay_dir ? g_extern.overlay_dir : "<default>", type_str_size);
         break;
#endif
      case RGUI_SAVESTATE_DIR_PATH:
         strlcpy(type_str, *g_extern.savestate_dir ? g_extern.savestate_dir : "<ROM dir>", type_str_size);
         break;
      case RGUI_LIBRETRO_DIR_PATH:
         strlcpy(type_str, *g_settings.libretro_directory ? g_settings.libretro_directory : "<None>", type_str_size);
         break;
      case RGUI_LIBRETRO_INFO_DIR_PATH:
         strlcpy(type_str, *g_settings.libretro_info_path ? g_settings.libretro_info_path : "<Core dir>", type_str_size);
         break;
      case RGUI_CONFIG_DIR_PATH:
         strlcpy(type_str, *g_settings.rgui_config_directory ? g_settings.rgui_config_directory : "<default>", type_str_size);
         break;
      case RGUI_FILTER_DIR_PATH:
         strlcpy(type_str, *g_settings.video.filter_dir ? g_settings.video.filter_dir : "<default>", type_str_size);
         break;
      case RGUI_DSP_FILTER_DIR_PATH:
         strlcpy(type_str, *g_settings.audio.filter_dir ? g_settings.audio.filter_dir : "<default>", type_str_size);
         break;
      case RGUI_SHADER_DIR_PATH:
         strlcpy(type_str, *g_settings.video.shader_dir ? g_settings.video.shader_dir : "<default>", type_str_size);
         break;
      case RGUI_SYSTEM_DIR_PATH:
         strlcpy(type_str, *g_settings.system_directory ? g_settings.system_directory : "<ROM dir>", type_str_size);
         break;
      case RGUI_AUTOCONFIG_DIR_PATH:
         strlcpy(type_str, *g_settings.input.autoconfig_dir ? g_settings.input.autoconfig_dir : "<default>", type_str_size);
         break;
      case RGUI_SETTINGS_DISK_INDEX:
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
      case RGUI_SETTINGS_CONFIG:
         if (*g_extern.config_path)
            fill_pathname_base(type_str, g_extern.config_path, type_str_size);
         else
            strlcpy(type_str, "<default>", type_str_size);
         break;
      case RGUI_SETTINGS_OPEN_FILEBROWSER:
      case RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE:
      case RGUI_SETTINGS_OPEN_HISTORY:
      case RGUI_SETTINGS_CORE_OPTIONS:
      case RGUI_SETTINGS_CORE_INFO:
      case RGUI_SETTINGS_CUSTOM_VIEWPORT:
      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
      case RGUI_SETTINGS_VIDEO_OPTIONS:
      case RGUI_SETTINGS_FONT_OPTIONS:
      case RGUI_SETTINGS_AUDIO_OPTIONS:
      case RGUI_SETTINGS_DISK_OPTIONS:
#ifdef HAVE_SHADER_MANAGER
      case RGUI_SETTINGS_SHADER_OPTIONS:
      case RGUI_SETTINGS_SHADER_PRESET:
#endif
      case RGUI_SETTINGS_GENERAL_OPTIONS:
      case RGUI_SETTINGS_SHADER_PRESET_SAVE:
      case RGUI_SETTINGS_CORE:
      case RGUI_SETTINGS_DISK_APPEND:
      case RGUI_SETTINGS_INPUT_OPTIONS:
      case RGUI_SETTINGS_PATH_OPTIONS:
      case RGUI_SETTINGS_OVERLAY_OPTIONS:
      case RGUI_SETTINGS_NETPLAY_OPTIONS:
      case RGUI_SETTINGS_PRIVACY_OPTIONS:
      case RGUI_SETTINGS_OPTIONS:
      case RGUI_SETTINGS_DRIVERS:
      case RGUI_SETTINGS_CUSTOM_BIND_ALL:
      case RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         strlcpy(type_str, "...", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_SOFTFILTER:
         {
            const char *filter_name = rarch_softfilter_get_name(g_extern.filter.filter);
            strlcpy(type_str, filter_name ? filter_name : "N/A", type_str_size);
         }
         break;
      case RGUI_SETTINGS_AUDIO_DSP_FILTER:
         strlcpy(type_str, path_basename(g_settings.audio.dsp_plugin), type_str_size);
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_PRESET:
         strlcpy(type_str, path_basename(g_settings.input.overlay), type_str_size);
         break;
      case RGUI_SETTINGS_OVERLAY_OPACITY:
         snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_opacity);
         break;
      case RGUI_SETTINGS_OVERLAY_SCALE:
         snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_scale);
         break;
#endif
      case RGUI_SETTINGS_BIND_PLAYER:
         snprintf(type_str, type_str_size, "#%d", rgui->current_pad + 1);
         break;
      case RGUI_SETTINGS_BIND_DEVICE:
         {
            int map = g_settings.input.joypad_map[rgui->current_pad];
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
      case RGUI_SETTINGS_BIND_ANALOG_MODE:
         {
            static const char *modes[] = {
               "None",
               "Left Analog",
               "Right Analog",
               "Dual Analog",
            };

            strlcpy(type_str, modes[g_settings.input.analog_dpad_mode[rgui->current_pad] % ANALOG_DPAD_LAST], type_str_size);
         }
         break;
      case RGUI_SETTINGS_INPUT_AXIS_THRESHOLD:
         snprintf(type_str, type_str_size, "%.3f", g_settings.input.axis_threshold);
         break;
      case RGUI_SETTINGS_BIND_DEVICE_TYPE:
         {
            const struct retro_controller_description *desc;
            desc = NULL;
            if (rgui->current_pad < g_extern.system.num_ports)
            {
               desc = libretro_find_controller_description(&g_extern.system.ports[rgui->current_pad],
                     g_settings.input.libretro_device[rgui->current_pad]);
            }

            const char *name = desc ? desc->desc : NULL;
            if (!name) // Find generic name.
            {
               switch (g_settings.input.libretro_device[rgui->current_pad])
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
      case RGUI_SETTINGS_DEVICE_AUTODETECT_ENABLE:
         strlcpy(type_str, g_settings.input.autodetect_enable ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_CUSTOM_BIND_MODE:
         strlcpy(type_str, rgui->bind_mode_keyboard ? "Keyboard" : "Joypad", type_str_size);
         break;
      case RGUI_SETTINGS_BIND_UP:
      case RGUI_SETTINGS_BIND_DOWN:
      case RGUI_SETTINGS_BIND_LEFT:
      case RGUI_SETTINGS_BIND_RIGHT:
      case RGUI_SETTINGS_BIND_A:
      case RGUI_SETTINGS_BIND_B:
      case RGUI_SETTINGS_BIND_X:
      case RGUI_SETTINGS_BIND_Y:
      case RGUI_SETTINGS_BIND_START:
      case RGUI_SETTINGS_BIND_SELECT:
      case RGUI_SETTINGS_BIND_L:
      case RGUI_SETTINGS_BIND_R:
      case RGUI_SETTINGS_BIND_L2:
      case RGUI_SETTINGS_BIND_R2:
      case RGUI_SETTINGS_BIND_L3:
      case RGUI_SETTINGS_BIND_R3:
      case RGUI_SETTINGS_BIND_TURBO_ENABLE:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
      case RGUI_SETTINGS_BIND_FAST_FORWARD_KEY:
      case RGUI_SETTINGS_BIND_FAST_FORWARD_HOLD_KEY:
      case RGUI_SETTINGS_BIND_LOAD_STATE_KEY:
      case RGUI_SETTINGS_BIND_SAVE_STATE_KEY:
      case RGUI_SETTINGS_BIND_FULLSCREEN_TOGGLE_KEY:
      case RGUI_SETTINGS_BIND_QUIT_KEY:
      case RGUI_SETTINGS_BIND_STATE_SLOT_PLUS:
      case RGUI_SETTINGS_BIND_STATE_SLOT_MINUS:
      case RGUI_SETTINGS_BIND_REWIND:
      case RGUI_SETTINGS_BIND_MOVIE_RECORD_TOGGLE:
      case RGUI_SETTINGS_BIND_PAUSE_TOGGLE:
      case RGUI_SETTINGS_BIND_FRAMEADVANCE:
      case RGUI_SETTINGS_BIND_RESET:
      case RGUI_SETTINGS_BIND_SHADER_NEXT:
      case RGUI_SETTINGS_BIND_SHADER_PREV:
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_PLUS:
      case RGUI_SETTINGS_BIND_CHEAT_INDEX_MINUS:
      case RGUI_SETTINGS_BIND_CHEAT_TOGGLE:
      case RGUI_SETTINGS_BIND_SCREENSHOT:
      case RGUI_SETTINGS_BIND_MUTE:
      case RGUI_SETTINGS_BIND_NETPLAY_FLIP:
      case RGUI_SETTINGS_BIND_SLOWMOTION:
      case RGUI_SETTINGS_BIND_ENABLE_HOTKEY:
      case RGUI_SETTINGS_BIND_VOLUME_UP:
      case RGUI_SETTINGS_BIND_VOLUME_DOWN:
      case RGUI_SETTINGS_BIND_OVERLAY_NEXT:
      case RGUI_SETTINGS_BIND_DISK_EJECT_TOGGLE:
      case RGUI_SETTINGS_BIND_DISK_NEXT:
      case RGUI_SETTINGS_BIND_GRAB_MOUSE_TOGGLE:
      case RGUI_SETTINGS_BIND_MENU_TOGGLE:
         input_get_bind_string(type_str, &g_settings.input.binds[rgui->current_pad][type - RGUI_SETTINGS_BIND_BEGIN], type_str_size);
         break;
      case RGUI_SETTINGS_AUDIO_DSP_EFFECT:
#ifdef RARCH_CONSOLE
         strlcpy(type_str, (g_extern.console.sound.volume_level) ? "Loud" : "Normal", type_str_size);
         break;
#endif
      case RGUI_SETTINGS_AUDIO_VOLUME:
         snprintf(type_str, type_str_size, "%.1f dB", g_extern.audio_data.volume_db);
         break;
#ifdef _XBOX1
      case RGUI_SETTINGS_FLICKER_FILTER:
         snprintf(type_str, type_str_size, "%d", g_extern.console.screen.flicker_filter_index);
         break;
      case RGUI_SETTINGS_SOFT_DISPLAY_FILTER:
         snprintf(type_str, type_str_size,
               (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
         break;
#endif
      case RGUI_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE:
         strlcpy(type_str, (g_extern.lifecycle_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_PAUSE_IF_WINDOW_FOCUS_LOST:
         strlcpy(type_str, g_settings.pause_nonactive ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_WINDOW_COMPOSITING_ENABLE:
         strlcpy(type_str, g_settings.video.disable_composition ? "OFF" : "ON", type_str_size);
         break;
#ifdef HAVE_NETPLAY
      case RGUI_SETTINGS_NETPLAY_ENABLE:
         strlcpy(type_str, g_extern.netplay_enable ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_NETPLAY_HOST_IP_ADDRESS:
         strlcpy(type_str, g_extern.netplay_server, type_str_size);
         break;
      case RGUI_SETTINGS_NETPLAY_DELAY_FRAMES:
         snprintf(type_str, type_str_size, "%d", g_extern.netplay_sync_frames);
         break;
      case RGUI_SETTINGS_NETPLAY_TCP_UDP_PORT:
         snprintf(type_str, type_str_size, "%d", g_extern.netplay_port ? g_extern.netplay_port : RARCH_DEFAULT_PORT);
         break;
      case RGUI_SETTINGS_NETPLAY_NICKNAME:
         snprintf(type_str, type_str_size, "%s", g_extern.netplay_nick);
         break;
      case RGUI_SETTINGS_NETPLAY_MODE:
         snprintf(type_str, type_str_size, g_extern.netplay_is_client ? "Client" : "Server");
         break;
      case RGUI_SETTINGS_NETPLAY_SPECTATOR_MODE_ENABLE:
         snprintf(type_str, type_str_size, g_extern.netplay_is_spectate ? "ON" : "OFF");
         break;
#endif
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_PRIVACY_CAMERA_ALLOW:
         snprintf(type_str, type_str_size, g_settings.camera.allow ? "ON" : "OFF");
         break;
#endif
#ifdef HAVE_LOCATION
      case RGUI_SETTINGS_PRIVACY_LOCATION_ALLOW:
         snprintf(type_str, type_str_size, g_settings.location.allow ? "ON" : "OFF");
         break;
#endif
#ifdef HAVE_OSK
      case RGUI_SETTINGS_ONSCREEN_KEYBOARD_ENABLE:
         snprintf(type_str, type_str_size, g_settings.osk.enable ? "ON" : "OFF");
         break;
#endif
      case RGUI_SETTINGS_FONT_ENABLE:
         snprintf(type_str, type_str_size, g_settings.video.font_enable ? "ON" : "OFF");
         break;
      case RGUI_SETTINGS_FONT_SCALE:
         snprintf(type_str, type_str_size, g_settings.video.font_scale ? "ON" : "OFF");
         break;
      case RGUI_SETTINGS_FONT_SIZE:
         snprintf(type_str, type_str_size, "%.1f", g_settings.video.font_size);
         break;
      case RGUI_SETTINGS_LOAD_DUMMY_ON_CORE_SHUTDOWN:
         snprintf(type_str, type_str_size, g_settings.load_dummy_on_core_shutdown ? "ON" : "OFF");
         break;
      default:
         *type_str = '\0';
         *w = 0;
         break;
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
