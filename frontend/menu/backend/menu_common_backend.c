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
#include "../../../gfx/shader_common.h"
#include "../../../driver.h"
#include "../../../file_ext.h"
#include "../../../input/input_common.h"
#include "../../../config.def.h"
#include "../../../input/keyboard_line.h"

#include "../../../settings_data.h"

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif
#endif

static inline struct gfx_shader *shader_manager_get_current_shader(menu_handle_t *menu, unsigned type)
{
   if (type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
      return menu->shader;
   else if (driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);

   return NULL;
}

static void add_entry(menu_handle_t *menu, const char *label, unsigned id,
      rarch_setting_t *settings)
{
   rarch_setting_t *setting = (rarch_setting_t*)
      setting_data_find_setting(settings, label);

   if (setting)
      file_list_push(menu->selection_buf, setting->short_description, setting->name,
            id, 0);
}

static void menu_common_entries_init(menu_handle_t *menu, unsigned menu_type)
{
   unsigned i;
   char tmp[256];
   rarch_setting_t *current_setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)setting_data_get_list();

   switch (menu_type)
   {
      case MENU_SETTINGS_SHADER_PARAMETERS:
      case MENU_SETTINGS_SHADER_PRESET_PARAMETERS:
         {
            file_list_clear(menu->selection_buf);

            struct gfx_shader *shader = (struct gfx_shader*)
               shader_manager_get_current_shader(menu, menu_type);

            if (shader)
               for (i = 0; i < shader->num_parameters; i++)
                  file_list_push(menu->selection_buf,
                        shader->parameters[i].desc, "",
                        MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
            menu->parameter_shader = shader;
            break;
         }
      case MENU_SETTINGS_SHADER_OPTIONS:
         {
            struct gfx_shader *shader = (struct gfx_shader*)menu->shader;

            if (!shader)
               return;

            file_list_clear(menu->selection_buf);
            file_list_push(menu->selection_buf, "Apply Shader Changes", "",
                  MENU_SETTINGS_SHADER_APPLY, 0);
            file_list_push(menu->selection_buf, "Default Filter", "",
                  MENU_SETTINGS_SHADER_FILTER, 0);
            file_list_push(menu->selection_buf, "Load Shader Preset", "",
                  MENU_SETTINGS_SHADER_PRESET, 0);
            file_list_push(menu->selection_buf, "Save As Shader Preset", "",
                  MENU_SETTINGS_SHADER_PRESET_SAVE, 0);
            file_list_push(menu->selection_buf, "Parameters (Current)", "",
                  MENU_SETTINGS_SHADER_PARAMETERS, 0);
            file_list_push(menu->selection_buf, "Parameters (Menu)", "",
                  MENU_SETTINGS_SHADER_PRESET_PARAMETERS, 0);
            file_list_push(menu->selection_buf, "Shader Passes", "",
                  MENU_SETTINGS_SHADER_PASSES, 0);

            for (i = 0; i < shader->passes; i++)
            {
               char buf[64];

               snprintf(buf, sizeof(buf), "Shader #%u", i);
               file_list_push(menu->selection_buf, buf, "",
                     MENU_SETTINGS_SHADER_0 + 3 * i, 0);

               snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
               file_list_push(menu->selection_buf, buf, "",
                     MENU_SETTINGS_SHADER_0_FILTER + 3 * i, 0);

               snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
               file_list_push(menu->selection_buf, buf, "",
                     MENU_SETTINGS_SHADER_0_SCALE + 3 * i, 0);
            }
         }
         break;
      case MENU_SETTINGS_GENERAL_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"libretro_log_level", 0, setting_data);
         add_entry(menu,"log_verbosity", 0, setting_data);
         add_entry(menu,"perfcnt_enable", 0, setting_data);
         add_entry(menu,"game_history_size", 0, setting_data);
         add_entry(menu,"config_save_on_exit", 0, setting_data);
         add_entry(menu,"core_specific_config", 0, setting_data);
         add_entry(menu,"video_gpu_screenshot", 0, setting_data);
         add_entry(menu,"dummy_on_core_shutdown", 0, setting_data);
         add_entry(menu,"fps_show", 0, setting_data);
         add_entry(menu,"fastforward_ratio", 0, setting_data);
         add_entry(menu,"slowmotion_ratio", 0, setting_data);
         add_entry(menu,"rewind_enable", 0, setting_data);
         add_entry(menu,"rewind_granularity", 0, setting_data);
         add_entry(menu,"block_sram_overwrite", 0, setting_data);
         add_entry(menu,"autosave_interval", 0, setting_data);
         add_entry(menu,"video_disable_composition", 0, setting_data);
         add_entry(menu,"pause_nonactive", 0, setting_data);
         add_entry(menu,"savestate_auto_save", 0, setting_data);
         add_entry(menu,"savestate_auto_load", 0, setting_data);
         break;
      case MENU_SETTINGS_VIDEO_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"video_shared_context", 0, setting_data);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         file_list_push(menu->selection_buf, "Screen Resolution", "",
               MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         add_entry(menu,"video_viwidth", 0, setting_data);
         add_entry(menu,"video_filter", MENU_SETTINGS_VIDEO_SOFTFILTER, setting_data);
#if defined(__CELLOS_LV2__)
         file_list_push(menu->selection_buf, "PAL60 Mode", "",
               MENU_SETTINGS_VIDEO_PAL60, 0);
#endif
         add_entry(menu,"video_smooth", 0, setting_data);
#ifdef HW_RVL
         file_list_push(menu->selection_buf, "VI Trap filtering", "",
               MENU_SETTINGS_VIDEO_SOFT_FILTER, 0);
#endif
         add_entry(menu,"video_gamma", MENU_SETTINGS_VIDEO_GAMMA, setting_data);
#ifdef _XBOX1
         file_list_push(menu->selection_buf, "Soft filtering", "",
               MENU_SETTINGS_SOFT_DISPLAY_FILTER, 0);
#endif
         add_entry(menu,"video_filter_flicker", 0,
               setting_data);
         add_entry(menu,"video_scale_integer", 0, setting_data);
         add_entry(menu,"aspect_ratio_index", 0, setting_data);
         file_list_push(menu->selection_buf, "Custom Ratio", "",
               MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
         add_entry(menu,"video_fullscreen", 0, setting_data);
         add_entry(menu,"video_rotation", 0, setting_data);
         add_entry(menu,"video_vsync", 0, setting_data);
         add_entry(menu,"video_hard_sync", 0, setting_data);
         add_entry(menu,"video_hard_sync_frames", 0, setting_data);
         add_entry(menu,"video_frame_delay", 0, setting_data);
         add_entry(menu,"video_black_frame_insertion", 0, setting_data);
         add_entry(menu,"video_swap_interval", 0, setting_data);
         add_entry(menu,"video_threaded", 0, setting_data);
         add_entry(menu,"video_scale", 0, setting_data);
         add_entry(menu,"video_crop_overscan", 0, setting_data);
         add_entry(menu,"video_monitor_index", 0, setting_data);
         add_entry(menu,"video_refresh_rate", 0, setting_data);
         add_entry(menu,"video_refresh_rate_auto", 0, setting_data);
         break;
      case MENU_SETTINGS_FONT_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"video_font_enable", 0, setting_data);
         add_entry(menu,"video_font_size", 0, setting_data);
         break;
      case MENU_SETTINGS_CORE_OPTIONS:
         file_list_clear(menu->selection_buf);

         if (g_extern.system.core_options)
         {
            size_t i, opts;

            opts = core_option_size(g_extern.system.core_options);
            for (i = 0; i < opts; i++)
               file_list_push(menu->selection_buf,
                     core_option_get_desc(g_extern.system.core_options, i), "",
                     MENU_SETTINGS_CORE_OPTION_START + i, 0);
         }
         else
            file_list_push(menu->selection_buf, "No options available.", "",
                  MENU_SETTINGS_CORE_OPTION_NONE, 0);
         break;
      case MENU_SETTINGS_CORE_INFO:
         {
            core_info_t *info = (core_info_t*)menu->core_info_current;
            file_list_clear(menu->selection_buf);

            if (info->data)
            {
               snprintf(tmp, sizeof(tmp), "Core name: %s",
                     info->display_name ? info->display_name : "");
               file_list_push(menu->selection_buf, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);

               if (info->authors_list)
               {
                  strlcpy(tmp, "Authors: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp),
                        info->authors_list, ", ");
                  file_list_push(menu->selection_buf, tmp, "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->permissions_list)
               {
                  strlcpy(tmp, "Permissions: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp),
                        info->permissions_list, ", ");
                  file_list_push(menu->selection_buf, tmp, "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->supported_extensions_list)
               {
                  strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
                  string_list_join_concat(tmp, sizeof(tmp),
                        info->supported_extensions_list, ", ");
                  file_list_push(menu->selection_buf, tmp, "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);
               }

               if (info->firmware_count > 0)
               {
                  core_info_list_update_missing_firmware(menu->core_info, info->path,
                        g_settings.system_directory);

                  file_list_push(menu->selection_buf, "Firmware: ", "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);
                  for (i = 0; i < info->firmware_count; i++)
                  {
                     if (info->firmware[i].desc)
                     {
                        snprintf(tmp, sizeof(tmp), "	name: %s",
                              info->firmware[i].desc ? info->firmware[i].desc : "");
                        file_list_push(menu->selection_buf, tmp, "",
                              MENU_SETTINGS_CORE_INFO_NONE, 0);

                        snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                              info->firmware[i].missing ?
                              "missing" : "present",
                              info->firmware[i].optional ?
                              "optional" : "required");
                        file_list_push(menu->selection_buf, tmp, "",
                              MENU_SETTINGS_CORE_INFO_NONE, 0);
                     }
                  }
               }

               if (info->notes)
               {
                  snprintf(tmp, sizeof(tmp), "Core notes: ");
                  file_list_push(menu->selection_buf, tmp, "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);

                  for (i = 0; i < info->note_list->size; i++)
                  {
                     snprintf(tmp, sizeof(tmp), " %s",
                           info->note_list->elems[i].data);
                     file_list_push(menu->selection_buf, tmp, "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);
                  }
               }
            }
            else
               file_list_push(menu->selection_buf,
                     "No information available.", "",
                     MENU_SETTINGS_CORE_OPTION_NONE, 0);
         }
         break;
      case MENU_SETTINGS_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"Driver Options", MENU_SETTINGS_DRIVERS, setting_data);
         add_entry(menu,"General Options", MENU_SETTINGS_GENERAL_OPTIONS, setting_data);
         add_entry(menu,"Video Options", MENU_SETTINGS_VIDEO_OPTIONS, setting_data);
         add_entry(menu,"Shader Options", MENU_SETTINGS_SHADER_OPTIONS, setting_data);
         add_entry(menu,"Font Options", MENU_SETTINGS_FONT_OPTIONS, setting_data);
         add_entry(menu,"Audio Options", MENU_SETTINGS_AUDIO_OPTIONS, setting_data);
         add_entry(menu,"Input Options", MENU_SETTINGS_INPUT_OPTIONS, setting_data);
         add_entry(menu,"Overlay Options", MENU_SETTINGS_OVERLAY_OPTIONS,
               setting_data);
         add_entry(menu,"User Options", MENU_SETTINGS_USER_OPTIONS, setting_data);
         add_entry(menu,"Netplay Options", MENU_SETTINGS_NETPLAY_OPTIONS,
               setting_data);
         add_entry(menu,"Path Options", MENU_SETTINGS_PATH_OPTIONS, setting_data);
         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            if (g_extern.system.disk_control.get_num_images)
               file_list_push(menu->selection_buf, "Disk Options", "",
                     MENU_SETTINGS_DISK_OPTIONS, 0);
         }
         add_entry(menu,"Privacy Options", MENU_SETTINGS_PRIVACY_OPTIONS,
               setting_data);
         break;
      case MENU_SETTINGS_PRIVACY_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"camera_allow", 0, setting_data);
         add_entry(menu,"location_allow", 0, setting_data);
         break;
      case MENU_SETTINGS_DISK_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Disk Index", "",
               MENU_SETTINGS_DISK_INDEX, 0);
         file_list_push(menu->selection_buf, "Disk Image Append", "",
               MENU_SETTINGS_DISK_APPEND, 0);
         break;
      case MENU_SETTINGS_OVERLAY_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"input_overlay", MENU_SETTINGS_OVERLAY_PRESET,
               setting_data);
         add_entry(menu,"input_overlay_opacity", 0, setting_data);
         add_entry(menu,"input_overlay_scale", 0, setting_data);
         break;
      case MENU_SETTINGS_USER_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"netplay_nickname", MENU_SETTINGS_NETPLAY_NICKNAME,
               setting_data);
         add_entry(menu,"user_language", 0, setting_data);
         break;
      case MENU_SETTINGS_NETPLAY_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"netplay_enable", 0, setting_data);
         add_entry(menu,"netplay_mode", 0, setting_data);
         add_entry(menu,"netplay_spectator_mode_enable", 0, setting_data);
         add_entry(menu,"netplay_ip_address",
               MENU_SETTINGS_NETPLAY_HOST_IP_ADDRESS, setting_data);
         add_entry(menu,"netplay_tcp_udp_port", 0, setting_data);
         add_entry(menu,"netplay_delay_frames", 0, setting_data);
         break;
      case MENU_SETTINGS_PATH_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"rgui_browser_directory",
               MENU_BROWSER_DIR_PATH, setting_data);
         add_entry(menu,"content_directory",
               MENU_CONTENT_DIR_PATH, setting_data);
         add_entry(menu,"assets_directory",
               MENU_ASSETS_DIR_PATH, setting_data);
         add_entry(menu,"rgui_config_directory",
               MENU_CONFIG_DIR_PATH, setting_data);
         add_entry(menu,"libretro_dir_path",
               MENU_LIBRETRO_DIR_PATH, setting_data);
         add_entry(menu,"libretro_info_path",
               MENU_LIBRETRO_INFO_DIR_PATH, setting_data);
         add_entry(menu,"game_history_path",
               MENU_CONTENT_HISTORY_PATH, setting_data);
         add_entry(menu,"video_filter_dir",
               MENU_FILTER_DIR_PATH, setting_data);
         add_entry(menu,"audio_filter_dir",
               MENU_DSP_FILTER_DIR_PATH, setting_data);
         add_entry(menu,"video_shader_dir", MENU_SHADER_DIR_PATH, setting_data);
         add_entry(menu,"savestate_directory", MENU_SAVESTATE_DIR_PATH,
               setting_data);
         add_entry(menu,"savefile_directory", MENU_SAVEFILE_DIR_PATH, setting_data);
         add_entry(menu,"overlay_directory", MENU_OVERLAY_DIR_PATH, setting_data);
         add_entry(menu,"system_directory", MENU_SYSTEM_DIR_PATH, setting_data);
         add_entry(menu,"screenshot_directory", MENU_SCREENSHOT_DIR_PATH,
               setting_data);
         add_entry(menu,"joypad_autoconfig_dir", MENU_AUTOCONFIG_DIR_PATH,
               setting_data);
         add_entry(menu,"extraction_directory", MENU_EXTRACTION_DIR_PATH,
               setting_data);
         break;
      case MENU_SETTINGS_INPUT_OPTIONS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Player", "",
               MENU_SETTINGS_BIND_PLAYER, 0);
         file_list_push(menu->selection_buf, "Device", "",
               MENU_SETTINGS_BIND_DEVICE, 0);
         file_list_push(menu->selection_buf, "Device Type", "",
               MENU_SETTINGS_BIND_DEVICE_TYPE, 0);
         file_list_push(menu->selection_buf, "Analog D-pad Mode", "",
               MENU_SETTINGS_BIND_ANALOG_MODE, 0);
         add_entry(menu,"input_axis_threshold", 0, setting_data);
         add_entry(menu,"input_autodetect_enable", 0, setting_data);
         file_list_push(menu->selection_buf, "Bind Mode", "",
               MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
         file_list_push(menu->selection_buf, "Configure All (RetroPad)", "",
               MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
         file_list_push(menu->selection_buf, "Default All (RetroPad)", "",
               MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
         add_entry(menu,"osk_enable", MENU_SETTINGS_ONSCREEN_KEYBOARD_ENABLE,
               setting_data);
         for (i = MENU_SETTINGS_BIND_BEGIN; i <= MENU_SETTINGS_BIND_ALL_LAST; i++)
            file_list_push(menu->selection_buf,
                  input_config_bind_map[i - MENU_SETTINGS_BIND_BEGIN].desc,
                  "", i, 0);
         break;
      case MENU_SETTINGS_AUDIO_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"audio_dsp_plugin", MENU_SETTINGS_AUDIO_DSP_FILTER, setting_data);
         add_entry(menu,"audio_enable", 0, setting_data);
         add_entry(menu,"audio_mute", 0, setting_data);
         add_entry(menu,"audio_latency", 0, setting_data);
         add_entry(menu,"audio_sync", 0, setting_data);
         add_entry(menu,"audio_rate_control_delta", 0, setting_data);
#ifdef __CELLOS_LV2__
         file_list_push(menu->selection_buf, "System BGM Control", "",
               MENU_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE, 0);
#endif
         add_entry(menu,"audio_volume", 0, setting_data);
         add_entry(menu,"audio_device", MENU_SETTINGS_DRIVER_AUDIO_DEVICE, setting_data);
         break;
      case MENU_SETTINGS_DRIVERS:
         file_list_clear(menu->selection_buf);
         add_entry(menu,"video_driver", MENU_SETTINGS_DRIVER_VIDEO, setting_data);
         add_entry(menu,"audio_driver", MENU_SETTINGS_DRIVER_AUDIO, setting_data);
         add_entry(menu,"audio_resampler_driver",
               MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER, setting_data);
         add_entry(menu,"input_driver", MENU_SETTINGS_DRIVER_INPUT, setting_data);
         add_entry(menu,"camera_driver", MENU_SETTINGS_DRIVER_CAMERA, setting_data);
         add_entry(menu,"location_driver", MENU_SETTINGS_DRIVER_LOCATION,
               setting_data);
         add_entry(menu,"menu_driver", MENU_SETTINGS_DRIVER_MENU, setting_data);
         break;
      case MENU_SETTINGS_PERFORMANCE_COUNTERS:
         file_list_clear(menu->selection_buf);
         file_list_push(menu->selection_buf, "Frontend Counters", "",
               MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND, 0);
         file_list_push(menu->selection_buf, "Core Counters", "",
               MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO, 0);
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
                  file_list_push(menu->selection_buf, counters[i]->ident, "",
                        MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN + i, 0);
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
                  file_list_push(menu->selection_buf, counters[i]->ident, "",
                        MENU_SETTINGS_PERF_COUNTERS_BEGIN + i, 0);
         }
         break;
      case MENU_SETTINGS:
         setting_data = (rarch_setting_t *)setting_data_get_mainmenu(true);
         file_list_clear(menu->selection_buf);
         add_entry(menu,"core_list", MENU_SETTINGS_CORE, setting_data);
         add_entry(menu,"history_list", MENU_SETTINGS_OPEN_HISTORY, setting_data);
         add_entry(menu,"detect_core_list", MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, setting_data);
         add_entry(menu,"load_content", MENU_SETTINGS_OPEN_FILEBROWSER, setting_data);
         add_entry(menu,"core_options", MENU_SETTINGS_CORE_OPTIONS, setting_data);
         add_entry(menu,"core_information", MENU_SETTINGS_CORE_INFO, setting_data);
         add_entry(menu,"settings", MENU_SETTINGS_OPTIONS, setting_data);
         add_entry(menu,"performance_counters", MENU_SETTINGS_PERFORMANCE_COUNTERS, setting_data);
         add_entry(menu,"savestate", MENU_SETTINGS_SAVESTATE_SAVE, setting_data);
         add_entry(menu,"loadstate", MENU_SETTINGS_SAVESTATE_LOAD, setting_data);
         add_entry(menu,"take_screenshot", 0, setting_data);
         add_entry(menu,"resume_content", 0, setting_data);
         add_entry(menu,"restart_content", 0, setting_data);
         add_entry(menu,"restart_retroarch", 0, setting_data);
         add_entry(menu,"configurations", MENU_SETTINGS_CONFIG, setting_data);
         add_entry(menu,"save_new_config", 0, setting_data);
         add_entry(menu,"help", MENU_START_SCREEN, setting_data);
         add_entry(menu,"quit_retroarch", 0, setting_data);
         break;
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, menu_type);
}

