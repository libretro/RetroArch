/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <lists/file_list.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <retro_stat.h>
#include <string/stdstring.h>

#include "menu_driver.h"
#include "menu_navigation.h"
#include "menu_hash.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../defaults.h"
#include "../cheats.h"
#include "../general.h"
#include "../retroarch.h"
#include "../system.h"
#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_shader_driver.h"
#include "../config.features.h"
#include "../git_version.h"
#include "../input/input_config.h"
#include "../list_special.h"
#include "../performance.h"
#include "../core_info.h"

#ifdef HAVE_CHEEVOS
#include "../cheevos.h"
#endif

#ifdef __linux__
#include "../frontend/drivers/platform_linux.h"
#endif

extern unsigned rpl_entry_selection_ptr;

#ifdef HAVE_NETWORKING
extern char *core_buf;
extern size_t core_len;

static void print_buf_lines(file_list_t *list, char *buf, int buf_size,
      unsigned type)
{
   char c;
   int i, j = 0;
   char *line_start = buf;

   if (!buf || !buf_size)
   {
      menu_entries_push(list, "No entries to display.", "",
            MENU_FILE_NONE, 0, 0);
      return;
   }

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

      menu_entries_push(list, line_start, "",
            type, 0, 0);
      if (type == MENU_FILE_DOWNLOAD_CORE)
      {
         settings_t *settings      = config_get_ptr();

         if (settings)
         {
            char core_path[PATH_MAX_LENGTH];
            char display_name[PATH_MAX_LENGTH];
            char *last = NULL;

            fill_pathname_join(core_path, settings->libretro_info_path,
                  line_start, sizeof(core_path));

            path_remove_extension(core_path);
            path_remove_extension(core_path);
            last = (char*)strrchr(core_path, '_');
            if (*last)
            {
               if (!string_is_equal(last, "_libretro"))
                  *last = '\0';
            }
            strlcat(core_path, ".info", sizeof(core_path));

            if (core_info_get_display_name(
                     core_path, display_name, sizeof(display_name)))
               menu_entries_set_alt_at_offset(list, j, display_name);
         }
      }
      j++;

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start     = buf + i + 1;
   }
   file_list_sort_on_alt(list);
   /* If the buffer was completely full, and didn't end
    * with a newline, just ignore the partial last line. */
}
#endif

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
   {
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS),
            "", 0, 0, 0);
      return;
   }

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_entries_push(info->list,
               counters[i]->ident, "", id + i, 0, 0);
}

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info)
{
   unsigned i;
   char tmp[PATH_MAX_LENGTH];
   settings_t *settings      = config_get_ptr();
   core_info_t *core_info    = NULL;

   core_info_ctl(CORE_INFO_CTL_CURRENT_CORE_GET, &core_info);

   if (!core_info || !core_info->config_data)
   {
      menu_entries_push(info->list,
            menu_hash_to_str(
               MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            "", 0, 0, 0);
      return 0;
   }

   strlcpy(tmp,
         menu_hash_to_str(
            MENU_LABEL_VALUE_CORE_INFO_CORE_NAME), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->core_name)
      strlcat(tmp, core_info->core_name, sizeof(tmp));

   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(tmp,
         menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->display_name)
      strlcat(tmp, core_info->display_name, sizeof(tmp));
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   if (core_info->systemname)
   {
      strlcpy(tmp,
            menu_hash_to_str(
               MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, core_info->systemname, sizeof(tmp));
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->system_manufacturer)
   {
      strlcpy(tmp,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, core_info->system_manufacturer, sizeof(tmp));
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->categories_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CATEGORIES),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->categories_list, ", ");
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->authors_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_AUTHORS),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->authors_list, ", ");
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->permissions_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->permissions_list, ", ");
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->licenses_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_LICENSES),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->licenses_list, ", ");
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->supported_extensions_list)
   {
      strlcpy(tmp,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->supported_extensions_list, ", ");
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->firmware_count > 0)
   {
      core_info_ctx_firmware_t firmware_info;

      firmware_info.path             = core_info->path;
      firmware_info.system_directory = settings->system_directory;

      if (core_info_ctl(CORE_INFO_CTL_LIST_UPDATE_MISSING_FIRMWARE, &firmware_info))
      {
         strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_FIRMWARE),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

         /* FIXME: This looks hacky and probably
          * needs to be improved for good translation support. */

         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (core_info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "	%s: %s",
                     menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_NAME),
                     core_info->firmware[i].desc ?
                     core_info->firmware[i].desc : "");
               menu_entries_push(info->list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

               snprintf(tmp, sizeof(tmp), "	%s: %s, %s",
                     menu_hash_to_str(MENU_VALUE_STATUS),
                     core_info->firmware[i].missing ?
                     menu_hash_to_str(MENU_VALUE_MISSING) :
                     menu_hash_to_str(MENU_VALUE_PRESENT),
                     core_info->firmware[i].optional ?
                     menu_hash_to_str(MENU_VALUE_OPTIONAL) :
                     menu_hash_to_str(MENU_VALUE_REQUIRED)
                     );
               menu_entries_push(info->list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
            }
         }
      }
   }

   if (core_info->notes)
   {
      strlcpy(tmp,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      for (i = 0; i < core_info->note_list->size; i++)
      {
         strlcpy(tmp,
               core_info->note_list->elems[i].data, sizeof(tmp));
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_debug_info(menu_displaylist_info_t *info)
{
   char tmp[PATH_MAX_LENGTH];

   settings_t *settings                = config_get_ptr();
   global_t *global                    = global_get_ptr();

   bool ret;

   menu_entries_push(info->list, "Directory Tests:", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Assume libretro directory exists and check if stat works */
   ret = path_is_directory(settings->libretro_directory);
   snprintf(tmp, sizeof(tmp), "- stat directory... %s",
         ret ? "passed" : "failed");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Try to create a "test" subdirectory on top of libretro directory */
   fill_pathname_join(tmp,
         settings->libretro_directory, ".retroarch", sizeof(tmp));
   ret = path_mkdir(tmp);
   snprintf(tmp, sizeof(tmp), "- create a directory... %s",
         ret ? "passed" : "failed");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   menu_entries_push(info->list, "", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if save directory exists */
   menu_entries_push(info->list, "Savefile Directory", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   ret = path_is_directory(global->dir.savefile);
   snprintf(tmp, sizeof(tmp), "- directory name: %s",
         global->dir.savefile);
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   snprintf(tmp, sizeof(tmp), "- directory exists: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if save directory is writable */
   fill_pathname_join(tmp, global->dir.savefile, ".retroarch",
         sizeof(tmp));
   ret = path_mkdir(tmp);
   snprintf(tmp, sizeof(tmp), "- directory writable: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   menu_entries_push(info->list, "", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if state directory exists */
   menu_entries_push(info->list, "Savestate Directory", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   ret = path_is_directory(global->dir.savestate);
   snprintf(tmp, sizeof(tmp), "- directory name: %s", global->dir.savestate);
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   snprintf(tmp, sizeof(tmp), "- directory exists: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if save directory is writable */
   fill_pathname_join(tmp, global->dir.savestate, ".retroarch", sizeof(tmp));
   ret = path_mkdir(tmp);
   snprintf(tmp, sizeof(tmp), "- directory writable: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   menu_entries_push(info->list, "", "",
      MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if system directory exists */
   menu_entries_push(info->list, "System Directory", "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   ret = path_is_directory(settings->system_directory);
   snprintf(tmp, sizeof(tmp), "- directory name: %s",
         settings->system_directory);
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   snprintf(tmp, sizeof(tmp), "- directory exists: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   /* Check if save directory is writable */
   fill_pathname_join(tmp, settings->system_directory, ".retroarch",
         sizeof(tmp));
   ret = path_mkdir(tmp);
   snprintf(tmp, sizeof(tmp), "- directory writable: %s",
         ret ? "true" : "false");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   return 0;
}

#ifdef HAVE_NETPLAY
#ifndef HAVE_SOCKET_LEGACY
#include <net/net_ifinfo.h>

static int menu_displaylist_parse_network_info(menu_displaylist_info_t *info)
{
   unsigned k              = 0;
   net_ifinfo_t *list = 
      (net_ifinfo_t*)calloc(1, sizeof(*list));

   if (!list)
      return -1;

   if (!net_ifinfo_new(list))
      return -1;

   for (k = 0; k < list->size; k++)
   {
      char tmp[PATH_MAX_LENGTH];
      snprintf(tmp, sizeof(tmp), "Interface (%s) : %s\n",
            list->entries[k].name, list->entries[k].host);
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   net_ifinfo_free(list);
   return 0;
}
#endif
#endif

static int menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   int controller;
#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
   gfx_ctx_ident_t ident_info;
#endif
   char tmp[PATH_MAX_LENGTH], feat_str[PATH_MAX_LENGTH];
   const char *tmp_string                = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();
   settings_t                  *settings = config_get_ptr();

   snprintf(tmp, sizeof(tmp), "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE), __DATE__);
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   (void)tmp_string;

#ifdef HAVE_GIT_VERSION
   strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION),
         sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, rarch_git_version, sizeof(tmp));
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
#endif

   rarch_info_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   menu_entries_push(info->list, tmp, "", MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#ifdef ANDROID
   bool perms = test_permissions(sdcard_dir);

   snprintf(tmp, sizeof(tmp), "%s: %s", "Internal SD card status",
         perms ? "read-write" : "read-only");
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#endif
   {
      char cpu_str[PATH_MAX_LENGTH];

      strlcpy(cpu_str,
            menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES),
            sizeof(cpu_str));
      strlcat(cpu_str, ": ", sizeof(cpu_str));

      rarch_info_get_capabilities(RARCH_CAPABILITIES_CPU,
            cpu_str, sizeof(cpu_str));
      menu_entries_push(info->list, cpu_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   for(controller = 0; controller < MAX_USERS; controller++)
   {
       if (settings->input.autoconfigured[controller])
       {
           snprintf(tmp, sizeof(tmp), "Port #%d device name: %s (#%d)",
                 controller, settings->input.device_names[controller],
                 settings->input.device_name_index[controller]);
           menu_entries_push(info->list, tmp, "",
                 MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
           snprintf(tmp, sizeof(tmp), "Port #%d device VID/PID: %d/%d",
                 controller, settings->input.vid[controller],
                 settings->input.pid[controller]);
           menu_entries_push(info->list, tmp, "",
                 MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
       }
   }

   if (frontend)
   {
      char tmp2[PATH_MAX_LENGTH];
      int major = 0, minor = 0;

      strlcpy(tmp,
            menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER),
            sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, frontend->ident, sizeof(tmp));
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, frontend->get_name ?
               tmp2 : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), sizeof(tmp));
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s : %s %d.%d",
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS),
               frontend->get_os
               ? tmp2 : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE),
               major, minor);
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      snprintf(tmp, sizeof(tmp), "%s : %d",
            menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
            frontend->get_rating ? frontend->get_rating() : -1);
      menu_entries_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      if (frontend->get_powerstate)
      {
         int seconds = 0, percent = 0;
         enum frontend_powerstate state =
            frontend->get_powerstate(&seconds, &percent);

         tmp2[0] = '\0';

         if (percent != 0)
            snprintf(tmp2, sizeof(tmp2), "%d%%", percent);

         switch (state)
         {
            case FRONTEND_POWERSTATE_NONE:
               strlcat(tmp2, " ", sizeof(tmp));
               strlcat(tmp2,
                     menu_hash_to_str(
                        MENU_VALUE_NOT_AVAILABLE), sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_NO_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     menu_hash_to_str(
                        MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGING:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     menu_hash_to_str(
                        MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGED:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, 
                     menu_hash_to_str(
                        MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, 
                     menu_hash_to_str(
                        MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
         }

         strlcpy(tmp,
               menu_hash_to_str(
                  MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, tmp2, sizeof(tmp));
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
   gfx_ctx_ctl(GFX_CTL_IDENT_GET, &ident_info);
   tmp_string = ident_info.ident;

   strlcpy(tmp,
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER),
         sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, tmp_string ? tmp_string 
         : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE),
         sizeof(tmp));
   menu_entries_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   {
      gfx_ctx_metrics_t metrics;
      float val = 0.0f;

      metrics.type  = DISPLAY_METRIC_MM_WIDTH;
      metrics.value = &val; 

      if (gfx_ctx_ctl(GFX_CTL_GET_METRICS, &metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(
                  MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH),
               val);
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      metrics.type  = DISPLAY_METRIC_MM_HEIGHT;

      if (gfx_ctx_ctl(GFX_CTL_GET_METRICS, &metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(
                  MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT),
               val);
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      metrics.type  = DISPLAY_METRIC_DPI;

      if (gfx_ctx_ctl(GFX_CTL_GET_METRICS, &metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(
                  MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI),
               val);
         menu_entries_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }
#endif

   strlcpy(feat_str,
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT),
         sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str,
         _libretrodb_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str,
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT),
         sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _overlay_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str,
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT),
         sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _command_supp 
         ? menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s : %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT),
         _network_command_supp 
         ? menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s : %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT),
         _network_gamepad_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT),
          _cocoa_supp ? 
          menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
          menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT),
         _rpng_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT),
         _sdl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT),
         _sdl2_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT),
         _vulkan_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT),
         _opengl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT),
         _opengles_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT),
         _thread_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT),
         _kms_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT),
         _udev_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT),
         _vg_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT),
         _egl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT),
         _x11_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT),
         _wayland_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT),
         _xvideo_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT),
         _alsa_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT),
         _oss_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT),
         _al_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT),
         _sl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT),
         _rsound_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT),
         _roar_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT),
         _jack_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT),
         _pulse_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT),
         _dsound_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT),
         _xaudio_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT),
         _zlib_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT),
         _7zip_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT),
         _dylib_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT),
         _dynamic_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT),
         _cg_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT),
         _glsl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT),
         _hlsl_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT),
         _libxml2_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT),
         _sdl_image_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT),
         _fbo_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT),
         _ffmpeg_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT),
         _coretext_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT),
         _freetype_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) : 
         menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT),
         _netplay_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT),
         _python_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT),
         _v4l2_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(
            MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT),
         _libusb_supp ? 
         menu_hash_to_str(MENU_LABEL_VALUE_YES) 
         : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_entries_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   return 0;
}

