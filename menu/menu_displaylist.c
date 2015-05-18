/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stddef.h>

#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

#include "menu.h"
#include "menu_display.h"
#include "menu_displaylist.h"
#include "menu_navigation.h"
#include "menu_setting.h"

#include "../gfx/video_shader_driver.h"
#include "../config.features.h"
#include "../retroarch.h"
#include "../git_version.h"

#include "../performance.h"
#include "../settings.h"

#ifdef HAVE_NETWORKING
extern char *core_buf;
extern size_t core_len;

static void print_buf_lines(file_list_t *list, char *buf, int buf_size,
      unsigned type)
{
   int i;
   char c, *line_start = buf;

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;

      /* The end of the buffer, print the last bit */
      if (*(buf + i) == '\0')
         break;

      if (*(buf + i) != '\n')
         continue;

      /* Found a line ending, print the line and compute new line_start */

      /* Save the next char  */
      c = *(buf + i + 1);
      /* replace with \0 */
      *(buf + i + 1) = '\0';

      /* We need to strip the newline. */
      ln = strlen(line_start) - 1;
      if (line_start[ln] == '\n')
         line_start[ln] = '\0';

      menu_list_push(list, line_start, "",
            type, 0);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start = buf + i + 1;
   }
   /* If the buffer was completely full, and didn't end with a newline, just
    * ignore the partial last line.
    */
}
#endif

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(info->list,
               counters[i]->ident, "", id + i, 0);
}

static int menu_displaylist_push_perfcounter_generic(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned ident)
{
   menu_displaylist_push_perfcounter(info, counters, num, ident);
   return 0;
}

/**
 * menu_displaylist_parse_drive_list:
 * @list                     : File list handle.
 *
 * Generates default directory drive list.
 * Platform-specific.
 *
 **/