static void *get_last_setting(const file_list_t *list, int index,
      rarch_setting_t *settings)
{
   if (settings)
      return (rarch_setting_t*)setting_data_find_setting(settings,
            list->list[index].label);
   return NULL;
}

static int menu_info_screen_iterate(unsigned action)
{
   char msg[PATH_MAX];
   rarch_setting_t *current_setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)setting_data_get_list();

   if (!driver.menu || !setting_data)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   current_setting = (rarch_setting_t*)get_last_setting(
         driver.menu->selection_buf,
         driver.menu->selection_ptr,
         setting_data_get_list());

   if (current_setting)
      setting_data_get_description(current_setting, msg, sizeof(msg));
   else
   {
      current_setting = (rarch_setting_t*)get_last_setting(
            driver.menu->selection_buf,
            driver.menu->selection_ptr,
            setting_data_get_mainmenu(true));

      if (current_setting)
         setting_data_get_description(current_setting, msg, sizeof(msg));
      else
      {
         switch (driver.menu->info_selection)
         {
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
      }
   }

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

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
      const struct retro_keybind *bind = (const struct retro_keybind*)&g_settings.input.binds[0][binds[i]];
      const struct retro_keybind *auto_bind = (const struct retro_keybind*)input_get_auto_bind(0, binds[i]);

      input_get_bind_string(desc[i], bind, auto_bind, sizeof(desc[i]));
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
   if (
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
         type == MENU_SETTINGS_USER_OPTIONS ||
         type == MENU_SETTINGS_NETPLAY_OPTIONS ||
         type == MENU_SETTINGS_OPTIONS ||
         type == MENU_SETTINGS_DRIVERS ||
         type == MENU_SETTINGS_PERFORMANCE_COUNTERS ||
         type == MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO ||
         type == MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND ||
         type == MENU_SETTINGS_INPUT_OPTIONS
         )
         return MENU_SETTINGS;

   if (
         (
          type >= MENU_SETTINGS_SHADER_0 &&
          type <= MENU_SETTINGS_SHADER_LAST &&
          (((type - MENU_SETTINGS_SHADER_0) % 3) == 0)) ||
         type == MENU_SETTINGS_SHADER_PRESET)
      return MENU_SETTINGS_SHADER_OPTIONS;

   if (
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
         type == MENU_SYSTEM_DIR_PATH)
      return MENU_FILE_DIRECTORY;

   return 0;
}