static int menu_displaylist_parse_playlist(menu_displaylist_info_t *info,
      content_playlist_t *playlist, const char *path_playlist, bool is_history)
{
   unsigned i;
   size_t list_size = 0;

   if (!playlist)
      return -1;

   list_size = content_playlist_size(playlist);

   if (list_size == 0)
   {
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            menu_hash_to_str(MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            MENU_INFO_MESSAGE, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      uint32_t core_name_hash;
      char fill_buf[PATH_MAX_LENGTH], path_copy[PATH_MAX_LENGTH];
      bool core_detected              = false;
      const char *core_name           = NULL;
      const char *db_name             = NULL;
      const char *path                = NULL;
      const char *label               = NULL;
      const char *crc32               = NULL;

      (void)core_detected;

      strlcpy(path_copy, info->path, sizeof(path_copy));

      path = path_copy;

      content_playlist_get_index(playlist, i,
            &path, &label, NULL, &core_name, &crc32, &db_name);
      strlcpy(fill_buf, core_name, sizeof(fill_buf));

      core_name_hash = core_name ? menu_hash_calculate(core_name) : 0;

      if (path)
      {
         char path_short[PATH_MAX_LENGTH] = {0};

         fill_short_pathname_representation(path_short, path,
               sizeof(path_short));
         strlcpy(fill_buf,
               (!string_is_empty(label)) ? label : path_short,
               sizeof(fill_buf));

         if (!string_is_empty(core_name))
         {
            if (core_name_hash != MENU_VALUE_DETECT)
            {
               char tmp[PATH_MAX_LENGTH] = {0};
               snprintf(tmp, sizeof(tmp), " (%s)", core_name);
               strlcat(fill_buf, tmp, sizeof(fill_buf));
               core_detected = true;
            }
         }
      }

      if (!path)
         menu_entries_push(info->list, fill_buf, path_playlist,
               MENU_FILE_PLAYLIST_ENTRY, 0, i);
      else if (is_history)
         menu_entries_push(info->list, fill_buf,
               path, MENU_FILE_RPL_ENTRY, 0, i);
      else
         menu_entries_push(info->list, label,
               path, MENU_FILE_RPL_ENTRY, 0, i);
   }

   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   struct video_shader *shader = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   if (!shader)
      return -1;

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SHADER_APPLY_CHANGES),
         menu_hash_to_str(MENU_LABEL_SHADER_APPLY_CHANGES),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET),
         MENU_FILE_PATH, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PARAMETERS),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_NUM_PASSES),
         0, 0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf_tmp[64], buf[64];

      snprintf(buf_tmp, sizeof(buf_tmp),
            "%s #%u", menu_hash_to_str(MENU_VALUE_SHADER), i);

      menu_entries_push(info->list, buf_tmp,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PASS),
            MENU_SETTINGS_SHADER_PASS_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s Filter", buf_tmp);
      menu_entries_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_FILTER_PASS),
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s Scale", buf_tmp);
      menu_entries_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_SCALE_PASS),
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0, 0);
   }

   return 0;
}

