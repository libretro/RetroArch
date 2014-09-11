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
      menu_handle_t *menu, const char *label, unsigned type)
{
   if (!strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "video_shader_parameters"))
      return menu->shader;
   else if (driver.video_poke && driver.video_data &&
         driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);
   return NULL;
}

static inline bool menu_list_elem_is_dir(file_list_t *buf,
      unsigned offset)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   file_list_get_at_offset(buf, offset, &path, &label, &type);

   return type != MENU_FILE_PLAIN;
}

static inline int menu_list_get_first_char(file_list_t *buf,
      unsigned offset)
{
   int ret;
   const char *path = NULL;

   file_list_get_alt_at_offset(buf, offset, &path);
   ret = tolower(*path);

   /* "Normalize" non-alphabetical entries so they 
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static void menu_build_scroll_indices(file_list_t *buf)
{
   size_t i;
   int current;
   bool current_is_dir;

   if (!driver.menu || !buf)
      return;

   driver.menu->scroll_indices_size = 0;
   if (!buf->size)
      return;

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 0;

   current = menu_list_get_first_char(buf, 0);
   current_is_dir = menu_list_elem_is_dir(buf, 0);

   for (i = 1; i < buf->size; i++)
   {
      int first = menu_list_get_first_char(buf, i);
      bool is_dir = menu_list_elem_is_dir(buf, i);

      if ((current_is_dir && !is_dir) || (first > current))
         driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 
      buf->size - 1;
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
   unsigned i;
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

static int setting_set_flags(rarch_setting_t *setting)
{
   if (setting->flags & SD_FLAG_ALLOW_INPUT)
      return MENU_FILE_LINEFEED;
   if (setting->flags & SD_FLAG_PUSH_ACTION)
      return MENU_FILE_SWITCH;
   if (setting->type == ST_PATH)
      return MENU_FILE_PATH;
   return 0;
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
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
            "Main Menu");

      file_list_clear(list);

      for (; setting->type != ST_END_GROUP; setting++)
      {
         if (
               setting->type == ST_GROUP ||
               setting->type == ST_SUB_GROUP ||
               setting->type == ST_END_SUB_GROUP
            )
            continue;

         file_list_push(list, setting->short_description,
               setting->name, setting_set_flags(setting), 0);
      }
   }
   else if (
         !strcmp(label, "Driver Options") ||
         !strcmp(label, "General Options") ||
         !strcmp(label, "Overlay Options") ||
         !strcmp(label, "Privacy Options") ||
         !strcmp(label, "Video Options") ||
         !strcmp(label, "Audio Options") ||
         !strcmp(label, "Path Options") ||
         !strcmp(label, "Font Options") ||
         !strcmp(label, "User Options") ||
         !strcmp(label, "Netplay Options")
         )
   {
      rarch_setting_t *setting_data = (rarch_setting_t*)setting_data_get_list();
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
            label);

      file_list_clear(list);

      if (!strcmp(label, "Video Options"))
      {
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         file_list_push(list, "Screen Resolution", "",
               MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         file_list_push(list, "Custom Ratio", "",
               MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
      }

      for (; setting->type != ST_END_GROUP; setting++)
      {
         if (
               setting->type == ST_GROUP ||
               setting->type == ST_SUB_GROUP ||
               setting->type == ST_END_SUB_GROUP
            )
            continue;

         file_list_push(list, setting->short_description,
               setting->name, setting_set_flags(setting), 0);
      }
   }
   else if (!strcmp(label, "settings"))
   {
      rarch_setting_t *setting_data = (rarch_setting_t*)setting_data_get_list();
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
            "Driver Options");

      file_list_clear(list);

      for (; setting->type != ST_NONE; setting++)
      {
         if (setting->type == ST_GROUP)
            file_list_push(list, setting->short_description,
                  setting->name, MENU_FILE_SWITCH, 0);
      }
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
   else if (!strcmp(label, "performance_counters"))
   {
      file_list_clear(list);
      file_list_push(list, "Frontend Counters", "frontend_counters",
            MENU_FILE_SWITCH, 0);
      file_list_push(list, "Core Counters", "core_counters",
            MENU_FILE_SWITCH, 0);
   }
   else if (!strcmp(label, "core_information"))
   {
      core_info_t *info = (core_info_t*)g_extern.core_info_current;
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

         if (info->licenses_list)
         {
            strlcpy(tmp, "License(s): ", sizeof(tmp));
            string_list_join_concat(tmp, sizeof(tmp),
                  info->licenses_list, ", ");
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
            core_info_list_update_missing_firmware(
                  g_extern.core_info, info->path,
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
   else if (!strcmp(label, "deferred_core_list"))
   {
      const core_info_t *info = NULL;
      file_list_clear(list);
      core_info_list_get_supported_cores(g_extern.core_info,
            driver.menu->deferred_path, &info, &list_size);
      for (i = 0; i < list_size; i++)
      {
         file_list_push(list, info[i].path, "",
               MENU_FILE_CORE, 0);
         file_list_set_alt_at_offset(list, i,
               info[i].display_name);
      }
      file_list_sort_on_alt(list);

      do_action = true;
   }
   else if (!strcmp(label, "core_counters"))
   {
      file_list_clear(list);
      menu_entries_push_perfcounter(menu, list, perf_counters_libretro,
            perf_ptr_libretro, MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   }
   else if (!strcmp(label, "frontend_counters"))
   {
      file_list_clear(list);
      menu_entries_push_perfcounter(menu, list, perf_counters_rarch,
            perf_ptr_rarch, MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   }
   else if (!strcmp(label, "core_options"))
   {
      file_list_clear(list);
      if (g_extern.system.core_options)
      {
         size_t i;
         size_t opts = core_option_size(g_extern.system.core_options);
         for (i = 0; i < opts; i++)
            file_list_push(list,
                  core_option_get_desc(g_extern.system.core_options, i), "",
                  MENU_SETTINGS_CORE_OPTION_START + i, 0);
      }
      else
         file_list_push(list, "No options available.", "",
               MENU_SETTINGS_CORE_OPTION_NONE, 0);
   }
   else if (!strcmp(label, "Input Options"))
   {
      file_list_clear(list);
      file_list_push(list, "Player", "input_bind_player_no", 0, 0);
      file_list_push(list, "Device", "input_bind_device_id", 0, 0);
      file_list_push(list, "Device Type", "input_bind_device_type", 0, 0);
      file_list_push(list, "Analog D-pad Mode", "input_bind_analog_dpad_mode", 0, 0);
      add_setting_entry(menu,list,"input_axis_threshold", 0, setting_data);
      add_setting_entry(menu,list,"input_autodetect_enable", 0, setting_data);
      add_setting_entry(menu,list,"input_turbo_period", 0, setting_data);
      add_setting_entry(menu,list,"input_duty_cycle", 0, setting_data);
      file_list_push(list, "Bind Mode", "",
            MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
      file_list_push(list, "Configure All (RetroPad)", "",
            MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
      file_list_push(list, "Default All (RetroPad)", "",
            MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);
      add_setting_entry(menu,list,"osk_enable", 0, setting_data);
      for (i = MENU_SETTINGS_BIND_BEGIN; i <= MENU_SETTINGS_BIND_ALL_LAST; i++)
         add_setting_entry(menu, list, input_config_bind_map[i - MENU_SETTINGS_BIND_BEGIN].base, i, setting_data);
   }
   else if (!strcmp(label, "Shader Options"))
   {
      struct gfx_shader *shader = (struct gfx_shader*)menu->shader;

      if (!shader)
         return -1;

      file_list_clear(list);
      file_list_push(list, "Apply Shader Changes", "shader_apply_changes",
            MENU_FILE_SWITCH, 0);
      file_list_push(list, "Default Filter", "video_shader_default_filter",
            0, 0);
      file_list_push(list, "Load Shader Preset", "video_shader_preset",
            MENU_FILE_SWITCH, 0);
      file_list_push(list, "Shader Preset Save As",
            "video_shader_preset_save_as", MENU_FILE_LINEFEED_SWITCH, 0);
      file_list_push(list, "Parameters (Current)",
            "video_shader_parameters", MENU_FILE_SWITCH, 0);
      file_list_push(list, "Parameters (Menu)",
            "video_shader_preset_parameters", MENU_FILE_SWITCH, 0);
      file_list_push(list, "Shader Passes", "video_shader_num_passes",
            0, 0);

      for (i = 0; i < shader->passes; i++)
      {
         char buf[64];

         snprintf(buf, sizeof(buf), "Shader #%u", i);
         file_list_push(list, buf, "video_shader_pass",
               MENU_SETTINGS_SHADER_PASS_0 + i, 0);

         snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
         file_list_push(list, buf, "video_shader_filter_pass",
               MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0);

         snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
         file_list_push(list, buf, "video_shader_scale_pass",
               MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0);
      }
   }
   else if (!strcmp(label, "disk_options"))
   {
      file_list_clear(list);
      file_list_push(list, "Disk Index", "disk_index", 0, 0);
      file_list_push(list, "Disk Image Append", "disk_image_append", 0, 0);
   }
   else if (
         !strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "video_shader_parameters")
         )
   {
      file_list_clear(list);

      struct gfx_shader *shader = (struct gfx_shader*)
         shader_manager_get_current_shader(menu, label, menu_type);

      if (shader)
         for (i = 0; i < shader->num_parameters; i++)
            file_list_push(list,
                  shader->parameters[i].desc, label,
                  MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
      menu->parameter_shader = shader;
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
            menu_type == MENU_FILE_CARCHIVE ||
            menu_common_type_is(label, menu_type) == MENU_SETTINGS_SHADER_OPTIONS ||
            menu_common_type_is(label, menu_type) == MENU_FILE_DIRECTORY ||
            menu_type == MENU_FILE_PATH ||
            !strcmp(label, "core_list") ||
            !strcmp(label, "configurations") ||
            !strcmp(label, "disk_image_append"))))
      return -1;
   return 0;
}

int menu_parse_and_resolve(file_list_t *list, file_list_t *menu_list)
{
   size_t i, list_size;
   unsigned menu_type = 0, default_type_plain = MENU_FILE_PLAIN;

   const char *dir = NULL;
   const char *label = NULL;
   const char *exts = NULL;
   char ext_buf[PATH_MAX];

   file_list_get_last(menu_list, &dir, &label, &menu_type);

#if 0
   RARCH_LOG("label: %s\n", label);
#endif

   if (!strcmp(label, "history_list") || 
         !strcmp(label, "deferred_core_list"))
      return menu_entries_push_list(driver.menu, list, dir, label, menu_type);

   if (menu_parse_check(label, menu_type) == -1)
      return - 1;

   file_list_clear(list);

   if (!*dir)
   {
#if defined(GEKKO)
#ifdef HW_RVL
      file_list_push(list,
            "sd:/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "usb:/", "", MENU_FILE_DIRECTORY, 0);
#endif
      file_list_push(list,
            "carda:/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "cardb:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX1)
      file_list_push(list,
            "C:", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "D:", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "E:", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "F:", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "G:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX360)
      file_list_push(list,
            "game:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_WIN32)
      unsigned drives = GetLogicalDrives();
      char drive[] = " :\\";
      for (i = 0; i < 32; i++)
      {
         drive[0] = 'A' + i;
         if (drives & (1 << i))
            file_list_push(list,
                  drive, "", MENU_FILE_DIRECTORY, 0);
      }
#elif defined(__CELLOS_LV2__)
      file_list_push(list,
            "/app_home/",   "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_hdd0/",   "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_hdd1/",   "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/host_root/",  "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb000/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb001/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb002/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb003/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb004/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb005/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "/dev_usb006/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(PSP)
      file_list_push(list,
            "ms0:/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "ef0:/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            "host0:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(IOS)
      file_list_push(list,
            "/var/mobile/", "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list,
            g_defaults.core_dir, "", MENU_FILE_DIRECTORY, 0);
      file_list_push(list, "/", "",
            MENU_FILE_DIRECTORY, 0);
#else
      file_list_push(list, "/", "",
            MENU_FILE_DIRECTORY, 0);
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

   //RARCH_LOG("LABEL: %s\n", label);
   if (!strcmp(label, "core_list"))
      exts = EXT_EXECUTABLES;
   else if (!strcmp(label, "configurations"))
   {
      exts = "cfg";
      default_type_plain = MENU_FILE_CONFIG;
   }
   else if (!strcmp(label, "video_shader_preset"))
   {
      exts = "cgp|glslp";
      default_type_plain = MENU_FILE_SHADER_PRESET;
   }
   else if (!strcmp(label, "video_shader_pass"))
   {
      exts = "cg|glsl";
      default_type_plain = MENU_FILE_SHADER;
   }
   else if (!strcmp(label, "video_filter"))
   {
      exts = "filt";
      default_type_plain = MENU_FILE_VIDEOFILTER;
   }
   else if (!strcmp(label, "audio_dsp_plugin"))
   {
      exts = "dsp";
      default_type_plain = MENU_FILE_AUDIOFILTER;
   }
   else if (!strcmp(label, "input_overlay"))
   {
      exts = "cfg";
      default_type_plain = MENU_FILE_OVERLAY;
   }
   else if (!strcmp(label, "video_font_path"))
   {
      exts = "ttf";
      default_type_plain = MENU_FILE_FONT;
   }
   else if (!strcmp(label, "game_history_path"))
      exts = "cfg";
   else if (menu_common_type_is(label, menu_type) == MENU_FILE_DIRECTORY)
      exts = ""; /* we ignore files anyway */
   else if (!strcmp(label, "detect_core_list"))
      exts = g_extern.core_info ? core_info_list_get_all_extensions(
            g_extern.core_info) : "";
   else if (g_extern.menu.info.valid_extensions)
   {
      exts = ext_buf;
      if (*g_extern.menu.info.valid_extensions)
         snprintf(ext_buf, sizeof(ext_buf), "%s|zip",
               g_extern.menu.info.valid_extensions);
      else
         *ext_buf = '\0';
   }
   else
      exts = g_extern.system.valid_extensions;

   struct string_list *str_list = NULL;
   bool path_is_compressed = path_is_compressed_file(dir);

   if (path_is_compressed)
   {
      str_list = compressed_file_list_new(dir,exts);
   }
   else
   {
      str_list = dir_list_new(dir, exts, true);
   }
   if (!str_list)
      return -1;

   dir_list_sort(str_list, true);

   if (menu_common_type_is(label, menu_type) == MENU_FILE_DIRECTORY)
      file_list_push(list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   list_size = str_list->size;
   for (i = 0; i < str_list->size; i++)
   {
      menu_file_type_t file_type = MENU_FILE_NONE;
      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = MENU_FILE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = MENU_FILE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = MENU_FILE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            file_type = (menu_file_type_t)default_type_plain;
            break;
      }
      bool is_dir = (file_type == MENU_FILE_DIRECTORY);

      if ((menu_common_type_is(label, menu_type) == MENU_FILE_DIRECTORY) && !is_dir)
         continue;


      /* Need to preserve slash first time. */
      const char *path = str_list->elems[i].data;

      if (*dir && !path_is_compressed)
         path = path_basename(path);


#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
      if (!strcmp(label, "core_list") && (is_dir ||
               strcasecmp(path, SALAMANDER_FILE) == 0))
         continue;
#endif
#endif

      /* Push menu_type further down in the chain.
       * Needed for shader manager currently. */
      if (!strcmp(label, "core_list"))
      {
         /* Compressed cores are unsupported */
         if (file_type == MENU_FILE_CARCHIVE)
            continue;

         file_list_push(list, path, "",
               is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE, 0);
      }
      else
      file_list_push(list, path, "",
            file_type, 0);
   }

   menu_entries_push_list(driver.menu, list,
         dir, label, menu_type);
   string_list_free(str_list);

   if (!strcmp(label, "core_list"))
   {
      file_list_get_last(menu_list, &dir, NULL, NULL);
      list_size = file_list_get_size(list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX], display_name[256];
         const char *path = NULL;
         unsigned type = 0;

         file_list_get_at_offset(list, i, &path, NULL, &type);
         if (type != MENU_FILE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (g_extern.core_info &&
               core_info_list_get_display_name(g_extern.core_info,
                  core_path, display_name, sizeof(display_name)))
            file_list_set_alt_at_offset(list, i, display_name);
      }
      file_list_sort_on_alt(list);
   }

   driver.menu->scroll_indices_size = 0;
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

void menu_entries_pop_stack(file_list_t *list,
      const char *needle)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list, &path, &label, &type);
   while (strcmp(needle, label) == 0)
   {
      file_list_pop(list, &driver.menu->selection_ptr);
      file_list_get_last(list, &path, &label, &type);
   }
}

void menu_flush_stack_label(file_list_t *list,
      const char *needle)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list, &path, &label, &type);
   while (strcmp(needle, label) != 0)
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