static void menu_displaylist_parse_drive_list(file_list_t *list)
{
   size_t i = 0;

   (void)i;

#if defined(GEKKO)
#ifdef HW_RVL
   menu_list_push(list,
         "sd:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "usb:/", "", MENU_FILE_DIRECTORY, 0);
#endif
   menu_list_push(list,
         "carda:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "cardb:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX1)
   menu_list_push(list,
         "C:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "D:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "E:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "F:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "G:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX360)
   menu_list_push(list,
         "game:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_WIN32)
   unsigned drives = GetLogicalDrives();
   char drive[] = " :\\";
   for (i = 0; i < 32; i++)
   {
      drive[0] = 'A' + i;
      if (drives & (1 << i))
         menu_list_push(list,
               drive, "", MENU_FILE_DIRECTORY, 0);
   }
#elif defined(__CELLOS_LV2__)
   menu_list_push(list,
         "/app_home/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_hdd0/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_hdd1/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/host_root/",  "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb000/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb001/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb002/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb003/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb004/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb005/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb006/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(PSP)
   menu_list_push(list,
         "ms0:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "ef0:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "host0:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_3DS)
   menu_list_push(list,
         "sdmc:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(IOS)
   menu_list_push(list,
         "/var/mobile/Documents/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/var/mobile/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         g_defaults.core_dir, "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list, "/", "",
         MENU_FILE_DIRECTORY, 0);
#else
   menu_list_push(list, "/", "",
         MENU_FILE_DIRECTORY, 0);
#endif
}

static int menu_displaylist_parse(menu_displaylist_info_t *info, bool *need_sort)
{
   size_t i, list_size;
   bool path_is_compressed, push_dir;
   int                   device = 0;
   struct string_list *str_list = NULL;
   settings_t *settings         = config_get_ptr();
   menu_handle_t *menu          = menu_driver_get_ptr();
   global_t *global             = global_get_ptr();

   (void)device;

   if (!*info->path)
   {
      menu_displaylist_parse_drive_list(info->list);
      return 0;
   }

#if defined(GEKKO) && defined(HW_RVL)
   slock_lock(gx_device_mutex);
   device = gx_get_device_from_path(info->path);

   if (device != -1 && !gx_devices[device].mounted &&
         gx_devices[device].interface->isInserted())
      fatMountSimple(gx_devices[device].name, gx_devices[device].interface);

   slock_unlock(gx_device_mutex);
#endif

   path_is_compressed = path_is_compressed_file(info->path);
   push_dir           = (info->setting 
         && info->setting->browser_selection_type == ST_DIR);

   if (path_is_compressed)
      str_list = compressed_file_list_new(info->path, info->exts);
   else
      str_list = dir_list_new(info->path,
            settings->menu.navigation.browser.filter.supported_extensions_enable 
            ? info->exts : NULL, true);

   if (push_dir)
      menu_list_push(info->list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   if (!str_list)
   {
      const char *str = path_is_compressed
            ? "Unable to read compressed file."
            : "Directory not found.";

      menu_list_push(info->list, str, "", 0, 0);
      return 0;
   }

   dir_list_sort(str_list, true);


   list_size = str_list->size;

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            "No items.", "", 0, 0);
      string_list_free(str_list);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      const char *path = NULL;
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
            if (!strcmp(info->label, "detect_core_list"))
            {
               if (path_is_compressed_file(str_list->elems[i].data))
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  file_type = MENU_FILE_CARCHIVE;
                  break;
               }
            }
            file_type = (menu_file_type_t)info->type_default;
            break;
      }

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (*info->path && !path_is_compressed)
         path = path_basename(path);


#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
      if (!strcmp(info->label, "core_list") && (is_dir ||
               strcasecmp(path, SALAMANDER_FILE) == 0))
         continue;
#endif
#endif

      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      if (!strcmp(info->label, "core_list"))
      {
         /* Compressed cores are unsupported */
         if (file_type == MENU_FILE_CARCHIVE)
            continue;

         menu_list_push(info->list, path, "",
               is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE, 0);
      }
      else
      menu_list_push(info->list, path, "",
            file_type, 0);
   }

   string_list_free(str_list);

   if (!strcmp(info->label, "core_list"))
   {
      const char *dir = NULL;
      menu_list_get_last_stack(menu->menu_list, &dir, NULL, NULL);
      list_size = file_list_get_size(info->list);

      for (i = 0; i < list_size; i++)
      {
         unsigned type = 0;
         char core_path[PATH_MAX_LENGTH], display_name[PATH_MAX_LENGTH];
         const char *path = NULL;

         menu_list_get_at_offset(info->list, i, &path, NULL, &type);

         if (type != MENU_FILE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (global->core_info &&
               core_info_list_get_display_name(global->core_info,
                  core_path, display_name, sizeof(display_name)))
            menu_list_set_alt_at_offset(info->list, i, display_name);
      }
      *need_sort = true;
   }

   return 0;
}

static int menu_displaylist_parse_settings(menu_handle_t *menu,
      menu_displaylist_info_t *info, unsigned setting_flags)
{
   rarch_setting_t *setting = NULL;
   settings_t *settings     = config_get_ptr();
   
   if (menu && menu->list_settings)
      settings_list_free(menu->list_settings);

   menu->list_settings      = setting_new(setting_flags);
   setting                  = menu_setting_find(info->label);

   if (!setting)
      return -1;


   for (; setting->type != ST_END_GROUP; setting++)
   {
      if (
            setting->type == ST_GROUP 
            || setting->type == ST_SUB_GROUP
            || setting->type == ST_END_SUB_GROUP
            || (setting->flags & SD_FLAG_ADVANCED && 
               !settings->menu.show_advanced_settings)
         )
         continue;

      menu_list_push(info->list, setting->short_description,
            setting->name, menu_setting_set_flags(setting), 0);
   }

   return 0;
}

static void menu_displaylist_push_horizontal_menu_list_content(
      file_list_t *list, core_info_t *info, const char* path)
{
   unsigned j;
   struct string_list *str_list = NULL;

   if (!info)
      return;

   str_list = (struct string_list*)dir_list_new(path,
         info->supported_extensions, true);

   if (!str_list)
      return;

   dir_list_sort(str_list, true);

   for (j = 0; j < str_list->size; j++)
   {
      const char *name = str_list->elems[j].data;
      
      if (!name)
         continue;

      if (str_list->elems[j].attr.i == RARCH_DIRECTORY)
         menu_displaylist_push_horizontal_menu_list_content(list, info, name);
      else
         menu_list_push(
               list, name,
               "content_actions",
               MENU_FILE_CONTENTLIST_ENTRY, 0);
   }

   string_list_free(str_list);
}

static int menu_displaylist_push_horizontal_menu_list_cores(
      file_list_t *list, core_info_t *info,
      const char *path, bool push_databases_enable)
{
   size_t i;
   settings_t *settings     = config_get_ptr();

   if (!info->supports_no_game)
      menu_displaylist_push_horizontal_menu_list_content(list, info, path);
   else
      menu_list_push(list, info->display_name, "content_actions",
            MENU_FILE_CONTENTLIST_ENTRY, 0);

   if (!push_databases_enable)
      return 0;
   if (!info->databases_list)
      return 0;

   for (i = 0; i < info->databases_list->size; i++)
   {
      char db_path[PATH_MAX_LENGTH];
      struct string_list *str_list = (struct string_list*)info->databases_list;

      if (!str_list)
         continue;

      fill_pathname_join(db_path, settings->content_database,
            str_list->elems[i].data, sizeof(db_path));
      strlcat(db_path, ".rdb", sizeof(db_path));

      if (!path_file_exists(db_path))
         continue;

      menu_list_push(list, path_basename(db_path), "core_database",
            MENU_FILE_RDB, 0);
   }

   return 0;
}

static int menu_displaylist_parse_horizontal_list(menu_displaylist_info_t *info) 
{
   core_info_t      *core_info = NULL;
   global_t            *global = global_get_ptr();
   core_info_list_t *info_list = (core_info_list_t*)global->core_info;
   menu_handle_t        *menu  = menu_driver_get_ptr();
   settings_t *settings        = config_get_ptr();

   if (!info_list)
      return -1;

   core_info = (core_info_t*)&info_list->list[menu->categories.selection_ptr - 1];

   if (!core_info)
      return -1;

   strlcpy(settings->libretro, core_info->path, sizeof(settings->libretro));

   menu_displaylist_push_horizontal_menu_list_cores(info->list,
         core_info, settings->core_assets_directory, true);

   return 0;
}

static int menu_displaylist_parse_historylist(menu_displaylist_info_t *info)
{
   unsigned i;
   size_t list_size = content_playlist_size(g_defaults.history);

   if (list_size <= 0)
   {
      menu_list_push(info->list, "No history available.", "",
            MENU_SETTINGS_CORE_OPTION_NONE, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      char fill_buf[PATH_MAX_LENGTH];
      char path_copy[PATH_MAX_LENGTH];
      const char *core_name = NULL;
      const char *path      = NULL;
      
      strlcpy(path_copy, info->path, sizeof(path_copy));

      path = path_copy;

      content_playlist_get_index(g_defaults.history, i,
            &path, NULL, &core_name);
      strlcpy(fill_buf, core_name, sizeof(fill_buf));

      if (path)
      {
         char path_short[PATH_MAX_LENGTH];

         fill_short_pathname_representation(path_short, path,
               sizeof(path_short));
         snprintf(fill_buf,sizeof(fill_buf),"%s (%s)",
               path_short, core_name);
      }

      menu_list_push(info->list, fill_buf, "",
            MENU_FILE_PLAYLIST_ENTRY, 0);
   }

   return 0;
}

static int menu_displaylist_parse_cores(menu_displaylist_info_t *info)
{
   unsigned i;
   size_t list_size = 0;
   const core_info_t *core_info = NULL;
   global_t *global        = global_get_ptr();
   menu_handle_t *menu     = menu_driver_get_ptr();
   if (!menu)
      return -1;

   core_info_list_get_supported_cores(global->core_info,
         menu->deferred_path, &core_info, &list_size);

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            "No cores available.", "",
            0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      menu_list_push(info->list, core_info[i].path, "",
            MENU_FILE_CORE, 0);
      menu_list_set_alt_at_offset(info->list, i,
            core_info[i].display_name);
   }

   return 0;
}

int menu_displaylist_parse_horizontal_content_actions(menu_displaylist_info_t *info)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   global_t *global       = global_get_ptr();
   if (!menu)
      return -1;

   if (global->main_is_init && !global->libretro_dummy &&
         !strcmp(menu->deferred_path, global->fullpath))
   {
      menu_list_push(info->list, "Resume", "file_load_or_resume", MENU_SETTING_ACTION_RUN, 0);
      menu_list_push(info->list, "Save State", "savestate", MENU_SETTING_ACTION_SAVESTATE, 0);
      menu_list_push(info->list, "Load State", "loadstate", MENU_SETTING_ACTION_LOADSTATE, 0);
      menu_list_push(info->list, "Core Information", "core_information", MENU_SETTING_ACTION_CORE_INFORMATION, 0);
      menu_list_push(info->list, "Options", "options", MENU_SETTING_ACTION_CORE_OPTIONS, 0);
      menu_list_push(info->list, "Take Screenshot", "take_screenshot", MENU_SETTING_ACTION_SCREENSHOT, 0);
      menu_list_push(info->list, "Reset", "restart_content", MENU_SETTING_ACTION_RESET, 0);
   }
   else
      menu_list_push(info->list, "Run", "file_load_or_resume", MENU_SETTING_ACTION_RUN, 0);

   return 0;
}

static int menu_displaylist_parse_core_options(menu_displaylist_info_t *info)
{
   unsigned i;
   global_t *global       = global_get_ptr();

   if (global->system.core_options)
   {
      size_t opts = core_option_size(global->system.core_options);

      for (i = 0; i < opts; i++)
         menu_list_push(info->list,
               core_option_get_desc(global->system.core_options, i), "",
               MENU_SETTINGS_CORE_OPTION_START + i, 0);
   }
   else
      menu_list_push(info->list, "No options available.", "",
               MENU_SETTINGS_CORE_OPTION_NONE, 0);

   return 0;
}

#ifdef HAVE_SHADER_MANAGER
static int deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   size_t list_size = shader->num_parameters;

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            "No shader parameters.", "", 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      menu_list_push(info->list, shader->parameters[i].desc,
            info->label, base_parameter + i, 0);
   }

   return 0;
}
#endif

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info)
{
   unsigned i;
   settings_t *settings   = config_get_ptr();
   global_t *global       = global_get_ptr();
   core_info_t *core_info = (core_info_t*)global->core_info_current;

   if (core_info && core_info->data)
   {
      char tmp[PATH_MAX_LENGTH];

      snprintf(tmp, sizeof(tmp), "Core name: %s",
            core_info->core_name ? core_info->core_name : "");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(tmp, sizeof(tmp), "Core label: %s",
            core_info->display_name ? core_info->display_name : "");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      if (core_info->systemname)
      {
         snprintf(tmp, sizeof(tmp), "System name: %s",
               core_info->systemname);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->system_manufacturer)
      {
         snprintf(tmp, sizeof(tmp), "System manufacturer: %s",
               core_info->system_manufacturer);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->categories_list)
      {
         strlcpy(tmp, "Categories: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               core_info->categories_list, ", ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->authors_list)
      {
         strlcpy(tmp, "Authors: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               core_info->authors_list, ", ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->permissions_list)
      {
         strlcpy(tmp, "Permissions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               core_info->permissions_list, ", ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->licenses_list)
      {
         strlcpy(tmp, "License(s): ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               core_info->licenses_list, ", ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->supported_extensions_list)
      {
         strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               core_info->supported_extensions_list, ", ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (core_info->firmware_count > 0)
      {
         core_info_list_update_missing_firmware(
               global->core_info, core_info->path,
               settings->system_directory);

         menu_list_push(info->list, "Firmware: ", "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (core_info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "	name: %s",
                     core_info->firmware[i].desc ? 
                     core_info->firmware[i].desc : "");
               menu_list_push(info->list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);

               snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                     core_info->firmware[i].missing ?
                     "missing" : "present",
                     core_info->firmware[i].optional ?
                     "optional" : "required");
               menu_list_push(info->list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);
            }
         }
      }

      if (core_info->notes)
      {
         snprintf(tmp, sizeof(tmp), "Core notes: ");
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         for (i = 0; i < core_info->note_list->size; i++)
         {
            snprintf(tmp, sizeof(tmp), " %s",
                  core_info->note_list->elems[i].data);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
   }
   else
      menu_list_push(info->list,
            "No information available.", "",
            0, 0);

   return 0;
}

static int menu_displaylist_parse_all_settings(menu_displaylist_info_t *info)
{
   rarch_setting_t *setting = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   settings_t *settings     = config_get_ptr();

   settings_list_free(menu->list_settings);
   menu->list_settings = setting_new(SL_FLAG_ALL_SETTINGS);

   setting = menu_setting_find("Driver Settings");

   if (settings->menu.collapse_subgroups_enable)
   {
      for (; setting->type != ST_NONE; setting++)
      {
         if (setting->type == ST_GROUP)
            menu_list_push(info->list, setting->short_description,
                  setting->name, menu_setting_set_flags(setting), 0);
      }
   }
   else
   {
      for (; setting->type != ST_NONE; setting++)
      {
         char group_label[PATH_MAX_LENGTH];
         char subgroup_label[PATH_MAX_LENGTH];

         if (setting->type == ST_GROUP)
            strlcpy(group_label, setting->name, sizeof(group_label));
         else if (setting->type == ST_SUB_GROUP)
         {
            char new_label[PATH_MAX_LENGTH], new_path[PATH_MAX_LENGTH];
            strlcpy(subgroup_label, setting->name, sizeof(group_label));
            strlcpy(new_label, group_label, sizeof(new_label));
            strlcat(new_label, "|", sizeof(new_label));
            strlcat(new_label, subgroup_label, sizeof(new_label));

            strlcpy(new_path, group_label, sizeof(new_path));
            strlcat(new_path, " - ", sizeof(new_path));
            strlcat(new_path, setting->short_description, sizeof(new_path));

            menu_list_push(info->list, new_path,
                  new_label, MENU_SETTING_SUBGROUP, 0);
         }
      }
   }

   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   struct video_shader *shader = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   shader = menu->shader;

   if (!shader)
      return -1;

   menu_list_push(info->list, "Apply Shader Changes", "shader_apply_changes",
         MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Load Shader Preset", "video_shader_preset",
         MENU_FILE_PATH, 0);
   menu_list_push(info->list, "Shader Preset Save As",
         "video_shader_preset_save_as", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Parameters (Current)",
         "video_shader_parameters", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Parameters (Menu)",
         "video_shader_preset_parameters", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Shader Passes", "video_shader_num_passes",
         0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf[64];

      snprintf(buf, sizeof(buf), "Shader #%u", i);
      menu_list_push(info->list, buf, "video_shader_pass",
            MENU_SETTINGS_SHADER_PASS_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
      menu_list_push(info->list, buf, "video_shader_filter_pass",
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
      menu_list_push(info->list, buf, "video_shader_scale_pass",
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0);
   }

   return 0;
}

static int menu_displaylist_parse_disk_options(menu_displaylist_info_t *info)
{
   menu_list_push(info->list, "Disk Index", "disk_idx",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0);
   menu_list_push(info->list, "Disk Cycle Tray Status", "disk_cycle_tray_status",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0);
   menu_list_push(info->list, "Disk Image Append", "disk_image_append",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0);
   return 0;
}

static int menu_displaylist_parse_options(menu_displaylist_info_t *info)
{
   global_t *global            = global_get_ptr();

   menu_list_push(info->list, "Core Options", "core_options",
         MENU_SETTING_ACTION, 0);
   if (global->main_is_init)
   {
      if (global->has_set_input_descriptors)
         menu_list_push(info->list, "Core Input Remapping Options", "core_input_remapping_options",
               MENU_SETTING_ACTION, 0);
      menu_list_push(info->list, "Core Cheat Options", "core_cheat_options",
            MENU_SETTING_ACTION, 0);
      if (!global->libretro_dummy && global->system.disk_control.get_num_images)
         menu_list_push(info->list, "Core Disk Options", "disk_options",
               MENU_SETTING_ACTION, 0);
   }
   menu_list_push(info->list, "Video Options", "video_options",
         MENU_SETTING_ACTION, 0);
#ifdef HAVE_SHADER_MANAGER
   menu_list_push(info->list, "Shader Options", "shader_options",
         MENU_SETTING_ACTION, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_options_management(menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   menu_list_push(info->list, "Database Manager", "database_manager_list",
         MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Cursor Manager", "cursor_manager_list",
         MENU_SETTING_ACTION, 0);
#endif
   return 0;
}

static int menu_displaylist_parse_settings_in_subgroup(menu_displaylist_info_t *info)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   if (info->label)
   {
      str_list = string_split(info->label, "|");

      if (str_list && str_list->size > 0)
         strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
      if (str_list && str_list->size > 1)
         strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

      if (str_list)
      {
         string_list_free(str_list);
         str_list = NULL;
      }
   }

   settings_list_free(menu->list_settings);
   menu->list_settings = setting_new(SL_FLAG_ALL_SETTINGS);

   info->setting = menu_setting_find(elem0);

   if (!info->setting)
      return -1;

   while (1)
   {
      if (!info->setting)
         return -1;
      if (info->setting->type == ST_SUB_GROUP)
      {
         if ((strlen(info->setting->name) != 0) && !strcmp(info->setting->name, elem1))
            break;
      }
      info->setting++;
   }

   info->setting++;

   for (; info->setting->type != ST_END_SUB_GROUP; info->setting++)
   {
      char group_label[PATH_MAX_LENGTH];

      strlcpy(group_label, info->setting->name, sizeof(group_label));
      menu_list_push(info->list, info->setting->short_description,
            group_label, menu_setting_set_flags(info->setting), 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_cheats(menu_displaylist_info_t *info)
{
   unsigned i;
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global ? global->cheat : NULL;

   if (!cheat)
   {
      global->cheat = cheat_manager_new(0);

      if (!global->cheat)
         return -1;
      cheat = global->cheat;
   }

   menu_list_push(info->list, "Cheat File Load", "cheat_file_load",
         MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Cheat File Save As",
         "cheat_file_save_as", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Cheat Passes", "cheat_num_passes",
         0, 0);
   menu_list_push(info->list, "Apply Cheat Changes", "cheat_apply_changes",
         MENU_SETTING_ACTION, 0);

   for (i = 0; i < cheat->size; i++)
   {
      char cheat_label[64];
      snprintf(cheat_label, sizeof(cheat_label), "Cheat #%u: ", i);
      if (cheat->cheats[i].desc)
         strlcat(cheat_label, cheat->cheats[i].desc, sizeof(cheat_label));
      menu_list_push(info->list, cheat_label, "", MENU_SETTINGS_CHEAT_BEGIN + i, 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_remappings(menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   settings_t *settings   = config_get_ptr();
   global_t *global       = global_get_ptr();

   menu_list_push(info->list, "Load Remap File", "remap_file_load",
         MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Save Remap File As",
         "remap_file_save_as", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Save Core Remap File",
         "remap_file_save_core", MENU_SETTING_ACTION, 0);
   menu_list_push(info->list, "Save Game Remap File",
         "remap_file_save_game", MENU_SETTING_ACTION, 0);         

   for (p = 0; p < settings->input.max_users; p++)
   {
      for (retro_id = 0; retro_id < RARCH_FIRST_META_KEY; retro_id++)
      {
         char desc_label[64];
         unsigned user = p + 1;
         const char *description = global->system.input_desc_btn[p][retro_id];

         if (!description)
            continue;

         snprintf(desc_label, sizeof(desc_label),
               "User %u %s : ", user, description);
         menu_list_push(info->list, desc_label, "",
               MENU_SETTINGS_INPUT_DESC_BEGIN + 
               (p * RARCH_FIRST_META_KEY) +  retro_id, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   {
      char tmp[PATH_MAX_LENGTH];
      char tmp2[PATH_MAX_LENGTH];
      const char *tmp_string = NULL;
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();

      snprintf(tmp, sizeof(tmp), "Build date: %s", __DATE__);
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      (void)tmp_string;

#ifdef HAVE_GIT_VERSION
      snprintf(tmp, sizeof(tmp), "Git version: %s", rarch_git_version);
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
#endif

      rarch_info_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
      menu_list_push(info->list, tmp, "", MENU_SETTINGS_CORE_INFO_NONE, 0);

      {
         char cpu_str[PATH_MAX_LENGTH];

         snprintf(cpu_str, sizeof(cpu_str), "CPU Features: ");

         rarch_info_get_capabilities(RARCH_CAPABILITIES_CPU, cpu_str, sizeof(cpu_str));
         menu_list_push(info->list, cpu_str, "", MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (frontend)
      {
         int major = 0, minor = 0;

         snprintf(tmp, sizeof(tmp), "Frontend identifier: %s",
               frontend->ident);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         if (frontend->get_name)
         {
            frontend->get_name(tmp2, sizeof(tmp2));
            snprintf(tmp, sizeof(tmp), "Frontend name: %s",
                  frontend->get_name ? tmp2 : "N/A");
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (frontend->get_os)
         {
            frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
            snprintf(tmp, sizeof(tmp), "Frontend OS: %s %d.%d",
                  frontend->get_os ? tmp2 : "N/A", major, minor);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         snprintf(tmp, sizeof(tmp), "RetroRating level: %d", 
               frontend->get_rating ? frontend->get_rating() : -1);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         if (frontend->get_powerstate)
         {
            int seconds = 0, percent = 0;
            enum frontend_powerstate state = frontend->get_powerstate(&seconds, &percent);

            tmp2[0] = '\0';

            if (percent != 0)
               snprintf(tmp2, sizeof(tmp2), "%d%%", percent);

            switch (state)
            {
               case FRONTEND_POWERSTATE_NONE:
                  strlcat(tmp2, " N/A", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_NO_SOURCE:
                  strlcat(tmp2, " (No source)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_CHARGING:
                  strlcat(tmp2, " (Charging)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_CHARGED:
                  strlcat(tmp2, " (Charged)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
                  strlcat(tmp2, " (Discharging)", sizeof(tmp));
                  break;
            }

            snprintf(tmp, sizeof(tmp), "Power source : %s",
                  tmp2);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }

#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
      tmp_string = gfx_ctx_get_ident();

      snprintf(tmp, sizeof(tmp), "Video context driver: %s",
            tmp_string ? tmp_string : "N/A");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      {
         float val = 0.0f;
         if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_WIDTH, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric width (mm): %.2f", 
                  val);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_HEIGHT, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric height (mm): %.2f", 
                  val);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric DPI: %.2f", 
                  val);
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
#endif

   }

   {
      char feat_str[PATH_MAX_LENGTH];

      snprintf(feat_str, sizeof(feat_str),
            "LibretroDB support: %s", _libretrodb_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Overlay support: %s", _overlay_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Command interface support: %s", _command_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Network Command interface support: %s", _network_command_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Cocoa support: %s", _cocoa_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "PNG support (RPNG): %s", _rpng_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "SDL1.2 support: %s", _sdl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "SDL2 support: %s", _sdl2_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);


      snprintf(feat_str, sizeof(feat_str),
            "OpenGL support: %s", _opengl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenGL ES support: %s", _opengles_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Threading support: %s", _thread_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "KMS/EGL support: %s", _kms_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Udev support: %s", _udev_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenVG support: %s", _vg_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "EGL support: %s", _egl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "X11 support: %s", _x11_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Wayland support: %s", _wayland_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "XVideo support: %s", _xvideo_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "ALSA support: %s", _alsa_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OSS support: %s", _oss_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenAL support: %s", _al_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenSL support: %s", _sl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "RSound support: %s", _rsound_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "RoarAudio support: %s", _roar_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "JACK support: %s", _jack_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "PulseAudio support: %s", _pulse_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "DirectSound support: %s", _dsound_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "XAudio2 support: %s", _xaudio_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Zlib support: %s", _zlib_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "7zip support: %s", _7zip_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Dynamic library support: %s", _dylib_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Cg support: %s", _cg_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "GLSL support: %s", _glsl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "HLSL support: %s", _hlsl_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "libxml2 XML parsing support: %s", _libxml2_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "SDL image support: %s", _sdl_image_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenGL/Direct3D render-to-texture (multi-pass shaders) support: %s", _fbo_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "FFmpeg support: %s", _ffmpeg_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "CoreText support: %s", _coretext_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "FreeType support: %s", _freetype_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Netplay (peer-to-peer) support: %s", _netplay_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Python (script support in shaders) support: %s", _python_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Video4Linux2 support: %s", _v4l2_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Libusb support: %s", _libusb_supp ? "true" : "false");
      menu_list_push(info->list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
   }

   return 0;
}

#ifdef HAVE_LIBRETRODB
static int create_string_list_rdb_entry_string(const char *desc, const char *label,
      const char *actual_string, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   str_len += strlen(actual_string) + 1;
   string_list_append(str_list, actual_string, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %s", desc, actual_string);
   menu_list_push(list, tmp, output_label, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int create_string_list_rdb_entry_int(const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char str[PATH_MAX_LENGTH];
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   snprintf(str, sizeof(str), "%d", actual_int);
   str_len += strlen(str) + 1;
   string_list_append(str_list, str, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %d", desc, actual_int);
   menu_list_push(list, tmp, output_label,
         0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}
#endif

static int menu_displaylist_parse_database_entry(menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   char query[PATH_MAX_LENGTH];
   content_playlist_t *playlist;
   database_info_list_t *db_info = NULL;
   char path_rdl[PATH_MAX_LENGTH], path_base[PATH_MAX_LENGTH];
   unsigned i, j;
   menu_handle_t *menu           = menu_driver_get_ptr();
   settings_t *settings          = config_get_ptr();
   if (!menu)
      return -1;

   strlcpy(query, "{'name':\"", sizeof(query));
   strlcat(query, info->path_b, sizeof(query));
   strlcat(query, "\"}", sizeof(query));

   if (!(db_info = database_info_list_new(info->path, query)))
      return -1;

   strlcpy(path_base, path_basename(info->path), sizeof(path_base));
   path_remove_extension(path_base);
   strlcat(path_base, ".rdl", sizeof(path_base));

   fill_pathname_join(path_rdl, settings->content_database, path_base,
         sizeof(path_rdl));

   menu_database_realloc(path_rdl, false);

   playlist = menu->db_playlist;

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH];
      database_info_t *db_info_entry = &db_info->list[i];

      if (!db_info_entry)
         continue;


      if (db_info_entry->name)
      {
         snprintf(tmp, sizeof(tmp), "Name: %s", db_info_entry->name);
         menu_list_push(info->list, tmp, "rdb_entry_name",
               0, 0);
      }
      if (db_info_entry->description)
      {
         snprintf(tmp, sizeof(tmp), "Description: %s", db_info_entry->description);
         menu_list_push(info->list, tmp, "rdb_entry_description",
               0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string("Publisher", "rdb_entry_publisher",
               db_info_entry->publisher, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->developer)
      {
         if (create_string_list_rdb_entry_string("Developer", "rdb_entry_developer",
               db_info_entry->developer, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string("Origin", "rdb_entry_origin",
               db_info_entry->origin, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string("Franchise", "rdb_entry_franchise",
               db_info_entry->franchise, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int("Max Users",
               "rdb_entry_max_users", db_info_entry->max_users,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Famitsu Magazine Rating",
               "rdb_entry_famitsu_magazine_rating", db_info_entry->famitsu_magazine_rating,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_review)
      {
         if (create_string_list_rdb_entry_string("Edge Magazine Review", "rdb_entry_edge_magazine_review",
               db_info_entry->edge_magazine_review, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Rating",
               "rdb_entry_edge_magazine_rating", db_info_entry->edge_magazine_rating,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Issue",
               "rdb_entry_edge_magazine_issue", db_info_entry->edge_magazine_issue,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int("Releasedate Month",
               "rdb_entry_releasemonth", db_info_entry->releasemonth,
               info->path, info->list) == -1)
            return -1;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int("Releasedate Year",
               "rdb_entry_releaseyear", db_info_entry->releaseyear,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string("BBFC Rating", "rdb_entry_bbfc_rating",
               db_info_entry->bbfc_rating, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string("ESRB Rating", "rdb_entry_esrb_rating",
               db_info_entry->esrb_rating, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string("ELSPA Rating", "rdb_entry_elspa_rating",
               db_info_entry->elspa_rating, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string("PEGI Rating", "rdb_entry_pegi_rating",
               db_info_entry->pegi_rating, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string("Enhancement Hardware", "rdb_entry_enhancement_hw",
               db_info_entry->enhancement_hw, info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string("CERO Rating", "rdb_entry_cero_rating",
               db_info_entry->cero_rating, info->path, info->list) == -1)
            return -1;
      }
      snprintf(tmp, sizeof(tmp),
            "Analog supported: %s",
            (db_info_entry->analog_supported == 1)  ? "true" : 
            (db_info_entry->analog_supported == -1) ? "N/A"  : "false");
      menu_list_push(info->list, tmp, "rdb_entry_analog",
            0, 0);
      snprintf(tmp, sizeof(tmp),
            "Rumble supported: %s",
            (db_info_entry->rumble_supported == 1)  ? "true" : 
            (db_info_entry->rumble_supported == -1) ? "N/A"  :  "false");
      menu_list_push(info->list, tmp, "rdb_entry_rumble",
            0, 0);

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string("CRC32 Checksum",
               "rdb_entry_crc32", db_info_entry->crc32,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string("SHA1 Checksum",
               "rdb_entry_sha1", db_info_entry->sha1,
               info->path, info->list) == -1)
            return -1;
      }
      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string("MD5 Checksum",
               "rdb_entry_md5", db_info_entry->md5,
               info->path, info->list) == -1)
            return -1;
      }

      if (playlist)
      {
         for (j = 0; j < playlist->size; j++)
         {
            char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
            bool match_found = false;
            struct string_list *tmp_str_list = string_split(
                  playlist->entries[j].core_name, "|"); 

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
               strlcpy(elem0, tmp_str_list->elems[0].data, sizeof(elem0));
            if (tmp_str_list->size > 1)
               strlcpy(elem1, tmp_str_list->elems[1].data, sizeof(elem1));

            if (!strcmp(elem1, "crc"))
            {
               if (!strcmp(db_info_entry->crc32, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "sha1"))
            {
               if (!strcmp(db_info_entry->sha1, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "md5"))
            {
               if (!strcmp(db_info_entry->md5, elem0))
                  match_found = true;
            }

            string_list_free(tmp_str_list);

            if (!match_found)
               continue;

            rdb_entry_start_game_selection_ptr = j;
            menu_list_push(info->list, "Start Content", "rdb_entry_start_game",
                  MENU_FILE_PLAYLIST_ENTRY, 0);
         }
      }
   }

   if (db_info->count < 1)
      menu_list_push(info->list,
            "No information available.", "",
            0, 0);
#endif

   return 0;
}

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type)
{
   int ret = 0;
   bool need_sort    = false;
   bool need_refresh = false;
   bool need_push    = false;
   menu_handle_t    *menu = menu_driver_get_ptr();


   switch (type)
   {
      case DISPLAYLIST_NONE:
         break;
      case DISPLAYLIST_SELECT:
         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr);
         break;
      case DISPLAYLIST_MAIN_MENU:
      case DISPLAYLIST_SETTINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_settings(menu, info, info->flags);
         need_push    = true;
         break;
      case DISPLAYLIST_GENERIC:
         menu_driver_list_cache(false, 0);

         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr);
         menu_navigation_clear(&menu->navigation, true);
         menu_set_refresh();
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_all_settings(info);

         need_push    = true;
         break;
      case DISPLAYLIST_SETTINGS_SUBGROUP:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_settings_in_subgroup(info);

         need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_horizontal_list(info);

         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_horizontal_content_actions(info);
         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_cheats(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_MANAGEMENT:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_management(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_remappings(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_VIDEO:
         menu_list_clear(info->list);
#if defined(GEKKO) || defined(__CELLOS_LV2__)
         menu_list_push(info->list, "Screen Resolution", "",
               MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
         menu_list_push(info->list, "Custom Ratio", "",
               MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
#ifndef HAVE_FILTERS_BUILTIN
         menu_list_push(info->list, "Video Filter", "video_filter",
               0, 0);
#endif

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_disk_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_shader_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_DEFAULT:
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_DATABASES:
      case DISPLAYLIST_DATABASE_CURSORS:
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_IMAGES:
      case DISPLAYLIST_OVERLAYS:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_RECORD_CONFIG_FILES:
      case DISPLAYLIST_CONFIG_FILES:
      case DISPLAYLIST_CONTENT_HISTORY:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse(info, &need_sort);
         if (ret == 0)
         {
            need_refresh = true;
            need_push    = true;
         }
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_core_options(info);

         need_push = true;
         break;
      case DISPLAYLIST_CORE_INFO:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_core_info(info);

         need_push = true;
         break;
      case DISPLAYLIST_CORES_ALL:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_cores(info);

         need_sort    = true;
         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_system_info(info);

         need_push    = true;
         break;
      case DISPLAYLIST_HISTORY:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_historylist(info);

         if (ret == 0)
         {
            need_refresh = true;
            need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASE_QUERY:
         menu_list_clear(info->list);
         menu_database_populate_query(info->list, info->path, (info->path_c[0] == '\0') ? NULL : info->path_c);

         need_sort    = true;
         need_refresh = true;
         need_push    = true;
         strlcpy(info->path, info->path_b, sizeof(info->path));
         break;
      case DISPLAYLIST_DATABASE_ENTRY:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_database_entry(info);

         need_push    = true;
         break;
      case DISPLAYLIST_PERFCOUNTER_SELECTION:
         menu_list_clear(info->list);
         menu_list_push(info->list, "Frontend Counters", "frontend_counters",
               MENU_SETTING_ACTION, 0);
         menu_list_push(info->list, "Core Counters", "core_counters",
               MENU_SETTING_ACTION, 0);

         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         menu_list_clear(info->list);
         ret = menu_displaylist_push_perfcounter_generic(info,
              (type == DISPLAYLIST_PERFCOUNTERS_CORE) ? 
              perf_counters_libretro : perf_counters_rarch,
              (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
              perf_ptr_libretro : perf_ptr_rarch , 
              (type == DISPLAYLIST_PERFCOUNTERS_CORE) ? 
              MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN : MENU_SETTINGS_PERF_COUNTERS_BEGIN);

         need_refresh = false;
         need_push    = true;
         break;
      case DISPLAYLIST_CORES_UPDATER:
         menu_list_clear(info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf, core_len, MENU_FILE_DOWNLOAD_CORE);
         need_push    = true;
         need_refresh = true;
#endif
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
         menu_list_clear(info->list);
#ifdef HAVE_SHADER_MANAGER
         {
            struct video_shader *shader = video_shader_driver_get_current_shader();
            if (!shader)
               return 0;

            ret = deferred_push_video_shader_parameters_common(info, shader,
                  (type == DISPLAYLIST_SHADER_PARAMETERS) 
                  ? MENU_SETTINGS_SHADER_PARAMETER_0 : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
                  );

            need_push = true;
         }
#endif
         break;
   }


   if (need_sort)
      file_list_sort_on_alt(info->list);

   if (need_push)
   {
      driver_t *driver                = driver_get_ptr();
      const ui_companion_driver_t *ui = ui_companion_get_ptr();

      menu_list_populate_generic(info, need_refresh);

      if (ui && driver)
         ui->notify_list_loaded(driver->ui_companion_data,
               info->list, info->menu_list);
   }

   return ret;
}

static int menu_displaylist_deferred_push(menu_displaylist_info_t *info)
{
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t       *menu = menu_driver_get_ptr();

   if (!info->list)
      return -1;

   if (!strcmp(info->label, "Main Menu"))
   {
      info->flags = SL_FLAG_MAIN_MENU;
      return menu_displaylist_push_list(info, DISPLAYLIST_MAIN_MENU);
   }
   else if (!strcmp(info->label, "Horizontal Menu"))
   {
      return menu_displaylist_push_list(info, DISPLAYLIST_HORIZONTAL);
   }

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(menu->menu_list);

   if (cbs->action_deferred_push)
      return cbs->action_deferred_push(info);

   return 0;
}

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list)
{
   unsigned type                = 0;
   const char *path             = NULL;
   const char *label            = NULL;
   menu_handle_t *menu          = menu_driver_get_ptr();
   menu_displaylist_info_t info = {0};

   menu_list_get_last_stack(menu->menu_list, &path, &label, &type);

   info.list      = list;
   info.menu_list = menu_list;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));
   
   return menu_displaylist_deferred_push(&info);
}

/**
 * menu_displaylist_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu display list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_displaylist_init(void *data)
{
   menu_handle_t *menu    = (menu_handle_t*)data;
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_displaylist_info_t info = {0};
   if (!menu || !menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU;
   strlcpy(info.label, "Main Menu", sizeof(info.label));

   menu_list_push(menu_list->menu_stack,
         info.path, info.label, info.type, info.flags);
   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);
   menu_navigation_clear(nav, true);

   return true;
}