#ifdef HAVE_LIBRETRODB
static int create_string_list_rdb_entry_string(
      const char *desc, const char *label,
      const char *actual_string, const char *path,
      file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr = {0};
   char *output_label           = NULL;
   int str_len                  = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

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

   strlcpy(tmp, desc, sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, actual_string, sizeof(tmp));
   menu_entries_push(list, tmp, output_label, 0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int create_string_list_rdb_entry_int(
      const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr = {0};
   char str[PATH_MAX_LENGTH]        = {0};
   char *output_label               = NULL;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();

   if (!str_list)
      return -1;

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

   snprintf(tmp, sizeof(tmp), "%s : %d", desc, actual_int);
   menu_entries_push(list, tmp, output_label,
         0, 0, 0);

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
   unsigned i, j, k;
   char path_playlist[PATH_MAX_LENGTH];
   char path_base[PATH_MAX_LENGTH]     = {0};
   char query[PATH_MAX_LENGTH]         = {0};
   content_playlist_t *playlist        = NULL;
   database_info_list_t *db_info       = NULL;
   menu_handle_t *menu                 = NULL;
   settings_t *settings                = config_get_ptr();

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto error;

   database_info_build_query(query, sizeof(query),
         "displaylist_parse_database_entry", info->path_b);

   if (!(db_info = database_info_list_new(info->path, query)))
      goto error;

   fill_short_pathname_representation(path_base, info->path,
         sizeof(path_base));
   path_remove_extension(path_base);
   strlcat(path_base, ".lpl", sizeof(path_base));

   fill_pathname_join(path_playlist, settings->playlist_directory, path_base,
         sizeof(path_playlist));

   playlist = content_playlist_init(path_playlist, COLLECTION_SIZE);

   if (playlist)
      strlcpy(menu->db_playlist_file, path_playlist,
            sizeof(menu->db_playlist_file));

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH]      = {0};
      char crc_str[20]               = {0};
      database_info_t *db_info_entry = &db_info->list[i];
      settings_t *settings           = config_get_ptr();
      bool show_advanced_settings    = false;
      
      if (settings)
         show_advanced_settings      = settings->menu.show_advanced_settings;

      if (!db_info_entry)
         continue;

      snprintf(crc_str, sizeof(crc_str), "%08X", db_info_entry->crc32);

      if (playlist)
      {
         for (j = 0; j < content_playlist_size(playlist); j++)
         {
            const char *crc32                = NULL;
            char elem0[PATH_MAX_LENGTH]      = {0};
            char elem1[PATH_MAX_LENGTH]      = {0};
            bool match_found                 = false;
            struct string_list *tmp_str_list = NULL;
            uint32_t hash_value              = 0;

            content_playlist_get_index(playlist, j,
                  NULL, NULL, NULL, NULL,
                  NULL, &crc32);

            tmp_str_list                     = string_split(crc32, "|");

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
               strlcpy(elem0, tmp_str_list->elems[0].data, sizeof(elem0));
            if (tmp_str_list->size > 1)
               strlcpy(elem1, tmp_str_list->elems[1].data, sizeof(elem1));

            hash_value = menu_hash_calculate(elem1);

            switch (hash_value)
            {
               case MENU_VALUE_CRC:
                  if (string_is_equal(crc_str, elem0))
                     match_found = true;
                  break;
               case MENU_VALUE_SHA1:
                  if (string_is_equal(db_info_entry->sha1, elem0))
                     match_found = true;
                  break;
               case MENU_VALUE_MD5:
                  if (string_is_equal(db_info_entry->md5, elem0))
                     match_found = true;
                  break;
            }

            string_list_free(tmp_str_list);

            if (!match_found)
               continue;

            rdb_entry_start_game_selection_ptr = j;
         }
      }

      if (db_info_entry->name)
      {
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_NAME),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, db_info_entry->name, sizeof(tmp));
         menu_entries_push(info->list, tmp,
               menu_hash_to_str(MENU_LABEL_RDB_ENTRY_NAME),
               0, 0, 0);
      }
      if (db_info_entry->description)
      {
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, db_info_entry->description, sizeof(tmp));
         menu_entries_push(info->list, tmp,
               menu_hash_to_str(MENU_LABEL_RDB_ENTRY_DESCRIPTION),
               0, 0, 0);
      }
      if (db_info_entry->genre)
      {
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_GENRE),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, db_info_entry->genre, sizeof(tmp));
         menu_entries_push(info->list, tmp,
               menu_hash_to_str(MENU_LABEL_RDB_ENTRY_GENRE),
               0, 0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_PUBLISHER),
                  db_info_entry->publisher, info->path, info->list) == -1)
            goto error;
      }


      if (db_info_entry->developer)
      {
         for (k = 0; k < db_info_entry->developer->size; k++)
         {
            if (db_info_entry->developer->elems[k].data)
            {
               if (create_string_list_rdb_entry_string(
                        menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER),
                        menu_hash_to_str(MENU_LABEL_RDB_ENTRY_DEVELOPER),
                        db_info_entry->developer->elems[k].data,
                        info->path, info->list) == -1)
                  goto error;
            }
         }
      }

      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ORIGIN),
                  db_info_entry->origin, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_FRANCHISE),
                  db_info_entry->franchise, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_INPUT_MAX_USERS),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_MAX_USERS),
                  db_info_entry->max_users,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->tgdb_rating)
      {
         if (create_string_list_rdb_entry_int("TGDB Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_TGDB_RATING),
                  db_info_entry->tgdb_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Famitsu Magazine Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING),
                  db_info_entry->famitsu_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_review)
      {
         if (create_string_list_rdb_entry_string("Edge Magazine Review",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW),
                  db_info_entry->edge_magazine_review, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING),
                  db_info_entry->edge_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Issue",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE),
                  db_info_entry->edge_magazine_issue,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RELEASE_MONTH),
                  db_info_entry->releasemonth,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RELEASE_YEAR),
                  db_info_entry->releaseyear,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string("BBFC Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_BBFC_RATING),
                  db_info_entry->bbfc_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string("ESRB Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ESRB_RATING),
                  db_info_entry->esrb_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string("ELSPA Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ELSPA_RATING),
                  db_info_entry->elspa_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string("PEGI Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_PEGI_RATING),
                  db_info_entry->pegi_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string("Enhancement Hardware",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW),
                  db_info_entry->enhancement_hw, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string("CERO Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_CERO_RATING),
                  db_info_entry->cero_rating, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->serial)
      {
         if (create_string_list_rdb_entry_string("Serial",
                  "Serial",
                  db_info_entry->serial, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->analog_supported == 1)
      {
         if (create_string_list_rdb_entry_string("Analog supported",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ANALOG),
                  menu_hash_to_str(MENU_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->rumble_supported == 1)
      {
         if (create_string_list_rdb_entry_string("Rumble supported",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RUMBLE),
                  menu_hash_to_str(MENU_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->coop_supported == 1)
      {
         if (create_string_list_rdb_entry_string("Co-op supported",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_COOP),
                  menu_hash_to_str(MENU_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (!show_advanced_settings)
         continue;

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_CRC32),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_CRC32),
                  crc_str,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_SHA1),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_SHA1),
                  db_info_entry->sha1,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_MD5),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_MD5),
                  db_info_entry->md5,
                  info->path, info->list) == -1)
            goto error;
      }
   }

   if (db_info->count < 1)
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            menu_hash_to_str(MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            0, 0, 0);

   content_playlist_free(playlist);
   database_info_list_free(db_info);

   return 0;

error:
   content_playlist_free(playlist);

   return -1;
#else
   return 0;
#endif
}

static int menu_database_parse_query(file_list_t *list, const char *path,
      const char *query)
{
#ifdef HAVE_LIBRETRODB
   unsigned i;
   database_info_list_t *db_list = database_info_list_new(path, query);

   if (!db_list)
      return -1;

   for (i = 0; i < db_list->count; i++)
   {
      if (!string_is_empty(db_list->list[i].name))
         menu_entries_push(list, db_list->list[i].name,
               path, MENU_FILE_RDB_ENTRY, 0, 0);
   }

   database_info_list_free(db_list);
#endif

   return 0;
}

#ifdef HAVE_SHADER_MANAGER
static int deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   size_t list_size = shader->num_parameters;

   if (list_size == 0)
   {
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_SHADER_PARAMETERS),
            "", 0, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
      menu_entries_push(info->list, shader->parameters[i].desc,
            info->label, base_parameter + i, 0, 0);

   return 0;
}
#endif

static int menu_displaylist_parse_settings(void *data,
      menu_displaylist_info_t *info,
      const char *info_label,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry)
{
   enum setting_type precond;
   size_t             count  = 0;
   settings_t *settings      = config_get_ptr();
   rarch_setting_t *setting  = menu_setting_find(info_label);
   uint64_t flags            = menu_setting_get_flags(setting);

   if (!setting)
      return -1;

   switch (parse_type)
   {
      case PARSE_GROUP:
      case PARSE_SUB_GROUP:
         precond = ST_NONE;
         break;
      case PARSE_ACTION:
         precond = ST_ACTION;
         break;
      case PARSE_ONLY_INT:
         precond = ST_INT;
         break;
      case PARSE_ONLY_UINT:
         precond = ST_UINT;
         break;
      case PARSE_ONLY_BIND:
         precond = ST_BIND;
         break;
      case PARSE_ONLY_BOOL:
         precond = ST_BOOL;
         break;
      case PARSE_ONLY_FLOAT:
         precond = ST_FLOAT;
         break;
      case PARSE_ONLY_GROUP:
      default:
         precond = ST_END_GROUP;
         break;
   }

   for (;;)
   {
      bool time_to_exit             = false;
      const char *short_description = 
         menu_setting_get_short_description(setting);
      const char *name              = menu_setting_get_name(setting);
      enum setting_type type        = menu_setting_get_type(setting);

      switch (parse_type)
      {
         case PARSE_NONE:
            switch (type)
            {
               case ST_GROUP:
               case ST_END_GROUP:
               case ST_SUB_GROUP:
               case ST_END_SUB_GROUP:
                  goto loop;
               default:
                  break;
            }
            break;
         case PARSE_GROUP:
         case PARSE_ONLY_GROUP:
            if (type == ST_GROUP)
               break;
            goto loop;
         case PARSE_SUB_GROUP:
            break;
         case PARSE_ACTION:
            if (type == ST_ACTION)
               break;
            goto loop;
         case PARSE_ONLY_INT:
            if (type == ST_INT)
               break;
            goto loop;
         case PARSE_ONLY_UINT:
            if (type == ST_UINT)
               break;
            goto loop;
         case PARSE_ONLY_BIND:
            if (type == ST_BIND)
               break;
            goto loop;
         case PARSE_ONLY_BOOL:
            if (type == ST_BOOL)
               break;
            goto loop;
         case PARSE_ONLY_FLOAT:
            if (type == ST_FLOAT)
               break;
            goto loop;
      }

      if (flags & SD_FLAG_ADVANCED &&
            !settings->menu.show_advanced_settings)
         goto loop;


      menu_entries_push(info->list, short_description,
            name, menu_setting_set_flags(setting), 0, 0);
      count++;

loop:
      switch (parse_type)
      {
         case PARSE_NONE:
         case PARSE_GROUP:
         case PARSE_ONLY_GROUP:
         case PARSE_SUB_GROUP:
            if (menu_setting_get_type(setting) == precond)
               time_to_exit = true;
            break;
         case PARSE_ONLY_BIND:
         case PARSE_ONLY_FLOAT:
         case PARSE_ONLY_BOOL:
         case PARSE_ONLY_INT:
         case PARSE_ONLY_UINT:
         case PARSE_ACTION:
            time_to_exit = true;
            break;
      }

      if (time_to_exit)
         break;
      menu_settings_list_increment(&setting);
   }

   if (count == 0 && add_empty_entry)
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_SETTINGS_FOUND),
            menu_hash_to_str(MENU_LABEL_NO_SETTINGS_FOUND),
            0, 0, 0);

   return 0;
}

