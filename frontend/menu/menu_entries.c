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
#include "../../file_ext.h"

static void entries_refresh(file_list_t *list)
{
   /* Before a refresh, we could have deleted a file on disk, causing
    * selection_ptr to suddendly be out of range.
    * Ensure it doesn't overflow. */

   if (driver.menu->selection_ptr >= file_list_get_size(list)
         && file_list_get_size(list))
      menu_set_navigation(driver.menu, file_list_get_size(list) - 1);
   else if (!file_list_get_size(list))
      menu_clear_navigation(driver.menu);
}

static inline struct gfx_shader *shader_manager_get_current_shader(
      menu_handle_t *menu, unsigned type)
{
   if (type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
      return menu->shader;
   else if (driver.video_poke && driver.video_data &&
         driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);
   return NULL;
}

static void add_setting_entry(menu_handle_t *menu,
      file_list_t *list,
      const char *label, unsigned id,
      rarch_setting_t *settings)
{
   rarch_setting_t *setting = (rarch_setting_t*)
      setting_data_find_setting(settings, label);

   if (setting)
      file_list_push(list, setting->short_description,
            setting->name, id, 0);
}

void menu_entries_push_perfcounter(menu_handle_t *menu,
      file_list_t *list,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   int i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         file_list_push(list, counters[i]->ident, "",
               id + i, 0);
}

void menu_entries_pop(file_list_t *list)
{
   if (file_list_get_size(list) > 1)
   {
      file_list_pop(list, &driver.menu->selection_ptr);
      driver.menu->need_refresh = true;
   }
}

int menu_entries_push_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned menu_type)
{
   unsigned i;
   char tmp[256];
   size_t list_size = 0;
   rarch_setting_t *setting_data = (rarch_setting_t *)
      setting_data_get_list();
   bool do_action = false;

#if 0
   RARCH_LOG("Label is: %s\n", label);
   RARCH_LOG("Path is: %s\n", path);
   RARCH_LOG("Menu type is: %d\n", menu_type);
#endif

   if (!strcmp(label, "mainmenu"))
   {
      setting_data = (rarch_setting_t *)setting_data_get_mainmenu(true);
      file_list_clear(list);
      add_setting_entry(menu,list,"core_list", MENU_SETTINGS_CORE, setting_data);
      add_setting_entry(menu,list,"history_list", 0, setting_data);
      add_setting_entry(menu,list,"detect_core_list", 0, setting_data);
      add_setting_entry(menu,list,"load_content", 0, setting_data);
      add_setting_entry(menu,list,"core_options", MENU_SETTINGS_CORE_OPTIONS, setting_data);
      add_setting_entry(menu,list,"core_information", MENU_SETTINGS_CORE_INFO, setting_data);
      add_setting_entry(menu,list,"settings", MENU_SETTINGS_OPTIONS, setting_data);
      add_setting_entry(menu,list,"performance_counters", MENU_SETTINGS_PERFORMANCE_COUNTERS, setting_data);
      add_setting_entry(menu,list,"savestate", 0, setting_data);
      add_setting_entry(menu,list,"loadstate", 0, setting_data);
      add_setting_entry(menu,list,"take_screenshot", 0, setting_data);
      add_setting_entry(menu,list,"resume_content", 0, setting_data);
      add_setting_entry(menu,list,"restart_content", 0, setting_data);
      add_setting_entry(menu,list,"restart_retroarch", 0, setting_data);
      add_setting_entry(menu,list,"configurations", MENU_SETTINGS_CONFIG, setting_data);
      add_setting_entry(menu,list,"save_new_config", 0, setting_data);
      add_setting_entry(menu,list,"help", 0, setting_data);
      add_setting_entry(menu,list,"quit_retroarch", 0, setting_data);
   }
   else if (!strcmp(path, "General Options"))
   {
      file_list_clear(list);
      add_setting_entry(menu,list,"libretro_log_level", 0, setting_data);
      add_setting_entry(menu,list,"log_verbosity", 0, setting_data);
      add_setting_entry(menu,list,"perfcnt_enable", 0, setting_data);
      add_setting_entry(menu,list,"game_history_size", 0, setting_data);
      add_setting_entry(menu,list,"config_save_on_exit", 0, setting_data);
      add_setting_entry(menu,list,"core_specific_config", 0, setting_data);
      add_setting_entry(menu,list,"video_gpu_screenshot", 0, setting_data);
      add_setting_entry(menu,list,"dummy_on_core_shutdown", 0, setting_data);
      add_setting_entry(menu,list,"fps_show", 0, setting_data);
      add_setting_entry(menu,list,"fastforward_ratio", 0, setting_data);
      add_setting_entry(menu,list,"slowmotion_ratio", 0, setting_data);
      add_setting_entry(menu,list,"rewind_enable", 0, setting_data);
      add_setting_entry(menu,list,"rewind_granularity", 0, setting_data);
      add_setting_entry(menu,list,"block_sram_overwrite", 0, setting_data);
      add_setting_entry(menu,list,"autosave_interval", 0, setting_data);
      add_setting_entry(menu,list,"video_disable_composition", 0, setting_data);
      add_setting_entry(menu,list,"pause_nonactive", 0, setting_data);
      add_setting_entry(menu,list,"savestate_auto_save", 0, setting_data);
      add_setting_entry(menu,list,"savestate_auto_load", 0, setting_data);
   }
   else if (!strcmp(label, "history_list"))
   {
      file_list_clear(list);
      list_size = content_playlist_size(g_extern.history);

      for (i = 0; i < list_size; i++)
      {
         char fill_buf[PATH_MAX];
         const char *path      = NULL;
         const char *core_name = NULL;

         content_playlist_get_index(g_extern.history, i,
               &path, NULL, &core_name);
         strlcpy(fill_buf, core_name, sizeof(fill_buf));

         if (path)
         {
            char path_short[PATH_MAX];
            fill_pathname(path_short, path_basename(path), "",
                  sizeof(path_short));

            snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
                  path_short, core_name);
         }

         file_list_push(list, fill_buf, "",
               MENU_FILE_PLAYLIST_ENTRY, 0);

         do_action = true;
      }
   }
   else
   {
      switch (menu_type)
      {
         case MENU_SETTINGS_DEFERRED_CORE:
            {
               const core_info_t *info = NULL;
               file_list_clear(list);
               core_info_list_get_supported_cores(driver.menu->core_info,
                     driver.menu->deferred_path, &info, &list_size);
               for (i = 0; i < list_size; i++)
               {
                  file_list_push(list, info[i].path, "",
                        MENU_FILE_PLAIN, 0);
                  file_list_set_alt_at_offset(list, i,
                        info[i].display_name);
               }
               file_list_sort_on_alt(list);

               do_action = true;
            }
            break;
         case MENU_SETTINGS_SHADER_PARAMETERS:
         case MENU_SETTINGS_SHADER_PRESET_PARAMETERS:
            {
               file_list_clear(list);

               struct gfx_shader *shader = (struct gfx_shader*)
                  shader_manager_get_current_shader(menu, menu_type);

               if (shader)
                  for (i = 0; i < shader->num_parameters; i++)
                     file_list_push(list,
                           shader->parameters[i].desc, "",
                           MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
               menu->parameter_shader = shader;
               break;
            }
         case MENU_SETTINGS_SHADER_OPTIONS:
            {
               struct gfx_shader *shader = (struct gfx_shader*)menu->shader;

               if (!shader)
                  return -1;

               file_list_clear(list);
               file_list_push(list, "Apply Shader Changes", "",
                     MENU_SETTINGS_SHADER_APPLY, 0);
               file_list_push(list, "Default Filter", "",
                     MENU_SETTINGS_SHADER_FILTER, 0);
               file_list_push(list, "Load Shader Preset", "",
                     MENU_SETTINGS_SHADER_PRESET, 0);
               file_list_push(list, "Save As Shader Preset", "",
                     MENU_SETTINGS_SHADER_PRESET_SAVE, 0);
               file_list_push(list, "Parameters (Current)", "",
                     MENU_SETTINGS_SHADER_PARAMETERS, 0);
               file_list_push(list, "Parameters (Menu)", "",
                     MENU_SETTINGS_SHADER_PRESET_PARAMETERS, 0);
               file_list_push(list, "Shader Passes", "",
                     MENU_SETTINGS_SHADER_PASSES, 0);

               for (i = 0; i < shader->passes; i++)
               {
                  char buf[64];

                  snprintf(buf, sizeof(buf), "Shader #%u", i);
                  file_list_push(list, buf, "",
                        MENU_SETTINGS_SHADER_0 + 3 * i, 0);

                  snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
                  file_list_push(list, buf, "",
                        MENU_SETTINGS_SHADER_0_FILTER + 3 * i, 0);

                  snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
                  file_list_push(list, buf, "",
                        MENU_SETTINGS_SHADER_0_SCALE + 3 * i, 0);
               }
            }
            break;
         case MENU_SETTINGS_VIDEO_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"video_shared_context", 0, setting_data);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
            file_list_push(list, "Screen Resolution", "",
                  MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
            add_setting_entry(menu,list,"video_viwidth", 0, setting_data);
            add_setting_entry(menu,list,"video_filter", 0, setting_data);
            add_setting_entry(menu,list, "pal60_enable", 0, setting_data);
            add_setting_entry(menu,list,"video_smooth", 0, setting_data);
            add_setting_entry(menu,list, "soft_filter", 0, setting_data);
            add_setting_entry(menu,list,"video_gamma", 0, setting_data);
            add_setting_entry(menu,list,"video_filter_flicker", 0,
                  setting_data);
            add_setting_entry(menu,list,"video_scale_integer", 0, setting_data);
            add_setting_entry(menu,list,"aspect_ratio_index", 0, setting_data);
            file_list_push(list, "Custom Ratio", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
            add_setting_entry(menu,list,"video_fullscreen", 0, setting_data);
            add_setting_entry(menu,list,"video_windowed_fullscreen", 0, setting_data);
            add_setting_entry(menu,list,"video_rotation", 0, setting_data);
            add_setting_entry(menu,list,"video_vsync", 0, setting_data);
            add_setting_entry(menu,list,"video_hard_sync", 0, setting_data);
            add_setting_entry(menu,list,"video_hard_sync_frames", 0, setting_data);
            add_setting_entry(menu,list,"video_frame_delay", 0, setting_data);
            add_setting_entry(menu,list,"video_black_frame_insertion", 0, setting_data);
            add_setting_entry(menu,list,"video_swap_interval", 0, setting_data);
            add_setting_entry(menu,list,"video_threaded", 0, setting_data);
            add_setting_entry(menu,list,"video_scale", 0, setting_data);
            add_setting_entry(menu,list,"video_crop_overscan", 0, setting_data);
            add_setting_entry(menu,list,"video_monitor_index", 0, setting_data);
            add_setting_entry(menu,list,"video_refresh_rate", 0, setting_data);
            add_setting_entry(menu,list,"video_refresh_rate_auto", 0, setting_data);
            break;
         case MENU_SETTINGS_FONT_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"video_font_enable", 0, setting_data);
            add_setting_entry(menu,list,"video_font_size", 0, setting_data);
            break;
         case MENU_SETTINGS_CORE_OPTIONS:
            file_list_clear(list);

            if (g_extern.system.core_options)
            {
               size_t i, opts;

               opts = core_option_size(g_extern.system.core_options);
               for (i = 0; i < opts; i++)
                  file_list_push(list,
                        core_option_get_desc(g_extern.system.core_options, i), "",
                        MENU_SETTINGS_CORE_OPTION_START + i, 0);
            }
            else
               file_list_push(list, "No options available.", "",
                     MENU_SETTINGS_CORE_OPTION_NONE, 0);
            break;
         case MENU_SETTINGS_CORE_INFO:
            {
               core_info_t *info = (core_info_t*)menu->core_info_current;
               file_list_clear(list);

               if (info->data)
               {
                  snprintf(tmp, sizeof(tmp), "Core name: %s",
                        info->display_name ? info->display_name : "");
                  file_list_push(list, tmp, "",
                        MENU_SETTINGS_CORE_INFO_NONE, 0);

                  if (info->authors_list)
                  {
                     strlcpy(tmp, "Authors: ", sizeof(tmp));
                     string_list_join_concat(tmp, sizeof(tmp),
                           info->authors_list, ", ");
                     file_list_push(list, tmp, "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);
                  }

                  if (info->permissions_list)
                  {
                     strlcpy(tmp, "Permissions: ", sizeof(tmp));
                     string_list_join_concat(tmp, sizeof(tmp),
                           info->permissions_list, ", ");
                     file_list_push(list, tmp, "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);
                  }

                  if (info->supported_extensions_list)
                  {
                     strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
                     string_list_join_concat(tmp, sizeof(tmp),
                           info->supported_extensions_list, ", ");
                     file_list_push(list, tmp, "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);
                  }

                  if (info->firmware_count > 0)
                  {
                     core_info_list_update_missing_firmware(menu->core_info, info->path,
                           g_settings.system_directory);

                     file_list_push(list, "Firmware: ", "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);
                     for (i = 0; i < info->firmware_count; i++)
                     {
                        if (info->firmware[i].desc)
                        {
                           snprintf(tmp, sizeof(tmp), "	name: %s",
                                 info->firmware[i].desc ? info->firmware[i].desc : "");
                           file_list_push(list, tmp, "",
                                 MENU_SETTINGS_CORE_INFO_NONE, 0);

                           snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                                 info->firmware[i].missing ?
                                 "missing" : "present",
                                 info->firmware[i].optional ?
                                 "optional" : "required");
                           file_list_push(list, tmp, "",
                                 MENU_SETTINGS_CORE_INFO_NONE, 0);
                        }
                     }
                  }

                  if (info->notes)
                  {
                     snprintf(tmp, sizeof(tmp), "Core notes: ");
                     file_list_push(list, tmp, "",
                           MENU_SETTINGS_CORE_INFO_NONE, 0);

                     for (i = 0; i < info->note_list->size; i++)
                     {
                        snprintf(tmp, sizeof(tmp), " %s",
                              info->note_list->elems[i].data);
                        file_list_push(list, tmp, "",
                              MENU_SETTINGS_CORE_INFO_NONE, 0);
                     }
                  }
               }
               else
                  file_list_push(list,
                        "No information available.", "",
                        MENU_SETTINGS_CORE_OPTION_NONE, 0);
            }
            break;
         case MENU_SETTINGS_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"Driver Options", MENU_SETTINGS_DRIVERS, setting_data);
            add_setting_entry(menu,list,"General Options", MENU_SETTINGS_GENERAL_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Video Options", MENU_SETTINGS_VIDEO_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Shader Options", MENU_SETTINGS_SHADER_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Font Options", MENU_SETTINGS_FONT_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Audio Options", MENU_SETTINGS_AUDIO_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Input Options", MENU_SETTINGS_INPUT_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Overlay Options", MENU_SETTINGS_OVERLAY_OPTIONS,
                  setting_data);
            add_setting_entry(menu,list,"User Options", MENU_SETTINGS_USER_OPTIONS, setting_data);
            add_setting_entry(menu,list,"Netplay Options", MENU_SETTINGS_NETPLAY_OPTIONS,
                  setting_data);
            add_setting_entry(menu,list,"Path Options", MENU_SETTINGS_PATH_OPTIONS, setting_data);
            if (g_extern.main_is_init && !g_extern.libretro_dummy)
            {
               if (g_extern.system.disk_control.get_num_images)
                  file_list_push(list, "Disk Options", "",
                        MENU_SETTINGS_DISK_OPTIONS, 0);
            }
            add_setting_entry(menu,list,"Privacy Options", MENU_SETTINGS_PRIVACY_OPTIONS,
                  setting_data);
            break;
         case MENU_SETTINGS_PRIVACY_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"camera_allow", 0, setting_data);
            add_setting_entry(menu,list,"location_allow", 0, setting_data);
            break;
         case MENU_SETTINGS_DISK_OPTIONS:
            file_list_clear(list);
            file_list_push(list, "Disk Index", "",
                  MENU_SETTINGS_DISK_INDEX, 0);
            file_list_push(list, "Disk Image Append", "",
                  MENU_SETTINGS_DISK_APPEND, 0);
            break;
         case MENU_SETTINGS_OVERLAY_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"input_overlay", MENU_SETTINGS_OVERLAY_PRESET,
                  setting_data);
            add_setting_entry(menu,list,"input_overlay_opacity", 0, setting_data);
            add_setting_entry(menu,list,"input_overlay_scale", 0, setting_data);
            break;
         case MENU_SETTINGS_USER_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"netplay_nickname", 0, setting_data);
            add_setting_entry(menu,list,"user_language", 0, setting_data);
            break;
         case MENU_SETTINGS_NETPLAY_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"netplay_enable", 0, setting_data);
            add_setting_entry(menu,list,"netplay_mode", 0, setting_data);
            add_setting_entry(menu,list,"netplay_spectator_mode_enable", 0, setting_data);
            add_setting_entry(menu,list,"netplay_ip_address", 0, setting_data);
            add_setting_entry(menu,list,"netplay_tcp_udp_port", 0, setting_data);
            add_setting_entry(menu,list,"netplay_delay_frames", 0, setting_data);
            break;
         case MENU_SETTINGS_PATH_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"rgui_browser_directory",
                  MENU_BROWSER_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"content_directory",
                  MENU_CONTENT_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"assets_directory",
                  MENU_ASSETS_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"rgui_config_directory",
                  MENU_CONFIG_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"libretro_dir_path",
                  MENU_LIBRETRO_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"libretro_info_path",
                  MENU_LIBRETRO_INFO_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"game_history_path",
                  MENU_CONTENT_HISTORY_PATH, setting_data);
            add_setting_entry(menu,list,"video_filter_dir",
                  MENU_FILTER_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"audio_filter_dir",
                  MENU_DSP_FILTER_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"video_shader_dir", MENU_SHADER_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"savestate_directory", MENU_SAVESTATE_DIR_PATH,
                  setting_data);
            add_setting_entry(menu,list,"savefile_directory", MENU_SAVEFILE_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"overlay_directory", MENU_OVERLAY_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"system_directory", MENU_SYSTEM_DIR_PATH, setting_data);
            add_setting_entry(menu,list,"screenshot_directory", MENU_SCREENSHOT_DIR_PATH,
                  setting_data);
            add_setting_entry(menu,list,"joypad_autoconfig_dir", MENU_AUTOCONFIG_DIR_PATH,
                  setting_data);
            add_setting_entry(menu,list,"extraction_directory", MENU_EXTRACTION_DIR_PATH,
                  setting_data);
            break;
         case MENU_SETTINGS_INPUT_OPTIONS:
            file_list_clear(list);
            file_list_push(list, "Player", "",
                  MENU_SETTINGS_BIND_PLAYER, 0);
            file_list_push(list, "Device", "",
                  MENU_SETTINGS_BIND_DEVICE, 0);
            file_list_push(list, "Device Type", "",
                  MENU_SETTINGS_BIND_DEVICE_TYPE, 0);
            file_list_push(list, "Analog D-pad Mode", "",
                  MENU_SETTINGS_BIND_ANALOG_MODE, 0);
            add_setting_entry(menu,list,"input_axis_threshold", 0, setting_data);
            add_setting_entry(menu,list,"input_autodetect_enable", 0, setting_data);
            file_list_push(list, "Bind Mode", "",
                  MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
            file_list_push(list, "Configure All (RetroPad)", "",
                  MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
            file_list_push(list, "Default All (RetroPad)", "",
                  MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
            add_setting_entry(menu,list,"osk_enable", 0, setting_data);
            for (i = MENU_SETTINGS_BIND_BEGIN; i <= MENU_SETTINGS_BIND_ALL_LAST; i++)
               file_list_push(list,
                     input_config_bind_map[i - MENU_SETTINGS_BIND_BEGIN].desc,
                     "", i, 0);
            break;
         case MENU_SETTINGS_AUDIO_OPTIONS:
            file_list_clear(list);
            add_setting_entry(menu,list,"audio_dsp_plugin", 0, setting_data);
            add_setting_entry(menu,list,"audio_enable", 0, setting_data);
            add_setting_entry(menu,list,"audio_mute", 0, setting_data);
            add_setting_entry(menu,list,"audio_latency", 0, setting_data);
            add_setting_entry(menu,list,"audio_sync", 0, setting_data);
            add_setting_entry(menu,list,"audio_rate_control_delta", 0, setting_data);
            add_setting_entry(menu,list,"system_bgm_enable", 0, setting_data);
            add_setting_entry(menu,list,"audio_volume", 0, setting_data);
            add_setting_entry(menu,list,"audio_device", 0, setting_data);
            break;
         case MENU_SETTINGS_DRIVERS:
            file_list_clear(list);
            add_setting_entry(menu,list,"video_driver", 0, setting_data);
            add_setting_entry(menu,list,"audio_driver", 0, setting_data);
            add_setting_entry(menu,list,"audio_resampler_driver", 0, setting_data);
            add_setting_entry(menu,list,"input_driver", 0, setting_data);
            add_setting_entry(menu,list,"camera_driver", 0, setting_data);
            add_setting_entry(menu,list,"location_driver", 0, setting_data);
            add_setting_entry(menu,list,"menu_driver", 0, setting_data);
            break;
         case MENU_SETTINGS_PERFORMANCE_COUNTERS:
            file_list_clear(list);
            file_list_push(list, "Frontend Counters", "",
                  MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND, 0);
            file_list_push(list, "Core Counters", "",
                  MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO, 0);
            break;
         case MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO:
            file_list_clear(list);
            menu_entries_push_perfcounter(menu, list, perf_counters_libretro,
                  perf_ptr_libretro, MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
            break;
         case MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND:
            file_list_clear(list);
            menu_entries_push_perfcounter(menu, list, perf_counters_rarch,
                  perf_ptr_rarch, MENU_SETTINGS_PERF_COUNTERS_BEGIN);
            break;
      }
   }

   if (do_action)
   {
      driver.menu->scroll_indices_size = 0;
      if (strcmp(label, "history_list") != 0)
         menu_build_scroll_indices(list);

      entries_refresh(list);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, path, label, menu_type);

   return 0;
}

int menu_parse_check(const char *label, unsigned menu_type)
{
#if 0
   RARCH_LOG("label is menu_parse_check: %s\n", label);
#endif
   if (!((menu_type == MENU_FILE_DIRECTORY ||
            menu_common_type_is(menu_type) == MENU_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY ||
            menu_type == MENU_SETTINGS_OVERLAY_PRESET ||
            menu_type == MENU_CONTENT_HISTORY_PATH ||
            !strcmp(label, "video_filter") ||
            !strcmp(label, "audio_dsp_plugin") ||
            menu_type == MENU_SETTINGS_CORE ||
            menu_type == MENU_SETTINGS_CONFIG ||
            menu_type == MENU_SETTINGS_DISK_APPEND)))
      return -1;
   return 0;
}

int menu_parse_and_resolve(file_list_t *list, file_list_t *menu_list)
{
   size_t i, list_size;
   unsigned menu_type = 0;

   const char *dir = NULL;
   const char *label = NULL;

   file_list_get_last(menu_list, &dir, &label, &menu_type);

#if 0
   RARCH_LOG("label: %s\n", label);
#endif

   if (!strcmp(label, "history_list") || 
         menu_type == MENU_SETTINGS_DEFERRED_CORE)
      return menu_entries_push_list(driver.menu, list, dir, label, menu_type);

   if (menu_parse_check(label, menu_type) == -1)
      return - 1;

   file_list_clear(list);

   if (!*dir)
   {
#if defined(GEKKO)
#ifdef HW_RVL
      file_list_push(list,
            "sd:/", "", menu_type, 0);
      file_list_push(list,
            "usb:/", "", menu_type, 0);
#endif
      file_list_push(list,
            "carda:/", "", menu_type, 0);
      file_list_push(list,
            "cardb:/", "", menu_type, 0);
#elif defined(_XBOX1)
      file_list_push(list,
            "C:", "", menu_type, 0);
      file_list_push(list,
            "D:", "", menu_type, 0);
      file_list_push(list,
            "E:", "", menu_type, 0);
      file_list_push(list,
            "F:", "", menu_type, 0);
      file_list_push(list,
            "G:", "", menu_type, 0);
#elif defined(_XBOX360)
      file_list_push(list,
            "game:", "", menu_type, 0);
#elif defined(_WIN32)
      unsigned drives = GetLogicalDrives();
      char drive[] = " :\\";
      for (i = 0; i < 32; i++)
      {
         drive[0] = 'A' + i;
         if (drives & (1 << i))
            file_list_push(list,
                  drive, "", menu_type, 0);
      }
#elif defined(__CELLOS_LV2__)
      file_list_push(list,
            "/app_home/",   "", menu_type, 0);
      file_list_push(list,
            "/dev_hdd0/",   "", menu_type, 0);
      file_list_push(list,
            "/dev_hdd1/",   "", menu_type, 0);
      file_list_push(list,
            "/host_root/",  "", menu_type, 0);
      file_list_push(list,
            "/dev_usb000/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb001/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb002/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb003/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb004/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb005/", "", menu_type, 0);
      file_list_push(list,
            "/dev_usb006/", "", menu_type, 0);
#elif defined(PSP)
      file_list_push(list,
            "ms0:/", "", menu_type, 0);
      file_list_push(list,
            "ef0:/", "", menu_type, 0);
      file_list_push(list,
            "host0:/", "", menu_type, 0);
#elif defined(IOS)
      file_list_push(list,
            "/var/mobile/", "", menu_type, 0);
      file_list_push(list,
            g_defaults.core_dir, "",menu_type, 0);
      file_list_push(list, "/", "",
            menu_type, 0);
#else
      file_list_push(list, "/", "",
            menu_type, 0);
#endif
      return 0;
   }
#if defined(GEKKO) && defined(HW_RVL)
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(dir);

   if (dev != -1 && !gx_devices[dev].mounted &&
         gx_devices[dev].interface->isInserted())
      fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   const char *exts;
   char ext_buf[1024];

   //RARCH_LOG("LABEL: %s\n", label);
   if (!strcmp(label, "core_list"))
      exts = EXT_EXECUTABLES;
   else if (!strcmp(label, "configurations"))
      exts = "cfg";
   else if (!strcmp(label, "video_shader_preset"))
      exts = "cgp|glslp";
   else if (!strcmp(label, "video_shader_pass"))
      exts = "cg|glsl";
   else if (!strcmp(label, "video_filter"))
      exts = "filt";
   else if (!strcmp(label, "audio_dsp_plugin"))
      exts = "dsp";
   else if (!strcmp(label, "input_overlay"))
      exts = "cfg";
   else if (menu_type == MENU_CONTENT_HISTORY_PATH)
      exts = "cfg";
   else if (menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY)
      exts = ""; /* we ignore files anyway */
   else if (driver.menu->defer_core)
      exts = driver.menu->core_info ? core_info_list_get_all_extensions(
            driver.menu->core_info) : "";
   else if (driver.menu->info.valid_extensions)
   {
      exts = ext_buf;
      if (*driver.menu->info.valid_extensions)
         snprintf(ext_buf, sizeof(ext_buf), "%s|zip",
               driver.menu->info.valid_extensions);
      else
         *ext_buf = '\0';
   }
   else
      exts = g_extern.system.valid_extensions;

   struct string_list *str_list = dir_list_new(dir, exts, true);
   if (!str_list)
      return -1;

   dir_list_sort(str_list, true);

   if (menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY)
      file_list_push(list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   list_size = str_list->size;
   for (i = 0; i < str_list->size; i++)
   {
      bool is_dir = str_list->elems[i].attr.b;

      if ((menu_common_type_is(menu_type) == MENU_FILE_DIRECTORY) && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      const char *path = str_list->elems[i].data;
      if (*dir)
         path = path_basename(path);

#ifdef HAVE_LIBRETRO_MANAGEMENT
      if (!strcmp(label, "core_list") && (is_dir ||
               strcasecmp(path, SALAMANDER_FILE) == 0))
         continue;
#endif

      /* Push menu_type further down in the chain.
       * Needed for shader manager currently. */
      file_list_push(list, path, "",
            is_dir ? menu_type : MENU_FILE_PLAIN, 0);
   }

   menu_entries_push_list(driver.menu, list,
         dir, label, menu_type);
   string_list_free(str_list);

   if (!strcmp(label, "core_list"))
   {
      file_list_get_last(menu_list, &dir, NULL,
            &menu_type);
      list_size = file_list_get_size(list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX], display_name[256];
         const char *path = NULL;
         unsigned type = 0;

         file_list_get_at_offset(list, i, &path, NULL, &type);
         if (type != MENU_FILE_PLAIN)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (driver.menu->core_info &&
               core_info_list_get_display_name(driver.menu->core_info,
                  core_path, display_name, sizeof(display_name)))
            file_list_set_alt_at_offset(list, i, display_name);
      }
      file_list_sort_on_alt(list);
   }

   driver.menu->scroll_indices_size = 0;
   if (strcmp(label, "history_list") != 0)
      menu_build_scroll_indices(list);

   entries_refresh(list);
   
   return 0;
}

void menu_flush_stack_type(file_list_t *list,
      unsigned final_type)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list, &path, &label, &type);
   while (type != final_type)
   {
      file_list_pop(list, &driver.menu->selection_ptr);
      file_list_get_last(list, &path, &label, &type);
   }
}

void menu_entries_push(file_list_t *list,
      const char *path, const char *label, unsigned type,
      size_t directory_ptr)
{
   file_list_push(list, path, label, type, directory_ptr);
   menu_clear_navigation(driver.menu);
   driver.menu->need_refresh = true;
}