static void menu_common_setting_push_current_menu(file_list_t *list, const char *path, unsigned type,
      size_t directory_ptr, unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
         file_list_push(list, path, "", type, directory_ptr);
         menu_clear_navigation(driver.menu);
         driver.menu->need_refresh = true;
         break;
   }
}

static void menu_action_cancel(void)
{
   file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
   driver.menu->need_refresh = true;
}

static int menu_settings_iterate(unsigned action)
{
   const char *label = NULL;
   const char *dir = NULL;
   unsigned type = 0;
   unsigned menu_type = 0;

   if (!driver.menu)
      return 0;

   driver.menu->frame_buf_pitch = driver.menu->width * 2;

   if (action != MENU_ACTION_REFRESH)
      file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &label, &type);

   if (type == MENU_SETTINGS_CORE)
      label = g_settings.libretro_directory;
   else if (type == MENU_SETTINGS_CONFIG)
      label = g_settings.menu_config_directory;
   else if (type == MENU_SETTINGS_DISK_APPEND)
      label = g_settings.menu_content_directory;

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
         if ((driver.menu->selection_ptr + 1) < file_list_get_size(driver.menu->selection_buf))
            menu_increment_navigation(driver.menu);
         else
            menu_clear_navigation(driver.menu);
         break;

      case MENU_ACTION_CANCEL:
         if (file_list_get_size(driver.menu->menu_stack) > 1)
            menu_action_cancel();
         break;
      case MENU_ACTION_SELECT:
         {
            const char *path = NULL;
            file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &path, &driver.menu->info_selection);
            file_list_push(driver.menu->menu_stack, "", "",
                  MENU_INFO_SCREEN, driver.menu->selection_ptr);
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
            menu_common_setting_push_current_menu(driver.menu->menu_stack, g_settings.menu_content_directory, MENU_FILE_DIRECTORY, driver.menu->selection_ptr, action);
         }
         else if ((type == MENU_SETTINGS_OPEN_HISTORY || menu_common_type_is(type) == MENU_FILE_DIRECTORY) && action == MENU_ACTION_OK)
            menu_common_setting_push_current_menu(driver.menu->menu_stack, "", type, driver.menu->selection_ptr, action);
         else if ((menu_common_type_is(type) == MENU_SETTINGS || type == MENU_SETTINGS_CORE || type == MENU_SETTINGS_CONFIG || type == MENU_SETTINGS_DISK_APPEND) && action == MENU_ACTION_OK)
            menu_common_setting_push_current_menu(driver.menu->menu_stack, label, type, driver.menu->selection_ptr, action);
         else if (type == MENU_SETTINGS_CUSTOM_VIEWPORT && action == MENU_ACTION_OK)
         {
            file_list_push(driver.menu->menu_stack, "", "",
                  type, driver.menu->selection_ptr);

            // Start with something sane.
            rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;

            if (driver.video_data && driver.video && driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, custom);
            aspectratio_lut[ASPECT_RATIO_CUSTOM].value = (float)custom->width / custom->height;

            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

            rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
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
            menu_type == MENU_CONTENT_HISTORY_PATH ||
            menu_type == MENU_SETTINGS_CORE ||
            menu_type == MENU_SETTINGS_CONFIG ||
            menu_type == MENU_SETTINGS_DISK_APPEND ||
            menu_type == MENU_SETTINGS_OPEN_HISTORY))
   {
      driver.menu->need_refresh = false;
      if (menu_common_type_is(menu_type) == MENU_SETTINGS)
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
      file_list_push(driver.menu->menu_stack, "", "", MENU_START_SCREEN, 0);
   }

   return 0;
}