static int menu_displaylist_sort_playlist(const content_playlist_entry_t *a,
      const content_playlist_entry_t *b)
{
   const char *a_label = content_playlist_entry_get_label(a);
   const char *b_label = content_playlist_entry_get_label(b);

   if (!a_label || !b_label)
      return 0;

   return strcasecmp(a_label, b_label);
}

static int menu_displaylist_parse_horizontal_list(
      menu_displaylist_info_t *info)
{
   menu_ctx_list_t list_info;
   menu_ctx_list_t list_horiz_info;
   char path_playlist[PATH_MAX_LENGTH], lpl_basename[PATH_MAX_LENGTH];
   bool is_historylist                 = false;
   content_playlist_t *playlist        = NULL;
   settings_t      *settings           = config_get_ptr();
   menu_handle_t        *menu          = NULL;
   struct item_file *item              = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SELECTION, &list_info);

   list_info.type       = MENU_LIST_TABS;
   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SIZE,      &list_info);

   list_horiz_info.type = MENU_LIST_HORIZONTAL;
   list_horiz_info.idx  = list_info.selection - (list_info.size +1);

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_ENTRY,      &list_horiz_info);
   
   item = (struct item_file*)list_horiz_info.entry;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   if (!item)
      return -1;

   strlcpy(lpl_basename, item->path, sizeof(lpl_basename));
   path_remove_extension(lpl_basename);

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);

   fill_pathname_join(path_playlist,
         settings->playlist_directory, item->path,
         sizeof(path_playlist));

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_INIT, (void*)path_playlist);

   strlcpy(menu->db_playlist_file,
         path_playlist, sizeof(menu->db_playlist_file));
   strlcpy(path_playlist,
         menu_hash_to_str(MENU_LABEL_COLLECTION),
         sizeof(path_playlist));

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   content_playlist_qsort(playlist, menu_displaylist_sort_playlist);

   if (string_is_equal(lpl_basename, "content_history"))
      is_historylist = true;

   menu_displaylist_parse_playlist(info,
         playlist, path_playlist, is_historylist);

   return 0;
}

static int menu_displaylist_parse_load_content_settings(
      menu_displaylist_info_t *info)
{
   menu_handle_t *menu    = NULL;
   global_t *global       = global_get_ptr();
#ifdef HAVE_CHEEVOS
   settings_t *settings   = config_get_ptr();
#endif
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
      rarch_system_info_t *system = NULL;

      runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_RESUME_CONTENT),
            menu_hash_to_str(MENU_LABEL_RESUME_CONTENT),
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_RESTART_CONTENT),
            menu_hash_to_str(MENU_LABEL_RESTART_CONTENT),
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CLOSE_CONTENT),
            menu_hash_to_str(MENU_LABEL_CLOSE_CONTENT),
            MENU_SETTING_ACTION_CLOSE, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_TAKE_SCREENSHOT),
            menu_hash_to_str(MENU_LABEL_TAKE_SCREENSHOT),
            MENU_SETTING_ACTION_SCREENSHOT, 0, 0);

      menu_displaylist_parse_settings(menu, info,
            menu_hash_to_str(MENU_LABEL_STATE_SLOT), PARSE_ONLY_INT, true);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SAVE_STATE),
            menu_hash_to_str(MENU_LABEL_SAVE_STATE),
            MENU_SETTING_ACTION_SAVESTATE, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_LOAD_STATE),
            menu_hash_to_str(MENU_LABEL_LOAD_STATE),
            MENU_SETTING_ACTION_LOADSTATE, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_OPTIONS),
            menu_hash_to_str(MENU_LABEL_CORE_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);

      if (global->has_set.input_descriptors)
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS),
               menu_hash_to_str(MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS),
               MENU_SETTING_ACTION, 0, 0);


      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS),
            menu_hash_to_str(MENU_LABEL_CORE_CHEAT_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);
      if (     (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            && system && system->disk_control_cb.get_num_images)
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_OPTIONS),
               menu_hash_to_str(MENU_LABEL_DISK_OPTIONS),
               MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0);
#ifdef HAVE_SHADER_MANAGER
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SHADER_OPTIONS),
            menu_hash_to_str(MENU_LABEL_SHADER_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);
#endif
#ifdef HAVE_CHEEVOS
      if(settings->cheevos.enable)
         menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_ACHIEVEMENT_LIST),
            menu_hash_to_str(MENU_LABEL_ACHIEVEMENT_LIST),
            MENU_SETTING_ACTION, 0, 0);
#endif
   }
   else
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
            "", MENU_SETTING_NO_ITEM, 0, 0);

   return 0;
}

static int menu_displaylist_parse_horizontal_content_actions(
      menu_displaylist_info_t *info)
{
   unsigned idx                    = rpl_entry_selection_ptr;
   settings_t *settings            = NULL;
   menu_handle_t *menu             = NULL;
   const char *label               = NULL;
   const char *core_path           = NULL;
   const char *core_name           = NULL;
   const char *db_name             = NULL;
   char *fullpath                  = NULL;
   content_playlist_t *playlist    = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   settings = config_get_ptr();

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, fullpath))
      menu_displaylist_parse_load_content_settings(info);
   else
      menu_entries_push(info->list, "Run", "collection",
            MENU_FILE_PLAYLIST_ENTRY, 0, idx);

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   content_playlist_get_index(playlist, idx,
         NULL, &label, &core_path, &core_name, NULL, &db_name);

   if (!string_is_empty(db_name))
   {
      char db_path[PATH_MAX_LENGTH] = {0};

      fill_pathname_join(db_path, settings->content_database,
            db_name, sizeof(db_path));
      path_remove_extension(db_path);
      strlcat(db_path, ".rdb", sizeof(db_path));

      menu_entries_push(info->list, label,
            db_path, MENU_FILE_RDB_ENTRY, 0, idx);
   }

   return 0;
}

static int menu_displaylist_parse_information_list(
      menu_displaylist_info_t *info)
{
   settings_t *settings        = config_get_ptr();

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFORMATION),
         menu_hash_to_str(MENU_LABEL_CORE_INFORMATION),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_NETPLAY
#ifndef HAVE_SOCKET_LEGACY
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_NETWORK_INFORMATION),
         menu_hash_to_str(MENU_LABEL_NETWORK_INFORMATION),
         MENU_SETTING_ACTION, 0, 0);
#endif
#endif

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFORMATION),
         menu_hash_to_str(MENU_LABEL_SYSTEM_INFORMATION),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_DATABASE_MANAGER),
         menu_hash_to_str(MENU_LABEL_DATABASE_MANAGER_LIST),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CURSOR_MANAGER),
         menu_hash_to_str(MENU_LABEL_CURSOR_MANAGER_LIST),
         MENU_SETTING_ACTION, 0, 0);
#endif

   if (runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL))
   {
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_FRONTEND_COUNTERS),
            menu_hash_to_str(MENU_LABEL_FRONTEND_COUNTERS),
            MENU_SETTING_ACTION, 0, 0);

      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_COUNTERS),
            menu_hash_to_str(MENU_LABEL_CORE_COUNTERS),
            MENU_SETTING_ACTION, 0, 0);
   }

   if(settings->debug_panel_enable)
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_DEBUG_INFORMATION),
            menu_hash_to_str(MENU_LABEL_DEBUG_INFORMATION),
            MENU_SETTING_ACTION, 0, 0);

   return 0;
}

static int menu_displaylist_parse_add_content_list(
      menu_displaylist_info_t *info)
{
#ifdef HAVE_NETWORKING
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         menu_hash_to_str(MENU_LABEL_DOWNLOAD_CORE_CONTENT),
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_LIBRETRODB
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SCAN_DIRECTORY),
         menu_hash_to_str(MENU_LABEL_SCAN_DIRECTORY),
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SCAN_FILE),
         menu_hash_to_str(MENU_LABEL_SCAN_FILE),
         MENU_SETTING_ACTION, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_scan_directory_list(
      menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SCAN_DIRECTORY),
         menu_hash_to_str(MENU_LABEL_SCAN_DIRECTORY),
         MENU_SETTING_ACTION, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_options(
      menu_displaylist_info_t *info)
{
#ifdef HAVE_NETWORKING

#ifdef HAVE_LAKKA
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_LAKKA),
         menu_hash_to_str(MENU_LABEL_UPDATE_LAKKA),
         MENU_SETTING_ACTION, 0, 0);
#endif

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST),
         menu_hash_to_str(MENU_LABEL_CORE_UPDATER_LIST),
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES),
         menu_hash_to_str(MENU_LABEL_UPDATE_CORE_INFO_FILES),
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_ASSETS),
         menu_hash_to_str(MENU_LABEL_UPDATE_ASSETS),
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES),
         menu_hash_to_str(MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES),
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CHEATS),
         menu_hash_to_str(MENU_LABEL_UPDATE_CHEATS),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_DATABASES),
         menu_hash_to_str(MENU_LABEL_UPDATE_DATABASES),
         MENU_SETTING_ACTION, 0, 0);
