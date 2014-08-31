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

#include "menu_entries.h"
#include "backend/menu_common_backend.h"
#include "../../settings_data.h"

static inline struct gfx_shader *shader_manager_get_current_shader(menu_handle_t *menu, unsigned type)
{
   if (type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
      return menu->shader;
   else if (driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);

   return NULL;
}

static void add_setting_entry(menu_handle_t *menu, const char *label, unsigned id,
      rarch_setting_t *settings)
{
   rarch_setting_t *setting = (rarch_setting_t*)
      setting_data_find_setting(settings, label);

   if (setting)
      file_list_push(menu->selection_buf, setting->short_description, setting->name,
            id, 0);
}

void menu_entries_push(menu_handle_t *menu, unsigned menu_type)
{
   unsigned i;
   char tmp[256];
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
         add_setting_entry(menu,"libretro_log_level", 0, setting_data);
         add_setting_entry(menu,"log_verbosity", 0, setting_data);
         add_setting_entry(menu,"perfcnt_enable", 0, setting_data);
         add_setting_entry(menu,"game_history_size", 0, setting_data);
         add_setting_entry(menu,"config_save_on_exit", 0, setting_data);
         add_setting_entry(menu,"core_specific_config", 0, setting_data);
         add_setting_entry(menu,"video_gpu_screenshot", 0, setting_data);
         add_setting_entry(menu,"dummy_on_core_shutdown", 0, setting_data);
         add_setting_entry(menu,"fps_show", 0, setting_data);
         add_setting_entry(menu,"fastforward_ratio", 0, setting_data);
         add_setting_entry(menu,"slowmotion_ratio", 0, setting_data);
         add_setting_entry(menu,"rewind_enable", 0, setting_data);
         add_setting_entry(menu,"rewind_granularity", 0, setting_data);
         add_setting_entry(menu,"block_sram_overwrite", 0, setting_data);
         add_setting_entry(menu,"autosave_interval", 0, setting_data);
         add_setting_entry(menu,"video_disable_composition", 0, setting_data);
         add_setting_entry(menu,"pause_nonactive", 0, setting_data);
         add_setting_entry(menu,"savestate_auto_save", 0, setting_data);
         add_setting_entry(menu,"savestate_auto_load", 0, setting_data);
         break;
      case MENU_SETTINGS_VIDEO_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"video_shared_context", 0, setting_data);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         file_list_push(menu->selection_buf, "Screen Resolution", "",
               MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         add_setting_entry(menu,"video_viwidth", 0, setting_data);
         add_setting_entry(menu,"video_filter", MENU_SETTINGS_VIDEO_SOFTFILTER, setting_data);
         add_setting_entry(menu, "pal60_enable", 0, setting_data);
         add_setting_entry(menu,"video_smooth", 0, setting_data);
         add_setting_entry(menu, "soft_filter", 0, setting_data);
         add_setting_entry(menu,"video_gamma", 0, setting_data);
         add_setting_entry(menu,"video_filter_flicker", 0,
               setting_data);
         add_setting_entry(menu,"video_scale_integer", 0, setting_data);
         add_setting_entry(menu,"aspect_ratio_index", 0, setting_data);
         file_list_push(menu->selection_buf, "Custom Ratio", "",
               MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
         add_setting_entry(menu,"video_fullscreen", 0, setting_data);
         add_setting_entry(menu,"video_windowed_fullscreen", 0, setting_data);
         add_setting_entry(menu,"video_rotation", 0, setting_data);
         add_setting_entry(menu,"video_vsync", 0, setting_data);
         add_setting_entry(menu,"video_hard_sync", 0, setting_data);
         add_setting_entry(menu,"video_hard_sync_frames", 0, setting_data);
         add_setting_entry(menu,"video_frame_delay", 0, setting_data);
         add_setting_entry(menu,"video_black_frame_insertion", 0, setting_data);
         add_setting_entry(menu,"video_swap_interval", 0, setting_data);
         add_setting_entry(menu,"video_threaded", 0, setting_data);
         add_setting_entry(menu,"video_scale", 0, setting_data);
         add_setting_entry(menu,"video_crop_overscan", 0, setting_data);
         add_setting_entry(menu,"video_monitor_index", 0, setting_data);
         add_setting_entry(menu,"video_refresh_rate", 0, setting_data);
         add_setting_entry(menu,"video_refresh_rate_auto", 0, setting_data);
         break;
      case MENU_SETTINGS_FONT_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"video_font_enable", 0, setting_data);
         add_setting_entry(menu,"video_font_size", 0, setting_data);
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
         add_setting_entry(menu,"Driver Options", MENU_SETTINGS_DRIVERS, setting_data);
         add_setting_entry(menu,"General Options", MENU_SETTINGS_GENERAL_OPTIONS, setting_data);
         add_setting_entry(menu,"Video Options", MENU_SETTINGS_VIDEO_OPTIONS, setting_data);
         add_setting_entry(menu,"Shader Options", MENU_SETTINGS_SHADER_OPTIONS, setting_data);
         add_setting_entry(menu,"Font Options", MENU_SETTINGS_FONT_OPTIONS, setting_data);
         add_setting_entry(menu,"Audio Options", MENU_SETTINGS_AUDIO_OPTIONS, setting_data);
         add_setting_entry(menu,"Input Options", MENU_SETTINGS_INPUT_OPTIONS, setting_data);
         add_setting_entry(menu,"Overlay Options", MENU_SETTINGS_OVERLAY_OPTIONS,
               setting_data);
         add_setting_entry(menu,"User Options", MENU_SETTINGS_USER_OPTIONS, setting_data);
         add_setting_entry(menu,"Netplay Options", MENU_SETTINGS_NETPLAY_OPTIONS,
               setting_data);
         add_setting_entry(menu,"Path Options", MENU_SETTINGS_PATH_OPTIONS, setting_data);
         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            if (g_extern.system.disk_control.get_num_images)
               file_list_push(menu->selection_buf, "Disk Options", "",
                     MENU_SETTINGS_DISK_OPTIONS, 0);
         }
         add_setting_entry(menu,"Privacy Options", MENU_SETTINGS_PRIVACY_OPTIONS,
               setting_data);
         break;
      case MENU_SETTINGS_PRIVACY_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"camera_allow", 0, setting_data);
         add_setting_entry(menu,"location_allow", 0, setting_data);
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
         add_setting_entry(menu,"input_overlay", MENU_SETTINGS_OVERLAY_PRESET,
               setting_data);
         add_setting_entry(menu,"input_overlay_opacity", 0, setting_data);
         add_setting_entry(menu,"input_overlay_scale", 0, setting_data);
         break;
      case MENU_SETTINGS_USER_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"netplay_nickname", 0, setting_data);
         add_setting_entry(menu,"user_language", 0, setting_data);
         break;
      case MENU_SETTINGS_NETPLAY_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"netplay_enable", 0, setting_data);
         add_setting_entry(menu,"netplay_mode", 0, setting_data);
         add_setting_entry(menu,"netplay_spectator_mode_enable", 0, setting_data);
         add_setting_entry(menu,"netplay_ip_address", 0, setting_data);
         add_setting_entry(menu,"netplay_tcp_udp_port", 0, setting_data);
         add_setting_entry(menu,"netplay_delay_frames", 0, setting_data);
         break;
      case MENU_SETTINGS_PATH_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"rgui_browser_directory",
               MENU_BROWSER_DIR_PATH, setting_data);
         add_setting_entry(menu,"content_directory",
               MENU_CONTENT_DIR_PATH, setting_data);
         add_setting_entry(menu,"assets_directory",
               MENU_ASSETS_DIR_PATH, setting_data);
         add_setting_entry(menu,"rgui_config_directory",
               MENU_CONFIG_DIR_PATH, setting_data);
         add_setting_entry(menu,"libretro_dir_path",
               MENU_LIBRETRO_DIR_PATH, setting_data);
         add_setting_entry(menu,"libretro_info_path",
               MENU_LIBRETRO_INFO_DIR_PATH, setting_data);
         add_setting_entry(menu,"game_history_path",
               MENU_CONTENT_HISTORY_PATH, setting_data);
         add_setting_entry(menu,"video_filter_dir",
               MENU_FILTER_DIR_PATH, setting_data);
         add_setting_entry(menu,"audio_filter_dir",
               MENU_DSP_FILTER_DIR_PATH, setting_data);
         add_setting_entry(menu,"video_shader_dir", MENU_SHADER_DIR_PATH, setting_data);
         add_setting_entry(menu,"savestate_directory", MENU_SAVESTATE_DIR_PATH,
               setting_data);
         add_setting_entry(menu,"savefile_directory", MENU_SAVEFILE_DIR_PATH, setting_data);
         add_setting_entry(menu,"overlay_directory", MENU_OVERLAY_DIR_PATH, setting_data);
         add_setting_entry(menu,"system_directory", MENU_SYSTEM_DIR_PATH, setting_data);
         add_setting_entry(menu,"screenshot_directory", MENU_SCREENSHOT_DIR_PATH,
               setting_data);
         add_setting_entry(menu,"joypad_autoconfig_dir", MENU_AUTOCONFIG_DIR_PATH,
               setting_data);
         add_setting_entry(menu,"extraction_directory", MENU_EXTRACTION_DIR_PATH,
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
         add_setting_entry(menu,"input_axis_threshold", 0, setting_data);
         add_setting_entry(menu,"input_autodetect_enable", 0, setting_data);
         file_list_push(menu->selection_buf, "Bind Mode", "",
               MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
         file_list_push(menu->selection_buf, "Configure All (RetroPad)", "",
               MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
         file_list_push(menu->selection_buf, "Default All (RetroPad)", "",
               MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
         add_setting_entry(menu,"osk_enable", MENU_SETTINGS_ONSCREEN_KEYBOARD_ENABLE,
               setting_data);
         for (i = MENU_SETTINGS_BIND_BEGIN; i <= MENU_SETTINGS_BIND_ALL_LAST; i++)
            file_list_push(menu->selection_buf,
                  input_config_bind_map[i - MENU_SETTINGS_BIND_BEGIN].desc,
                  "", i, 0);
         break;
      case MENU_SETTINGS_AUDIO_OPTIONS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"audio_dsp_plugin", MENU_SETTINGS_AUDIO_DSP_FILTER, setting_data);
         add_setting_entry(menu,"audio_enable", 0, setting_data);
         add_setting_entry(menu,"audio_mute", 0, setting_data);
         add_setting_entry(menu,"audio_latency", 0, setting_data);
         add_setting_entry(menu,"audio_sync", 0, setting_data);
         add_setting_entry(menu,"audio_rate_control_delta", 0, setting_data);
         add_setting_entry(menu,"system_bgm_enable", 0, setting_data);
         add_setting_entry(menu,"audio_volume", 0, setting_data);
         add_setting_entry(menu,"audio_device", 0, setting_data);
         break;
      case MENU_SETTINGS_DRIVERS:
         file_list_clear(menu->selection_buf);
         add_setting_entry(menu,"video_driver", 0, setting_data);
         add_setting_entry(menu,"audio_driver", 0, setting_data);
         add_setting_entry(menu,"audio_resampler_driver", 0, setting_data);
         add_setting_entry(menu,"input_driver", 0, setting_data);
         add_setting_entry(menu,"camera_driver", 0, setting_data);
         add_setting_entry(menu,"location_driver", 0, setting_data);
         add_setting_entry(menu,"menu_driver", 0, setting_data);
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
         add_setting_entry(menu,"core_list", MENU_SETTINGS_CORE, setting_data);
         add_setting_entry(menu,"history_list", MENU_SETTINGS_OPEN_HISTORY, setting_data);
         add_setting_entry(menu,"detect_core_list", MENU_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, setting_data);
         add_setting_entry(menu,"load_content", MENU_SETTINGS_OPEN_FILEBROWSER, setting_data);
         add_setting_entry(menu,"core_options", MENU_SETTINGS_CORE_OPTIONS, setting_data);
         add_setting_entry(menu,"core_information", MENU_SETTINGS_CORE_INFO, setting_data);
         add_setting_entry(menu,"settings", MENU_SETTINGS_OPTIONS, setting_data);
         add_setting_entry(menu,"performance_counters", MENU_SETTINGS_PERFORMANCE_COUNTERS, setting_data);
         add_setting_entry(menu,"savestate", MENU_SETTINGS_SAVESTATE_SAVE, setting_data);
         add_setting_entry(menu,"loadstate", MENU_SETTINGS_SAVESTATE_LOAD, setting_data);
         add_setting_entry(menu,"take_screenshot", 0, setting_data);
         add_setting_entry(menu,"resume_content", 0, setting_data);
         add_setting_entry(menu,"restart_content", 0, setting_data);
         add_setting_entry(menu,"restart_retroarch", 0, setting_data);
         add_setting_entry(menu,"configurations", MENU_SETTINGS_CONFIG, setting_data);
         add_setting_entry(menu,"save_new_config", 0, setting_data);
         add_setting_entry(menu,"help", 0, setting_data);
         add_setting_entry(menu,"quit_retroarch", 0, setting_data);
         break;
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, menu_type);
}