static int menu_viewport_iterate(unsigned action)
{
   int stride_x, stride_y;
   char msg[64];
   struct retro_game_geometry *geom;
   const char *base_msg = NULL;
   unsigned menu_type = 0;
   rarch_viewport_t *custom = (rarch_viewport_t*)&g_extern.console.screen.viewports.custom_vp;

   if (!driver.menu)
      return 0;

   file_list_get_last(driver.menu->menu_stack, NULL, &menu_type);

   geom = (struct retro_game_geometry*)&g_extern.system.av_info.geometry;
   stride_x = g_settings.video.scale_integer ?
      geom->base_width : 1;
   stride_y = g_settings.video.scale_integer ?
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

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
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

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_LEFT:
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
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

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_CANCEL:
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT_2)
         {
            file_list_push(driver.menu->menu_stack, "", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  driver.menu->selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            file_list_push(driver.menu->menu_stack, "", "",
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

            rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
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

   rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

static void menu_parse_and_resolve(unsigned menu_type)
{
   size_t i, list_size;
   file_list_t *list = NULL;
   const core_info_t *info = NULL;
   const char *dir = NULL;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot parse and resolve menu, menu handle is not initialized.\n");
      return;
   }

   file_list_clear(driver.menu->selection_buf);

   // parsing switch
   switch (menu_type)
   {
      case MENU_SETTINGS_OPEN_HISTORY:
         /* History parse */
         list_size = content_playlist_size(g_extern.history);

         for (i = 0; i < list_size; i++)
         {
            char fill_buf[PATH_MAX];
            const char *path      = NULL;
            const char *core_path = NULL;
            const char *core_name = NULL;

            content_playlist_get_index(g_extern.history, i,
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

            file_list_push(driver.menu->selection_buf, fill_buf, "",
                  MENU_FILE_PLAIN, 0);
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
               file_list_push(driver.menu->selection_buf,
                     "sd:/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "usb:/", "", menu_type, 0);
#endif
               file_list_push(driver.menu->selection_buf,
                     "carda:/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "cardb:/", "", menu_type, 0);
#elif defined(_XBOX1)
               file_list_push(driver.menu->selection_buf,
                     "C:", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "D:", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "E:", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "F:", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "G:", "", menu_type, 0);
#elif defined(_XBOX360)
               file_list_push(driver.menu->selection_buf,
                     "game:", "", menu_type, 0);
#elif defined(_WIN32)
               unsigned drives = GetLogicalDrives();
               char drive[] = " :\\";
               for (i = 0; i < 32; i++)
               {
                  drive[0] = 'A' + i;
                  if (drives & (1 << i))
                     file_list_push(driver.menu->selection_buf,
                           drive, "", menu_type, 0);
               }
#elif defined(__CELLOS_LV2__)
               file_list_push(driver.menu->selection_buf,
                     "/app_home/",   "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_hdd0/",   "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_hdd1/",   "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/host_root/",  "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb000/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb001/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb002/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb003/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb004/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb005/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "/dev_usb006/", "", menu_type, 0);
#elif defined(PSP)
               file_list_push(driver.menu->selection_buf,
                     "ms0:/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "ef0:/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     "host0:/", "", menu_type, 0);
#elif defined(IOS)
               file_list_push(driver.menu->selection_buf,
                     "/var/mobile/", "", menu_type, 0);
               file_list_push(driver.menu->selection_buf,
                     g_defaults.core_dir, "",menu_type, 0);
               file_list_push(driver.menu->selection_buf, "/", "",
                     menu_type, 0);
#else
               file_list_push(driver.menu->selection_buf, "/", "",
                     menu_type, 0);
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
               exts = "filt";
            else if (menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER)
               exts = "dsp";
            else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
               exts = "cfg";
            else if (menu_type == MENU_CONTENT_HISTORY_PATH)
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
               file_list_push(driver.menu->selection_buf, "<Use this directory>", "",
                     MENU_FILE_USE_DIRECTORY, 0);

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
               file_list_push(driver.menu->selection_buf, path, "",
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
            char core_path[PATH_MAX], display_name[256];
            const char *path = NULL;
            unsigned type = 0;

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
            file_list_push(driver.menu->selection_buf, info[i].path, "",
                  MENU_FILE_PLAIN, 0);
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
   bool timed_out = false;
   menu_handle_t *menu = (menu_handle_t*)data;

   (void)action; // Have to ignore action here.

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   int64_t current = rarch_get_time_usec();
   int timeout = (menu->binds.timeout_end - current) / 1000000;

   snprintf(msg, sizeof(msg), "[%s]\npress keyboard\n(timeout %d seconds)",
         input_config_bind_map[menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc, timeout);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

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
   if (driver.menu)
   {
      menu_flush_stack_type(MENU_SETTINGS);
      driver.menu->msg_force = true;
   }
}

static void menu_common_defer_decision_manual(void)
{
   if (driver.menu)
      menu_common_setting_push_current_menu(driver.menu->menu_stack, g_settings.libretro_directory, MENU_SETTINGS_DEFERRED_CORE, driver.menu->selection_ptr,
            MENU_ACTION_OK);
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

static void menu_common_setting_set_current_path_selection(rarch_setting_t *setting, const char *start_path, unsigned type, unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
         menu_common_setting_push_current_menu(driver.menu->menu_stack, start_path, type, driver.menu->selection_ptr, action);
         break;
      case MENU_ACTION_START:
         *setting->value.string = '\0';
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_fraction(rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "video_refresh_rate_auto"))
   {
      if (action == MENU_ACTION_START)
         g_extern.measure_data.frame_time_samples_count = 0;
      else if (action == MENU_ACTION_OK)
      {
         double refresh_rate, deviation = 0.0;
         unsigned sample_points = 0;

         if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
         {
            driver_set_monitor_refresh_rate(refresh_rate);
            // Incase refresh rate update forced non-block video.
            rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
         }
      }
   }
   else if (!strcmp(setting->name, "fastforward_ratio"))
   {
      bool clamp_value = false;
      if (action == MENU_ACTION_START)
        *setting->value.fraction  = setting->default_value.fraction;
      else if (action == MENU_ACTION_LEFT)
      {
         *setting->value.fraction -= setting->step;
         if (*setting->value.fraction < 0.95f) // Avoid potential rounding errors when going from 1.1 to 1.0.
            *setting->value.fraction = setting->default_value.fraction;
         else
            clamp_value = true;
      }
      else if (action == MENU_ACTION_RIGHT)
      {
         *setting->value.fraction += setting->step;
         clamp_value = true;
      }
      if (clamp_value)
         g_settings.fastforward_ratio = max(min(*setting->value.fraction, setting->max), 1.0f);
   }
   else
   {
      switch (action)
      {
         case MENU_ACTION_LEFT:
            *setting->value.fraction = *setting->value.fraction - setting->step;

            if (setting->enforce_minrange)
            {
               if (*setting->value.fraction < setting->min)
                  *setting->value.fraction = setting->min;
            }
            break;

         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
            *setting->value.fraction = *setting->value.fraction + setting->step;

            if (setting->enforce_maxrange)
            {
               if (*setting->value.fraction > setting->max)
                  *setting->value.fraction = setting->max;
            }
            break;

         case MENU_ACTION_START:
            *setting->value.fraction = setting->default_value.fraction;
            break;
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_unsigned_integer(rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "netplay_tcp_udp_port"))
   {
      if (action == MENU_ACTION_OK)
         menu_key_start_line(driver.menu, "TCP/UDP Port: ", setting->name, st_uint_callback);
      else if (action == MENU_ACTION_START)
         *setting->value.unsigned_integer = setting->default_value.unsigned_integer;
   }
   else
   {
      switch (action)
      {
         case MENU_ACTION_LEFT:
            if (*setting->value.unsigned_integer != setting->min)
               *setting->value.unsigned_integer = *setting->value.unsigned_integer - setting->step;

            if (setting->enforce_minrange)
            {
               if (*setting->value.unsigned_integer < setting->min)
                  *setting->value.unsigned_integer = setting->min;
            }
            break;

         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
            *setting->value.unsigned_integer = *setting->value.unsigned_integer + setting->step;

            if (setting->enforce_maxrange)
            {
               if (*setting->value.unsigned_integer > setting->max)
                  *setting->value.unsigned_integer = setting->max;
            }
            break;

         case MENU_ACTION_START:
            *setting->value.unsigned_integer = setting->default_value.unsigned_integer;
            break;
      }
   } 

   if (setting->change_handler)
      setting->change_handler(setting);
}

void menu_common_setting_set_current_string(rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_string_path(rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_current_string_dir(rarch_setting_t *setting, const char *dir)
{
   strlcpy(setting->value.string, dir, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);
}

static int menu_action_ok(const char *dir, unsigned menu_type)
{
   const char *path = NULL;
   unsigned type = 0;
   rarch_setting_t *setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)setting_data_get_list();

   if (file_list_get_size(driver.menu->selection_buf) == 0)
      return 0;

   file_list_get_at_offset(driver.menu->selection_buf, driver.menu->selection_ptr, &path, &type);

   if (
         menu_common_type_is(type) == MENU_SETTINGS_SHADER_OPTIONS ||
         menu_common_type_is(type) == MENU_FILE_DIRECTORY ||
         type == MENU_SETTINGS_OVERLAY_PRESET ||
         type == MENU_SETTINGS_VIDEO_SOFTFILTER ||
         type == MENU_SETTINGS_AUDIO_DSP_FILTER ||
         type == MENU_CONTENT_HISTORY_PATH ||
         type == MENU_SETTINGS_CORE ||
         type == MENU_SETTINGS_CONFIG ||
         type == MENU_SETTINGS_DISK_APPEND ||
         type == MENU_FILE_DIRECTORY)
   {
      char cat_path[PATH_MAX];
      fill_pathname_join(cat_path, dir, path, sizeof(cat_path));

      menu_common_setting_push_current_menu(driver.menu->menu_stack, cat_path, type, driver.menu->selection_ptr,
            MENU_ACTION_OK);
   }
   else
   {
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
            struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
            unsigned pass = (menu_type - MENU_SETTINGS_SHADER_0) / 3;

            fill_pathname_join(shader->pass[pass].source.path,
                  dir, path, sizeof(shader->pass[pass].source.path));

            // This will reset any changed parameters.
            gfx_shader_resolve_parameters(NULL, driver.menu->shader);
         }

         // Pop stack until we hit shader manager again.
         menu_flush_stack_type(MENU_SETTINGS_SHADER_OPTIONS);
      }
      else if (menu_type == MENU_SETTINGS_DEFERRED_CORE)
      {
         strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
         strlcpy(g_extern.fullpath, driver.menu->deferred_path, sizeof(g_extern.fullpath));
         rarch_main_command(RARCH_CMD_LOAD_CONTENT);
         driver.menu->msg_force = true;
         menu_flush_stack_type(MENU_SETTINGS);
         return -1;
      }
      else if (menu_type == MENU_SETTINGS_CORE)
      {
         fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
         rarch_main_command(RARCH_CMD_LOAD_CORE);
#if defined(HAVE_DYNAMIC)
         // No content needed for this core, load core immediately.
         if (driver.menu->load_no_content)
         {
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            *g_extern.fullpath = '\0';
            driver.menu->msg_force = true;
            return -1;
         }

         // Core selection on non-console just updates directory listing.
         // Will take effect on new content load.
#elif defined(RARCH_CONSOLE)
#if defined(GEKKO) && defined(HW_RVL)
         fill_pathname_join(g_extern.fullpath, g_defaults.core_dir,
               SALAMANDER_FILE, sizeof(g_extern.fullpath));
#endif
         g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
         g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
         return -1;
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
            return -1;
         }
      }
      else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "input_overlay")))
            menu_common_setting_set_current_string_path(setting, dir, path);

         menu_flush_stack_type(MENU_SETTINGS_OPTIONS);
      }
      else if (menu_type == MENU_SETTINGS_DISK_APPEND)
      {
         char image[PATH_MAX];
         fill_pathname_join(image, dir, path, sizeof(image));
         rarch_disk_control_append_image(image);

         g_extern.lifecycle_state |= 1ULL << MODE_GAME;

         menu_flush_stack_type(MENU_SETTINGS);
         return -1;
      }
      else if (menu_type == MENU_SETTINGS_OPEN_HISTORY)
      {
         load_menu_content_history(driver.menu->selection_ptr);
         menu_flush_stack_type(MENU_SETTINGS);
         return -1;
      }
      else if (menu_type == MENU_CONTENT_HISTORY_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "game_history_path")))
            menu_common_setting_set_current_string_path(setting, dir, path);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_BROWSER_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "rgui_browser_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_CONTENT_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "content_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_ASSETS_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "assets_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_SCREENSHOT_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "screenshot_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_SAVEFILE_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "savefile_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);
         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_OVERLAY_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "overlay_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);
         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_filter")))
            menu_common_setting_set_current_string_path(setting, dir, path);

         menu_flush_stack_type(MENU_SETTINGS_VIDEO_OPTIONS);
      }
      else if (menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_dsp_plugin")))
            menu_common_setting_set_current_string_path(setting, dir, path);
         menu_flush_stack_type(MENU_SETTINGS_AUDIO_OPTIONS);
      }
      else if (menu_type == MENU_SAVESTATE_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "savestate_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_LIBRETRO_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "libretro_dir_path")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_CONFIG_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "rgui_config_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_LIBRETRO_INFO_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "libretro_info_path")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_SHADER_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_shader_dir")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_FILTER_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "video_filter_dir")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_DSP_FILTER_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "audio_filter_dir")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_SYSTEM_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "system_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_AUTOCONFIG_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "joypad_autoconfig_dir")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else if (menu_type == MENU_EXTRACTION_DIR_PATH)
      {
         if ((setting = (rarch_setting_t*)setting_data_find_setting(setting_data, "extraction_directory")))
            menu_common_setting_set_current_string_dir(setting, dir);

         menu_flush_stack_type(MENU_SETTINGS_PATH_OPTIONS);
      }
      else
      {
         if (driver.menu->defer_core)
         {
            int ret = menu_defer_core(driver.menu->core_info, dir, path, driver.menu->deferred_path, sizeof(driver.menu->deferred_path));

            if (ret == -1)
            {
               rarch_main_command(RARCH_CMD_LOAD_CORE);
               if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->defer_decision_automatic) 
                  driver.menu_ctx->backend->defer_decision_automatic();
               return -1;
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
            return -1;
         }
      }
   }

   return 0;
}

static int menu_common_iterate(unsigned action)
{
   int ret = 0;
   unsigned menu_type = 0;
   const char *dir = NULL;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot iterate menu, menu handle is not initialized.\n");
      return 0;
   }

   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

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
            menu_action_cancel();
         break;

      case MENU_ACTION_OK:
         ret = menu_action_ok(dir, menu_type);
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

   // refresh values in case the stack changed
   file_list_get_last(driver.menu->menu_stack, &dir, &menu_type);

   if (driver.menu->need_refresh && (menu_type == MENU_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY ||
            menu_type == MENU_SETTINGS_OVERLAY_PRESET ||
            menu_type == MENU_CONTENT_HISTORY_PATH ||
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

static void menu_common_shader_manager_init(menu_handle_t *menu)
{
   char cgp_path[PATH_MAX];
   config_file_t *conf = NULL;
   const char *config_path = NULL;
   struct gfx_shader *shader = (struct gfx_shader*)menu->shader;

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


   const char *ext = path_get_extension(g_settings.video.shader_path);
   if (strcmp(ext, "glslp") == 0 || strcmp(ext, "cgp") == 0)
   {
      conf = config_file_new(g_settings.video.shader_path);
      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, shader))
         {
            gfx_shader_resolve_relative(shader, g_settings.video.shader_path);
            gfx_shader_resolve_parameters(conf, shader);
         }
         config_file_free(conf);
      }
   }
   else if (strcmp(ext, "glsl") == 0 || strcmp(ext, "cg") == 0)
   {
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
         if (gfx_shader_read_conf_cgp(conf, shader))
         {
            gfx_shader_resolve_relative(shader, cgp_path);
            gfx_shader_resolve_parameters(conf, shader);
         }
         config_file_free(conf);
      }
   }
}