#endif

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_OVERLAYS),
         menu_hash_to_str(MENU_LABEL_UPDATE_OVERLAYS),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_CG
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CG_SHADERS),
         menu_hash_to_str(MENU_LABEL_UPDATE_CG_SHADERS),
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_GLSL
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS),
         menu_hash_to_str(MENU_LABEL_UPDATE_GLSL_SHADERS),
         MENU_SETTING_ACTION, 0, 0);
#endif

#else
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
         "", MENU_SETTING_NO_ITEM, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_options_cheats(
      menu_displaylist_info_t *info)
{
   unsigned i;

   if (!cheat_manager_alloc_if_empty())
      return -1;

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_NUM_PASSES),
         menu_hash_to_str(MENU_LABEL_CHEAT_NUM_PASSES),
         0, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES),
         menu_hash_to_str(MENU_LABEL_CHEAT_APPLY_CHANGES),
         MENU_SETTING_ACTION, 0, 0);

   for (i = 0; i < cheat_manager_get_size(); i++)
   {
      char cheat_label[64] = {0};

      snprintf(cheat_label, sizeof(cheat_label),
            "%s #%u: ", menu_hash_to_str(MENU_VALUE_CHEAT), i);
      if (cheat_manager_get_desc(i))
         strlcat(cheat_label, cheat_manager_get_desc(i), sizeof(cheat_label));
      menu_entries_push(info->list,
            cheat_label, "", MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_remappings(
      menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   settings_t        *settings = NULL;
   rarch_system_info_t *system = NULL;
   menu_handle_t       *menu   = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   settings = config_get_ptr();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   for (p = 0; p < settings->input.max_users; p++)
   {
      char key_type[PATH_MAX_LENGTH], key_analog[PATH_MAX_LENGTH];
      snprintf(key_type, sizeof(key_type),
               "input_libretro_device_p%u", p + 1);
      snprintf(key_analog, sizeof(key_analog),
               "input_player%u_analog_dpad_mode", p + 1);

      menu_displaylist_parse_settings(menu, info,
            key_type, PARSE_ONLY_UINT, true);
      menu_displaylist_parse_settings(menu, info,
            key_analog, PARSE_ONLY_UINT, true);
   }

   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_SAVE_CORE),
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_SAVE_GAME),
         MENU_SETTING_ACTION, 0, 0);

   for (p = 0; p < settings->input.max_users; p++)
   {
      for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND + 4; retro_id++)
      {
         char desc_label[64];
         unsigned user           = p + 1;
         unsigned desc_offset    = retro_id;
         const char *description = NULL;

         if (desc_offset >= RARCH_FIRST_CUSTOM_BIND)
            desc_offset = RARCH_FIRST_CUSTOM_BIND 
               + (desc_offset - RARCH_FIRST_CUSTOM_BIND) * 2;

         if (system)
            description = system->input_desc_btn[p][desc_offset];

         if (!description)
            continue;

         snprintf(desc_label, sizeof(desc_label),
               "%s %u %s : ", menu_hash_to_str(MENU_VALUE_USER),
               user, description);
         menu_entries_push(info->list, desc_label, "",
               MENU_SETTINGS_INPUT_DESC_BEGIN +
               (p * (RARCH_FIRST_CUSTOM_BIND + 4)) +  retro_id, 0, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_generic(
      menu_displaylist_info_t *info, bool horizontal)
{
   bool path_is_compressed, push_dir, filter_ext;
   size_t i, list_size;
   struct string_list *str_list = NULL;
   core_info_list_t *list       = NULL;
   unsigned items_found         = 0;
   int                   device = 0;
   settings_t *settings         = config_get_ptr();
   uint32_t hash_label          = menu_hash_calculate(info->label);

   core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);

   (void)device;

   if (!*info->path)
   {
      if (frontend_driver_parse_drive_list(info->list) != 0)
         menu_entries_push(info->list, "/", "",
               MENU_FILE_DIRECTORY, 0, 0);
      return 0;
   }

   path_is_compressed = path_is_compressed_file(info->path);
   push_dir           = 
      (menu_setting_get_browser_selection_type(info->setting) == ST_DIR);

   filter_ext = 
      settings->menu.navigation.browser.filter.supported_extensions_enable;

   if (hash_label == MENU_LABEL_SCAN_FILE)
      filter_ext = false;

   if (path_is_compressed)
      str_list = compressed_file_list_new(info->path, info->exts);
   else
      str_list = dir_list_new(info->path,
            filter_ext ? info->exts : NULL,
            true, true);

   if (hash_label == MENU_LABEL_SCAN_DIRECTORY)
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY),
            menu_hash_to_str(MENU_LABEL_SCAN_THIS_DIRECTORY),
            MENU_FILE_SCAN_DIRECTORY, 0 ,0);

   if (push_dir)
      menu_entries_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_USE_THIS_DIRECTORY),
            menu_hash_to_str(MENU_LABEL_USE_THIS_DIRECTORY),
            MENU_FILE_USE_DIRECTORY, 0 ,0);

   if (!horizontal && hash_label != MENU_LABEL_CORE_LIST)
   {
      char out_dir[PATH_MAX_LENGTH];
      fill_pathname_parent_dir(out_dir, info->path, sizeof(out_dir));

      if (!string_is_empty(out_dir))
      {
         menu_entries_push(info->list, "..", info->path,
               MENU_FILE_PARENT_DIRECTORY, 0, 0);
      }
   }

   if (!str_list)
   {
      const char *str = path_is_compressed
         ? menu_hash_to_str(MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE)
         : menu_hash_to_str(MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      menu_entries_push(info->list, str, "", 0, 0, 0);
      return 0;
   }

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size == 0)
   {
      if (!(info->flags & SL_FLAG_ALLOW_EMPTY_LIST))
      {
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
               "", MENU_SETTING_NO_ITEM, 0, 0);
      }

      string_list_free(str_list);

      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      char label[PATH_MAX_LENGTH];
      const char *path              = NULL;
      enum menu_file_type file_type = MENU_FILE_NONE;

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
            if (hash_label == MENU_LABEL_DETECT_CORE_LIST ||
                  hash_label == MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST)
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
            file_type = (enum menu_file_type)info->type_default;
            break;
      }

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;
      if (hash_label == MENU_LABEL_SCAN_DIRECTORY && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (*info->path && !path_is_compressed)
         path = path_basename(path);

      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      switch (hash_label)
      {
         case MENU_LABEL_CONTENT_COLLECTION_LIST:
            if (is_dir && !horizontal)
               file_type = MENU_FILE_DIRECTORY;
            else if (is_dir && horizontal)
               continue;
            else
               file_type = MENU_FILE_PLAYLIST_COLLECTION;
            break;
         case MENU_LABEL_CORE_LIST:
#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
            {
               char salamander_name[PATH_MAX_LENGTH];

               if (frontend_driver_get_salamander_basename(
                        salamander_name, sizeof(salamander_name)))
               {
                  if (string_is_equal_noncase(path, salamander_name))
                     continue;
               }

               if (is_dir)
                  continue;
            }
#endif
            /* Compressed cores are unsupported */
            if (file_type == MENU_FILE_CARCHIVE)
               continue;

            file_type = is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE;
            break;
      }

      if (settings->multimedia.builtin_mediaplayer_enable ||
            settings->multimedia.builtin_imageviewer_enable)
      {
         switch (rarch_path_is_media_type(path))
         {
            case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
               if (settings->multimedia.builtin_mediaplayer_enable)
                  file_type = MENU_FILE_MOVIE;
#endif
               break;
            case RARCH_CONTENT_MUSIC:
#ifdef HAVE_FFMPEG
               if (settings->multimedia.builtin_mediaplayer_enable)
                  file_type = MENU_FILE_MUSIC;
#endif
               break;
            case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
               if (settings->multimedia.builtin_imageviewer_enable
                     && hash_label != MENU_LABEL_MENU_WALLPAPER)
                  file_type = MENU_FILE_IMAGEVIEWER;
#endif
            default:
               break;
         }
      }

      items_found++;
      menu_entries_push(info->list, path, label,
            file_type, 0, 0);
   }

   string_list_free(str_list);

   if (items_found == 0)
   {
      if (!(info->flags & SL_FLAG_ALLOW_EMPTY_LIST))
      {
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
               "", MENU_SETTING_NO_ITEM, 0, 0);
      }

      return 0;
   }

   switch (hash_label)
   {
      case MENU_LABEL_CORE_LIST:
         {
            const char *dir = NULL;

            menu_entries_get_last_stack(&dir, NULL, NULL, NULL);

            list_size = file_list_get_size(info->list);

            for (i = 0; i < list_size; i++)
            {
               char core_path[PATH_MAX_LENGTH], display_name[PATH_MAX_LENGTH];
               unsigned type                      = 0;
               const char *path                   = NULL;

               menu_entries_get_at_offset(info->list,
                     i, &path, NULL, &type, NULL,
                     NULL);

               if (type != MENU_FILE_CORE)
                  continue;

               fill_pathname_join(core_path, dir, path, sizeof(core_path));

               if (core_info_list_get_display_name(list,
                        core_path, display_name, sizeof(display_name)))
                  menu_entries_set_alt_at_offset(info->list, i, display_name);
            }
            info->need_sort = true;
         }
         break;
   }

   return 0;
}

bool menu_playlist_find_associated_core(const char *path, char *s, size_t len)
{
   bool ret = false;
   unsigned j;
   settings_t *settings         = config_get_ptr();
   struct string_list *existing_core_names = 
      string_split(settings->playlist_names, ";");
   struct string_list *existing_core_paths = 
      string_split(settings->playlist_cores, ";");

   for (j = 0; j < existing_core_names->size; j++)
   {
      if (string_is_equal(path, existing_core_names->elems[j].data))
      {
         if (existing_core_paths)
         {
            const char *existing_core = existing_core_paths->elems[j].data;

            if (existing_core)
            {
               strlcpy(s, existing_core, len);
               ret = true;
            }
         }
         break;
      }
   }

   string_list_free(existing_core_names);
   string_list_free(existing_core_paths);
   return ret;
}

static void menu_displaylist_parse_playlist_associations(
      menu_displaylist_info_t *info)
{
   settings_t      *settings    = config_get_ptr();
   struct string_list *stnames  = string_split(settings->playlist_names, ";");
   struct string_list *stcores  = string_split(settings->playlist_cores, ";");
   struct string_list *str_list = 
      dir_list_new_special(settings->playlist_directory,
            DIR_LIST_COLLECTIONS, NULL);

   if (str_list && str_list->size)
   {
      unsigned i;
      char new_playlist_names[PATH_MAX_LENGTH] = {0};
      char new_playlist_cores[PATH_MAX_LENGTH] = {0};

      for (i = 0; i < str_list->size; i++)
      {
         unsigned found = 0;
         union string_list_elem_attr attr = {0};
         char path_base[PATH_MAX_LENGTH]  = {0};
         char core_path[PATH_MAX_LENGTH]  = {0};
         const char *path                 = 
            path_basename(str_list->elems[i].data);

         if (!menu_playlist_find_associated_core(
                  path, core_path, sizeof(core_path)))
            strlcpy(core_path, "DETECT", sizeof(core_path));

         strlcpy(path_base, path, sizeof(path_base));

         found = string_list_find_elem(stnames, path_base);
         if (found)
         {
            string_list_set(stcores, found-1, core_path);
         }
         else
         {
            string_list_append(stnames, path_base, attr);
            string_list_append(stcores, core_path, attr);
         }

         path_remove_extension(path_base);
         menu_entries_push(info->list,
               path_base,
               str_list->elems[i].data,
               MENU_SETTINGS_PLAYLIST_ASSOCIATION_START + i,
               0, 0);
      }

      string_list_join_concat(new_playlist_names,
            sizeof(new_playlist_names), stnames, ";");
      string_list_join_concat(new_playlist_cores,
            sizeof(new_playlist_cores), stcores, ";");

      strlcpy(settings->playlist_names,
            new_playlist_names, sizeof(settings->playlist_names));
      strlcpy(settings->playlist_cores,
            new_playlist_cores, sizeof(settings->playlist_cores));
   }

   string_list_free(str_list);
   string_list_free(stnames);
   string_list_free(stcores);
}

static bool menu_displaylist_push_list_process(menu_displaylist_info_t *info)
{
   if (!info)
      return false;

   if (info->need_sort)
      file_list_sort_on_alt(info->list);

   if (info->need_refresh)
      menu_entries_ctl(MENU_ENTRIES_CTL_REFRESH, info->list);

   if (info->need_push)
   {
      info->label_hash = menu_hash_calculate(info->label);
      menu_driver_ctl(RARCH_MENU_CTL_POPULATE_ENTRIES, info);
      ui_companion_driver_notify_list_loaded(info->list, info->menu_list);
   }

   return true;
}

static bool menu_displaylist_push(menu_displaylist_ctx_entry_t *entry)
{
   bool push_list_process       = false;
   menu_file_list_cbs_t *cbs    = NULL;
   const char *path             = NULL;
   const char *label            = NULL;
   uint32_t          hash_label = 0;
   unsigned type                = 0;
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();

   if (!entry)
      return false;

   menu_entries_get_last_stack(&path, &label, &type, NULL);

   info.list      = entry->list;
   info.menu_list = entry->stack;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   hash_label     = menu_hash_calculate(label);

   if (!info.list)
      return false;

   switch (hash_label)
   {
      case MENU_VALUE_MAIN_MENU:
         if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
            return false;
         push_list_process = true;
         break;
      case MENU_VALUE_SETTINGS_TAB:
         if (!menu_displaylist_ctl(DISPLAYLIST_SETTINGS_ALL, &info))
            return false;
         push_list_process = true;
         break;
      case MENU_VALUE_HISTORY_TAB:
         if (!menu_displaylist_ctl(DISPLAYLIST_HISTORY, &info))
            return false;
         push_list_process = true;
         break;
      case MENU_VALUE_ADD_TAB:
         if (!menu_displaylist_ctl(DISPLAYLIST_SCAN_DIRECTORY_LIST, &info))
            return false;
         push_list_process = true;
         break;
      case MENU_VALUE_PLAYLISTS_TAB:
         info.type = 42;
         strlcpy(info.exts, "lpl", sizeof(info.exts));
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_CONTENT_COLLECTION_LIST),
               sizeof(info.label));

         if (string_is_empty(settings->playlist_directory))
         {
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info.list);
            menu_entries_push(info.list,
                  menu_hash_to_str(
                     MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
                  menu_hash_to_str(
                     MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
                  MENU_INFO_MESSAGE, 0, 0);
            info.need_refresh = true;
            info.need_push    = true;
         }
         else
         {
            strlcpy(info.path, settings->playlist_directory,
                  sizeof(info.path));
            if (!menu_displaylist_ctl(
                     DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
               return false;
         }
         push_list_process = true;
         break;
      case MENU_VALUE_HORIZONTAL_MENU:
         if (!menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL, &info))
            return false;
         push_list_process = true;
         break;
   }

   if (push_list_process)
      return menu_displaylist_push_list_process(&info);

   cbs = menu_entries_get_last_stack_actiondata();

   if (cbs && cbs->action_deferred_push)
   {
      if (cbs->action_deferred_push(&info) != 0)
         return -1;
   }

   return true;
}

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type, void *data)
{
   size_t i;
   menu_ctx_displaylist_t disp_list;
#ifdef HAVE_SHADER_MANAGER
   video_shader_ctx_t shader_info;
#endif
   int ret                       = 0;
   rarch_system_info_t *system   = NULL;
   core_info_list_t *list        = NULL;
   menu_handle_t       *menu     = NULL;
   settings_t      *settings     = NULL;
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;

   switch (type)
   {
      case DISPLAYLIST_PROCESS:
         return menu_displaylist_push_list_process(info);
      case DISPLAYLIST_PUSH_ONTO_STACK:
         return menu_displaylist_push((menu_displaylist_ctx_entry_t*)data);
      default:
         break;
   }

   if (!info)
      return false;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;

   settings = config_get_ptr();

   core_info_ctl(CORE_INFO_CTL_LIST_GET,    &list);
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   disp_list.info = info;
   disp_list.type = type;

   if (menu_driver_ctl(RARCH_MENU_CTL_LIST_PUSH, &disp_list))
      return true;

   switch (type)
   {
      case DISPLAYLIST_HELP_SCREEN_LIST:
      case DISPLAYLIST_MAIN_MENU:
      case DISPLAYLIST_SETTINGS:
      case DISPLAYLIST_SETTINGS_ALL:
      case DISPLAYLIST_HORIZONTAL:
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
      case DISPLAYLIST_CONTENT_SETTINGS:
      case DISPLAYLIST_INFORMATION_LIST:
      case DISPLAYLIST_ADD_CONTENT_LIST:
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
      case DISPLAYLIST_LOAD_CONTENT_LIST:
      case DISPLAYLIST_USER_BINDS_LIST:
      case DISPLAYLIST_ACCOUNTS_LIST:
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
      case DISPLAYLIST_OPTIONS:
      case DISPLAYLIST_OPTIONS_CHEATS:
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
      case DISPLAYLIST_DATABASE_ENTRY:
      case DISPLAYLIST_DATABASE_QUERY:
      case DISPLAYLIST_OPTIONS_SHADERS:
      case DISPLAYLIST_CORE_CONTENT:
      case DISPLAYLIST_PLAYLIST_COLLECTION:
      case DISPLAYLIST_HISTORY:
      case DISPLAYLIST_OPTIONS_DISK:
      case DISPLAYLIST_NETWORK_INFO:
      case DISPLAYLIST_SYSTEM_INFO:
      case DISPLAYLIST_DEBUG_INFO:
      case DISPLAYLIST_ACHIEVEMENT_LIST:
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_CORES_UPDATER:
      case DISPLAYLIST_LAKKA:
      case DISPLAYLIST_CORES_SUPPORTED:
      case DISPLAYLIST_CORES_COLLECTION_SUPPORTED:
      case DISPLAYLIST_CORE_INFO:
      case DISPLAYLIST_CORE_OPTIONS:
      case DISPLAYLIST_DEFAULT:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_DATABASES:
      case DISPLAYLIST_DATABASE_CURSORS:
      case DISPLAYLIST_DATABASE_PLAYLISTS:
      case DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL:
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_IMAGES:
      case DISPLAYLIST_OVERLAYS:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_RECORD_CONFIG_FILES:
      case DISPLAYLIST_CONFIG_FILES:
      case DISPLAYLIST_CONTENT_HISTORY:
      case DISPLAYLIST_ARCHIVE_ACTION:
      case DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         break;
      default:
         break;
   }

   switch (type)
   {
      case DISPLAYLIST_NONE:
         break;
      case DISPLAYLIST_INFO:
         menu_entries_push(info->list, info->path,
               info->label, info->type, info->directory_ptr, 0);
         break;
      case DISPLAYLIST_GENERIC:
         {
            menu_ctx_list_t list_info;
            bool refresh      = false;
            bool pending_push = true;

            list_info.type    = MENU_LIST_PLAIN;
            list_info.action  = 0;

            menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

            menu_entries_push(info->list, info->path,
                  info->label, info->type, info->directory_ptr, 0);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
            menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
         }
         break;
      case DISPLAYLIST_USER_BINDS_LIST:
         {
            char lbl[PATH_MAX_LENGTH];
            snprintf(lbl, sizeof(lbl),
                  "Input User %.1s Binds", info->path);
            ret = menu_displaylist_parse_settings(menu, info,
                  lbl, PARSE_NONE, true);
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_ACCOUNTS_LIST:
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_ACCOUNTS_LIST_END),
               PARSE_NONE, true);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
#ifdef HAVE_CHEEVOS
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS),
               PARSE_NONE, true);