static void menu_common_shader_manager_set_preset(struct gfx_shader *shader, unsigned type, const char *cgp_path)
{
   RARCH_LOG("Setting Menu shader: %s.\n", cgp_path ? cgp_path : "N/A (stock)");

   if (driver.video->set_shader && driver.video->set_shader(driver.video_data, (enum rarch_shader_type)type, cgp_path))
   {
      // Makes sure that we use Menu CGP shader on driver reinit.
      // Only do this when the cgp actually works to avoid potential errors.
      strlcpy(g_settings.video.shader_path, cgp_path ? cgp_path : "",
            sizeof(g_settings.video.shader_path));
      g_settings.video.shader_enable = true;

      if (cgp_path && shader)
      {
         // Load stored CGP into menu on success.
         // Used when a preset is directly loaded.
         // No point in updating when the CGP was created from the menu itself.
         config_file_t *conf = config_file_new(cgp_path);

         if (conf)
         {
            if (gfx_shader_read_conf_cgp(conf, shader))
            {
               gfx_shader_resolve_relative(shader, cgp_path);
               gfx_shader_resolve_parameters(conf, shader);
            }
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
}

static void menu_common_shader_manager_get_str(struct gfx_shader *shader, char *type_str, size_t type_str_size, unsigned type)
{
   *type_str = '\0';

   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0 && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      // menu->parameter_shader here.
      if (shader)
      {
         const struct gfx_shader_parameter *param = (const struct gfx_shader_parameter*)&shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
         snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]", param->current, param->minimum, param->maximum);
      }
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
            {
               static const char *modes[] = {
                  "Don't care",
                  "Linear",
                  "Nearest"
               };

               strlcpy(type_str, modes[shader->pass[pass].filter], type_str_size);
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
}

static void menu_common_shader_manager_save_preset(const char *basename, bool apply)
{
   char buffer[PATH_MAX], config_directory[PATH_MAX], cgp_path[PATH_MAX];
   unsigned d, type = RARCH_SHADER_NONE;
   config_file_t *conf = NULL;
   const char *conf_path = NULL;
   bool ret = false;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot save shader preset, menu handle is not initialized.\n");
      return;
   }

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_type)
      type = driver.menu_ctx->backend->shader_manager_get_type(driver.menu->shader);

   if (type == RARCH_SHADER_NONE)
      return;

   conf_path = (type == RARCH_SHADER_GLSL) ? driver.menu->default_glslp : driver.menu->default_cgp;
   *config_directory = '\0';

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
   }

   if (*g_extern.config_path)
      fill_pathname_basedir(config_directory, g_extern.config_path, sizeof(config_directory));

   const char *dirs[] = {
      g_settings.video.shader_dir,
      g_settings.menu_config_directory,
      config_directory,
   };

   if (!(conf = (config_file_t*)config_file_new(NULL)))
      return;
   gfx_shader_write_conf_cgp(conf, driver.menu->shader);

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
}