#else
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
               "", MENU_SETTING_NO_ITEM, 0, 0);
         ret = 0;
#endif
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HELP_SCREEN_LIST:
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_CONTROLS),
               menu_hash_to_str(MENU_LABEL_HELP_CONTROLS),
               0, 0, 0);
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE),
               menu_hash_to_str(MENU_LABEL_HELP_WHAT_IS_A_CORE),
               0, 0, 0);
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_LOADING_CONTENT),
               menu_hash_to_str(MENU_LABEL_HELP_LOADING_CONTENT),
               0, 0, 0);
#ifdef HAVE_LIBRETRODB
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_SCANNING_CONTENT),
               menu_hash_to_str(MENU_LABEL_HELP_SCANNING_CONTENT),
               0, 0, 0);
#endif
#ifdef HAVE_OVERLAY
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD),
               menu_hash_to_str(MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD),
               0, 0, 0);
#endif
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               menu_hash_to_str(MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               0, 0, 0);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HELP:
         menu_entries_push(info->list, info->path,
               info->label, info->type, info->directory_ptr, 0);
         menu->push_help_screen = false;
         break;
      case DISPLAYLIST_SETTING:
         {
            menu_displaylist_ctx_parse_entry_t *entry  = 
               (menu_displaylist_ctx_parse_entry_t*)data;

            if (!entry)
               return false;

            if (menu_displaylist_parse_settings(entry->data,
                     entry->info,
                     entry->info_label,
                     entry->parse_type,
                     entry->add_empty_entry) == -1)
               return false;

         }
         return true;
      case DISPLAYLIST_SETTINGS:
         ret = menu_displaylist_parse_settings(menu, info,
               info->label, PARSE_NONE, true);
         info->need_push    = true;
         break;
      case DISPLAYLIST_MAIN_MENU:
         runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

         if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CONTENT_SETTINGS),
                  PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_START_CORE), PARSE_ACTION, false);

#ifndef HAVE_DYNAMIC
         if (frontend_driver_has_fork())
#endif
         {
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CORE_LIST), PARSE_ACTION, false);
         }

         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_LIST),
               PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_HISTORY),
               PARSE_ACTION, false);
#if defined(HAVE_NETWORKING)
#if defined(HAVE_LIBRETRODB)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ADD_CONTENT_LIST),
               PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ONLINE_UPDATER),
               PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SETTINGS), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INFORMATION_LIST),
               PARSE_ACTION, false);
#ifndef HAVE_DYNAMIC
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_RESTART_RETROARCH),
               PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CONFIGURATIONS),
               PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_CURRENT_CONFIG),
               PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_NEW_CONFIG),
               PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_HELP_LIST),
               PARSE_ACTION, false);
#if !defined(IOS)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_QUIT_RETROARCH),
               PARSE_ACTION, false);
#endif
#if defined(HAVE_LAKKA)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SHUTDOWN),
               PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_REBOOT),
               PARSE_ACTION, false);
#endif
         info->need_push    = true;
         break;
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_HISTORY_LIST_ENABLE),
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CONTENT_HISTORY_SIZE),
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_playlist_associations(info);
         info->need_push    = true;
         break;
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
         {
            unsigned i;

            for (i = 0; i < RARCH_BIND_LIST_END; i++)
            {
               ret = menu_displaylist_parse_settings(menu, info,
                     input_config_bind_map_get_base(i), PARSE_ONLY_BIND, false);
               (void)ret;
            }
         }
         info->need_push    = true;
         break;
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_MAX_USERS),
               PARSE_ONLY_UINT, false);
#if TARGET_OS_IPHONE
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_SMALL_KEYBOARD_ENABLE),
               PARSE_ONLY_BOOL, false);
#endif
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_POLL_TYPE_BEHAVIOR),
               PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_ICADE_ENABLE),
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(
                  MENU_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE),
               PARSE_ONLY_UINT, false);
#ifdef ANDROID
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_BACK_AS_MENU_TOGGLE_ENABLE),
               PARSE_ONLY_BOOL, false);
#endif
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_MENU_TOGGLE_GAMEPAD_COMBO), PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_REMAP_BINDS_ENABLE), PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_AUTODETECT_ENABLE), PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW), PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND), PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_AXIS_THRESHOLD), PARSE_ONLY_FLOAT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_TURBO_PERIOD), PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_DUTY_CYCLE), PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_BIND_MODE), PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_HOTKEY_BINDS), PARSE_ACTION, false);

         {
            unsigned user;
            for (user = 0; user < settings->input.max_users; user++)
            {
               char binds_list[PATH_MAX_LENGTH];
               snprintf(binds_list,  sizeof(binds_list), "%d_input_binds_list", user + 1);
               menu_displaylist_parse_settings(menu, info,
                     binds_list, PARSE_ACTION, false);
            }
         }

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_DRIVER_SETTINGS),          PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_AUDIO_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INPUT_SETTINGS),   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_SETTINGS),            PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_CONFIGURATION_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_SAVING_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_LOGGING_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_REWIND_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_RECORDING_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_OVERLAY_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_MENU_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_UI_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS),  PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_NETWORK_SETTINGS),   PARSE_ONLY_GROUP, false);
#ifdef HAVE_LAKKA
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_LAKKA_SERVICES),   PARSE_ONLY_GROUP, false);
#endif
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_PLAYLIST_SETTINGS),   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_USER_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_DIRECTORY_SETTINGS),   PARSE_ONLY_GROUP, false);
         ret = menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_VALUE_PRIVACY_SETTINGS),   PARSE_ONLY_GROUP, false);
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         ret = menu_displaylist_parse_horizontal_list(info);

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         ret = menu_displaylist_parse_horizontal_content_actions(info);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CONTENT_SETTINGS:
         ret = menu_displaylist_parse_load_content_settings(info);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INFORMATION_LIST:
         ret = menu_displaylist_parse_information_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_ADD_CONTENT_LIST:
         ret = menu_displaylist_parse_add_content_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
         ret = menu_displaylist_parse_scan_directory_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT),
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT),
               MENU_SETTING_ACTION, 0, 0);

         if (core_info_list_num_info_files(list))
         {
            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DETECT_CORE_LIST),
                  menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);

            menu_entries_push(info->list,
                  menu_hash_to_str(
                     MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  menu_hash_to_str(
                     MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);
         }

#ifdef HAVE_LIBRETRODB
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST),
               menu_hash_to_str(MENU_LABEL_CONTENT_COLLECTION_LIST),
               MENU_SETTING_ACTION, 0, 0);
#endif

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_OPTIONS:
         ret = menu_displaylist_parse_options(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         ret = menu_displaylist_parse_options_cheats(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         ret = menu_displaylist_parse_options_remappings(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
#ifdef HAVE_SHADER_MANAGER
         video_shader_driver_ctl(SHADER_CTL_GET_CURRENT_SHADER, &shader_info);

         if (shader_info.data)
            ret = deferred_push_video_shader_parameters_common(
                  info, shader_info.data,
                  (type == DISPLAYLIST_SHADER_PARAMETERS)
                  ? MENU_SETTINGS_SHADER_PARAMETER_0 
                  : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
                  );
         else
         {
            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_NO_SHADER_PARAMETERS),
                  "", 0, 0, 0);
            ret = 0;
         }

         info->need_push = true;
#endif
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         menu_displaylist_push_perfcounter(info,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               retro_get_perf_counter_libretro() 
               : retro_get_perf_counter_rarch(),
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               retro_get_perf_count_libretro()   
               : retro_get_perf_count_rarch(),
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN :
               MENU_SETTINGS_PERF_COUNTERS_BEGIN);
         ret = 0;

         info->need_refresh = false;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_ENTRY:
         {
            struct string_list *str_list  = string_split(info->label, "|");

            if (!str_list)
               return false;

            strlcpy(info->path_b,
                  str_list->elems[1].data, sizeof(info->path_b));
            strlcpy(info->label,
                  str_list->elems[0].data, sizeof(info->label));

            string_list_free(str_list);
         }
         ret = menu_displaylist_parse_database_entry(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_QUERY:
         ret = menu_database_parse_query(info->list,
               info->path, string_is_empty(info->path_c) 
               ? NULL : info->path_c);
         strlcpy(info->path, info->path_b, sizeof(info->path));

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         ret = menu_displaylist_parse_shader_options(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_CORE_CONTENT:
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf,
               core_len, MENU_FILE_DOWNLOAD_CORE_CONTENT);
         info->need_push    = true;
         info->need_refresh = true;
#endif
         break;
      case DISPLAYLIST_CORES_UPDATER:
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf,
               core_len, MENU_FILE_DOWNLOAD_CORE);
         info->need_push    = true;
         info->need_refresh = true;
#endif
         break;
      case DISPLAYLIST_LAKKA:
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf,
               core_len, MENU_FILE_DOWNLOAD_LAKKA);
         info->need_push    = true;
         info->need_refresh = true;