static unsigned menu_common_shader_manager_get_type(const struct gfx_shader *shader)
{
   // All shader types must be the same, or we cannot use it.
   unsigned i;
   unsigned type = RARCH_SHADER_NONE;

   if (!shader)
   {
      RARCH_ERR("Cannot get shader type, shader handle is not initialized.\n");
      return RARCH_SHADER_NONE;
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
}


static int menu_common_shader_manager_setting_toggle(unsigned id, unsigned action)
{
   if (!driver.menu)
   {
      RARCH_ERR("Cannot toggle shader setting, menu handle is not initialized.\n");
      return 0;
   }

   rarch_setting_t *current_setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)setting_data_get_list();

   unsigned dist_shader = id - MENU_SETTINGS_SHADER_0;
   unsigned dist_filter = id - MENU_SETTINGS_SHADER_0_FILTER;
   unsigned dist_scale  = id - MENU_SETTINGS_SHADER_0_SCALE;

   if (id == MENU_SETTINGS_SHADER_FILTER)
   {
      if ((current_setting = setting_data_find_setting(setting_data, "video_smooth")))
         menu_common_setting_set_current_boolean(current_setting, action);
   }
   else if ((id == MENU_SETTINGS_SHADER_PARAMETERS || id == MENU_SETTINGS_SHADER_PRESET_PARAMETERS))
      menu_common_setting_push_current_menu(driver.menu->menu_stack, "", id, driver.menu->selection_ptr, action);
   else if (id >= MENU_SETTINGS_SHADER_PARAMETER_0 && id <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      struct gfx_shader *shader = NULL;
      struct gfx_shader_parameter *param = NULL;

      if (!(shader = (struct gfx_shader*)driver.menu->parameter_shader))
         return 0;

      if (!(param = &shader->parameters[id - MENU_SETTINGS_SHADER_PARAMETER_0]))
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
   else if ((id == MENU_SETTINGS_SHADER_APPLY || id == MENU_SETTINGS_SHADER_PASSES) &&
         (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_set))
      driver.menu_ctx->backend->setting_set(id, action);
   else if (((dist_shader % 3) == 0 || id == MENU_SETTINGS_SHADER_PRESET))
   {
      struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
      struct gfx_shader_pass *pass = NULL;

      dist_shader /= 3;
      if (shader && id == MENU_SETTINGS_SHADER_PRESET)
         pass = &shader->pass[dist_shader];

      switch (action)
      {
         case MENU_ACTION_OK:
            menu_common_setting_push_current_menu(driver.menu->menu_stack, g_settings.video.shader_dir, id, driver.menu->selection_ptr, action);
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
      struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
      struct gfx_shader_pass *pass = (struct gfx_shader_pass*)&shader->pass[dist_filter];

      switch (action)
      {
         case MENU_ACTION_START:
            if (shader && shader->pass)
               shader->pass[dist_filter].filter = RARCH_FILTER_UNSPEC;
            break;

         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
         {
            unsigned delta = (action == MENU_ACTION_LEFT) ? 2 : 1;
            if (pass)
               pass->filter = ((pass->filter + delta) % 3);
            break;
         }

         default:
         break;
      }
   }
   else if ((dist_scale % 3) == 0)
   {
      dist_scale /= 3;
      struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
      struct gfx_shader_pass *pass = (struct gfx_shader_pass*)&shader->pass[dist_scale];

      switch (action)
      {
         case MENU_ACTION_START:
            if (shader && shader->pass)
            {
               pass->fbo.scale_x = pass->fbo.scale_y = 0;
               pass->fbo.valid = false;
            }
            break;

         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
         {
            unsigned current_scale = pass->fbo.scale_x;
            unsigned delta = action == MENU_ACTION_LEFT ? 5 : 1;
            current_scale = (current_scale + delta) % 6;

            if (pass)
            {
               pass->fbo.valid = current_scale;
               pass->fbo.scale_x = pass->fbo.scale_y = current_scale;
            }
            break;
         }

         default:
         break;
      }
   }

   return 0;
}

static int menu_common_setting_toggle(unsigned id, unsigned action,
      unsigned menu_type)
{
   (void)menu_type;

   if ((id >= MENU_SETTINGS_SHADER_FILTER) && (id <= MENU_SETTINGS_SHADER_LAST))
   {
      if (driver.menu_ctx && driver.menu_ctx->backend
            && driver.menu_ctx->backend->shader_manager_setting_toggle)
         return driver.menu_ctx->backend->shader_manager_setting_toggle(id, action);
      return 0;
   }
   if ((id >= MENU_SETTINGS_CORE_OPTION_START) &&
         (driver.menu_ctx && driver.menu_ctx->backend
          && driver.menu_ctx->backend->core_setting_toggle)
      )
      return driver.menu_ctx->backend->core_setting_toggle(id, action);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->setting_set)
      return driver.menu_ctx->backend->setting_set(id, action);

   return 0;
}

static int menu_common_core_setting_toggle(unsigned setting, unsigned action)
{
   unsigned index = setting - MENU_SETTINGS_CORE_OPTION_START;

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
#endif

static void handle_setting(rarch_setting_t *setting,
      unsigned id, unsigned action)
{
   if (setting->type == ST_BOOL)
      menu_common_setting_set_current_boolean(setting, action);
   else if (setting->type == ST_UINT)
      menu_common_setting_set_current_unsigned_integer(setting, action);
   else if (setting->type == ST_FLOAT)
      menu_common_setting_set_current_fraction(setting, action);
   else if (setting->type == ST_DIR)
   {
      if (action == MENU_ACTION_START)
      {
         *setting->value.string = '\0';

         if (setting->change_handler)
            setting->change_handler(setting);
      }
   }
   else if (setting->type == ST_PATH)
      menu_common_setting_set_current_path_selection(setting, setting->default_value.string, id, action);
   else if (setting->type == ST_STRING)
   {
      switch (id)
      {
         case MENU_SETTINGS_DRIVER_VIDEO:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_VIDEO, g_settings.video.driver, sizeof(g_settings.video.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_VIDEO, g_settings.video.driver, sizeof(g_settings.video.driver));
            break;
         case MENU_SETTINGS_DRIVER_AUDIO:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_AUDIO, g_settings.audio.driver, sizeof(g_settings.audio.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_AUDIO, g_settings.audio.driver, sizeof(g_settings.audio.driver));
            break;
         case MENU_SETTINGS_DRIVER_AUDIO_RESAMPLER:
            if (action == MENU_ACTION_LEFT)
               find_prev_resampler_driver();
            else if (action == MENU_ACTION_RIGHT)
               find_next_resampler_driver();
            break;
         case MENU_SETTINGS_DRIVER_INPUT:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_INPUT, g_settings.input.driver, sizeof(g_settings.input.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_INPUT, g_settings.input.driver, sizeof(g_settings.input.driver));
            break;
         case MENU_SETTINGS_DRIVER_CAMERA:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_CAMERA, g_settings.camera.driver, sizeof(g_settings.camera.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_CAMERA, g_settings.camera.driver, sizeof(g_settings.camera.driver));
            break;
         case MENU_SETTINGS_DRIVER_LOCATION:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_LOCATION, g_settings.location.driver, sizeof(g_settings.location.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_LOCATION, g_settings.location.driver, sizeof(g_settings.location.driver));
            break;
         case MENU_SETTINGS_DRIVER_MENU:
            if (action == MENU_ACTION_LEFT)
               find_prev_driver(RARCH_DRIVER_MENU, g_settings.menu.driver, sizeof(g_settings.menu.driver));
            else if (action == MENU_ACTION_RIGHT)
               find_next_driver(RARCH_DRIVER_MENU, g_settings.menu.driver, sizeof(g_settings.menu.driver));
            break;
      }
   }
}

static int menu_common_setting_set(unsigned id, unsigned action)
{
   struct retro_perf_counter **counters;
   unsigned port = driver.menu->current_pad;
   rarch_setting_t *setting = (rarch_setting_t*)get_last_setting(
         driver.menu->selection_buf, driver.menu->selection_ptr,
         setting_data_get_list()
         );

   if (id >= MENU_SETTINGS_PERF_COUNTERS_BEGIN && id <= MENU_SETTINGS_PERF_COUNTERS_END)
   {
      counters = (struct retro_perf_counter**)perf_counters_rarch;
      return menu_common_setting_set_perf(id, action, counters, id - MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   }
   else if (id >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN && id <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
   {
      counters = (struct retro_perf_counter**)perf_counters_libretro;
      return menu_common_setting_set_perf(id, action, counters, id - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   }
   else if (id >= MENU_SETTINGS_BIND_BEGIN && id <= MENU_SETTINGS_BIND_ALL_LAST)
   {
      struct retro_keybind *bind = (struct retro_keybind*)&g_settings.input.binds[port][id - MENU_SETTINGS_BIND_BEGIN];

      if (action == MENU_ACTION_OK)
      {
         driver.menu->binds.begin  = id;
         driver.menu->binds.last   = id;
         driver.menu->binds.target = bind;
         driver.menu->binds.player = port;
         file_list_push(driver.menu->menu_stack, "", "",
               driver.menu->bind_mode_keyboard ?
               MENU_SETTINGS_CUSTOM_BIND_KEYBOARD : MENU_SETTINGS_CUSTOM_BIND,
               driver.menu->selection_ptr);

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
            bind->key = def_binds[id - MENU_SETTINGS_BIND_BEGIN].key;
         }
         else
         {
            bind->joykey = NO_BTN;
            bind->joyaxis = AXIS_NONE;
         }
      }
      return 0;
   }
   else if (setting)
      handle_setting(setting, id, action);
   else
   {
      setting = (rarch_setting_t*)get_last_setting(
            driver.menu->selection_buf, driver.menu->selection_ptr,
            setting_data_get_mainmenu(true)
            );

      if (setting)
         handle_setting(setting, id, action);
      else
      {
         switch (id)
         {
            case MENU_SETTINGS_SAVESTATE_SAVE:
            case MENU_SETTINGS_SAVESTATE_LOAD:
               if (action == MENU_ACTION_OK)
               {
                  if (id == MENU_SETTINGS_SAVESTATE_SAVE)
                     rarch_main_command(RARCH_CMD_SAVE_STATE);
                  else
                     rarch_main_command(RARCH_CMD_LOAD_STATE);
                  return -1;
               }
               else if (action == MENU_ACTION_START)
                  g_settings.state_slot = 0;
               else if (action == MENU_ACTION_LEFT)
               {
                  // Slot -1 is (auto) slot.
                  if (g_settings.state_slot >= 0)
                     g_settings.state_slot--;
               }
               else if (action == MENU_ACTION_RIGHT)
                  g_settings.state_slot++;
               break;
            case MENU_SETTINGS_DISK_INDEX:
               {
                  int step = 0;

                  if (action == MENU_ACTION_RIGHT || action == MENU_ACTION_OK)
                     step = 1;
                  else if (action == MENU_ACTION_LEFT)
                     step = -1;

                  if (step)
                  {
                     const struct retro_disk_control_callback *control = (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
                     unsigned num_disks = control->get_num_images();
                     unsigned current   = control->get_image_index();
                     unsigned next_index = (current + num_disks + 1 + step) % (num_disks + 1);
                     rarch_disk_control_set_eject(true, false);
                     rarch_disk_control_set_index(next_index);
                     rarch_disk_control_set_eject(false, false);
                  }

                  break;
               }
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
               {
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
            case MENU_SETTINGS_BIND_DEVICE_TYPE:
               {
                  unsigned current_device, current_index, i, devices[128];
                  const struct retro_controller_info *desc;
                  unsigned types = 0;

                  devices[types++] = RETRO_DEVICE_NONE;
                  devices[types++] = RETRO_DEVICE_JOYPAD;
                  // Only push RETRO_DEVICE_ANALOG as default if we use an older core which doesn't use SET_CONTROLLER_INFO.
                  if (!g_extern.system.num_ports)
                     devices[types++] = RETRO_DEVICE_ANALOG;

                  desc = port < g_extern.system.num_ports ? &g_extern.system.ports[port] : NULL;
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
            case MENU_SETTINGS_CUSTOM_BIND_MODE:
               if (action == MENU_ACTION_OK || action == MENU_ACTION_LEFT || action == MENU_ACTION_RIGHT)
                  driver.menu->bind_mode_keyboard = !driver.menu->bind_mode_keyboard;
               break;
            case MENU_SETTINGS_CUSTOM_BIND_ALL:
               if (action == MENU_ACTION_OK)
               {
                  driver.menu->binds.target = &g_settings.input.binds[port][0];
                  driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
                  driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;
                  if (driver.menu->bind_mode_keyboard)
                  {
                     file_list_push(driver.menu->menu_stack, "", "",
                           MENU_SETTINGS_CUSTOM_BIND_KEYBOARD,
                           driver.menu->selection_ptr);
                     driver.menu->binds.timeout_end = rarch_get_time_usec() + MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
                     input_keyboard_wait_keys(driver.menu, menu_custom_bind_keyboard_cb);
                  }
                  else
                  {
                     file_list_push(driver.menu->menu_stack, "", "",
                           MENU_SETTINGS_CUSTOM_BIND,
                           driver.menu->selection_ptr);
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

                  rarch_main_command(RARCH_CMD_REINIT);
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

                        rarch_main_command(RARCH_CMD_REINIT);
                     }
                     break;
                  case MENU_ACTION_START:
                     if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
                     {
                        g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                        rarch_main_command(RARCH_CMD_REINIT);
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

               rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
               break;
#endif

               break;
            case MENU_SETTINGS_SHADER_PASSES:
               {
                  struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;

                  switch (action)
                  {
                     case MENU_ACTION_START:
                        if (shader && shader->passes)
                           shader->passes = 0;
                        driver.menu->need_refresh = true;
                        break;

                     case MENU_ACTION_LEFT:
                        if (shader && shader->passes)
                           shader->passes--;
                        driver.menu->need_refresh = true;
                        break;

                     case MENU_ACTION_RIGHT:
                     case MENU_ACTION_OK:
                        if (shader && (shader->passes < GFX_MAX_SHADERS))
                           shader->passes++;
                        driver.menu->need_refresh = true;
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
                  struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
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
#ifdef _XBOX1
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
#ifdef HAVE_NETPLAY
            case MENU_SETTINGS_NETPLAY_HOST_IP_ADDRESS:
               if (action == MENU_ACTION_OK)
                  menu_key_start_line(driver.menu, "IP Address: ", "netplay_ip_address", st_string_callback);
               else if (action == MENU_ACTION_START)
                  *g_extern.netplay_server = '\0';
               break;
#endif
            case MENU_SETTINGS_DRIVER_AUDIO_DEVICE:
               if (action == MENU_ACTION_OK)
                  menu_key_start_line(driver.menu, "Audio Device Name / IP: ", "audio_device", st_string_callback);
               else if (action == MENU_ACTION_START)
                  *g_settings.audio.device = '\0';
               break;
            case MENU_SETTINGS_NETPLAY_NICKNAME:
               if (action == MENU_ACTION_OK)
                  menu_key_start_line(driver.menu, "Username: ", "netplay_nickname", st_string_callback);
               else if (action == MENU_ACTION_START)
                  *g_settings.username = '\0';
               break;
            case MENU_SETTINGS_SHADER_PRESET_SAVE:
               if (action == MENU_ACTION_OK)
                  menu_key_start_line(driver.menu, "Preset Filename: ", "shader_preset_save", preset_filename_callback);
               break;
            default:
               break;
         }
      }
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

static void menu_common_setting_set_label_st_bool(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   strlcpy(type_str, *setting->value.boolean ? setting->boolean.on_label : setting->boolean.off_label, type_str_size);
}

static void menu_common_setting_set_label_st_float(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   if (setting && !strcmp(setting->name, "video_refresh_rate_auto"))
   {
      double refresh_rate = 0.0;
      double deviation = 0.0;
      unsigned sample_points = 0;

      if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
         snprintf(type_str, type_str_size, "%.3f Hz (%.1f%% dev, %u samples)", refresh_rate, 100.0 * deviation, sample_points);
      else
         strlcpy(type_str, "N/A", type_str_size);
   }
   else
      snprintf(type_str, type_str_size, setting->rounding_fraction, *setting->value.fraction);
}

static void menu_common_setting_set_label_st_uint(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   if (setting && !strcmp(setting->name, "video_monitor_index"))
   {
      if (*setting->value.unsigned_integer)
         snprintf(type_str, type_str_size, "%d", *setting->value.unsigned_integer);
      else
         strlcpy(type_str, "0 (Auto)", type_str_size);
   }
   else if (setting && !strcmp(setting->name, "video_rotation"))
      strlcpy(type_str, rotation_lut[*setting->value.unsigned_integer],
            type_str_size);
   else if (setting && !strcmp(setting->name, "aspect_ratio_index"))
      strlcpy(type_str, aspectratio_lut[*setting->value.unsigned_integer].name, type_str_size);
   else if (setting && !strcmp(setting->name, "autosave_interval"))
   {
      if (*setting->value.unsigned_integer)
         snprintf(type_str, type_str_size, "%u seconds", *setting->value.unsigned_integer);
      else
         strlcpy(type_str, "OFF", type_str_size);
   }
   else if (setting && !strcmp(setting->name, "user_language"))
   {
      static const char *modes[] = {
         "English",
         "Japanese",
         "French",
         "Spanish",
         "German",
         "Italian",
         "Dutch",
         "Portuguese",
         "Russian",
         "Korean",
         "Chinese (Traditional)",
         "Chinese (Simplified)"
      };

      strlcpy(type_str, modes[g_settings.user_language], type_str_size);
   }
   else if (setting && !strcmp(setting->name, "libretro_log_level"))
   {
      static const char *modes[] = {
         "0 (Debug)",
         "1 (Info)",
         "2 (Warning)",
         "3 (Error)"
      };

      strlcpy(type_str, modes[*setting->value.unsigned_integer], type_str_size);
   }
   else
      snprintf(type_str, type_str_size, "%d", *setting->value.unsigned_integer);
}

static void handle_setting_label(char *type_str,
      size_t type_str_size, rarch_setting_t *setting)
{
   if (setting->type == ST_BOOL)
      menu_common_setting_set_label_st_bool(setting, type_str, type_str_size);
   else if (setting->type == ST_UINT)
      menu_common_setting_set_label_st_uint(setting, type_str, type_str_size);
   else if (setting->type == ST_FLOAT)
      menu_common_setting_set_label_st_float(setting, type_str, type_str_size);
   else if (setting->type == ST_DIR)
      strlcpy(type_str, *setting->value.string ? setting->value.string : setting->dir.empty_path, type_str_size);
   else if (setting->type == ST_PATH)
      strlcpy(type_str, path_basename(setting->value.string), type_str_size);
   else if (setting->type == ST_STRING)
      strlcpy(type_str, setting->value.string, type_str_size);
   else if (setting->type == ST_GROUP)
      strlcpy(type_str, "...", type_str_size);
}

static void menu_common_setting_set_label(char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, unsigned index)
{
   rarch_setting_t *setting_data = (rarch_setting_t*)setting_data_get_list();
   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
         driver.menu->selection_buf->list[index].label);

   if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type, perf_counters_rarch,
            type - MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type, perf_counters_libretro,
            type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   else if (type >= MENU_SETTINGS_BIND_BEGIN && type <= MENU_SETTINGS_BIND_ALL_LAST)
   {
      const struct retro_keybind *auto_bind = 
         (const struct retro_keybind*)input_get_auto_bind(driver.menu->current_pad,
               type - MENU_SETTINGS_BIND_BEGIN);

      input_get_bind_string(type_str, &g_settings.input.binds[driver.menu->current_pad][type - MENU_SETTINGS_BIND_BEGIN], auto_bind, type_str_size);
   }
   else if (setting)
      handle_setting_label(type_str, type_str_size, setting);
   else
   {
      setting_data = (rarch_setting_t*)setting_data_get_mainmenu(true);

      setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
            driver.menu->selection_buf->list[index].label);

      if (setting)
      {
         if (type == MENU_SETTINGS_CONFIG)
         {
            if (*g_extern.config_path)
               fill_pathname_base(type_str, g_extern.config_path,
                     type_str_size);
            else
               strlcpy(type_str, "<default>", type_str_size);
         }
         else if (
               type == MENU_SETTINGS_SAVESTATE_SAVE ||
               type == MENU_SETTINGS_SAVESTATE_LOAD)
         {
            if (g_settings.state_slot < 0)
               strlcpy(type_str, "-1 (auto)", type_str_size);
            else
               snprintf(type_str, type_str_size, "%d", g_settings.state_slot);
         }
         else
            handle_setting_label(type_str, type_str_size, setting);
      }
      else
      {
         switch (type)
         {
            case MENU_SETTINGS_VIDEO_SOFT_FILTER:
               snprintf(type_str, type_str_size,
                     (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
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
            case MENU_SETTINGS_CUSTOM_VIEWPORT:
            case MENU_SETTINGS_DISK_OPTIONS:
            case MENU_SETTINGS_SHADER_PRESET:
            case MENU_SETTINGS_SHADER_PRESET_SAVE:
            case MENU_SETTINGS_DISK_APPEND:
            case MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND:
            case MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO:
            case MENU_SETTINGS_CUSTOM_BIND_ALL:
            case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
               strlcpy(type_str, "...", type_str_size);
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
            case MENU_SETTINGS_BIND_DEVICE_TYPE:
               {
                  const struct retro_controller_description *desc = NULL;
                  if (driver.menu->current_pad < g_extern.system.num_ports)
                     desc = libretro_find_controller_description(&g_extern.system.ports[driver.menu->current_pad],
                           g_settings.input.libretro_device[driver.menu->current_pad]);

                  const char *name = desc ? desc->desc : NULL;
                  if (!name)
                  {
                     /* Find generic name. */

                     switch (g_settings.input.libretro_device[driver.menu->current_pad])
                     {
                        case RETRO_DEVICE_NONE:
                           name = "None";
                           break;
                        case RETRO_DEVICE_JOYPAD:
                           name = "RetroPad";
                           break;
                        case RETRO_DEVICE_ANALOG:
                           name = "RetroPad w/ Analog";
                           break;
                        default:
                           name = "Unknown";
                           break;
                     }
                  }

                  strlcpy(type_str, name, type_str_size);
               }
               break;
            case MENU_SETTINGS_CUSTOM_BIND_MODE:
               strlcpy(type_str, driver.menu->bind_mode_keyboard ? "RetroKeyboard" : "RetroPad", type_str_size);
               break;
#ifdef _XBOX1
            case MENU_SETTINGS_SOFT_DISPLAY_FILTER:
               snprintf(type_str, type_str_size,
                     (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
               break;
#endif
            case MENU_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE:
               strlcpy(type_str, (g_extern.lifecycle_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF", type_str_size);
               break;
            default:
               *type_str = '\0';
               *w = 0;
               break;
         }
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