#endif
         break;
      case DISPLAYLIST_PLAYLIST_COLLECTION:
         if (string_is_equal(info->path, "content_history.lpl"))
         {
            if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info))
               return menu_displaylist_push_list_process(info);
            return false;
         }
         else
         {
            char path_playlist[PATH_MAX_LENGTH];
            content_playlist_t *playlist        = NULL;

            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);

            fill_pathname_join(path_playlist,
                  settings->playlist_directory, info->path,
                  sizeof(path_playlist));

            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_INIT,
                  (void*)path_playlist);

            strlcpy(menu->db_playlist_file, path_playlist,
                  sizeof(menu->db_playlist_file));
            strlcpy(path_playlist,
                  menu_hash_to_str(MENU_LABEL_COLLECTION),
                  sizeof(path_playlist));

            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

            content_playlist_qsort(playlist, menu_displaylist_sort_playlist);

            ret = menu_displaylist_parse_playlist(info,
                  playlist, path_playlist, false);

            if (ret == 0)
            {
               info->need_sort    = true;
               info->need_refresh = true;
               info->need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_HISTORY:
         {
            char path_playlist[PATH_MAX_LENGTH];
            content_playlist_t *playlist        = g_defaults.history;

            if (!playlist)
               event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);

            strlcpy(path_playlist, "history", sizeof(path_playlist));

            ret = menu_displaylist_parse_playlist(info,
                  playlist, path_playlist, true);

            strlcpy(menu->db_playlist_file, settings->content_history_path,
                  sizeof(menu->db_playlist_file));

            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);
            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_INIT,
                  (void*)menu->db_playlist_file);

            if (ret == 0)
            {
               info->need_refresh = true;
               info->need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_INDEX),
               menu_hash_to_str(MENU_LABEL_DISK_INDEX),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0);
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS),
               menu_hash_to_str(MENU_LABEL_DISK_CYCLE_TRAY_STATUS),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0);
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_IMAGE_APPEND),
               menu_hash_to_str(MENU_LABEL_DISK_IMAGE_APPEND),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0);

         info->need_push    = true;
         break;
      case DISPLAYLIST_NETWORK_INFO:
#ifdef HAVE_NETPLAY
#ifndef HAVE_SOCKET_LEGACY
         menu_displaylist_parse_network_info(info);
#endif
#endif
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_displaylist_parse_system_info(info);
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_DEBUG_INFO:
         menu_displaylist_parse_debug_info(info);
         info->need_push    = true;
         info->need_refresh = true;
         break;

#ifdef HAVE_CHEEVOS
      case DISPLAYLIST_ACHIEVEMENT_LIST:
         cheevos_ctl(CHEEVOS_CTL_POPULATE_MENU, info);
         info->need_push    = true;
         info->need_refresh = true;
         break;
#endif

      case DISPLAYLIST_CORES_SUPPORTED:
      case DISPLAYLIST_CORES_COLLECTION_SUPPORTED:
         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;

         {
            unsigned cores_names_len;
            size_t cores_names_size;
            unsigned cores_paths_len;
            size_t cores_paths_size;
            struct string_list *cores_names = 
               string_list_new_special(STRING_LIST_SUPPORTED_CORES_NAMES,
                     (void*)menu->deferred_path,
                     &cores_names_len, &cores_names_size);
            struct string_list *cores_paths = 
               string_list_new_special(STRING_LIST_SUPPORTED_CORES_PATHS,
                  (void*)menu->deferred_path,
                  &cores_paths_len, &cores_paths_size);

            if (cores_names_size == 0)
            {
               menu_entries_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_NO_CORES_AVAILABLE),
                     "",
                     0, 0, 0);
            }
            else
            {
               for (i = 0; i < cores_names_size; i++)
               {
                  if (type == DISPLAYLIST_CORES_COLLECTION_SUPPORTED)
                     menu_entries_push(info->list, cores_paths->elems[i].data, "",
                           MENU_FILE_CORE, 0, 0);
                  else
                     menu_entries_push(info->list, cores_paths->elems[i].data,
                           menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST_OK),
                           MENU_FILE_CORE, 0, 0);
                  menu_entries_set_alt_at_offset(info->list, i,
                        cores_names->elems[i].data);
               }

               string_list_free(cores_names);
               string_list_free(cores_paths);
            }
         }
         break;
      case DISPLAYLIST_CORE_INFO:
         menu_displaylist_parse_core_info(info);
         info->need_push = true;
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         if (runloop_ctl(RUNLOOP_CTL_HAS_CORE_OPTIONS, NULL))
         {
            size_t opts = 0;

            runloop_ctl(RUNLOOP_CTL_GET_CORE_OPTION_SIZE, &opts);

            if (settings->game_specific_options)
            {
               if (!runloop_ctl(RUNLOOP_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  menu_entries_push(info->list,
                        menu_hash_to_str(
                           MENU_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE), "",
                        MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0);
               else
                  menu_entries_push(info->list,
                        menu_hash_to_str(
                           MENU_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE), "",
                        MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0);
            }
            if (opts == 0)
            {
               menu_entries_push(info->list,
                     menu_hash_to_str(
                        MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), "",
                     MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
            }
            else
            {
               for (i = 0; i < opts; i++)
                  menu_entries_push(info->list,
                        core_option_get_desc(system->core_options, i), "",
                        MENU_SETTINGS_CORE_OPTION_START + i, 0, 0);
            }
         }
         else
            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), "",
                  MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_DATABASES:
         info->type_default = MENU_FILE_RDB;
         strlcpy(info->exts, "rdb", sizeof(info->exts));
         strlcpy(info->path, settings->content_database, sizeof(info->path));
         break;
      case DISPLAYLIST_ARCHIVE_ACTION:
#ifdef HAVE_COMPRESSION
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_OPEN_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_OPEN_ARCHIVE),
               0, 0, 0);
#endif
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_LOAD_ARCHIVE),
               0, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE:
#ifdef HAVE_COMPRESSION
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_OPEN_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE),
               0, 0, 0);
#endif
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE),
               0, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_DATABASE_CURSORS:
         info->type_default = MENU_FILE_CURSOR;
         strlcpy(info->exts, "dbc", sizeof(info->exts));
         strlcpy(info->path, settings->cursor_directory, sizeof(info->path));
         break;
      case DISPLAYLIST_DATABASE_PLAYLISTS:
         info->type_default = MENU_FILE_PLAIN;
         strlcpy(info->exts, "lpl", sizeof(info->exts));
         break;
      case DISPLAYLIST_CORES:
         {
            char ext_name[PATH_MAX_LENGTH];

            info->type_default = MENU_FILE_PLAIN;
            if (frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
               strlcpy(info->exts, ext_name, sizeof(info->exts));
         }
         break;
      case DISPLAYLIST_CONFIG_FILES:
         info->type_default = MENU_FILE_CONFIG;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         break;
      case DISPLAYLIST_SHADER_PRESET:
         info->type_default = MENU_FILE_SHADER_PRESET;
         strlcpy(info->exts, "cgp|glslp|slangp", sizeof(info->exts));
         break;
      case DISPLAYLIST_SHADER_PASS:
         info->type_default = MENU_FILE_SHADER;
         strlcpy(info->exts, "cg|glsl|slang", sizeof(info->exts));
         break;
      case DISPLAYLIST_VIDEO_FILTERS:
         info->type_default = MENU_FILE_VIDEOFILTER;
         strlcpy(info->exts, "filt", sizeof(info->exts));
         break;
      case DISPLAYLIST_IMAGES:
         info->type_default = MENU_FILE_IMAGE;
         strlcpy(info->exts, "png", sizeof(info->exts));
         break;
      case DISPLAYLIST_AUDIO_FILTERS:
         info->type_default = MENU_FILE_AUDIOFILTER;
         strlcpy(info->exts, "dsp", sizeof(info->exts));
         break;
      case DISPLAYLIST_CHEAT_FILES:
         info->type_default = MENU_FILE_CHEAT;
         strlcpy(info->exts, "cht", sizeof(info->exts));
         break;
      case DISPLAYLIST_CONTENT_HISTORY:
         info->type_default = MENU_FILE_PLAIN;
         strlcpy(info->exts, "lpl", sizeof(info->exts));
         break;
      case DISPLAYLIST_FONTS:
         info->type_default = MENU_FILE_FONT;
         strlcpy(info->exts, "ttf", sizeof(info->exts));
         break;
      case DISPLAYLIST_OVERLAYS:
         info->type_default = MENU_FILE_OVERLAY;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         break;
      case DISPLAYLIST_RECORD_CONFIG_FILES:
         info->type_default = MENU_FILE_RECORD_CONFIG;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         break;
      case DISPLAYLIST_REMAP_FILES:
         info->type_default = MENU_FILE_REMAP;
         strlcpy(info->exts, "rmp", sizeof(info->exts));
         break;
      default:
         break;
   }

   switch (type)
   {
      case DISPLAYLIST_DATABASES:
      case DISPLAYLIST_DEFAULT:
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_DATABASE_CURSORS:
      case DISPLAYLIST_DATABASE_PLAYLISTS:
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
      case DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL:
         {
            bool horizontal = 
               (type == DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL);

            if (menu_displaylist_parse_generic(info, horizontal) == 0)
            {
               info->need_refresh = true;
               info->need_push    = true;
            }
         }
         break;
      default:
         break;
   }

   if (ret != 0)
      return false;

   return true;
}

